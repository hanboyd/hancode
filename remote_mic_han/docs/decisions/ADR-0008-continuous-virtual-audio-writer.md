# ADR-0008: Continuous Blocking Virtual-Audio Writer

- Status: accepted
- Date: 2026-08-23
- Related: ADR-0006 (session-boundary taps), ADR-0007 (Qianwen Right-Alt physicalizer)

## Context

The RC003 transport repeatedly delivered valid decoded PCM while Qianwen or
Typeless intermittently reported no audio. Reopening or writing the PortAudio
stream only when a BLE notification arrived did not provide a sufficiently
stable live endpoint. A healthy process and successful `write()` return were
not proof that CABLE Output was receiving a continuous signal.

The macOS implementation keeps its audio engine and player node alive, feeds
bounded buffers, checks device health and drains scheduled audio before the
session closes. Windows needs the same lifetime property while continuing to
use the existing PortAudio/VB-CABLE product baseline; this ADR does not add a
driver or change either voice-tool preset.

## Decision

`EndpointPlaybackSink` owns one dedicated blocking writer thread for the
selected virtual playback endpoint.

- The stream is prepared before the opening host shortcut.
- The writer emits fixed 20 ms chunks continuously and emits silence when no
  decoded RC003 audio is queued, keeping the endpoint active.
- BLE/audio callbacks only enqueue PCM; they do not perform blocking endpoint,
  file or process work.
- The queue is bounded to two seconds of output audio. On overflow, the oldest
  queued chunks are discarded so latency cannot grow without bound.
- Session close drains already queued audio before the closing host shortcut.
- The existing application protocols remain isolated and unchanged: Typeless
  uses `lctrl+lalt + TOGGLE`; Qianwen uses `ralt + TOGGLE` plus ADR-0007.

## Consequences

- The virtual endpoint stays warm across notification gaps and repeated voice
  attempts no longer depend on reopening a playback stream for every burst.
- A dedicated thread and bounded queue add small, explicit memory and lifecycle
  costs. Shutdown must stop and join the writer without blocking an audio
  callback.
- Dropping oldest audio during a two-second overflow favors current speech and
  bounded latency over replaying stale speech.
- Target applications may still fail to commit an otherwise completed
  transcription into the foreground editor. That is tracked separately from
  audio delivery and recognition.

## Verification

- Focused regression: 82 tests passed with 1 environment skip.
- Clean synthetic six-burst playback reached CABLE Output with peak 0.244141.
- In the user-driven Qianwen acceptance, six of six numbered attempts produced
  transcriptions and all six matching RC003 sessions logged `result=signal`.
  Notepad automatic insertion succeeded four of six times; attempts 1 and 3
  required manual paste from Qianwen.

The user accepts the result as usable. No package or installer was rebuilt for
this source-only change.
