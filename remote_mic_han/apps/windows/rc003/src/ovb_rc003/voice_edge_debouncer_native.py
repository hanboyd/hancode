"""Phase 3 / ADR-0013 §3.2: module-level switch for VoiceEdgeDebouncer.

Exposes ``make_voice_edge_debouncer(release_window_seconds) ->
VoiceEdgeDebouncer`` which dispatches via ``choose_implementation`` to
either:

  * ``python``: ``ovb_rc003.voice_edge_debouncer.VoiceEdgeDebouncer``
    (the pure-Python implementation with a ``threading.Timer`` factory)
  * ``native``: ``remotemic_native._C.VoiceEdgeDebouncer`` (the
    pybind11 binding; the bridge supplies a daemon ``threading.Timer``
    which drives the C++ debouncer's single pending-fire path)
  * ``shadow``: runs both with identical inputs; only allowed when the
    native side uses a no-thread (manual) timer (the unit-test mode).
    The shadow parity test in step 4 uses the python side directly and
    constructs a parallel native side with the no-thread factory.

The default is ``python`` (per migration plan §1 rule 4). Switch via:

    REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER=native
    REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER=shadow
    REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER=python  # default

Both python and native returns share the same Python surface
(``release_window_seconds`` property and ``on_press`` /
``on_release`` / ``shutdown`` methods). The bridge wrapper holds the
TimerFactory plumbing so callers never see it.
"""

from __future__ import annotations

import threading
from typing import Callable

from . import voice_edge_debouncer as py_mod
from ._remotemic_native_runtime import choose_implementation


class _NativeVoiceEdgeDebouncer:
    """Thin shim over ``remotemic_native._C.VoiceEdgeDebouncer`` plus
    the bridge-supplied TimerFactory plumbing so production timing
    matches the python baseline."""

    def __init__(self, release_window_seconds: float) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        self._timers: list[threading.Timer] = []
        self._timers_lock = threading.Lock()
        if not _rn._C_AVAILABLE:
            self._impl = py_mod.VoiceEdgeDebouncer(release_window_seconds)
            self._release_window_seconds = release_window_seconds
            self._is_native = False
            return
        # The C++ binding exposes the debouncer with a no-op timer
        # factory at the seam; we wrap each ``on_release`` handler in
        # a Timer that fires after ``release_window`` and asks the C++
        # object to consume its pending handler.  The handler must not
        # be invoked directly here: doing so would leave the C++ pending
        # state armed and permit a later duplicate fire.
        self._impl = _rn.VoiceEdgeDebouncer(
            int(round(release_window_seconds * 1000))
        )
        self._release_window_seconds = release_window_seconds
        self._is_native = True

    @property
    def release_window_seconds(self) -> float:
        return self._release_window_seconds

    def on_press(self) -> None:
        if not self._is_native:
            self._impl.on_press()
            return
        self._cancel_pending_timers()
        self._impl.on_press()

    def on_release(self, handler: Callable[[], None]) -> None:
        if not self._is_native:
            self._impl.on_release(handler)
            return
        self._cancel_pending_timers()
        captured_timer_holder: list[threading.Timer | None] = [None]

        def _bridge() -> None:
            with self._timers_lock:
                self._timers = [
                    t for t in self._timers
                    if t is not captured_timer_holder[0]
                ]
            self._impl.fire_pending_now_for_test()

        timer = threading.Timer(self._release_window_seconds, _bridge)
        timer.daemon = True
        captured_timer_holder[0] = timer
        with self._timers_lock:
            self._timers.append(timer)
        self._impl.on_release(handler)
        timer.start()

    def shutdown(self) -> None:
        if not self._is_native:
            self._impl.shutdown()
            return
        self._cancel_pending_timers()
        self._impl.shutdown()

    def fire_pending_now_for_test(self) -> bool:
        return bool(self._impl.fire_pending_now_for_test())

    def _cancel_pending_timers(self) -> None:
        with self._timers_lock:
            timers = self._timers
            self._timers = []
        for timer in timers:
            timer.cancel()


def _make_voice_edge_debouncer_python(
    release_window_seconds: float,
) -> py_mod.VoiceEdgeDebouncer:
    return py_mod.VoiceEdgeDebouncer(release_window_seconds)


def _make_voice_edge_debouncer_native(
    release_window_seconds: float,
) -> _NativeVoiceEdgeDebouncer:
    return _NativeVoiceEdgeDebouncer(release_window_seconds)


make_voice_edge_debouncer_python = _make_voice_edge_debouncer_python
make_voice_edge_debouncer_native = _make_voice_edge_debouncer_native


make_voice_edge_debouncer = choose_implementation(
    "voice_edge_debouncer",
    python_impl=_make_voice_edge_debouncer_python,
    native_impl=_make_voice_edge_debouncer_native,
    side_effect_free=False,
)


__all__ = [
    "make_voice_edge_debouncer",
    "make_voice_edge_debouncer_python",
    "make_voice_edge_debouncer_native",
]
