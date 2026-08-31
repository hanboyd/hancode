// Phase 5 / ADR-0015 §8 / step 1: IHostActionSink + recording double
// + Windows stub red state.

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

bool test_send_input_stub_rejects_submits_until_step_2() {
    // Step 1 contract: SendInputActionSink refuses every submit_* call.
    // Step 2 replaces with a real user32.SendInput adapter that
    // batches into a worker thread.
    SendInputActionSink sink;
    assert(sink.start() == false);
    bool ok = sink.submit_key(0x44, true, std::chrono::milliseconds(100));
    assert(!ok);
    ok = sink.submit_system_action(SystemAction::VolumeMute);
    assert(!ok);
    assert(sink.submitted_count() == 0);
    assert(sink.submit_error_count() == 2);
    sink.stop();
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
        {"send_input_stub_rejects_submits_until_step_2",
         &test_send_input_stub_rejects_submits_until_step_2},
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