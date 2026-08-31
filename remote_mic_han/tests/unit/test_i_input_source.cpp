// Phase 5 / ADR-0015 §8 / step 1: IInputSource + recording double red
// state. Recording-double tests pass on day 1 (the fake already
// records what the test injects). Windows-only stubs fail start() and
// are asserted to fail, marking step 1 red state.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <remotemic/input/fake_input_source.hpp>
#include <remotemic/input/frida_hid_tap_source.hpp>
#include <remotemic/input/i_input_source.hpp>
#include <remotemic/input/input_event.hpp>
#include <remotemic/input/low_level_keyboard_hook.hpp>
#include <remotemic/input/raw_input_source.hpp>

using remotemic::input::FakeInputSource;
using remotemic::input::FridaHidTapSource;
using remotemic::input::IInputSource;
using remotemic::input::InputEvent;
using remotemic::input::LowLevelKeyboardHook;
using remotemic::input::RawInputSource;

namespace {

// Recording-double helper: the test injects events through the source
// itself; if the sink registered via set_event_sink gets called, we count
// that as "real callback fired". Step 1 fake deliberately does NOT
// invoke the sink from inject_event_for_test (the fake is a recording
// double, not a relay); tests assert exactly the contract documented.
std::vector<InputEvent> g_captured;

void capture_sink(InputEvent ev, void* /*user_data*/) {
    g_captured.push_back(ev);
}

bool test_fake_input_source_injects_and_records() {
    FakeInputSource src;
    g_captured.clear();
    src.set_event_sink(&capture_sink, nullptr);
    bool ok = src.start();
    assert(ok);

    InputEvent ev{};
    ev.kind = InputEvent::EventKind::KeyDown;
    ev.vk_code = 0x41;  // 'A'
    ev.source = InputEvent::SourceKind::RawInputKeyboard;
    src.inject_event_for_test(ev);

    assert(src.event_count() == 1);
    assert(src.dropped_count() == 0);
    auto recorded = src.recorded_events();
    assert(recorded.size() == 1);
    assert(recorded[0].vk_code == 0x41);

    // The fake is a recording double, NOT a relay — sink stays empty
    // until step 2 wires a relay path on top of the recording buffer.
    assert(g_captured.empty());

    src.stop();
    return true;
}

bool test_fake_input_source_dropped_counter_settable() {
    FakeInputSource src;
    src.set_dropped_count_for_test(42);
    assert(src.dropped_count() == 42);
    return true;
}

bool test_windows_stubs_refuse_start_until_step_2() {
    // Step 1 contract: the three Windows-only input sources return
    // false from start(). Step 2 replaces each stub with a real
    // implementation; until then, production code MUST NOT route real
    // input through these stubs (they'll fail-closed to python).
    RawInputSource ri;
    LowLevelKeyboardHook hook;
    FridaHidTapSource tap;

    assert(ri.start() == false);
    assert(hook.start() == false);
    assert(tap.start() == false);

    ri.stop();
    hook.stop();
    tap.stop();
    return true;
}

bool test_input_source_is_polymorphic() {
    // IInputSource should be usable through the base pointer (the
    // production wiring will hold IInputSource*).
    FakeInputSource fake;
    IInputSource* base = &fake;
    assert(base->start());
    base->stop();
    return true;
}

} // namespace

int main() {
    struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"fake_input_source_injects_and_records",
         &test_fake_input_source_injects_and_records},
        {"fake_input_source_dropped_counter_settable",
         &test_fake_input_source_dropped_counter_settable},
        {"windows_stubs_refuse_start_until_step_2",
         &test_windows_stubs_refuse_start_until_step_2},
        {"input_source_is_polymorphic",
         &test_input_source_is_polymorphic},
    };

    int failures = 0;
    for (const auto& c : cases) {
        bool ok = c.fn();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failures;
    }
    if (failures != 0) {
        std::printf("test_i_input_source: %d/%zu failed\n",
                    failures, sizeof(cases) / sizeof(cases[0]));
        return 1;
    }
    std::printf("test_i_input_source: all PASS\n");
    return 0;
}