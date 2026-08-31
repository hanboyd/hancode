// Phase 4 / ADR-0014 §3.5: FakeAudioRoute real implementation.
//
// Recording test double for IAudioRoute. Used by:
//   - Linux/macOS CI to validate the writer-loop test path
//     (remotemic_wasapi_audio_route_tests on Windows builds the real
//      route; on Linux the test wires FakeAudioRoute into the same
//      writer loop via test scaffolding)
//   - Shadow parity tests (step 4) that compare sample-count / peak /
//      RMS / drop-count / drain-order against the Python baseline,
//      keeping the plan §3 rule 5 single-owner invariant intact
//      (FakeAudioRoute never touches a real device).

#include "remotemic/audio/fake_audio_route.hpp"

#include <chrono>
#include <cmath>

namespace remotemic::audio {

FakeAudioRoute::FakeAudioRoute() = default;

bool FakeAudioRoute::start(PcmFormat format) {
    {
        std::lock_guard<std::mutex> lk(m_);
        last_format_ = format;
        started_flag_ = true;
        recorded_.clear();
    }
    ++started_;
    return true;
}

bool FakeAudioRoute::write(std::span<const std::int16_t> samples) {
    // Count every invocation (including rejected ones) so operators
    // can compute success rate as 1 - dropped_/write_calls_.
    ++write_calls_;
    {
        std::lock_guard<std::mutex> lk(m_);
        if (!started_flag_) {
            ++dropped_;
            return false;
        }
        recorded_.insert(recorded_.end(), samples.begin(), samples.end());
    }
    return true;
}

void FakeAudioRoute::drain(std::chrono::milliseconds /*timeout*/) noexcept {
    // Recording-only: no real queue. drain() is a no-op.
}

void FakeAudioRoute::stop() noexcept {
    ++stopped_;
}

void FakeAudioRoute::close() noexcept {
    {
        std::lock_guard<std::mutex> lk(m_);
        started_flag_ = false;
    }
    ++closed_;
}

std::size_t FakeAudioRoute::recorded_samples() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    return recorded_.size();
}

std::vector<std::int16_t> FakeAudioRoute::recorded_snapshot() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    return recorded_;
}

std::int32_t FakeAudioRoute::peak_abs() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    if (recorded_.empty()) {
        return 0;
    }
    std::int32_t peak = 0;
    for (auto s : recorded_) {
        std::int32_t mag = s < 0
            ? -static_cast<std::int32_t>(s)
            : static_cast<std::int32_t>(s);
        if (mag > peak) {
            peak = mag;
        }
    }
    return peak;
}

double FakeAudioRoute::rms_value() const noexcept {
    std::lock_guard<std::mutex> lk(m_);
    if (recorded_.empty()) {
        return 0.0;
    }
    // int16 squared fits in int32 only for |s| <= 181 (32768/181 = 181);
    // we accumulate in int64 to avoid overflow across the full int16
    // range. The fake's recorded buffer is a test fixture so the cost
    // is irrelevant; this stays correct for any int16 input.
    std::int64_t acc = 0;
    for (auto s : recorded_) {
        acc += static_cast<std::int64_t>(s) *
               static_cast<std::int64_t>(s);
    }
    return std::sqrt(static_cast<double>(acc) /
                     static_cast<double>(recorded_.size()));
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
    std::lock_guard<std::mutex> lk(m_);
    return last_format_;
}

} // namespace remotemic::audio