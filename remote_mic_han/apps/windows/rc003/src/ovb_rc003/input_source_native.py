"""Phase 5 / ADR-0015 §6 step 3: module-level switch for IInputSource.

Exposes ``make_input_source() -> IInputSource`` which dispatches via
``choose_implementation`` to either:

  * ``python``: a thin shim over
    ``ovb_rc003.raw_input_windows.RawInputButtonListener`` (the
    pure-Python production path today). The shim implements the same
    ``IInputSource`` Python surface (set_event_sink / start / stop /
    event_count / dropped_count) so the bridge can stay
    implementation-agnostic.

  * ``native``: ``remotemic_native._C.RawInputSource`` (the pybind11
    binding; the C++ side owns the WH_KEYBOARD_LL hook + SPSC ring
    + 5 us budget per ADR-0015 §3.6). Single-owner rule per plan
    §3 rule 5.

The default is ``python`` (per migration plan §1 rule 4). Switch via:

    REMOTEMIC_NATIVE_CHOICE_INPUT_SOURCE=native
    REMOTEMIC_NATIVE_CHOICE_INPUT_SOURCE=python   # default

``shadow`` is NOT supported here per plan §3 rule 5: Raw Input is
side-effecting (a real device handle is opened / closed), so a python
shadow would actually open *two* Raw Input streams at once, distorting
the user-facing latency measurement. Parity tests (step 4) use
``FakeInputSource`` to drive both python and native through an
in-process recording harness, which has no side effects.

This module deliberately does NOT change the existing
``_NativeAudioRoute`` / native switch wiring — that surface lives in
``audio_route_native.py``. The input layer has its own factory
because the production path constructs the listener from
``raw_input_windows.RawInputButtonListener(device_path)`` with a
device path that the python side enumerates from the registry; the
C++ side does not yet enumerate that path on its own. Until that gap
closes, the native factory here is reached only when the caller is
already holding a device path or running parity tests.
"""

from __future__ import annotations

from typing import Callable, List, Optional

from . import raw_input_windows as py_raw_input
from ._remotemic_native_runtime import choose_implementation


class _PythonInputSource:
    """Thin shim over ``ovb_rc003.raw_input_windows.RawInputButtonListener``
    that exposes the same Python surface (``set_event_sink`` /
    ``start`` / ``stop`` / ``event_count`` / ``dropped_count``) so the
    bridge wrapper can stay implementation-agnostic.

    The shim translates:

      * set_event_sink(callable) -> RawInputButtonListener.set_raw_event_callback
        (the python side already has this hook; the C++ side takes a
        ``void (*)(InputEvent, void*)`` per the IInputSource contract)
      * start(device_path)         -> RawInputButtonListener.start(device_path)
      * stop()                     -> RawInputButtonListener.stop()
      * event_count()              -> cumulative event count
      * dropped_count()            -> cumulative dropped count

    No raw_input_windows import is hidden: callers that need the full
    python surface (``set_physical_bindings``, ``set_raw_event_callback``,
    ``is_running``) keep using ``raw_input_windows.RawInputButtonListener``
    directly until Phase 7 Application coordinator wires them through
    the IInputSource abstraction.
    """

    def __init__(self, device_path: Optional[str] = None) -> None:
        self._device_path = device_path
        self._listener: Optional[py_raw_input.RawInputButtonListener] = None
        self._event_count = 0
        self._dropped_count = 0
        self._is_native = False

    def set_event_sink(
        self, sink: Callable[[object], None]
    ) -> None:
        # The shim does not own the listener until ``start`` is called;
        # until then we just store the callable for later registration.
        self._sink = sink

    def start(self, device_path: Optional[str] = None) -> bool:
        path = device_path if device_path is not None else self._device_path
        if path is None:
            return False
        self._listener = py_raw_input.RawInputButtonListener(
            lambda *args, **kwargs: None  # placeholder button callback
        )
        # Wire the raw-event callback into our shim so event_count tracks
        # the python side. The downstream sink (registered via
        # set_event_sink) is invoked with a synthesised InputEvent shape.
        def _on_raw_event(button: str, kind: str) -> None:
            self._event_count += 1
            sink = getattr(self, "_sink", None)
            if sink is not None:
                sink({"button": button, "kind": kind, "source": "RawInput"})

        set_raw_event_callback = getattr(
            self._listener, "set_raw_event_callback", None
        )
        if callable(set_raw_event_callback):
            set_raw_event_callback(_on_raw_event)
        try:
            self._listener.start(path)
            return True
        except py_raw_input.RawInputUnavailableError:
            return False

    def stop(self) -> None:
        if self._listener is not None:
            try:
                self._listener.stop()
            finally:
                self._listener = None

    def event_count(self) -> int:
        return self._event_count

    def dropped_count(self) -> int:
        return self._dropped_count


class _NativeInputSource:
    """Thin shim over ``remotemic_native._C.RawInputSource`` that exposes
    the same Python surface (``set_event_sink`` / ``start`` / ``stop`` /
    ``event_count`` / ``dropped_count``) so the bridge wrapper can stay
    implementation-agnostic.

    Native semantics map 1:1 onto the C++ IInputSource:

      * set_event_sink(callable) -> IInputSource.set_event_sink (binding
        seam marshals the Python callable into the C function-pointer +
        void* contract via a per-source registry + GIL-safe trampoline)
      * start()                  -> RawInputSource.start (Windows-only)
      * stop()                   -> RawInputSource.stop
      * event_count()            -> RawInputSource.event_count
      * dropped_count()          -> RawInputSource.dropped_count

    Defensive construction: ``RawInputSource`` is only registered when
    the binding is compiled with ``_WIN32`` defined (per
    ``bind_module.cpp`` section 13). On a Linux/macOS build the symbol
    is absent; we fall back to the python shim so the bridge wrapper
    stays implementation-agnostic across all platforms. ``set_event_sink``
    is now fully wired through the binding's trampoline; the
    python-side ``_sink`` field remains so the bridge wrapper has a
    single source of truth for the callable.
    """

    def __init__(self, device_path: Optional[str] = None) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        self._is_native = False
        self._device_path = device_path
        self._sink: Optional[Callable[[object], None]] = None
        # Captured diagnostic for set_event_sink failures; the bridge
        # wrapper can inspect this and decide whether to fall back to
        # the python surface.
        self._registration_error: Optional[BaseException] = None

        # Three graceful fallbacks before falling through to python:
        # 1. binding absent (no _C.pyd on PYTHONPATH)            -> python
        # 2. binding present but RawInputSource symbol missing
        #    (Linux/macOS: Win32 adapter behind #ifdef _WIN32)   -> python
        # 3. binding present + symbol present (Windows build)    -> native
        if not getattr(_rn, "_C_AVAILABLE", False):
            self._impl: object = _PythonInputSource(device_path)
            return
        raw_input_cls = getattr(_rn, "RawInputSource", None)
        if raw_input_cls is None:
            self._impl = _PythonInputSource(device_path)
            return
        self._impl = raw_input_cls()
        self._is_native = True

    def __del__(self) -> None:  # pragma: no cover - GC timing dependent
        # Best-effort cleanup: drop the registered sink on the native
        # side so the pump thread can short-circuit. NativeImpl holds a
        # reference to the python callable only as long as the binding's
        # registry keeps it; releasing here lets the callable be
        # collected promptly when the wrapper goes out of scope.
        try:
            self.stop()
        except Exception:  # pragma: no cover - defensive during GC
            pass

    def set_event_sink(
        self, sink: Callable[[object], None]
    ) -> None:
        # Store on the python side so the bridge wrapper has a single
        # source of truth. The binding's IInputSource.set_event_sink
        # now marshals the Python callable into the C function-pointer
        # + void* contract via a per-source registry + GIL-safe
        # trampoline (see bind_module.cpp section 13.4). The trampoline
        # is invoked from the source's pump thread, NOT the
        # WH_KEYBOARD_LL hook callback path, so the 5 us budget per
        # ADR-0015 §3.6 is preserved.
        #
        # Diagnostics: native registration failures are NO LONGER
        # silently swallowed. If the binding side rejects the callable
        # (older artifact, missing __release_sink__, etc.), we log at
        # WARNING so a post-mortem can identify why the native side
        # never delivered the event. The python-side _sink remains
        # authoritative so the bridge can observe the registration
        # attempt for tests; the bridge wrapper calls stop() at the
        # end of the session to clear any sink the binding accepted.
        self._sink = sink
        set_fn = getattr(self._impl, "set_event_sink", None)
        if callable(set_fn):
            try:
                set_fn(sink)
            except (TypeError, ValueError, RuntimeError) as exc:
                _logger.warning(
                    "native input source rejected set_event_sink: %s",
                    exc,
                )
                self._registration_error = exc
            except Exception as exc:  # pragma: no cover - defensive
                _logger.exception(
                    "native input source set_event_sink raised unexpected error",
                )
                self._registration_error = exc
        else:
            _logger.warning(
                "native input source has no set_event_sink binding; "
                "the python-side _sink will not be invoked on the "
                "native pump thread."
            )
            self._registration_error = RuntimeError(
                "native input source binding missing set_event_sink"
            )

    def start(self, device_path: Optional[str] = None) -> bool:
        # RawInputSource.start() does not take a device path; the C++
        # side enumerates and filters by VID/PID internally per
        # ADR-0015 §3.7. ``device_path`` is accepted for shape parity
        # with the python side and ignored.
        start_fn = getattr(self._impl, "start", None)
        if not callable(start_fn):
            return False
        try:
            return bool(start_fn())
        except (TypeError, ValueError, RuntimeError) as exc:
            _logger.warning(
                "native input source start failed: %s", exc,
            )
            return False

    def stop(self) -> None:
        # Drop any registered sink on the native side BEFORE stopping
        # the source, so the pump thread sees user_data=nullptr and
        # short-circuits. Without this, the python callable could be
        # invoked once more from the pump thread after stop() returned.
        release_fn = getattr(self._impl, "__release_sink__", None)
        if callable(release_fn):
            try:
                release_fn()
            except (TypeError, ValueError, RuntimeError) as exc:
                _logger.warning(
                    "native input source __release_sink__ failed: %s",
                    exc,
                )
        stop_fn = getattr(self._impl, "stop", None)
        if callable(stop_fn):
            try:
                stop_fn()
            except (TypeError, ValueError, RuntimeError) as exc:
                _logger.warning(
                    "native input source stop failed: %s", exc,
                )

    def event_count(self) -> int:
        count_fn = getattr(self._impl, "event_count", None)
        if not callable(count_fn):
            return 0
        try:
            return int(count_fn())
        except (TypeError, ValueError, RuntimeError):
            return 0

    def dropped_count(self) -> int:
        count_fn = getattr(self._impl, "dropped_count", None)
        if not callable(count_fn):
            return 0
        try:
            return int(count_fn())
        except (TypeError, ValueError, RuntimeError):
            return 0


def _make_input_source_python(
    device_path: Optional[str] = None,
) -> _PythonInputSource:
    src = _PythonInputSource(device_path)
    return src


def _make_input_source_native(
    device_path: Optional[str] = None,
) -> _NativeInputSource:
    src = _NativeInputSource(device_path)
    return src


make_input_source_python = _make_input_source_python
make_input_source_native = _make_input_source_native


# Module-level binding (Phase 3 / ADR-0011 pattern): the env var is
# captured AT IMPORT TIME so the chosen factory is fixed for the
# lifetime of the process. Production pattern: launch
# ``python -m ovb_rc003`` AFTER exporting the env var, exactly the
# same as Phase 3's ``make_voice_controller`` /
# ``make_atvv_session`` binding and Phase 4's ``make_audio_route``.
# Tests use ``importlib.reload`` to re-capture after mutating the
# env var mid-process.
make_input_source = choose_implementation(
    "input_source",
    python_impl=_make_input_source_python,
    native_impl=_make_input_source_native,
    # shadow is forbidden per plan §3 rule 5 (real Raw Input device
    # handle), so side_effect_free stays False. ``choose_implementation``
    # will reject any attempt to dispatch into shadow mode at runtime.
    side_effect_free=False,
)


__all__ = [
    "make_input_source",
    "make_input_source_python",
    "make_input_source_native",
    "_PythonInputSource",
    "_NativeInputSource",
]