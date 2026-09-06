"""Carrier-key integration tests for the RC003 HID filter prototype.

The device-specific lower filter rewrites the three kbdhid-invisible usages
(0x0080/0x0081/0x00F1) into F13/F14/F15.  These tests verify the user-mode
half: the Raw Input keyboard decoder maps the carrier VKs to the same
logical button ids the saved bindings already reference, without exposing
the carrier keys, and that nothing else changed.
"""

import struct
import unittest

from ovb_rc003 import device_profile, hid_identity, raw_input_windows

WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101

# Real shapes for this machine's RC003 keyboard collection (BLE HID path
# contains Dev_VID&012717_PID&32B8; a USB HID keyboard would contain
# VID_2717&PID_32B8 instead).
RC003_PATH = (
    "\\\\?\\HID#{00001812-0000-1000-8000-00805f9b34fb}"
    "_Dev_VID&012717_PID&32b8_REV&00a4_c05d39c2c486"
    "\\8&647af85&0&0055"
)
OTHER_RC003_SHAPED_PATH = (
    "\\\\?\\HID#{00001812-0000-1000-8000-00805f9b34fb}"
    "_Dev_VID&012717_PID&32b8_REV&00a4_c05d39c2c486"
    "\\8&647af85&0&0066"
)
NON_RC003_PATH = "\\\\?\\HID#VID_046D&PID_C52B&MI_00#7&2a3b4c5d&0&0000"


def _rawkeyboard_body(vkey: int, message: int = WM_KEYDOWN) -> bytes:
    # RAWKEYBOARD: MakeCode(u16) Flags(u16) Reserved(u16) VKey(u16) Message(u32) ExtraInformation(u32)
    return struct.pack("<HHHHII", 0, 0, 0, vkey, message, 0)


class RecordingListener:
    def __init__(self):
        self.events = []
        self.listener = raw_input_windows.RawInputButtonListener(
            self._on_event, None
        )

    def _on_event(self, button_id, is_pressed):
        self.events.append((button_id, is_pressed))


class CarrierKeyTests(unittest.TestCase):
    def test_carrier_vks_map_to_logical_buttons(self):
        self.assertEqual(
            raw_input_windows.KEYBOARD_VK_TO_BUTTON[0x7C], "volume_up"
        )
        self.assertEqual(
            raw_input_windows.KEYBOARD_VK_TO_BUTTON[0x7D], "volume_down"
        )
        self.assertEqual(
            raw_input_windows.KEYBOARD_VK_TO_BUTTON[0x7E], "back"
        )

    def test_carrier_ids_match_the_saved_binding_ids(self):
        # The saved bindings (back -> Delete, volume_up -> Ctrl+C,
        # volume_down -> Ctrl+V) reference the same ids the physical-usage
        # table uses - so existing configuration applies without changes.
        self.assertEqual(
            raw_input_windows.KEYBOARD_VK_TO_BUTTON[0x7C],
            device_profile.BUTTON_USAGE_IDS[0x0080],
        )
        self.assertEqual(
            raw_input_windows.KEYBOARD_VK_TO_BUTTON[0x7D],
            device_profile.BUTTON_USAGE_IDS[0x0081],
        )
        self.assertEqual(
            raw_input_windows.KEYBOARD_VK_TO_BUTTON[0x7E],
            device_profile.BUTTON_USAGE_IDS[0x00F1],
        )

    def test_f13_press_and_release_emit_volume_up_edges(self):
        rec = RecordingListener()
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7C, WM_KEYDOWN))
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7C, WM_KEYUP))
        self.assertEqual(rec.events, [("volume_up", True), ("volume_up", False)])

    def test_f14_press_and_release_emit_volume_down_edges(self):
        rec = RecordingListener()
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7D, WM_KEYDOWN))
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7D, WM_KEYUP))
        self.assertEqual(rec.events, [("volume_down", True), ("volume_down", False)])

    def test_f15_press_and_release_emit_back_edges(self):
        rec = RecordingListener()
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7E, WM_KEYDOWN))
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7E, WM_KEYUP))
        self.assertEqual(rec.events, [("back", True), ("back", False)])

    def test_repeated_carrier_keydown_is_one_logical_press(self):
        rec = RecordingListener()
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7C, WM_KEYDOWN))
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7C, WM_KEYDOWN))
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7C, WM_KEYUP))
        self.assertEqual(rec.events, [("volume_up", True), ("volume_up", False)])

    def test_release_without_prior_press_is_inert(self):
        rec = RecordingListener()
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x7E, WM_KEYUP))
        self.assertEqual(rec.events, [])

    def test_working_keys_keep_their_existing_mapping(self):
        cases = {
            0x74: "mic",  # VK_F5 voice key
            0x0D: "ok",
            0x26: "up",
            0x27: "right",
            0x25: "left",
            0x28: "down",
            0x24: "home",
            0x5D: "menu",
            0xC0: "tv",
            0x5F: "power",
            0xAD: "volume_mute",
            0xAF: "volume_up",
            0xAE: "volume_down",
        }
        for vkey, expected in cases.items():
            with self.subTest(vkey=hex(vkey)):
                self.assertEqual(
                    raw_input_windows.KEYBOARD_VK_TO_BUTTON[vkey], expected
                )

    def test_unknown_vkey_still_emits_no_logical_button(self):
        rec = RecordingListener()
        rec.listener._handle_keyboard_body(_rawkeyboard_body(0x99, WM_KEYDOWN))
        self.assertEqual(rec.events, [])


class CarrierDeviceScopeTests(unittest.TestCase):
    """The carrier VKs (F13/F14/F15) are only meaningful on the RC003: a
    physical F13-F15 keyboard must never be mistaken for the remote.  The
    gate is the per-event exact-device-path check the listener applies
    before any VK-to-button lookup."""

    def _listener_scoped_to(self, path):
        listener = raw_input_windows.RawInputButtonListener(lambda *_: None, None)
        listener._normalized_device_path = hid_identity.normalize_device_path(path)
        return listener

    def test_event_from_the_exact_selected_rc003_path_is_accepted(self):
        listener = self._listener_scoped_to(RC003_PATH)
        self.assertTrue(listener._device_path_matches(RC003_PATH))

    def test_second_rc003_shaped_device_path_is_rejected(self):
        # Even a path that still matches the RC003 VID/PID spelling must be
        # rejected when it is not the exact path start() selected.
        listener = self._listener_scoped_to(RC003_PATH)
        self.assertFalse(listener._device_path_matches(OTHER_RC003_SHAPED_PATH))

    def test_physical_f13_f15_keyboard_path_is_rejected(self):
        # A normal keyboard's F13/F14/F15 events carry a non-RC003 device
        # path and never reach KEYBOARD_VK_TO_BUTTON.
        listener = self._listener_scoped_to(RC003_PATH)
        self.assertFalse(listener._device_path_matches(NON_RC003_PATH))

    def test_missing_or_empty_device_path_is_rejected(self):
        listener = self._listener_scoped_to(RC003_PATH)
        self.assertFalse(listener._device_path_matches(None))
        self.assertFalse(listener._device_path_matches(""))

    def test_case_insensitive_exact_match(self):
        listener = self._listener_scoped_to(RC003_PATH)
        self.assertTrue(listener._device_path_matches(RC003_PATH.lower()))


if __name__ == "__main__":
    unittest.main()
