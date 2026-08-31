"""Phase 4 / ADR-0014 §6 step 4: byte-exact parity between the python
3-tap linear interpolation in ``audio_playback.py:154-172`` and the
C++ ``remotemic::audio::upsample_16k_to_48k`` exposed via
``remotemic_native._C``.

Per ADR-0014 §6 step 4: ``upsample_16k_to_48k_native_matches_python_byte_exact``.
Both sides are driven with identical input sequences (including the
carry-over previous-sample state across calls so the parity covers
the real BLE-notification-cadence use case). Output is asserted
byte-equal via ``self.assertEqual``.

This is build-time parity proof only. Real-device G6 validation
(Typeless + RC003) is the step-5/6 deliverable.

Skipped when ``remotemic_native._C_AVAILABLE`` is False (no
``_C.pyd`` built locally) so a source-tree-only developer still gets
a green suite.
"""

from __future__ import annotations

import unittest
from typing import List, Optional


def _python_upsample(
    source: List[int], previous: Optional[int], have_previous: bool
) -> tuple[List[int], int, bool]:
    """Pure-python baseline that mirrors ``audio_playback.py:154-172``.

    Each source sample expands to (prev + round(delta/3),
    prev + round(2*delta/3), current), where prev is the previous
    source sample if have_previous else source[0]. Returns the
    expanded list plus the carry-over state so consecutive calls
    compose the same way the production code does across BLE
    notifications.
    """
    if not source:
        return [], previous if have_previous else 0, have_previous
    values = [int(v) for v in source]
    output: List[int] = []
    prev = int(previous) if have_previous else values[0]
    if not have_previous:
        output.extend([values[0], values[0], values[0]])
        start_index = 1
    else:
        start_index = 0
    for current in values[start_index:]:
        delta = current - prev
        output.append(prev + int(round(delta / 3.0)))
        output.append(prev + int(round(delta * (2.0 / 3.0))))
        output.append(current)
        prev = current
    return output, prev, True


class UpsampleParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        cls._rn = _rn
        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; "
                "upsample parity skipped"
            )
        # ``upsample_16k_to_48k`` and ``UpsampleState`` are test-only
        # surface (parity harness); access them via the private _C
        # module like the G3 bind smoke does.
        cls._native_upsample = _rn._C.upsample_16k_to_48k
        cls._UpsampleState = _rn._C.UpsampleState

    # ------------------------------------------------------------------
    # Edge cases: empty input, single sample, two-sample carry-over
    # ------------------------------------------------------------------

    def test_empty_source_returns_empty(self) -> None:
        state = self._UpsampleState()
        py_out, _, _ = _python_upsample([], 0, False)
        native_out = self._native_upsample([], state)
        self.assertEqual(py_out, list(native_out))
        self.assertEqual([], list(native_out))

    def test_single_sample_with_no_previous_outputs_triple(self) -> None:
        # When have_previous=False, prev defaults to source[0] -> delta
        # is 0 -> the first 3 outputs are all source[0]. This is the
        # d443d03-corrected behavior; earlier tests asserted a
        # wrong delta that would never trigger in production.
        state = self._UpsampleState()
        py_out, _, _ = _python_upsample([1000], 0, False)
        native_out = self._native_upsample([1000], state)
        self.assertEqual(py_out, list(native_out))
        self.assertEqual([1000, 1000, 1000], list(native_out))

    def test_single_sample_with_previous_uses_carry(self) -> None:
        # Steady-state: a previous sample is known; the next sample
        # expands to (prev + d/3, prev + 2d/3, current). This is the
        # common case during a voice session where the writer loop
        # has carry-over from the prior chunk.
        state = self._UpsampleState()
        state.previous_sample = 0
        state.have_previous = True
        py_out, py_prev, py_have = _python_upsample([1000], 0, True)
        native_out = self._native_upsample([1000], state)
        self.assertEqual(py_out, list(native_out))
        self.assertEqual(py_prev, state.previous_sample)
        self.assertTrue(state.have_previous)
        self.assertTrue(py_have)

    # ------------------------------------------------------------------
    # Multi-sample: from previous state + fresh batch
    # ------------------------------------------------------------------

    def test_multi_sample_with_no_previous_then_with_carry(self) -> None:
        # Two calls: first with no previous (delta=0 for first sample),
        # then with the carry-over state from the prior output. This
        # matches the BLE notification cadence where the writer loop
        # carries state between 20 ms chunks.
        py_state_prev = 0
        py_state_have = False
        native_state = self._UpsampleState()

        for batch in [[0, 1000, 2000, 3000], [4000, 5000]]:
            py_out, py_state_prev, py_state_have = _python_upsample(
                batch, py_state_prev, py_state_have
            )
            native_out = self._native_upsample(batch, native_state)
            self.assertEqual(
                py_out,
                list(native_out),
                f"batch={batch}: py={py_out} native={list(native_out)}",
            )
        self.assertEqual(py_state_prev, native_state.previous_sample)
        self.assertTrue(native_state.have_previous)

    def test_negative_values_round_trip_byte_exact(self) -> None:
        state = self._UpsampleState()
        py_out, _, _ = _python_upsample([-1000, -500, 0, 500, 1000], 0, False)
        native_out = self._native_upsample(
            [-1000, -500, 0, 500, 1000], state
        )
        self.assertEqual(py_out, list(native_out))

    def test_large_delta_saturates_to_int16(self) -> None:
        # delta near the int16 boundary should saturate rather than
        # overflow. Both sides must clamp at [-32768, 32767].
        state = self._UpsampleState()
        py_out, _, _ = _python_upsample([-32768, 32767], 0, False)
        native_out = self._native_upsample([-32768, 32767], state)
        self.assertEqual(py_out, list(native_out))
        for v in native_out:
            self.assertGreaterEqual(v, -32768)
            self.assertLessEqual(v, 32767)

    def test_repeated_calls_produce_same_output(self) -> None:
        # Determinism check: running the same input twice with a
        # fresh state must produce identical output (no time, no
        # random seed). Catches accidental non-determinism in the
        # C++ impl that the python baseline would never show.
        state_a = self._UpsampleState()
        state_b = self._UpsampleState()
        inputs = [123, -456, 789, -1011, 1213]
        out_a = self._native_upsample(list(inputs), state_a)
        out_b = self._native_upsample(list(inputs), state_b)
        self.assertEqual(list(out_a), list(out_b))
        self.assertEqual(state_a.previous_sample, state_b.previous_sample)

    def test_silence_input_produces_silence(self) -> None:
        state = self._UpsampleState()
        py_out, _, _ = _python_upsample([0, 0, 0, 0, 0], 0, False)
        native_out = self._native_upsample([0, 0, 0, 0, 0], state)
        self.assertEqual(py_out, list(native_out))
        self.assertEqual([0] * 15, list(native_out))


if __name__ == "__main__":
    unittest.main()
