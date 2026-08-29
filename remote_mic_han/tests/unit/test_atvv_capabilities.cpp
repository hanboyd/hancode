// Phase 2 / Area 1: TDD unit tests for ATVV capability parse.
//
// Reads the same JSON golden fixtures that
// apps/windows/rc003/tests/test_atvv_golden_fixture.py reads, and
// asserts byte-exact field equality against remotemic::atvv::parse.
//
// All fixtures are 100% synthetic (no captured device or voice data);
// see ADR-0012 §4.
//
// On the stub implementation (parse always returns std::nullopt):
//   - reject fixtures pass (parse returns nullopt as expected)
//   - valid fixtures FAIL (parse returns nullopt, but expected_* is set)
// This is the intentional TDD red state at the end of step 2; step 3
// implements parse() so all fixtures pass.

#include "remotemic/atvv/capabilities.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using json = nlohmann::json;
using remotemic::atvv::Capabilities;
using remotemic::atvv::parse;

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

// Locate fixture file by walking up candidate roots until we find
// apps/windows/rc003/tests/fixtures/atvv/<name>.
bool load_fixture(const std::string& name, json& out) {
    const std::vector<std::string> candidate_roots = {
        ".",
        "..",
        "../..",
        "../../..",
        "../../../..",
        "../../../../..",
    };
    for (const auto& root : candidate_roots) {
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

void run_fixture(const std::string& name) {
    json fixture;
    if (!load_fixture(name, fixture)) {
        expect(false, "could not locate fixture " + name);
        return;
    }
    const auto payload = hex_to_bytes(
        fixture.at("capabilities_hex").get<std::string>());
    const auto parsed = parse(payload);

    const bool expects_nullopt =
        fixture.contains("expected") && fixture.at("expected").is_null();

    if (expects_nullopt) {
        expect(!parsed.has_value(),
               "fixture " + name + ": expected parse()=nullopt, got a value");
        return;
    }

    if (!parsed.has_value()) {
        expect(false, "fixture " + name + ": expected parse() to succeed, got nullopt");
        return;
    }

    const auto& expected_version      = fixture.at("expected_version").get<std::uint16_t>();
    const auto& expected_codecs       = fixture.at("expected_codecs").get<std::uint8_t>();
    const auto& expected_interaction  = fixture.at("expected_interaction").get<std::uint8_t>();
    const auto& expected_frame_size   = fixture.at("expected_frame_size").get<std::uint16_t>();
    const auto& expected_codec        = fixture.at("expected_codec").get<std::uint8_t>();
    const auto& expected_sample_rate  = fixture.at("expected_sample_rate").get<double>();

    expect(parsed->version == expected_version,
           "fixture " + name + ": version " + std::to_string(parsed->version)
           + " != expected " + std::to_string(expected_version));
    expect(parsed->codecs == expected_codecs,
           "fixture " + name + ": codecs " + std::to_string(parsed->codecs)
           + " != expected " + std::to_string(expected_codecs));
    expect(parsed->interaction == expected_interaction,
           "fixture " + name + ": interaction " + std::to_string(parsed->interaction)
           + " != expected " + std::to_string(expected_interaction));
    expect(parsed->frame_size == expected_frame_size,
           "fixture " + name + ": frame_size " + std::to_string(parsed->frame_size)
           + " != expected " + std::to_string(expected_frame_size));
    expect(parsed->selected_codec == expected_codec,
           "fixture " + name + ": selected_codec " + std::to_string(parsed->selected_codec)
           + " != expected " + std::to_string(expected_codec));
    expect(parsed->sample_rate == expected_sample_rate,
           "fixture " + name + ": sample_rate " + std::to_string(parsed->sample_rate)
           + " != expected " + std::to_string(expected_sample_rate));
}

}  // namespace

int main() {
    // Each fixture is one independent sub-test; running them all from
    // a single main() matches the project's hand-rolled test style
    // (see tests/unit/core_tests.cpp) and avoids a GoogleTest dep.
    run_fixture("synthetic-v1.json");
    run_fixture("synthetic-v1-8k-fallback.json");
    run_fixture("synthetic-v1-zero-frame-size.json");
    run_fixture("synthetic-v1-zero-codecs-quirk.json");
    run_fixture("synthetic-legacy-pre-1.0.json");
    run_fixture("synthetic-legacy-rejects-short.json");
    run_fixture("synthetic-wrong-opcode.json");
    run_fixture("synthetic-short-payload.json");

    if (failures == 0) {
        std::cout << "All atvv_capabilities tests passed\n";
        return 0;
    }
    std::cerr << failures << " atvv_capabilities test(s) failed\n";
    return 1;
}