// Phase 3 / ADR-0013 §3.1: VoiceController stub.
//
// Step 1 (this file) returns the no-op default for every event so the
// red-state unit tests in tests/unit/test_voice_controller.cpp fail
// in a controlled way. Step 2 lands the real state machine.

#include "remotemic/voice/voice_controller.hpp"

namespace remotemic::voice {

VoiceController::VoiceController(VoiceTriggerMode mode) noexcept
    : mode_(mode) {}

bool VoiceController::holding() const noexcept { return holding_; }

bool VoiceController::active() const noexcept {
    return holding_ || toggle_active_;
}

VoiceHostAction VoiceController::on_mic_button_pressed() noexcept {
    // STUB: always returns Tap and never mutates state. Real
    // behaviour (Toggle -> Tap + toggle_active=true; Hold -> KeyDown
    // + holding=true) lands in step 2.
    return VoiceHostAction::Tap;
}

std::optional<VoiceHostAction>
VoiceController::on_audio_stopped() noexcept {
    return std::nullopt;
}

std::optional<VoiceHostAction> VoiceController::reset() noexcept {
    return std::nullopt;
}

void VoiceController::restore_pending(VoiceHostAction /*action*/) noexcept {
    // STUB: ignored. Step 2 restores ``holding_`` on KeyUp and
    // ``toggle_active_`` on Tap.
}

void VoiceController::cancel_pending() noexcept {
    // STUB: ignored. Step 2 clears both flags.
}

}  // namespace remotemic::voice
