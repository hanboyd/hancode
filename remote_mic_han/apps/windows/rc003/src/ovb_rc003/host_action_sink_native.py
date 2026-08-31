"""Phase 5 / ADR-0015 §6 step 3: module-level switch for IHostActionSink.

Exposes ``make_host_action_sink() -> IHostActionSink`` which dispatches
via ``choose_implementation`` to either:

  * ``python``: a thin shim over
    ``ovb_rc003.win32_input.send_keys`` (the pure-Python production
    path today). The shim implements the same IHostActionSink Python
    surface (submit_key / submit_system_action / cancel_pending /
    start / stop) so the bridge can stay implementation-agnostic.

  * ``native``: ``remotemic_native._C.SendInputActionSink`` (the
    pybind11 binding; the C++ side owns user32.SendInput + the worker
    thread + physical scan-code modifiers per ADR-0015 §3.7).
    Single-owner rule per plan §3 rule 5.

The default is ``python`` (per migration plan §1 rule 4). Switch via:

    REMOTEMIC_NATIVE_CHOICE_HOST_ACTION_SINK=native
    REMOTEMIC_NATIVE_CHOICE_HOST_ACTION_SINK=python   # default

``shadow`` is NOT supported here per plan §3 rule 5: ``SendInput``
is side-effecting (real key events dispatched to the foreground app),
so a python shadow would actually double-physicalize every key event.
Parity tests (step 4) use ``FakeHostActionSink`` to drive both python
and native through an in-process recording harness, which has no
side effects.
"""

from __future__ import annotations

import enum
from typing import Optional

from . import win32_input as py_win32_input
from ._remotemic_native_runtime import choose_implementation


# The python side already names SystemAction values via string tokens
# in ``win32_input``; mirror that mapping here so submit_system_action
# works without a separate import on the python side.
class _PythonSystemAction(enum.IntEnum):
    VolumeUp = 0
    VolumeDown = 1
    VolumeMute = 2
    ShowDesktop = 3
    Escape = 4
    Return = 5
    Backspace = 6
    ContextMenu = 7
    AppSwitch = 8
    CodexOpen = 9


# ``win32_input.send_keys`` accepts vk+down tuples; map SystemAction
# values to the existing python-side helpers so the python shim stays
# thin.
_SYSTEM_ACTION_DISPATCH = {
    _PythonSystemAction.VolumeUp: "volume_up",
    _PythonSystemAction.VolumeDown: "volume_down",
    _PythonSystemAction.VolumeMute: "volume_mute",
    _PythonSystemAction.ShowDesktop: "show_desktop",
    _PythonSystemAction.Escape: "escape",
    _PythonSystemAction.Return: "return_key",
    _PythonSystemAction.Backspace: "backspace",
    _PythonSystemAction.ContextMenu: "context_menu",
    _PythonSystemAction.AppSwitch: "app_switch",
    _PythonSystemAction.CodexOpen: "codex_open",
}


class _PythonHostActionSink:
    """Thin shim over ``ovb_rc003.win32_input`` that exposes the same
    Python surface (``submit_key`` / ``submit_system_action`` /
    ``cancel_pending`` / ``start`` / ``stop``) so the bridge wrapper
    can stay implementation-agnostic.

    The shim translates:

      * submit_key(vk, down, deadline)   -> win32_input.send_keys
      * submit_system_action(action)    -> win32_input.<action-name>
      * cancel_pending()                -> no-op (python side is synchronous)
      * start()                         -> no-op (python side has no
                                            worker thread; ``start`` is a
                                            marker for parity)
      * stop()                          -> no-op
    """

    def __init__(self) -> None:
        self._started = False
        self._is_native = False
        self._submitted_count = 0
        self._submit_error_count = 0

    def submit_key(
        self, vk_code: int, key_down: bool, deadline: int = 50
    ) -> bool:
        if not self._started:
            self._submit_error_count += 1
            return False
        try:
            # win32_input.send_keys accepts a list of (vk, down) tuples.
            py_win32_input.send_keys([(int(vk_code), bool(key_down))])
            self._submitted_count += 1
            return True
        except Exception:
            self._submit_error_count += 1
            return False

    def submit_system_action(self, action: object) -> bool:
        if not self._started:
            self._submit_error_count += 1
            return False
        try:
            try:
                action_value = int(action)
            except (TypeError, ValueError):
                # Accept string forms too: "VolumeUp" -> VolumeUp.
                name = str(action).split(".")[-1]
                action_value = int(_PythonSystemAction[name])
            try:
                py_action = _PythonSystemAction(action_value)
            except ValueError:
                self._submit_error_count += 1
                return False
            method_name = _SYSTEM_ACTION_DISPATCH.get(py_action)
            if method_name is None:
                self._submit_error_count += 1
                return False
            method = getattr(py_win32_input, method_name, None)
            if not callable(method):
                self._submit_error_count += 1
                return False
            method()
            self._submitted_count += 1
            return True
        except Exception:
            self._submit_error_count += 1
            return False

    def cancel_pending(self) -> None:
        # python side is synchronous; nothing to cancel.
        return None

    def start(self) -> bool:
        self._started = True
        return True

    def stop(self) -> None:
        self._started = False

    def submitted_count(self) -> int:
        return self._submitted_count

    def submit_error_count(self) -> int:
        return self._submit_error_count


class _NativeHostActionSink:
    """Thin shim over ``remotemic_native._C.SendInputActionSink`` that
    exposes the same Python surface (``submit_key`` /
    ``submit_system_action`` / ``cancel_pending`` / ``start`` /
    ``stop``) so the bridge wrapper can stay implementation-agnostic.

    Native semantics map 1:1 onto the C++ IHostActionSink:

      * submit_key(vk, down, deadline) -> SendInputActionSink.submit_key
      * submit_system_action(action)   -> SendInputActionSink.submit_system_action
      * cancel_pending()               -> SendInputActionSink.cancel_pending
      * start()                        -> SendInputActionSink.start (Windows-only)
      * stop()                         -> SendInputActionSink.stop

    Defensive construction: ``SendInputActionSink`` is only registered
    when the binding is compiled with ``_WIN32`` defined (per
    ``bind_module.cpp`` section 13). On a Linux/macOS build the symbol
    is absent; we fall back to the python shim so the bridge wrapper
    stays implementation-agnostic across all platforms.
    """

    def __init__(self) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        self._is_native = False

        # Two graceful fallbacks before falling through to python:
        # 1. binding absent (no _C.pyd on PYTHONPATH)               -> python
        # 2. binding present but SendInputActionSink symbol missing
        #    (Linux/macOS: Win32 adapter behind #ifdef _WIN32)      -> python
        # 3. binding present + symbol present (Windows build)       -> native
        if not getattr(_rn, "_C_AVAILABLE", False):
            self._impl: object = _PythonHostActionSink()
            return
        send_input_cls = getattr(_rn, "SendInputActionSink", None)
        if send_input_cls is None:
            self._impl = _PythonHostActionSink()
            return
        self._impl = send_input_cls()
        self._is_native = True

    def submit_key(
        self, vk_code: int, key_down: bool, deadline: int = 50
    ) -> bool:
        import datetime as _dt

        submit_fn = getattr(self._impl, "submit_key", None)
        if not callable(submit_fn):
            return False
        ms = (
            int(deadline)
            if isinstance(deadline, (int, float))
            else int(deadline.total_seconds() * 1000)
        )
        try:
            return bool(
                submit_fn(int(vk_code), bool(key_down),
                          _dt.timedelta(milliseconds=ms))
            )
        except Exception:
            return False

    def submit_system_action(self, action: object) -> bool:
        submit_fn = getattr(self._impl, "submit_system_action", None)
        if not callable(submit_fn):
            return False
        try:
            try:
                action_int = int(action)
            except (TypeError, ValueError):
                # Accept string forms too: "VolumeUp" -> VolumeUp.
                name = str(action).split(".")[-1]
                action_int = int(_PythonSystemAction[name])
            return bool(submit_fn(action_int))
        except (KeyError, ValueError):
            return False
        except Exception:
            return False

    def cancel_pending(self) -> None:
        cancel_fn = getattr(self._impl, "cancel_pending", None)
        if callable(cancel_fn):
            cancel_fn()

    def start(self) -> bool:
        start_fn = getattr(self._impl, "start", None)
        if not callable(start_fn):
            return False
        return bool(start_fn())

    def stop(self) -> None:
        stop_fn = getattr(self._impl, "stop", None)
        if callable(stop_fn):
            stop_fn()

    def submitted_count(self) -> int:
        count_fn = getattr(self._impl, "submitted_count", None)
        if not callable(count_fn):
            return 0
        try:
            return int(count_fn())
        except Exception:
            return 0

    def submit_error_count(self) -> int:
        count_fn = getattr(self._impl, "submit_error_count", None)
        if not callable(count_fn):
            return 0
        try:
            return int(count_fn())
        except Exception:
            return 0


def _make_host_action_sink_python() -> _PythonHostActionSink:
    sink = _PythonHostActionSink()
    sink.start()
    return sink


def _make_host_action_sink_native() -> _NativeHostActionSink:
    sink = _NativeHostActionSink()
    sink.start()
    return sink


make_host_action_sink_python = _make_host_action_sink_python
make_host_action_sink_native = _make_host_action_sink_native


# Module-level binding (Phase 3 / ADR-0011 pattern): the env var is
# captured AT IMPORT TIME so the chosen factory is fixed for the
# lifetime of the process. Production pattern: launch
# ``python -m ovb_rc003`` AFTER exporting the env var, exactly the
# same as Phase 3's ``make_voice_controller`` /
# ``make_atvv_session`` binding and Phase 4's ``make_audio_route``.
# Tests use ``importlib.reload`` to re-capture after mutating the
# env var mid-process.
make_host_action_sink = choose_implementation(
    "host_action_sink",
    python_impl=_make_host_action_sink_python,
    native_impl=_make_host_action_sink_native,
    # shadow is forbidden per plan §3 rule 5 (real SendInput
    # dispatch), so side_effect_free stays False.
    # ``choose_implementation`` will reject any attempt to dispatch
    # into shadow mode at runtime.
    side_effect_free=False,
)


__all__ = [
    "make_host_action_sink",
    "make_host_action_sink_python",
    "make_host_action_sink_native",
    "_PythonHostActionSink",
    "_NativeHostActionSink",
]