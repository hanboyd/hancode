"""Phase 3 / ADR-0013 §3.3 step 4: runtime shadow parity test for
ATVVSession (G5 of ADR-0013).

When ``REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION=shadow`` is set, the
python-side ``ATVVSession`` (``ovb_rc003.atvv_session``) and the
C++-side ``Session`` (via
``ovb_rc003.atvv_session_native.make_atvv_session``) must yield
identical state transitions and byte-identical
``handle_control`` / ``handle_audio`` / ``mic_open_command`` /
``mic_close_command`` outputs across a scripted protocol sequence.

Both implementations share the same surface; the bridge wrappers
normalize the python-side dataclass to the same dict shape the C++
binding returns, so shadow parity compares apples-to-apples.

Hard rule from the user (phase 3 entry scope): no tolerance.
Any drift fails this test and per ADR-0013 §6 that aborts Phase 3
Area 3 entirely.

The test is skipped if the binding is unavailable
(``_C_AVAILABLE == False``) so source-tree imports without a CMake build
still get a green test suite.

Env-leak safety: the env override is set inside ``setUpClass`` and
restored in ``tearDownClass`` (NOT at module top), matching the
corrective fix pattern from commit 5ce9bd5.
"""

from __future__ import annotations

import os
import unittest
from typing import List

from ovb_rc003 import atvv_session as py_mod
from ovb_rc003.atvv_session_native import (
    make_atvv_session_python,
    make_atvv_session_native,
)


# Synthetic v1 caps payload: opcode=0x0B, version=0x0100, codecs=0x02,
# interaction=0x00, frame_size=0x0078 (120). The python baseline
# rejects anything that doesn't have sample_rate == 16 kHz, so the
# caps payload is the same shape the python baseline accepts.
_CAPS_PAYLOAD = bytes.fromhex("0b010002000078")
_AUDIO_START_PAYLOAD = bytes.fromhex("04000042")
_AUDIO_STOP_PAYLOAD = bytes.fromhex("00")
_MIC_BUTTON_PAYLOAD = bytes.fromhex("08")
_AUDIO_SYNC_PAYLOAD = bytes.fromhex("0a00000000006407")
_SHORT_AUDIO_SYNC_PAYLOAD = bytes.fromhex("0a0000")


class _Scenario:
    def __init__(
        self,
        name: str,
        control_payloads: List[bytes],
        audio_payloads: List[bytes],
        check_mic_open_command: bool = True,
        check_mic_close_command: bool = True,
    ) -> None:
        self.name = name
        self.control_payloads = control_payloads
        self.audio_payloads = audio_payloads
        self.check_mic_open_command = check_mic_open_command
        self.check_mic_close_command = check_mic_close_command


_SCENARIOS: List[_Scenario] = [
    _Scenario(
        "caps + start + stop",
        [_CAPS_PAYLOAD, _AUDIO_START_PAYLOAD, _AUDIO_STOP_PAYLOAD],
        [],
    ),
    _Scenario(
        "mic_button only",
        [_MIC_BUTTON_PAYLOAD],
        [],
        check_mic_open_command=False,
        check_mic_close_command=False,
    ),
    _Scenario(
        "short audio_sync becomes unknown",
        [_SHORT_AUDIO_SYNC_PAYLOAD],
        [],
        check_mic_open_command=False,
        check_mic_close_command=False,
    ),
    _Scenario(
        "audio_dropped_inside_late_audio_guard: caps+start+stop+audio",
        [_CAPS_PAYLOAD, _AUDIO_START_PAYLOAD, _AUDIO_STOP_PAYLOAD],
        [b"\xaa\xbb" * 60],
        # mic_open_command needs caps to have arrived (so the
        # python baseline has version=0x0100); we did get caps here.
    ),
]


class AtvvSessionNativeParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._env_was_set = (
            "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION" in os.environ
        )
        cls._env_old = os.environ.get(
            "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION"
        )
        os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION"] = "shadow"

        import remotemic_native as _rn  # type: ignore[import-not-found]

        cls._rn = _rn
        if not getattr(_rn, "_C_AVAILABLE", False):
            raise unittest.SkipTest(
                "remotemic_native._C not available; "
                "shadow parity skipped"
            )

    @classmethod
    def tearDownClass(cls) -> None:
        if cls._env_was_set:
            os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION"] = (
                cls._env_old
            )
        else:
            os.environ.pop(
                "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION", None
            )

    def _script(self, session, scenario: _Scenario) -> dict:
        events: List[object] = []
        audio_samples: List[object] = []
        for payload in scenario.control_payloads:
            events.append(session.handle_control(payload))
        for payload in scenario.audio_payloads:
            audio_samples.append(session.handle_audio(payload))
        result = {
            "events": events,
            "audio_samples": audio_samples,
            "mic_open": session.mic_open,
            "capabilities": session.capabilities,
        }
        if scenario.check_mic_open_command:
            result["mic_open_command"] = session.mic_open_command()
        if scenario.check_mic_close_command:
            result["mic_close_command"] = session.mic_close_command()
        return result

    def _normalize_events(self, events: List[object]) -> List[dict]:
        """Convert the python-side dataclasses to the same dict shape
        the native binding returns, so cross-impl comparison is
        apples-to-apples."""
        from ovb_rc003.atvv_session_native import _event_to_dict

        return [_event_to_dict(event) for event in events]

    def test_all_scenarios_drive_python_and_native(self) -> None:
        for scenario in _SCENARIOS:
            with self.subTest(scenario=scenario.name):
                py_session = make_atvv_session_python()
                native_session = make_atvv_session_native()
                py_result = self._script(py_session, scenario)
                native_result = self._script(native_session, scenario)

                # Both sides need normalization to a common shape:
                # - python side returns dataclasses (CapsReceived etc.)
                # - native side returns dicts ({"opcode": "Caps", ...})
                # The bridge wrapper exposes ``_event_to_dict`` for
                # this; it works on both dataclasses and dicts
                # (idempotent).
                from ovb_rc003.atvv_session_native import _event_to_dict

                py_events_dict = [
                    _event_to_dict(event) for event in py_result["events"]
                ]
                native_events_dict = [
                    _event_to_dict(event)
                    for event in native_result["events"]
                ]
                self.assertEqual(
                    py_events_dict,
                    native_events_dict,
                    f"scenario {scenario.name!r} handle_control "
                    f"events mismatch",
                )
                self.assertEqual(
                    py_result["audio_samples"],
                    native_result["audio_samples"],
                    f"scenario {scenario.name!r} handle_audio "
                    f"samples mismatch",
                )
                self.assertEqual(
                    py_result["mic_open"],
                    native_result["mic_open"],
                    f"scenario {scenario.name!r} mic_open mismatch",
                )
                if scenario.check_mic_open_command:
                    self.assertEqual(
                        py_result["mic_open_command"],
                        native_result["mic_open_command"],
                        f"scenario {scenario.name!r} "
                        f"mic_open_command mismatch",
                    )
                if scenario.check_mic_close_command:
                    self.assertEqual(
                        py_result["mic_close_command"],
                        native_result["mic_close_command"],
                        f"scenario {scenario.name!r} "
                        f"mic_close_command mismatch",
                    )

    def test_native_rejections_keep_python_exception_abi(self) -> None:
        from ovb_rc003 import atvv_session

        cases = (
            (b"", atvv_session.ATVVProtocolError),
            (b"\x0b\x01\x00", atvv_session.ATVVProtocolError),
            (
                b"\x0b\x01\x00\x01\x00\x00\x78",
                atvv_session.UnsupportedSampleRateError,
            ),
        )
        for payload, expected_error in cases:
            with self.subTest(payload=payload.hex()):
                native_session = make_atvv_session_native()
                with self.assertRaises(expected_error):
                    native_session.handle_control(payload)
                self.assertIsNone(native_session.capabilities)


if __name__ == "__main__":
    unittest.main()
