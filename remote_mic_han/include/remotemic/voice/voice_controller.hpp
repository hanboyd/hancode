// Phase 3 / ADR-0013 §3.1: VoiceController — pure value-class state
// machine that decides which host voice action (TAP / KEY_DOWN / KEY_UP)
// to emit in response to a stream of mic-button / audio-stop events.
//
// This header is the contract; the implementation is delivered in
// step 2 (commit that fills in ``src/voice/voice_controller.cpp``).
// The stub returns the "noop" default for every input so the test
// suite in step 1 fails red until step 2 lands.

#ifndef REMOTEMIC_VOICE_VOICE_CONTROLLER_HPP
#define REMOTEMIC_VOICE_VOICE_CONTROLLER_HPP

#include <optional>

namespace remotemic::voice {

enum class VoiceTriggerMode {
    Toggle,
    Hold,
};

enum class VoiceHostAction {
    Tap,
    KeyDown,
    KeyUp,
};

class VoiceController {
public:
    explicit VoiceController(VoiceTriggerMode mode) noexcept;

    // ---- queries (cheap, no state mutation) ------------------------------
    VoiceTriggerMode trigger_mode() const noexcept { return mode_; }
    bool holding() const noexcept;
    bool active() const noexcept;

    // ---- event handlers --------------------------------------------------
    // Each returns the host action the caller must execute (or
    // ``std::nullopt`` if no action is owed right now). The caller
    // performs the actual send; this class never touches the host
    // input layer.
    VoiceHostAction on_mic_button_pressed() noexcept;
    std::optional<VoiceHostAction> on_audio_stopped() noexcept;

    // Force-close any outstanding session. Returns the closing action
    // the caller must execute, or ``std::nullopt`` if nothing is owed.
    std::optional<VoiceHostAction> reset() noexcept;

    // Restore a pending session whose closing action failed to
    // deliver. Only ``VoiceHostAction::KeyUp`` and
    // ``VoiceHostAction::Tap`` are accepted as a closing action; any
    // other value is a caller bug and is silently ignored (matches
    // the Python ``restore_pending`` behaviour on the same path).
    void restore_pending(VoiceHostAction action) noexcept;

    // Clear any pending session without emitting an action. Used when
    // the opening action itself failed to deliver and there is
    // therefore nothing to release.
    void cancel_pending() noexcept;

private:
    VoiceTriggerMode mode_;
    bool holding_ = false;
    bool toggle_active_ = false;
};

}  // namespace remotemic::voice

#endif  // REMOTEMIC_VOICE_VOICE_CONTROLLER_HPP
