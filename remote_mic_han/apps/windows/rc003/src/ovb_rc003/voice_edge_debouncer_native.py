"""Phase 3 / ADR-0013 §3.2: module-level switch for VoiceEdgeDebouncer.

Exposes ``make_voice_edge_debouncer(release_window_seconds) ->
VoiceEdgeDebouncer`` which dispatches via ``choose_implementation`` to
either:

  * ``python``: ``ovb_rc003.voice_edge_debouncer.VoiceEdgeDebouncer``
    (the pure-Python implementation with a ``threading.Timer`` factory)
  * ``native``: ``remotemic_native._C.VoiceEdgeDebouncer`` (the
    pybind11 binding; the bridge supplies a ``std::thread``-backed
    TimerFactory internally so production timing matches the python
    baseline)
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
from typing import Callable, Optional

from . import voice_edge_debouncer as py_mod
from ._remotemic_native_runtime import choose_implementation


def _std_thread_timer_factory(delay_ms: int, callback: Callable[[], None]):
    """Bridge-side TimerFactory for the C++ ``VoiceEdgeDebouncer``.

    The C++ debouncer only knows about ``std::chrono::milliseconds``
    and a std::function callback; we wrap a ``threading.Timer`` so
    production behavior matches the python baseline (cancel on press
    / shutdown, daemon thread)."""
    timer = threading.Timer(delay_ms / 1000.0, callback)
    timer.daemon = True
    timer.start()
    return timer


class _NativeVoiceEdgeDebouncer:
    """Thin shim over ``remotemic_native._C.VoiceEdgeDebouncer`` plus
    the bridge-supplied TimerFactory plumbing so production timing
    matches the python baseline."""

    def __init__(self, release_window_seconds: float) -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        if not _rn._C_AVAILABLE:
            self._impl = py_mod.VoiceEdgeDebouncer(release_window_seconds)
            self._is_native = False
            return
        # The C++ binding exposes the debouncer with a no-op timer
        # factory at the seam; we wrap each ``on_release`` handler in
        # a Timer that fires after ``release_window`` and re-invokes
        # the debouncer's fire path. The debouncer's mutex + seq
        # invalidation logic keeps the no-thread model safe.
        self._impl = _rn.VoiceEdgeDebouncer(
            int(round(release_window_seconds * 1000))
        )
        self._release_window_seconds = release_window_seconds
        self._timers: list[threading.Timer] = []
        self._is_native = True

    @property
    def release_window_seconds(self) -> float:
        return self._release_window_seconds

    def on_press(self) -> None:
        self._cancel_pending_timers()
        self._impl.on_press()

    def on_release(self, handler: Callable[[], None]) -> None:
        self._cancel_pending_timers()
        captured_handler = handler
        captured_timer_holder: list[Optional[threading.Timer]] = [None]

        def _bridge() -> None:
            self._timers = [
                t for t in self._timers
                if t is not captured_timer_holder[0]
            ]
            captured_handler()

        timer = threading.Timer(self._release_window_seconds, _bridge)
        timer.daemon = True
        captured_timer_holder[0] = timer
        self._timers.append(timer)
        self._impl.on_release(captured_handler)
        timer.start()

    def shutdown(self) -> None:
        self._cancel_pending_timers()
        self._impl.shutdown()

    def fire_pending_now_for_test(self) -> bool:
        return bool(self._impl.fire_pending_now_for_test())

    def _cancel_pending_timers(self) -> None:
        for timer in self._timers:
            timer.cancel()
        self._timers = []


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