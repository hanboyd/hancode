"""Phase 2 / Area 4 step 5: runtime shadow parity for ADPCM DC high-pass,
smoothing/gain (``postprocess``), and ``FrameAccumulator``
(ADR-0012 section 6 / section 8 validation gate G5).

Scope (locked to step 5):

* DcHighPassFilter
* postprocess
* FrameAccumulator
* reset state across the seam
* partial frame carried across calls
* the existing area-4 JSON gold fixtures
  (``apps/windows/rc003/tests/fixtures/atvv/dc-*.json``,
  ``postprocess-*.json``, ``frame-*.json``)

Hard rule from the project: discrete (int16 / uint8 / bytes) results
must be value-exact between the Python baseline and the C++ binding;
no tolerance, no per-call softening. Per-call heuristics (e.g., the
3-tap smoothing / float conversions inside the helpers) are part of
the protocol contract; we compare after the int16 / uint8 clamp
inside both impls.

The test is skipped when the binding is unavailable
(``_C_AVAILABLE == False``) so a source-tree import without a CMake
build still gets a clean run. On drift we STOP, report the fixture,
both outputs, and the first differing index; we do NOT silently
loosen the comparison to make a real diff disappear.
"""

from __future__ import annotations

import json
import math
import os
import unittest
from pathlib import Path

# The area-4 module switches are side-effect-free and side-step the
# default policy ``"python"`` only when the user / CI sets the shadow
# env var. Without that, the bridge's ``_shadow`` falls back to a
# passthrough and the parity check below would be meaningless, so the
# whole test class is skipped by the gate at the top of each test
# method.
os.environ.setdefault(
    "REMOTEMIC_NATIVE_CHOICE_ADPCM_DC_HIGHPASS", "shadow"
)
os.environ.setdefault(
    "REMOTEMIC_NATIVE_CHOICE_ADPCM_POSTPROCESS", "shadow"
)
os.environ.setdefault(
    "REMOTEMIC_NATIVE_CHOICE_ADPCM_FRAME_ACCUMULATOR", "shadow"
)


_FIXTURE_DIR = (
    Path(__file__).resolve().parent / "fixtures" / "atvv"
)


def _load(name: str) -> dict:
    return json.loads((_FIXTURE_DIR / name).read_text(encoding="utf-8"))


_DC_FIXTURE_NAMES = (
    "dc-empty.json",
    "dc-single-sample.json",
    "dc-two-samples.json",
    "dc-dc-blocked.json",
    "dc-ac-passes.json",
)


_POSTPROCESS_FIXTURE_NAMES = (
    "postprocess-empty.json",
    "postprocess-single-default-gain.json",
    "postprocess-zero-gain.json",
    "postprocess-max-gain.json",
    "postprocess-min-gain.json",
    "postprocess-gain-clamps-above-24.json",
    "postprocess-gain-nan.json",
    "postprocess-gain-inf.json",
    "postprocess-clamp-to-int16.json",
    "postprocess-two-samples-no-smoothing.json",
)


_FRAME_FIXTURE_NAMES = (
    "frame-empty.json",
    "frame-under-size.json",
    "frame-exact-size.json",
    "frame-multi-from-single.json",
    "frame-multi-append-across-calls.json",
    "frame-zero-size.json",
    "frame-multi-frame-size-10.json",
)


def _gain_db(fixture: dict) -> float:
    """Translate the JSON ``gain_db`` string sentinel into a real
    float, matching ``_gen_area4_fixtures.py``'s output convention."""
    raw = fixture["gain_db"]
    if isinstance(raw, str):
        if raw == "NaN":
            return math.nan
        if raw in ("Infinity", "+Infinity"):
            return math.inf
        if raw == "-Infinity":
            return -math.inf
    return float(raw)


def _bytes(obj):
    """pybind11 maps ``std::vector<uint8_t>`` to ``list[int]`` on the
    Python side. Convert that back to bytes for value-exact compare."""
    return bytes(obj)


def _first_diff(py_result, na_result) -> str:
    """Return a diagnostic string identifying the first difference
    between two list-likes. Used to STOP on drift instead of letting
    an opaque ``!= False`` paper over the divergence."""
    try:
        plen, nlen = len(py_result), len(na_result)
    except TypeError:
        return (
            f"python={py_result!r} native={na_result!r} "
            f"(uncomparable; see script body)"
        )
    common = min(plen, nlen)
    for i in range(common):
        if py_result[i] != na_result[i]:
            return (
                f"first diff at index {i}: "
                f"python={py_result[i]!r} native={na_result[i]!r}; "
                f"python len={plen} native len={nlen}"
            )
    return (
        f"length mismatch: python len={plen} native len={nlen}; "
        f"head python={list(py_result[:8])!r} "
        f"native={list(na_result[:8])!r}"
    )


class _Gate:
    """Tiny guard so we never silently fall back to ``python`` mode."""

    @staticmethod
    def require_native(testcase: unittest.TestCase) -> bool:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; shadow parity skipped"
            )
        return True


class AdpcmDcNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        _Gate.require_native(cls)
        from ovb_rc003 import atvv_protocol as proto
        from ovb_rc003.atvv_native_bridge import apply_dc_highpass

        cls._proto = proto
        cls._apply_dc_highpass = staticmethod(apply_dc_highpass)

    def _drive(self, samples: list[int]) -> tuple:
        """Run the Python baseline directly and the bridge shadow
        helper for the same input. Returns (python_result,
        bridge_result). bridge_result is the shadow-comparison
        verdict (it raises on drift, which we catch and surface)."""
        py_result = [int(s) for s in self._proto.DCHighPassFilter().process(
            list(samples)
        )]
        try:
            bridge_result = self._apply_dc_highpass(list(samples))
        except RuntimeError as exc:
            self.fail(
                f"shadow_runtime_error: native drift: {exc}; "
                f"python_only={py_result[:32]!r}"
            )
        return py_result, bridge_result

    def test_dc_fixture_parity_value_exact(self) -> None:
        for name in _DC_FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                samples = list(fixture["samples"])
                py_result, bridge_result = self._drive(samples)
                if py_result != bridge_result:
                    self.fail(
                        f"fixture={name}: {_first_diff(py_result, bridge_result)}; "
                        f"expected={fixture['expected_filtered']!r}"
                    )
                # Tie back to the gold fixture so a future drift
                # cannot survive by hiding inside the bridge.
                self.assertEqual(
                    bridge_result, fixture["expected_filtered"],
                    f"fixture={name}: shadow != golden expected_filtered"
                )

    def test_dc_reset_state_parity_after_warmup(self) -> None:
        """Drive non-trivial state into the filter, reset(), run the
        same payload again, and verify the result sample-equals a
        freshly constructed filter fed the post-reset payload.
        Drives both sides in lock-step: same payloads, same order,
        same reset timing.
        """
        fixture = _load("dc-ac-passes.json")
        samples = list(fixture["samples"])
        split = len(samples) // 2
        warmup = samples[:split]
        payload = samples[split:]

        # Path A (warmup + reset + payload)
        py_a = self._proto.DCHighPassFilter()
        [int(s) for s in py_a.process(warmup)]
        py_a.reset()
        py_a_out = [int(s) for s in py_a.process(payload)]

        # Path B (fresh instance on payload)
        py_b = self._proto.DCHighPassFilter()
        py_b_out = [int(s) for s in py_b.process(payload)]

        # The Python baseline is by definition consistent with
        # itself. The actual parity check:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        na_a = _rn.DcHighPassFilter(16000.0, 20.0)
        # warmup the native filter once so reset() is exercised
        [int(s) for s in na_a.process(warmup)]
        na_a.reset()
        na_a_out = [int(s) for s in na_a.process(payload)]

        if py_a_out != na_a_out:
            self.fail(
                "reset_state_parity (post-warmup, post-reset, "
                f"identical payload): {_first_diff(py_a_out, na_a_out)}"
            )
        # Tied to the fresh baseline so a drift that "agrees with
        # itself" cannot hide.
        self.assertEqual(py_a_out, py_b_out)
        self.assertEqual(na_a_out, py_b_out)

    def test_dc_reset_state_parity_no_warmup(self) -> None:
        """A reset() on a never-used filter must leave it
        byte-equivalent to a freshly constructed one."""
        import remotemic_native as _rn  # type: ignore[import-not-found]

        payload = [42, -42, 84, -84, 126, -126]

        py_used = self._proto.DCHighPassFilter()
        py_used.reset()
        py_used_out = [int(s) for s in py_used.process(payload)]

        py_fresh = self._proto.DCHighPassFilter()
        py_fresh_out = [int(s) for s in py_fresh.process(payload)]

        na_used = _rn.DcHighPassFilter(16000.0, 20.0)
        na_used.reset()
        na_used_out = [int(s) for s in na_used.process(payload)]

        if py_used_out != na_used_out:
            self.fail(
                "reset_state_parity (no-warmup): "
                f"{_first_diff(py_used_out, na_used_out)}"
            )
        self.assertEqual(py_used_out, py_fresh_out)


class AdpcmPostprocessNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        _Gate.require_native(cls)
        from ovb_rc003 import atvv_protocol as proto
        from ovb_rc003.atvv_native_bridge import postprocess_pcm

        cls._proto = proto
        cls._postprocess_pcm = staticmethod(postprocess_pcm)

    def _drive(self, samples: list[int], gain_db: float) -> tuple:
        py_result = [int(s) for s in
                     self._proto.postprocess(list(samples), gain_db)]
        try:
            bridge_result = self._postprocess_pcm(list(samples), gain_db)
        except RuntimeError as exc:
            self.fail(
                f"shadow_runtime_error: native drift: {exc}; "
                f"python_only={py_result!r}"
            )
        return py_result, bridge_result

    def test_postprocess_fixture_parity_value_exact(self) -> None:
        for name in _POSTPROCESS_FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                samples = list(fixture["samples"])
                gain = _gain_db(fixture)
                py_result, bridge_result = self._drive(samples, gain)
                if py_result != bridge_result:
                    self.fail(
                        f"fixture={name}: "
                        f"{_first_diff(py_result, bridge_result)}; "
                        f"expected={fixture['expected_output']!r}"
                    )
                self.assertEqual(
                    bridge_result, fixture["expected_output"],
                    f"fixture={name}: shadow != golden expected_output"
                )

    def test_postprocess_each_window_reads_original_samples(self) -> None:
        samples = [1000, -2000, 3000, -4000, 5000, -6000]
        py_result, bridge_result = self._drive(samples, 0.0)
        self.assertEqual(py_result, [1000, 0, 0, 0, 0, -6000])
        self.assertEqual(bridge_result, py_result)


class AdpcmFrameAccumulatorNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        _Gate.require_native(cls)
        from ovb_rc003 import atvv_protocol as proto
        from ovb_rc003.atvv_native_bridge import accumulate_frames

        cls._proto = proto
        cls._accumulate_frames = staticmethod(accumulate_frames)

    def _drive_single(self, data: bytes, frame_size: int) -> tuple:
        """Single-chunk parity: one append, compare bytes."""
        py_acc = self._proto.FrameAccumulator()
        py_out = [bytes(f) for f in py_acc.append(data, frame_size)]
        try:
            bridge_out = self._accumulate_frames([data], frame_size)
        except RuntimeError as exc:
            self.fail(
                f"shadow_runtime_error: native drift: {exc}; "
                f"python_only={py_out!r}"
            )
        return py_out, list(bridge_out)

    def _drive_multi(self, chunks: list, frame_size: int) -> tuple:
        """Multi-chunk parity: same sequence of appends on both
        sides; verify all emitted frames cumulative byte-exact."""
        py_acc = self._proto.FrameAccumulator()
        py_out = []
        for chunk in chunks:
            for f in py_acc.append(chunk, frame_size):
                py_out.append(bytes(f))
        try:
            bridge_out = list(self._accumulate_frames(chunks, frame_size))
        except RuntimeError as exc:
            self.fail(
                f"shadow_runtime_error: native drift: {exc}; "
                f"python_only={py_out!r}"
            )
        return py_out, bridge_out

    def _assert_hex_pair_equal(self, py_hex: list, na_hex: list, ctx: str) -> None:
        if py_hex == na_hex:
            return
        for i, (a, b) in enumerate(zip(py_hex, na_hex)):
            if a != b:
                self.fail(
                    f"{ctx}: first diff at frame {i}: "
                    f"python={a!r} native={b!r}; "
                    f"python_total={len(py_hex)} native_total={len(na_hex)}"
                )
        self.fail(
            f"{ctx}: length mismatch (python={len(py_hex)} "
            f"native={len(na_hex)})"
        )

    def test_frame_fixture_parity_value_exact(self) -> None:
        for name in _FRAME_FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                data = bytes.fromhex(fixture["data_hex"])
                frame_size = fixture["frame_size"]
                append_count = fixture["append_count"]
                expected_hex = fixture["expected_frames_hex"]

                # Replicate the binding smoke's split convention
                # for the multi-append-across-calls fixture (the
                # fixture's data_hex is just the second call's
                # bytes; the first call uses bytes(0..80) mod 256).
                if name == "frame-multi-append-across-calls.json" and \
                        append_count == 2:
                    first = bytes(i % 256 for i in range(80))
                    py_out, bridge_out = self._drive_multi(
                        [first, data], frame_size
                    )
                else:
                    # For single-append fixtures the bridge loops
                    # over append_count chunks; if append_count > 1
                    # with the SAME data repeated, the bridge path
                    # already exercises that. Single-call is the
                    # dominant case (append_count = 1 for every
                    # fixture in this set).
                    if append_count == 1:
                        py_out, bridge_out = self._drive_single(
                            data, frame_size
                        )
                    else:
                        py_out, bridge_out = self._drive_multi(
                            [data] * append_count, frame_size
                        )

                self._assert_hex_pair_equal(
                    py_out, bridge_out,
                    f"fixture={name}"
                )
                # Tie to gold so the parity assertion cannot become
                # a "two impls agree, both wrong" trap. The gold
                # format is list[str] (hex), while bridge_out is
                # list[bytes]; normalize to hex for the final
                # apples-to-apples comparison.
                self.assertEqual(
                    [b.hex() for b in bridge_out], expected_hex,
                    f"fixture={name}: shadow != golden expected_frames_hex"
                )

    def test_frame_reset_state_parity(self) -> None:
        """reset() on a stateful accumulator + a new stream must
        sample-equal the fresh-instance case. Done via the same
        bridge helper (per-call) so each call's parity check is
        exercised."""
        import remotemic_native as _rn  # type: ignore[import-not-found]

        # Path A: leave a partial frame, reset(), then a fresh
        # stream.
        py_a = self._proto.FrameAccumulator()
        [bytes(f) for f in py_a.append(bytes(range(50)), 60)]
        py_a.reset()
        py_a_out = [
            bytes(f) for f in py_a.append(bytes(range(100, 190)), 60)
        ]

        # Path B (fresh instance, same stream).
        py_fresh = self._proto.FrameAccumulator()
        py_fresh_out = [
            bytes(f) for f in py_fresh.append(bytes(range(100, 190)), 60)
        ]

        # Native side, mirrored:
        na_a = _rn.FrameAccumulator()
        [bytes(f) for f in na_a.append(bytes(range(50)), 60)]
        na_a.reset()
        na_a_out = [
            bytes(f) for f in na_a.append(bytes(range(100, 190)), 60)
        ]

        self._assert_hex_pair_equal(
            py_a_out, na_a_out,
            "reset_state_parity (warm-up then reset then new stream)"
        )
        # Both sides must equal the fresh-instance baseline:
        self.assertEqual(py_a_out, py_fresh_out)
        self.assertEqual(na_a_out, py_fresh_out)

    def test_frame_partial_across_calls(self) -> None:
        """A frame straddling two append() calls must come out byte
        identical on both sides; same chunk sequence, same
        frame_size."""
        # Build a partial: first call 80 bytes @ frame_size=120,
        # second call 120 bytes to close + emit another frame.
        first = bytes(range(80))
        second = bytes(range(80, 200))

        py_acc = self._proto.FrameAccumulator()
        py_first = [bytes(f) for f in py_acc.append(first, 120)]
        py_second = [bytes(f) for f in py_acc.append(second, 120)]
        py_total = py_first + py_second

        try:
            bridge_total = list(self._accumulate_frames(
                [first, second], 120
            ))
        except RuntimeError as exc:
            self.fail(
                f"shadow_runtime_error on partial_across_calls: {exc}; "
                f"python_only={[b.hex() for b in py_total]!r}"
            )

        self._assert_hex_pair_equal(
            py_total, bridge_total,
            "partial_across_calls"
        )

    def test_frame_no_op_contract_parity(self) -> None:
        """The binding-level contract for frame_size <= 0 is a no-op
        at the public API (returns []; pending unchanged). The
        bridge implements the same contract on the Python side via
        the proto guard, and on the C++ side via binding-layer
        short-circuit. Verify the byte-exact output matches.
        """
        import remotemic_native as _rn  # type: ignore[import-not-found]

        for bad_frame_size in (0, -1):
            with self.subTest(frame_size=bad_frame_size):
                payload = bytes(range(20))
                # Python baseline: ``if frame_size <= 0: return []``
                py_acc = self._proto.FrameAccumulator()
                py_out = [bytes(f) for f in
                          py_acc.append(payload, bad_frame_size)]

                # Native side: binding refuses to enter C++.
                na_acc = _rn.FrameAccumulator()
                # Python wrapper via the bridge only accepts int
                # values that pass ``1..65535``. The bridge's
                # ``accumulate_frames`` skips the binding for
                # ``<=0`` only because the call is intercepted by
                # the proto guard on the Python side. So we use the
                # raw binding directly here to exercise the explicit
                # reject.
                try:
                    na_out = [
                        bytes(f)
                        for f in na_acc.append(payload, bad_frame_size)
                    ]
                except Exception as exc:  # noqa: BLE001 - cross-implementation
                    self.fail(
                        f"native raised {type(exc).__name__} on "
                        f"frame_size={bad_frame_size}: {exc}; "
                        f"python got {py_out!r}"
                    )

                if py_out != na_out:
                    self.fail(
                        f"frame_size={bad_frame_size}: "
                        f"{_first_diff(py_out, na_out)}"
                    )


if __name__ == "__main__":
    unittest.main()
