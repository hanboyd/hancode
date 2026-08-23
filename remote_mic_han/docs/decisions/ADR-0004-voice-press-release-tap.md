# ADR-0004: Voice Press / Release Boundary TAP

- Status: superseded
- Date: 2026-08-23
- Superseded by: revert commit 287db60 (same day), after the real-device
  acceptance run under commit ``5a1f039`` made it clear that the
  proposed behaviour does not match the user's actual target
  application.
- Related: ADR-0003 (voice edge debounce + hook decoupling)

## Why this ADR was originally proposed

The RC003's physical microphone button only delivers a "long-press to start
streaming" signal: ATVV emits `MIC_BUTTON` + `AUDIO_STARTED` while the
button is held, `AUDIO_STOPPED` + `MIC_CLOSE` on release.  There is no
"short tap" mode on the hardware, so every physical press is a hold of
indeterminate length.  The proposed change was to force every voice-edge
delivery to be a **complete key-pair TAP** (one down + ~70 ms hold + up)
regardless of `voice_trigger_mode`, so each physical press-and-release
pair would cycle a host application's toggle exactly once.

## Why this was superseded

The proposed design assumed every shipping Windows target application is
**toggle-style** (one down+up pair => one toggle).  Real-device acceptance
under commit ``5a1f039`` against the user's running configuration of
Typeless + 千问 (the same two IMEs they actually use on this machine)
demonstrated that the press-side TAP did open a voice window, but the
release-side TAP **did not close it** in Typeless.

The user's downstream investigation surfaced the actual default mode of
Typeless: **hold-to-dictate**.  Typeless's stock behaviour is to start
recording while the configured shortcut is held down, then stop and
submit the transcription on release.  Under a toggle-edge contract, the
release-side TAP is interpreted as a brand new "second toggle" rather
than as a hold release - the typeless state machine does not see the
two TAPs as the two ends of a single physical hold.  The proposed
behaviour therefore cannot drive Typeless in its default mode, and
千问 voice mode shares the same toggle-style contract, so the same
problem would surface there as well.

## What replaces this design

The voice_hotkey delivery for the Typeless + 千问 target surface returns
to ``voice_controller``'s original HOLD-mode semantics: a sustained
KEY_DOWN at the press side, a KEY_UP at the physical release side.  This
maps onto Typeless's hold-to-dictate contract exactly:

```
press edge   -> sustained send_voice_key_combo_down(("lctrl", "lalt"))
hold period  -> nothing
release edge -> send_voice_key_combo_up(("lctrl", "lalt"))
```

The downgrade was made by ``git revert 5a1f039`` -> commit ``287db60``,
which restored the unmodified ``_handle_mic_button_pressed`` /
``_close_voice_host_session`` paths and removed the new TAP-only
assertion in ``tests/test_app_wiring.py``.  No new test or code path was
introduced in its place.

## What still applies from this ADR

- ``voice_controller.on_audio_stopped()`` is still called on every
  host-edge close so the controller's HOLD latch and TOGGLE closing
  state advance exactly as before.  This is unchanged by the revert.
- The 200 ms release debounce (ADR-0003 "Window refinement") remains the
  mechanism that absorbs RC003 firmware release/press bounce during the
  physical hold.  The revert does not touch this.

## Out of scope after the revert

- A future target application that genuinely is toggle-only might
  benefit from a press-TAP / release-TAP contract of the kind this ADR
  originally proposed.  That would warrant a new ADR with a different
  scope (config flag or new mode explicitly named after the toggle
  contract), and would have to be exercised against the specific
  application before being accepted.  Such work is deferred until a
  concrete toggle-only target application appears in the supported set.

## Sources

- Typeless default activation model confirmed via its official help /
  documentation: "Hold a key, release to stop."  This is the model's
  direct contract and overrides the earlier "Typeless toggles on every
  short-tap" assumption baked into the original draft of this ADR.
- Live ``CURRENT_STATUS.md`` 2026-08-23 "Typeless: pop then no close"
  entry, which is the real-device evidence that triggered this
  supersession.
