#pragma once

// Phase 4 / ADR-0014 §3.2: PcmChunker — slice incoming PCM into fixed-size
// chunks (default 20 ms @ 16 kHz = 320 samples) for the writer thread to
// hand to WASAPI's IAudioClient::Write. Residue is buffered; flush_remaining
// pads with silence up to one chunk so the device never underruns.

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace remotemic::audio {

class PcmChunker {
public:
    explicit PcmChunker(std::chrono::milliseconds chunk_duration,
                        std::uint32_t sample_rate_hz = 16'000);

    std::size_t chunk_samples() const noexcept { return chunk_samples_; }

    // Append ``incoming`` to internal buffer. If at least one full chunk
    // is ready, return the front chunk and keep the residue. Otherwise
    // return nullopt.
    std::optional<std::vector<std::int16_t>>
    next_chunk(std::span<const std::int16_t> incoming) noexcept;

    // Return any buffered residue padded with int16 zero up to one full
    // chunk. After this call the buffer is empty. Used by the writer
    // thread on shutdown so the device never plays back a held-final
    // sample.
    std::vector<std::int16_t> flush_remaining_with_silence() noexcept;

    std::size_t buffered_samples() const noexcept { return buf_.size(); }

private:
    std::size_t chunk_samples_;
    std::vector<std::int16_t> buf_;
};

} // namespace remotemic::audio