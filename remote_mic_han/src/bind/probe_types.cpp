#include "remotemic/bind/probe_types.hpp"

namespace remotemic {

Counter::Counter() : value_(0) {}

void Counter::increment(std::int64_t delta) {
    std::scoped_lock lock{mutex_};
    value_ += delta;
}

std::int64_t Counter::value() const {
    std::scoped_lock lock{mutex_};
    return value_;
}

} // namespace remotemic