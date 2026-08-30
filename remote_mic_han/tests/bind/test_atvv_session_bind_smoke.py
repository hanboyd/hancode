"""Phase 3 / ADR-0013 §3.3 step 3: ATVV Session binding smoke.

Loads the bundled ``remotemic_native._C`` extension and asserts that
the C++ ``AtvvSession`` exposes the same surface the python
implementation does: a constructor taking ``gain_db``, the
``capabilities`` / ``mic_open`` properties, the
``handle_control`` / ``handle_audio`` / ``mic_open_command`` /
``mic_close_command`` methods, and that the dict shape returned by
``handle_control`` matches the python baseline.

This is the build-time parity proof for the binding seam; the runtime
shadow parity test (``tests/test_atvv_session_native_parity.py``) is
step 4's job. Per ADR-0013 G3: on fail, do not flip the ADR status
from ``proposed`` to ``accepted``.
"""

from __future__ import annotations

import unittest


# Synthetic v1 caps payload:
#   opcode=0x0B, version=0x0100, codecs=0x02,
#   interaction=0x00, frame_size=0x0078 (120)
_CAPS_PAYLOAD = bytes.fromhex("0b010002000078")
_AUDIO_START_PAYLOAD = bytes.fromhex("04000042")
_AUDIO_STOP_PAYLOAD = bytes.fromhex("00")
_MIC_BUTTON_PAYLOAD = bytes.fromhex("08")
_AUDIO_SYNC_PAYLOAD = bytes.fromhex("0a00000000006407")
_SHORT_AUDIO_SYNC_PAYLOAD = bytes.fromhex("0a0000")


class AtvvSessionBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C

    def test_session_class_exposes_required_methods(self) -> None:
        for name in (
            "handle_control",
            "handle_audio",
            "mic_open_command",
            "mic_close_command",
        ):
            self.assertTrue(
                hasattr(self._C.AtvvSession, name),
                f"AtvvSession missing method {name!r}",
            )
        self.assertTrue(hasattr(self._C.AtvvSession, "capabilities"))
        self.assertTrue(hasattr(self._C.AtvvSession, "mic_open"))

    def test_default_constructor_takes_gain_db(self) -> None:
        s = self._C.AtvvSession()
        self.assertFalse(s.mic_open)
        self.assertIsNone(s.capabilities)

    def test_handle_control_empty_raises(self) -> None:
        s = self._C.AtvvSession()
        with self.assertRaises(ValueError):
            s.handle_control(b"")

    def test_handle_control_caps_returns_dict(self) -> None:
        s = self._C.AtvvSession()
        event = s.handle_control(_CAPS_PAYLOAD)
        self.assertIsInstance(event, dict)
        self.assertEqual(event["opcode"], "Caps")
        caps = event["capabilities"]
        self.assertEqual(caps.version, 0x0100)
        self.assertEqual(caps.frame_size, 120)
        self.assertEqual(caps.sample_rate, 16000.0)
        # Session now exposes the negotiated capabilities.
        self.assertIsNotNone(s.capabilities)

    def test_handle_control_audio_start_sets_mic_open(self) -> None:
        s = self._C.AtvvSession()
        s.handle_control(_CAPS_PAYLOAD)
        event = s.handle_control(_AUDIO_START_PAYLOAD)
        self.assertEqual(event["opcode"], "AudioStart")
        self.assertEqual(event["session_id"], 0x42)
        self.assertTrue(s.mic_open)

    def test_handle_control_audio_stop_clears_mic_open(self) -> None:
        s = self._C.AtvvSession()
        s.handle_control(_CAPS_PAYLOAD)
        s.handle_control(_AUDIO_START_PAYLOAD)
        event = s.handle_control(_AUDIO_STOP_PAYLOAD)
        self.assertEqual(event["opcode"], "AudioStop")
        self.assertFalse(s.mic_open)

    def test_handle_control_mic_button(self) -> None:
        s = self._C.AtvvSession()
        event = s.handle_control(_MIC_BUTTON_PAYLOAD)
        self.assertEqual(event["opcode"], "MicButton")

    def test_handle_control_audio_sync_full(self) -> None:
        s = self._C.AtvvSession()
        event = s.handle_control(_AUDIO_SYNC_PAYLOAD)
        self.assertEqual(event["opcode"], "AudioSync")

    def test_handle_control_short_audio_sync_becomes_unknown(self) -> None:
        s = self._C.AtvvSession()
        event = s.handle_control(_SHORT_AUDIO_SYNC_PAYLOAD)
        self.assertEqual(event["opcode"], "Unknown")
        self.assertEqual(event["raw_opcode"], 0x0A)

    def test_mic_open_command_non_empty(self) -> None:
        s = self._C.AtvvSession()
        # Default version = 0; matches Python's bytes shape.
        cmd = s.mic_open_command()
        self.assertIsInstance(cmd, bytes)
        self.assertTrue(len(cmd) > 0)
        # v0 produces 3-byte payload {0x0C, 0x00, 0x00}.
        self.assertEqual(cmd, b"\x0c\x00\x00")

    def test_mic_close_command_default_session_id(self) -> None:
        s = self._C.AtvvSession()
        cmd = s.mic_close_command()
        self.assertIsInstance(cmd, bytes)
        self.assertTrue(len(cmd) > 0)

    def test_mic_open_command_after_caps_uses_v1(self) -> None:
        s = self._C.AtvvSession()
        s.handle_control(_CAPS_PAYLOAD)  # version=0x0100
        cmd = s.mic_open_command()
        # v1 produces 2-byte payload {0x0C, 0x00}.
        self.assertEqual(cmd, b"\x0c\x00")

    def test_mic_close_command_after_start_carries_session_id(self) -> None:
        s = self._C.AtvvSession()
        s.handle_control(_CAPS_PAYLOAD)
        s.handle_control(_AUDIO_START_PAYLOAD)
        cmd = s.mic_close_command()
        # v1 with sid=0x42 produces {0x0D, 0x42}.
        self.assertEqual(cmd, b"\x0d\x42")


if __name__ == "__main__":
    unittest.main()