"""Phase 3 / ADR-0013 §3.1 step 4: runtime shadow parity test for
VoiceController (G5 of ADR-0013).

When ``REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER=shadow`` is set, the
python-side ``VoiceController`` (``ovb_rc003.voice_controller``) and the
C++-side ``VoiceController`` (``remotemic_native._C.VoiceController``
through ``ovb_rc003.voice_controller_native.make_voice_controller``)
must yield byte-exact state equality (active / holding flags and
identical returned ``VoiceHostAction`` per call).

Hard rule from the user (phase 3 entry scope): no tolerance.
Any drift fails this test and per ADR-0013 §6 that aborts Phase 3
Area 1 entirely.

The test is skipped if the binding is unavailable
(``_C_AVAILABLE == False``) so source-tree imports without a CMake build
still get a green test suite.

Env-leak safety: the env override is set inside ``setUpClass`` and
restored in ``tearDownClass`` (NOT at module top), matching the
corrective fix pattern from commit 5ce9bd5.
"""

from __future__ import annotations

import os
import unittest
from typing import List

from ovb_rc003 import voice_controller as py_mod
from ovb_rc003.voice_controller_native import make_voice_controller


_SCENARIOS: List[dict] = [
    {
        "name": "toggle: press -> stop",
        "mode": py_mod.VoiceTriggerMode.TOGGLE,
        "script": [
            ("press",),
            ("active",),
            ("stop",),
            ("active",),
        ],
    },
    {
        "name": "hold: press -> stop",
        "mode": py_mod.VoiceTriggerMode.HOLD,
        "script": [
            ("press",),
            ("holding",),
            ("stop",),
            ("holding",),
        ],
    },
    {
        "name": "toggle: press -> stop -> press -> reset",
        "mode": py_mod.VoiceTriggerMode.TOGGLE,
        "script": [
            ("press",),
            ("stop",),
            ("press",),
            ("reset",),
            ("active",),
        ],
    },
    {
        "name": "hold: press -> reset",
        "mode": py_mod.VoiceTriggerMode.HOLD,
        "script": [
            ("press",),
            ("reset",),
            ("holding",),
            ("active",),
        ],
    },
    {
        "name": "toggle: stop without press is no-op",
        "mode": py_mod.VoiceTriggerMode.TOGGLE,
        "script": [
            ("stop",),
            ("active",),
        ],
    },
    {
        "name": "toggle: cancel_pending clears state",
        "mode": py_mod.VoiceTriggerMode.TOGGLE,
        "script": [
            ("press",),
            ("cancel",),
            ("active",),
            ("stop",),
            ("active",),
        ],
    },
    {
        "name": "hold: restore_pending(KeyUp) re-arms",
        "mode": py_mod.VoiceTriggerMode.HOLD,
        "script": [
            ("press",),
            ("stop",),
            ("restore_keyup",),
            ("holding",),
            ("active",),
        ],
    },
    {
        "name": "toggle: restore_pending(Tap) re-arms",
        "mode": py_mod.VoiceTriggerMode.TOGGLE,
        "script": [
            ("press",),
            ("stop",),
            ("restore_tap",),
            ("active",),
        ],
    },
]


class VoiceControllerNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # Env-leak safety: capture and restore, NEVER module-level
        # os.environ.setdefault. The earlier 5ce9bd5 fix established
        # this pattern; Phase 3 tests must follow it.
        cls._env_was_set = (
            "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER" in os.environ
        )
        cls._env_old = os.environ.get(
            "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"
        )
        os.environ["REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"] = "shadow"

        import remotemic_native as _rn  # type: ignore[import-not-found]

        cls._rn = _rn
        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; "
                "shadow parity skipped"
            )

    @classmethod
    def tearDownClass(cls) -> None:
        if cls._env_was_set:
            os.environ["REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"] = (
                cls._env_old
            )
        else:
            os.environ.pop(
                "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER", None
            )

    def _script(self, ctrl, script):
        trace: List[object] = []
        for step in script:
            op = step[0]
            if op == "press":
                trace.append(ctrl.on_mic_button_pressed())
            elif op == "stop":
                trace.append(ctrl.on_audio_stopped())
            elif op == "reset":
                trace.append(ctrl.reset())
            elif op == "active":
                trace.append(ctrl.active)
            elif op == "holding":
                trace.append(ctrl.holding)
            elif op == "cancel":
                ctrl.cancel_pending()
            elif op == "restore_keyup":
                ctrl.restore_pending(py_mod.VoiceHostAction.KEY_UP)
            elif op == "restore_tap":
                ctrl.restore_pending(py_mod.VoiceHostAction.TAP)
            else:
                self.fail(f"unknown script op: {op!r}")
        return trace

    def test_all_scenarios_drive_python_and_native(self) -> None:
        for scenario in _SCENARIOS:
            with self.subTest(scenario=scenario["name"]):
                py_ctrl = py_mod.VoiceController(scenario["mode"])
                native_ctrl = make_voice_controller(scenario["mode"])
                py_trace = self._script(py_ctrl, scenario["script"])
                native_trace = self._script(
                    native_ctrl, scenario["script"]
                )
                self.assertEqual(
                    py_trace,
                    native_trace,
                    f"scenario {scenario['name']!r}: "
                    f"python={py_trace!r} native={native_trace!r}",
                )


if __name__ == "__main__":
    unittest.main()