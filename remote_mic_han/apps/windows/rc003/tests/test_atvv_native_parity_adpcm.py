"""Phase 2 / Area 3 step 5: runtime shadow parity test for the IMA/DVI
ADPCM decoder (G5 of ADR-0012).

Per ADR-0012 section 6 / section 8 validation gate G5: when
``REMOTEMIC_NATIVE_CHOICE_ADPCM_IMA_DECODE=shadow`` is set, every
adpcm JSON fixture must yield sample-exact equality between the
Python baseline (``ovb_rc003.atvv_protocol.IMAADPCMDecoder``) and the
C++ binding (``remotemic_native.ImaDecoder``) via the
``ovb_rc003.atvv_native_bridge.decode_adpcm_frame`` wrapper.

Hard rule from the user (phase 2 entry scope): no tolerance.
Any drift fails this test and per plan section 8 that aborts Phase 2
Area 3 entirely.

The test is skipped if the binding is unavailable
(``_C_AVAILABLE == False``) so source-tree imports without a CMake
build still get a green test suite. This matches the existing
graceful-degradation pattern in
``apps/windows/rc003/src/remotemic_native/__init__.py`` and the Area 1
/ Area 2 parity tests.
"""

from __future__ import annotations

import json
import os
import unittest
from pathlib import Path

# Required for the helper's shadow branch to run; the default
# ``implementation_choice`` is ``"python"`` so without this env var
# the helper just returns the python baseline and the parity test
# becomes a no-op. The skip branch catches that case.
os.environ.setdefault(
    "REMOTEMIC_NATIVE_CHOICE_ADPCM_IMA_DECODE", "shadow"
)

_FIXTURE_DIR = (
    Path(__file__).resolve().parent / "fixtures" / "atvv"
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


class AdpcmImaNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        cls._rn = _rn
        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; shadow parity skipped"
            )

        from ovb_rc003.atvv_native_bridge import decode_adpcm_frame

        # staticmethod wrapper: unittest's TestCase attribute machinery
        # would otherwise bind ``self`` as the first arg.
        cls._decode_adpcm_frame = staticmethod(decode_adpcm_frame)

    def test_python_baseline_matches_expected(self) -> None:
        """Sanity: the Python baseline must still match the JSON's
        expected_pcm field for every fixture. If this fails, the C++
        drift is the least of our problems."""
        from ovb_rc003 import atvv_protocol as proto

        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                data = bytes.fromhex(fixture["input_hex"])
                reset = fixture.get("reset", {"predictor": 0, "step_index": 0})
                decoder = proto.IMAADPCMDecoder()
                decoder.reset(reset["predictor"], reset["step_index"])
                samples = decoder.decode(data)
                self.assertEqual(
                    samples, fixture["expected_pcm"],
                    f"{name}: python baseline != expected",
                )

    def test_native_matches_python_sample_exact(self) -> None:
        """The actual parity check: python baseline and C++ binding
        must yield identical sample lists for every fixture, with
        no tolerance anywhere (per ADR-0012 section 5 / plan section
        1 rule 3).
        """
        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                data = bytes.fromhex(fixture["input_hex"])
                reset = fixture.get("reset", {"predictor": 0, "step_index": 0})

                # ``_decode_adpcm_frame`` is in shadow mode here, so
                # the helper runs both implementations, asserts
                # equality, and returns the python result. Any drift
                # raises RuntimeError inside ``_shadow``.
                bridge_result = self._decode_adpcm_frame(
                    data, reset["predictor"], reset["step_index"]
                )

                self.assertEqual(
                    bridge_result, fixture["expected_pcm"],
                    f"{name}: shadow != expected",
                )


if __name__ == "__main__":
    unittest.main()