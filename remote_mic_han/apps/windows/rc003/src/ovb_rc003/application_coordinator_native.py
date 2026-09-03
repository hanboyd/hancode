"""Phase 7 Python UI bridge for the C++ application coordinator."""

from __future__ import annotations

import asyncio
import queue
import threading
import time
from typing import Optional

import remotemic_native as native

from . import (
    ble_transport_winrt,
    config,
    connection_supervisor,
    hotkey,
    identity,
    key_mapping,
    legacy_key_suppressor_windows,
    logging_setup,
    voice_edge_debouncer_native,
    win32_input,
    win32_keys,
)


class NativeCoordinatorApp:
    """Keep settings/retry policy in Python while C++ owns live backends."""

    def __init__(self) -> None:
        if not native._C_AVAILABLE or native.ApplicationCoordinator is None:
            raise RuntimeError("native application coordinator is unavailable")
        root = config.config_root()
        self._config = config.load_config(config.config_path(root))
        self._logger = logging_setup.get_logger(root)
        # Reuse the established Python gesture/binding/statistics adapter
        # without starting its BLE, HID or audio owners. C++ emits ordinary
        # button edges; this object retains user-configured and third-party
        # actions during Phase 7.
        from .app import RC003App

        self._python_adapter = RC003App()
        self._service: Optional[object] = None
        self._event_task: Optional[asyncio.Task[None]] = None
        self._legacy_key_suppressor: Optional[
            legacy_key_suppressor_windows.LegacyKeySuppressor
        ] = None
        self._legacy_f5_is_down = False
        self._voice_edge_queue: queue.Queue[bool] = queue.Queue(maxsize=32)
        self._voice_edge_thread: Optional[threading.Thread] = None
        self._voice_edge_stop = threading.Event()
        # Whether the latest physical press delivered a working Typeless open
        # tap. A failed open must not be answered by a close tap: Typeless is
        # a toggle, so the close would open it (state inversion).
        self._voice_open_tap_ok = False
        self._voice_edge_debouncer = (
            voice_edge_debouncer_native.make_voice_edge_debouncer(0.200)
        )
        self._voice_hotkey_tokens: tuple[str, ...] = ()
        self._supervisor = connection_supervisor.ConnectionSupervisor(
            connect=self._connect_once,
            cleanup=self._cleanup_once,
            retry_delay=float(self._config.get("retry_delay", 2.0)),
            max_retry_delay=float(self._config.get("max_retry_delay", 60.0)),
            logger=self._logger,
        )

    async def run_forever(self) -> None:
        await self._supervisor.run_forever()

    async def stop(self) -> None:
        await self._supervisor.stop()

    async def _connect_once(self) -> None:
        candidate = identity.select_single_candidate(
            await ble_transport_winrt.discover_candidates()
        )
        device_id = str(candidate.handle.id)
        spec = hotkey.HotkeySpec.parse(str(self._config["voice_hotkey"]))
        self._voice_hotkey_tokens = tuple(spec.modifiers) + (spec.key,)
        voice_keys = list(win32_keys.resolve_vk_codes((*spec.modifiers, spec.key)))
        trigger_mode = (
            native.VoiceTriggerMode.Hold
            if self._config["voice_trigger_mode"] == key_mapping.VoiceTriggerMode.HOLD.value
            else native.VoiceTriggerMode.Toggle
        )
        self._service = native.ApplicationCoordinator(
            device_id,
            str(self._config.get("output_endpoint_name") or ""),
            str(self._config.get("output_endpoint_host_api") or "Windows WASAPI"),
            voice_keys,
            trigger_mode,
            float(self._config["gain_db"]),
        )
        # The low-level F5 hook is the accepted Python baseline's
        # authoritative physical hold source. Start it only after the native
        # service exists, but before adapters begin receiving device input.
        self._start_voice_edge_worker()
        self._start_voice_key_guard(voice_keys)
        result = await asyncio.to_thread(
            self._service.execute, 1, native.CoordinatorCommandKind.Start
        )
        if not result.ok:
            raise RuntimeError(f"native coordinator start failed: {result.message}")
        self._logger.info("native application coordinator started")
        self._event_task = asyncio.create_task(self._pump_events())

    def _start_voice_key_guard(self, voice_keys: list[int]) -> None:
        """Own the legacy F5 leak and physicalize native voice output.

        ``RawInputSource`` remains the device-scoped microphone owner.  The
        low-level hook has two deliberately narrow jobs: swallow only the
        physical RC003 F5 translation, and strip the injected flag only from
        bridge-marked keys in the configured voice chord.  Direction/OK keys
        are not in the suppression set and keep their physical pass-through.
        """

        if self._legacy_key_suppressor is not None:
            return
        guard = legacy_key_suppressor_windows.LegacyKeySuppressor(
            {0x74},
            on_key_event=self._on_legacy_voice_key_event,
            rc003_vk_codes=frozenset({0x74}),
            voice_physicalize_vk_codes=frozenset(int(vk) for vk in voice_keys),
            consume_wait_seconds=0.0,
        )
        try:
            guard.start()
        except legacy_key_suppressor_windows.LegacyKeySuppressorUnavailableError:
            self._logger.exception(
                "native coordinator voice-key guard failed to start"
            )
            raise
        self._legacy_key_suppressor = guard
        self._logger.info(
            "native coordinator RC003 F5 guard and voice physicalizer started"
        )

    def _start_voice_edge_worker(self) -> None:
        if self._voice_edge_thread is not None and self._voice_edge_thread.is_alive():
            return
        # Exit is signalled by ``_voice_edge_stop``, never by a queue
        # sentinel: a stale sentinel would otherwise be consumed by a worker
        # spawned after a reconnect and kill it immediately.
        self._voice_edge_stop.clear()
        self._voice_edge_thread = threading.Thread(
            target=self._run_voice_edge_worker,
            name="native-voice-edge-worker",
            daemon=True,
        )
        self._voice_edge_thread.start()

    def _run_voice_edge_worker(self) -> None:
        while not self._voice_edge_stop.is_set():
            try:
                edge = self._voice_edge_queue.get(timeout=0.250)
            except queue.Empty:
                continue
            pressed = bool(edge)
            # Deliver Typeless first, outside the native coordinator lock.
            # Continuous ATVV audio callbacks can otherwise keep the native
            # mutex busy until the physical key is released, which produces
            # the observed inverted behavior: no window while held, then an
            # opening toggle on release. Both physical edges are complete
            # toggle taps because RC003 is hold-to-talk while Typeless is
            # toggle-to-open / toggle-to-close.
            if pressed:
                try:
                    win32_input.send_voice_key_combo_tap(self._voice_hotkey_tokens)
                    self._voice_open_tap_ok = True
                except Exception:
                    self._voice_open_tap_ok = False
                    self._logger.exception(
                        "native external Typeless open shortcut failed"
                    )
            elif self._voice_open_tap_ok:
                try:
                    win32_input.send_voice_key_combo_tap(self._voice_hotkey_tokens)
                except Exception:
                    self._logger.exception(
                        "native external Typeless close shortcut failed"
                    )
            # The native mic state must always track the physical key, even
            # when a tap failed, so BLE/audio cannot diverge from the hold.
            # Never let an exception kill the worker: a dead consumer would
            # silently stop every later toggle.
            try:
                service = self._service
                if service is not None:
                    service.handle_physical_mic_edge(pressed)
            except Exception:
                self._logger.exception("native physical mic edge delivery failed")

    def _queue_voice_edge(self, pressed: bool) -> None:
        try:
            self._voice_edge_queue.put_nowait(bool(pressed))
        except queue.Full:
            self._logger.warning(
                "native voice edge queue full; dropping %s edge",
                "press" if pressed else "release",
            )

    def _on_legacy_voice_key_event(self, vk_code: int, is_pressed: bool) -> None:
        """Forward suppressed F5 as a non-blocking, deduplicated hold edge."""

        if int(vk_code) != 0x74:
            return
        if is_pressed:
            if self._legacy_f5_is_down:
                return
            self._legacy_f5_is_down = True
            self._voice_edge_debouncer.on_press()
            self._queue_voice_edge(True)
            return
        if not self._legacy_f5_is_down:
            return
        self._legacy_f5_is_down = False
        self._voice_edge_debouncer.on_release(
            lambda: self._queue_voice_edge(False)
        )

    async def _pump_events(self) -> None:
        assert self._service is not None
        while True:
            event = self._service.poll_event()
            if event is None:
                await asyncio.sleep(0.050)
                continue
            kind = event["kind"]
            if kind == native.CoordinatorEventKind.Input:
                detail = str(event["detail"])
                if detail != "unknown" and ":" in detail:
                    button_id, edge = detail.rsplit(":", 1)
                    if self._python_adapter._physical_key_is_authoritative(button_id):
                        continue
                    self._python_adapter._on_raw_input_button_event(
                        button_id, edge == "down"
                    )
                continue
            if kind == native.CoordinatorEventKind.Disconnected:
                self._logger.info("native coordinator observed BLE disconnect")
                self._supervisor.request_reconnect()
                return
            if kind == native.CoordinatorEventKind.AudioStarted:
                self._logger.info("native coordinator audio route started")
                continue
            if kind == native.CoordinatorEventKind.AudioStopped:
                self._logger.info(
                    "native coordinator audio route stopped; BLE audio bytes=%s",
                    self._service.audio_bytes_received,
                )
                continue
            if kind == native.CoordinatorEventKind.Capabilities:
                self._logger.info(
                    "native coordinator capabilities: sample_rate=%s",
                    event["detail"],
                )
                continue
            if kind == native.CoordinatorEventKind.Error:
                # Backend-local faults (audio endpoint, policy lease, host
                # sink) retry on the next physical edge inside the still-
                # running coordinator. A full reconnect would stop the F5
                # guard and take minutes of BLE rediscovery, leaving Notepad
                # exposed to the physical F5 leak the whole time.
                self._logger.info("native coordinator error: %s", event["detail"])
                continue

    async def _cleanup_once(self) -> None:
        task, self._event_task = self._event_task, None
        if task is not None:
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass
        guard = getattr(self, "_legacy_key_suppressor", None)
        self._legacy_key_suppressor = None
        if guard is not None:
            try:
                guard.stop()
            except legacy_key_suppressor_windows.LegacyKeySuppressorUnavailableError:
                self._logger.exception(
                    "native coordinator voice-key guard failed to stop"
                )
        self._legacy_f5_is_down = False
        self._voice_edge_debouncer.shutdown()
        # The guard and debouncer are stopped, so no new edges arrive. Let the
        # worker finish the ones already queued (a release edge must still
        # close Typeless during shutdown) before asking it to exit.
        deadline = time.monotonic() + 1.0
        while not self._voice_edge_queue.empty() and time.monotonic() < deadline:
            await asyncio.sleep(0.020)
        self._voice_open_tap_ok = False
        self._voice_edge_stop.set()
        worker = self._voice_edge_thread
        if worker is not None:
            await asyncio.to_thread(worker.join, 2.0)
            if worker.is_alive():
                self._logger.warning(
                    "native voice edge worker did not stop within 2.0s"
                )
        service, self._service = self._service, None
        if service is not None:
            result = await asyncio.to_thread(
                service.execute, 2, native.CoordinatorCommandKind.Stop
            )
            if not result.ok:
                self._logger.info("native coordinator stop failed: %s", result.message)
        self._python_adapter._button_gestures.reset()


__all__ = ["NativeCoordinatorApp"]
