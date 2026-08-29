#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace remotemic {

struct VersionInfo {
    std::string product;
    std::string version;
    std::uint32_t build_number;
};

class Counter {
public:
    Counter();

    void increment(std::int64_t delta);
    [[nodiscard]] std::int64_t value() const;

private:
    mutable std::mutex mutex_;
    std::int64_t value_;
};

using CounterSink = std::function<void(std::int64_t)>;

} // namespace remotemic