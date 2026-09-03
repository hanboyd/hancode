"""Phase 3 / ADR-0013 §6 step 5: native switch + fake-backend shadow
verification.

This file exercises two invariants the bridge wrappers must satisfy
beyond the byte-exact shadow parity check (step 4):

  1. Single-session owner: each call to ``make_*`` factory returns a
     FRESH instance. Two calls must produce independent objects;
     state never leaks across factory invocations (the contract the
     app.py owner relies on so reconnect / cleanup never reuses a
     stale session / debouncer / controller).

  2. Bridge wrappers are pure compute / no side effects. A fake
     backend that mimics the C++ binding's surface (no threads, no
     I/O, deterministic state) is plugged into ``remotemic_native``
     via a runtime monkey-patch; the bridge wrapper's ``native_impl``
     is then exercised and the fake is asserted to be the path
     actually taken. This catches accidental globals, shared state,
     and side-effecting helpers at the bridge seam.

Both checks are scoped to the bridge wrappers only; the C++ binding
itself is already proven by the binding smokes + shadow parity tests.

Env-leak safety: every env override is set inside setUpClass and
restored in tearDownClass (NOT at module top), matching the
corrective fix pattern from commit 5ce9bd5.
"""

from __future__ import annotations

import os
import types
import unittest
from typing import List

from ovb_rc003 import (
    atvv_session as py_atvv,
    voice_controller as py_vc,
)
from ovb_rc003.atvv_session_native import make_atvv_session
from ovb_rc003.voice_controller_native import make_voice_controller


class _FakeVoiceController:
    """Fake backend that mimics ``remotemic_native.VoiceController``
    surface; records every method call so the bridge wrapper's
    native path is observable from the test."""

    instances: List["_FakeVoiceController"] = []

    def __init__(self, mode: object) -> None:
        self.mode = mode
        self.active = False
        self.holding = False
        self.toggle_active = False
        self.calls: List[str] = []
        _FakeVoiceController.instances.append(self)

    def on_mic_button_pressed(self) -> object:
        self.calls.append("on_mic_button_pressed")
        self.active = True
        self.holding = True
        # Use the python-baseline's snake_case str-enum values so
        # the bridge wrapper's ``_to_py_action`` can round-trip
        # without remapping (the bridge strips the ``KeyDown`` ->
        # ``key_down`` form).
        return "key_down"

    def on_audio_stopped(self) -> object:
        self.calls.append("on_audio_stopped")
        self.active = False
        self.holding = False
        return "key_up"

    def reset(self) -> object:
        self.calls.append("reset")
        self.active = False
        self.holding = False
        return "key_up"

    def restore_pending(self, action: object) -> None:
        self.calls.append(f"restore_pending:{action}")

    def cancel_pending(self) -> None:
        self.calls.append("cancel_pending")
        self.active = False
        self.holding = False


class _FakeAtvvSession:
    """Fake backend that mimics ``remotemic_native.AtvvSession``
    surface; records every method call so the bridge wrapper's
    native path is observable from the test."""

    instances: List["_FakeAtvvSession"] = []

    def __init__(self, gain_db: float = 10.0) -> None:
        self.gain_db = gain_db
        self.calls: List[str] = []
        _FakeAtvvSession.instances.append(self)

    @property
    def capabilities(self):
        self.calls.append("capabilities")
        return None

    @property
    def mic_open(self) -> bool:
        self.calls.append("mic_open")
        return False

    def handle_control(self, payload: bytes) -> dict:
        self.calls.append("handle_control")
        return {"opcode": "Unknown", "raw_opcode": payload[0] if payload else 0}

    def handle_audio(self, payload: bytes) -> list:
        self.calls.append("handle_audio")
        return []

    def mic_open_command(self) -> bytes:
        self.calls.append("mic_open_command")
        return b""

    def mic_close_command(self) -> bytes:
        self.calls.append("mic_close_command")
        return b""


class NativeSwitchAndFakeBackendTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # Backup the env override for the two module keys we exercise.
        cls._env_was_set_vc = (
            "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER" in os.environ
        )
        cls._env_old_vc = os.environ.get(
            "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"
        )
        cls._env_was_set_atvv = (
            "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION" in os.environ
        )
        cls._env_old_atvv = os.environ.get(
            "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION"
        )

    @classmethod
    def tearDownClass(cls) -> None:
        for was_set, old, key in (
            (
                cls._env_was_set_vc,
                cls._env_old_vc,
                "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER",
            ),
            (
                cls._env_was_set_atvv,
                cls._env_old_atvv,
                "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION",
            ),
        ):
            if was_set:
                os.environ[key] = old
            else:
                os.environ.pop(key, None)

    # ---- Single-session owner contract --------------------------------

    def test_make_voice_controller_returns_fresh_instances(self) -> None:
        os.environ["REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"] = "python"
        a = make_voice_controller(py_vc.VoiceTriggerMode.TOGGLE)
        b = make_voice_controller(py_vc.VoiceTriggerMode.TOGGLE)
        # Two calls must produce two distinct objects (no shared
        # session); state never leaks across factory invocations.
        self.assertIsNot(a, b)
        a.on_mic_button_pressed()
        self.assertFalse(
            b.active,
            "second factory call must NOT see first call's state",
        )

    def test_make_atvv_session_returns_fresh_instances(self) -> None:
        os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION"] = "python"
        a = make_atvv_session()
        b = make_atvv_session()
        self.assertIsNot(a, b)
        a.mic_open_command()  # mutates state internally? no; just reads
        # b's capabilities must be independent of a's.
        self.assertEqual(a.capabilities, None)
        self.assertEqual(b.capabilities, None)

    # ---- Native switch round trip -------------------------------------

    def test_native_choice_changes_at_runtime(self) -> None:
        # python path -> produces a python ``VoiceController`` instance.
        os.environ["REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"] = "python"
        py_instance = make_voice_controller(
            py_vc.VoiceTriggerMode.TOGGLE
        )
        self.assertIsInstance(py_instance, py_vc.VoiceController)

        # native path -> produces the bridge shim. We don't have the
        # .pyd at hand in this test, but the factory must NOT raise
        # even when native is unavailable (the bridge falls back to
        # python transparently per ``_C_AVAILABLE`` semantics).
        os.environ["REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"] = "native"
        native_or_fallback = make_voice_controller(
            py_vc.VoiceTriggerMode.TOGGLE
        )
        self.assertIsNotNone(native_or_fallback)

    # ---- Fake backend: bridge wrapper's native path is pure compute ---

    def test_bridge_native_impl_invokes_fake_voice_controller(self) -> None:
        # Monkey-patch ``remotemic_native._C.VoiceController`` so the
        # bridge wrapper's ``native_impl`` (which imports it from the
        # public package) routes through the fake. The bridge must
        # produce a ``_NativeVoiceController`` instance whose calls
        # go to the fake.
        import remotemic_native as _rn

        original = getattr(_rn, "VoiceController", None)
        original_mode = getattr(_rn, "VoiceTriggerMode", None)
        original_available = _rn._C_AVAILABLE
        _FakeVoiceController.instances.clear()
        _rn._C_AVAILABLE = True
        _rn.VoiceTriggerMode = types.SimpleNamespace(Hold="Hold", Toggle="Toggle")
        _rn.VoiceController = _FakeVoiceController  # type: ignore[assignment]
        try:
            from ovb_rc003.voice_controller_native import (
                make_voice_controller_native,
            )
            shim = make_voice_controller_native(
                py_vc.VoiceTriggerMode.HOLD
            )
            self.assertEqual(len(_FakeVoiceController.instances), 1)
            action = shim.on_mic_button_pressed()
            self.assertEqual(action, py_vc.VoiceHostAction.KEY_DOWN)
            self.assertIsInstance(action, py_vc.VoiceHostAction)
            self.assertEqual(
                _FakeVoiceController.instances[0].calls,
                ["on_mic_button_pressed"],
            )
        finally:
            _rn._C_AVAILABLE = original_available
            _rn.VoiceTriggerMode = original_mode
            if original is None:
                del _rn.VoiceController
            else:
                _rn.VoiceController = original

    def test_bridge_native_impl_invokes_fake_atvv_session(self) -> None:
        import remotemic_native as _rn

        original = getattr(_rn, "AtvvSession", None)
        original_available = _rn._C_AVAILABLE
        _FakeAtvvSession.instances.clear()
        _rn._C_AVAILABLE = True
        _rn.AtvvSession = _FakeAtvvSession  # type: ignore[assignment]
        try:
            from ovb_rc003.atvv_session_native import (
                make_atvv_session_native,
            )
            shim = make_atvv_session_native()
            self.assertEqual(len(_FakeAtvvSession.instances), 1)
            event = shim.handle_control(b"\x08")
            # Native shim now restores the public Python event ABI instead of
            # leaking the private binding dict into app.py's isinstance-based
            # dispatcher.
            self.assertIsInstance(event, py_atvv.UnknownControl)
            self.assertEqual(event.opcode, 0x08)
            self.assertIn(
                "handle_control", _FakeAtvvSession.instances[0].calls
            )
        finally:
            _rn._C_AVAILABLE = original_available
            if original is None:
                del _rn.AtvvSession
            else:
                _rn.AtvvSession = original


if __name__ == "__main__":
    unittest.main()
