# ADR-0007: Qianwen Target-Local Right-Alt Physicalizer

- Status: accepted, target acceptance pending
- Date: 2026-08-23
- Amends: ADR-0006 for the Qianwen preset only
- Related: ADR-0003 (non-blocking F5 owner), ADR-0005 (generated-key physicalization)

## Context

Qianwen is configured to use Right Alt and has "short press also invokes voice
input" enabled. Manual testing proved that both a short and a sustained press on
the physical keyboard open Qianwen correctly.

RemoteMic then delivered several complete `ralt` taps while receiving valid
RC003 audio. `app.log` records the opening and closing host actions, but
Qianwen's `qianwen_voice_overlay.log` contains no corresponding session. The
same Qianwen log does contain sessions for the user's physical-keyboard tests.
Therefore timing, audio selection and Qianwen's short/long setting are not the
failed boundary: Qianwen rejects the separately generated Right-Alt input.

Qianwen's UI client runs at high integrity. An elevated RemoteMic bridge removed
the UIPI boundary and gained direct RC003 HID access, but elevated generated
Right-Alt down/up still produced no Qianwen session. A global in-place F5 hook
record transform also failed. Qianwen therefore evaluates the injected flag in
its own process-local low-level keyboard callback.

The RC003 microphone button already reaches RemoteMic's low-level keyboard hook
as a physical F5 down/hold/up lifecycle. RemoteMic must continue swallowing F5
so it cannot leak into the foreground application.

## Decision

The built-in presets use target-specific host protocols:

- Typeless: `lctrl+lalt + TOGGLE`; one complete tap at press and one at release.
- Qianwen: `ralt + TOGGLE`; one complete tap at the press boundary and one at
  release, matching its enabled short-press option.

For Qianwen 0.7.5.20, the exact installed EXE's
`SetWindowsHookExW(WH_KEYBOARD_LL, ...)` call loads
runtime RVA `0x85684` as its callback, and the adjacent handler code verifies
Right Alt (`0xA5`). The separately shipped PDB does not match the installed
EXE's runtime layout, so it is not used as the address authority. This applies
to `QianwenIMEUiClient.exe`. RemoteMic may attach a runtime Frida interceptor only
when the executable name, installation directory and SHA-256 exactly match the
verified build. Inside that callback it clears `LLKHF_INJECTED`,
`LLKHF_LOWER_IL_INJECTED` and `dwExtraInfo` only when the key is `VK_RMENU` and
`dwExtraInfo` is RemoteMic's private marker. Any mismatch or attach failure
makes the Qianwen voice action fail closed.

RemoteMic does not modify Qianwen files or depend on private Qianwen IPC. This
adapter is runtime-only and version-locked; the signed executable remains
unchanged on disk.

## Consequences

- The Qianwen preset retains the user-selected two-boundary TAP interaction.
- Typeless behavior and accepted timing are unchanged.
- A Qianwen update changes the executable hash and fails closed until its PDB,
  callback RVA and behavior are revalidated; RemoteMic must never patch the
  signed Qianwen executable.
- Qianwen target acceptance remains pending until the user observes its window,
  RC003 audio transcription and clean release behavior.

## Verification

Automated tests prove preset-to-protocol selection, `ralt+toggle` persistence,
path/hash gating, callback RVA and RemoteMic-marker filtering.
The required manual test is one RC003 hold with Qianwen already running: the
window must open, remain active while speaking, close on release and insert one
transcription. Logs must show one physical F5 session without generated ralt
TAP messages, plus a matching Qianwen overlay session.
