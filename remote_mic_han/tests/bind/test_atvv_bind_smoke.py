"""Phase 2 / Area 1 step 4: ATVV capability binding smoke (G3).

Loads the bundled remotemic_native._C extension, calls
``atvv_capabilities_parse`` against one valid + one reject JSON
fixture, and asserts the return shape matches the C++ unit test
(``remotemic_atvv_tests``).

This is the build-time parity proof for the binding seam; the
runtime shadow parity test (``tests/test_atvv_native_parity.py``)
is step 5's job. Per ADR-0012 G3: on fail, do not flip the ADR
status from ``proposed`` to ``accepted``.
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


class AtvvCapabilitiesBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # The binding must be present (the build only runs this test
        # when REMOTEMIC_BUILD_PYTHON=ON, and CMake stashes the .pyd
        # next to remotemic_native/__init__.py inside the build tree).
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.parse = _C.atvv_capabilities_parse
        cls.Capabilities = _C.AtvvCapabilities

    def test_valid_v1_fixture_returns_populated_struct(self) -> None:
        fixture = _load("synthetic-v1.json")
        payload = bytes.fromhex(fixture["capabilities_hex"])
        result = self.parse(payload)
        self.assertIsNotNone(result)
        self.assertIsInstance(result, self.Capabilities)
        self.assertEqual(result.version, fixture["expected_version"])
        self.assertEqual(result.codecs, fixture["expected_codecs"])
        self.assertEqual(result.interaction, fixture["expected_interaction"])
        self.assertEqual(result.frame_size, fixture["expected_frame_size"])
        self.assertEqual(result.selected_codec, fixture["expected_codec"])
        self.assertEqual(result.sample_rate, fixture["expected_sample_rate"])

    def test_reject_fixture_returns_none(self) -> None:
        fixture = _load("synthetic-wrong-opcode.json")
        payload = bytes.fromhex(fixture["capabilities_hex"])
        result = self.parse(payload)
        self.assertIsNone(result)

    def test_atvv_capabilities_class_has_all_six_fields(self) -> None:
        # Guard against accidental rename: the parity contract (ADR-0012
        # §3 / §5) names the 6 fields; the Python baseline dataclass and
        # the C++ struct both spell them exactly this way.
        for name in (
            "version",
            "codecs",
            "interaction",
            "frame_size",
            "selected_codec",
            "sample_rate",
        ):
            self.assertTrue(
                hasattr(self.Capabilities, name),
                f"AtvvCapabilities missing field {name!r}",
            )

    def test_empty_payload_returns_none(self) -> None:
        # Phase 2 / Area 4 step 4 malformed seam: a completely empty
        # notification payload must be rejected as malformed (None),
        # not crash and not raise. This matches the C++ contract
        # (data.size() < 7 -> std::nullopt). This is reported as the
        # actual behavior at the public Python/native seam, not
        # inferred from pybind11 docs.
        result = self.parse(b"")
        self.assertIsNone(result,
                          "empty payload must parse to None (malformed)")

    def test_short_sub_seven_payloads_return_none(self) -> None:
        # Phase 2 / Area 4 step 4 malformed seam: every payload
        # shorter than the 7-byte minimum header is rejected. The
        # observable behavior is None (no exception). Content bytes
        # do not matter because the length gate fires first.
        for length in range(0, 7):
            with self.subTest(length=length):
                # Pick a fixed byte (0x00) so every payload is
                # deterministic; any non-empty buffer of length < 7
                # must round-trip to None.
                payload = b"\x00" * length
                self.assertIsNone(
                    self.parse(payload),
                    f"length={length} payload must parse to None",
                )


if __name__ == "__main__":
    unittest.main()