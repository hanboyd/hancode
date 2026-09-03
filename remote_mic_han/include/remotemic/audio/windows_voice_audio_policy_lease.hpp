#pragma once

#include <atomic>

#include "remotemic/interfaces/system_state_lease.hpp"

namespace remotemic::audio {

// Temporarily owns the two Windows policies needed by a voice session:
// default capture endpoint roles and communications ducking. The concrete
// implementation is Windows-only and fails closed elsewhere.
class WindowsVoiceAudioPolicyLease final : public ISystemStateLease {
public:
    WindowsVoiceAudioPolicyLease() = default;
    ~WindowsVoiceAudioPolicyLease() override;
    bool recover_stale() noexcept override;
    bool acquire() noexcept override;
    void restore() noexcept override;
    [[nodiscard]] bool active() const noexcept override;
    [[nodiscard]] bool defaults_are_cable_output() const noexcept;
private:
    std::atomic<bool> active_{false};
};

} // namespace remotemic::audio
