"""Phase 2 / Area 2 step 4: ATVV control binding smoke (G3).

Loads the bundled remotemic_native._C extension, calls the three
newly-bound control functions against each JSON fixture, and asserts
the binding returns match the C++ unit test
(``remotemic_atvv_control_tests``).

Per ADR-0012 G3: on fail, do not flip the ADR status from ``proposed``
to ``accepted``. The runtime shadow parity test
(``tests/test_atvv_native_parity_control.py``) is step 5's job.
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


class AtvvControlBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # The binding must be present (the build only runs this test
        # when REMOTEMIC_BUILD_PYTHON=ON, and CMake stashes the .pyd
        # next to remotemic_native/__init__.py inside the build tree).
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.parse = _C.atvv_control_parse
        cls.mic_open = _C.atvv_mic_open_command
        cls.mic_close = _C.atvv_mic_close_command

    # ---- encode fixtures ------------------------------------------------

    def test_mic_open_v1_encode(self) -> None:
        f = _load("control-mic-open-v1.json")
        out = self.mic_open(f["input"]["version"])
        self.assertEqual(out, bytes.fromhex(f["expected_hex"]))

    def test_mic_open_legacy_encode(self) -> None:
        f = _load("control-mic-open-legacy.json")
        out = self.mic_open(f["input"]["version"])
        self.assertEqual(out, bytes.fromhex(f["expected_hex"]))

    def test_mic_close_v1_encode(self) -> None:
        f = _load("control-mic-close-v1.json")
        out = self.mic_close(f["input"]["version"], f["input"]["session_id"])
        self.assertEqual(out, bytes.fromhex(f["expected_hex"]))

    def test_mic_close_legacy_encode(self) -> None:
        f = _load("control-mic-close-legacy.json")
        out = self.mic_close(f["input"]["version"], f["input"]["session_id"])
        self.assertEqual(out, bytes.fromhex(f["expected_hex"]))

    # ---- decode fixtures ------------------------------------------------

    def test_decode_caps(self) -> None:
        f = _load("control-decode-caps.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertEqual(result, {"opcode": "Caps"})

    def test_decode_mic_button(self) -> None:
        f = _load("control-decode-mic-button.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertEqual(result, {"opcode": "MicButton"})

    def test_decode_audio_start_with_sid(self) -> None:
        f = _load("control-decode-audio-start-with-sid.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertEqual(
            result,
            {"opcode": "AudioStart", "session_id": f["expected_session_id"]},
        )

    def test_decode_audio_start_no_sid(self) -> None:
        f = _load("control-decode-audio-start-no-sid.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertEqual(result, {"opcode": "AudioStart", "session_id": None})

    def test_decode_audio_stop(self) -> None:
        f = _load("control-decode-audio-stop.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertEqual(result, {"opcode": "AudioStop"})

    def test_decode_audio_sync(self) -> None:
        f = _load("control-decode-audio-sync.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertEqual(
            result,
            {
                "opcode": "AudioSync",
                "predictor": f["expected_predictor"],
                "step_index": f["expected_step_index"],
            },
        )

    def test_decode_unknown(self) -> None:
        f = _load("control-decode-unknown.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertEqual(
            result,
            {"opcode": "Unknown", "raw_opcode": f["expected_raw_opcode"]},
        )

    def test_decode_empty(self) -> None:
        f = _load("control-decode-empty.json")
        result = self.parse(bytes.fromhex(f["input_hex"]))
        self.assertIsNone(result)


if __name__ == "__main__":
    unittest.main()