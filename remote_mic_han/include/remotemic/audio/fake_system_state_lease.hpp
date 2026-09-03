#pragma once

#include <cstdint>
#include "remotemic/interfaces/system_state_lease.hpp"

namespace remotemic::audio {

class FakeSystemStateLease final : public ISystemStateLease {
public:
    bool recover_stale() noexcept override { ++recover_count; return recover_ok; }
    bool acquire() noexcept override {
        ++acquire_count;
        if (!acquire_ok) return false;
        active_ = true;
        return true;
    }
    void restore() noexcept override { ++restore_count; active_ = false; }
    [[nodiscard]] bool active() const noexcept override { return active_; }
    bool recover_ok{true};
    bool acquire_ok{true};
    std::uint64_t recover_count{0};
    std::uint64_t acquire_count{0};
    std::uint64_t restore_count{0};
private:
    bool active_{false};
};

} // namespace remotemic::audio
