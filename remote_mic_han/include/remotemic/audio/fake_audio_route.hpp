#pragma once

// Phase 4 / ADR-0014 §3.5: FakeAudioRoute — test double for IAudioRoute.
// All writes append to an internal buffer; start() always succeeds; stop()
// and close() are no-ops. Used by the writer-loop test in CI (Linux /
// macOS) and by the shadow parity tests (ADR-0014 §6) in place of real
// WASAPI to keep single-owner rule (plan §3 rule 5) intact.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

#include "remotemic/interfaces/audio_route.hpp"

namespace remotemic::audio {

class FakeAudioRoute final : public IAudioRoute {
public:
    FakeAudioRoute();

    bool start(PcmFormat format) override;
    bool write(std::span<const std::int16_t> samples) override;
    void drain(std::chrono::milliseconds timeout) noexcept override;
    void stop() noexcept override;
    void close() noexcept override;

    // Test introspection. All read under mutex_.
    std::size_t recorded_samples() const noexcept;
    std::vector<std::int16_t> recorded_snapshot() const noexcept;
    std::int32_t peak_abs() const noexcept;
    double rms_value() const noexcept;
    std::uint64_t write_call_count() const noexcept;
    std::uint64_t started_count() const noexcept;
    std::uint64_t stopped_count() const noexcept;
    std::uint64_t closed_count() const noexcept;
    std::uint64_t dropped_count() const noexcept;
    PcmFormat last_format() const noexcept;

private:
    mutable std::mutex m_;
    std::vector<std::int16_t> recorded_;
    std::atomic<std::uint64_t> write_calls_{0};
    std::atomic<std::uint64_t> started_{0};
    std::atomic<std::uint64_t> stopped_{0};
    std::atomic<std::uint64_t> closed_{0};
    std::atomic<std::uint64_t> dropped_{0};
    PcmFormat last_format_{};
    bool started_flag_{false};
};

} // namespace remotemic::audio