"""Phase 2 / Area 1 step 5: runtime shadow parity test (G5).

Per ADR-0012 §6 / §8 validation gate G5: when
``REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL=shadow`` is set, every fixture
must yield byte-exact field equality between the Python baseline
``ovb_rc003.atvv_protocol.ATVVCapabilities.parse`` and the C++ binding
``remotemic_native.atvv_capabilities_parse`` (via
``ovb_rc003.atvv_native_bridge.parse_capabilities``).

Hard rule from the user (phase 2 entry scope): no tolerance.
Every drift fails this test, and per plan §8 that aborts Phase 2
Area 1 entirely.

The test is skipped if the binding is unavailable (``_C_AVAILABLE ==
False``) so source-tree imports without a CMake build still get a
green test suite, with a single ``unittest.SkipTest`` line per
sub-test. This matches the existing graceful-degradation pattern in
``apps/windows/rc003/src/remotemic_native/__init__.py``.
"""

from __future__ import annotations

import json
import os
import unittest
from pathlib import Path

_FIXTURE_DIR = (
    Path(__file__).resolve().parent / "fixtures" / "atvv"
)


def _load(name: str) -> dict:
    return json.loads((_FIXTURE_DIR / name).read_text(encoding="utf-8"))


# All eight capability fixtures share the same shape so a single
# parameterized loop covers the whole matrix.
_FIXTURE_NAMES = (
    "synthetic-v1.json",
    "synthetic-v1-8k-fallback.json",
    "synthetic-v1-zero-frame-size.json",
    "synthetic-v1-zero-codecs-quirk.json",
    "synthetic-legacy-pre-1.0.json",
    "synthetic-legacy-rejects-short.json",
    "synthetic-wrong-opcode.json",
    "synthetic-short-payload.json",
)


class AtvvCapabilitiesNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # If the .pyd isn't bundled / built, skip every parity check.
        # Source-tree imports without a CMake build (and frozen
        # bundles where _C.pyd was removed) both end up here.
        import remotemic_native as _rn  # type: ignore[import-not-found]

        cls._rn = _rn
        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; shadow parity skipped"
            )

        # Activate shadow mode for the duration of this test class
        # only. The product default stays "python" (no global env
        # mutation). Honor an explicit user override (setdefault
        # semantics) so this test never silently overrides a CI /
        # production env choice; capture the original state so
        # ``tearDownClass`` can restore it and the parity test leaves
        # the unittest process's environment exactly as it found it
        # (Phase 1's ``test_default_is_python`` asserts the default
        # is "python" with a clean env, and module-level ``setdefault``
        # at import time previously broke that contract).
        cls._atvv_protocol_was_set = (
            "REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL" in os.environ
        )
        cls._atvv_protocol_old = os.environ.get(
            "REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"
        )
        os.environ.setdefault(
            "REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL", "shadow"
        )

        from ovb_rc003.atvv_native_bridge import parse_capabilities

        # Wrap in staticmethod so unittest's TestCase attribute
        # machinery doesn't bind ``self`` as the first arg when the
        # closure calls ``python_impl(*args)``. Without this,
        # ``self._parse_capabilities(payload)`` ends up calling
        # ``_shadow(self, payload)`` and the inner ``python_impl`` gets
        # 2 positional args instead of 1.
        cls._parse_capabilities = staticmethod(parse_capabilities)

    @classmethod
    def tearDownClass(cls) -> None:
        # Restore the env to the state we found at setUpClass. Guarded
        # in case SkipTest fired before the flags were set, even though
        # CPython skips tearDownClass in that path already.
        if getattr(cls, "_atvv_protocol_was_set", False):
            os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"] = (
                cls._atvv_protocol_old
            )
        else:
            os.environ.pop(
                "REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL", None
            )

    def test_fixture_python_baseline_matches_expected(self) -> None:
        # Sanity: the Python baseline (the source of truth for the
        # matrix) must still match the JSON's expected_* fields for
        # every valid fixture. If this fails, the C++ drift is the
        # least of our problems.
        from ovb_rc003 import atvv_protocol as proto

        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                payload = bytes.fromhex(fixture["capabilities_hex"])
                is_reject = "expected_version" not in fixture
                py_result = proto.ATVVCapabilities.parse(payload)

                if is_reject:
                    self.assertIsNone(py_result, f"{name}: expected nullopt")
                    continue

                self.assertIsNotNone(py_result, f"{name}: expected a result")
                self.assertEqual(py_result.version,       fixture["expected_version"])
                self.assertEqual(py_result.codecs,        fixture["expected_codecs"])
                self.assertEqual(py_result.interaction,   fixture["expected_interaction"])
                self.assertEqual(py_result.frame_size,    fixture["expected_frame_size"])
                self.assertEqual(py_result.selected_codec, fixture["expected_codec"])
                self.assertEqual(py_result.sample_rate,   fixture["expected_sample_rate"])

    def test_fixture_native_matches_python_byte_exact(self) -> None:
        # The actual parity check: python baseline and C++ binding
        # must yield identical field values for every fixture, with
        # no tolerance anywhere (per ADR-0012 §5 / plan §1 rule 3).
        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                payload = bytes.fromhex(fixture["capabilities_hex"])

                # Valid vs reject fixtures are distinguished by the
                # presence of ``expected_version``. ``expected_version``
                # is set on every valid fixture and absent on every
                # reject fixture (which carry ``"expected": null``
                # instead).
                is_reject = "expected_version" not in fixture

                # ``_parse_capabilities`` is in shadow mode here, so
                # the helper runs both implementations, asserts
                # equality, and returns the python result. Any drift
                # raises RuntimeError inside ``_shadow``.
                bridge_result = self._parse_capabilities(payload)

                if is_reject:
                    self.assertIsNone(
                        bridge_result,
                        f"{name}: shadow returned a result, expected None",
                    )
                    continue

                self.assertIsNotNone(
                    bridge_result, f"{name}: shadow returned None"
                )
                self.assertEqual(
                    bridge_result.version,        fixture["expected_version"]
                )
                self.assertEqual(
                    bridge_result.codecs,         fixture["expected_codecs"]
                )
                self.assertEqual(
                    bridge_result.interaction,    fixture["expected_interaction"]
                )
                self.assertEqual(
                    bridge_result.frame_size,     fixture["expected_frame_size"]
                )
                self.assertEqual(
                    bridge_result.selected_codec, fixture["expected_codec"]
                )
                self.assertEqual(
                    bridge_result.sample_rate,    fixture["expected_sample_rate"]
                )


if __name__ == "__main__":
    unittest.main()