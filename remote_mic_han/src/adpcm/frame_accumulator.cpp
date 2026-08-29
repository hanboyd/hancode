// Phase 2 / Area 4: real implementation of FrameAccumulator.
//
// Mirrors apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:265-285
// (FrameAccumulator) byte-for-byte. The accumulator buffers incoming
// bytes and emits complete frames of `frame_size` bytes each time
// append() is called; leftover bytes stay pending for the next call.
//
// frame_size <= 0 returns an empty list and does not buffer the data
// (matches the Python baseline's guard at atvv_protocol.py:272-273).
//
// Validation gate G1/G2 for Area 4 (per ADR-0012):
//   ctest -C Debug   -R '^remotemic_adpcm_frame_tests\$' -> 1/1 Passed
//   ctest -C Release -R '^remotemic_adpcm_frame_tests\$' -> 1/1 Passed

#include "remotemic/adpcm/frame_accumulator.hpp"

#include <algorithm>

namespace remotemic::adpcm {

FrameAccumulator::FrameAccumulator() noexcept = default;

std::vector<std::vector<std::uint8_t>> FrameAccumulator::append(
    std::span<const std::uint8_t> data, std::uint16_t frame_size) {
    std::vector<std::vector<std::uint8_t>> out;
    if (frame_size <= 0) {
        // The Python baseline's guard rejects zero / negative widths
        // and does not buffer the data; the call is a no-op. The
        // accumulated pending buffer is NOT cleared on this no-op
        // path (matches the Python FrameAccumulator.append at
        // atvv_protocol.py:271-279); the caller controls a stream
        // boundary via reset() rather than by append() with a bad
        // frame_size.
        return out;
    }

    pending_.insert(pending_.end(), data.begin(), data.end());

    const auto target = static_cast<std::size_t>(frame_size);
    while (pending_.size() >= target) {
        out.emplace_back(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(target));
        pending_.erase(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(target));
    }
    // Post-condition (ADR-0012 Phase 2 / Area 4 step 4 invariant):
    // pending_size_ < frame_size <= 65535. The lower bound "<" holds
    // because the while loop above drains frames of exactly
    // frame_size bytes until fewer bytes remain. The upper bound
    // "<= 65535" follows directly from the std::uint16_t type.
    return out;
}

void FrameAccumulator::reset() noexcept {
    // Drop the pending buffer in O(1). capacity() is retained so
    // the next stream can reuse the allocation without an extra
    // heap request, but the size() drops to 0 immediately.
    pending_.clear();
}

std::size_t FrameAccumulator::pending_size() const noexcept {
    return pending_.size();
}

}  // namespace remotemic::adpcm