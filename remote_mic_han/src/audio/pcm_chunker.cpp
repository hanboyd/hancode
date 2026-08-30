// Phase 4 / ADR-0014 §3.2: PcmChunker real implementation.
//
// Buffers incoming PCM samples and emits fixed-size 20 ms chunks for
// the writer jthread to push into WASAPI's IAudioClient::Write. The
// residue (anything less than one chunk when the writer shuts down)
// is silence-padded up to a full chunk on flush_remaining_with_silence,
// so the device never plays back a held-final sample.

#include "remotemic/audio/pcm_chunker.hpp"

#include <algorithm>
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
    const auto samples_per_ms = static_cast<std::size_t>(sample_rate_hz / 1000U);
    chunk_samples_ = samples_per_ms * static_cast<std::size_t>(chunk_duration.count());
    if (chunk_samples_ == 0) {
        throw std::invalid_argument("PcmChunker chunk too small");
    }
    buf_.reserve(chunk_samples_);
}

std::optional<std::vector<std::int16_t>>
PcmChunker::next_chunk(std::span<const std::int16_t> incoming) noexcept {
    if (!incoming.empty()) {
        buf_.insert(buf_.end(), incoming.begin(), incoming.end());
    }
    if (buf_.size() < chunk_samples_) {
        return std::nullopt;
    }
    std::vector<std::int16_t> chunk(buf_.begin(),
                                    buf_.begin() + static_cast<std::ptrdiff_t>(chunk_samples_));
    buf_.erase(buf_.begin(),
               buf_.begin() + static_cast<std::ptrdiff_t>(chunk_samples_));
    return chunk;
}

std::vector<std::int16_t> PcmChunker::flush_remaining_with_silence() noexcept {
    if (buf_.empty()) {
        // No residue; still emit a full silence chunk so the device
        // gets one more 20 ms of zero PCM (matches Python baseline
        // behavior of always emitting one trailing silence block).
        return std::vector<std::int16_t>(chunk_samples_, 0);
    }
    std::vector<std::int16_t> chunk;
    chunk.reserve(chunk_samples_);
    chunk.assign(buf_.begin(), buf_.end());
    chunk.resize(chunk_samples_, 0);
    buf_.clear();
    return chunk;
}

} // namespace remotemic::audio