"""Android TV Voice-over-BLE (ATVV) protocol constants and codecs.

Clean-room reimplementation informed by the GPL-3.0-only upstream project
xxb26553663-star/remote-bridge-hub @ 8a93f321ac71a602300c6cd77f7256fa4b63068e
(source/bridges/xiaomi/atvv_record.py, atvv_live_bridge.py). The GATT UUIDs,
control opcodes, and IMA/DVI ADPCM tables are interoperability facts (the
values the physical remote actually speaks), not creative expression; no
upstream source lines were copied to produce this module.

This module is pure Python with no platform dependencies, so it can be unit
tested on any OS.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List, Optional, Sequence

VOICE_SERVICE_UUID = "AB5E0001-5A21-4F05-BC7D-AF01F617B664"
VOICE_TX_UUID = "AB5E0002-5A21-4F05-BC7D-AF01F617B664"
VOICE_AUDIO_UUID = "AB5E0003-5A21-4F05-BC7D-AF01F617B664"
VOICE_CONTROL_UUID = "AB5E0004-5A21-4F05-BC7D-AF01F617B664"

GET_CAPABILITIES_V10 = bytes((0x0A, 0x01, 0x00, 0x00, 0x03, 0x03))

# Control-channel opcodes (first byte of every VOICE_CONTROL_UUID notification).
OPCODE_AUDIO_STOP = 0x00
OPCODE_AUDIO_START = 0x04
OPCODE_MIC_BUTTON = 0x08
OPCODE_AUDIO_SYNC = 0x0A
OPCODE_CAPS = 0x0B

DEFAULT_FRAME_SIZE = 120
SUPPORTED_SAMPLE_RATE_HZ = 16000

# End-of-voice residual flush delay used by the reference implementation
# before tearing down the output session, so the last decoded audio isn't
# truncated.
AUDIO_STOP_FLUSH_SECONDS = 0.12

# Guard window: audio bytes that arrive this soon after AUDIO_STOP are
# considered leftovers from the just-finished session, not a new one, and
# must be discarded rather than implicitly starting a fresh stream.
LATE_AUDIO_GUARD_SECONDS = 0.3


def mic_open_command(version: int) -> bytes:
    """Host->device command that opens/starts the microphone stream."""

    if version >= 0x0100:
        return bytes((0x0C, 0x00))
    return bytes((0x0C, 0x00, 0x00))


def mic_close_command(version: int, session_id: int) -> bytes:
    """Host->device command that closes the microphone stream."""

    if version >= 0x0100:
        return bytes((0x0D, session_id & 0xFF))
    return bytes((0x0D,))


@dataclass(frozen=True)
class ATVVCapabilities:
    version: int
    codecs: int
    interaction: int
    frame_size: int
    selected_codec: int
    sample_rate: float

    @classmethod
    def parse(cls, data: bytes) -> Optional["ATVVCapabilities"]:
        if len(data) < 7 or data[0] != OPCODE_CAPS:
            return None

        version = (data[1] << 8) | data[2]

        if version >= 0x0100:
            codecs = data[3]
            interaction = data[4]
            if codecs == 0 and len(data) >= 9 and (data[4] & 0x03):
                codecs = data[4]
                interaction = 0x03
        else:
            if len(data) < 9:
                return None
            codecs = data[4]
            interaction = 0x00

        frame_size = (data[5] << 8) | data[6]
        selected_codec = 0x02 if (codecs & 0x02) else 0x01
        return cls(
            version=version,
            codecs=codecs,
            interaction=interaction,
            frame_size=frame_size or DEFAULT_FRAME_SIZE,
            selected_codec=selected_codec,
            sample_rate=16000.0 if selected_codec == 0x02 else 8000.0,
        )


def parse_control_payload(data: bytes) -> Optional[dict]:
    """Pure field parser for ATVV control-channel payloads.

    Mirrors the field extraction half of
    ``atvv_session.handle_control`` (``atvv_session.py:178-223``)
    byte-for-byte so the C++ binding
    (``remotemic_native.atvv_control_parse``) can be shadow-compared
    against it. Per ADR-0012 §2 the state machine (capability gate,
    decoder reset, sync predictor hold, late-audio discard) stays in
    Python; this function only extracts fields.

    Returns:
      * ``None`` for empty payloads (the state machine raises
        ATVVProtocolError on None; the parser itself does not raise).
      * A dict shaped to match the C++ binding exactly:
          {"opcode": "Caps"}
          {"opcode": "MicButton"}
          {"opcode": "AudioStart", "session_id": int_or_None}
          {"opcode": "AudioStop"}
          {"opcode": "AudioSync", "predictor": int, "step_index": int}
          {"opcode": "Unknown", "raw_opcode": int}

    AUDIO_SYNC payloads shorter than 7 bytes fall through to Unknown
    with ``raw_opcode == OPCODE_AUDIO_SYNC`` so the state machine sees
    a uniform value and decides whether to discard.
    """
    if not data:
        return None

    opcode = data[0]

    if opcode == OPCODE_CAPS:
        return {"opcode": "Caps"}
    if opcode == OPCODE_MIC_BUTTON:
        return {"opcode": "MicButton"}
    if opcode == OPCODE_AUDIO_STOP:
        return {"opcode": "AudioStop"}
    if opcode == OPCODE_AUDIO_START:
        session_id = data[3] if len(data) >= 4 else None
        return {"opcode": "AudioStart", "session_id": session_id}
    if opcode == OPCODE_AUDIO_SYNC:
        if len(data) >= 7:
            predictor = int.from_bytes(data[4:6], "big", signed=True)
            step_index = data[6]
            return {
                "opcode": "AudioSync",
                "predictor": predictor,
                "step_index": step_index,
            }
        return {"opcode": "Unknown", "raw_opcode": OPCODE_AUDIO_SYNC}

    return {"opcode": "Unknown", "raw_opcode": opcode}


class IMAADPCMDecoder:
    """Standard IMA/DVI 4-bit ADPCM decoder, 2 samples per byte (high nibble first)."""

    _STEP_TABLE = (
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
        34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
        143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
        494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
        1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
        4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
        11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
        27086, 29794, 32767,
    )
    _INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8)

    def __init__(self) -> None:
        self.predictor = 0
        self.step_index = 0

    def reset(self, predictor: int = 0, step_index: int = 0) -> None:
        self.predictor = min(32767, max(-32768, predictor))
        self.step_index = min(88, max(0, step_index))

    def decode(self, data: bytes) -> List[int]:
        samples: List[int] = []
        samples_append = samples.append
        for byte in data:
            samples_append(self._decode_nibble(byte >> 4))
            samples_append(self._decode_nibble(byte & 0x0F))
        return samples

    def _decode_nibble(self, nibble: int) -> int:
        step = self._STEP_TABLE[self.step_index]
        difference = step >> 3
        if nibble & 1:
            difference += step >> 2
        if nibble & 2:
            difference += step >> 1
        if nibble & 4:
            difference += step

        if nibble & 8:
            self.predictor -= difference
        else:
            self.predictor += difference
        self.predictor = min(32767, max(-32768, self.predictor))

        self.step_index += self._INDEX_TABLE[nibble & 7]
        self.step_index = min(88, max(0, self.step_index))
        return self.predictor


class DCHighPassFilter:
    """Remove slow ADPCM predictor bias without filtering speech frequencies."""

    def __init__(self, sample_rate: float = SUPPORTED_SAMPLE_RATE_HZ, cutoff_hz: float = 20.0) -> None:
        if sample_rate <= 0 or cutoff_hz <= 0:
            raise ValueError("sample_rate and cutoff_hz must be positive")
        self._alpha = math.exp(-2.0 * math.pi * cutoff_hz / sample_rate)
        self.reset()

    def reset(self) -> None:
        self._previous_input = 0.0
        self._previous_output = 0.0
        self._initialized = False

    def process(self, samples: Sequence[int]) -> List[int]:
        if not samples:
            return []
        if not self._initialized:
            self._previous_input = float(samples[0])
            self._initialized = True

        filtered: List[int] = []
        for sample in samples:
            current = float(sample)
            output = current - self._previous_input + self._alpha * self._previous_output
            self._previous_input = current
            self._previous_output = output
            filtered.append(min(32767, max(-32768, int(round(output)))))
        return filtered


def postprocess(samples: Sequence[int], gain_db: float = 10.0) -> List[int]:
    """3-tap smoothing filter plus gain, matching ATVVProtocol.swift's PCMPostprocessor."""

    if not samples:
        return []

    filtered = list(samples)
    if len(samples) >= 3:
        for index in range(1, len(samples) - 1):
            filtered[index] = (
                samples[index - 1] + 2 * samples[index] + samples[index + 1]
            ) >> 2

    finite_gain_db = gain_db if gain_db == gain_db and abs(gain_db) != float("inf") else 0.0
    safe_gain_db = min(24.0, max(-24.0, finite_gain_db))
    gain = 10.0 ** (safe_gain_db / 20.0)

    result: List[int] = []
    for value in filtered:
        scaled = int(round(value * gain))
        result.append(min(32767, max(-32768, scaled)))
    return result


class FrameAccumulator:
    """Re-chunks arbitrarily-fragmented notification payloads into fixed frames."""

    def __init__(self) -> None:
        self._pending = bytearray()

    def append(self, data: bytes, frame_size: int) -> List[bytes]:
        if frame_size <= 0:
            return []
        self._pending.extend(data)
        frames: List[bytes] = []
        while len(self._pending) >= frame_size:
            frames.append(bytes(self._pending[:frame_size]))
            del self._pending[:frame_size]
        return frames

    def reset(self) -> None:
        self._pending.clear()
