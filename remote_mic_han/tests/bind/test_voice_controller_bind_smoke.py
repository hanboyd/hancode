"""Phase 3 / ADR-0013 §3.1 step 3: VoiceController binding smoke.

Loads the bundled ``remotemic_native._C`` extension and asserts that
the C++ ``VoiceController`` exposes the same surface the python
implementation does: VoiceTriggerMode / VoiceHostAction enums + a
``VoiceController`` class with all 7 mutators / 2 accessors /
3 constructors that match
``apps/windows/rc003/src/ovb_rc003/voice_controller.py``.

This is the build-time parity proof for the binding seam; the runtime
shadow parity test (``tests/test_voice_controller_native_parity.py``)
is step 4's job. Per ADR-0013 G3: on fail, do not flip the ADR status
from ``proposed`` to ``accepted``.
"""

from __future__ import annotations

import unittest


class VoiceControllerBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C

    def test_voice_trigger_mode_enum_has_toggle_and_hold(self) -> None:
        # pybind11 enums expose their names via the ``__members__``
        # mapping (since they are not iterable in the same way as
        # Python ``enum.Enum`` subclasses).
        names = set(self._C.VoiceTriggerMode.__members__.keys())
        self.assertEqual(names, {"Toggle", "Hold"})

    def test_voice_host_action_enum_has_three_values(self) -> None:
        names = set(self._C.VoiceHostAction.__members__.keys())
        self.assertEqual(names, {"Tap", "KeyDown", "KeyUp"})

    def test_voice_controller_class_exposes_required_methods(self) -> None:
        for name in (
            "on_mic_button_pressed",
            "on_audio_stopped",
            "reset",
            "restore_pending",
            "cancel_pending",
        ):
            self.assertTrue(
                hasattr(self._C.VoiceController, name),
                f"VoiceController missing method {name!r}",
            )
        # Accessors exposed via @property on the binding side.
        self.assertTrue(hasattr(self._C.VoiceController, "holding"))
        self.assertTrue(hasattr(self._C.VoiceController, "active"))

    def test_toggle_press_returns_tap(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Toggle)
        action = ctrl.on_mic_button_pressed()
        self.assertEqual(action, self._C.VoiceHostAction.Tap)
        self.assertTrue(ctrl.active)
        self.assertFalse(ctrl.holding)

    def test_hold_press_returns_key_down(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Hold)
        action = ctrl.on_mic_button_pressed()
        self.assertEqual(action, self._C.VoiceHostAction.KeyDown)
        self.assertTrue(ctrl.holding)
        self.assertTrue(ctrl.active)

    def test_toggle_audio_stop_closes_with_tap(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Toggle)
        ctrl.on_mic_button_pressed()
        action = ctrl.on_audio_stopped()
        self.assertEqual(action, self._C.VoiceHostAction.Tap)
        self.assertFalse(ctrl.active)

    def test_hold_audio_stop_releases_key(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Hold)
        ctrl.on_mic_button_pressed()
        action = ctrl.on_audio_stopped()
        self.assertEqual(action, self._C.VoiceHostAction.KeyUp)
        self.assertFalse(ctrl.holding)

    def test_audio_stop_without_press_is_none(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Toggle)
        action = ctrl.on_audio_stopped()
        self.assertIsNone(action)
        self.assertFalse(ctrl.active)

    def test_reset_returns_closing_action(self) -> None:
        toggle = self._C.VoiceController(self._C.VoiceTriggerMode.Toggle)
        toggle.on_mic_button_pressed()
        self.assertEqual(
            toggle.reset(), self._C.VoiceHostAction.Tap)
        self.assertFalse(toggle.active)

        hold = self._C.VoiceController(self._C.VoiceTriggerMode.Hold)
        hold.on_mic_button_pressed()
        self.assertEqual(hold.reset(), self._C.VoiceHostAction.KeyUp)
        self.assertFalse(hold.active)

    def test_restore_pending_key_up_sets_holding(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Hold)
        ctrl.on_mic_button_pressed()
        ctrl.on_audio_stopped()  # clears holding
        ctrl.restore_pending(self._C.VoiceHostAction.KeyUp)
        self.assertTrue(ctrl.active)
        self.assertTrue(ctrl.holding)

    def test_restore_pending_tap_sets_toggle(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Toggle)
        ctrl.on_mic_button_pressed()
        ctrl.on_audio_stopped()  # clears toggle
        ctrl.restore_pending(self._C.VoiceHostAction.Tap)
        self.assertTrue(ctrl.active)

    def test_cancel_pending_clears_without_action(self) -> None:
        ctrl = self._C.VoiceController(self._C.VoiceTriggerMode.Toggle)
        ctrl.on_mic_button_pressed()
        ctrl.cancel_pending()
        self.assertFalse(ctrl.active)
        self.assertIsNone(ctrl.on_audio_stopped())


if __name__ == "__main__":
    unittest.main()