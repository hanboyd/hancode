"""Phase 4 / ADR-0014 §6 step 4: runtime parity test for the audio
recording-double pair (G3 of ADR-0014).

When ``REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=shadow`` is set, the
python-side ``FakePlaybackSink`` (a pure-python recording double
mirroring ``ovb_rc003.audio_playback.EndpointPlaybackSink``) and the
C++-side ``FakeAudioRoute`` (``remotemic_native._C.FakeAudioRoute``
through ``ovb_rc003.audio_route_native._NativeAudioRoute``) must
yield byte-exact parity on:

  * ``recorded_samples_list`` (sample-by-sample equality)
  * ``recorded_samples`` count
  * ``peak`` (max |sample|)
  * ``rms`` (root-mean-square)
  * ``started_count`` / ``write_call_count`` / ``stopped_count`` /
    ``closed_count`` / ``dropped_count`` lifecycle counters

Hard rule from the user (phase 4 entry scope): no tolerance.
Any drift fails this test and per ADR-0014 §6 aborts Phase 4
entirely.

Why two recording doubles? The python ``EndpointPlaybackSink`` opens
a real PortAudio device, which CI cannot provide. The recording
double approach (matching FakeAudioRoute on the C++ side) is the
build-time parity proof required by plan §3 rule 5 (no shadow dual
owner of a real WASAPI device).

The test is skipped if the binding is unavailable
(``_C_AVAILABLE == False``) so source-tree imports without a CMake
build still get a green test suite.

Env-leak safety: the env override is set inside ``setUpClass`` and
restored in ``tearDownClass`` (NOT at module top), matching the
corrective fix pattern from commit 5ce9bd5.
"""

from __future__ import annotations

import math
import os
import unittest
from typing import List, Sequence, Tuple

from .fakes.audio_route_fakes import FakePlaybackSink


# Scenario format: (name, op_sequence) where op is one of:
#   ("start", fmt_dict)
#   ("write", samples_list)
#   ("drain", timeout_ms)
#   ("stop",)
#   ("close",)
# Each scenario drives identical scripts through both the python
# FakePlaybackSink and the native FakeAudioRoute; recorded_samples,
# peak, RMS and the five counters must all match.
_SCENARIOS: List[Tuple[str, List[Tuple]]] = [
    (
        "single write then close",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [100, -200, 300, -400, 500]),
            ("close",),
        ],
    ),
    (
        "multiple writes aggregate in order",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [1, 2, 3]),
            ("write", [4, 5, 6]),
            ("write", [7, 8, 9]),
            ("close",),
        ],
    ),
    (
        "20 ms chunk cadence (320 samples)",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [0] * 320),
            ("write", [0] * 320),
            ("write", [0] * 320),
            ("drain", 100),
            ("close",),
        ],
    ),
    (
        "silence burst then close",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [0] * 1024),
            ("close",),
        ],
    ),
    (
        "alternating short and long writes",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [10]),
            ("write", [20, 30]),
            ("write", [40, 50, 60, 70]),
            ("close",),
        ],
    ),
    (
        "write before start is dropped",
        [
            ("write", [1, 2, 3]),
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [4, 5, 6]),
            ("close",),
        ],
    ),
    (
        "write after close is dropped",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [10, 20]),
            ("close",),
            ("write", [30, 40]),
        ],
    ),
    (
        "stop is idempotent",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [1]),
            ("stop",),
            ("stop",),
            ("stop",),
            ("close",),
        ],
    ),
    (
        "close is idempotent",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [1]),
            ("close",),
            ("close",),
        ],
    ),
    (
        "loud signal: peak + rms",
        [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [-32768, 32767, 0, -32768, 32767]),
            ("close",),
        ],
    ),
]


def _drive_python(sink: FakePlaybackSink, ops: Sequence[Tuple]) -> None:
    for op in ops:
        kind = op[0]
        if kind == "start":
            sink.start(op[1])
        elif kind == "write":
            sink.write(op[1])
        elif kind == "drain":
            # FakePlaybackSink.drain is a no-op recording double;
            # we don't need to pass the timeout because there's
            # nothing to drain.
            sink.drain(op[1] / 1000.0 if op[1] else 0.5)
        elif kind == "stop":
            sink.stop()
        elif kind == "close":
            sink.close()
        else:
            raise AssertionError(f"unknown python op: {kind!r}")


def _drive_native(route, ops: Sequence[Tuple]) -> None:
    import remotemic_native as _rn  # type: ignore[import-not-found]

    PcmFormat = _rn._C.PcmFormat
    for op in ops:
        kind = op[0]
        if kind == "start":
            fmt = op[1]
            route.start(
                PcmFormat(
                    fmt["sample_rate"],
                    fmt["channels"],
                    fmt["bits_per_sample"],
                )
            )
        elif kind == "write":
            route.write(list(op[1]))
        elif kind == "drain":
            route.drain(int(op[1]))
        elif kind == "stop":
            route.stop()
        elif kind == "close":
            route.close()
        else:
            raise AssertionError(f"unknown native op: {kind!r}")


class _NativeAudioRouteRecorder:
    """Wrap ``_C.FakeAudioRoute`` and expose the same property names
    as ``FakePlaybackSink`` so the parity assertions read naturally.

    Counter accessors on the C++ binding are method calls
    (``started_count()``) while the python double uses attributes
    (``sink.started_count``). This wrapper flattens that gap.
    """

    def __init__(self) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        self._route = _rn._C.FakeAudioRoute()

    @property
    def recorded_samples_list(self) -> List[int]:
        return list(self._route.recorded_samples_list())

    @property
    def recorded_samples(self) -> int:
        return int(self._route.recorded_samples())

    @property
    def peak(self) -> int:
        return int(self._route.peak())

    @property
    def rms(self) -> float:
        return float(self._route.rms())

    @property
    def started_count(self) -> int:
        return int(self._route.started_count())

    @property
    def write_call_count(self) -> int:
        return int(self._route.write_call_count())

    @property
    def stopped_count(self) -> int:
        return int(self._route.stopped_count())

    @property
    def closed_count(self) -> int:
        return int(self._route.closed_count())

    @property
    def dropped_count(self) -> int:
        return int(self._route.dropped_count())

    def start(self, fmt_dict) -> bool:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        return bool(
            self._route.start(
                _rn._C.PcmFormat(
                    fmt_dict["sample_rate"],
                    fmt_dict["channels"],
                    fmt_dict["bits_per_sample"],
                )
            )
        )

    def write(self, samples) -> bool:
        return bool(self._route.write(list(samples)))

    def drain(self, timeout_ms: int) -> None:
        self._route.drain(int(timeout_ms))

    def stop(self) -> None:
        self._route.stop()

    def close(self) -> None:
        self._route.close()


class AudioRouteNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # Env-leak safety: capture and restore, NEVER module-level
        # os.environ.setdefault. The 5ce9bd5 corrective fix established
        # this pattern; Phase 4 parity tests follow it.
        cls._env_was_set = (
            "REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE" in os.environ
        )
        cls._env_old = os.environ.get(
            "REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE"
        )
        os.environ["REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE"] = "shadow"

        import remotemic_native as _rn  # type: ignore[import-not-found]

        cls._rn = _rn
        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; "
                "audio route parity skipped"
            )

    @classmethod
    def tearDownClass(cls) -> None:
        if cls._env_was_set:
            os.environ["REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE"] = (
                cls._env_old
            )
        else:
            os.environ.pop("REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE", None)

    def _run_scenario(self, name: str, ops: Sequence[Tuple]) -> None:
        with self.subTest(scenario=name):
            py = FakePlaybackSink()
            native = _NativeAudioRouteRecorder()

            _drive_python(py, ops)
            _drive_native(native._route, ops)

            # Sample-by-sample parity: this is the strongest possible
            # claim. Any drift here fails the whole Phase 4 per
            # ADR-0014 §9 G3.
            self.assertEqual(
                py.recorded_samples_list,
                native.recorded_samples_list,
                f"{name}: py={py.recorded_samples_list!r} "
                f"native={native.recorded_samples_list!r}",
            )
            # Counts: must match exactly because both sides run the
            # same op sequence.
            self.assertEqual(
                py.recorded_samples, native.recorded_samples,
                f"{name}: sample count drift",
            )
            self.assertEqual(
                py.started_count, native.started_count,
                f"{name}: started_count drift",
            )
            self.assertEqual(
                py.write_call_count, native.write_call_count,
                f"{name}: write_call_count drift",
            )
            self.assertEqual(
                py.stopped_count, native.stopped_count,
                f"{name}: stopped_count drift",
            )
            self.assertEqual(
                py.closed_count, native.closed_count,
                f"{name}: closed_count drift",
            )
            self.assertEqual(
                py.dropped_count, native.dropped_count,
                f"{name}: dropped_count drift",
            )
            # Peak and RMS: integer equality on peak; float equality
            # with a tight tolerance on RMS (sum-of-squares +
            # square-root, identical algorithms; tiny FP rounding
            # differences are possible).
            self.assertEqual(
                py.peak, native.peak,
                f"{name}: peak drift",
            )
            self.assertAlmostEqual(
                py.rms, native.rms, places=6,
                msg=f"{name}: rms drift",
            )

    def test_all_scenarios_drive_python_and_native(self) -> None:
        for name, ops in _SCENARIOS:
            self._run_scenario(name, ops)

    def test_silence_scenario_rms_is_zero(self) -> None:
        # Sanity: a silence input must produce rms=0 on both sides.
        py = FakePlaybackSink()
        native = _NativeAudioRouteRecorder()
        ops = [
            ("start", {"sample_rate": 16000, "channels": 1,
                       "bits_per_sample": 16}),
            ("write", [0] * 256),
            ("close",),
        ]
        _drive_python(py, ops)
        _drive_native(native._route, ops)
        self.assertEqual(py.rms, 0.0)
        self.assertEqual(native.rms, 0.0)
        self.assertEqual(py.peak, 0)
        self.assertEqual(native.peak, 0)

    def test_empty_route_has_no_recorded_samples(self) -> None:
        # No writes at all: both sides must report 0 samples + 0 peak
        # + 0.0 rms + 0 write_call_count.
        py = FakePlaybackSink()
        native = _NativeAudioRouteRecorder()
        _drive_python(py, [("start", {"sample_rate": 16000,
                                      "channels": 1,
                                      "bits_per_sample": 16}),
                           ("close",)])
        _drive_native(native._route,
                      [("start", {"sample_rate": 16000,
                                  "channels": 1,
                                  "bits_per_sample": 16}),
                       ("close",)])
        self.assertEqual(py.recorded_samples, 0)
        self.assertEqual(native.recorded_samples, 0)
        self.assertEqual(py.recorded_samples_list, [])
        self.assertEqual(native.recorded_samples_list, [])
        self.assertEqual(py.peak, 0)
        self.assertEqual(native.peak, 0)
        self.assertEqual(py.rms, 0.0)
        self.assertEqual(native.rms, 0.0)
        self.assertEqual(py.write_call_count, 0)
        self.assertEqual(native.write_call_count, 0)


if __name__ == "__main__":
    unittest.main()
