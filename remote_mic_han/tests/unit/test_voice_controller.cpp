// Phase 3 / ADR-0013 §3.1: VoiceController TDD red-state unit tests.
//
// The stub returns Tap for every mic-button press and never mutates
// state. The real behaviour (TOGGLE -> Tap + toggle_active=true;
// HOLD -> KeyDown + holding=true) lands in step 2.
//
// On the stub, only the no-op / single-shot paths pass; every
// stateful path fails. This is intentional.

#include "remotemic/voice/voice_controller.hpp"

#include <iostream>
#include <optional>
#include <string>

namespace {

using remotemic::voice::VoiceController;
using remotemic::voice::VoiceHostAction;
using remotemic::voice::VoiceTriggerMode;

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_toggle_press_returns_tap_and_arms_active() {
    VoiceController c(VoiceTriggerMode::Toggle);
    auto action = c.on_mic_button_pressed();
    expect(action == VoiceHostAction::Tap,
           "Toggle: on_mic_button_pressed -> Tap");
    expect(c.active(),
           "Toggle: after press, active() == true (session owed)");
    expect(!c.holding(),
           "Toggle: holding() always false in Toggle mode");
}

void test_hold_press_returns_key_down_and_arms_holding() {
    VoiceController c(VoiceTriggerMode::Hold);
    auto action = c.on_mic_button_pressed();
    expect(action == VoiceHostAction::KeyDown,
           "Hold: on_mic_button_pressed -> KeyDown");
    expect(c.holding(),
           "Hold: after press, holding() == true (KeyUp owed)");
    expect(c.active(),
           "Hold: after press, active() == true");
}

void test_toggle_audio_stop_closes_with_tap() {
    VoiceController c(VoiceTriggerMode::Toggle);
    c.on_mic_button_pressed();
    auto action = c.on_audio_stopped();
    expect(action.has_value(), "Toggle: on_audio_stopped returns an action");
    if (action) {
        expect(*action == VoiceHostAction::Tap,
               "Toggle: closing action is Tap");
    }
    expect(!c.active(), "Toggle: after stop, active() == false");
}

void test_hold_audio_stop_releases_key() {
    VoiceController c(VoiceTriggerMode::Hold);
    c.on_mic_button_pressed();
    auto action = c.on_audio_stopped();
    expect(action.has_value(), "Hold: on_audio_stopped returns an action");
    if (action) {
        expect(*action == VoiceHostAction::KeyUp,
               "Hold: closing action is KeyUp");
    }
    expect(!c.holding(), "Hold: after stop, holding() == false");
}

void test_audio_stop_without_press_is_noop() {
    VoiceController c(VoiceTriggerMode::Toggle);
    auto action = c.on_audio_stopped();
    expect(!action.has_value(),
           "Toggle: audio_stopped without prior press returns nullopt");
    expect(!c.active(),
           "Toggle: still inactive after orphan audio_stopped");
}

void test_reset_returns_closing_action_when_active() {
    VoiceController toggle(VoiceTriggerMode::Toggle);
    toggle.on_mic_button_pressed();
    auto a = toggle.reset();
    expect(a.has_value(), "Toggle: reset() returns an action when session is open");
    if (a) {
        expect(*a == VoiceHostAction::Tap,
               "Toggle: reset() returns Tap when session is open");
    }
    expect(!toggle.active(), "Toggle: reset() leaves active() == false");

    VoiceController hold(VoiceTriggerMode::Hold);
    hold.on_mic_button_pressed();
    auto a2 = hold.reset();
    expect(a2.has_value(), "Hold: reset() returns an action when session is open");
    if (a2) {
        expect(*a2 == VoiceHostAction::KeyUp,
               "Hold: reset() returns KeyUp when session is open");
    }
    expect(!hold.active(), "Hold: reset() leaves active() == false");
}

void test_reset_without_session_returns_nullopt() {
    VoiceController c(VoiceTriggerMode::Toggle);
    auto a = c.reset();
    expect(!a.has_value(),
           "Toggle: reset() on idle controller returns nullopt");
}

void test_restore_pending_after_failed_close() {
    // XRBM-019: if the closing action failed to deliver (e.g.
    // win32_input.send_key_combo_up now raises), the controller must
    // remember the session is still owed so the worker can retry.
    VoiceController toggle(VoiceTriggerMode::Toggle);
    toggle.on_mic_button_pressed();
    auto closing = toggle.on_audio_stopped();
    expect(closing.has_value(),
           "Toggle: stop yields closing action first");
    if (closing) {
        toggle.restore_pending(*closing);
    }
    expect(toggle.active(),
           "Toggle: after failed close + restore_pending, still active");

    VoiceController hold(VoiceTriggerMode::Hold);
    hold.on_mic_button_pressed();
    auto key_up = hold.on_audio_stopped();
    expect(key_up.has_value(),
           "Hold: stop yields closing action first");
    if (key_up) {
        hold.restore_pending(*key_up);
    }
    expect(hold.active(),
           "Hold: after failed KeyUp + restore_pending, still active");
}

void test_cancel_pending_clears_without_action() {
    VoiceController c(VoiceTriggerMode::Toggle);
    c.on_mic_button_pressed();
    c.cancel_pending();
    expect(!c.active(),
           "Toggle: cancel_pending() clears without emitting action");
    auto stop = c.on_audio_stopped();
    expect(!stop.has_value(),
           "Toggle: after cancel_pending, audio_stopped yields nullopt");
}

}  // namespace

int main() {
    test_toggle_press_returns_tap_and_arms_active();
    test_hold_press_returns_key_down_and_arms_holding();
    test_toggle_audio_stop_closes_with_tap();
    test_hold_audio_stop_releases_key();
    test_audio_stop_without_press_is_noop();
    test_reset_returns_closing_action_when_active();
    test_reset_without_session_returns_nullopt();
    test_restore_pending_after_failed_close();
    test_cancel_pending_clears_without_action();

    if (failures != 0) {
        std::cerr << "VoiceController tests: " << failures
                  << " failure(s) (red state on stub; step 2 turns green)\n";
        return 1;
    }
    std::cout << "All VoiceController tests passed\n";
    return 0;
}
