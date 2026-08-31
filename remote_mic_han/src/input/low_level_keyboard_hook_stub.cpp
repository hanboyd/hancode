// Phase 5 / ADR-0015 step 1 stub: LowLevelKeyboardHook returns false
// from start() so red-state tests can fail without touching Win32
// APIs. Step 2 lands the real implementation behind the same header.

#include <remotemic/input/low_level_keyboard_hook.hpp>

namespace remotemic::input {

void LowLevelKeyboardHook::set_event_sink(SinkFn sink, void* user_data) noexcept {
    sink_ = sink;
    user_data_ = user_data;
}

bool LowLevelKeyboardHook::start() noexcept {
    // Stub: refuse to start so any caller exercising the production
    // path through this stub falls back to the python baseline. Step 2
    // replaces with the real WH_KEYBOARD_LL install.
    started_ = false;
    return false;
}

void LowLevelKeyboardHook::stop() noexcept {
    started_ = false;
}

std::uint64_t LowLevelKeyboardHook::dropped_count() const noexcept {
    return dropped_count_;
}

std::uint64_t LowLevelKeyboardHook::event_count() const noexcept {
    return event_count_;
}

std::uint64_t LowLevelKeyboardHook::slow_callback_count() const noexcept {
    return slow_callback_count_;
}

} // namespace remotemic::input