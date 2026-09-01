#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

#include <remotemic/input/i_host_action_sink.hpp>
#include <remotemic/input/input_event.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §8: cross-OS test double for IHostActionSink.
// Holds submitted vk/down pairs and system actions. NOT Windows-only.
class FakeHostActionSink final : public IHostActionSink {
public:
    bool submit_key(std::uint16_t vk_code, bool key_down,
                    std::chrono::milliseconds deadline) noexcept override;
    bool submit_system_action(SystemAction action) noexcept override;
    void cancel_pending() noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t submit_error_count() const noexcept override;
    std::uint64_t submitted_count() const noexcept override;

    using KeyEntry   = std::pair<std::uint16_t, bool>;  // (vk, key_down)
    using SysEntry   = SystemAction;

    // Snapshot under mutex; safe to call from any thread.
    std::vector<KeyEntry> recorded_keys() const;
    std::vector<SysEntry> recorded_system_actions() const;
    std::size_t pending_count() const;

    // Test-only helper: configure the sink to reject every submit_* call
    // (returns false). Mirrors "send_input_error_count" expectations.
    void set_submit_fails_for_test(bool fails) noexcept;

    // Test-only helper: configure the sink to start failing once the
    // submitted_count has reached ``threshold``. Use 0 (default) for
    // no threshold-based failing. Lets release_held tests simulate a
    // sink that succeeds for the first N submits then drops mid-stream.
    void set_fail_after_count_for_test(std::uint64_t threshold) noexcept;

private:
    std::uint64_t submitted_count_{0};
    std::uint64_t submit_error_count_{0};
    bool submit_fails_{false};
    std::uint64_t fail_after_count_{0};  // 0 = never
    mutable std::mutex mu_;
    std::deque<KeyEntry> keys_;
    std::deque<SysEntry> sys_;
};

} // namespace remotemic::input