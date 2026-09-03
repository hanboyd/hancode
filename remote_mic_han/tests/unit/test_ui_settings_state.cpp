#include "remotemic/ui/settings_state.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

remotemic::ui::SettingsState make_state() {
    return {"lctrl+lalt", 0, {"lctrl+lalt", "ralt"}, 2, 0,
            {"xiaomi-rc003", "dji-mic-2"}, "xiaomi-rc003",
            {"ok", "power", "mic"}, "ok"};
}

void test_trigger_mode_pairs_hotkey() {
    auto state = make_state();
    expect(state.set_trigger_mode_index(1), "trigger mode changes");
    expect(state.trigger_mode_index() == 1, "trigger index stored");
    expect(state.hotkey_text() == "ralt", "paired hotkey applied");
    expect(!state.set_trigger_mode_index(9), "invalid trigger rejected");
}

void test_recorded_hotkey_is_preserved() {
    auto state = make_state();
    expect(state.set_hotkey_text("lctrl+lwin"), "recorded hotkey stored");
    expect(state.set_trigger_mode_preserving_hotkey(1), "mode inferred");
    expect(state.hotkey_text() == "lctrl+lwin", "recorded hotkey preserved");
}

void test_selections_are_bounded() {
    auto state = make_state();
    expect(state.set_selected_endpoint_index(1), "endpoint changes");
    expect(!state.set_selected_endpoint_index(2), "endpoint overflow rejected");
    expect(state.set_selected_device_index(1), "device changes");
    expect(state.selected_device_id() == "dji-mic-2", "device id follows index");
    expect(state.select_button("power"), "known button selected");
    expect(!state.select_button("unknown"), "unknown button rejected");
}

} // namespace

int main() {
    test_trigger_mode_pairs_hotkey();
    test_recorded_hotkey_is_preserved();
    test_selections_are_bounded();
    if (failures != 0) return 1;
    std::cout << "UI settings state tests passed\n";
    return 0;
}
