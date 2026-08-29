// Phase 2 / Area 1: ATVV capability parse (pure compute).
//
// Per ADR-0012 §3, this header declares the C++ value type and parser
// for ATVV capability notifications (opcode 0x0B on VOICE_CONTROL_UUID).
// The Python baseline lives in
// apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:65-101 and the
// golden fixtures under apps/windows/rc003/tests/fixtures/atvv/ are
// the single source of truth.
//
// Contract:
//   - parse() is a pure function. No I/O, no threads, no global state.
//   - On any malformed payload (too short, wrong opcode, or legacy
//     version with insufficient length) parse() returns std::nullopt;
//     it does NOT throw.
//   - The Capabilities struct is a value type with trivial destructor
//     and trivially copyable semantics; safe to pass by value.
//   - All fields map 1:1 to the Python ATVVCapabilities dataclass
//     (apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:64-71).

#ifndef REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_CAPABILITIES_HPP
#define REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_CAPABILITIES_HPP

#include <cstdint>
#include <optional>
#include <span>

namespace remotemic::atvv {

struct Capabilities {
    std::uint16_t version;        // big-endian u16 from bytes [1..2]
    std::uint8_t  codecs;         // raw codecs byte (post-quirk if any)
    std::uint8_t  interaction;    // raw interaction byte (post-quirk if any)
    std::uint16_t frame_size;     // big-endian u16 from bytes [5..6]; 0 -> 120
    std::uint8_t  selected_codec; // derived: 0x02 if (codecs & 0x02), else 0x01
    double        sample_rate;    // derived: 16000.0 if selected_codec == 0x02 else 8000.0
};

// Parses an ATVV capability notification payload.
//
// Returns std::nullopt (does NOT throw) if:
//   - data.size() < 7, or
//   - data[0] != OPCODE_CAPS (0x0B), or
//   - data is a legacy (version < 0x0100) payload shorter than 9 bytes
//
// Otherwise returns a fully-populated Capabilities value matching the
// Python ATVVCapabilities.parse contract byte-for-byte.
std::optional<Capabilities> parse(std::span<const std::uint8_t> data) noexcept;

// Control-channel opcodes that share VOICE_CONTROL_UUID with CAPS.
// Listed here only because Capabilities::parse needs OPCODE_CAPS.
// The decoder side (Area 2) extends this list with the remaining opcodes.
inline constexpr std::uint8_t OPCODE_CAPS = 0x0B;

}  // namespace remotemic::atvv

#endif  // REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_CAPABILITIES_HPP