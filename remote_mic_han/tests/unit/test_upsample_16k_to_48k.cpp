// Phase 4 / ADR-0014 §3.3: Upsample16kTo48k TDD red-state tests.
//
// Stub behavior: always returns an empty vector. Tests below assert
// the real contract: 3x expansion with three-tap linear interpolation
// byte-aligned with audio_playback.py:154-172. They FAIL on the stub;
// step 2 turns them green.

#include "remotemic/audio/upsample_16k_to_48k.hpp"

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

using remotemic::audio::UpsampleState;
using remotemic::audio::upsample_16k_to_48k;

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_eq_int(std::int16_t got, std::int16_t expected, const char* message) {
    if (got != expected) {
        std::cerr << "FAIL: " << message << " (got " << got
                  << ", expected " << expected << ")\n";
        ++failures;
    }
}

void test_empty_source_returns_empty() {
    UpsampleState s;
    std::vector<std::int16_t> src{};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect(out.empty(),
           "upsample(empty source) returns empty vector");
}

void test_first_sample_no_previous_initializes_state() {
    // No previous_sample -> state.have_previous=false -> output is
    // just three copies of the source sample (Python baseline behavior:
    // previous defaults to values[0]).
    UpsampleState s;
    std::vector<std::int16_t> src{1000};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect(out.size() == 3,
           "single source -> 3 output samples (3x expansion)");
    expect_eq_int(out[0], 1000, "first output is the source sample");
    expect_eq_int(out[1], 1000, "second output is the source sample");
    expect_eq_int(out[2], 1000, "third output is the source sample");
    expect(s.have_previous, "state.have_previous becomes true after first sample");
    expect_eq_int(s.previous_sample, 1000,
                  "state.previous_sample is the first source sample");
}

void test_two_samples_produce_six_outputs() {
    UpsampleState s;
    std::vector<std::int16_t> src{0, 1000};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect(out.size() == 6,
           "two source samples -> 6 output samples");
}

void test_constant_source_yields_three_copies_per_input() {
    UpsampleState s;
    std::vector<std::int16_t> src{500, 500, 500};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect(out.size() == 9, "three constant samples -> 9 outputs");
    for (std::size_t i = 0; i < 9; ++i) {
        expect_eq_int(out[i], 500, "all outputs equal the source");
    }
}

void test_step_input_matches_python_baseline() {
    // Source {0, 1000}. Per audio_playback.py:154-172 (no previous
    // state yet, so previous defaults to values[0] = 0):
    //   i=0: delta=0   -> (0, 0, 0)        previous was values[0]
    //   i=1: delta=1000 -> (333, 667, 1000)
    UpsampleState s;
    std::vector<std::int16_t> src{0, 1000};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect_eq_int(out[0], 0, "first triplet: round(0 + 0/3) (delta=0 since prev=source[0])");
    expect_eq_int(out[1], 0, "second output: round(0 + 2*0/3)");
    expect_eq_int(out[2], 0, "third output: current sample (source[0])");
    expect_eq_int(out[3], 333, "fourth output: round(0 + 1000/3)");
    expect_eq_int(out[4], 667, "fifth output: round(0 + 2*1000/3)");
    expect_eq_int(out[5], 1000, "sixth output: current sample (source[1])");
}

void test_negative_step_matches_python_baseline() {
    // Source {0, -1000}; previous defaults to values[0] = 0.
    //   i=0: delta=0    -> (0, 0, 0)
    //   i=1: delta=-1000 -> (-333, -667, -1000)
    UpsampleState s;
    std::vector<std::int16_t> src{0, -1000};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect_eq_int(out[0], 0, "first triplet: round(0 + 0/3)");
    expect_eq_int(out[1], 0, "second output: round(0 + 2*0/3)");
    expect_eq_int(out[2], 0, "third output: current sample (source[0])");
    expect_eq_int(out[3], -333, "fourth output: round(0 + -1000/3)");
    expect_eq_int(out[4], -667, "fifth output: round(0 + 2*-1000/3)");
    expect_eq_int(out[5], -1000, "sixth output: current sample (source[1])");
}

void test_step_input_with_previous_state_matches_python_baseline() {
    // Source {1000}; previous=0 (set externally).
    //   i=0: delta=1000 -> (333, 667, 1000)
    UpsampleState s;
    s.previous_sample = 0;
    s.have_previous = true;
    std::vector<std::int16_t> src{1000};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect(out.size() == 3, "single source with previous -> 3 outputs");
    expect_eq_int(out[0], 333, "first output: round(0 + 1000/3)");
    expect_eq_int(out[1], 667, "second output: round(0 + 2*1000/3)");
    expect_eq_int(out[2], 1000, "third output: current sample");
}

void test_intermediate_values_within_int16() {
    // With valid int16 inputs, the formula (previous + k*delta/3)
    // stays within int16 range because |delta| <= 65535 and the
    // weighted average of two int16 values cannot exceed int16.
    // Verify that taps stay in range for a worst-case scenario.
    UpsampleState s;
    s.previous_sample = -32768;
    s.have_previous = true;
    std::vector<std::int16_t> src{32767};
    auto out = upsample_16k_to_48k(std::span<const std::int16_t>(src), s);
    expect(out.size() == 3, "single source -> 3 outputs");
    // All three outputs should be valid int16 (no clamping needed
    // since the math is bounded by max(|previous|, |current|) <= 32767).
    expect(out[0] >= -32768 && out[0] <= 32767, "tap1 within int16 range");
    expect(out[1] >= -32768 && out[1] <= 32767, "tap2 within int16 range");
    expect_eq_int(out[2], 32767, "current sample preserved");
}

}  // namespace

int main() {
    test_empty_source_returns_empty();
    test_first_sample_no_previous_initializes_state();
    test_two_samples_produce_six_outputs();
    test_constant_source_yields_three_copies_per_input();
    test_step_input_matches_python_baseline();
    test_negative_step_matches_python_baseline();
    test_step_input_with_previous_state_matches_python_baseline();
    test_intermediate_values_within_int16();

    if (failures != 0) {
        std::cerr << "Upsample16kTo48k tests: " << failures
                  << " failure(s) (red state on stub; step 2 turns green)\n";
        return 1;
    }
    std::cout << "All Upsample16kTo48k tests passed\n";
    return 0;
}