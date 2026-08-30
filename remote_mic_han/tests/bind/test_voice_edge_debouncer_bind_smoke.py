"""Phase 3 / ADR-0013 §3.2 step 3: VoiceEdgeDebouncer binding smoke.

Loads the bundled ``remotemic_native._C`` extension and asserts that
the C++ ``VoiceEdgeDebouncer`` exposes the same surface the python
implementation does: a constructor taking ``release_window_ms`` plus
``on_press`` / ``on_release`` / ``shutdown`` /
``fire_pending_now_for_test`` methods and a ``release_window_ms``
property.

The binding plugs a no-op timer factory at the seam (per ADR-0013 §3.2
"thread-safe mutex" caveat), so the production-side timing comes from
the python bridge wrapper's ``threading.Timer``-backed factory. The
smoke test exercises the contract that
``fire_pending_now_for_test`` actually fires the held handler.

This is the build-time parity proof for the binding seam; the runtime
shadow parity test (``tests/test_voice_edge_debouncer_native_parity.py``)
is step 4's job.
"""

from __future__ import annotations

import unittest


class VoiceEdgeDebouncerBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C

    def test_voice_edge_debouncer_class_exposes_required_methods(self) -> None:
        for name in (
            "on_press",
            "on_release",
            "shutdown",
            "fire_pending_now_for_test",
        ):
            self.assertTrue(
                hasattr(self._C.VoiceEdgeDebouncer, name),
                f"VoiceEdgeDebouncer missing method {name!r}",
            )
        self.assertTrue(
            hasattr(self._C.VoiceEdgeDebouncer, "release_window_ms")
        )

    def test_default_release_window_is_200ms(self) -> None:
        d = self._C.VoiceEdgeDebouncer()
        self.assertEqual(d.release_window_ms, 200)

    def test_custom_release_window(self) -> None:
        d = self._C.VoiceEdgeDebouncer(300)
        self.assertEqual(d.release_window_ms, 300)

    def test_release_window_in_range(self) -> None:
        # The C++ binding accepts the same [50ms, 500ms] range the
        # python baseline enforces; values outside are rejected
        # loudly rather than silently clamped.
        self._C.VoiceEdgeDebouncer(50)
        self._C.VoiceEdgeDebouncer(500)

    def test_release_window_below_50ms_raises(self) -> None:
        with self.assertRaises(ValueError):
            self._C.VoiceEdgeDebouncer(49)

    def test_release_window_above_500ms_raises(self) -> None:
        with self.assertRaises(ValueError):
            self._C.VoiceEdgeDebouncer(501)

    def test_fire_pending_now_for_test_runs_handler(self) -> None:
        d = self._C.VoiceEdgeDebouncer(200)
        fired: list[int] = []
        d.on_release(lambda: fired.append(1))
        self.assertTrue(d.fire_pending_now_for_test())
        self.assertEqual(fired, [1])

    def test_on_press_invalidates_pending_handler(self) -> None:
        d = self._C.VoiceEdgeDebouncer(200)
        fired: list[int] = []
        d.on_release(lambda: fired.append(1))
        d.on_press()
        self.assertFalse(d.fire_pending_now_for_test())
        self.assertEqual(fired, [])

    def test_shutdown_invalidates_pending_handler(self) -> None:
        d = self._C.VoiceEdgeDebouncer(200)
        fired: list[int] = []
        d.on_release(lambda: fired.append(1))
        d.shutdown()
        self.assertFalse(d.fire_pending_now_for_test())
        self.assertEqual(fired, [])


if __name__ == "__main__":
    unittest.main()