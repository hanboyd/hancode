# ADR-0003: Voice Edge Debounce and Hook Decoupling

- Status: accepted
- Date: 2026-08-22
- Supersedes: (none)
- Related: ADR-0001 (preserve Windows baseline), ADR-0002 (interface boundaries)

## Context

Real-RC003 acceptance in Phase 3 surfaces two distinct key-conflict defects
that the current single-owner routing does not yet solve:

1. **Notepad F5 date insertion** — long-pressing the RC003 microphone in
   Notepad causes `F5 → Edit → Time/Date` to fire, even though the
   `legacy_key_suppressor_windows.py` low-level keyboard hook is running
   (`startup: RC003 voice legacy-key guard enabled` is logged and
   `consume key edge: vk=0x74 ... matched=False armed=0` keeps firing for
   the suppressed physical key).
2. **Typeless multi-session flicker** — a single physical hold produces
   more than one independent Typeless voice window. The live log shows the
   F5 LL hook receiving distinct down/up edge pairs at e.g. 23:01:23.339 /
   23:01:26.685 / 23:01:37.238 during a "single hold"; each edge pair
   issues a separate `lctrl+lalt` down/up, so Typeless sees three
   consecutive voice sessions instead of one.

The macOS upstream `HD838A/remote-mic-app` solves both problems with two
cooperating mechanisms:

- `KeyboardEventSuppressor` (CGEvent tap-level suppression, 180 ms window)
- `VoiceFnTapSessionController` (start-delay, pre-roll, generation
  tracking, drain-before-stop)

The Windows upstream `nijez/open-voice-bridge` does **not** solve them:
its `app.py` only handles ATVV control events for the voice key, never
installs a low-level keyboard hook, and never suppresses the translated
F5. Its own README explicitly marks the Windows port as "源码/构建候选，
未真机验收" — source/build candidate without real-hardware acceptance. On
real hardware this means every RC003 voice press would leak F5 to Notepad
and to any target application. Local `apps/windows/rc003` therefore adds
the LL hook, the `voice legacy F5 trigger received` path, and the
direct-HID tap as **necessary** divergence from the upstream Windows
implementation; it is not optional rework.

The defects in the local divergence are implementation bugs, not
architecture choices:

### Defect 1 root cause (Notepad F5 leak)

`LegacyKeySuppressor._hookproc` calls the application's
`_on_legacy_key_event` synchronously on the hook thread. The application
callback chains into `_on_button_event("mic", True)` which acquires
`self._voice_trigger_lock`. The ATVV `AudioStarted` callback acquires the
same lock and, on first press only, executes
`_open_playback_for_new_session()` inside the lock — PortAudio stream
initialization measured at ~486 ms on this machine (live log
22:49:57.823 → 22:49:58.384).

The WH_KEYBOARD_LL callback therefore blocks ~480 ms waiting for the
playback open. Windows' documented tolerance for a low-level hook callback
is roughly 200–300 ms; once exceeded, Windows dispatches the keyboard
message anyway. The original physical F5 reaches Notepad before the LL
hook returns 1, so Notepad inserts the current date/time. The defect
appears only on the **first** physical press of any bridge run, because
`_open_playback_for_new_session()` reuses an already-open sink on later
presses and returns within microseconds.

### Defect 2 root cause (Typeless multi-session)

RC003 firmware can produce a brief real release-then-press during a
"hold" (observed in live log at 23:01:23 / 23:01:26 / 23:01:37 as three
distinct F5 down-up edge pairs spanning ~14 s of physical hold). The
local `voice_controller.VoiceController` correctly emits one
`KEY_DOWN` / `KEY_UP` per edge pair, but does **not** debounce the
physical edges: any release above 0 ms is treated as a session end and
the next press starts a new session. Typeless therefore sees three
sessions.

The macOS upstream avoids this with `VoiceFnTapSessionController`'s
`generation &+= 1` and `startDelay = 0.15 s` — the 150 ms start delay plus
generation tagging rejects any press that arrives inside the previous
session's wind-down window. The local Windows code has neither.

## Decision

Keep the local divergence from the upstream Windows port
(`legacy_key_suppressor_windows.py` + `button_gesture.py` + direct HID tap).
Apply two surgical, additive fixes; do not change the public interfaces
documented in ADR-0002; do not replace working production modules.

### Fix A — Decouple LL hook thread from application locks

`LegacyKeySuppressor._on_legacy_key_event` (or, more precisely, the
application-side callback installed via `LegacyKeySuppressor(...,
on_key_event=self._on_legacy_key_event, ...)`) must never acquire any
application thread lock, must never call any I/O that can block, and must
return within ≪ 200 ms. The hook thread's only job is to record the edge
in atomic, lock-free state and queue an event for the application worker
to consume asynchronously.

Concretely:

- Introduce a single-producer / single-consumer queue (or equivalent
  `queue.Queue` + a dedicated worker thread) inside `RC003App`.
- The LL hook callback's only writes are atomic flags
  (`_legacy_f5_is_down`, `_voice_legacy_transform_key_down`, `_direct_hid_tap_active`)
  and a non-blocking `queue.put_nowait(...)` of a small `(edge, time)`
  tuple. The callback returns 1 to swallow the F5 immediately.
- A new `_voice_event_worker` thread consumes the queue, takes the
  application locks on its own thread, and dispatches the existing
  `_on_button_event("mic", ...)` path. No application code change beyond
  moving the call site off the hook thread.

This fix addresses the Notepad F5 leak directly: with no application lock
acquired on the hook thread, every LL hook callback completes in
sub-millisecond time and never crosses Windows' 200 ms tolerance.

### Fix B — Debounce the voice-key release edge

On the consumer side (the `voice-edge-worker` thread created by Fix A),
defer the release-to-host action by `VOICE_RELEASE_DEBOUNCE_SECONDS =
0.200` (200 ms by default). If a new press edge arrives within that
window, treat the release as part of the same physical hold and continue
the existing host session.

The window is configurable via the new top-level config key
`voice_release_debounce_seconds` (range 0.050 – 0.500 s; values outside
the range fall back to the default 0.200). The macOS upstream's
`VoiceFnTapSessionController.startDelay = 0.15 s` is the closest
analogue, but the local Windows port needs a release-side debounce
because the press side already serialises correctly through
`_voice_physical_button_down`.

This fix addresses the Typeless multi-session flicker: a sub-200 ms
hardware bounce no longer ends the host session; only a true user
release does.

#### Window refinement 2026-08-23

The first issue of this ADR picked 50 ms as the smallest bounce the
firmware was observed to produce, with the rationale that a wider window
would suppress legitimate quick re-presses (intentional double-tap to
pause/resume voice on macOS, see upstream's "双击停用/恢复桥接").
The 2026-08-23 live `%LOCALAPPDATA%\RemoteMic\RC003\logs\app.log`
recorded two new bounces of 62 ms (06:13:34,914 → 06:13:34,976) and
65 ms (06:31:33,421 → 06:31:33,486) — measured between consecutive
`voice legacy F5 up` and `voice legacy F5 down` log line entries during
a *single physical hold*. 50 ms would let both leaks through, so the
production default was raised to 200 ms (≈3× the worst observed
bounce). The macOS-style double-tap pause window stays available above
the 200 ms threshold (the real upstream macOS double-tap budget is
~350 ms, comfortably above 200 ms), so the user-facing gesture is not
removed — only the previously short bounce window is widened.

The 50 ms / 100 ms / 200 ms / 500 ms boundaries remain covered by
`tests/test_voice_edge_debouncer.py` plus a new configurable-window
case; the application default of 200 ms is encoded both in
`voice_edge_debouncer.VoiceEdgeDebouncer(release_window_seconds=0.200)`
and in `config.py`'s `voice_release_debounce_seconds` fallback, so a
later user-facing tooling change cannot silently drift the two values
apart.

### Fix C — Drain the playback sink before the host release edge

`VoiceController.on_audio_stopped()` (and therefore `_voice.on_audio_stopped()`)
currently returns `KEY_UP` synchronously the moment the application's
internal state machine marks the session closed. For target applications
whose like a toggle voice input (Typeless, the豆包输入法 `Fn` long-press
mode, and any similar tool whose UI shows an X/✓ confirmation pill
during recording), this causes the target application to immediately
close its voice UI when the user releases the remote — even though the
audio device may still be playing buffered voice samples. The user sees
the target UI pop up and immediately disappear, and never gets the
chance to confirm or cancel the transcription.

The macOS upstream avoids this with `VoiceFnTapSessionController`'s
explicit drain step:

> 松开时等待 ` `VirtualAudioOutput.endSessionAfterDraining` 排空队列，
> 再发送配对的 Fn 结束点按。

Concretely:

- Extend `audio_playback.EndpointPlaybackSink` with `drain(timeout_ms)`:
  wait up to `timeout_ms` for the PortAudio internal buffer to empty,
  returning `True` if drained and `False` if the timeout expired.
- Change `VoiceController` (or the application wiring in `app.py`) so
  that `KEY_UP` is emitted only after `drain` returns. Specifically:
  the `_voice_event_worker` thread calls `_on_button_event("mic", False)`
  only after `_playback.drain(RELEASE_DRAIN_TIMEOUT_MS)` returns.
- If the drain times out (which can happen if PortAudio buffering is
  unusually large), the release is still emitted; the trade-off is that
  the target application sees the UI close mid-drain. A reasonable
  timeout is 500 ms, matching the upstream macOS default.

The window is bounded so a single physical press never extends the
host session beyond ~500 ms after physical release under any
circumstance; once the drain completes (or times out), the next press
begins a fresh session.

This fix addresses the Typeless "release closes UI immediately" defect:
the X / ✓ confirmation pill remains visible during the drain window
so the user can review and either confirm or cancel the transcription.

### Fix ordering and interaction

- Fix A is a precondition: the hook thread cannot dispatch on its own
  after Fix A removes the application lock; Fix B and Fix C run on the
  worker thread that Fix A creates.
- Fix B and Fix C are sequential on the worker thread: when a release
  edge arrives, Fix B's debouncer schedules `handler_release`; that
  handler performs Fix C's drain first, then emits the host `KEY_UP`.
  A press arriving inside the debounce window cancels both the timer
  and any in-flight drain.

## Consequences

- **Notepad F5 leak** moves from `failed` to `passed` once Fix A ships
  and one real RC003 press in Notepad confirms no date/text is inserted.
- **Typeless multi-session flicker** moves from `failed` to `passed`
  once Fix B ships and a real RC003 hold in Typeless produces exactly
  one voice window with one complete transcription. The default 200 ms
  window is the 3× margin over the 65 ms firmware bounce observed on
  2026-08-23; it is overridable per user via the
  `voice_release_debounce_seconds` config key if a future firmware
  revision observes a still-wider bounce, without requiring another
  ADR.
- **Typeless "release closes UI immediately" defect** moves from
  `failed` to `passed` once Fix C ships and a real RC003 hold in
  Typeless shows the X / ✓ confirmation pill persisting after
  physical release until the drain completes.
- Existing 129 focused tests in `test_app_wiring` /
  `test_voice_controller` / `test_atvv_session` /
  `test_ble_transport_contract` / `test_legacy_key_suppressor` continue
  to pass without modification; the debounce logic is exercised by a new
  `test_voice_edge_debouncer.py` that uses an injectable clock.
- `_voice_trigger_lock` remains as the application-side state-machine
  mutex; it is now only ever held on the `_voice_event_worker` thread, so
  hook-thread blocking cannot recur even if `_open_playback_for_new_session`
  is slow in the future.
- The LL hook's `_on_key_event` callback contract changes: it must be
  non-blocking and lock-free. This is documented at the
  `LegacyKeySuppressor` constructor's `on_key_event` parameter.
- `EndpointPlaybackSink.drain()` adds a small public method on the
  audio sink; existing callers are unaffected because the method only
  reads sink-internal state.
- No public interface changes. No C++ rewrite. No new dependencies.

## Rejected alternatives

- **Drop the LL hook and trust ATVV only (revert to upstream Windows)**:
  rejected — produces an unfixed Notepad F5 leak on every voice press.
- **Add a watchdog that re-enables the hook after a timeout (Mac's
  `tapDisabledByUserInput` analogue)**: rejected — the root cause is that
  the hook blocks, not that Windows disables it. A watchdog would mask
  the symptom without addressing it; the next slow code path would
  reintroduce the leak.
- **Introduce `VoiceFunctionKeyLatch` + `VoiceSessionController` (full
  macOS parity)**: rejected for this two-day delivery — broader than the
  defect requires, harder to roll back, and not justified by observed
  behavior once Fix A lands.
- **Move `_open_playback_for_new_session` outside the
  `_voice_trigger_lock` critical section**: rejected as a standalone fix
  because it only removes one specific slow path; the next slow code path
  would reintroduce the same class of defect. Fix A is structural.

## Verification

- New unit test `tests/test_voice_edge_debouncer.py` covers:
  press → release within 5 ms (treated as bounce, single session)
  press → release within 50 ms (boundary, single session)
  press → release at 200 ms (treated as real release, two sessions)
  press → release → press at 100 ms (single continuous session)
  shutdown cancels pending release
  negative window raises
  configurable-window case: instantiating `VoiceEdgeDebouncer` with
  50 ms, 100 ms, 200 ms, 350 ms via the same configuration key shape
  and asserting each boundary behaves correctly (the production
  default is 200 ms; the 50 ms case is kept to verify the class still
  matches the originally-asserted ADR text and to support users who
  prefer the tighter budget on non-bouncing firmware)
- New unit test `tests/test_audio_playback_drain.py` covers:
  `drain` returns `True` immediately when no samples are buffered
  `drain` waits for buffered samples to be played out
  `drain` returns `False` after the configured timeout
- Re-run the 129 existing tests; must all pass.
- One real RC003 long-press in Notepad: confirm no date/time string is
  inserted. Capture live `app.log` for one press cycle; must show
  `_on_legacy_key_event` (or equivalent) firing on the worker thread,
  not on `Thread-3 (_run)`.
- One real RC003 long-hold in Typeless: confirm exactly one voice
  window with one complete transcription, with the X / ✓ confirmation
  pill persisting after physical release until the playback drain
  completes; live log must show at most one `voice physical mic trigger
  received before audio start` followed by one `voice physical mic
  released; closing held host shortcut` for the single physical hold,
  with the worker thread name (`voice-edge-worker`) appearing on the
  closing edge log line.
