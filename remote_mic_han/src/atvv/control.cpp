// Phase 2 / Area 2: real implementation of ATVV control message
// encoding + decoding.
//
// Mirrors two Python surfaces (ADR-0012 §3 / §5):
//   - apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:48-61 for
//     mic_open_command / mic_close_command (host -> device encoders)
//   - apps/windows/rc003/src/ovb_rc003/atvv_session.py:178-223 for
//     the field extraction half of handle_control. The state machine
//     half (capability gate, decoder reset, sync predictor hold,
//     late-audio discard) stays in Python per ADR-0012 §2 / Non-scope.
//
// Validation gate G1/G2 for Area 2 (per ADR-0012):
//   ctest -C Debug   -R '^remotemic_atvv_control_tests\$' -> 1/1 Passed
//   ctest -C Release -R '^remotemic_atvv_control_tests\$' -> 1/1 Passed

#include "remotemic/atvv/control.hpp"

#include <cstdint>

namespace remotemic::atvv {

namespace {

constexpr std::uint8_t OPCODE_CAPS        = 0x0B;
constexpr std::uint8_t OPCODE_MIC_BUTTON  = 0x08;
constexpr std::uint8_t OPCODE_AUDIO_START = 0x04;
constexpr std::uint8_t OPCODE_AUDIO_STOP  = 0x00;
constexpr std::uint8_t OPCODE_AUDIO_SYNC  = 0x0A;

constexpr std::uint16_t VERSION_1_0 = 0x0100;

// The minimum payload length for an AUDIO_SYNC to carry predictor +
// step_index. Shorter AUDIO_SYNC payloads fall through to
// UnknownPayload per the contract in control.hpp.
constexpr std::size_t AUDIO_SYNC_MIN_LEN = 7;

}  // namespace

std::optional<ControlMessage> parse_control_message(
    std::span<const std::uint8_t> data) noexcept {
    // Empty payload -> nullopt; the state machine raises
    // ATVVProtocolError on nullopt (see atvv_session.handle_control
    // line 179-180). The parser itself never throws.
    if (data.empty()) {
        return std::nullopt;
    }

    const auto opcode = data[0];

    if (opcode == OPCODE_CAPS) {
        return CapsPayload{};
    }
    if (opcode == OPCODE_MIC_BUTTON) {
        return MicButtonPayload{};
    }
    if (opcode == OPCODE_AUDIO_STOP) {
        return remotemic::atvv::AudioStopPayload{};
    }
    if (opcode == OPCODE_AUDIO_START) {
        // Matches Python:
        //   session_id = payload[3] if len(payload) >= 4 else None
        std::optional<std::uint8_t> session_id;
        if (data.size() >= 4) {
            session_id = data[3];
        }
        return AudioStartPayload{ .session_id = session_id };
    }
    if (opcode == OPCODE_AUDIO_SYNC) {
        // Matches Python's guarded branch in handle_control:
        //   if opcode == OPCODE_AUDIO_SYNC and len(payload) >= 7:
        //       predictor = int.from_bytes(payload[4:6], 'big',
        //           signed=True)
        //       step_index = payload[6]
        //       ...
        // A too-short AUDIO_SYNC falls through to UnknownPayload so
        // the state machine sees a uniform value and can decide
        // whether to discard; the parser itself does not silently
        // drop or raise.
        if (data.size() >= AUDIO_SYNC_MIN_LEN) {
            const auto predictor = static_cast<std::int16_t>(
                (static_cast<std::uint16_t>(data[4]) << 8)
                | static_cast<std::uint16_t>(data[5]));
            return AudioSyncPayload{
                .predictor  = predictor,
                .step_index = data[6],
            };
        }
        return UnknownPayload{ .raw_opcode = OPCODE_AUDIO_SYNC };
    }

    // Unrecognized opcode byte -> UnknownPayload with the raw byte
    // preserved verbatim so logging / future-extension code can see
    // exactly what the device sent.
    return UnknownPayload{ .raw_opcode = opcode };
}

std::vector<std::uint8_t> mic_open_command(std::uint16_t version) {
    if (version >= VERSION_1_0) {
        return {0x0C, 0x00};
    }
    return {0x0C, 0x00, 0x00};
}

std::vector<std::uint8_t> mic_close_command(
    std::uint16_t version, std::uint8_t session_id) {
    if (version >= VERSION_1_0) {
        return {0x0D, static_cast<std::uint8_t>(session_id & 0xFF)};
    }
    return {0x0D};
}

}  // namespace remotemic::atvv