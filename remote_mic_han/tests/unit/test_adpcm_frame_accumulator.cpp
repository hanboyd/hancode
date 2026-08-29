// Phase 2 / Area 4: TDD unit tests for FrameAccumulator
// (ADR-0012 section 3 / section 8).
//
// Reads the same JSON golden fixtures that the Area 4 shadow parity
// test (step 5) reads. Each fixture may exercise multiple append()
// calls; the test runs them in sequence and compares the cumulative
// returned frames against the fixture's expected_frames_hex.
//
// On the stub implementation:
//   - append() returns {} for any input
//
// All 7 fixtures fail on the stub; step 3 implements the real
// accumulator and all 7 pass.

#include "remotemic/adpcm/frame_accumulator.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using json = nlohmann::json;
using remotemic::adpcm::FrameAccumulator;

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

// Each fixture carries a single data_hex + frame_size that the test
// uses across append_count calls. The multi-append fixture's first
// call uses the data_hex *truncated* to the first 80 bytes (matching
// the generator). We replicate that splitting here.
void run_frame_fixture(const std::string& name) {
    json fixture;
    if (!load_fixture(name, fixture)) {
        expect(false, "could not locate fixture " + name);
        return;
    }

    const auto data = hex_to_bytes(fixture.at("data_hex").get<std::string>());
    const auto frame_size = fixture.at("frame_size").get<std::uint16_t>();
    const auto append_count = fixture.at("append_count").get<std::size_t>();
    const auto& expected_hex = fixture.at("expected_frames_hex");

    FrameAccumulator acc;

    // Multi-append fixtures split the data across the calls in a
    // fixture-specific way. The fixture's data_hex always describes
    // what goes into the *last* call; earlier calls use whatever
    // bytes precede. For frame-multi-append-across-calls.json the
    // generator stores the second call's data in data_hex (80..200)
    // and the first call's data is implicit (range(80)).
    //
    // To keep the fixture format simple, the generator encodes
    // append_count=2 with the second-call data only. We replicate
    // the first call here using the convention: first call uses the
    // first 80 bytes of a 0..200 sequence.
    std::vector<std::vector<std::uint8_t>> all_frames;
    if (name == "frame-multi-append-across-calls.json" && append_count == 2) {
        // First call: bytes(0..80) mod 256
        std::vector<std::uint8_t> first;
        first.reserve(80);
        for (int i = 0; i < 80; ++i) {
            first.push_back(static_cast<std::uint8_t>(i));
        }
        auto first_frames = acc.append(first, frame_size);
        for (auto& f : first_frames) all_frames.push_back(std::move(f));

        // Second call: data (bytes(80..200) mod 256)
        auto second_frames = acc.append(data, frame_size);
        for (auto& f : second_frames) all_frames.push_back(std::move(f));
    } else {
        // Single-append fixtures
        for (std::size_t i = 0; i < append_count; ++i) {
            auto frames = acc.append(data, frame_size);
            for (auto& f : frames) all_frames.push_back(std::move(f));
        }
    }

    if (all_frames.size() != expected_hex.size()) {
        expect(false, "fixture " + name + ": emitted "
               + std::to_string(all_frames.size()) + " frames, expected "
               + std::to_string(expected_hex.size()));
        return;
    }
    for (std::size_t i = 0; i < all_frames.size(); ++i) {
        const auto exp_hex = expected_hex.at(i).get<std::string>();
        const auto act_hex = bytes_to_hex(all_frames[i]);
        if (act_hex != exp_hex) {
            expect(false, "fixture " + name + ": frame["
                   + std::to_string(i) + "] = " + act_hex
                   + ", expected " + exp_hex);
            return;
        }
    }
}

}  // namespace

int main() {
    // Each fixture covers a different boundary of the accumulator:
    //   empty:                empty data
    //   under-size:           data shorter than frame_size
    //   exact-size:           data == frame_size exactly
    //   multi-from-single:    one append emits multiple frames
    //   multi-append-across:  frames straddle two append calls
    //   zero-size:            frame_size=0 guard
    //   multi-frame-size-10:  small frame_size exercises multi-frame
    run_frame_fixture("frame-empty.json");
    run_frame_fixture("frame-under-size.json");
    run_frame_fixture("frame-exact-size.json");
    run_frame_fixture("frame-multi-from-single.json");
    run_frame_fixture("frame-multi-append-across-calls.json");
    run_frame_fixture("frame-zero-size.json");
    run_frame_fixture("frame-multi-frame-size-10.json");

    if (failures == 0) {
        std::cout << "All adpcm_frame_accumulator tests passed\n";
        return 0;
    }
    std::cerr << failures << " adpcm_frame_accumulator test(s) failed\n";
    return 1;
}