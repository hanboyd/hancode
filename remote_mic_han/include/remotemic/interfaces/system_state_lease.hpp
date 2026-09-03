#pragma once

#include <cstdint>

namespace remotemic {

class ISystemStateLease {
public:
    virtual ~ISystemStateLease() = default;
    virtual bool recover_stale() noexcept = 0;
    virtual bool acquire() noexcept = 0;
    virtual void restore() noexcept = 0;
    [[nodiscard]] virtual bool active() const noexcept = 0;
};

} // namespace remotemic
