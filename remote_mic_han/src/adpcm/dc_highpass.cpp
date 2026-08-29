// Phase 2 / Area 4: STUB implementation of DcHighPassFilter.
//
// TDD red state at the end of step 2 (ADR-0012 section 3 / section 8):
//   - ctor accepts (sample_rate, cutoff_hz) but ignores them; alpha=0
//   - reset() is a no-op (initialized stays false)
//   - process() returns {} for any input
//   - alpha() / sample_rate() / cutoff_hz() all return 0
//
// All 5 dc-*.json fixtures fail on this stub. Step 3 implements the
// real filter matching the Python baseline
// (apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:210-238).

#include "remotemic/adpcm/dc_highpass.hpp"

namespace remotemic::adpcm {

DcHighPassFilter::DcHighPassFilter(double sample_rate, double cutoff_hz) noexcept
    : sample_rate_(0.0), cutoff_hz_(0.0), alpha_(0.0),
      previous_input_(0.0), previous_output_(0.0), initialized_(false) {
    // STUB: real implementation computes alpha_ = exp(-2*pi*cutoff_hz/sample_rate).
    (void)sample_rate;
    (void)cutoff_hz;
}

void DcHighPassFilter::reset() noexcept {
    previous_input_ = 0.0;
    previous_output_ = 0.0;
    initialized_ = false;
}

std::vector<std::int16_t> DcHighPassFilter::process(
    std::span<const std::int16_t>) {
    return {};
}

double DcHighPassFilter::alpha() const noexcept { return alpha_; }
double DcHighPassFilter::sample_rate() const noexcept { return sample_rate_; }
double DcHighPassFilter::cutoff_hz() const noexcept { return cutoff_hz_; }

}  // namespace remotemic::adpcm