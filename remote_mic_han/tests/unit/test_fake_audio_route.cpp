// Phase 4 / ADR-0014 §3.5: FakeAudioRoute TDD red-state tests.
//
// Stub behavior: start() returns false; write() returns false;
// recorded_samples() returns 0. Tests below assert the real contract:
// start() returns true, write() appends to recorded buffer, stop()/close()
// are idempotent and counted. They FAIL on the stub; step 2 turns them
// green.

#include "remotemic/audio/fake_audio_route.hpp"

#include <iostream>
#include <span>
#include <vector>

namespace {

using remotemic::audio::FakeAudioRoute;
using remotemic::PcmFormat;

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_start_returns_true_and_increments_counter() {
    FakeAudioRoute r;
    PcmFormat fmt{};
    expect(r.start(fmt),
           "start() returns true (test double must succeed)");
    expect(r.started_count() == 1,
           "start() increments started_count to 1");
    expect(r.last_format().sample_rate == fmt.sample_rate,
           "last_format matches the format passed to start()");
}

void test_write_appends_and_returns_true() {
    FakeAudioRoute r;
    r.start(PcmFormat{});
    std::vector<std::int16_t> first{1, 2, 3};
    std::vector<std::int16_t> second{4, 5};
    expect(r.write(std::span<const std::int16_t>(first)),
           "write(3 samples) returns true");
    expect(r.write(std::span<const std::int16_t>(second)),
           "write(2 samples) returns true");
    expect(r.write_call_count() == 2,
           "write_call_count == 2 after two writes");
    expect(r.recorded_samples() == 5,
           "recorded_samples == 5 after two writes (3 + 2)");
}

void test_write_before_start_returns_false() {
    FakeAudioRoute r;
    std::vector<std::int16_t> samples{1, 2, 3};
    expect(!r.write(std::span<const std::int16_t>(samples)),
           "write before start() returns false");
    expect(r.recorded_samples() == 0,
           "recorded buffer stays empty when write rejects");
}

void test_stop_is_idempotent_and_counted() {
    FakeAudioRoute r;
    r.start(PcmFormat{});
    r.stop();
    r.stop();
    r.stop();
    expect(r.stopped_count() == 3,
           "stopped_count tracks every stop() call (3 in this test)");
}

void test_close_implies_stop() {
    FakeAudioRoute r;
    r.start(PcmFormat{});
    r.close();
    expect(r.closed_count() == 1, "close() increments closed_count");
    expect(r.stopped_count() == 0,
           "close() does not itself call stop() (separate counters)");
}

}  // namespace

int main() {
    test_start_returns_true_and_increments_counter();
    test_write_appends_and_returns_true();
    test_write_before_start_returns_false();
    test_stop_is_idempotent_and_counted();
    test_close_implies_stop();

    if (failures != 0) {
        std::cerr << "FakeAudioRoute tests: " << failures
                  << " failure(s) (red state on stub; step 2 turns green)\n";
        return 1;
    }
    std::cout << "All FakeAudioRoute tests passed\n";
    return 0;
}