# ADR-0020: Known input and voice-audio policy bugs

- Status: Accepted
- Date: 2026-09-02

## Context verified from the current source

The three reported symptoms cross different boundaries. Code inspection proves
that the product previously did not manage default capture roles or Windows
communications ducking. It also proves that ordinary RC003 keyboard edges can
reach the foreground physically while Raw Input later dispatches a mapped
`SendInput` action. The 60 ms Raw Input/low-level-hook correlation is timing
dependent and cannot identify a device from `WH_KEYBOARD_LL` alone.

Live, non-voice evidence on the development machine adds two constraints:

- synthetic 997 Hz PCM written to WASAPI `CABLE Input` was captured from
  WASAPI `CABLE Output` (`peak=0.141422`, `rms=0.026593`), so the installed
  virtual-cable playback/capture path is currently operational;
- all three default capture roles already resolved to `CABLE Output` at the
  time of inspection. Therefore endpoint routing is a supported repair and a
  previously missing responsibility, but it cannot be called the unique cause
  of a failure that still reproduces in that current state.

## Decision

### Identity keyboard mappings

For RC003 keyboard controls whose configured single-click action is exactly the
same key (`up/down/left/right/ok`) and which have no secondary gesture, the
physical keyboard edge is authoritative. Raw Input records the device event but
does not arm suppression and does not inject a replacement. This removes the
deterministic double action without globally swallowing the same key from a
normal keyboard.

Custom remaps and secondary gestures retain the existing device-scoped Raw
Input path. A complete race-free custom-remap solution requires a device-scoped
pre-legacy input owner (verified direct HID path or a separately approved
driver); globally suppressing VK_UP and similar keys is rejected.

### Voice audio policy lease

The native voice coordinator owns one `WindowsVoiceAudioPolicyLease`. Before
the transcription hotkey it:

1. recovers a stale prior marker;
2. snapshots Console, Multimedia and Communications default capture IDs;
3. snapshots presence/value of `UserDuckingPreference`;
4. writes a local recovery marker;
5. selects active standard `CABLE Output` for all capture roles;
6. sets ducking to `Do nothing` (`3`) and verifies endpoint selection.

The lease restores after the voice stop edge, disconnect and application stop.
Acquire/restore are idempotent. Restore only overwrites a value that still
equals the lease-owned value, preserving an explicit user change. There is no
delayed restore timer, so stale-timer generation races do not exist in this
implementation. Failure to acquire suppresses the hotkey and MIC_OPEN rather
than opening a recognizer against the wrong endpoint.

The Python coordinator rollback uses the same native lease when the extension
is available. A source-only run without `_C.pyd` retains the old behavior and
is not claimed as the fixed Windows product route.

## Verification boundary

Automated and local integration tests can prove ordering, idempotent cleanup,
crash-marker recovery, registry restoration and virtual-cable signal. They
cannot prove RC003 speech reaches Typeless/WeChat, that the reported double key
is absent on the physical remote, or that another player remains audible during
a real recognizer communication session. Those rows remain `deferred` until
observed with the physical device and target applications.
