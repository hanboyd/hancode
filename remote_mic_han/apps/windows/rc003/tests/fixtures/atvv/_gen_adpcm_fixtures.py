"""Generate the Phase 2 / Area 3 ADPCM JSON golden fixtures from the
Python baseline.

The fixtures are the single source of truth shared between the C++
unit tests and the Python shadow parity tests; this script is
**build-time only** — its output is checked into git and never run
during test discovery.

Hard rule from the phase 2 entry scope (no tolerance, byte-exact /
sample-exact): every byte in ``input_hex`` and every integer in
``expected_pcm`` must match the C++ decoder's output exactly. If this
script ever needs to change, regenerate by hand against the Python
baseline and commit the diff - never write expected_pcm by hand.

Run:
    PYTHONPATH=apps/windows/rc003/src apps/windows/rc003/.venv/Scripts/python.exe \\
        apps/windows/rc003/tests/fixtures/atvv/_gen_adpcm_fixtures.py
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import List

from ovb_rc003 import atvv_protocol as proto


FIXTURE_DIR = Path(__file__).resolve().parent


def _decoder() -> proto.IMAADPCMDecoder:
    return proto.IMAADPCMDecoder()


def _write(name: str, body: dict) -> None:
    body.setdefault(
        "source",
        "synthetic; generated from ovb_rc003.atvv_protocol.IMAADPCMDecoder "
        "via _gen_adpcm_fixtures.py; contains no captured device or voice data",
    )
    body["fixture_kind"] = "adpcm_decode"
    body["function"] = "ImaDecoder.decode"
    out = FIXTURE_DIR / name
    out.write_text(json.dumps(body, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out.relative_to(FIXTURE_DIR.parent.parent.parent.parent.parent)}")


def fixture_empty() -> None:
    """Empty input -> empty output. Boundary case for the decoder's
    inner loop (no bytes -> no samples)."""
    _write(
        "adpcm-empty.json",
        {
            "description": "Empty ADPCM byte stream; decode() returns []. "
                           "Boundary case for the inner loop.",
            "input_hex": "",
            "expected_pcm": [],
        },
    )


def fixture_single_byte_zero_state() -> None:
    """Single byte with default reset state (0, 0). Exercises one
    iteration of the high/low nibble decode path. The 0x6 nibble
    pattern is sign=0 (positive diff) with code=6 (1<<2 | 1<<1) ->
    difference = step + step/2 + step/4 = step * 1.75."""
    d = _decoder()
    d.reset(0, 0)
    samples = d.decode(bytes.fromhex("60"))
    _write(
        "adpcm-single-byte-zero-state.json",
        {
            "description": "Single byte 0x60 with reset(0, 0). Nibble 6 = "
                           "sign=0, code=6 -> diff = step*1.75. Decoder "
                           "returns 2 samples (high nibble first).",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": "60",
            "expected_pcm": samples,
        },
    )


def fixture_four_byte_zero_state() -> None:
    """4-byte ADPCM stream from the Phase 0 mixed fixture, decoded
    from reset(0, 0). Matches the existing test_atvv_golden_fixture
    expectation byte-for-byte so the new fixtures are an extension,
    not a replacement."""
    d = _decoder()
    d.reset(0, 0)
    samples = d.decode(bytes.fromhex("007f80ff"))
    _write(
        "adpcm-four-byte-zero-state.json",
        {
            "description": "4-byte stream 007f80ff with reset(0, 0). "
                           "Mirrors the Phase 0 mixed fixture's ADPCM "
                           "half (synthetic-v1.json); regenerated here "
                           "as a standalone adpcm_decode fixture.",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": "007f80ff",
            "expected_pcm": samples,
        },
    )


def fixture_all_positive_nibbles() -> None:
    """Bytes 0x66 0x66 (nibbles 6, 6, 6, 6) -> all positive diffs.
    Predictor climbs monotonically from 0."""
    d = _decoder()
    d.reset(0, 0)
    samples = d.decode(bytes.fromhex("6666"))
    _write(
        "adpcm-all-positive-nibbles.json",
        {
            "description": "2 bytes 0x66 0x66 -> 4 nibbles all set to 6 "
                           "(sign=0, code=6 -> positive diff step*1.75). "
                           "Predictor rises monotonically.",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": "6666",
            "expected_pcm": samples,
        },
    )


def fixture_all_negative_nibbles() -> None:
    """Bytes 0xEE 0xEE (nibbles 0xE, 0xE) -> sign=1 (negative), code=6.
    Predictor descends monotonically."""
    d = _decoder()
    d.reset(0, 0)
    samples = d.decode(bytes.fromhex("eeee"))
    _write(
        "adpcm-all-negative-nibbles.json",
        {
            "description": "2 bytes 0xEE 0xEE -> 4 nibbles all set to 0xE "
                           "(sign=1, code=6 -> negative diff step*1.75). "
                           "Predictor descends monotonically.",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": "eeee",
            "expected_pcm": samples,
        },
    )


def fixture_round_trip_ramp() -> None:
    """Round-trip via the test's _reference_ima_encode: build a
    synthetic ramp, encode it, decode it. Encoder and decoder must be
    self-consistent byte-exact (per ADR-0012 §5: PCM samples exact
    sample-for-sample, no tolerance)."""
    # Local copy of the test helper (cannot import from test dir at
    # fixture-gen time). Algorithm must stay byte-identical to
    # apps/windows/rc003/tests/test_atvv_protocol.py:_reference_ima_encode.
    step_table = proto.IMAADPCMDecoder._STEP_TABLE
    index_table = proto.IMAADPCMDecoder._INDEX_TABLE

    def encode(samples: List[int]) -> bytes:
        predictor = 0
        step_index = 0
        nibbles: List[int] = []
        for sample in samples:
            diff = sample - predictor
            sign = 8 if diff < 0 else 0
            diff = abs(diff)
            step = step_table[step_index]
            code = 0
            temp_step = step
            if diff >= temp_step:
                code |= 4
                diff -= temp_step
            temp_step >>= 1
            if diff >= temp_step:
                code |= 2
                diff -= temp_step
            temp_step >>= 1
            if diff >= temp_step:
                code |= 1
            nibble = sign | code
            # Reconstruct predictor exactly as the decoder would.
            decoded_step = step
            decoded_difference = decoded_step >> 3
            if nibble & 1:
                decoded_difference += decoded_step >> 2
            if nibble & 2:
                decoded_difference += decoded_step >> 1
            if nibble & 4:
                decoded_difference += decoded_step
            if nibble & 8:
                predictor -= decoded_difference
            else:
                predictor += decoded_difference
            predictor = min(32767, max(-32768, predictor))
            step_index += index_table[nibble & 7]
            step_index = min(88, max(0, step_index))
            nibbles.append(nibble)
        out = bytearray()
        for i in range(0, len(nibbles), 2):
            out.append((nibbles[i] << 4) | nibbles[i + 1])
        return bytes(out)

    # 16-sample ramp in the safe range.
    ramp = [
        min(32767, max(-32768, (i * 137) % 4000 - 2000))
        for i in range(16)
    ]
    encoded = encode(ramp)
    d = _decoder()
    d.reset(0, 0)
    samples = d.decode(encoded)
    _write(
        "adpcm-round-trip-ramp.json",
        {
            "description": "16-sample synthetic ramp (modulo bounded) "
                           "encoded with the independent reference "
                           "encoder, then decoded back. The encoder and "
                           "decoder are self-consistent byte-exact per "
                           "ADR-0012 §5 (no tolerance).",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": encoded.hex(),
            "expected_pcm": samples,
        },
    )


def fixture_clamp_predictor_high() -> None:
    """Extreme positive nibbles (0x7 -> sign=0, code=7 -> max positive
    diff step*1.75 + step*0.25 = step*2) repeated until predictor
    clamps at 32767. Exercises the predictor clamp."""
    d = _decoder()
    d.reset(0, 0)
    # 16 bytes of 0x77 -> 32 nibbles of 7
    samples = d.decode(bytes.fromhex("77") * 16)
    _write(
        "adpcm-clamp-predictor-high.json",
        {
            "description": "16 bytes of 0x77 (nibble=7 -> max positive "
                           "diff per step). Predictor clamps to +32767 "
                           "before the stream ends.",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": "77" * 16,
            "expected_pcm": samples,
        },
    )


def fixture_clamp_predictor_low() -> None:
    """Extreme negative nibbles (0xF -> sign=1, code=7) repeated
    until predictor clamps at -32768. Exercises the predictor clamp."""
    d = _decoder()
    d.reset(0, 0)
    samples = d.decode(bytes.fromhex("ff") * 16)
    _write(
        "adpcm-clamp-predictor-low.json",
        {
            "description": "16 bytes of 0xFF (nibble=0xF -> max negative "
                           "diff per step). Predictor clamps to -32768 "
                           "before the stream ends.",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": "ff" * 16,
            "expected_pcm": samples,
        },
    )


def fixture_clamp_step_index() -> None:
    """Nibbles 0x00 (-> step_index -= 1) and 0x07 (-> step_index += 8)
    repeated to push step_index to 0 (low) and 88 (high). Exercises
    the step_index clamp."""
    d_low = _decoder()
    d_low.reset(0, 88)
    samples_low = d_low.decode(bytes.fromhex("00") * 8)
    d_high = _decoder()
    d_high.reset(0, 0)
    samples_high = d_high.decode(bytes.fromhex("07") * 8)
    _write(
        "adpcm-clamp-step-index.json",
        {
            "description": "Two streams back-to-back: reset(0, 88) then "
                           "8 bytes of 0x00 (each nibble=0 -> step_index "
                           "decrements by 1, clamped at 0); then reset(0, 0) "
                           "then 8 bytes of 0x07 (each nibble=7 -> "
                           "step_index increments by 8, clamped at 88). "
                           "Exercises the step_index clamp at both ends.",
            "reset": {"predictor": 0, "step_index": 88},
            "input_hex": "00" * 8,
            "expected_pcm": samples_low,
        },
    )
    _write(
        "adpcm-clamp-step-index-high.json",
        {
            "description": "reset(0, 0) then 8 bytes of 0x07 (each nibble=7 "
                           "-> step_index increments by 8, clamped at 88). "
                           "Exercises the step_index clamp at the high end.",
            "reset": {"predictor": 0, "step_index": 0},
            "input_hex": "07" * 8,
            "expected_pcm": samples_high,
        },
    )


def fixture_reset_nonzero_state() -> None:
    """Decode with reset(predictor=1000, step_index=30) - exercises
    the AUDIO_SYNC pathway where the decoder is primed by an explicit
    predictor+step_index pair before the first audio frame."""
    d = _decoder()
    d.reset(1000, 30)
    samples = d.decode(bytes.fromhex("4466"))
    _write(
        "adpcm-reset-nonzero-state.json",
        {
            "description": "reset(1000, 30) then 2 bytes 0x44 0x66. "
                           "Exercises the AUDIO_SYNC pathway where the "
                           "decoder is primed by an explicit predictor "
                           "+ step_index before the first audio frame. "
                           "The non-zero start state means the same "
                           "input bytes would produce different samples "
                           "compared to reset(0, 0).",
            "reset": {"predictor": 1000, "step_index": 30},
            "input_hex": "4466",
            "expected_pcm": samples,
        },
    )


def main() -> None:
    fixture_empty()
    fixture_single_byte_zero_state()
    fixture_four_byte_zero_state()
    fixture_all_positive_nibbles()
    fixture_all_negative_nibbles()
    fixture_round_trip_ramp()
    fixture_clamp_predictor_high()
    fixture_clamp_predictor_low()
    fixture_clamp_step_index()
    fixture_reset_nonzero_state()


if __name__ == "__main__":
    main()