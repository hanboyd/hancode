// Phase 5 / ADR-0015 §8 / step 2 sub-pass B: IHostActionSink +
// recording double + real SendInput adapter tests.

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <remotemic/input/fake_host_action_sink.hpp>
#include <remotemic/input/i_host_action_sink.hpp>
#include <remotemic/input/input_event.hpp>
#include <remotemic/input/send_input_action_sink.hpp>

using remotemic::input::FakeHostActionSink;
using remotemic::input::IHostActionSink;
using remotemic::input::SendInputActionSink;
using remotemic::input::SystemAction;

namespace {

bool test_fake_action_sink_records_key_submissions() {
    FakeHostActionSink sink;
    assert(sink.start());

    using namespace std::chrono_literals;
    bool ok = sink.submit_key(0x41, /*key_down=*/true, 100ms);
    assert(ok);
    ok = sink.submit_key(0x41, /*key_down=*/false, 100ms);
    assert(ok);

    auto keys = sink.recorded_keys();
    assert(keys.size() == 2);
    assert(keys[0].first == 0x41);
    assert(keys[0].second == true);
    assert(keys[1].first == 0x41);
    assert(keys[1].second == false);
    assert(sink.submitted_count() == 2);
    assert(sink.submit_error_count() == 0);
    sink.stop();
    return true;
}

bool test_fake_action_sink_records_system_actions() {
    FakeHostActionSink sink;
    assert(sink.start());

    assert(sink.submit_system_action(SystemAction::VolumeUp));
    assert(sink.submit_system_action(SystemAction::VolumeDown));

    auto sys = sink.recorded_system_actions();
    assert(sys.size() == 2);
    assert(sys[0] == SystemAction::VolumeUp);
    assert(sys[1] == SystemAction::VolumeDown);
    sink.stop();
    return true;
}

bool test_fake_action_sink_cancel_clears_pending() {
    FakeHostActionSink sink;
    assert(sink.start());
    sink.submit_key(0x42, true, std::chrono::milliseconds(100));
    sink.submit_system_action(SystemAction::Escape);
    assert(sink.pending_count() == 2);
    sink.cancel_pending();
    assert(sink.pending_count() == 0);
    sink.stop();
    return true;
}

bool test_fake_action_sink_submit_failure_increments_error() {
    FakeHostActionSink sink;
    assert(sink.start());
    sink.set_submit_fails_for_test(true);
    bool ok = sink.submit_key(0x43, true, std::chrono::milliseconds(100));
    assert(!ok);
    assert(sink.submitted_count() == 0);
    assert(sink.submit_error_count() == 1);
    sink.stop();
    return true;
}

bool test_send_input_starts_on_windows() {
    // Step 2 sub-pass B: SendInputActionSink is now a real adapter.
    // On Windows it verifies user32.dll + SendInput availability and
    // starts a worker thread; on non-Windows CI it fails closed per
    // ADR-0015 §2.
    SendInputActionSink sink;
#ifdef _WIN32
    assert(sink.start() == true);
    // submit_key queues to the worker (returns true if started).
    using namespace std::chrono_literals;
    bool ok = sink.submit_key(0x44, /*key_down=*/true, 100ms);
    assert(ok);
    // submit_system_action dispatches Win32 system commands directly.
    ok = sink.submit_system_action(SystemAction::VolumeMute);
    assert(ok);
    sink.stop();
#else
    assert(sink.start() == false);
    bool ok = sink.submit_key(0x44, true, std::chrono::milliseconds(100));
    assert(!ok);
    ok = sink.submit_system_action(SystemAction::VolumeMute);
    assert(!ok);
    assert(sink.submitted_count() == 0);
    assert(sink.submit_error_count() == 2);
#endif
    return true;
}

bool test_action_sink_is_polymorphic() {
    FakeHostActionSink fake;
    IHostActionSink* base = &fake;
    assert(base->start());
    base->cancel_pending();
    base->stop();
    return true;
}

} // namespace

int main() {
    struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"fake_action_sink_records_key_submissions",
         &test_fake_action_sink_records_key_submissions},
        {"fake_action_sink_records_system_actions",
         &test_fake_action_sink_records_system_actions},
        {"fake_action_sink_cancel_clears_pending",
         &test_fake_action_sink_cancel_clears_pending},
        {"fake_action_sink_submit_failure_increments_error",
         &test_fake_action_sink_submit_failure_increments_error},
        {"send_input_starts_on_windows",
         &test_send_input_starts_on_windows},
        {"action_sink_is_polymorphic",
         &test_action_sink_is_polymorphic},
    };

    int failures = 0;
    for (const auto& c : cases) {
        bool ok = c.fn();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failures;
    }
    if (failures != 0) {
        std::printf("test_i_host_action_sink: %d/%zu failed\n",
                    failures, sizeof(cases) / sizeof(cases[0]));
        return 1;
    }
    std::printf("test_i_host_action_sink: all PASS\n");
    return 0;
}