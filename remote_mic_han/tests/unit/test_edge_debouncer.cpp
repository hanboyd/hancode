// Phase 3 / ADR-0013 §3.2: VoiceEdgeDebouncer TDD red-state unit tests.
//
// The stub drops every release handler on the floor and never fires.
// Step 2 wires the timer factory + pending-handler bookkeeping and
// these tests turn green.

#include "remotemic/voice/edge_debouncer.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

namespace {

using std::chrono::milliseconds;
using remotemic::voice::TimerFactory;
using remotemic::voice::TimerHandle;
using remotemic::voice::VoiceEdgeDebouncer;

int failures = 0;
int fired = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

// A test-only timer that exposes its handler and a cancel flag so the
// test can fire / cancel without ever sleeping or starting a thread.
class ManualTimer final : public TimerHandle {
public:
    explicit ManualTimer(std::function<void()> handler)
        : handler_(std::move(handler)) {}
    void cancel() noexcept override;
    std::function<void()> handler_;
};

bool timer_created = false;
bool last_timer_cancelled = false;

void ManualTimer::cancel() noexcept { last_timer_cancelled = true; }

std::unique_ptr<TimerHandle> manual_factory(
    milliseconds /*delay*/,
    std::function<void()> handler) {
    auto t = std::make_unique<ManualTimer>(std::move(handler));
    timer_created = true;
    return t;
}

void reset_test_state() {
    fired = 0;
    timer_created = false;
    last_timer_cancelled = false;
}

void handler_a() { ++fired; }

void test_release_window_clamps_to_50_to_500_ms() {
    bool threw_below = false;
    try {
        VoiceEdgeDebouncer d(milliseconds(10));
    } catch (const std::invalid_argument&) {
        threw_below = true;
    }
    expect(threw_below, "release_window = 10 ms throws (below 50 ms)");

    bool threw_above = false;
    try {
        VoiceEdgeDebouncer d(milliseconds(1000));
    } catch (const std::invalid_argument&) {
        threw_above = true;
    }
    expect(threw_above, "release_window = 1000 ms throws (above 500 ms)");

    bool constructed_ok = false;
    try {
        VoiceEdgeDebouncer d(milliseconds(200));
        constructed_ok = true;
    } catch (...) {
    }
    expect(constructed_ok, "release_window = 200 ms constructs cleanly");

    bool edge_low = false;
    try {
        VoiceEdgeDebouncer d(milliseconds(50));
        edge_low = true;
    } catch (...) {
    }
    expect(edge_low, "release_window = 50 ms constructs (lower bound)");

    bool edge_high = false;
    try {
        VoiceEdgeDebouncer d(milliseconds(500));
        edge_high = true;
    } catch (...) {
    }
    expect(edge_high, "release_window = 500 ms constructs (upper bound)");
}

void test_release_then_press_cancels_pending_timer() {
    reset_test_state();
    VoiceEdgeDebouncer d(milliseconds(200), &manual_factory);
    d.on_release(&handler_a);
    d.on_press();  // cancels the pending release

    expect(timer_created && last_timer_cancelled,
           "release then press: pending timer is cancelled");
}

void test_release_then_fire_runs_handler_exactly_once() {
    reset_test_state();
    VoiceEdgeDebouncer d(milliseconds(200), &manual_factory);
    d.on_release(&handler_a);

    bool fired_now = d.fire_pending_now_for_test();
    expect(fired_now,
           "fire_pending_now_for_test returns true after release");
    expect(fired == 1,
           "handler ran exactly once after fire_pending_now_for_test");

    bool fired_again = d.fire_pending_now_for_test();
    expect(!fired_again,
           "fire_pending_now_for_test returns false on second call");
    expect(fired == 1,
           "handler still ran exactly once after second fire");
}

void test_shutdown_cancels_pending_release() {
    reset_test_state();
    VoiceEdgeDebouncer d(milliseconds(200), &manual_factory);
    d.on_release(&handler_a);
    d.shutdown();

    expect(timer_created && last_timer_cancelled,
           "shutdown cancels the pending timer");
    bool fired_now = d.fire_pending_now_for_test();
    expect(!fired_now,
           "fire_pending_now_for_test returns false after shutdown");
    expect(fired == 0, "shutdown prevents the handler from running");
}

void test_query_returns_configured_window() {
    VoiceEdgeDebouncer d(milliseconds(200), &manual_factory);
    expect(d.release_window() == milliseconds(200),
           "release_window() returns the value passed to the constructor");
}

}  // namespace

int main() {
    test_release_window_clamps_to_50_to_500_ms();
    test_release_then_press_cancels_pending_timer();
    test_release_then_fire_runs_handler_exactly_once();
    test_shutdown_cancels_pending_release();
    test_query_returns_configured_window();

    if (failures != 0) {
        std::cerr << "VoiceEdgeDebouncer tests: " << failures
                  << " failure(s) (red state on stub; step 2 turns green)\n";
        return 1;
    }
    std::cout << "All VoiceEdgeDebouncer tests passed\n";
    return 0;
}
