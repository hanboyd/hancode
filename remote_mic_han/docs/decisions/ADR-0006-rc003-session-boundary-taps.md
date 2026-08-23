# ADR-0006: RC003 Session Boundary Taps

- Status: accepted
- Date: 2026-08-23
- Supersedes: ADR-0004's hold-to-dictate conclusion
- Amended by: ADR-0007 for the Qianwen preset only
- Related: ADR-0003 (edge debounce, hook decoupling and drain), ADR-0005
  (bridge-owned hotkey physicalization)

## Context

The RC003 microphone button has a fixed device protocol: the user must hold the
button long enough for the remote to open ATVV audio, keep holding it while the
remote streams voice, and release it to end that stream. RemoteMic cannot change
this hardware lifecycle.

The user's two current Windows voice-input tools use a different host protocol:
one complete shortcut tap opens their input window, the window remains open while
audio arrives, and a later complete shortcut tap closes the window and starts
speech-to-text conversion. The time between those taps is intentional. The user
waits until the input window is visible before speaking and releases the RC003
button when finished.

ADR-0004 was superseded after an acceptance attempt was interpreted as evidence
that the host required a sustained shortcut. The user has now clarified the
actual product contract: both current targets use discrete toggle taps. The
failed attempt did not prove hold-to-dictate; it proved that RemoteMic's injected
shortcut still was not accepted reliably. ADR-0005 addresses that separate
physicalization boundary.

The architecture must describe RemoteMic's translation responsibility, not the
internals of any named input method. Target applications are acceptance surfaces,
not dependencies of the core state machine.

## Decision

RemoteMic translates one accepted RC003 physical voice session into a configured
host shortcut protocol.

For `VoiceTriggerMode.TOGGLE`:

1. Accept exactly one physical microphone press edge through the single-owner
   HID/F5 routing and release debouncer.
2. Open the selected bounded audio playback path.
3. Deliver exactly one complete host shortcut `TAP` at the accepted press
   boundary. Only after the endpoint and TAP succeed may RemoteMic request/open
   device audio.
4. Forward audio for the duration of the physical hold. Intermediate ATVV stop
   notifications do not close the host session while the physical button remains
   down.
5. Prefer the debounced physical release as the normal close boundary. If ATVV
   `AUDIO_STOP` remains stable for 2.5 seconds, treat it as the stronger fallback
   boundary and close without waiting indefinitely for a translated Windows F5
   KeyUp. Any `AUDIO_START` inside that window cancels the fallback.
6. Drain bounded playback audio, then deliver exactly one complete closing
   `TAP`.
7. If the closing TAP fails, retain the controller's pending-close ownership and
   request reconnect so cleanup can retry it.

Typeless uses `lctrl+lalt + TOGGLE`. The initial Qianwen assumption in this ADR
was disproved by target logs: Qianwen rejects the generated `ralt` taps even
though its physical keyboard shortcut accepts both short and long presses.
ADR-0007 replaces the Qianwen portion with `ralt + HOLD` through an in-place
physical F5 hook-record transform. Configuration
load and save repair the interrupted legacy pair `voice_hotkey=lctrl+lalt` plus
`voice_trigger_mode=hold` to TOGGLE, while repairing `voice_hotkey=ralt` to
HOLD for Qianwen.

`VoiceTriggerMode.HOLD` remains as an explicit compatibility protocol for targets
that genuinely require a sustained host shortcut. It continues to produce
`KEY_DOWN` at session start and `KEY_UP` at session end. Custom shortcuts remain
under user control; RemoteMic does not infer a target application from a process
name or window title.

## Invariants

- One physical RC003 hold owns at most one host session.
- A TOGGLE session emits at most one opening TAP and one closing TAP.
- Firmware bounce and repeated translated F5 edges cannot create extra sessions.
- After a stable audio-stop fallback close, stale F5 repeats cannot reopen the
  session; only a debounced physical release or a later authoritative ATVV
  `AUDIO_START` may clear that latch.
- If Windows KeyUp remains delayed, a later ATVV `AUDIO_START` is accepted as
  authoritative evidence of a genuinely new RC003 voice session and may clear
  the latch; repeated F5 alone cannot do so.
- No host shortcut is held between the two TOGGLE boundaries.
- Audio drain is bounded and happens before the closing host action.
- Shutdown, disconnect and failure cleanup use the same controller-owned closing
  action as the normal release path.
- Core code and production logs describe device/session/actions, not assumptions
  about a named input method.

## Consequences

- The physical long press and the host's two short presses are intentionally
  different protocols joined by RemoteMic.
- Users can see the host input window after the opening TAP and begin speaking
  before the closing TAP occurs on physical release.
- ADR-0004's claim that the current target requires hold-to-dictate is rejected.
- ADR-0005 remains required: a correctly timed TAP still fails if the target
  rejects bridge-owned injected keys before physicalization.
- Real target-application acceptance remains `deferred` until the user manually
  performs the test and observes the window and transcription.

## Verification

Automated:

- Configuration tests prove `lctrl+lalt + hold` is repaired to TOGGLE and
  `ralt + toggle` is repaired to HOLD for the Qianwen preset.
- Voice-controller tests prove TOGGLE produces TAP on press and TAP on close,
  while HOLD retains KEY_DOWN/KEY_UP compatibility.
- Application wiring tests prove audio stop is deferred while the physical button
  is held, the physical release closes once, and failed closing actions retain
  retry ownership.
- Edge-debounce, playback-drain, legacy-key suppression and hotkey
  physicalization regressions must remain green.

Manual, performed by the user:

1. Focus an empty text field in a configured short-press voice-input tool.
2. Long-press the RC003 microphone button and wait for exactly one input window.
3. After the window appears, speak one short test sentence while still holding.
4. Release the RC003 microphone button.
5. Confirm the window closes once and exactly one transcription is inserted.
6. Repeat with the second configured short-press tool.

Live logging may be observed during this manual test to correlate one opening TAP,
audio frames, one debounced release and one closing TAP. UI automation is not
required.
