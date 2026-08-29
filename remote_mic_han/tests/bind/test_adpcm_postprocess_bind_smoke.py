"""Phase 2 / Area 4 step 4: postprocess binding smoke (G3).

Loads the bundled ``remotemic_native._C`` extension, calls the
``postprocess`` function per fixture, and asserts the returned vector
matches the C++ unit test (``remotemic_adpcm_postprocess_tests``)
sample-for-sample.

Per ADR-0012 G3: on fail, do not flip the ADR status from ``proposed``
to ``accepted``.
"""

from __future__ import annotations

import json
import math
import unittest
from pathlib import Path

_FIXTURE_DIR = (
    Path(__file__).resolve().parent.parent.parent
    / "apps" / "windows" / "rc003" / "tests" / "fixtures" / "atvv"
)


def _load(name: str) -> dict:
    return json.loads((_FIXTURE_DIR / name).read_text(encoding="utf-8"))


_FIXTURE_NAMES = (
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


def _gain_db(fixture: dict) -> float:
    """JSON has no native NaN encoding; the generator encodes NaN
    and Infinity as strings."""
    raw = fixture["gain_db"]
    if isinstance(raw, str):
        if raw == "NaN":
            return math.nan
        if raw == "Infinity" or raw == "+Infinity":
            return math.inf
        if raw == "-Infinity":
            return -math.inf
    return float(raw)


class AdpcmPostprocessBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.postprocess = _C.postprocess

    def test_fixture(self) -> None:
        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                samples = list(fixture["samples"])
                gain = _gain_db(fixture)
                actual = list(self.postprocess(samples, gain))
                self.assertEqual(actual, fixture["expected_output"])


if __name__ == "__main__":
    unittest.main()