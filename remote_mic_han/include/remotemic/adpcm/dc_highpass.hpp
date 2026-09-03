// Phase 2 / Area 4: DC-removal single-pole high-pass filter
// (pure compute).
//
// Per ADR-0012 section 3, this header declares the C++ equivalent of
// apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:210-238
// (DCHighPassFilter). The filter is a single-pole IIR:
//   output[n] = input[n] - input[n-1] + alpha * output[n-1]
// with
//   alpha = exp(-2 * pi * cutoff_hz / sample_rate)
//
// Contract:
//   - No I/O, no threads, no globals. Construction rejects non-positive
//     sample_rate/cutoff_hz with std::invalid_argument, matching Python.
//   - State is owned by the instance; reset before each new audio
//     session.
//   - First sample initializes the filter: previous_input is set to
//     input[0] and the first output is computed using
//     previous_output = 0. Output equals input on the first sample.
//   - The cutoff_hz is fixed at construction; the typical ATVV use
//     case is sample_rate=16000, cutoff_hz=20 (per the Python
//     baseline default).

#ifndef REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_DC_HIGHPASS_HPP
#define REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_DC_HIGHPASS_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace remotemic::adpcm {

class DcHighPassFilter {
public:
    // Construct a filter with the given sample rate and cutoff
    // frequency. Matches the Python baseline's __init__
    // (atvv_protocol.py:213-217). Both values must be positive.
    explicit DcHighPassFilter(
        double sample_rate,
        double cutoff_hz);

    // Reset to the uninitialized state. The next call to process()
    // will re-initialize previous_input from samples[0].
    void reset() noexcept;

    // Filter the input samples in place (output is a new vector; the
    // input span is not modified). Empty input -> empty output.
    // Each output sample is clamped to int16 range before being
    // returned, matching the Python baseline's int(round(...))
    // sequence followed by clamp at line 237.
    //
    // Not const: filtering advances internal state.
    std::vector<std::int16_t> process(std::span<const std::int16_t> samples);

    // Read-only accessors for the filter coefficients. Useful for
    // diagnostic / parity tests that want to confirm the alpha
    // computation matches the Python baseline.
    double alpha() const noexcept;
    double sample_rate() const noexcept;
    double cutoff_hz() const noexcept;

private:
    double sample_rate_;
    double cutoff_hz_;
    double alpha_;
    double previous_input_;
    double previous_output_;
    bool initialized_;
};

}  // namespace remotemic::adpcm

#endif  // REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_DC_HIGHPASS_HPP
