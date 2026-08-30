// Phase 4 / ADR-0014 §3.1: BoundedPcmQueue STUB (step 1 of 6 per
// ADR-0014 §10). Step 2 replaces this with the real mutex-protected
// drop-oldest queue.
//
// Red-state behavior:
//   * constructor takes capacity but the queue never grows
//   * push() accepts samples and immediately discards them
//   * dropped_count_ is always 0
//   * pop_up_to() always returns an empty vector
//
// Tests assert the real contract (push retains, overflow drops oldest,
// pop returns front samples, dropped_count_ increases). They will fail
// on this stub.

#include "remotemic/audio/bounded_pcm_queue.hpp"

#include <stdexcept>

namespace remotemic::audio {

BoundedPcmQueue::BoundedPcmQueue(std::size_t capacity_samples, ClockFn now)
    : capacity_(capacity_samples), now_(now ? std::move(now) : monotonic_clock) {
    if (capacity_ == 0) {
        throw std::invalid_argument(
            "BoundedPcmQueue capacity must be > 0");
    }
}

std::size_t BoundedPcmQueue::size() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    return 0;
}

std::uint64_t BoundedPcmQueue::dropped_count() const noexcept {
    return 0;
}

bool BoundedPcmQueue::empty() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    return true;
}

std::size_t BoundedPcmQueue::push(std::span<const std::int16_t> /*samples*/) noexcept {
    // Stub: silently discard.
    return 0;
}

std::vector<std::int16_t> BoundedPcmQueue::pop_up_to(std::size_t /*max_samples*/) noexcept {
    std::lock_guard<std::mutex> lk(m_);
    return {};
};

} // namespace remotemic::audio