// Phase 3 / ADR-0013 §3.1: VoiceController — real implementation.
//
// Replaces the step-1 stub. Behavior matches
// apps/windows/rc003/src/ovb_rc003/voice_controller.py:44-148 byte
// for byte (or, more accurately, action for action and state-flag
// for state-flag), so the Phase 2 shadow parity test for area 4
// (which was deliberately deferred to Phase 3 because VoiceController
// is a state machine, not pure compute) can drive both sides through
// the same script.

#include "remotemic/voice/voice_controller.hpp"

namespace remotemic::voice {

VoiceController::VoiceController(VoiceTriggerMode mode) noexcept
    : mode_(mode) {}

bool VoiceController::holding() const noexcept { return holding_; }

bool VoiceController::active() const noexcept {
    return holding_ || toggle_active_;
}

VoiceHostAction VoiceController::on_mic_button_pressed() noexcept {
    if (mode_ == VoiceTriggerMode::Hold) {
        holding_ = true;
        return VoiceHostAction::KeyDown;
    }
    toggle_active_ = true;
    return VoiceHostAction::Tap;
}

std::optional<VoiceHostAction>
VoiceController::on_audio_stopped() noexcept {
    if (mode_ == VoiceTriggerMode::Hold) {
        if (!holding_) {
            return std::nullopt;
        }
        holding_ = false;
        return VoiceHostAction::KeyUp;
    }
    if (!toggle_active_) {
        return std::nullopt;
    }
    toggle_active_ = false;
    return VoiceHostAction::Tap;
}

std::optional<VoiceHostAction> VoiceController::reset() noexcept {
    if (holding_) {
        holding_ = false;
        return VoiceHostAction::KeyUp;
    }
    if (toggle_active_) {
        toggle_active_ = false;
        return VoiceHostAction::Tap;
    }
    return std::nullopt;
}

void VoiceController::restore_pending(VoiceHostAction action) noexcept {
    if (action == VoiceHostAction::KeyUp) {
        holding_ = true;
    } else if (action == VoiceHostAction::Tap) {
        toggle_active_ = true;
    }
}

void VoiceController::cancel_pending() noexcept {
    holding_ = false;
    toggle_active_ = false;
}

}  // namespace remotemic::voice
