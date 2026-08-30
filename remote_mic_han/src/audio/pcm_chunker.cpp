// Phase 4 / ADR-0014 §3.2: PcmChunker STUB (step 1 of 6 per
// ADR-0014 §10). Step 2 replaces this with the real 20 ms chunk slicer
// + silence padding on flush.
//
// Red-state behavior:
//   * next_chunk() always returns nullopt (never emits a chunk)
//   * flush_remaining_with_silence() always returns empty vector
//   * buffered_samples() returns 0
//
// Tests assert the real contract: 20 ms chunks emitted in order,
// residue buffered, last partial chunk silence-padded.

#include "remotemic/audio/pcm_chunker.hpp"

#include <stdexcept>

namespace remotemic::audio {

PcmChunker::PcmChunker(std::chrono::milliseconds chunk_duration,
                       std::uint32_t sample_rate_hz) {
    if (chunk_duration <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("PcmChunker chunk_duration must be > 0");
    }
    if (sample_rate_hz == 0) {
        throw std::invalid_argument("PcmChunker sample_rate_hz must be > 0");
    }
    // 320 samples per 20 ms @ 16 kHz. Cast carefully to avoid truncation.
    auto samples_per_ms = static_cast<std::size_t>(sample_rate_hz / 1000U);
    chunk_samples_ = samples_per_ms * static_cast<std::size_t>(chunk_duration.count());
    if (chunk_samples_ == 0) {
        throw std::invalid_argument("PcmChunker chunk too small");
    }
}

std::optional<std::vector<std::int16_t>>
PcmChunker::next_chunk(std::span<const std::int16_t> /*incoming*/) noexcept {
    return std::nullopt;
}

std::vector<std::int16_t> PcmChunker::flush_remaining_with_silence() noexcept {
    return {};
}

} // namespace remotemic::audio