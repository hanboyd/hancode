"""Phase 2 / Area 3 step 4: IMA/DVI ADPCM decoder binding smoke (G3).

Loads the bundled ``remotemic_native._C`` extension, creates an
``ImaDecoder`` instance per fixture, calls ``reset`` + ``decode``,
and asserts the returned samples match the C++ unit test
(``remotemic_adpcm_ima_tests``) sample-for-sample.

Per ADR-0012 G3: on fail, do not flip the ADR status from
``proposed`` to ``accepted``. The runtime shadow parity test
(``tests/test_atvv_native_parity_adpcm.py``) is step 5's job.
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
    "adpcm-empty.json",
    "adpcm-single-byte-zero-state.json",
    "adpcm-four-byte-zero-state.json",
    "adpcm-all-positive-nibbles.json",
    "adpcm-all-negative-nibbles.json",
    "adpcm-round-trip-ramp.json",
    "adpcm-clamp-predictor-high.json",
    "adpcm-clamp-predictor-low.json",
    "adpcm-clamp-step-index.json",
    "adpcm-clamp-step-index-high.json",
    "adpcm-reset-nonzero-state.json",
)


class AdpcmImaBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # The binding must be present (the build only runs this test
        # when REMOTEMIC_BUILD_PYTHON=ON, and CMake stashes the .pyd
        # next to remotemic_native/__init__.py inside the build tree).
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.ImaDecoder = _C.ImaDecoder

    def _decode(self, fixture: dict) -> list[int]:
        decoder = self.ImaDecoder()
        if "reset" in fixture:
            r = fixture["reset"]
            decoder.reset(r["predictor"], r["step_index"])
        data = bytes.fromhex(fixture["input_hex"])
        result = decoder.decode(data)
        # pybind11 returns a py::list / C++ vector as a list[int] when
        # the C++ type is std::vector<int16_t>.
        return list(result)

    def test_fixture(self) -> None:
        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                actual = self._decode(fixture)
                expected = fixture["expected_pcm"]
                self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()