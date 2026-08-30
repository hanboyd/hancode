"""Phase 3 / ADR-0013 §3.1: module-level switch for VoiceController.

Exposes ``make_voice_controller(trigger_mode) -> VoiceController`` which
dispatches via ``choose_implementation`` to either:

  * ``python``: ``ovb_rc003.voice_controller.VoiceController`` (the
    pure-Python state machine mirrored in
    C++ ``remotemic::voice::VoiceController``)
  * ``native``: ``remotemic_native._C.VoiceController`` (the pybind11
    binding)
  * ``shadow``: runs both with identical inputs and asserts byte-exact
    state parity (active / holding flags and identical returned
    ``VoiceHostAction`` per call). Mismatch raises RuntimeError.

The default is ``python`` (per migration plan §1 rule 4). Switch via:

    REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER=native
    REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER=shadow
    REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER=python  # default

Both python and native returns are normalized to the
``VoiceHostAction`` enum so callers do not have to know which
implementation ran. ``VoiceController`` is a stateful object; the
``shadow`` mode runs the python side and the native side through the
same script and asserts equality after every mutating call.
"""

from __future__ import annotations

from . import voice_controller as py_mod
from ._remotemic_native_runtime import choose_implementation


def _to_py_action(action: object) -> py_mod.VoiceHostAction:
    """Normalize a ``_C.VoiceHostAction`` enum (or python enum) to the
    Python ``VoiceHostAction`` str-enum so callers get a stable shape
    regardless of which implementation actually ran."""
    if isinstance(action, py_mod.VoiceHostAction):
        return action
    # pybind11-bound enum (str-coercible)
    name = str(action).split(".")[-1]
    return py_mod.VoiceHostAction(name.lower())


class _NativeVoiceController:
    """Thin shim over ``remotemic_native._C.VoiceController`` that
    exposes the same Python surface (``holding`` / ``active`` properties
    and the same mutator methods) so the bridge wrapper can stay
    implementation-agnostic."""

    def __init__(self, trigger_mode: py_mod.VoiceTriggerMode) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        if not _rn._C_AVAILABLE:
            # Fallback to the python baseline so a missing .pyd does
            # not break callers.
            self._impl = py_mod.VoiceController(trigger_mode)
            self._is_native = False
            # Mirror python baseline surface (``voice_controller.py:46``
            # sets the same attribute on the python class so that
            # downstream reads like ``app.py:583`` work uniformly).
            self.trigger_mode = trigger_mode
            return
        mode = (
            _rn.VoiceTriggerMode.Hold
            if trigger_mode == py_mod.VoiceTriggerMode.HOLD
            else _rn.VoiceTriggerMode.Toggle
        )
        self._impl = _rn.VoiceController(mode)
        self._is_native = True
        # Same python-baseline parity as above; without this, app.py's
        # ``self._voice.trigger_mode == VoiceTriggerMode.HOLD`` reads
        # (583/1163/1176/1193) raise ``AttributeError`` when production
        # routes through this shim.
        self.trigger_mode = trigger_mode

    @property
    def holding(self) -> bool:
        return bool(self._impl.holding)

    @property
    def active(self) -> bool:
        return bool(self._impl.active)

    def on_mic_button_pressed(self) -> py_mod.VoiceHostAction:
        return _to_py_action(self._impl.on_mic_button_pressed())

    def on_audio_stopped(self):
        result = self._impl.on_audio_stopped()
        return _to_py_action(result) if result is not None else None

    def reset(self):
        result = self._impl.reset()
        return _to_py_action(result) if result is not None else None

    def restore_pending(self, action: py_mod.VoiceHostAction) -> None:
        c_action = {
            py_mod.VoiceHostAction.TAP: "Tap",
            py_mod.VoiceHostAction.KEY_DOWN: "KeyDown",
            py_mod.VoiceHostAction.KEY_UP: "KeyUp",
        }[action]
        self._impl.restore_pending(c_action)

    def cancel_pending(self) -> None:
        self._impl.cancel_pending()


def _make_voice_controller_python(
    trigger_mode: py_mod.VoiceTriggerMode,
) -> py_mod.VoiceController:
    return py_mod.VoiceController(trigger_mode)


def _make_voice_controller_native(
    trigger_mode: py_mod.VoiceTriggerMode,
) -> object:
    return _NativeVoiceController(trigger_mode)


make_voice_controller_python = _make_voice_controller_python
make_voice_controller_native = _make_voice_controller_native


# Stateful module: ``shadow`` runs both with the same script and
# asserts identity at every step. ADR-0013 §3.1 keeps this strictly
# compute (no I/O), so shadow parity is permitted.
def _make_voice_controller_shadow(
    trigger_mode: py_mod.VoiceTriggerMode,
) -> py_mod.VoiceController:
    py_ctrl = py_mod.VoiceController(trigger_mode)
    native_ctrl = _NativeVoiceController(trigger_mode)
    return _ShadowVoiceController(py_ctrl, native_ctrl, trigger_mode)


class _ShadowVoiceController:
    """Drives python and native controllers with the same script and
    asserts identity after every mutating call. Reads always return the
    python side (the python side is authoritative)."""

    def __init__(
        self,
        py_ctrl: py_mod.VoiceController,
        native_ctrl: _NativeVoiceController,
        trigger_mode: py_mod.VoiceTriggerMode,
    ) -> None:
        self._py = py_ctrl
        self._native = native_ctrl
        self._trigger_mode = trigger_mode

    @staticmethod
    def _check(name: str, expected: object, actual: object) -> None:
        if expected != actual:
            raise RuntimeError(
                f"shadow(voice_controller).{name}: "
                f"python={expected!r} native={actual!r}"
            )

    def _assert_parity(self) -> None:
        self._check("holding", self._py.holding, self._native.holding)
        self._check("active",  self._py.active,  self._native.active)

    @property
    def holding(self) -> bool:
        return self._py.holding

    @property
    def active(self) -> bool:
        return self._py.active

    def on_mic_button_pressed(self) -> py_mod.VoiceHostAction:
        py_action = self._py.on_mic_button_pressed()
        native_action = self._native.on_mic_button_pressed()
        self._check("on_mic_button_pressed", py_action, native_action)
        self._assert_parity()
        return py_action

    def on_audio_stopped(self):
        py_action = self._py.on_audio_stopped()
        native_action = self._native.on_audio_stopped()
        self._check("on_audio_stopped", py_action, native_action)
        self._assert_parity()
        return py_action

    def reset(self):
        py_action = self._py.reset()
        native_action = self._native.reset()
        self._check("reset", py_action, native_action)
        self._assert_parity()
        return py_action

    def restore_pending(self, action: py_mod.VoiceHostAction) -> None:
        self._py.restore_pending(action)
        self._native.restore_pending(action)
        self._assert_parity()

    def cancel_pending(self) -> None:
        self._py.cancel_pending()
        self._native.cancel_pending()
        self._assert_parity()


make_voice_controller = choose_implementation(
    "voice_controller",
    python_impl=_make_voice_controller_python,
    native_impl=_make_voice_controller_native,
    side_effect_free=True,
)


__all__ = [
    "make_voice_controller",
    "make_voice_controller_python",
    "make_voice_controller_native",
]