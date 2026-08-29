// Phase 2 / Area 4: real implementation of DcHighPassFilter.
//
// Mirrors apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:210-238
// (DCHighPassFilter) sample-for-sample. Single-pole IIR high-pass:
//
//   output[n] = input[n] - input[n-1] + alpha * output[n-1]
//   alpha = exp(-2 * pi * cutoff_hz / sample_rate)
//
// The first call to process() initializes previous_input from
// samples[0]; the first output is computed using previous_output = 0
// so it equals samples[0] exactly. Each output is clamped to int16
// range via the same int(round(...)) + min/max sequence the Python
// baseline uses (atvv_protocol.py:237).
//
// Validation gate G1/G2 for Area 4 (per ADR-0012):
//   ctest -C Debug   -R '^remotemic_adpcm_dc_tests\$' -> 1/1 Passed
//   ctest -C Release -R '^remotemic_adpcm_dc_tests\$' -> 1/1 Passed

#include "remotemic/adpcm/dc_highpass.hpp"

#include <algorithm>
#include <cmath>

namespace remotemic::adpcm {

namespace {

// M_PI is a POSIX / GNU extension and is not exposed by MSVC's
// <cmath> by default. Define a local constant rather than relying on
// the non-portable macro.
constexpr double kPi = 3.14159265358979323846;

constexpr std::int16_t kInt16Min = -32768;
constexpr std::int16_t kInt16Max = 32767;

inline std::int16_t clamp_int16(double value) noexcept {
    if (value < static_cast<double>(kInt16Min)) {
        return kInt16Min;
    }
    if (value > static_cast<double>(kInt16Max)) {
        return kInt16Max;
    }
    return static_cast<std::int16_t>(value);
}

}  // namespace

DcHighPassFilter::DcHighPassFilter(
    double sample_rate, double cutoff_hz) noexcept
    : sample_rate_(sample_rate),
      cutoff_hz_(cutoff_hz),
      alpha_(std::exp(-2.0 * kPi * cutoff_hz / sample_rate)),
      previous_input_(0.0),
      previous_output_(0.0),
      initialized_(false) {}

void DcHighPassFilter::reset() noexcept {
    previous_input_ = 0.0;
    previous_output_ = 0.0;
    initialized_ = false;
}

std::vector<std::int16_t> DcHighPassFilter::process(
    std::span<const std::int16_t> samples) {
    std::vector<std::int16_t> filtered;
    if (samples.empty()) {
        return filtered;
    }
    filtered.reserve(samples.size());

    if (!initialized_) {
        previous_input_ = static_cast<double>(samples[0]);
        initialized_ = true;
    }

    for (const auto sample : samples) {
        const auto current = static_cast<double>(sample);
        const auto output =
            current - previous_input_ + alpha_ * previous_output_;
        previous_input_ = current;
        previous_output_ = output;
        filtered.push_back(clamp_int16(std::round(output)));
    }
    return filtered;
}

double DcHighPassFilter::alpha() const noexcept { return alpha_; }
double DcHighPassFilter::sample_rate() const noexcept { return sample_rate_; }
double DcHighPassFilter::cutoff_hz() const noexcept { return cutoff_hz_; }

}  // namespace remotemic::adpcm