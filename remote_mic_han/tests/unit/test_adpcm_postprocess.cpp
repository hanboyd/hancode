// Phase 2 / Area 4: TDD unit tests for postprocess
// (ADR-0012 section 3 / section 8).
//
// Reads the same JSON golden fixtures that the Area 4 shadow parity
// test (step 5) reads.
//
// On the stub implementation:
//   - postprocess() returns {} for any input
//
// All 10 fixtures fail on the stub; step 3 implements the real
// function and all 10 pass.

#include "remotemic/adpcm/postprocess.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using json = nlohmann::json;
using remotemic::adpcm::postprocess;

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
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

// The fixture stores gain_db as a number OR the literal string
// "NaN" / "Infinity" (JSON doesn't have a native NaN encoding).
double read_gain_db(const json& value) {
    if (value.is_number()) {
        return value.get<double>();
    }
    if (value.is_string()) {
        const auto s = value.get<std::string>();
        if (s == "NaN") return std::nan("");
        if (s == "Infinity" || s == "+Infinity") {
            return std::numeric_limits<double>::infinity();
        }
        if (s == "-Infinity") {
            return -std::numeric_limits<double>::infinity();
        }
    }
    throw std::runtime_error("unparseable gain_db");
}

void run_postprocess_fixture(const std::string& name) {
    json fixture;
    if (!load_fixture(name, fixture)) {
        expect(false, "could not locate fixture " + name);
        return;
    }

    const auto& samples = fixture.at("samples");
    std::vector<std::int16_t> input;
    input.reserve(samples.size());
    for (const auto& s : samples) {
        input.push_back(static_cast<std::int16_t>(s.get<int>()));
    }
    const auto gain_db = read_gain_db(fixture.at("gain_db"));

    const auto actual = postprocess(input, gain_db);

    const auto& expected = fixture.at("expected_output");
    if (actual.size() != expected.size()) {
        expect(false, "fixture " + name + ": postprocessed "
               + std::to_string(actual.size()) + " samples, expected "
               + std::to_string(expected.size()));
        return;
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const auto exp_value =
            static_cast<std::int16_t>(expected.at(i).get<int>());
        if (actual[i] != exp_value) {
            expect(false, "fixture " + name + ": output["
                   + std::to_string(i) + "] = " + std::to_string(actual[i])
                   + ", expected " + std::to_string(exp_value));
            return;
        }
    }
}

}  // namespace

int main() {
    // Each fixture covers a different branch of postprocess:
    //   empty:                         inner-loop boundary
    //   single-default-gain:           1 sample + 10 dB
    //   zero-gain:                     smoothing visible + gain=1
    //   max-gain:                      +24 dB
    //   min-gain:                      -24 dB
    //   gain-clamps-above-24:          gain_db=100 -> 24
    //   gain-nan:                      NaN -> 0 dB
    //   gain-inf:                      +inf -> 0 dB
    //   clamp-to-int16:                output saturates at +32767
    //   two-samples-no-smoothing:      len < 3 skips smoothing
    run_postprocess_fixture("postprocess-empty.json");
    run_postprocess_fixture("postprocess-single-default-gain.json");
    run_postprocess_fixture("postprocess-zero-gain.json");
    run_postprocess_fixture("postprocess-max-gain.json");
    run_postprocess_fixture("postprocess-min-gain.json");
    run_postprocess_fixture("postprocess-gain-clamps-above-24.json");
    run_postprocess_fixture("postprocess-gain-nan.json");
    run_postprocess_fixture("postprocess-gain-inf.json");
    run_postprocess_fixture("postprocess-clamp-to-int16.json");
    run_postprocess_fixture("postprocess-two-samples-no-smoothing.json");

    // Regression: every 3-tap window must read the original input,
    // not the previously smoothed output.  The old recursive loop
    // produced [1000, 0, 500, -625, 843, -6000].
    const std::vector<std::int16_t> alternating = {
        1000, -2000, 3000, -4000, 5000, -6000};
    const std::vector<std::int16_t> alternating_expected = {
        1000, 0, 0, 0, 0, -6000};
    expect(
        postprocess(alternating, 0.0) == alternating_expected,
        "3-tap smoothing reads immutable source samples");

    if (failures == 0) {
        std::cout << "All adpcm_postprocess tests passed\n";
        return 0;
    }
    std::cerr << failures << " adpcm_postprocess test(s) failed\n";
    return 1;
}
