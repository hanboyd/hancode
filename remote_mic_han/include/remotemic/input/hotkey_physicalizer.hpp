#pragma once

#include <cstdint>
#include <vector>

#include <remotemic/input/i_host_action_sink.hpp>
#include <remotemic/input/input_event.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §4: HotkeyPhysicalizer turns a named voice
// hotkey (``"ralt"``, ``"lctrl+lalt"``) into a sequence of VK
// down/up events submitted through an IHostActionSink. The Qianwen /
// Typeless physicalize path lives here; the core state machine does
// not inspect process names or window titles. Step 1 stub records
// nothing and returns false.
class HotkeyPhysicalizer {
public:
    explicit HotkeyPhysicalizer(IHostActionSink& sink) noexcept;

    // Resolve ``tokens`` (slash-separated chord names) and submit the
    // resulting VK sequence. Returns false on unknown token / sink-down.
    bool physicalize(const char* tokens) noexcept;

    // Re-release any keys this physicalizer currently holds down.
    // Used at voice-session transition + at shutdown.
    void release_held() noexcept;

private:
    IHostActionSink& sink_;
    std::uint64_t physicalized_count_{0};
    std::uint64_t physicalize_error_count_{0};
    std::vector<std::uint16_t> held_keys_;
};

} // namespace remotemic::input