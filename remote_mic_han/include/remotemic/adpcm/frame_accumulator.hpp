// Phase 2 / Area 4: arbitrary-payload-to-fixed-frame re-chunker,
// pure compute (stateful value type).
//
// Per ADR-0012 section 3, this header declares the C++ equivalent of
// apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:265-285
// (FrameAccumulator). The accumulator buffers incoming bytes and
// emits complete frames of `frame_size` bytes each time append() is
// called; any leftover bytes (less than frame_size) stay pending for
// the next call.
//
// Contract:
//   - No I/O, no threads, no globals, no exceptions.
//   - frame_size <= 0 -> append returns an empty list and does not
//     buffer the data (matches the Python baseline's guard at
//     atvv_protocol.py:272-273).
//   - The accumulator owns its pending buffer; append() does not
//     modify the input span.
//   - Each emitted frame is a fresh std::vector<std::uint8_t> with
//     exactly frame_size bytes.

#ifndef REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_FRAME_ACCUMULATOR_HPP
#define REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_FRAME_ACCUMULATOR_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace remotemic::adpcm {

class FrameAccumulator {
public:
    FrameAccumulator() noexcept;

    // Append data to the pending buffer. Returns every complete
    // frame of frame_size bytes that the append caused to be
    // emitted; leftover bytes (less than frame_size) stay pending
    // for future calls. frame_size <= 0 returns an empty list and
    // does not buffer the data.
    //
    // Not const: append() advances internal pending state.
    std::vector<std::vector<std::uint8_t>> append(
        std::span<const std::uint8_t> data,
        std::uint16_t frame_size);

    // Read-only accessor for the current pending byte count. Useful
    // for diagnostics and parity tests that need to confirm the
    // accumulator correctly retains remainders across calls.
    std::size_t pending_size() const noexcept;

private:
    std::vector<std::uint8_t> pending_;
};

}  // namespace remotemic::adpcm

#endif  // REMOTEMIC_INCLUDE_REMOTEMIC_ADPCM_FRAME_ACCUMULATOR_HPP