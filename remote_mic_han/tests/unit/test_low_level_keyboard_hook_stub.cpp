// Phase 5 / ADR-0015 §8 / step 1: LowLevelKeyboardHook stub red-state
// tests. The hook returns false from start() and emits no events
// until step 2 lands the real WH_KEYBOARD_LL implementation.

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <remotemic/input/low_level_keyboard_hook.hpp>

using remotemic::input::LowLevelKeyboardHook;

namespace {

bool test_hook_refuses_start_until_step_2() {
    LowLevelKeyboardHook hook;
    bool ok = hook.start();
    assert(!ok);
    hook.stop();
    return true;
}

bool test_hook_dropped_and_event_counts_start_at_zero() {
    LowLevelKeyboardHook hook;
    assert(hook.event_count() == 0);
    assert(hook.dropped_count() == 0);
    assert(hook.slow_callback_count() == 0);
    return true;
}

bool test_hook_accepts_sink_registration_before_start() {
    LowLevelKeyboardHook hook;
    // Registration itself never fails; the failure surfaces at start().
    hook.set_event_sink(nullptr, nullptr);
    assert(hook.start() == false);
    return true;
}

} // namespace

int main() {
    struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"hook_refuses_start_until_step_2",
         &test_hook_refuses_start_until_step_2},
        {"hook_dropped_and_event_counts_start_at_zero",
         &test_hook_dropped_and_event_counts_start_at_zero},
        {"hook_accepts_sink_registration_before_start",
         &test_hook_accepts_sink_registration_before_start},
    };

    int failures = 0;
    for (const auto& c : cases) {
        bool ok = c.fn();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failures;
    }
    if (failures != 0) {
        std::printf("test_low_level_keyboard_hook_stub: %d/%zu failed\n",
                    failures, sizeof(cases) / sizeof(cases[0]));
        return 1;
    }
    std::printf("test_low_level_keyboard_hook_stub: all PASS\n");
    return 0;
}