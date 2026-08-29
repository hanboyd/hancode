// Phase 2 / Area 3: STUB implementation of the IMA/DVI ADPCM decoder.
//
// TDD red state at the end of step 2 (ADR-0012 section 3 / section 8):
//   - ImaDecoder::reset() leaves state at 0
//   - ImaDecoder::decode() returns {} for any input
//   - predictor() / step_index() always return 0
//
// All 11 JSON fixtures fail on this stub. Step 3 replaces this body
// with the real implementation matching the Python baseline
// (apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:158-207)
// sample-for-sample.

#include "remotemic/adpcm/ima_decoder.hpp"

namespace remotemic::adpcm {

ImaDecoder::ImaDecoder() noexcept
    : predictor_(0), step_index_(0) {}

void ImaDecoder::reset(std::int16_t predictor, std::uint8_t step_index) noexcept {
    // STUB: real implementation clamps to [-32768, 32767] and [0, 88].
    predictor_ = 0;
    step_index_ = 0;
}

std::vector<std::int16_t> ImaDecoder::decode(std::span<const std::uint8_t>) {
    // STUB: real implementation walks each byte, decodes the high
    // then the low nibble, and appends the post-update predictor to
    // the result vector.
    return {};
}

std::int16_t ImaDecoder::predictor() const noexcept { return predictor_; }
std::uint8_t ImaDecoder::step_index() const noexcept { return step_index_; }

std::int16_t ImaDecoder::decode_nibble(std::uint8_t) noexcept {
    return predictor_;
}

}  // namespace remotemic::adpcm