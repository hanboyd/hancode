// Phase 3 / ADR-0013 §3.2: VoiceEdgeDebouncer stub.
//
// Step 1: every release handler is dropped on the floor. Step 2 wires
// the timer factory and pending-handler bookkeeping. Production timer
// factory is provided in a separate translation unit so step 2 can
// switch between the production (std::thread-backed) timer and a
// stub without recompiling the debouncer itself.

#include "remotemic/voice/edge_debouncer.hpp"

#include <stdexcept>

namespace remotemic::voice {

namespace {

std::chrono::milliseconds monotonic_clock_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

class NoopTimer final : public TimerHandle {
public:
    void cancel() noexcept override {}
};

std::unique_ptr<TimerHandle> noop_timer_factory(
    std::chrono::milliseconds /*delay*/,
    std::function<void()> /*handler*/) {
    return std::make_unique<NoopTimer>();
}

}  // namespace

VoiceEdgeDebouncer::VoiceEdgeDebouncer(
    std::chrono::milliseconds release_window,
    TimerFactory factory,
    ClockFn clock)
    : release_window_(release_window),
      factory_(std::move(factory)),
      clock_(std::move(clock)) {
    if (release_window_ < std::chrono::milliseconds(50) ||
        release_window_ > std::chrono::milliseconds(500)) {
        throw std::invalid_argument(
            "VoiceEdgeDebouncer release_window must be in [50ms, 500ms]");
    }
}

VoiceEdgeDebouncer::VoiceEdgeDebouncer(
    std::chrono::milliseconds release_window,
    TimerFactory factory)
    : VoiceEdgeDebouncer(release_window, std::move(factory),
                         &monotonic_clock_now) {}

VoiceEdgeDebouncer::VoiceEdgeDebouncer(
    std::chrono::milliseconds release_window)
    : VoiceEdgeDebouncer(release_window, &noop_timer_factory,
                         &monotonic_clock_now) {}

void VoiceEdgeDebouncer::on_press() noexcept {
    // STUB: drops the pending release. Step 2 cancels the underlying
    // timer and bumps ``release_seq_`` to invalidate any in-flight
    // firing.
}

void VoiceEdgeDebouncer::on_release(std::function<void()> /*handler*/) noexcept {
    // STUB: nothing scheduled.
}

void VoiceEdgeDebouncer::shutdown() noexcept {
    // STUB: nothing to cancel.
}

bool VoiceEdgeDebouncer::fire_pending_now_for_test() noexcept {
    return false;
}

}  // namespace remotemic::voice
