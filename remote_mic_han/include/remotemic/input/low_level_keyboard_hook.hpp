#pragma once

#include <cstdint>

#include <remotemic/input/i_input_source.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.4: Windows-only low-level keyboard hook. The
// real implementation in step 2 will install ``WH_KEYBOARD_LL``,
// maintain an atomic suppression table, and push events through a
// lock-free SPSC queue. Step 1 ships the stub (returns false from
// start, no hook installed) so red-state tests can fail without
// touching Win32 APIs.
//
// Header is platform-independent — only the implementation file pulls
// in <windows.h>. That keeps the test build compilable on Linux/macOS
// CI runners.
class LowLevelKeyboardHook final : public IInputSource {
public:
    void set_event_sink(SinkFn sink, void* user_data) noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t dropped_count() const noexcept override;
    std::uint64_t event_count() const noexcept override;

    // Diagnostic — the real hook records any callback that exceeds the
    // 5 us latency budget. Step 1 stub returns 0.
    std::uint64_t slow_callback_count() const noexcept;

private:
    SinkFn sink_{nullptr};
    void*  user_data_{nullptr};
    bool   started_{false};
    std::uint64_t event_count_{0};
    std::uint64_t dropped_count_{0};
    std::uint64_t slow_callback_count_{0};
};

} // namespace remotemic::input