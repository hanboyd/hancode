"""Phase 2 / Area 2 step 5: runtime shadow parity test for the ATVV
control channel (G5 of ADR-0012).

Per ADR-0012 §6 / §8 validation gate G5: when
``REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_PARSE=shadow`` and
``REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_ENCODE=shadow`` are set, every
control JSON fixture must yield byte-exact equality between the Python
baseline (``ovb_rc003.atvv_protocol.parse_control_payload`` /
``mic_open_command`` / ``mic_close_command``) and the C++ binding
(``remotemic_native.atvv_control_parse`` /
``atvv_mic_open_command`` / ``atvv_mic_close_command``) via the
``ovb_rc003.atvv_native_bridge`` wrappers.

Hard rule from the user (phase 2 entry scope): no tolerance.
Any drift fails this test and per plan §8 that aborts Phase 2 Area 2
entirely.

The test is skipped if the binding is unavailable
(``_C_AVAILABLE == False``) so source-tree imports without a CMake build
still get a green test suite, with a single ``unittest.SkipTest`` per
sub-test. This matches the existing graceful-degradation pattern in
``apps/windows/rc003/src/remotemic_native/__init__.py`` and the Area 1
capability parity test.
"""

from __future__ import annotations

import json
import os
import unittest
from pathlib import Path

# Required for the helper's shadow branch to run; the default
# ``implementation_choice`` is ``"python"`` so without these env vars
# the helper just returns the python baseline and the parity test
# becomes a no-op. The skip branch catches that case.
os.environ.setdefault(
    "REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_PARSE", "shadow"
)
os.environ.setdefault(
    "REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_ENCODE", "shadow"
)

_FIXTURE_DIR = (
    Path(__file__).resolve().parent / "fixtures" / "atvv"
)


def _load(name: str) -> dict:
    return json.loads((_FIXTURE_DIR / name).read_text(encoding="utf-8"))


_DECODE_FIXTURE_NAMES = (
    "control-decode-caps.json",
    "control-decode-mic-button.json",
    "control-decode-audio-start-with-sid.json",
    "control-decode-audio-start-no-sid.json",
    "control-decode-audio-stop.json",
    "control-decode-audio-sync.json",
    "control-decode-unknown.json",
    "control-decode-empty.json",
)

_ENCODE_FIXTURE_NAMES = (
    "control-mic-open-v1.json",
    "control-mic-open-legacy.json",
    "control-mic-close-v1.json",
    "control-mic-close-legacy.json",
)


class AtvvControlNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        cls._rn = _rn
        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; shadow parity skipped"
            )

        from ovb_rc003.atvv_native_bridge import (
            mic_close_command,
            mic_open_command,
            parse_control,
        )

        # staticmethod wrapper: unittest's TestCase attribute machinery
        # would otherwise bind ``self`` as the first arg. Without this,
        # ``self._parse_control(payload)`` ends up calling
        # ``_shadow(self, payload)`` and the inner python_impl gets 2
        # positional args instead of 1.
        cls._parse_control = staticmethod(parse_control)
        cls._mic_open_command = staticmethod(mic_open_command)
        cls._mic_close_command = staticmethod(mic_close_command)

    # ---- decode fixtures ------------------------------------------------

    def test_decode_python_baseline_matches_expected(self) -> None:
        """Sanity: the Python baseline must still match the JSON's
        expected_* fields for every decode fixture. If this fails, the
        C++ drift is the least of our problems."""
        from ovb_rc003 import atvv_protocol as proto

        for name in _DECODE_FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                payload = bytes.fromhex(fixture["input_hex"])
                py_result = proto.parse_control_payload(payload)

                if "expected" in fixture and fixture["expected"] is None:
                    self.assertIsNone(py_result, f"{name}: expected None")
                    continue

                self.assertIsNotNone(py_result, f"{name}: expected a dict")
                expected_opcode = fixture["expected_opcode"]
                self.assertEqual(py_result["opcode"], expected_opcode)
                if expected_opcode == "AudioStart":
                    self.assertEqual(
                        py_result["session_id"],
                        fixture["expected_session_id"],
                    )
                elif expected_opcode == "AudioSync":
                    self.assertEqual(
                        py_result["predictor"],
                        fixture["expected_predictor"],
                    )
                    self.assertEqual(
                        py_result["step_index"],
                        fixture["expected_step_index"],
                    )
                elif expected_opcode == "Unknown":
                    self.assertEqual(
                        py_result["raw_opcode"],
                        fixture["expected_raw_opcode"],
                    )

    def test_decode_native_matches_python_byte_exact(self) -> None:
        """The actual parity check: python baseline and C++ binding
        must yield identical dict shapes for every decode fixture,
        with no tolerance anywhere (per ADR-0012 §5 / plan §1 rule 3).
        """
        for name in _DECODE_FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                payload = bytes.fromhex(fixture["input_hex"])

                # ``_parse_control`` is in shadow mode here, so the
                # helper runs both implementations, asserts equality,
                # and returns the python result. Any drift raises
                # RuntimeError inside ``_shadow``.
                bridge_result = self._parse_control(payload)

                if "expected" in fixture and fixture["expected"] is None:
                    self.assertIsNone(
                        bridge_result,
                        f"{name}: shadow returned a dict, expected None",
                    )
                    continue

                self.assertIsNotNone(
                    bridge_result, f"{name}: shadow returned None"
                )
                expected_opcode = fixture["expected_opcode"]
                self.assertEqual(bridge_result["opcode"], expected_opcode)
                if expected_opcode == "AudioStart":
                    self.assertEqual(
                        bridge_result["session_id"],
                        fixture["expected_session_id"],
                    )
                elif expected_opcode == "AudioSync":
                    self.assertEqual(
                        bridge_result["predictor"],
                        fixture["expected_predictor"],
                    )
                    self.assertEqual(
                        bridge_result["step_index"],
                        fixture["expected_step_index"],
                    )
                elif expected_opcode == "Unknown":
                    self.assertEqual(
                        bridge_result["raw_opcode"],
                        fixture["expected_raw_opcode"],
                    )

    # ---- encode fixtures ------------------------------------------------

    def test_encode_python_baseline_matches_expected(self) -> None:
        from ovb_rc003 import atvv_protocol as proto

        for name in _ENCODE_FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                func = fixture["function"]
                inp = fixture["input"]
                if func == "mic_open_command":
                    result = proto.mic_open_command(inp["version"])
                else:
                    result = proto.mic_close_command(
                        inp["version"], inp["session_id"]
                    )
                self.assertEqual(
                    result, bytes.fromhex(fixture["expected_hex"]),
                    f"{name}: python baseline != expected",
                )

    def test_encode_native_matches_python_byte_exact(self) -> None:
        for name in _ENCODE_FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                inp = fixture["input"]
                func = fixture["function"]
                if func == "mic_open_command":
                    bridge_result = self._mic_open_command(inp["version"])
                else:
                    bridge_result = self._mic_close_command(
                        inp["version"], inp["session_id"]
                    )
                self.assertEqual(
                    bridge_result, bytes.fromhex(fixture["expected_hex"]),
                    f"{name}: shadow != expected",
                )


if __name__ == "__main__":
    unittest.main()