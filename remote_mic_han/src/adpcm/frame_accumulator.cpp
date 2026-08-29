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
        // and does not buffer the data; the call is a no-op.
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
    return out;
}

std::size_t FrameAccumulator::pending_size() const noexcept {
    return pending_.size();
}

}  // namespace remotemic::adpcm