// Phase 2 / Area 4: real implementation of postprocess.
//
// Mirrors apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:241-262
// (postprocess) sample-for-sample. Two stages:
//   1. 3-tap smoothing on interior samples via integer
//      right-shift-by-2:
//        smoothed[i] = (samples[i-1] + 2*samples[i] + samples[i+1]) >> 2
//   2. dB gain with NaN / +inf / -inf treated as 0 and the final
//      dB clamped to [-24, +24]:
//        gain = 10^(safe_gain_db / 20)
//        scaled = round(smoothed[i] * gain)
//
// Each output is clamped to int16 range. Endpoints skip the 3-tap
// smoothing (Python: range(1, len-1) excludes both 0 and len-1).
//
// Validation gate G1/G2 for Area 4 (per ADR-0012):
//   ctest -C Debug   -R '^remotemic_adpcm_postprocess_tests\$' -> 1/1 Passed
//   ctest -C Release -R '^remotemic_adpcm_postprocess_tests\$' -> 1/1 Passed

#include "remotemic/adpcm/postprocess.hpp"

#include <cmath>

namespace remotemic::adpcm {

namespace {

constexpr std::int16_t kInt16Min = -32768;
constexpr std::int16_t kInt16Max = 32767;

inline std::int16_t clamp_int16(long long value) noexcept {
    if (value < static_cast<long long>(kInt16Min)) {
        return kInt16Min;
    }
    if (value > static_cast<long long>(kInt16Max)) {
        return kInt16Max;
    }
    return static_cast<std::int16_t>(value);
}

}  // namespace

std::vector<std::int16_t> postprocess(
    std::span<const std::int16_t> samples, double gain_db) {
    std::vector<std::int16_t> result;
    if (samples.empty()) {
        return result;
    }
    result.reserve(samples.size());

    // Stage 1: 3-tap smoothing. Endpoints stay unsmoothed (matches
    // the Python baseline's range(1, len-1) iteration).
    std::vector<long long> smoothed(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        smoothed[i] = static_cast<long long>(samples[i]);
    }
    if (samples.size() >= 3) {
        for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
            smoothed[i] = (smoothed[i - 1] + 2 * smoothed[i] + smoothed[i + 1]) >> 2;
        }
    }

    // Stage 2: dB gain. NaN / +inf / -inf -> 0.0; clamp to [-24, 24].
    // Python's `gain_db == gain_db and abs(gain_db) != inf` collapses
    // both NaN and inf into 0.0.
    double safe_gain_db = gain_db;
    if (std::isnan(safe_gain_db) || !std::isfinite(safe_gain_db)) {
        safe_gain_db = 0.0;
    }
    if (safe_gain_db < kMinGainDb) {
        safe_gain_db = kMinGainDb;
    }
    if (safe_gain_db > kMaxGainDb) {
        safe_gain_db = kMaxGainDb;
    }
    const double gain = std::pow(10.0, safe_gain_db / 20.0);

    for (const auto value : smoothed) {
        const auto scaled =
            static_cast<long long>(std::llround(static_cast<double>(value) * gain));
        result.push_back(clamp_int16(scaled));
    }
    return result;
}

}  // namespace remotemic::adpcm