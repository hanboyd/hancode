// Phase 4 / ADR-0014 §3.1: BoundedPcmQueue real implementation.
//
// Thread-safe bounded queue with drop-oldest overflow. Producer threads
// (BLE callback / Python worker via pybind11) call push(); consumer
// (writer jthread inside WasapiAudioRoute) calls pop_up_to(). Overflow
// drops the OLDEST samples to make room for the NEWEST ones, which
// preserves the most recent voice UX (losing the oldest syllable is
// better than dropping the user's current word).
//
// All public methods take a single internal mutex; pybind11 wrappers
// use py::call_guard<py::gil_scoped_release> so producer threads do
// not block the GIL. dropped_count_ is a u64 that only ever increases
// (reset to 0 only at construction), suitable for diagnostics.

#include "remotemic/audio/bounded_pcm_queue.hpp"

#include <algorithm>
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
    return buf_.size();
}

std::uint64_t BoundedPcmQueue::dropped_count() const noexcept {
    return dropped_count_;  // u64; reads are atomic on every common arch
}

bool BoundedPcmQueue::empty() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    return buf_.empty();
}

std::size_t BoundedPcmQueue::push(std::span<const std::int16_t> samples) noexcept {
    if (samples.empty()) {
        return 0;
    }
    std::lock_guard<std::mutex> lk(m_);

    // Drop oldest samples if the incoming batch would overflow.
    const std::size_t incoming = samples.size();
    std::size_t available = capacity_ - buf_.size();
    std::size_t to_drop = 0;
    if (incoming > available) {
        to_drop = incoming - available;
        // Erase the oldest `to_drop` samples from the front.
        buf_.erase(buf_.begin(),
                   buf_.begin() + static_cast<std::ptrdiff_t>(to_drop));
        dropped_count_ += to_drop;
    }

    // Append incoming samples. capacity_ is u32; this is bounded to
    // capacity_ by construction.
    buf_.insert(buf_.end(), samples.begin(), samples.end());

    // Sanity invariant: size never exceeds capacity.
    if (buf_.size() > capacity_) {
        const std::size_t overflow = buf_.size() - capacity_;
        buf_.erase(buf_.begin(),
                   buf_.begin() + static_cast<std::ptrdiff_t>(overflow));
        dropped_count_ += overflow;
        to_drop += overflow;
    }
    return to_drop;
}

std::vector<std::int16_t> BoundedPcmQueue::pop_up_to(std::size_t max_samples) noexcept {
    std::lock_guard<std::mutex> lk(m_);
    if (buf_.empty()) {
        return {};
    }
    const std::size_t take = std::min(max_samples, buf_.size());
    std::vector<std::int16_t> out;
    out.reserve(take);
    out.assign(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(take));
    buf_.erase(buf_.begin(),
               buf_.begin() + static_cast<std::ptrdiff_t>(take));
    return out;
}

} // namespace remotemic::audio