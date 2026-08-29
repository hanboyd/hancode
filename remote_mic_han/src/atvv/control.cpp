// Phase 2 / Area 2: STUB implementation of ATVV control message
// encoding + decoding.
//
// TDD red state at the end of step 2 (ADR-0012 §3 / §8):
//
//   - parse_control_message returns std::nullopt for any payload,
//     so the empty-input fixture passes and the 7 valid-input
//     decode fixtures FAIL.
//   - mic_open_command returns {}, so all 2 mic_open encode
//     fixtures FAIL.
//   - mic_close_command returns {}, so all 2 mic_close encode
//     fixtures FAIL.
//
// Step 3 replaces this body with the real implementation matching
// the Python baseline byte-for-byte; see
// apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:48-61 (encode)
// and apps/windows/rc003/src/ovb_rc003/atvv_session.py:178-223
// (decode field extraction only; state machine stays in Python).

#include "remotemic/atvv/control.hpp"

namespace remotemic::atvv {

std::optional<ControlMessage> parse_control_message(
    std::span<const std::uint8_t>) noexcept {
    return std::nullopt;
}

std::vector<std::uint8_t> mic_open_command(std::uint16_t) {
    return {};
}

std::vector<std::uint8_t> mic_close_command(
    std::uint16_t, std::uint8_t) {
    return {};
}

}  // namespace remotemic::atvv