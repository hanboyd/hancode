// Phase 4 / ADR-0014 §3.2: PcmChunker TDD red-state tests.
//
// Stub behavior: next_chunk() always returns nullopt;
// flush_remaining_with_silence() always returns empty. Tests below
// assert the real contract. They FAIL on the stub; step 2 turns them
// green.

#include "remotemic/audio/pcm_chunker.hpp"

#include <iostream>
#include <span>
#include <vector>

namespace {

using remotemic::audio::PcmChunker;

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_chunk_size_for_20ms_at_16khz_is_320() {
    PcmChunker c(std::chrono::milliseconds(20), /*sample_rate_hz=*/16'000);
    expect(c.chunk_samples() == 320,
           "20 ms @ 16 kHz chunk is 320 samples");
    expect(c.buffered_samples() == 0, "buffer starts empty");
}

void test_underfilled_input_does_not_emit_chunk() {
    PcmChunker c(std::chrono::milliseconds(20), /*sample_rate_hz=*/16'000);
    std::vector<std::int16_t> in(100, 7);  // 100 samples < 320
    auto chunk = c.next_chunk(std::span<const std::int16_t>(in));
    expect(!chunk.has_value(),
           "next_chunk returns nullopt when input is under chunk size");
    expect(c.buffered_samples() == 100,
           "buffered_samples == 100 after underfilled push");
}

void test_exact_multiple_emits_one_chunk_and_keeps_zero() {
    PcmChunker c(std::chrono::milliseconds(20), /*sample_rate_hz=*/16'000);
    std::vector<std::int16_t> in(320, 11);
    auto chunk = c.next_chunk(std::span<const std::int16_t>(in));
    expect(chunk.has_value(),
           "next_chunk returns a chunk when input is exactly one chunk");
    if (chunk) {
        expect(chunk->size() == 320, "chunk has 320 samples");
        expect((*chunk)[0] == 11, "first sample is the input value");
    }
    expect(c.buffered_samples() == 0, "buffer is empty after exact fit");
}

void test_multiple_chunks_returned_one_at_a_time() {
    PcmChunker c(std::chrono::milliseconds(20), /*sample_rate_hz=*/16'000);
    std::vector<std::int16_t> in(640, 5);  // 2 chunks
    auto first = c.next_chunk(std::span<const std::int16_t>(in));
    expect(first.has_value() && first->size() == 320,
           "first call returns the first chunk");
    auto second = c.next_chunk(std::span<const std::int16_t>({}));
    expect(second.has_value() && second->size() == 320,
           "second call returns the next chunk");
    auto third = c.next_chunk(std::span<const std::int16_t>({}));
    expect(!third.has_value(),
           "third call returns nullopt (no more buffered samples)");
}

void test_flush_remaining_pads_with_silence() {
    PcmChunker c(std::chrono::milliseconds(20), /*sample_rate_hz=*/16'000);
    std::vector<std::int16_t> in(100, 9);
    c.next_chunk(std::span<const std::int16_t>(in));
    auto flushed = c.flush_remaining_with_silence();
    expect(flushed.size() == 320,
           "flush pads the residue up to a full chunk (320)");
    expect(flushed[0] == 9 && flushed[99] == 9,
           "the 100 real samples come first");
    expect(flushed[100] == 0 && flushed[319] == 0,
           "the trailing 220 samples are silence (int16 zero)");
    expect(c.buffered_samples() == 0, "buffer is empty after flush");
}

void test_flush_with_no_residue_returns_silence_chunk() {
    PcmChunker c(std::chrono::milliseconds(20), /*sample_rate_hz=*/16'000);
    auto flushed = c.flush_remaining_with_silence();
    expect(flushed.size() == 320,
           "flush with no residue still emits a full silence chunk");
    bool all_zero = true;
    for (auto s : flushed) {
        if (s != 0) {
            all_zero = false;
            break;
        }
    }
    expect(all_zero, "emitted chunk is all zeros");
}

void test_zero_duration_throws() {
    bool threw = false;
    try {
        PcmChunker c(std::chrono::milliseconds(0));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "PcmChunker(0 ms) throws std::invalid_argument");
}

}  // namespace

int main() {
    test_chunk_size_for_20ms_at_16khz_is_320();
    test_underfilled_input_does_not_emit_chunk();
    test_exact_multiple_emits_one_chunk_and_keeps_zero();
    test_multiple_chunks_returned_one_at_a_time();
    test_flush_remaining_pads_with_silence();
    test_flush_with_no_residue_returns_silence_chunk();
    test_zero_duration_throws();

    if (failures != 0) {
        std::cerr << "PcmChunker tests: " << failures
                  << " failure(s) (red state on stub; step 2 turns green)\n";
        return 1;
    }
    std::cout << "All PcmChunker tests passed\n";
    return 0;
}