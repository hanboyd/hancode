import unittest

import remotemic_native as rn


class BleTransportBindSmokeTests(unittest.TestCase):
    def test_native_transport_constructs_without_opening_hardware(self):
        self.assertTrue(rn._C_AVAILABLE)
        transport = rn.WinRTBleTransport()
        self.assertFalse(transport.connected)
        self.assertIsNone(transport.poll_event())
        self.assertEqual(transport.dropped_notification_count, 0)
        transport.disconnect()

    def test_empty_device_id_fails_closed(self):
        transport = rn.WinRTBleTransport()
        self.assertFalse(transport.connect(""))
        self.assertFalse(transport.connected)


if __name__ == "__main__":
    unittest.main()
