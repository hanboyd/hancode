"""Phase 2 / Area 4 step 4: DcHighPassFilter binding smoke (G3).

Loads the bundled ``remotemic_native._C`` extension, creates a
``DcHighPassFilter`` instance per fixture, calls ``process`` on the
fixture's samples, and asserts the returned vector matches the C++
unit test (``remotemic_adpcm_dc_tests``) sample-for-sample.

Per ADR-0012 G3: on fail, do not flip the ADR status from ``proposed``
to ``accepted``. The runtime shadow parity test
(``tests/test_atvv_native_parity_area4.py``) is step 5's job.

In addition to the gold-fixture loop, this smoke verifies that
``reset()`` parity holds at the public Python/native seam: after
``reset()`` the filter processes the same samples sample-for-sample
identical to a freshly constructed instance.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

_FIXTURE_DIR = (
    Path(__file__).resolve().parent.parent.parent
    / "apps" / "windows" / "rc003" / "tests" / "fixtures" / "atvv"
)


def _load(name: str) -> dict:
    return json.loads((_FIXTURE_DIR / name).read_text(encoding="utf-8"))


_FIXTURE_NAMES = (
    "dc-empty.json",
    "dc-single-sample.json",
    "dc-two-samples.json",
    "dc-dc-blocked.json",
    "dc-ac-passes.json",
)


class AdpcmDcBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.DcHighPassFilter = _C.DcHighPassFilter

    def test_fixture(self) -> None:
        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                flt = self.DcHighPassFilter(16000.0, 20.0)
                flt.reset()
                samples = list(fixture["samples"])
                actual = list(flt.process(samples))
                self.assertEqual(actual, fixture["expected_filtered"])

    def test_reset_parity_equals_fresh_instance(self) -> None:
        # Drive state into the filter, reset(), drive the same
        # samples again, and compare to a freshly constructed
        # filter fed the same samples. The outputs must be
        # sample-equal.
        fixture = _load("dc-ac-passes.json")
        samples = list(fixture["samples"])
        split = len(samples) // 2
        warmup = samples[:split]
        payload = samples[split:]

        # Path A: warmup, reset, payload
        flt_a = self.DcHighPassFilter(16000.0, 20.0)
        flt_a.process(warmup)
        flt_a.reset()
        out_a = list(flt_a.process(payload))

        # Path B: fresh on payload
        flt_b = self.DcHighPassFilter(16000.0, 20.0)
        out_b = list(flt_b.process(payload))

        self.assertEqual(out_a, out_b)

    def test_reset_parity_no_warmup_matches_constructor(self) -> None:
        # A reset() on a never-used filter must leave it in the
        # same state as a freshly constructed one.
        flt_used = self.DcHighPassFilter(16000.0, 20.0)
        flt_used.reset()
        payload = [42, -42, 84, -84, 126, -126]
        out_used = list(flt_used.process(payload))

        flt_fresh = self.DcHighPassFilter(16000.0, 20.0)
        out_fresh = list(flt_fresh.process(payload))

        self.assertEqual(out_used, out_fresh)


if __name__ == "__main__":
    unittest.main()