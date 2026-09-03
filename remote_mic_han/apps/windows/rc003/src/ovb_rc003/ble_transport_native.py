"""Phase 6 single-owner switch for the RC003 BLE/GATT session.

Discovery and fail-closed identity selection remain in ``ble_transport_winrt``
because they are already hardware-proven. In native mode only the selected
device's connection, notification subscriptions, bounded queue, TX writes and
cleanup move to C++/WinRT. Python and C++ never connect concurrently.
"""

from __future__ import annotations

import asyncio
import threading
from typing import Optional

from . import atvv_session
from . import ble_transport_winrt as py_mod
from ._remotemic_native_runtime import choose_implementation
from .atvv_session_native import make_atvv_session


class NativeRC003BleSession:
    def __init__(
        self,
        on_pcm_frame,
        on_control_event=None,
        on_error=None,
        on_disconnected=None,
        gain_db: float = 10.0,
        winrt=None,
        loop: Optional[asyncio.AbstractEventLoop] = None,
    ) -> None:
        del winrt  # Native WinRT is owned inside the extension.
        import remotemic_native as rn  # type: ignore[import-not-found]

        if not rn._C_AVAILABLE or rn.WinRTBleTransport is None:
            raise RuntimeError("native BLE transport is not available")
        self._transport = rn.WinRTBleTransport()
        self._session = make_atvv_session(gain_db=gain_db)
        self._on_pcm_frame = on_pcm_frame
        self._on_control_event = on_control_event
        self._on_error = on_error
        self._on_disconnected = on_disconnected
        self._loop = loop or asyncio.get_event_loop()
        self._generation = 0
        self._closing = False
        self._worker_stop = threading.Event()
        self._worker_thread: Optional[threading.Thread] = None
        self._write_tasks: set[asyncio.Task] = set()

    @property
    def session(self):
        return self._session

    @property
    def dropped_event_count(self) -> int:
        return int(self._transport.dropped_notification_count)

    async def connect(self, candidate) -> None:
        device_id = str(getattr(candidate.handle, "id", ""))
        if not device_id:
            raise ConnectionError("selected RC003 candidate has no WinRT device id")
        self._generation += 1
        self._closing = False
        connected = await asyncio.to_thread(self._transport.connect, device_id)
        if not connected:
            raise ConnectionError("native WinRT transport could not connect to RC003")
        self._worker_stop.clear()
        self._worker_thread = threading.Thread(
            target=self._worker_loop,
            args=(self._generation,),
            name="RemoteMicNativeBleDispatch",
            daemon=True,
        )
        self._worker_thread.start()

    def _worker_loop(self, generation: int) -> None:
        while not self._worker_stop.wait(0.005):
            event = self._transport.poll_event()
            if event is None:
                continue
            if generation != self._generation:
                continue
            kind, payload = event
            try:
                if kind == 0:
                    samples = self._session.handle_audio(bytes(payload))
                    if samples:
                        self._on_pcm_frame(samples)
                elif kind == 1:
                    control = self._session.handle_control(bytes(payload))
                    if self._on_control_event is not None:
                        self._on_control_event(control)
                elif kind == 2 and self._on_disconnected is not None:
                    self._on_disconnected()
            except Exception as exc:  # transport boundary: report, keep worker alive
                if self._on_error is not None:
                    self._on_error(exc)

    async def _write(self, payload: bytes) -> None:
        if self._closing:
            return
        if not await asyncio.to_thread(self._transport.write, bytes(payload)):
            raise ConnectionError("native WinRT GATT write failed")

    def send_mic_open_threadsafe(self) -> None:
        generation = self._generation

        def schedule() -> None:
            if self._closing or generation != self._generation:
                return
            task = self._loop.create_task(self._write(self._session.mic_open_command()))
            self._write_tasks.add(task)

            def done(finished: asyncio.Task) -> None:
                self._write_tasks.discard(finished)
                if finished.cancelled():
                    return
                error = finished.exception()
                if error is not None and self._on_error is not None:
                    self._on_error(error)

            task.add_done_callback(done)

        self._loop.call_soon_threadsafe(schedule)

    async def close(self) -> None:
        self._closing = True
        self._generation += 1
        for task in tuple(self._write_tasks):
            task.cancel()
        if self._write_tasks:
            await asyncio.gather(*tuple(self._write_tasks), return_exceptions=True)
        self._worker_stop.set()
        if self._worker_thread is not None:
            await asyncio.to_thread(self._worker_thread.join, 2.0)
            if self._worker_thread.is_alive():
                raise RuntimeError("native BLE dispatch thread did not stop")
            self._worker_thread = None
        if self._session.mic_open and self._transport.connected:
            try:
                await asyncio.to_thread(
                    self._transport.write, bytes(self._session.mic_close_command())
                )
            except Exception:
                pass
        await asyncio.to_thread(self._transport.disconnect)


def _make_python(**kwargs):
    return py_mod.RC003BleSession(**kwargs)


def _make_native(**kwargs):
    import remotemic_native as rn  # type: ignore[import-not-found]

    if not rn._C_AVAILABLE or rn.WinRTBleTransport is None:
        return py_mod.RC003BleSession(**kwargs)
    return NativeRC003BleSession(**kwargs)


make_ble_session_python = _make_python
make_ble_session_native = _make_native
make_ble_session = choose_implementation(
    "ble_transport",
    python_impl=_make_python,
    native_impl=_make_native,
    side_effect_free=False,
)


__all__ = [
    "NativeRC003BleSession",
    "make_ble_session",
    "make_ble_session_python",
    "make_ble_session_native",
]
