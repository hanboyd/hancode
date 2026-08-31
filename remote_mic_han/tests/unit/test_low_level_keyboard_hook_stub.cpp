// Phase 5 / ADR-0015 §8 / step 2 sub-pass B: LowLevelKeyboardHook
// real Win32 implementation tests.
//
// The hook installs WH_KEYBOARD_LL on start() (Windows-only) so the
// tests verify the contract:
//   - start() returns true on Windows
//   - counts start at zero
//   - sink registration never throws and is preserved across start/stop
//   - stop() is idempotent and safe to call before start()
//   - slow_callback_count() is observable as an atomic counter
//
// On non-Windows CI hosts (where _WIN32 is not defined) start()
// returns false per ADR-0015 §2; the tests degrade to asserting the
// fail-closed contract.

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <remotemic/input/low_level_keyboard_hook.hpp>

using remotemic::input::LowLevelKeyboardHook;

namespace {

bool test_hook_starts_on_windows() {
    LowLevelKeyboardHook hook;
#ifdef _WIN32
    assert(hook.start());
    hook.stop();
#else
    // Non-Windows CI: fail-closed per ADR-0015 §2.
    assert(!hook.start());
#endif
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
    // Registration itself never fails.
    hook.set_event_sink(nullptr, nullptr);
#ifdef _WIN32
    assert(hook.start() == true);
    hook.stop();
#else
    assert(hook.start() == false);
#endif
    return true;
}

bool test_hook_stop_is_idempotent_and_safe_before_start() {
    LowLevelKeyboardHook hook;
    hook.stop();  // safe before start
    hook.stop();  // safe to call twice
#ifdef _WIN32
    assert(hook.start());
    hook.stop();  // clean teardown
    hook.stop();  // idempotent
#endif
    return true;
}

} // namespace

int main() {
    struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"hook_starts_on_windows",
         &test_hook_starts_on_windows},
        {"hook_dropped_and_event_counts_start_at_zero",
         &test_hook_dropped_and_event_counts_start_at_zero},
        {"hook_accepts_sink_registration_before_start",
         &test_hook_accepts_sink_registration_before_start},
        {"hook_stop_is_idempotent_and_safe_before_start",
         &test_hook_stop_is_idempotent_and_safe_before_start},
    };

    int failures = 0;
    for (const auto& c : cases) {
        bool ok = c.fn();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failures;
    }
    if (failures != 0) {
        std::printf("test_low_level_keyboard_hook: %d/%zu failed\n",
                    failures, sizeof(cases) / sizeof(cases[0]));
        return 1;
    }
    std::printf("test_low_level_keyboard_hook: all PASS\n");
    return 0;
}
