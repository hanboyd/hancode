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


__all__ = [
    "parse_capabilities",
    "parse_control",
    "mic_open_command",
    "mic_close_command",
]