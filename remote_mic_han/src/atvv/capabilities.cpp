// Phase 2 / Area 1: real implementation of ATVV capability parse.
//
// Mirrors apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:73-101
// (ATVVCapabilities.parse) byte-for-byte. The Python dataclass
// becomes the C++ value type remotemic::atvv::Capabilities
// (declared in include/remotemic/atvv/capabilities.hpp).
//
// Validation gate G1/G2 (per ADR-0012):
//   ctest -C Debug -R '^remotemic_atvv_tests\$' -> 1/1 passed
//   ctest -C Release -R '^remotemic_atvv_tests\$' -> 1/1 passed
//
// Step 3 replaces the step-2 stub. The fixture loader, hex decoder,
// and field-by-field comparison path are unchanged from step 2; only
// the body of parse() changes.

#include "remotemic/atvv/capabilities.hpp"

namespace remotemic::atvv {

namespace {

// The Python DEFAULT_FRAME_SIZE constant (atvv_protocol.py:34).
constexpr std::uint16_t DEFAULT_FRAME_SIZE = 120;

constexpr std::uint8_t OPCODE_CAPS_VALUE = 0x0B;

// The version threshold above which a v1 capability payload is allowed
// to be 7 bytes long. Below it, the payload MUST be at least 9 bytes
// (legacy quirk) or it is rejected.
constexpr std::uint16_t VERSION_1_0 = 0x0100;

constexpr double SAMPLE_RATE_16K = 16000.0;
constexpr double SAMPLE_RATE_8K  = 8000.0;

}  // namespace

std::optional<Capabilities> parse(std::span<const std::uint8_t> data) noexcept {
    // ---- Length / opcode gate (matches Python: ``if len(data) < 7 or
    //      data[0] != OPCODE_CAPS: return None``).
    if (data.size() < 7) {
        return std::nullopt;
    }
    if (data[0] != OPCODE_CAPS_VALUE) {
        return std::nullopt;
    }

    // ---- version = (data[1] << 8) | data[2]
    const auto version = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[1]) << 8) |
         static_cast<std::uint16_t>(data[2]));

    std::uint8_t codecs;
    std::uint8_t interaction;

    if (version >= VERSION_1_0) {
        // ---- v1 path: codecs = data[3]; interaction = data[4]
        codecs = data[3];
        interaction = data[4];
        // ---- upstream quirk: if codecs byte is 0 but byte[4] has any
        //      of the low 2 bits set and the payload is at least 9
        //      bytes long, re-read byte[4] as codecs and force
        //      interaction = 0x03.
        if (codecs == 0 && data.size() >= 9 && (data[4] & 0x03) != 0) {
            codecs = data[4];
            interaction = 0x03;
        }
    } else {
        // ---- legacy (pre-1.0) path: requires len >= 9; codecs =
        //      data[4]; interaction forced to 0x00.
        if (data.size() < 9) {
            return std::nullopt;
        }
        codecs = data[4];
        interaction = 0x00;
    }

    // ---- frame_size = (data[5] << 8) | data[6]; 0 -> DEFAULT_FRAME_SIZE.
    const auto raw_frame_size = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[5]) << 8) |
         static_cast<std::uint16_t>(data[6]));
    const auto frame_size = raw_frame_size != 0
                                ? raw_frame_size
                                : DEFAULT_FRAME_SIZE;

    // ---- selected_codec / sample_rate derivation (matches Python).
    const auto selected_codec = (codecs & 0x02) != 0
                                    ? static_cast<std::uint8_t>(0x02)
                                    : static_cast<std::uint8_t>(0x01);
    const auto sample_rate = selected_codec == 0x02
                                 ? SAMPLE_RATE_16K
                                 : SAMPLE_RATE_8K;

    return Capabilities{
        .version        = version,
        .codecs         = codecs,
        .interaction    = interaction,
        .frame_size     = frame_size,
        .selected_codec = selected_codec,
        .sample_rate    = sample_rate,
    };
}

}  // namespace remotemic::atvv