// Phase 5 / ADR-0015 §8 / step 2 sub-pass B: IHostActionSink +
// recording double + real SendInput adapter tests.
//
// Live-input policy (P0 fix): the default unit test never touches the
// user's desktop.  The Windows wiring test swaps in recording fakes via
// SetSendInputBackendsForTest() and asserts argument shapes; the only
// test that may emit real input is the opt-in live smoke, which runs
// only when REMOTEMIC_ALLOW_LIVE_INPUT_TESTS=1 is set explicitly and is
// excluded from ordinary unittest/ctest/packaging runs.

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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <tuple>
#endif

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
    assert(!sink.submit_key(0x43, true, std::chrono::milliseconds(100)));
    assert(sink.submitted_count() == 0);
    assert(sink.submit_error_count() == 1);
    sink.stop();
    return true;
}

#ifdef _WIN32

// Recording fakes for the raw Win32 injection backends.  They never emit
// real input; they only record what the production wiring would have sent.

struct FakeBackendRecords {
    // SendInput observations: one entry per call, with the INPUTs.
    std::vector<std::vector<INPUT>> send_input_calls;
    // SendMessageW observations: (hwnd, msg, wparam, lparam).
    std::vector<std::tuple<HWND, UINT, WPARAM, LPARAM>> send_message_calls;
    // keybd_event observations: (vk, scan, flags, extra_info).
    std::vector<std::tuple<BYTE, BYTE, DWORD, ULONG_PTR>> keybd_event_calls;
};

FakeBackendRecords g_records;

UINT WINAPI FakeSendInput(UINT count, LPINPUT inputs, int cb_size) {
    (void)cb_size;  // Release builds: the Debug-only assert below is gone.
    assert(cb_size == static_cast<int>(sizeof(INPUT)));
    if (count > 0) {
        g_records.send_input_calls.emplace_back(inputs, inputs + count);
    }
    return count;
}

LRESULT WINAPI FakeSendMessageW(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam) {
    g_records.send_message_calls.emplace_back(hwnd, msg, wparam, lparam);
    return 0;
}

void WINAPI FakeKeybdEvent(BYTE vk, BYTE scan, DWORD flags,
                           ULONG_PTR extra_info) {
    g_records.keybd_event_calls.emplace_back(vk, scan, flags, extra_info);
}

bool test_send_input_backend_wiring_on_windows() {
    // Step 2 sub-pass B: SendInputActionSink is a real adapter.  Its
    // wiring is verified through the recording fakes - the raw Win32
    // backends are swapped out, so this test CANNOT emit real input
    // into the user's desktop (P0 input-isolation policy).
    g_records = FakeBackendRecords{};
    remotemic::input::SetSendInputBackendsForTest(
        reinterpret_cast<void*>(&FakeSendInput),
        reinterpret_cast<void*>(&FakeSendMessageW),
        reinterpret_cast<void*>(&FakeKeybdEvent));

    SendInputActionSink sink;
    assert(sink.start() == true);

    using namespace std::chrono_literals;
    bool ok = sink.submit_key(0x44, /*key_down=*/true, 100ms);
    assert(ok);

    // The worker drains asynchronously; wait (bounded) for delivery so the
    // assertions below are deterministic before stop() joins the thread.
    for (int i = 0; i < 200 && g_records.send_input_calls.empty(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    assert(!g_records.send_input_calls.empty());

    // submit_system_action dispatches on the caller thread - immediate.
    ok = sink.submit_system_action(SystemAction::VolumeMute);
    assert(ok);

    sink.stop();

    // The system-action path must not have reached keybd_event.
    assert(g_records.keybd_event_calls.empty());

    // SendInput received exactly the submitted key: VK 0x44, key-down,
    // no KEYUP flag.
    assert(g_records.send_input_calls.size() == 1);
    const auto& inputs = g_records.send_input_calls[0];
    assert(inputs.size() == 1);
    assert(inputs[0].type == INPUT_KEYBOARD);
    assert(inputs[0].ki.wVk == 0x44);
    assert((inputs[0].ki.dwFlags & KEYEVENTF_KEYUP) == 0);
    (void)inputs;  // Release builds: the Debug-only asserts above are gone.

    // VolumeMute reached SendMessageW as the documented WM_APPCOMMAND
    // broadcast with the correct command in the LPARAM high word.
    assert(g_records.send_message_calls.size() == 1);
    auto [hwnd, msg, wparam, lparam] = g_records.send_message_calls[0];
    (void)hwnd;
    (void)msg;
    (void)wparam;
    (void)lparam;
    assert(hwnd == HWND_BROADCAST);
    assert(msg == WM_APPCOMMAND);
    assert(GET_APPCOMMAND_LPARAM(lparam) == APPCOMMAND_VOLUME_MUTE);

    assert(sink.submit_error_count() == 0);

    remotemic::input::SetSendInputBackendsForTest(nullptr, nullptr, nullptr);
    return true;
}

bool test_live_send_input_smoke_opt_in() {
    // Explicit opt-in live smoke: emits REAL keyboard input into the
    // user's desktop.  Gated behind REMOTEMIC_ALLOW_LIVE_INPUT_TESTS=1 so
    // ordinary unittest/ctest/packaging runs never reach this code.
    // Always pairs every down with an up and releases on every exit path.
    char live_buffer[4] = {};
    const DWORD live_len = ::GetEnvironmentVariableA(
        "REMOTEMIC_ALLOW_LIVE_INPUT_TESTS", live_buffer,
        static_cast<DWORD>(sizeof(live_buffer)));
    if (live_len == 0 || live_len >= sizeof(live_buffer) ||
        std::strcmp(live_buffer, "1") != 0) {
        std::printf(
            "SKIPPED: live input smoke disabled by default; set "
            "REMOTEMIC_ALLOW_LIVE_INPUT_TESTS=1 to run it (WILL AFFECT "
            "USER DESKTOP)\n");
        return true;
    }
    std::printf(
        "LIVE INPUT TEST - WILL AFFECT USER DESKTOP (opt-in via "
        "REMOTEMIC_ALLOW_LIVE_INPUT_TESTS=1)\n");

    SendInputActionSink sink;
    if (!sink.start()) {
        std::printf("live smoke: sink failed to start\n");
        return false;
    }
    using namespace std::chrono_literals;
    const std::uint16_t kVkD = 0x44;
    bool ok = false;
    try {
        ok = sink.submit_key(kVkD, /*key_down=*/true, 100ms);
        assert(ok);
        // Always pair the down with an up so no held state survives.
        ok = sink.submit_key(kVkD, /*key_down=*/false, 100ms);
        assert(ok);
        for (int i = 0; i < 200 && sink.submitted_count() < 2; ++i) {
            std::this_thread::sleep_for(10ms);
        }
        assert(sink.submitted_count() >= 2);
    } catch (...) {
        // Exception safety: still attempt the release before teardown.
        (void)sink.submit_key(kVkD, /*key_down=*/false, 100ms);
        sink.stop();
        std::printf("live smoke: aborted, best-effort key-up submitted\n");
        return false;
    }
    (void)ok;  // Release builds: the Debug-only asserts above are gone.
    sink.stop();
    return true;
}

#else  // !_WIN32

bool test_send_input_backend_wiring_on_windows() {
    // Non-Windows hosts: the adapter fails closed per ADR-0015 §2 and the
    // backend seam is a no-op (never emitting anything anywhere).
    SendInputActionSink sink;
    assert(sink.start() == false);
    assert(!sink.submit_key(0x44, true, std::chrono::milliseconds(100)));
    assert(!sink.submit_system_action(SystemAction::VolumeMute));
    assert(sink.submitted_count() == 0);
    assert(sink.submit_error_count() == 2);
    remotemic::input::SetSendInputBackendsForTest(nullptr, nullptr, nullptr);
    return true;
}

bool test_live_send_input_smoke_opt_in() {
    std::printf("SKIPPED: live input smoke is Windows-only\n");
    return true;
}

#endif  // _WIN32

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
        {"send_input_backend_wiring_on_windows",
         &test_send_input_backend_wiring_on_windows},
        {"live_send_input_smoke_opt_in",
         &test_live_send_input_smoke_opt_in},
        {"action_sink_is_polymorphic",
         &test_action_sink_is_polymorphic},
    };

    bool all_ok = true;
    for (const auto& test_case : cases) {
        std::printf("test: %s ... ", test_case.name);
        std::fflush(stdout);
        const bool ok = test_case.fn();
        std::printf("%s\n", ok ? "OK" : "FAIL");
        if (!ok) {
            all_ok = false;
        }
    }
    std::printf("%s\n", all_ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return all_ok ? 0 : 1;
}
