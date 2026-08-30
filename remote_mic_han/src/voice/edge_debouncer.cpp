// Phase 3 / ADR-0013 §3.2: VoiceEdgeDebouncer — real implementation.
//
// Replaces the step-1 stub. Behavior matches
// apps/windows/rc003/src/ovb_rc003/voice_edge_debouncer.py:48-149,
// with the same invariant on a monotonic ``release_seq`` invalidating
// in-flight firings after a newer release or ``shutdown``. The
// production timer is an injected ``TimerFactory`` so the worker
// thread in app.py can plug a ``std::thread``-backed timer without
// recompiling the debouncer itself; tests plug a manual timer.

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
    std::lock_guard<std::mutex> lock(lock_);
    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }
    ++release_seq_;
    pending_handler_ = nullptr;
    pending_seq_ = 0;
}

void VoiceEdgeDebouncer::on_release(std::function<void()> handler) noexcept {
    std::lock_guard<std::mutex> lock(lock_);
    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }
    ++release_seq_;
    pending_handler_ = std::move(handler);
    pending_seq_ = release_seq_;
    timer_ = factory_(release_window_, [this]() { _run_handler(); });
    if (timer_) {
        timer_ = std::move(timer_);
    }
}

void VoiceEdgeDebouncer::shutdown() noexcept {
    std::lock_guard<std::mutex> lock(lock_);
    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }
    ++release_seq_;
    pending_handler_ = nullptr;
    pending_seq_ = 0;
}

bool VoiceEdgeDebouncer::fire_pending_now_for_test() noexcept {
    std::function<void()> handler;
    {
        std::lock_guard<std::mutex> lock(lock_);
        if (!pending_handler_ || !timer_) {
            return false;
        }
        timer_->cancel();
        timer_.reset();
        handler = std::move(pending_handler_);
        pending_handler_ = nullptr;
    }
    handler();
    return true;
}

void VoiceEdgeDebouncer::_run_handler() noexcept {
    std::function<void()> handler;
    {
        std::lock_guard<std::mutex> lock(lock_);
        timer_.reset();
        if (!pending_handler_) {
            return;
        }
        const std::uint64_t seq = pending_seq_;
        if (seq != release_seq_) {
            // A newer release or shutdown invalidated this firing.
            pending_handler_ = nullptr;
            return;
        }
        handler = std::move(pending_handler_);
        pending_handler_ = nullptr;
    }
    handler();
}

}  // namespace remotemic::voice
