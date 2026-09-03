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
#include <stdexcept>
#include <utility>

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

    // ------------------------------------------------------------------
    // Phase 2 / Area 4 step 4: reset() parity.
    //
    // Drive state into the filter, reset(), drive the SAME bytes
    // again, and compare to a freshly constructed filter fed the
    // same bytes. After reset() the post-reset output must equal
    // the fresh-instance output sample-for-sample. This proves
    // reset() really clears previous_input_ / previous_output_ /
    // initialized_ rather than only partially zeroing state.
    // ------------------------------------------------------------------
    {
        // Use the 100 Hz AC tone fixture: it carries non-trivial
        // state into the filter so any partial reset would be
        // detectable.
        json fixture;
        if (!load_fixture("dc-ac-passes.json", fixture)) {
            expect(false, "reset_parity: dc-ac-passes.json not found");
        } else {
            const auto& samples = fixture.at("samples");
            const std::size_t half = samples.size() / 2;
            std::vector<std::int16_t> first_half;
            std::vector<std::int16_t> second_half;
            first_half.reserve(half);
            second_half.reserve(samples.size() - half);
            for (std::size_t i = 0; i < half; ++i) {
                first_half.push_back(
                    static_cast<std::int16_t>(samples.at(i).get<int>()));
            }
            for (std::size_t i = half; i < samples.size(); ++i) {
                second_half.push_back(
                    static_cast<std::int16_t>(samples.at(i).get<int>()));
            }

            // Path A: drive the filter with the first half, reset(),
            // then drive again with the full tone. This must equal
            // a fresh-instance filter running on the full tone.
            DcHighPassFilter fa(16000.0, 20.0);
            (void)fa.process(first_half);
            fa.reset();
            const auto a_reset = fa.process(second_half);

            // Path B: a brand new filter driving only the
            // second_half (matched to the post-reset segment).
            DcHighPassFilter fb(16000.0, 20.0);
            const auto b_fresh = fb.process(second_half);

            expect(a_reset.size() == b_fresh.size(),
                   "reset_parity: post-reset output length differs from fresh instance");
            bool match = true;
            for (std::size_t i = 0; i < a_reset.size(); ++i) {
                if (a_reset[i] != b_fresh[i]) {
                    match = false;
                    break;
                }
            }
            expect(match,
                   "reset_parity: post-reset output must sample-equal fresh-instance output");
        }
    }

    {
        // Even stronger check: the AC full tone fed into a long-
        // lived filter, then reset(), then the AC full tone fed
        // again, must reproduce sample-exact.
        const std::vector<std::int16_t> six{100, -100, 200, -200, 300, -300};
        DcHighPassFilter runner(16000.0, 20.0);
        (void)runner.process(six);
        runner.reset();
        // Fresh instance on the same samples -> the result
        // captured below is the gold output that the post-reset
        // runner must reproduce.
        DcHighPassFilter gold(16000.0, 20.0);
        const auto gold_out = gold.process(six);
        const auto runner_out = runner.process(six);
        expect(runner_out == gold_out,
               "reset_parity_strong: post-reset multi-sample run must be sample-exact with a fresh instance");
    }

    {
        // reset() before any process() must keep the filter in the
        // same state as a freshly constructed one (alpha / sample
        // rate / cutoff preserved, but state == uninitialized).
        const std::vector<std::int16_t> samples{42, -42, 84, -84};
        DcHighPassFilter f(16000.0, 20.0);
        f.reset();
        const auto actual = f.process(samples);
        DcHighPassFilter fresh(16000.0, 20.0);
        const auto expected = fresh.process(samples);
        expect(actual == expected,
               "reset_parity_constructor: reset() on a never-used filter must match the constructor state");
    }

    for (const auto [sample_rate, cutoff_hz] :
         std::vector<std::pair<double, double>>{{0.0, 20.0}, {-1.0, 20.0},
                                                 {16000.0, 0.0}, {16000.0, -1.0}}) {
        bool rejected = false;
        try {
            DcHighPassFilter invalid(sample_rate, cutoff_hz);
            (void)invalid;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        expect(rejected, "non-positive filter parameters must be rejected");
    }

    if (failures == 0) {
        std::cout << "All adpcm_dc_highpass tests passed\n";
        return 0;
    }
    std::cerr << failures << " adpcm_dc_highpass test(s) failed\n";
    return 1;
}
