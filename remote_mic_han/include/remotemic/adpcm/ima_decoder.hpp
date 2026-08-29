// Phase 2 / Area 3: standard IMA/DVI 4-bit ADPCM decoder, pure compute.
//
// Per ADR-0012 section 3, this header declares the C++ equivalent of
// apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:158-207
// (IMAADPCMDecoder). The decoder is a stateful value type:
//   - reset(predictor, step_index) primes the predictor and
//     step_index (clamped to [-32768, 32767] and [0, 88]).
//   - decode(data) consumes a byte stream (high nibble first) and
//     returns 2 * len(data) samples. Each nibble mutates the
//     internal predictor and step_index; the returned samples are
//     the post-update predictor values, matching the Python
//     implementation byte-for-byte.
//
// Contract:
//   - No I/O, no threads, no globals, no exceptions.
//   - State is owned by the instance; reset before each new audio
//     session (typically primed by an AUDIO_SYNC notification).
//   - The step and index tables are implementation details of the
//     standard IMA/DVI algorithm; the fixture corpus in
//     apps/windows/rc003/tests/fixtures/atvv/adpcm-*.json is the
//     single source of truth for byte-exact equivalence with the
//     Python baseline.

#ifndef REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_IMA_DECODER_HPP
#define REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_IMA_DECODER_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace remotemic::adpcm {

class ImaDecoder {
public:
    // Constructs a decoder with predictor = 0, step_index = 0.
    ImaDecoder() noexcept;

    // Reset the predictor and step_index. predictor is clamped to
    // [-32768, 32767] and step_index to [0, 88], matching the
    // Python baseline (atvv_protocol.py:177-179).
    void reset(std::int16_t predictor, std::uint8_t step_index) noexcept;

    // Decode a byte stream into PCM samples. Each input byte yields
    // exactly 2 samples (high nibble first). The returned vector has
    // length 2 * data.size(); an empty input yields an empty vector.
    // The decoder's predictor and step_index are updated in place;
    // the returned samples are the post-update predictor values,
    // matching the Python implementation (atvv_protocol.py:181-187,
    // 189-207) sample-for-sample.
    //
    // Not const: decoding advances internal state.
    std::vector<std::int16_t> decode(std::span<const std::uint8_t> data);

    // Read-only accessors for the current decoder state. Useful for
    // round-trip tests and for diagnostics that compare against
    // AUDIO_SYNC-supplied predictor / step_index pairs.
    std::int16_t predictor() const noexcept;
    std::uint8_t step_index() const noexcept;

private:
    // Decode one nibble (0..15) and update predictor + step_index.
    // Returns the post-update predictor value. Matches the Python
    // baseline's _decode_nibble (atvv_protocol.py:189-207).
    std::int16_t decode_nibble(std::uint8_t nibble) noexcept;

    std::int16_t predictor_;
    std::uint8_t step_index_;
};

}  // namespace remotemic::adpcm

#endif  // REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_IMA_DECODER_HPP