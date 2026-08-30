// Phase 3 / ADR-0013 §3.2: VoiceEdgeDebouncer — release-window
// debouncer for the voice microphone key. Pure state machine plus an
// injectable timer factory so tests can run without real threads or
// real sleeps. Time is read through the injected ``ClockFn`` (default
// monotonic).
//
// Stub state: every release handler is dropped on the floor. Step 2
// wires the timer factory and pending-handler bookkeeping; step 3
// surfaces the same behaviour via pybind11.

#ifndef REMOTEMIC_VOICE_EDGE_DEBOUNCER_HPP
#define REMOTEMIC_VOICE_EDGE_DEBOUNCER_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace remotemic::voice {

using ClockFn = std::function<std::chrono::milliseconds()>;

class TimerHandle {
public:
    virtual ~TimerHandle() = default;
    virtual void cancel() noexcept = 0;
};

// Timer factory: create a one-shot timer that calls ``handler`` after
// ``delay``. Implementations are responsible for ensuring the timer can
// be cancelled via ``TimerHandle::cancel()`` and that a cancelled
// handler does NOT run. Production builds plug
// ``std::thread``-backed timers; tests plug a manual clock.
using TimerFactory = std::function<
    std::unique_ptr<TimerHandle>(std::chrono::milliseconds delay,
                                 std::function<void()> handler)>;

class VoiceEdgeDebouncer {
public:
    VoiceEdgeDebouncer(std::chrono::milliseconds release_window,
                       TimerFactory factory,
                       ClockFn clock);

    explicit VoiceEdgeDebouncer(std::chrono::milliseconds release_window,
                                TimerFactory factory);

    explicit VoiceEdgeDebouncer(std::chrono::milliseconds release_window);

    std::chrono::milliseconds release_window() const noexcept {
        return release_window_;
    }

    // Cancel any pending release.
    void on_press() noexcept;

    // Schedule ``handler`` after ``release_window_``. A pending
    // release is cancelled if one is already scheduled. A press
    // arriving inside the window cancels the timer; the handler does
    // not run.
    void on_release(std::function<void()> handler) noexcept;

    // Cancel any pending release so a worker thread can exit cleanly.
    void shutdown() noexcept;

    // Test-only: synchronously fire the pending release handler if
    // any. Returns ``true`` if a handler was fired. Production code
    // MUST NOT call this.
    bool fire_pending_now_for_test() noexcept;

private:
    // Timer-fire handler. Acquires ``lock_``, validates the in-flight
    // ``release_seq`` against the current ``release_seq_``, and runs
    // the pending handler if it still belongs to the most recent
    // release. Lock is released before the handler runs so a long
    // handler can't deadlock against an on_press / shutdown arriving
    // on the worker thread.
    void _run_handler() noexcept;

    std::chrono::milliseconds release_window_;
    TimerFactory factory_;
    ClockFn clock_;
    std::mutex lock_;
    std::unique_ptr<TimerHandle> timer_;
    std::uint64_t release_seq_ = 0;
    std::function<void()> pending_handler_;
    std::uint64_t pending_seq_ = 0;
};

}  // namespace remotemic::voice

#endif  // REMOTEMIC_VOICE_EDGE_DEBOUNCER_HPP
