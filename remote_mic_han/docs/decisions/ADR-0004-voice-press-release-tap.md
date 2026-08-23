# ADR-0004: Voice Press / Release Boundary TAP

- Status: accepted
- Date: 2026-08-23
- Related: ADR-0003 (voice edge debounce + hook decoupling)

## Context

The RC003's physical microphone button only delivers a "long-press to start streaming"
signal: ATVV emits `MIC_BUTTON` + `AUDIO_STARTED` while the button is held,
`AUDIO_STOPPED` + `MIC_CLOSE` on release.  There is no "short tap" mode on the
hardware, so every physical press is a hold of indeterminate length.  See
`CURRENT_STATUS.md` and `AI_HANDOVER.md` for the surrounding context: the
shipping Windows target applications (Typeless, 千问 voice mode, and the
macOS-style double-tap window) are all **toggle-style** voice-input tools that
toggle their own voice window on every complete key-pair cycle (one down+hold-
window+up pair => one toggle).  Neither ships in a "push-to-talk" mode.

Before this ADR the bridge delivered the user's voice_hotkey like a
push-to-talk controller in both `voice_trigger_mode` branches:

| Mode        | Press side                                  | Release side                  |
| ----------- | ------------------------------------------- | ----------------------------- |
| TOGGLE      | TAP (single down+up pair)                  | TAP                           |
| HOLD (old)  | sustained KEY_DOWN for the duration of hold | KEY_UP on physical release    |

The old HOLD contract is correct for an application that interprets a sustained
key-down as a continuous voice-input signal (e.g. the upstream macOS Fn-style
push-to-talk).  It is wrong for toggle-style target applications: Typeless /
千问 voice mode see the 14 s sustained Ctrl+Alt-DOWN as a half-completed toggle
cycle and react unpredictably (voice window opens then closes, or stays closed,
depending on each app's local interpretation of "modifier A down followed ~10
ms later by modifier B down").  Real-device acceptance under commit ``e91b9c2``
reproduced this on Typeless with the user's standing configuration:

```
voice_hostkey = lctrl+lalt
voice_trigger_mode = hold
long-press RC003 microphone
=> Typeless voice window opens briefly then closes
```

## Decision

Force every voice-edge delivery through app.py to be a **complete key-pair TAP**
(one down + ~70 ms hold + up) regardless of `voice_trigger_mode`:

* `_handle_mic_button_pressed` always emits `VoiceHostAction.TAP` instead of
  using the controller's HOLD-mode `KEY_DOWN`.
* `_close_voice_host_session` always emits `VoiceHostAction.TAP` instead of
  using the controller's HOLD-mode `KEY_UP`.
* `voice_controller.on_audio_stopped()` is still called so the controller's
  internal HOLD latch and TOGGLE closing state advance exactly as before; only
  the **delivered** action changes.  The controller's returned shape is reused
  by the `restore_pending` retry branch so a failed TAP still records the
  controller's intended closing edge for `voice_controller.restore_pending`.
* VoiceController's own mode-specific behaviour is left intact: HOLD mode still
  drives the internal `voice_controller._holding` latch and the audio-stop
  defer path.  The change lives in the boundary between `voice_controller` and
  the host shortcut sender, not in `voice_controller` itself.

The hold period between press and physical release is still silent at the host
boundary - the bridge sends no host-shortcut traffic while the physical button
is held.  ATVV's `AudioStopped` events that land inside the hold window remain
deferred exactly as before; only the physical release ends the session.  The
net effect: one TAP at press, no traffic during the hold, one TAP at release.

Concretely the host sees, for every RC003 physical press-and-release:

```
press edge   -> send_voice_key_combo_down(tokens)
                time.sleep(0.07)            # the 70 ms "tap" hold window
                send_voice_key_combo_up(tokens)
hold period  -> nothing
release edge -> send_voice_key_combo_down(tokens)
                time.sleep(0.07)
                send_voice_key_combo_up(tokens)
```

This is what a user manually typing the hotkey twice would look like to the
target application; it cycles the target's toggle exactly once per physical
press-and-release pair.

The change is **additive with respect to user-visible configuration**: no new
config key, no new mode.  Every user gets the corrected toggle behaviour
silently.

## Consequences

- Typeless / 千问 voice mode / macOS-style double-tap window all now react
  correctly to a physical long-press on the RC003 microphone.
- The macOS-style **double-tap to pause/resume** upstream feature still works:
  the 200 ms release debounce (`voice_release_debounce_seconds`, ADR-0003 Fix
  B) absorbs the firmware bounce; two distinct physical releases inside the
  upstream macOS 350 ms double-tap budget still send two TAPs, two distinct
  toggles.
- **TOGGLE mode** behaviour is unchanged for the user (it already sent TAPs
  on both edges).
- **HOLD mode** users running a true push-to-talk host application no longer
  get a sustained key-down.  In practice there is no such user on the supported
  target set today, so the deletion is safe.
- `voice_controller.HOLD` semantics still exist internally; the test suite
  keeps its KEY_DOWN / KEY_UP assertions against the controller alone.
- Three `test_app_wiring.py` cases (`test_custom_hold_chord_stays_down_across_
  audio_stop_until_physical_release`, `test_mac_style_direct_hid_mic_edge_is_
  the_only_hold_shortcut_source`, `test_mac_style_f5_fallback_owns_hold_
  shortcut_not_atvv_or_raw_input`) move from asserting a down/up pair to
  asserting two TAPs, matching the new boundary contract.

## Rejected alternatives

- **Add a new `voice_trigger_mode` value** to keep the old HOLD behaviour
  opt-in.  Adds a config knob for a path with zero current users; rejected.
- **Move the change into `voice_controller`** (e.g. a new `press_tap_mode`
  parameter): makes the state machine more tangled without buying anything.
- **Skip a release-time TAP and rely on the press-side TAP alone**: leaves the
  target application in a half-toggled state on every release.

## Validation and rollback

- The 129 focused + 11 voice_hotkey_normalize + 8 audible-applications tests
  all run with the new boundary; the only previously-green tests that changed
  expectations are the three listed above, all asserting the new TAP/TAP
  contract.
- Real-device acceptance on Typeless (the user's confirmable target) must show
  exactly one voice-window-open / one transcription per physical press-and-
  release pair, verified live in the Typeless settings UI.
- Real-device acceptance on 千问 voice mode is out of scope for this two-day
  delivery but should reproduce identically because the toggle contract is
  the same.
- Rollback: revert the commit produced under this ADR, replay commit ``e91b9c2``
  and earlier; the only configuration affected is the in-memory `_voice_
  hotkey` object that `_handle_mic_button_pressed` reads, which is loaded once
  at bridge start, so no user-side config change is required.

## Out of scope

- Hardware-level RC003 firmware changes to enable a true short-tap signal are
  out of scope (`docs/decisions/ADR-0001-incremental-baseline.md` and the
  `Status` tables in `CURRENT_STATUS.md`).
- Signing or unsigned-artifact pipelines are out of scope.
