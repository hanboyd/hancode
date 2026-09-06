"""P0 regression guard: the unit suite must never emit real input.

tests/__init__.py replaces win32_input's raw injection backends with a
sentinel that raises as soon as any test calls them.  This module pins
that guard so a future edit cannot silently remove it, and documents the
opt-in escape hatch.
"""

import os
import unittest

from ovb_rc003 import win32_input


class NoLiveInputInjectionGuardTests(unittest.TestCase):
    def test_real_send_input_batch_is_blocked_by_default(self):
        if os.environ.get("REMOTEMIC_ALLOW_LIVE_INPUT_TESTS") == "1":
            self.skipTest("live input tests explicitly enabled")
        with self.assertRaises(AssertionError) as ctx:
            win32_input._real_send_input_batch([(0x41, False)])
        self.assertIn("live input injection blocked", str(ctx.exception))

    def test_real_keybd_event_is_blocked_by_default(self):
        if os.environ.get("REMOTEMIC_ALLOW_LIVE_INPUT_TESTS") == "1":
            self.skipTest("live input tests explicitly enabled")
        with self.assertRaises(AssertionError):
            win32_input._real_keybd_event(0x41, False)

    def test_real_voice_event_is_blocked_by_default(self):
        if os.environ.get("REMOTEMIC_ALLOW_LIVE_INPUT_TESTS") == "1":
            self.skipTest("live input tests explicitly enabled")
        with self.assertRaises(AssertionError):
            win32_input._real_voice_event(0x41, False)

    def test_guard_keeps_windows_backends_available_under_opt_in_env(self):
        # The escape hatch is an environment variable the operator sets
        # explicitly; it must not be set by any packaging/release script.
        self.assertNotEqual(
            os.environ.get("REMOTEMIC_ALLOW_LIVE_INPUT_TESTS"),
            "1",
            "REMOTEMIC_ALLOW_LIVE_INPUT_TESTS must not be enabled during "
            "ordinary validation runs",
        )


if __name__ == "__main__":
    unittest.main()
