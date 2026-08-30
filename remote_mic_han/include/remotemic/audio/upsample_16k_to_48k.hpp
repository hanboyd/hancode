#pragma once

// Phase 4 / ADR-0014 §3.3: Upsample16kTo48k — 16 kHz mono int16 -> 48 kHz
// mono int16 three-tap linear interpolation. Byte-aligned with the
// existing Python audio_playback.py:154-172 implementation: every source
// sample expands to (prev + delta/3, prev + 2*delta/3, current), rounded
// to nearest int16 and clamped to [-32768, 32767].

#include <cstdint>
#include <span>
#include <vector>

namespace remotemic::audio {

struct UpsampleState {
    std::int16_t previous_sample{0};
    bool have_previous{false};
};

std::vector<std::int16_t>
upsample_16k_to_48k(std::span<const std::int16_t> source, UpsampleState& state);

} // namespace remotemic::audio