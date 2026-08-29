// Phase 2 / Area 3: TDD unit tests for the IMA/DVI ADPCM decoder
// (ADR-0012 section 3 / section 8).
//
// Reads the same JSON golden fixtures that the Area 1 capability tests
// and Area 2 control tests read; the Python shadow parity test for
// Area 3 (step 5) reads the same files.
//
// On the stub implementation:
//   - reset() leaves predictor / step_index at 0
//   - decode() returns {} (empty vector) for any input
//   - predictor() / step_index() always return 0
//
// All 11 fixtures fail on the stub; step 3 implements the real
// decoder and all 11 pass.

#include "remotemic/adpcm/ima_decoder.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using json = nlohmann::json;
using remotemic::adpcm::ImaDecoder;

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

void run_decode_fixture(const std::string& name) {
    json fixture;
    if (!load_fixture(name, fixture)) {
        expect(false, "could not locate fixture " + name);
        return;
    }

    const auto input = hex_to_bytes(fixture.at("input_hex").get<std::string>());

    ImaDecoder decoder;
    if (fixture.contains("reset")) {
        const auto& r = fixture.at("reset");
        decoder.reset(
            r.at("predictor").get<std::int16_t>(),
            r.at("step_index").get<std::uint8_t>());
    }

    const auto actual = decoder.decode(input);

    // expected_pcm is an array of ints. We cast each to int16_t for
    // element-wise comparison; the JSON uses signed integers.
    const auto& expected = fixture.at("expected_pcm");
    if (actual.size() != expected.size()) {
        expect(false, "fixture " + name + ": decoded "
               + std::to_string(actual.size()) + " samples, expected "
               + std::to_string(expected.size()));
        return;
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const auto exp_value =
            static_cast<std::int16_t>(expected.at(i).get<int>());
        if (actual[i] != exp_value) {
            expect(false, "fixture " + name + ": sample[" + std::to_string(i)
                   + "] = " + std::to_string(actual[i])
                   + ", expected " + std::to_string(exp_value));
            return;
        }
    }
}

}  // namespace

int main() {
    // Each fixture covers a different aspect of the decoder:
    //   empty:                       inner-loop boundary
    //   single-byte-zero-state:      one iteration of the decode path
    //   four-byte-zero-state:        multi-byte, matches Phase 0 fixture
    //   all-positive-nibbles:        monotonic predictor rise
    //   all-negative-nibbles:        monotonic predictor fall
    //   round-trip-ramp:             encoder/decoder self-consistency
    //   clamp-predictor-high:        predictor clamps to +32767
    //   clamp-predictor-low:         predictor clamps to -32768
    //   clamp-step-index:            step_index clamps at 0
    //   clamp-step-index-high:       step_index clamps at 88
    //   reset-nonzero-state:         AUDIO_SYNC priming path
    run_decode_fixture("adpcm-empty.json");
    run_decode_fixture("adpcm-single-byte-zero-state.json");
    run_decode_fixture("adpcm-four-byte-zero-state.json");
    run_decode_fixture("adpcm-all-positive-nibbles.json");
    run_decode_fixture("adpcm-all-negative-nibbles.json");
    run_decode_fixture("adpcm-round-trip-ramp.json");
    run_decode_fixture("adpcm-clamp-predictor-high.json");
    run_decode_fixture("adpcm-clamp-predictor-low.json");
    run_decode_fixture("adpcm-clamp-step-index.json");
    run_decode_fixture("adpcm-clamp-step-index-high.json");
    run_decode_fixture("adpcm-reset-nonzero-state.json");

    if (failures == 0) {
        std::cout << "All adpcm_ima tests passed\n";
        return 0;
    }
    std::cerr << failures << " adpcm_ima test(s) failed\n";
    return 1;
}