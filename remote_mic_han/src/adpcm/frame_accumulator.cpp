// Phase 2 / Area 4: STUB implementation of FrameAccumulator.
//
// TDD red state at the end of step 2 (ADR-0012 section 3 / section 8):
//   - ctor initializes an empty pending buffer (real behavior)
//   - append() returns {} for any input (real behavior emits frames)
//   - pending_size() always returns 0
//
// All 7 frame-*.json fixtures fail on this stub. Step 3 implements
// the real accumulator matching the Python baseline
// (apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:265-285).

#include "remotemic/adpcm/frame_accumulator.hpp"

namespace remotemic::adpcm {

FrameAccumulator::FrameAccumulator() noexcept = default;

std::vector<std::vector<std::uint8_t>> FrameAccumulator::append(
    std::span<const std::uint8_t>, std::uint16_t) {
    return {};
}

std::size_t FrameAccumulator::pending_size() const noexcept { return 0; }

}  // namespace remotemic::adpcm