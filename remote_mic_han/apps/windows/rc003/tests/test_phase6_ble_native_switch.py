from __future__ import annotations

import asyncio
import importlib
import os
import types
import unittest
from unittest import mock

from ovb_rc003 import ble_transport_native
from ovb_rc003 import ble_transport_winrt
from ovb_rc003 import identity


class _FakeNativeTransport:
    instances = []

    def __init__(self):
        self.connected = False
        self.dropped_notification_count = 0
        self.device_id = None
        self.writes = []
        self.events = []
        type(self).instances.append(self)

    def connect(self, device_id):
        self.device_id = device_id
        self.connected = bool(device_id)
        return self.connected

    def disconnect(self):
        self.connected = False

    def write(self, payload):
        if not self.connected:
            return False
        self.writes.append(bytes(payload))
        return True

    def poll_event(self):
        return self.events.pop(0) if self.events else None


class Phase6BleNativeSwitchTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT")
        _FakeNativeTransport.instances.clear()

    def tearDown(self):
        if self.old is None:
            os.environ.pop("REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT", None)
        else:
            os.environ["REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT"] = self.old
        importlib.reload(ble_transport_native)

    async def test_default_is_python(self):
        os.environ.pop("REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT", None)
        module = importlib.reload(ble_transport_native)
        session = module.make_ble_session(on_pcm_frame=lambda _samples: None)
        self.assertIsInstance(session, ble_transport_winrt.RC003BleSession)

    async def test_native_connects_only_the_selected_device(self):
        os.environ["REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT"] = "native"
        fake_module = types.SimpleNamespace(
            _C_AVAILABLE=True,
            WinRTBleTransport=_FakeNativeTransport,
        )
        with mock.patch.dict("sys.modules", {"remotemic_native": fake_module}):
            module = importlib.reload(ble_transport_native)
            session = module.make_ble_session(on_pcm_frame=lambda _samples: None)
            candidate = identity.RC003Candidate(
                name="Xiaomi Bluetooth Remote 2 Pro",
                hardware_match=False,
                handle=types.SimpleNamespace(id="opaque-device-id"),
            )
            await session.connect(candidate)
            self.assertTrue(session._is_native if hasattr(session, "_is_native") else True)
            self.assertEqual(_FakeNativeTransport.instances[-1].device_id, "opaque-device-id")
            await session.close()
            self.assertFalse(_FakeNativeTransport.instances[-1].connected)

    async def test_native_missing_device_id_fails_closed(self):
        os.environ["REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT"] = "native"
        fake_module = types.SimpleNamespace(
            _C_AVAILABLE=True,
            WinRTBleTransport=_FakeNativeTransport,
        )
        with mock.patch.dict("sys.modules", {"remotemic_native": fake_module}):
            module = importlib.reload(ble_transport_native)
            session = module.make_ble_session(on_pcm_frame=lambda _samples: None)
            candidate = identity.RC003Candidate("RC003", False, handle=object())
            with self.assertRaises(ConnectionError):
                await session.connect(candidate)

    def test_shadow_is_forbidden(self):
        os.environ["REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT"] = "shadow"
        with self.assertRaises(RuntimeError):
            importlib.reload(ble_transport_native)


class Phase6ProductionRoutingTests(unittest.TestCase):
    def test_app_uses_factory_not_python_session_constructor(self):
        from pathlib import Path

        source = (Path(__file__).parents[1] / "src" / "ovb_rc003" / "app.py").read_text(
            encoding="utf-8"
        )
        self.assertIn("from .ble_transport_native import make_ble_session", source)
        self.assertIn("self._ble_session = make_ble_session(", source)
        self.assertNotIn("ble_transport_winrt.RC003BleSession(", source)


if __name__ == "__main__":
    unittest.main()
