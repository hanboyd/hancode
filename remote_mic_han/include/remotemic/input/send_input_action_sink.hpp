#pragma once

#include <cstdint>

#include <remotemic/input/i_host_action_sink.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.7: SendInputActionSink owns the Win32
// ``user32.SendInput`` handle + a worker thread that coalesces batches
// into single syscalls. submit_key submits submit_system_action
// submit directly. Step 1 stub returns false from every submit_* path.
class SendInputActionSink final : public IHostActionSink {
public:
    bool submit_key(std::uint16_t vk_code, bool key_down,
                    std::chrono::milliseconds deadline) noexcept override;
    bool submit_system_action(SystemAction action) noexcept override;
    void cancel_pending() noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t submit_error_count() const noexcept override;
    std::uint64_t submitted_count() const noexcept override;

private:
    bool   started_{false};
    std::uint64_t submitted_count_{0};
    std::uint64_t submit_error_count_{0};
};

} // namespace remotemic::input