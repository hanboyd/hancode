from __future__ import annotations

import os
import queue
import threading
import time
import unittest
from unittest import mock

import remotemic_native as native

from ovb_rc003._remotemic_native_runtime import implementation_choice
from ovb_rc003 import legacy_key_suppressor_windows
from ovb_rc003.application_coordinator_native import NativeCoordinatorApp


class Phase7CoordinatorRoutingTests(unittest.TestCase):
    def test_first_usable_release_default_is_python(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop("REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR", None)
            self.assertEqual(implementation_choice("application_coordinator"), "python")

    def test_native_opt_in_is_recognized(self) -> None:
        with mock.patch.dict(
            os.environ,
            {"REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR": "native"},
        ):
            self.assertEqual(implementation_choice("application_coordinator"), "native")

    def test_python_rollback_is_recognized(self) -> None:
        with mock.patch.dict(
            os.environ,
            {"REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR": "python"},
        ):
            self.assertEqual(implementation_choice("application_coordinator"), "python")

    def test_native_voice_guard_swallows_only_f5_and_physicalizes_voice_chord(self) -> None:
        app = object.__new__(NativeCoordinatorApp)
        app._legacy_key_suppressor = None
        app._logger = mock.Mock()
        app._on_legacy_voice_key_event = mock.Mock()
        guard = mock.Mock()

        with mock.patch(
            "ovb_rc003.application_coordinator_native."
            "legacy_key_suppressor_windows.LegacyKeySuppressor",
            return_value=guard,
        ) as make_guard:
            app._start_voice_key_guard([0xA2, 0xA4])

        make_guard.assert_called_once_with(
            {0x74},
            on_key_event=app._on_legacy_voice_key_event,
            rc003_vk_codes=frozenset({0x74}),
            voice_physicalize_vk_codes=frozenset({0xA2, 0xA4}),
            consume_wait_seconds=0.0,
        )
        guard.start.assert_called_once_with()
        self.assertIs(app._legacy_key_suppressor, guard)

    def test_native_voice_guard_start_failure_is_fail_closed(self) -> None:
        app = object.__new__(NativeCoordinatorApp)
        app._legacy_key_suppressor = None
        app._logger = mock.Mock()
        error = legacy_key_suppressor_windows.LegacyKeySuppressorUnavailableError(
            "hook unavailable"
        )
        guard = mock.Mock()
        guard.start.side_effect = error

        with mock.patch(
            "ovb_rc003.application_coordinator_native."
            "legacy_key_suppressor_windows.LegacyKeySuppressor",
            return_value=guard,
        ):
            with self.assertRaisesRegex(
                legacy_key_suppressor_windows.LegacyKeySuppressorUnavailableError,
                "hook unavailable",
            ):
                app._start_voice_key_guard([0xA2, 0xA4])

        self.assertIsNone(app._legacy_key_suppressor)

    def test_legacy_f5_edges_are_deduplicated_and_release_is_debounced(self) -> None:
        app = object.__new__(NativeCoordinatorApp)
        app._legacy_f5_is_down = False
        app._voice_edge_queue = queue.Queue(maxsize=8)
        app._logger = mock.Mock()
        app._voice_edge_debouncer = mock.Mock()

        app._on_legacy_voice_key_event(0x74, True)
        app._on_legacy_voice_key_event(0x74, True)
        self.assertTrue(app._voice_edge_queue.get_nowait())
        self.assertTrue(app._voice_edge_queue.empty())
        app._voice_edge_debouncer.on_press.assert_called_once_with()

        app._on_legacy_voice_key_event(0x74, False)
        release = app._voice_edge_debouncer.on_release.call_args.args[0]
        release()
        self.assertFalse(app._voice_edge_queue.get_nowait())


class Phase7VoiceEdgeWorkerTests(unittest.TestCase):
    """The Typeless toggle must leave the worker before any native wait."""

    def _make_worker_app(self) -> NativeCoordinatorApp:
        app = object.__new__(NativeCoordinatorApp)
        app._logger = mock.Mock()
        app._voice_edge_queue = queue.Queue()
        app._voice_edge_stop = threading.Event()
        app._voice_edge_thread = None
        app._voice_hotkey_tokens = ("lctrl", "lalt")
        app._voice_open_tap_ok = False
        app._service = mock.Mock()
        return app

    def _run_until(
        self, app: NativeCoordinatorApp, condition, timeout: float = 2.0
    ) -> None:
        deadline = time.monotonic() + timeout
        while not condition() and time.monotonic() < deadline:
            time.sleep(0.01)
        app._voice_edge_stop.set()
        thread = app._voice_edge_thread
        if thread is not None:
            thread.join(timeout=2.0)
        self.assertTrue(condition())

    def test_open_tap_precedes_native_edge(self) -> None:
        app = self._make_worker_app()
        app._voice_edge_queue.put(True)
        with mock.patch(
            "ovb_rc003.application_coordinator_native.win32_input."
            "send_voice_key_combo_tap"
        ) as tap:
            app._start_voice_edge_worker()
            self._run_until(
                app, lambda: app._service.handle_physical_mic_edge.called
            )
        tap.assert_called_once_with(("lctrl", "lalt"))
        app._service.handle_physical_mic_edge.assert_called_once_with(True)
        self.assertTrue(app._voice_open_tap_ok)

    def test_close_tap_skipped_when_open_tap_failed(self) -> None:
        app = self._make_worker_app()
        app._voice_edge_queue.put(True)
        app._voice_edge_queue.put(False)
        with mock.patch(
            "ovb_rc003.application_coordinator_native.win32_input."
            "send_voice_key_combo_tap",
            side_effect=OSError("send failed"),
        ) as tap:
            app._start_voice_edge_worker()
            self._run_until(
                app,
                lambda: app._service.handle_physical_mic_edge.call_count == 2,
            )
        # The failed open tap must never be answered by a close tap, which
        # would toggle Typeless open at release (state inversion).
        self.assertEqual(tap.call_count, 1)
        self.assertEqual(
            app._service.handle_physical_mic_edge.call_args_list,
            [mock.call(True), mock.call(False)],
        )

    def test_worker_survives_native_edge_failure(self) -> None:
        app = self._make_worker_app()
        app._service.handle_physical_mic_edge.side_effect = [
            RuntimeError("native boom"),
            None,
        ]
        app._voice_edge_queue.put(True)
        app._voice_edge_queue.put(False)
        with mock.patch(
            "ovb_rc003.application_coordinator_native.win32_input."
            "send_voice_key_combo_tap"
        ):
            app._start_voice_edge_worker()
            self._run_until(
                app,
                lambda: app._service.handle_physical_mic_edge.call_count == 2,
            )
        # The release edge was still processed after the failed press edge,
        # which proves the exception did not kill the worker.
        self.assertEqual(
            app._service.handle_physical_mic_edge.call_args_list,
            [mock.call(True), mock.call(False)],
        )

    def test_worker_exits_on_stop_event_without_queue_sentinel(self) -> None:
        app = self._make_worker_app()
        app._start_voice_edge_worker()
        app._voice_edge_stop.set()
        thread = app._voice_edge_thread
        assert thread is not None
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())


@unittest.skipUnless(native._C_AVAILABLE, "native binding unavailable")
class Phase7CoordinatorEventPumpTests(unittest.IsolatedAsyncioTestCase):
    async def test_input_edge_reaches_python_adapter_then_disconnect_reconnects(self) -> None:
        app = object.__new__(NativeCoordinatorApp)
        app._service = mock.Mock()
        app._service.poll_event.side_effect = [
            {"kind": native.CoordinatorEventKind.Input, "detail": "right:down"},
            {"kind": native.CoordinatorEventKind.Disconnected, "detail": ""},
        ]
        app._python_adapter = mock.Mock()
        app._python_adapter._physical_key_is_authoritative.return_value = False
        app._supervisor = mock.Mock()
        app._logger = mock.Mock()

        await app._pump_events()

        app._python_adapter._on_raw_input_button_event.assert_called_once_with(
            "right", True
        )
        app._supervisor.request_reconnect.assert_called_once_with()

    async def test_identity_edge_is_not_reinjected_by_python_adapter(self) -> None:
        app = object.__new__(NativeCoordinatorApp)
        app._service = mock.Mock()
        app._service.poll_event.side_effect = [
            {"kind": native.CoordinatorEventKind.Input, "detail": "up:down"},
            {"kind": native.CoordinatorEventKind.Disconnected, "detail": ""},
        ]
        app._python_adapter = mock.Mock()
        app._python_adapter._physical_key_is_authoritative.return_value = True
        app._supervisor = mock.Mock()
        app._logger = mock.Mock()

        await app._pump_events()

        app._python_adapter._on_raw_input_button_event.assert_not_called()


if __name__ == "__main__":
    unittest.main()
