"""Phase 3 / ADR-0013 §3.3: module-level switch for ATVVSession.

Exposes ``make_atvv_session(gain_db=10.0) -> ATVVSession`` which
dispatches via ``choose_implementation`` to either:

  * ``python``: ``ovb_rc003.atvv_session.ATVVSession`` (the pure-Python
    implementation; the single-session owner in production today)
  * ``native``: ``remotemic_native._C.AtvvSession`` (the pybind11
    binding, with the Phase 2 PCM pipeline wired internally)
  * ``shadow``: runs both with identical inputs; on every
    ``handle_control`` / ``handle_audio`` call asserts that the returned
    ``ControlEvent`` / sample list is byte-identical.

The default is ``python`` (per migration plan §1 rule 4). Switch via:

    REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION=native
    REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION=shadow
    REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION=python  # default

Both python and native returns share the same Python surface
(``capabilities`` / ``mic_open`` properties and the same
``handle_control`` / ``handle_audio`` / ``mic_open_command`` /
``mic_close_command`` methods).
"""

from __future__ import annotations

from . import atvv_session as py_mod
from ._remotemic_native_runtime import choose_implementation


class _NativeATVVSession:
    """Thin shim over ``remotemic_native._C.AtvvSession`` that exposes
    the same Python surface as ``ovb_rc003.atvv_session.ATVVSession``
    so the bridge wrapper can stay implementation-agnostic.

    Note: time injection lives at the C++ binding seam; the production
    constructor (gain_db only) maps to the C++ monotonic-clock
    constructor. Unit tests that need to advance time use the python
    implementation directly.
    """

    def __init__(self, gain_db: float = 10.0) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        if not _rn._C_AVAILABLE:
            self._impl = py_mod.ATVVSession(gain_db)
            self._is_native = False
            return
        self._impl = _rn.AtvvSession(gain_db)
        self._is_native = True

    @property
    def capabilities(self):
        caps = self._impl.capabilities
        if not self._is_native or caps is None:
            return caps
        return py_mod.proto.ATVVCapabilities(
            version=caps.version,
            codecs=caps.codecs,
            interaction=caps.interaction,
            frame_size=caps.frame_size,
            selected_codec=caps.selected_codec,
            sample_rate=caps.sample_rate,
        )

    @property
    def mic_open(self) -> bool:
        return bool(self._impl.mic_open)

    def handle_control(self, payload: bytes):
        try:
            event = self._impl.handle_control(payload)
        except ValueError as exc:
            message = str(exc)
            if "unsupported ATVV sample rate" in message:
                raise py_mod.UnsupportedSampleRateError(message) from exc
            raise py_mod.ATVVProtocolError(message) from exc
        return _dict_to_event(event) if self._is_native else event

    def handle_audio(self, payload: bytes):
        return list(self._impl.handle_audio(payload))

    def mic_open_command(self) -> bytes:
        return bytes(self._impl.mic_open_command())

    def mic_close_command(self) -> bytes:
        return bytes(self._impl.mic_close_command())


def _make_atvv_session_python(gain_db: float = 10.0) -> py_mod.ATVVSession:
    return py_mod.ATVVSession(gain_db)


def _make_atvv_session_native(gain_db: float = 10.0) -> _NativeATVVSession:
    return _NativeATVVSession(gain_db)


make_atvv_session_python = _make_atvv_session_python
make_atvv_session_native = _make_atvv_session_native


# Session is stateful: ``shadow`` runs both with the same script and
# asserts equality at every step. The C++ side is pure compute (no
# I/O), so shadow parity is permitted per ADR-0013 §6.
def _make_atvv_session_shadow(gain_db: float = 10.0):
    py_session = py_mod.ATVVSession(gain_db)
    native_session = _NativeATVVSession(gain_db)
    return _ShadowATVVSession(py_session, native_session)


class _ShadowATVVSession:
    """Drives python and native sessions with the same script and
    asserts identity after every mutating call. Returns the python
    side as the authoritative result (the python side is what callers
    see today)."""

    def __init__(self, py_session, native_session) -> None:
        self._py = py_session
        self._native = native_session

    @staticmethod
    def _check(name: str, expected: object, actual: object) -> None:
        if expected != actual:
            raise RuntimeError(
                f"shadow(atvv_session).{name}: "
                f"python={expected!r} native={actual!r}"
            )

    def _assert_parity(self) -> None:
        self._check(
            "capabilities", self._py.capabilities, self._native.capabilities
        )
        self._check("mic_open", self._py.mic_open, self._native.mic_open)

    @property
    def capabilities(self):
        return self._py.capabilities

    @property
    def mic_open(self) -> bool:
        return self._py.mic_open

    def handle_control(self, payload: bytes):
        py_event = self._py.handle_control(payload)
        native_event = self._native.handle_control(payload)
        # Normalize native dict to a comparable form. The native
        # binding always returns a dict with opcode + per-opcode
        # fields; python returns one of the dataclasses. Convert the
        # python dataclass to the same dict shape for comparison.
        py_dict = _event_to_dict(py_event)
        native_dict = _event_to_dict(native_event)
        if py_dict != native_dict:
            raise RuntimeError(
                f"shadow(atvv_session).handle_control: "
                f"python={py_dict!r} native={native_dict!r}"
            )
        self._assert_parity()
        return py_event

    def handle_audio(self, payload: bytes):
        py_samples = self._py.handle_audio(payload)
        native_samples = self._native.handle_audio(payload)
        self._check("handle_audio", list(py_samples), list(native_samples))
        return py_samples

    def mic_open_command(self) -> bytes:
        py_bytes = self._py.mic_open_command()
        native_bytes = self._native.mic_open_command()
        self._check("mic_open_command", py_bytes, native_bytes)
        return py_bytes

    def mic_close_command(self) -> bytes:
        py_bytes = self._py.mic_close_command()
        native_bytes = self._native.mic_close_command()
        self._check("mic_close_command", py_bytes, native_bytes)
        return py_bytes


def _event_to_dict(event: object) -> dict:
    """Convert a python-side ``ControlEvent`` dataclass (or a native
    dict already produced by the C++ binding) to a flat dict shape
    that compares equally across both implementations.

    Idempotent: feeding an already-normalized dict back in returns
    the same dict (so the shadow parity test can normalize both
    sides through this helper without losing structure)."""
    if isinstance(event, dict):
        # Native binding may emit ``{"capabilities": <C++ struct>}``;
        # normalize the inner value too so cross-impl equality works.
        if "capabilities" in event and not isinstance(
            event["capabilities"], dict
        ):
            caps = event["capabilities"]
            return {
                **event,
                "capabilities": {
                    "version": caps.version,
                    "codecs": caps.codecs,
                    "interaction": caps.interaction,
                    "frame_size": caps.frame_size,
                    "selected_codec": caps.selected_codec,
                    "sample_rate": caps.sample_rate,
                },
            }
        return event
    if isinstance(event, py_mod.CapsReceived):
        caps = event.capabilities
        return {
            "opcode": "Caps",
            "capabilities": {
                "version": caps.version,
                "codecs": caps.codecs,
                "interaction": caps.interaction,
                "frame_size": caps.frame_size,
                "selected_codec": caps.selected_codec,
                "sample_rate": caps.sample_rate,
            },
        }
    if isinstance(event, py_mod.MicButtonPressed):
        return {"opcode": "MicButton"}
    if isinstance(event, py_mod.AudioStarted):
        return {
            "opcode": "AudioStart",
            "session_id": event.session_id,
        }
    if isinstance(event, py_mod.AudioStopped):
        return {"opcode": "AudioStop"}
    if isinstance(event, py_mod.AudioSynced):
        return {"opcode": "AudioSync"}
    if isinstance(event, py_mod.UnknownControl):
        return {"opcode": "Unknown", "raw_opcode": event.opcode}
    return {"opcode": repr(event)}


def _dict_to_event(event: object) -> object:
    """Translate the binding's stable dict ABI back to the Python event ABI.

    ``app.py`` deliberately dispatches with ``isinstance`` against these
    dataclasses. Returning the raw binding dict made native mode connect and
    decode successfully while silently ignoring CAPS/AUDIO_START/AUDIO_STOP
    in the actual product path.
    """
    if not isinstance(event, dict):
        return event
    opcode = event.get("opcode")
    if opcode == "Caps":
        caps = event.get("capabilities")
        if caps is None:
            raise py_mod.ATVVProtocolError("native CAPS event omitted capabilities")
        return py_mod.CapsReceived(
            py_mod.proto.ATVVCapabilities(
                version=int(caps.version),
                codecs=int(caps.codecs),
                interaction=int(caps.interaction),
                frame_size=int(caps.frame_size),
                selected_codec=int(caps.selected_codec),
                sample_rate=float(caps.sample_rate),
            )
        )
    if opcode == "MicButton":
        return py_mod.MicButtonPressed()
    if opcode == "AudioStart":
        return py_mod.AudioStarted(session_id=event.get("session_id"))
    if opcode == "AudioStop":
        return py_mod.AudioStopped()
    if opcode == "AudioSync":
        return py_mod.AudioSynced()
    if opcode == "Unknown":
        return py_mod.UnknownControl(opcode=int(event.get("raw_opcode", 0)))
    raise py_mod.ATVVProtocolError(f"unknown native control event: {opcode!r}")


make_atvv_session = choose_implementation(
    "atvv_session",
    python_impl=_make_atvv_session_python,
    native_impl=_make_atvv_session_native,
    side_effect_free=True,
)


__all__ = [
    "make_atvv_session",
    "make_atvv_session_python",
    "make_atvv_session_native",
]
