#pragma once

#include <chrono>
#include <cstdint>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.1: InputEvent value type. Plain-old-data-ish;
// carries no allocation. Crosses the hook callback boundary by value
// into a lock-free SPSC queue.
struct InputEvent {
    enum class SourceKind : std::uint8_t {
        RawInputKeyboard = 0,
        RawInputHid      = 1,
        FridaHidTap      = 2,
        LowLevelHook     = 3,
        Synthetic        = 4,
    };

    enum class EventKind : std::uint8_t {
        KeyDown       = 0,
        KeyUp         = 1,
        KeyCancel     = 2,  // LL hook cancellation (rare)
        SystemAction  = 3,  // volume / power / showDesktop / openCodex
    };

    std::chrono::steady_clock::time_point timestamp{};
    SourceKind  source{SourceKind::RawInputKeyboard};
    EventKind   kind{EventKind::KeyDown};
    std::uint16_t vk_code{0};
    std::uint16_t scan_code{0};
    std::uint32_t usage_id{0};
    std::uint32_t extra_info{0};   // KBDLLHOOKSTRUCT dwExtraInfo (LL hook only)
    bool         injected{false};   // KBDLLHOOKSTRUCT flags LLKHF_INJECTED
    bool         extended{false};   // KBDLLHOOKSTRUCT flags LLKHF_EXTENDED
};

// Phase 5 / ADR-0015 §3.5: semantic system actions (Windows-side shell
// effects that the user binds via ``key_bindings.json``). The C++ core
// state machine does NOT inspect process names or window titles; only
// this enum reaches the action sink. New entries must be appended (no
// reordering) so user keymap files stay binary-compatible.
enum class SystemAction : std::uint8_t {
    VolumeUp      = 0,
    VolumeDown    = 1,
    VolumeMute    = 2,
    ShowDesktop   = 3,
    Escape        = 4,
    Return        = 5,
    Backspace     = 6,
    ContextMenu   = 7,
    AppSwitch     = 8,
    CodexOpen     = 9,
};

} // namespace remotemic::input