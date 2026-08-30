// Phase 4 / ADR-0014 §3.3: Upsample16kTo48k STUB (step 1 of 6 per
// ADR-0014 §10). Step 2 replaces this with the real three-tap linear
// interpolation byte-aligned with audio_playback.py:154-172.
//
// Red-state behavior:
//   * always returns an empty vector regardless of input
//
// Tests assert the real contract: 3x expansion with
// (prev + delta/3, prev + 2*delta/3, current), rounded + clamped.

#include "remotemic/audio/upsample_16k_to_48k.hpp"

namespace remotemic::audio {

std::vector<std::int16_t>
upsample_16k_to_48k(std::span<const std::int16_t> /*source*/,
                    UpsampleState& /*state*/) {
    return {};
}

} // namespace remotemic::audio