"""Fixture tests for the RC003 carrier-remap transform.

Loads the production remap.c (compiled to a bare CRT-free DLL by
run_remap_dll.bat) via ctypes and replays the real historical raw reports
captured from this machine's RC003 on 2026-08-29/31 (app.log):

    back        010000f10000000000
    volume up   010000800000000000
    volume down 010000810000000000
    ok          010000280000000000
    mic         0100003e0000000000
    down        010000510000000000
    up          010000520000000000
    release     010000000000000000

Run from the driver directory:
    python tests/remap_fixtures.py
"""

import ctypes
import os
import sys
import unittest

DLL_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "bin",
    "remap.dll",
)


def load_remap():
    if not os.path.isfile(DLL_PATH):
        raise unittest.SkipTest(
            "remap.dll not built - run run_remap_dll.bat first"
        )
    library = ctypes.WinDLL(DLL_PATH) if os.name == "nt" else ctypes.CDLL(DLL_PATH)
    library.Rc003RemapReport.argtypes = (
        ctypes.POINTER(ctypes.c_ubyte),
        ctypes.c_uint64,
    )
    library.Rc003RemapReport.restype = ctypes.c_uint
    return library


class RemapFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._remap = load_remap()

    def remap(self, data: bytes) -> tuple:
        buffer = (ctypes.c_ubyte * len(data)).from_buffer_copy(data)
        replaced = self._remap.Rc003RemapReport(buffer, len(data))
        return bytes(buffer), replaced

    def assert_report(self, raw_hex: str, expected_hex: str, replaced: int):
        raw = bytes.fromhex(raw_hex)
        expected = bytes.fromhex(expected_hex)
        actual, count = self.remap(raw)
        self.assertEqual(actual, expected, f"{raw_hex} -> {actual.hex()}")
        self.assertEqual(count, replaced)

    def test_volume_up_0080_becomes_f13_0068(self):
        self.assert_report(
            "010000800000000000", "010000680000000000", 1
        )

    def test_volume_down_0081_becomes_f14_0069(self):
        self.assert_report(
            "010000810000000000", "010000690000000000", 1
        )

    def test_back_00f1_becomes_f15_006a(self):
        self.assert_report(
            "010000f10000000000", "0100006a0000000000", 1
        )

    def test_ok_0028_unchanged(self):
        self.assert_report(
            "010000280000000000", "010000280000000000", 0
        )

    def test_arrow_down_0051_unchanged(self):
        self.assert_report(
            "010000510000000000", "010000510000000000", 0
        )

    def test_arrow_up_0052_unchanged(self):
        self.assert_report(
            "010000520000000000", "010000520000000000", 0
        )

    def test_mic_003e_unchanged(self):
        self.assert_report(
            "0100003e0000000000", "0100003e0000000000", 0
        )

    def test_release_all_zero_unchanged_no_stuck_key(self):
        self.assert_report(
            "010000000000000000", "010000000000000000", 0
        )

    def test_multi_slot_only_matched_slot_changes(self):
        # Target at bytes 3-4 (slot 1), OK at bytes 5-6 (slot 2).
        self.assert_report(
            "01000080002800000000", "01000068002800000000", 1
        )

    def test_multi_slot_two_targets_both_rewrite(self):
        # Volume up at slot 0 (bytes 1-2), back at slot 1 (bytes 3-4).
        self.assert_report(
            "018000f10000000000", "0168006a0000000000", 2
        )

    def test_vendor_report_rid_6_untouched(self):
        raw = bytes([0x06, 0x00, 0x00, 0x80, 0x00]) + bytes(116)
        actual, replaced = self.remap(raw)
        self.assertEqual(actual, raw)
        self.assertEqual(replaced, 0)

    def test_short_buffer_untouched(self):
        self.assert_report("01000080", "01000080", 0)

    def test_non_target_usage_byte_identical(self):
        # A 9-byte rid-1 report whose slots contain only ordinary usages
        # must come back byte-identical (memcmp-equality by construction).
        raw = bytes.fromhex("01004f000028000000")
        actual, replaced = self.remap(raw)
        self.assertEqual(actual, raw)
        self.assertEqual(replaced, 0)


if __name__ == "__main__":
    unittest.main()
