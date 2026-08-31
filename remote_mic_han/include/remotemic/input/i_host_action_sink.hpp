#pragma once

#include <chrono>

#include <remotemic/input/input_event.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.3: action sink interface. The sink owns the
// Win32 SendInput handle + a worker thread that coalesces + dispatches
// batches. submit_* never blocks and never throws; they return false
// on sink-down.
class IHostActionSink {
public:
    virtual ~IHostActionSink() = default;

    virtual bool submit_key(std::uint16_t vk_code,
                            bool key_down,
                            std::chrono::milliseconds deadline) noexcept = 0;
    virtual bool submit_system_action(SystemAction action) noexcept = 0;
    virtual void cancel_pending() noexcept = 0;
    virtual bool start() noexcept = 0;
    virtual void stop() noexcept = 0;

    // Diagnostics. Implementation-defined counts.
    virtual std::uint64_t submit_error_count() const noexcept = 0;
    virtual std::uint64_t submitted_count() const noexcept = 0;
};

} // namespace remotemic::input