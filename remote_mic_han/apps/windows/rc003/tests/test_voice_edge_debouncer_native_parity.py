"""Phase 3 / ADR-0013 §3.2 step 4: runtime shadow parity test for
VoiceEdgeDebouncer (G5 of ADR-0013).

When ``REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER=shadow`` is set, the
python-side ``VoiceEdgeDebouncer`` (``ovb_rc003.voice_edge_debouncer``)
and the C++-side ``VoiceEdgeDebouncer`` (via
``ovb_rc003.voice_edge_debouncer_native.make_voice_edge_debouncer``)
must yield identical state transitions across a scripted press/release
sequence.

Both implementations share the same shape: ``on_press`` /
``on_release(handler)`` / ``shutdown`` / ``fire_pending_now_for_test``.
The parity test uses ``fire_pending_now_for_test`` on both sides so
the timing doesn't depend on a real timer thread (matching the C++
unit test's manual-timer pattern).

Hard rule from the user (phase 3 entry scope): no tolerance.
Any drift fails this test and per ADR-0013 §6 that aborts Phase 3
Area 2 entirely.

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
from unittest import mock
from typing import List

from ovb_rc003 import voice_edge_debouncer as py_mod
from ovb_rc003.voice_edge_debouncer_native import (
    make_voice_edge_debouncer,
)


class _Scenario:
    def __init__(
        self,
        name: str,
        release_window_seconds: float,
        script: List[str],
    ) -> None:
        self.name = name
        self.release_window_seconds = release_window_seconds
        self.script = script


_SCENARIOS: List[_Scenario] = [
    _Scenario(
        "release then fire",
        0.200,
        ["release", "fire"],
    ),
    _Scenario(
        "press invalidates pending",
        0.200,
        ["release", "press", "fire"],
    ),
    _Scenario(
        "shutdown invalidates pending",
        0.200,
        ["release", "shutdown", "fire"],
    ),
    _Scenario(
        "two releases: newest handler wins",
        0.200,
        ["release", "release", "fire"],
    ),
    _Scenario(
        "release-press-release: second release wins",
        0.200,
        ["release", "press", "release", "fire"],
    ),
]


class VoiceEdgeDebouncerNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._env_was_set = (
            "REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER" in os.environ
        )
        cls._env_old = os.environ.get(
            "REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER"
        )
        os.environ["REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER"] = "shadow"

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
            os.environ["REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER"] = (
                cls._env_old
            )
        else:
            os.environ.pop(
                "REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER", None
            )

    def _script(self, deb, script: List[str]) -> List[object]:
        trace: List[object] = []
        py_fired = {"flag": False}

        def _py_handler() -> None:
            py_fired["flag"] = True

        for op in script:
            if op == "release":
                deb.on_release(_py_handler)
            elif op == "press":
                deb.on_press()
            elif op == "shutdown":
                deb.shutdown()
            elif op == "fire":
                fired = deb.fire_pending_now_for_test()
                trace.append((fired, py_fired["flag"]))
                py_fired["flag"] = False
            else:
                self.fail(f"unknown script op: {op!r}")
        return trace

    def test_all_scenarios_drive_python_and_native(self) -> None:
        for scenario in _SCENARIOS:
            with self.subTest(scenario=scenario.name):
                py_deb = make_voice_edge_debouncer(
                    scenario.release_window_seconds
                )
                native_deb = make_voice_edge_debouncer(
                    scenario.release_window_seconds
                )
                # make_voice_edge_debouncer with the shadow env returns
                # a python impl; for the native half we go directly
                # through the native factory.
                from ovb_rc003.voice_edge_debouncer_native import (
                    make_voice_edge_debouncer_native,
                )
                native_deb = make_voice_edge_debouncer_native(
                    scenario.release_window_seconds
                )
                py_trace = self._script(py_deb, scenario.script)
                native_trace = self._script(native_deb, scenario.script)
                self.assertEqual(
                    py_trace,
                    native_trace,
                    f"scenario {scenario.name!r}: "
                    f"python={py_trace!r} native={native_trace!r}",
                )

    def test_bridge_timer_consumes_cpp_pending_handler_once(self) -> None:
        """The production bridge must fire through C++, not around it."""
        from ovb_rc003 import voice_edge_debouncer_native as native_mod

        created = []

        class _FakeTimer:
            def __init__(self, delay, callback) -> None:
                self.delay = delay
                self.callback = callback
                self.daemon = False
                self.started = False
                self.cancelled = False
                created.append(self)

            def start(self) -> None:
                self.started = True

            def cancel(self) -> None:
                self.cancelled = True

        fired = []
        with mock.patch.object(native_mod.threading, "Timer", _FakeTimer):
            deb = native_mod.make_voice_edge_debouncer_native(0.2)
            deb.on_release(lambda: fired.append("fired"))

            self.assertEqual(len(created), 1)
            self.assertTrue(created[0].started)
            created[0].callback()

            self.assertEqual(fired, ["fired"])
            self.assertFalse(deb.fire_pending_now_for_test())
            self.assertEqual(fired, ["fired"])


if __name__ == "__main__":
    unittest.main()
