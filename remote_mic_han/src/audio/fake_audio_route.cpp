// Phase 4 / ADR-0014 §3.5: FakeAudioRoute STUB (step 1 of 6 per
// ADR-0014 §10). Step 2 replaces this with the real recording double.
//
// Red-state behavior:
//   * start() returns false (so production callers fail closed)
//   * write() returns false
//   * recorded_samples() returns 0
//
// Tests assert the real contract: start() returns true, write() returns
// true, recorded buffer grows by write count.

#include "remotemic/audio/fake_audio_route.hpp"

namespace remotemic::audio {

FakeAudioRoute::FakeAudioRoute() = default;

bool FakeAudioRoute::start(PcmFormat /*format*/) {
    return false;
}

bool FakeAudioRoute::write(std::span<const std::int16_t> /*samples*/) {
    ++write_calls_;
    return false;
}

void FakeAudioRoute::drain(std::chrono::milliseconds /*timeout*/) noexcept {}

void FakeAudioRoute::stop() noexcept {
    ++stopped_;
}

void FakeAudioRoute::close() noexcept {
    ++closed_;
}

std::size_t FakeAudioRoute::recorded_samples() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    return recorded_.size();
}

std::uint64_t FakeAudioRoute::write_call_count() const noexcept {
    return write_calls_.load();
}

std::uint64_t FakeAudioRoute::started_count() const noexcept {
    return started_.load();
}

std::uint64_t FakeAudioRoute::stopped_count() const noexcept {
    return stopped_.load();
}

std::uint64_t FakeAudioRoute::closed_count() const noexcept {
    return closed_.load();
}

std::uint64_t FakeAudioRoute::dropped_count() const noexcept {
    return dropped_.load();
}

PcmFormat FakeAudioRoute::last_format() const noexcept {
    return last_format_;
}

} // namespace remotemic::audio