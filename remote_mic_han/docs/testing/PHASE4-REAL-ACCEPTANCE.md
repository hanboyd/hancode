# Phase 4 Real Acceptance — Manual Procedure (0.5.0-candidate)

Scope: native Phase 4 WASAPI AudioRoute C++ implementation
(`remotemic_native._C.WasapiAudioRoute`) exposed via the python bridge
shim (`ovb_rc003.audio_route_native._NativeAudioRoute`) and routed
through the production call site
(`RC003App._open_playback_for_new_session` in `app.py`). **No new
test mode, no GUI change, no protocol change.** Phase 4 only replaces
the audio-playback owner (python PortAudio baseline → C++ WASAPI).

Companion to `apps/windows/rc003/CHANGELOG.md` (`[0.5.0-candidate]` /
`真机 / 第三方验收（deferred）`) and
`docs/decisions/ADR-0014-phase4-audio-wasapi-cpp.md`.

---

## Pre-flight finding (read first)

Phase 4 step 5 wired `app.py:_open_playback_for_new_session` through
`audio_route_native.make_audio_route` and added the bridge shim. The
factory defaults to `python` (the existing `EndpointPlaybackSink`
baseline) per migration plan §1 rule 4. Build-time parity (G3) is
proven by `remotemic_upsample_parity` (8/8 byte-exact) and
`remotemic_audio_route_parity` (10/10 sample-count / peak / RMS /
drop-count / drain-order parity between `FakePlaybackSink` and
`FakeAudioRoute`). Production-routing (G5) is proven by
`remotemic_phase4_native_switch` (9/9) + `remotemic_phase4_production_routing`
(4/4 source-level proof).

What this document verifies: the **G6 gate** — real Windows WASAPI
device + VB-Cable + Typeless (or Qianwen) + RC003 BLE. **No new
behavior; only the implementation owner changed.**

Verify on your Windows desktop before doing any real-device work that
the env-var override reaches the production path:

```powershell
$env:REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE = "native"
$env:PYTHONPATH = "C:\Users\hanboyd\hanboyd-code\remote_mic_han\apps\windows\rc003\src;C:\Users\hanboyd\hanboyd-code\remote_mic_han\build\Release"
python -c "from ovb_rc003.audio_route_native import make_audio_route; \
import remotemic_native as rn; \
print('factory_default -> _NativeAudioRoute:', make_audio_route.__module__); \
print('remotemic_native._C_AVAILABLE:', rn._C_AVAILABLE)"
```

Expected:

- `factory_default -> _NativeAudioRoute: ovb_rc003.audio_route_native`
- `remotemic_native._C_AVAILABLE: True`

If `_C_AVAILABLE: False` you still get the bridge shim, but the shim
silently falls back to python per `audio_route_native.py:54-60`. The
shim is constructed, the C++ side is not. Run `cmake --build build
--config Release` first.

---

## Phase 4 module to flip to `native`

Set this one env var before launching the app (see "How to launch"
below). The audio_route module is the only Phase 4 native switch.

| Env var                                | Module         | Default today |
|----------------------------------------|----------------|---------------|
| `REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native` | `audio_route` | `python` |

The default policy table (`_remotemic_native_runtime.py:49-63`) keeps
`audio_route` at `"python"`. The env var is the only override mechanism
for the real path.

**Forbidden in real path:** `shadow`. The user contract explicitly bans
shadow dual-owner for side-effecting modules, and Phase 4 §3 rule 5
hardens this for WASAPI: a python shadow would actually open **two**
audio streams at once (real device handles), distorting the user-facing
latency measurement. `shadow` exists only inside the in-process parity
test harness (`test_audio_route_native_parity.py`), where the recording
doubles are side-effect-free.

## Modules that MUST stay `python`

Do NOT set any Phase 3 `REMOTEMIC_NATIVE_CHOICE_*` env vars. Phase 3
modules stay at their `python` default; this document is purely about
the Phase 4 audio_route swap. (Phase 3 closeout is its own
`PHASE3-REAL-ACCEPTANCE.md` document.)

## How to launch with the switch set

Open PowerShell, set `REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native` +
`PYTHONPATH` in the same shell where you launch the app, then start
the normal launcher. The venv's `python.exe` does not include
`<repo>/apps/windows/rc003/src` on `sys.path` by default; without
`PYTHONPATH`, `python -m ovb_rc003` fails with `No module named ovb_rc003`.

```powershell
$env:REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE = "native"
$env:PYTHONPATH = "C:\Users\hanboyd\hanboyd-code\remote_mic_han\apps\windows\rc003\src;C:\Users\hanboyd\hanboyd-code\remote_mic_han\build\Release"
cd C:\Users\hanboyd\hanboyd-code\remote_mic_han\apps\windows\rc003
.\.venv\Scripts\python.exe -m ovb_rc003
```

Replace `<repo>` with the actual repo root if it differs. The
`build\Release` entry is required when `_C.cp311-win_amd64.pyd` is
present — without it, `remotemic_native._C_AVAILABLE` reports `False`
and the bridge shim silently falls back to python (single-owner
contract preserved: the fallback is **inside the shim**, not a parallel
native instance).

Do NOT use `--dry-run` — it bypasses `RC003App.run_forever()` and only
probes the bindings; it does not exercise the audio playback path.

## How to verify native is actually running

Single source of truth: `app.log` at
`%LOCALAPPDATA%\RemoteMic\RC003\logs\app.log`. The C++ WASAPI path
emits the same audio lifecycle log lines as the python baseline
(`audio playback sink opened`, `audio playback sink drained`,
`audio playback sink closed`) — the difference is which side actually
opened the WASAPI handle.

To verify which implementation is driving the audio playback, use the
probe one-liner **before** launching the app, in the same shell with
the env vars set:

```powershell
$env:REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE = "native"
python -c "from ovb_rc003.audio_route_native import make_audio_route, _NativeAudioRoute; \
import remotemic_native as rn; \
print('factory_default -> _NativeAudioRoute:', make_audio_route.__module__); \
print('remotemic_native._C_AVAILABLE:', rn._C_AVAILABLE); \
print('factory is _make_audio_route_native:', make_audio_route is _NativeAudioRoute)"
```

Expected:

- `factory_default -> _NativeAudioRoute: ovb_rc003.audio_route_native`
- `remotemic_native._C_AVAILABLE: True`
- `factory is _make_audio_route_native: True`

If the third line prints `False`, the override did not take effect.
Check that the env var is set in the same shell as the probe and that
the value spelled exactly matches the table above (case-insensitive,
but the spelling matters).

Single-owner check (no shadow dual-owner in real path): the bridge
shim holds exactly one `_impl` (the C++ `WasapiAudioRoute`). Inspect:

```powershell
$env:REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE = "native"
python -c "from ovb_rc003.audio_route_native import _NativeAudioRoute; \
import remotemic_native as rn; \
print('_NativeAudioRoute._impl class:', type(_NativeAudioRoute.__init__.__globals__['rn'].WasapiAudioRoute).__name__)"
```

Expected: prints `WasapiAudioRoute` rooted in `remotemic_native._C`.
If it prints anything else, the binding did not load and the wrapper
silently fell back to python per `audio_route_native.py:54-60`.

---

## Real-device validation procedure

Pre-reqs: RC003 device paired and bound; Windows VB-Cable installed
and `CABLE Output` registered as a WASAPI endpoint (configure Typeless
or Qianwen to use `CABLE Output` as the microphone input); `app.log`
tail running in a second terminal:

```powershell
Get-Content "$env:LOCALAPPDATA\RemoteMic\RC003\logs\app.log" -Wait
```

The validation matrix below covers the three signals that the C++
WASAPI owner is functionally equivalent to the python baseline:

1. **Open / drain / close lifecycle** — Typeless / Qianwen receive the
   audio without truncation or stalls.
2. **Backpressure** — the bounded PCM queue drops the oldest frames
   cleanly when the consumer is slow (the C++ queue implements
   `drop_oldest` semantics matching the python
   `audio_playback.EndpointPlaybackSink`).
3. **Shutdown** — `close()` is idempotent and releases the WASAPI
   handle cleanly (no `CleanupIncompleteError`, no leaked endpoint).

| # | Action | Observation |
|---|--------|-------------|
| 1 | Launch with `REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native` set (above) | `startup: resolving RC003 identity` then `startup: exactly one RC003 candidate resolved` in `app.log`. No import error from `remotemic_native._C`. |
| 2 | Connect RC003 (power on, hold to wake) | BLE session reaches `connected` state. `app.log` shows no `on_session_error` lines. |
| 3 | Press the RC003 mic button (HOLD mode default) | `audio playback sink opened` in `app.log` (lifecycle hook fires from the bridge shim's `open()`). Typeless / Qianwen voice input opens within ~200 ms. |
| 4 | Speak for 5 s | Audio reaches `CABLE Output` cleanly. Typeless / Qianwen display interim transcription with no truncation. No `drop_oldest` warnings in `app.log` (the bounded queue has capacity for the BLE notification cadence; drop warnings only fire under genuine backpressure, e.g. step 5). |
| 5 | Trigger sustained backpressure: rapidly press-and-release the mic 5 times in 5 s while Typeless is processing a heavy transcription (forces the python consumer to slow down) | `audio playback sink drained` between presses. If the bounded queue overflows, `drop_oldest` lines appear in `app.log` with the dropped-frame count; the audio reaching Typeless is the latestest window, not the oldest. **No** crash, no infinite stall, no `OSError: handle invalid`. |
| 6 | Release the mic | `audio playback sink drained` then `audio playback sink closed` in `app.log`. Typeless / Qianwen stop transcribing within ~200 ms. |
| 7 | Switch trigger mode to TOGGLE (via config: `voice_trigger_mode = "toggle"`, restart app with the env var still set) | Re-test step 3: single press opens, single press closes. Audio lifecycle lines alternate correctly. No double-trigger on the first edge (Phase 3 contract). |
| 8 | Press and hold for > 2.5 s, then force-kill Typeless / Qianwen (Task Manager → End Task) | The `LATE_AUDIO_GUARD_SECONDS = 2500` fallback fires (Phase 3 contract). `app.log` shows an `AudioStopped` event arriving ~2500 ms after `AudioStarted`. WASAPI handle released cleanly; no zombie process holding `CABLE Output`. |
| 9 | Move RC003 out of range (or power off) | `on_disconnected` fires. `_cleanup_once` runs. WASAPI `close()` is invoked from `_open_playback_for_new_session`'s companion teardown. `app.log` shows clean shutdown of the audio playback sink with no leaked handle. No `CleanupIncompleteError`. |
| 10 | Bring RC003 back into range | Supervisor reconnects. `make_audio_route` is invoked afresh — **fresh instance per factory call** (single-session owner contract; reconnect / cleanup never reuses a stale route). `app.log` shows a new `audio playback sink opened` and a working voice session. |
| 11 | Close the app (Ctrl+C in the launching shell) | `_cleanup_once` runs. The bridge shim's `close()` invokes the C++ `WasapiAudioRoute::close()` which calls `IAudioClient::Stop` + `Release`. `app.log` shows clean shutdown lines and no `CleanupIncompleteError`. If it appears, note the exception text and stop — do not auto-fix. |

## Restore procedure (return to defaults)

When you are done — whether all steps passed, failed, or were aborted —
restore the production path to the default `python` baseline:

1. Unset the env var in the shell you used to launch:

   ```powershell
   Remove-Item Env:REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE
   ```

2. Close the running app (Ctrl+C). Confirm `app.log` shows clean
   shutdown with no `CleanupIncompleteError`.

3. Relaunch with no env var set (keep `PYTHONPATH` from step 0 if
   you set it):

   ```powershell
   cd C:\Users\hanboyd\hanboyd-code\remote_mic_han\apps\windows\rc003
   .\.venv\Scripts\python.exe -m ovb_rc003
   ```

4. Confirm the probe one-liner from "How to verify native is actually
   running" reports `_C_AVAILABLE: True` AND that
   `factory is _make_audio_route_native: False` (i.e. the factory
   reverts to the python baseline).

5. Run a single smoke press / release on the RC003 to confirm the
   default Python path is still functioning. If anything regresses,
   the env var was unset; the python baseline must work. Report the
   regression per Rule 1 (fix → retest → decide per Rule 1/2); do not
   pre-bump to `0.4.1-candidate`.

---

## Recording template

After each real run, fill one row per item. `deferred` stays valid for
items not yet exercised; only mark `passed` after actual observation.

| Item | Result | Notes (date, observed behaviour, log excerpts) |
|------|--------|------------------------------------------------|
| C++ WASAPI audio owner — open lifecycle | | |
| C++ WASAPI audio owner — drain / write | | |
| C++ WASAPI audio owner — drop_oldest backpressure | | |
| C++ WASAPI audio owner — idempotent close | | |
| C++ WASAPI audio owner — late-audio fallback (2.5 s) | | |
| C++ WASAPI audio owner — disconnect cleanup | | |
| C++ WASAPI audio owner — reconnect fresh instance | | |
| C++ WASAPI audio owner — app exit cleanup | | |
| Typeless (via CABLE Output) — opens on press | | |
| Typeless (via CABLE Output) — no truncation on 5 s hold | | |
| Typeless (via CABLE Output) — backpressure survives | | |
| Typeless (via CABLE Output) — closes on release | | |
| Qianwen (via CABLE Output) — opens on press | | |
| Qianwen (via CABLE Output) — no truncation on 5 s hold | | |
| Qianwen (via CABLE Output) — backpressure survives | | |
| Qianwen (via CABLE Output) — closes on release | | |

After each item: if `failed`, **stop the run, paste the `app.log`
excerpt and a 1-line symptom in the Notes column.** Do not auto-fix.
Report to me; the next step depends on what failed.

Once all sections are filled in with actual observations, paste the
table back. I will update `apps/windows/rc003/CHANGELOG.md` and
`docs/decisions/ADR-0014-...md` accordingly — not before.