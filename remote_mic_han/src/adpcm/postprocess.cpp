// Phase 2 / Area 4: STUB implementation of postprocess.
//
// TDD red state at the end of step 2 (ADR-0012 section 3 / section 8):
//   - postprocess() returns {} for any input.
//
// All 10 postprocess-*.json fixtures fail on this stub. Step 3
// implements the real filter matching the Python baseline
// (apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:241-262).

#include "remotemic/adpcm/postprocess.hpp"

namespace remotemic::adpcm {

std::vector<std::int16_t> postprocess(
    std::span<const std::int16_t>, double) {
    return {};
}

}  // namespace remotemic::adpcm