// Phase 2 / Area 4: TDD unit tests for DcHighPassFilter
// (ADR-0012 section 3 / section 8).
//
// Reads the same JSON golden fixtures that the Area 4 shadow parity
// test (step 5) reads. The Python shadow parity check needs to
// compare the C++ output to the Python baseline for every fixture
// with zero tolerance.
//
// On the stub implementation:
//   - process() returns {} for any input
//
// All 5 fixtures fail on the stub; step 3 implements the real filter
// and all 5 pass.

#include "remotemic/adpcm/dc_highpass.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using json = nlohmann::json;
using remotemic::adpcm::DcHighPassFilter;

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

void run_dc_fixture(const std::string& name) {
    json fixture;
    if (!load_fixture(name, fixture)) {
        expect(false, "could not locate fixture " + name);
        return;
    }

    // The fixture doesn't carry sample_rate / cutoff_hz; the Python
    // baseline defaults are sample_rate=16000, cutoff_hz=20 (see
    // atvv_protocol.py:213 and SUPPORTED_SAMPLE_RATE_HZ=16000).
    DcHighPassFilter f(16000.0, 20.0);
    f.reset();

    const auto& samples = fixture.at("samples");
    std::vector<std::int16_t> input;
    input.reserve(samples.size());
    for (const auto& s : samples) {
        input.push_back(static_cast<std::int16_t>(s.get<int>()));
    }

    const auto actual = f.process(input);

    const auto& expected = fixture.at("expected_filtered");
    if (actual.size() != expected.size()) {
        expect(false, "fixture " + name + ": filtered "
               + std::to_string(actual.size()) + " samples, expected "
               + std::to_string(expected.size()));
        return;
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const auto exp_value =
            static_cast<std::int16_t>(expected.at(i).get<int>());
        if (actual[i] != exp_value) {
            expect(false, "fixture " + name + ": filtered["
                   + std::to_string(i) + "] = " + std::to_string(actual[i])
                   + ", expected " + std::to_string(exp_value));
            return;
        }
    }
}

}  // namespace

int main() {
    // Each fixture covers a different aspect of the DC filter:
    //   empty:           inner-loop boundary
    //   single-sample:   self-initialization (first sample == input)
    //   two-samples:     first real filter step with alpha
    //   dc-blocked:      constant input -> output converges to 0
    //   ac-passes:       100 Hz tone passes through the high-pass
    run_dc_fixture("dc-empty.json");
    run_dc_fixture("dc-single-sample.json");
    run_dc_fixture("dc-two-samples.json");
    run_dc_fixture("dc-dc-blocked.json");
    run_dc_fixture("dc-ac-passes.json");

    if (failures == 0) {
        std::cout << "All adpcm_dc_highpass tests passed\n";
        return 0;
    }
    std::cerr << failures << " adpcm_dc_highpass test(s) failed\n";
    return 1;
}