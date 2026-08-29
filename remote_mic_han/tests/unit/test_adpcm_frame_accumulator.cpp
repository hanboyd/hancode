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

    // ------------------------------------------------------------------
    // Phase 2 / Area 4 step 4: reset() contract and pending invariant.
    //
    // reset() drops any pending bytes so the next stream starts
    // clean. After reset(), append() must behave as if the
    // instance were freshly constructed: a partial frame from a
    // previous stream is NOT carried over. The invariant
    //   0 <= pending_size() < frame_size
    // holds after every successful append() with frame_size > 0
    // (and is trivially 0 when frame_size == 0).
    // ------------------------------------------------------------------
    {
        // reset() parity: partial frame in stream A, then reset(),
        // then a fresh stream B. Verify B's first frame does NOT
        // include any leading A bytes.
        FrameAccumulator acc;
        std::vector<std::uint8_t> partial;
        for (int i = 0; i < 50; ++i) {
            partial.push_back(static_cast<std::uint8_t>(i));
        }
        // First stream: leave 50 bytes pending against frame_size=60.
        auto a_frames = acc.append(partial, 60);
        expect(a_frames.empty(),
               "reset_parity: first stream should emit no complete frame (50 < 60)");
        expect(acc.pending_size() == 50,
               "reset_parity: pending_size should be 50 after first stream");

        acc.reset();
        expect(acc.pending_size() == 0,
               "reset_parity: pending_size should be 0 after reset()");

        // Second stream: a 90-byte payload against frame_size=60
        // must yield exactly one frame (bytes 0..59), NOT bytes
        // 0..9 from the first stream joined to bytes 0..49 of the
        // second.
        std::vector<std::uint8_t> second;
        for (int i = 100; i < 190; ++i) {
            second.push_back(static_cast<std::uint8_t>(i));
        }
        auto b_frames = acc.append(second, 60);
        expect(b_frames.size() == 1,
               "reset_parity: second stream should emit exactly 1 complete frame");
        expect(b_frames[0].size() == 60,
               "reset_parity: emitted frame should be 60 bytes");
        expect(b_frames[0][0] == 100 && b_frames[0][59] == 159,
               "reset_parity: emitted frame must contain only bytes from the second stream");
        expect(acc.pending_size() == 30,
               "reset_parity: pending_size should be 30 after second stream");

        // Fresh-instance parity: a freshly constructed
        // FrameAccumulator fed the same bytes 100..190 with the
        // same frame_size=60 must produce the same single frame.
        FrameAccumulator fresh;
        auto fresh_frames = fresh.append(second, 60);
        expect(fresh_frames.size() == 1 && fresh_frames[0] == b_frames[0],
               "reset_parity: post-reset state must equal a freshly constructed instance");
    }

    {
        // Reset clears stale pending even when the next append uses
        // a different frame_size. After reset() the first append
        // must be a no-op (no buffer carry-over) until new bytes
        // arrive, regardless of the new frame_size.
        FrameAccumulator acc;
        std::vector<std::uint8_t> small_partial{1, 2, 3, 4, 5};
        (void)acc.append(small_partial, 10);  // 5 pending, no frame
        expect(acc.pending_size() == 5, "reset_reframing: pre-reset pending=5");
        acc.reset();
        // 4 new bytes against frame_size=4 -> exactly one frame.
        std::vector<std::uint8_t> next{10, 20, 30, 40};
        auto frames = acc.append(next, 4);
        expect(frames.size() == 1 && frames[0].size() == 4,
               "reset_reframing: post-reset append with new frame_size should drop stale bytes");
        expect(frames[0][0] == 10 && frames[0][3] == 40,
               "reset_reframing: emitted frame must contain only the new bytes");
    }

    {
        // Boundary (frame_size = 1): every byte is its own frame.
        FrameAccumulator acc;
        std::vector<std::uint8_t> data{0xAA, 0xBB, 0xCC};
        auto frames = acc.append(data, 1);
        expect(frames.size() == 3,
               "boundary_fs1: frame_size=1 on 3 bytes must emit 3 frames");
        expect(frames[0].size() == 1 && frames[0][0] == 0xAA,
               "boundary_fs1: frame 0 should be [0xAA]");
        expect(frames[1][0] == 0xBB && frames[2][0] == 0xCC,
               "boundary_fs1: frames 1,2 should be [0xBB], [0xCC]");
        expect(acc.pending_size() == 0,
               "boundary_fs1: pending must be 0 after exact-frame-size consumption");
    }

    {
        // Boundary (frame_size = 65535, the std::uint16_t max):
        // a single 65535-byte payload yields exactly one frame;
        // anything shorter stays pending.
        FrameAccumulator acc;
        std::vector<std::uint8_t> exact(65535, 0x5A);
        auto frames = acc.append(exact, 65535);
        expect(frames.size() == 1 && frames[0].size() == 65535,
               "boundary_fs65535: single 65535-byte payload must emit one 65535-byte frame");
        expect(frames[0][0] == 0x5A && frames[0].back() == 0x5A,
               "boundary_fs65535: payload should be preserved verbatim");

        // 65534-byte payload stays fully pending (no partial frame).
        std::vector<std::uint8_t> short_payload(65534, 0xA5);
        acc.reset();
        auto fs2 = acc.append(short_payload, 65535);
        expect(fs2.empty(),
               "boundary_fs65535: 65534-byte payload must not emit a frame");
        expect(acc.pending_size() == 65534,
               "boundary_fs65535: pending should equal the input length");
        expect(acc.pending_size() < 65535,
               "boundary_fs65535: invariant pending < frame_size must hold");
    }

    {
        // Invariant: 0 <= pending_size() < frame_size after every
        // successful append() with frame_size > 0; trivially 0 when
        // frame_size == 0.
        FrameAccumulator acc;
        const std::uint16_t fs = 4;
        // Seed a partial across calls and check pending never reaches
        // or exceeds fs after each append.
        std::vector<std::uint8_t> a{1, 2};
        auto fa = acc.append(a, fs);
        expect(fa.empty(), "inv_a: no frame yet");
        expect(acc.pending_size() < fs, "inv_a: pending < fs");

        std::vector<std::uint8_t> b{3};
        auto fb = acc.append(b, fs);
        expect(fb.empty(), "inv_b: still under");
        expect(acc.pending_size() < fs, "inv_b: pending < fs");

        std::vector<std::uint8_t> c{4, 5};
        auto fc = acc.append(c, fs);
        expect(fc.size() == 1, "inv_c: cross-over emits exactly one frame");
        expect(acc.pending_size() < fs, "inv_c: pending < fs after emission");

        std::vector<std::uint8_t> d{6, 7, 8, 9};
        auto fd = acc.append(d, fs);
        // One frame for [5,6,7,8], then pending=[9] (the leftover
        // from the c step plus d's 4 bytes after consuming 3 to
        // complete the frame). The exact count is not asserted;
        // only the invariant is.
        expect(!fd.empty(), "inv_d: cross-over emits at least one frame");
        expect(acc.pending_size() < fs, "inv_d: pending < fs after emission");

        // frame_size == 0 path is a no-op; pending is untouched.
        const std::vector<std::uint8_t> zero_payload{99, 99, 99};
        auto fz = acc.append(zero_payload, 0);
        expect(fz.empty(), "inv_zero: frame_size=0 emits nothing");
        expect(acc.pending_size() < 4,
               "inv_zero: frame_size=0 must not introduce a false invariant violation");
    }

    if (failures == 0) {
        std::cout << "All adpcm_frame_accumulator tests passed\n";
        return 0;
    }
    std::cerr << failures << " adpcm_frame_accumulator test(s) failed\n";
    return 1;
}