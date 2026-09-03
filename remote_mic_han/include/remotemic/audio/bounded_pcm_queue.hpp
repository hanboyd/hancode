#pragma once

// Phase 4 / ADR-0014 §3.1: BoundedPcmQueue — thread-safe bounded queue with
// drop-oldest overflow semantics. Used by WasapiAudioRoute to bridge
// BLE/atvv producer threads (Python / C++ ATVVSession) and the single
// writer jthread inside WasapiAudioRoute. drop-oldest keeps the most
// recent audio: voice UX prefers losing the oldest frame over dropping
// the user's current syllable.

#include <chrono>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

namespace remotemic::audio {

using ClockFn = std::function<std::chrono::milliseconds()>;

inline std::chrono::milliseconds monotonic_clock() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

class BoundedPcmQueue {
public:
    explicit BoundedPcmQueue(std::size_t capacity_samples,
                             ClockFn now = monotonic_clock);

    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t size() const noexcept;
    std::uint64_t dropped_count() const noexcept;
    bool empty() const noexcept;

    // Append samples. If the queue is full, drop the oldest samples to
    // make room. dropped_count_ is incremented by the number of samples
    // evicted. New samples are always retained (drop-oldest, never
    // drop-newest). Returns the number of samples dropped in this call.
    std::size_t push(std::span<const std::int16_t> samples) noexcept;

    // Pop up to max_samples from the front. If the queue is empty,
    // returns an empty vector.
    std::vector<std::int16_t> pop_up_to(std::size_t max_samples) noexcept;

private:
    std::size_t capacity_;
    ClockFn now_;
    mutable std::mutex m_;
    std::vector<std::int16_t> buf_;
    std::atomic<std::uint64_t> dropped_count_{0};
};

} // namespace remotemic::audio
