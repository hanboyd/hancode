"""Phase 2 / Area 1: module-level switch for ATVV capability parse
(ADR-0012 §6).

Exposes ``parse_capabilities(data: bytes)`` which dispatches via
``choose_implementation`` to either:

  * ``python``: ``ovb_rc003.atvv_protocol.ATVVCapabilities.parse``
  * ``native``: ``remotemic_native.atvv_capabilities_parse`` (the
    pybind11 binding backed by ``remotemic::atvv::parse``)
  * ``shadow``: runs both, asserts byte-exact field equality, returns
    the python result

The default is ``python`` (per migration plan §1 rule 4). Switch via:

    REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL=native
    REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL=shadow
    REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL=python  # default

Both python and native returns are normalized to the
``ATVVCapabilities`` dataclass so callers do not have to know which
implementation ran. The shadow mode re-uses ``choose_implementation``'s
strict equality check (any drift -> RuntimeError).

Phase 2 / Area 2 adds three sibling switches for the ATVV control
channel (ADR-0012 §3 / §6):
    parse_control(data: bytes) -> Optional[dict]
    mic_open_command(version: int) -> bytes
    mic_close_command(version: int, session_id: int) -> bytes

All three are pure compute; shadow is permitted.
"""

from __future__ import annotations

from typing import Optional

from . import atvv_protocol as proto
from ._remotemic_native_runtime import choose_implementation


def _native_to_python(result: object) -> Optional[proto.ATVVCapabilities]:
    """Convert the pybind11 ``AtvvCapabilities`` (or None) into the
    Python ``ATVVCapabilities`` dataclass so callers get a stable
    shape regardless of which implementation actually ran."""
    if result is None:
        return None
    return proto.ATVVCapabilities(
        version=result.version,        # type: ignore[union-attr]
        codecs=result.codecs,          # type: ignore[union-attr]
        interaction=result.interaction,  # type: ignore[union-attr]
        frame_size=result.frame_size,  # type: ignore[union-attr]
        selected_codec=result.selected_codec,  # type: ignore[union-attr]
        sample_rate=result.sample_rate,  # type: ignore[union-attr]
    )


def _parse_capabilities_python(data: bytes) -> Optional[proto.ATVVCapabilities]:
    return proto.ATVVCapabilities.parse(data)


def _parse_capabilities_native(data: bytes) -> Optional[proto.ATVVCapabilities]:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        # Mirror the dry-run fallback behavior: a stripped or
        # not-yet-built .pyd falls back to the python baseline so
        # product code never sees a None where it expected a result.
        return proto.ATVVCapabilities.parse(data)
    return _native_to_python(_rn.atvv_capabilities_parse(data))


# Module-level wrappers keep the functions plain (no ``self``) so
# ``choose_implementation`` can call them with a single positional arg.
parse_capabilities_python = _parse_capabilities_python
parse_capabilities_native = _parse_capabilities_native


# Side-effect-free compute module (ADR-0012 §6). shadow is therefore
# permitted.
parse_capabilities = choose_implementation(
    "atvv_protocol",
    python_impl=parse_capabilities_python,
    native_impl=parse_capabilities_native,
    side_effect_free=True,
)


# ---------------------------------------------------------------------------
# ATVV control channel (Phase 2 / Area 2, ADR-0012 §3 / §6)
# ---------------------------------------------------------------------------
# Both encoders and the decoder are pure compute; shadow is permitted. The
# native and python sides return byte-for-byte identical payloads; the
# decoder returns the same dict shape (``{opcode: ..., ...}``).


def _parse_control_python(data: bytes) -> Optional[dict]:
    return proto.parse_control_payload(data)


def _parse_control_native(data: bytes) -> Optional[dict]:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        return proto.parse_control_payload(data)
    return _rn.atvv_control_parse(data)


def _mic_open_command_python(version: int) -> bytes:
    return proto.mic_open_command(version)


def _mic_open_command_native(version: int) -> bytes:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        return proto.mic_open_command(version)
    return _rn.atvv_mic_open_command(version)


def _mic_close_command_python(version: int, session_id: int) -> bytes:
    return proto.mic_close_command(version, session_id)


def _mic_close_command_native(version: int, session_id: int) -> bytes:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        return proto.mic_close_command(version, session_id)
    return _rn.atvv_mic_close_command(version, session_id)


parse_control_python = _parse_control_python
parse_control_native = _parse_control_native
mic_open_command_python = _mic_open_command_python
mic_open_command_native = _mic_open_command_native
mic_close_command_python = _mic_close_command_python
mic_close_command_native = _mic_close_command_native


# parse_control uses its own switch name so callers can flip parse_control
# independently of parse_capabilities (e.g. switch capability to native in
# production while keeping control on python during a phased rollout).
parse_control = choose_implementation(
    "atvv_control_parse",
    python_impl=parse_control_python,
    native_impl=parse_control_native,
    side_effect_free=True,
)
mic_open_command = choose_implementation(
    "atvv_control_encode",
    python_impl=mic_open_command_python,
    native_impl=mic_open_command_native,
    side_effect_free=True,
)
mic_close_command = choose_implementation(
    "atvv_control_encode",
    python_impl=mic_close_command_python,
    native_impl=mic_close_command_native,
    side_effect_free=True,
)


# ---------------------------------------------------------------------------
# IMA/DVI ADPCM decoder (Phase 2 / Area 3, ADR-0012 section 3 / section 6)
# ---------------------------------------------------------------------------
# The decoder is a stateful value type, but shadow parity is straightforward
# because every call creates a fresh decoder. The ``predictor`` and
# ``step_index`` arguments let callers prime the decoder exactly the way
# ``atvv_session.handle_audio`` does (AUDIO_SYNC path: reset(predictor,
# step_index) before the first frame). The return value is a freshly
# allocated list[int] / vector<int16_t>; no shared state survives across
# calls, so shadow mode is safe.


def _decode_adpcm_frame_python(
    data: bytes, predictor: int = 0, step_index: int = 0
) -> list:
    decoder = proto.IMAADPCMDecoder()
    decoder.reset(predictor, step_index)
    return decoder.decode(data)


def _decode_adpcm_frame_native(
    data: bytes, predictor: int = 0, step_index: int = 0
) -> list:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        return _decode_adpcm_frame_python(data, predictor, step_index)
    decoder = _rn.ImaDecoder()
    decoder.reset(predictor, step_index)
    return list(decoder.decode(data))


decode_adpcm_frame_python = _decode_adpcm_frame_python
decode_adpcm_frame_native = _decode_adpcm_frame_native


decode_adpcm_frame = choose_implementation(
    "adpcm_ima_decode",
    python_impl=decode_adpcm_frame_python,
    native_impl=decode_adpcm_frame_native,
    side_effect_free=True,
)


# ---------------------------------------------------------------------------
# ADPCM DC high-pass + postprocess + FrameAccumulator
# (Phase 2 / Area 4, ADR-0012 section 3 / section 6)
# ---------------------------------------------------------------------------
# All three are pure compute (or pure compute with private state); shadow is
# permitted. Compare with strict ``==``: every output is a fresh
# list[int] / list[bytes] value whose elements are integer-or-bytes after
# the int16 / uint8 clamps inside the helpers, so a sample-exact
# comparison directly implements the ADR-0012 section 5 hard rule.


def _apply_dc_highpass_python(samples: list) -> list:
    """Run the Python baseline DC high-pass on a fresh filter."""
    flt = proto.DCHighPassFilter()
    return [int(s) for s in flt.process(samples)]


def _apply_dc_highpass_native(samples: list) -> list:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        return _apply_dc_highpass_python(samples)
    flt = _rn.DcHighPassFilter(16000.0, 20.0)
    return [int(s) for s in flt.process(samples)]


apply_dc_highpass_python = _apply_dc_highpass_python
apply_dc_highpass_native = _apply_dc_highpass_native


apply_dc_highpass = choose_implementation(
    "adpcm_dc_highpass",
    python_impl=apply_dc_highpass_python,
    native_impl=apply_dc_highpass_native,
    side_effect_free=True,
)


def _postprocess_pcm_python(samples: list, gain_db: float) -> list:
    return [int(s) for s in proto.postprocess(samples, gain_db)]


def _postprocess_pcm_native(samples: list, gain_db: float) -> list:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        return _postprocess_pcm_python(samples, gain_db)
    return [int(s) for s in _rn.postprocess(samples, gain_db)]


postprocess_pcm_python = _postprocess_pcm_python
postprocess_pcm_native = _postprocess_pcm_native


postprocess_pcm = choose_implementation(
    "adpcm_postprocess",
    python_impl=postprocess_pcm_python,
    native_impl=postprocess_pcm_native,
    side_effect_free=True,
)


def _accumulate_frames_python(
    data_chunks: list, frame_size: int
) -> list:
    """Drive the Python baseline FrameAccumulator through an ordered
    sequence of data chunks (each an ``int``-iterable / ``bytes`` /
    ``bytearray``) and return every emitted frame as ``list[bytes]``.
    Used by step 5 to verify byte-exact parity with the native side
    across both single-append and multi-append-across-calls inputs.
    """
    acc = proto.FrameAccumulator()
    out: list = []
    for chunk in data_chunks:
        # Mirror the binding contract: skip <= 0 and reject > 65535
        # by routing through the public Python guard.
        for f in acc.append(chunk, frame_size):
            out.append(bytes(f))
    return out


def _accumulate_frames_native(
    data_chunks: list, frame_size: int
) -> list:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    if not _rn._C_AVAILABLE:
        return _accumulate_frames_python(data_chunks, frame_size)
    acc = _rn.FrameAccumulator()
    out: list = []
    for chunk in data_chunks:
        for f in acc.append(chunk, frame_size):
            out.append(bytes(f))
    return out


accumulate_frames_python = _accumulate_frames_python
accumulate_frames_native = _accumulate_frames_native


accumulate_frames = choose_implementation(
    "adpcm_frame_accumulator",
    python_impl=accumulate_frames_python,
    native_impl=accumulate_frames_native,
    side_effect_free=True,
)


__all__ = [
    "parse_capabilities",
    "parse_control",
    "mic_open_command",
    "mic_close_command",
    "decode_adpcm_frame",
    "apply_dc_highpass",
    "postprocess_pcm",
    "accumulate_frames",
]