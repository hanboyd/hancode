// Phase 2 / Area 4: PCM post-processing (3-tap smoothing + dB gain),
// pure compute free function.
//
// Per ADR-0012 section 3, this header declares the C++ equivalent of
// apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:241-262
// (postprocess). Two stages:
//   1. 3-tap smoothing on the middle samples only:
//        smoothed[i] = (samples[i-1] + 2*samples[i] + samples[i+1]) >> 2
//      (integer arithmetic via right-shift-by-2, matching the Python
//      baseline). Endpoints are left unsmoothed.
//   2. Gain in dB, with NaN/inf treated as 0 and the final dB
//      clamped to [-24, +24]:
//        gain = 10^(safe_gain_db / 20)
//        scaled = round(smoothed[i] * gain)
//      Each scaled value is clamped to int16 range.
//
// Contract:
//   - No I/O, no threads, no globals, no exceptions.
//   - Empty input -> empty output.
//   - NaN / +inf / -inf gain_db -> treated as 0 dB (identity after
//     smoothing).
//   - gain_db outside [-24, 24] -> clamped.

#ifndef REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_POSTPROCESS_HPP
#define REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_POSTPROCESS_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace remotemic::adpcm {

// Match the Python baseline's default (atvv_protocol.py:241).
constexpr double kDefaultGainDb = 10.0;

// Match the Python baseline's clamp (atvv_protocol.py:255).
constexpr double kMinGainDb = -24.0;
constexpr double kMaxGainDb = 24.0;

// Apply the 3-tap smoothing + dB gain to a sample sequence.
// Returns a new vector with the same length as the input; empty
// input returns an empty vector. Output values are clamped to int16
// range. The 3-tap smoothing only applies to interior samples (i in
// [1, len-1]); endpoints are passed through unchanged. gain_db
// follows the same NaN/inf/clamp rules as the Python baseline.
std::vector<std::int16_t> postprocess(
    std::span<const std::int16_t> samples,
    double gain_db = kDefaultGainDb);

}  // namespace remotemic::adpcm

#endif  // REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_POSTPROCESS_HPP