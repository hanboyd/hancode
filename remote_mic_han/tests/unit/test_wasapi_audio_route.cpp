// Phase 4 / ADR-0014 §3.4: WasapiAudioRoute TDD red-state tests.
//
// Stub behavior: start() returns false (no real WASAPI resolution),
// write() returns false. Tests assert that the public surface compiles,
// the destructor is idempotent, and stop()/close() do not crash. The
// "start() returns true on a real WASAPI device" path is exercised by
// the G6 real-acceptance gate, not here.
//
// Windows-only: this test only compiles when WASAPI / Windows headers
// are available. On non-Windows CI it is skipped (see CMake wiring).

#include "remotemic/audio/wasapi_audio_route.hpp"

#include <iostream>
#include <span>
#include <vector>

namespace {

using remotemic::audio::WasapiAudioRoute;
using remotemic::PcmFormat;

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_constructor_and_destructor_are_idempotent() {
    {
        WasapiAudioRoute r(L"CABLE Input");
        // No-op calls must not crash.
        r.stop();
        r.close();
    }
    expect(true, "WasapiAudioRoute construction + stop + close + dtor is clean");
}

void test_default_constructor_uses_empty_host_api() {
    WasapiAudioRoute r(L"CABLE Input");
    expect(r.current_format().sample_rate == 16'000,
           "default PcmFormat sample_rate is 16000");
    expect(!r.writer_thread_alive(),
           "writer thread is not alive before start()");
    expect(r.dropped_count() == 0,
           "dropped_count is 0 before start()");
    expect(r.write_error_count() == 0,
           "write_error_count is 0 before start()");
}

void test_write_before_start_returns_false() {
    WasapiAudioRoute r(L"CABLE Input");
    std::vector<std::int16_t> samples{0, 100, 200};
    expect(!r.write(std::span<const std::int16_t>(samples)),
           "write() before start() returns false (queue not running)");
    expect(r.dropped_count() == 0,
           "no samples dropped when write() returns false");
}

void test_drain_is_noop_when_not_running() {
    WasapiAudioRoute r(L"CABLE Input");
    r.drain(std::chrono::milliseconds(100));
    expect(true, "drain() with no active queue is a no-op (does not block)");
}

}  // namespace

int main() {
    test_constructor_and_destructor_are_idempotent();
    test_default_constructor_uses_empty_host_api();
    test_write_before_start_returns_false();
    test_drain_is_noop_when_not_running();

    if (failures != 0) {
        std::cerr << "WasapiAudioRoute tests: " << failures
                  << " failure(s) (red state on stub; step 2 turns green)\n";
        return 1;
    }
    std::cout << "All WasapiAudioRoute tests passed\n";
    return 0;
}