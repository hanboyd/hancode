// Phase 2 / Area 2: TDD unit tests for ATVV control message
// encoding + decoding (ADR-0012 §3 / §8).
//
// Reads the same JSON golden fixtures that the C++ capability tests
// (Area 1) read; the Python shadow parity test (Area 2 step 5) reads
// the same files.
//
// On the stub implementation:
//   - empty decode fixture passes (parse returns nullopt == expected null)
//   - 7 valid decode fixtures FAIL (parse returns nullopt, expected a value)
//   - 4 encode fixtures FAIL (mic_open/close return empty vector,
//     expected specific bytes)
//
// Step 3 implements the real parser/encoders; all 12 fixtures pass.

#include "remotemic/atvv/control.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

using json = nlohmann::json;
using remotemic::atvv::AudioStartPayload;
using remotemic::atvv::AudioSyncPayload;
using remotemic::atvv::CapsPayload;
using remotemic::atvv::ControlMessage;
using remotemic::atvv::MicButtonPayload;
using remotemic::atvv::Opcode;
using remotemic::atvv::UnknownPayload;
using remotemic::atvv::mic_close_command;
using remotemic::atvv::mic_open_command;
using remotemic::atvv::parse_control_message;

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::vector<std::uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        const auto byte = static_cast<std::uint8_t>(
            std::stoi(hex.substr(i, 2), nullptr, 16));
        out.push_back(byte);
    }
    return out;
}

std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto b : bytes) {
        out.push_back(kHex[(b >> 4) & 0x0F]);
        out.push_back(kHex[b & 0x0F]);
    }
    return out;
}

bool load_fixture(const std::string& name, json& out) {
    const std::vector<std::string> roots = {
        ".",
        "..",
        "../..",
        "../../..",
        "../../../..",
        "../../../../..",
    };
    for (const auto& root : roots) {
        std::string p = root;
        p += "/apps/windows/rc003/tests/fixtures/atvv/";
        p += name;
        std::ifstream f(p);
        if (f.good()) {
            f >> out;
            return true;
        }
    }
    return false;
}

// Helper: assert that a variant holds T and that get<T>() matches
// expected. We don't need a general visitor because the per-opcode
// payload fields are small and known.
template <typename T>
bool variant_matches(
    const ControlMessage& msg,
    const std::function<bool(const T&)>& predicate) {
    if (!std::holds_alternative<T>(msg)) return false;
    return predicate(std::get<T>(msg));
}

void run_encode_fixture(const std::string& name) {
    json fixture;
    if (!load_fixture(name, fixture)) {
        expect(false, "could not locate fixture " + name);
        return;
    }
    const auto func = fixture.at("function").get<std::string>();
    const auto& input = fixture.at("input");
    const auto expected_hex = fixture.at("expected_hex").get<std::string>();
    const auto expected = hex_to_bytes(expected_hex);

    std::vector<std::uint8_t> actual;
    if (func == "mic_open_command") {
        const auto version = input.at("version").get<std::uint16_t>();
        actual = mic_open_command(version);
    } else if (func == "mic_close_command") {
        const auto version = input.at("version").get<std::uint16_t>();
        const auto sid = input.at("session_id").get<std::uint8_t>();
        actual = mic_close_command(version, sid);
    } else {
        expect(false, "fixture " + name + ": unknown function " + func);
        return;
    }

    expect(actual == expected,
           "fixture " + name + ": encode() = " + bytes_to_hex(actual)
           + " != expected " + expected_hex);
}

void run_decode_fixture(const std::string& name) {
    json fixture;
    if (!load_fixture(name, fixture)) {
        expect(false, "could not locate fixture " + name);
        return;
    }
    const auto payload = hex_to_bytes(
        fixture.at("input_hex").get<std::string>());
    const auto parsed = parse_control_message(payload);

    if (fixture.contains("expected") && fixture.at("expected").is_null()) {
        // Empty-payload reject fixture: parser MUST return nullopt.
        expect(!parsed.has_value(),
               "fixture " + name + ": expected parse()=nullopt, got a value");
        return;
    }

    if (!parsed.has_value()) {
        expect(false,
               "fixture " + name + ": expected parse() to succeed, got nullopt");
        return;
    }

    const auto expected_opcode =
        fixture.at("expected_opcode").get<std::string>();

    if (expected_opcode == "Caps") {
        expect(std::holds_alternative<CapsPayload>(*parsed),
               "fixture " + name + ": expected CapsPayload variant");
        return;
    }
    if (expected_opcode == "MicButton") {
        expect(std::holds_alternative<MicButtonPayload>(*parsed),
               "fixture " + name + ": expected MicButtonPayload variant");
        return;
    }
    if (expected_opcode == "AudioStop") {
        // No nested fields beyond the variant check (the state machine
        // needs only the opcode to dispatch).
        expect(std::holds_alternative<remotemic::atvv::AudioStopPayload>(*parsed),
               "fixture " + name + ": expected AudioStopPayload variant");
        return;
    }
    if (expected_opcode == "AudioStart") {
        const auto& expected_sid_field =
            fixture.at("expected_session_id");
        if (expected_sid_field.is_null()) {
            expect(
                variant_matches<AudioStartPayload>(
                    *parsed,
                    [](const AudioStartPayload& p) {
                        return !p.session_id.has_value();
                    }),
                "fixture " + name + ": expected AudioStart with no session_id");
        } else {
            const auto expected_sid =
                expected_sid_field.get<std::uint8_t>();
            expect(
                variant_matches<AudioStartPayload>(
                    *parsed,
                    [expected_sid](const AudioStartPayload& p) {
                        return p.session_id.has_value()
                            && *p.session_id == expected_sid;
                    }),
                "fixture " + name + ": expected AudioStart session_id="
                + std::to_string(expected_sid));
        }
        return;
    }
    if (expected_opcode == "AudioSync") {
        const auto expected_predictor =
            fixture.at("expected_predictor").get<std::int16_t>();
        const auto expected_step_index =
            fixture.at("expected_step_index").get<std::uint8_t>();
        expect(
            variant_matches<AudioSyncPayload>(
                *parsed,
                [expected_predictor, expected_step_index](
                    const AudioSyncPayload& p) {
                    return p.predictor == expected_predictor
                        && p.step_index == expected_step_index;
                }),
            "fixture " + name + ": expected AudioSync predictor="
            + std::to_string(expected_predictor)
            + " step_index=" + std::to_string(expected_step_index));
        return;
    }
    if (expected_opcode == "Unknown") {
        const auto expected_raw =
            fixture.at("expected_raw_opcode").get<std::uint8_t>();
        expect(
            variant_matches<UnknownPayload>(
                *parsed,
                [expected_raw](const UnknownPayload& p) {
                    return p.raw_opcode == expected_raw;
                }),
            "fixture " + name + ": expected Unknown raw_opcode="
            + std::to_string(expected_raw));
        return;
    }
    expect(false, "fixture " + name + ": unhandled expected_opcode "
           + expected_opcode);
}

}  // namespace

int main() {
    // Encode fixtures (host -> device).
    run_encode_fixture("control-mic-open-v1.json");
    run_encode_fixture("control-mic-open-legacy.json");
    run_encode_fixture("control-mic-close-v1.json");
    run_encode_fixture("control-mic-close-legacy.json");

    // Decode fixtures (device -> host payload).
    run_decode_fixture("control-decode-caps.json");
    run_decode_fixture("control-decode-mic-button.json");
    run_decode_fixture("control-decode-audio-start-with-sid.json");
    run_decode_fixture("control-decode-audio-start-no-sid.json");
    run_decode_fixture("control-decode-audio-stop.json");
    run_decode_fixture("control-decode-audio-sync.json");
    run_decode_fixture("control-decode-unknown.json");
    run_decode_fixture("control-decode-empty.json");

    if (failures == 0) {
        std::cout << "All atvv_control tests passed\n";
        return 0;
    }
    std::cerr << failures << " atvv_control test(s) failed\n";
    return 1;
}