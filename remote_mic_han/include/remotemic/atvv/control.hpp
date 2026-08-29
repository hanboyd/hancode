// Phase 2 / Area 2: ATVV control message encoding + decoding
// (pure compute).
//
// Per ADR-0012 §3, this header declares:
//   - The five real control-channel opcodes (Caps / MicButton /
//     AudioStart / AudioStop / AudioSync) plus an Unknown sentinel
//     that carries the raw byte for any opcode the device sent that
//     isn't recognized.
//   - A std::variant<ControlMessage> of one struct per opcode so the
//     state machine in ovb_rc003.atvv_session can dispatch on the
//     type without re-parsing the raw bytes.
//   - parse_control_message(std::span<const std::uint8_t>) ->
//     std::optional<ControlMessage> - a pure field parser; the
//     state machine owns the lifecycle, this only extracts fields.
//   - mic_open_command / mic_close_command - host -> device encoding,
//     byte-for-byte identical to the Python helpers in
//     apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:48-61.
//
// Contract:
//   - No I/O, no threads, no globals.
//   - Empty payload -> std::nullopt (the state machine raises
//     ATVVProtocolError on nullopt; the parser itself does not throw).
//   - Unknown opcodes are NEVER rejected: they become UnknownPayload
//     with the raw byte preserved, so a future device firmware that
//     adds a new opcode doesn't immediately break the parser - the
//     state machine gets a uniform value and decides whether to
//     ignore it.
//   - AudioSync payload must be >= 7 bytes; shorter AudioSync
//     payloads become UnknownPayload (raw_opcode = 0x0A) so the
//     state machine can choose how to react (e.g. discard).
//   - AudioStart session_id is read from payload[3] when the payload
//     is at least 4 bytes long, otherwise std::nullopt.

#ifndef REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_CONTROL_HPP
#define REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_CONTROL_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace remotemic::atvv {

// The five real opcodes that share the VOICE_CONTROL_UUID notification
// channel with CAPS. Values are the byte that appears as payload[0].
// ``Unknown`` is a sentinel only - it never appears as payload[0]; the
// actual unknown byte lives inside UnknownPayload::raw_opcode.
enum class Opcode : std::uint8_t {
    Caps       = 0x0B,
    MicButton  = 0x08,
    AudioStart = 0x04,
    AudioStop  = 0x00,
    AudioSync  = 0x0A,
    Unknown    = 0xFF,
};

// Per-opcode payload structs. Kept trivially destructible so the
// enclosing std::variant is a value type with no hidden ownership.

struct CapsPayload {};

struct MicButtonPayload {};

struct AudioStartPayload {
    // Matches the Python branch in atvv_session.handle_control:
    // ``session_id = payload[3] if len(payload) >= 4 else None``.
    std::optional<std::uint8_t> session_id;
};

struct AudioStopPayload {};

struct AudioSyncPayload {
    // Python: ``predictor = int.from_bytes(payload[4:6], 'big',
    // signed=True); step_index = payload[6]``. The ``>= 7`` length
    // gate is enforced by parse_control_message; if the payload is
    // shorter the message comes back as UnknownPayload instead.
    std::int16_t predictor;
    std::uint8_t step_index;
};

struct UnknownPayload {
    // The first byte of the original payload, preserved verbatim so
    // logging / future-extension code can see what the device sent.
    std::uint8_t raw_opcode;
};

using ControlMessage = std::variant<
    CapsPayload,
    MicButtonPayload,
    AudioStartPayload,
    AudioStopPayload,
    AudioSyncPayload,
    UnknownPayload
>;

// Pure field parser. Empty payload -> nullopt; any other payload
// (including unknown opcodes) -> a populated ControlMessage. Never
// throws.
std::optional<ControlMessage> parse_control_message(
    std::span<const std::uint8_t> data) noexcept;

// Host -> device encoders. Return values match the Python helpers
// (atvv_protocol.py:48-61) byte-for-byte:
//   mic_open_command(0x0100) == {0x0C, 0x00}
//   mic_open_command(0x0000) == {0x0C, 0x00, 0x00}
//   mic_close_command(0x0100, sid) == {0x0D, sid & 0xFF}
//   mic_close_command(0x0000, _)   == {0x0D}
std::vector<std::uint8_t> mic_open_command(std::uint16_t version);
std::vector<std::uint8_t> mic_close_command(
    std::uint16_t version, std::uint8_t session_id);

}  // namespace remotemic::atvv

#endif  // REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_CONTROL_HPP