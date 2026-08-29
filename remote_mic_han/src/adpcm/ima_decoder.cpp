// Phase 2 / Area 3: real implementation of the IMA/DVI ADPCM decoder.
//
// Mirrors apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:158-207
// (IMAADPCMDecoder) sample-for-sample. The Python implementation
// reuses a non-standard bit-to-difference mapping where the LSB of
// the nibble contributes step/4 (instead of the more common step/8);
// this is the mapping used by the Xiaomi Mi Box ATVV remote, so the
// C++ implementation has to follow it exactly to stay byte-exact.
//
// Validation gate G1/G2 for Area 3 (per ADR-0012):
//   ctest -C Debug   -R '^remotemic_adpcm_ima_tests\$' -> 1/1 Passed
//   ctest -C Release -R '^remotemic_adpcm_ima_tests\$' -> 1/1 Passed

#include "remotemic/adpcm/ima_decoder.hpp"

#include <algorithm>

namespace remotemic::adpcm {

namespace {

// Standard IMA/DVI step size table. Index 0..88; values monotonically
// non-decreasing. Copied verbatim from atvv_protocol.py:161-170.
constexpr std::int16_t kStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
};

// Standard IMA/DVI step index delta table. Index is the low 3 bits of
// the nibble (0..7). The Python baseline uses (-1, -1, -1, -1, 2, 4,
// 6, 8) - same as the standard table.
constexpr std::int8_t kIndexTable[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

constexpr std::int16_t kPredictorMin = -32768;
constexpr std::int16_t kPredictorMax = 32767;
constexpr std::uint8_t kStepIndexMin = 0;
constexpr std::uint8_t kStepIndexMax = 88;

inline std::int16_t clamp_predictor(std::int16_t value) noexcept {
    return std::clamp<std::int16_t>(value, kPredictorMin, kPredictorMax);
}

inline std::uint8_t clamp_step_index(int value) noexcept {
    if (value < static_cast<int>(kStepIndexMin)) {
        return kStepIndexMin;
    }
    if (value > static_cast<int>(kStepIndexMax)) {
        return kStepIndexMax;
    }
    return static_cast<std::uint8_t>(value);
}

}  // namespace

ImaDecoder::ImaDecoder() noexcept
    : predictor_(0), step_index_(0) {}

void ImaDecoder::reset(
    std::int16_t predictor, std::uint8_t step_index) noexcept {
    predictor_ = clamp_predictor(predictor);
    step_index_ = clamp_step_index(static_cast<int>(step_index));
}

std::vector<std::int16_t> ImaDecoder::decode(
    std::span<const std::uint8_t> data) {
    std::vector<std::int16_t> samples;
    samples.reserve(data.size() * 2);
    for (const auto byte : data) {
        // High nibble first, exactly as in the Python baseline
        // (atvv_protocol.py:184-186).
        samples.push_back(decode_nibble(static_cast<std::uint8_t>(byte >> 4)));
        samples.push_back(decode_nibble(static_cast<std::uint8_t>(byte & 0x0F)));
    }
    return samples;
}

std::int16_t ImaDecoder::predictor() const noexcept { return predictor_; }
std::uint8_t ImaDecoder::step_index() const noexcept { return step_index_; }

std::int16_t ImaDecoder::decode_nibble(std::uint8_t nibble) noexcept {
    // Per the Python baseline (atvv_protocol.py:189-207), the bit
    // mapping is:
    //   nibble & 1 -> add step >> 2  (step / 4 contribution)
    //   nibble & 2 -> add step >> 1  (step / 2 contribution)
    //   nibble & 4 -> add step       (full step contribution)
    //   nibble & 8 -> sign (subtract instead of add)
    // This differs from the more common IMA mapping where bit 0 is
    // step / 8, but the upstream device uses this non-standard
    // mapping so the C++ side has to match exactly.
    const auto step = static_cast<std::int16_t>(kStepTable[step_index_]);

    // Use int32_t for the accumulator: at large step_index values
    // (e.g. 15289) the sum of all four contributions can exceed
    // 32767 (the int16 max). The Python baseline uses Python int
    // (unbounded) so the equivalent arithmetic never overflows.
    // Wrapping here would cause the predictor to silently invert
    // sign mid-decode. Clamp the final predictor value back into
    // int16 range.
    std::int32_t difference = static_cast<std::int32_t>(step) >> 3;
    if (nibble & 1u) {
        difference += static_cast<std::int32_t>(step) >> 2;
    }
    if (nibble & 2u) {
        difference += static_cast<std::int32_t>(step) >> 1;
    }
    if (nibble & 4u) {
        difference += static_cast<std::int32_t>(step);
    }

    std::int32_t next = static_cast<std::int32_t>(predictor_);
    if (nibble & 8u) {
        next -= difference;
    } else {
        next += difference;
    }
    // Clamp in int32 space first; if we cast a value > 32767 to
    // int16 it wraps modulo 65536 and the clamp_predictor below would
    // silently accept the wrapped value as in-range.
    constexpr std::int32_t kPredictorMin32 = -32768;
    constexpr std::int32_t kPredictorMax32 = 32767;
    if (next < kPredictorMin32) next = kPredictorMin32;
    if (next > kPredictorMax32) next = kPredictorMax32;
    predictor_ = static_cast<std::int16_t>(next);

    // step_index uses the low 3 bits (nibble & 7). Using int for the
    // arithmetic so the + delta doesn't overflow std::uint8_t before
    // clamping.
    step_index_ = clamp_step_index(
        static_cast<int>(step_index_) + kIndexTable[nibble & 7u]);

    return predictor_;
}

}  // namespace remotemic::adpcm