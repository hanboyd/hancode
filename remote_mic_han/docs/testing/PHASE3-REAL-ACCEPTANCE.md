# Phase 3 Real Acceptance — Manual Procedure (0.4.0-candidate)

Scope: native Phase 3 session state machine (`VoiceController` /
`VoiceEdgeDebouncer` / `AtvvSession`) over the existing Python BLE / audio
/ Windows input / Typeless / Qianwen paths. **No code change.** No new
test mode, no GUI change, no DashScope/Qwen ASR API, no Phase 4.

Companion to `apps/windows/rc003/CHANGELOG.md` (`[0.4.0-candidate]` /
`真机 / 第三方验收（deferred）`) and
`docs/decisions/ADR-0013-phase3-session-state-machine-boundary.md`.

---

## Pre-flight finding (read first)

The Phase 3 native switch modules (`voice_controller_native.py` /
`voice_edge_debouncer_native.py` / `atvv_session_native.py`) and their
`make_*()` factories exist, are unit-tested, and pass 24/24 ctest Debug
+ Release. **However, `app.py` and `ble_transport_winrt.py` import the
python modules directly and do not route through these factories.** See:

- `apps/windows/rc003/src/ovb_rc003/app.py:81-82` —
  `from . import voice_controller, voice_edge_debouncer`
- `apps/windows/rc003/src/ovb_rc003/app.py:122` —
  `self._voice = voice_controller.VoiceController(...)`
- `apps/windows/rc003/src/ovb_rc003/app.py:193` —
  `self._voice_edge_debouncer = voice_edge_debouncer.VoiceEdgeDebouncer(...)`
- `apps/windows/rc003/src/ovb_rc003/ble_transport_winrt.py:218` —
  `self._session = atvv_session.ATVVSession(gain_db=gain_db)`

Practical consequence: **setting `REMOTEMIC_NATIVE_CHOICE_*` env vars
today has no effect on the running `python -m ovb_rc003`** — the
production app continues to use the Python baseline regardless of
override. The factories are reachable only through the parity tests
(`tests/.../test_*_native_parity.py`) and the fake-backend switch test
(`tests/test_phase3_native_switch.py`).

Verify on your Windows desktop before doing any real-device work:

```powershell
python -c "import inspect, ovb_rc003.app, ovb_rc003.ble_transport_winrt as bt; \
print('app.py uses voice_controller.VoiceController:', inspect.getsource(ovb_rc003.app.RC003App.__init__).count('voice_controller.VoiceController')); \
print('ble_transport_winrt uses atvv_session.ATVVSession:', inspect.getsource(bt.RC003BleSession.__init__).count('atvv_session.ATVVSession'))"
```

Expected output today: both counts `>= 1`. If either is `0`, the
wiring has been updated and the rest of this document applies directly.

**If both counts are `>= 1` (today's state), do NOT proceed with the
real-device steps below.** The env-var procedure cannot reach native in
the real path. Either:

1. Route `app.py` / `ble_transport_winrt.py` through the factories
   (out of scope for this document — requires code change), OR
2. Accept that real acceptance for the native path is blocked behind a
   separate wiring step and report all three items (`RC003`,
   `Typeless`, `Qianwen`) as `deferred` with reason "production path
   not yet routed through Phase 3 factories".

The procedure below is written assuming variant (1) — once routing is
in place. Until then, real-acceptance status stays `deferred` per the
CHANGELOG.

---

## Phase 3 modules to flip to `native`

Set these three env vars before launching the app (see "How to launch"
below). All three Phase 3 state machines; no shadow anywhere in the real
path.

| Env var                                | Module          | Default today |
|----------------------------------------|-----------------|---------------|
| `REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER=native`        | `voice_controller`        | `python` |
| `REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER=native`    | `voice_edge_debouncer`    | `python` |
| `REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION=native`            | `atvv_session`            | `python` |

The default policy table (`_remotemic_native_runtime.py:49-63`) keeps
all three at `"python"`. The env vars are the only override mechanism
for the real path.

**Forbidden in real path:** `shadow`. The user contract explicitly bans
shadow dual-owner. `shadow` exists for parity tests only — see
`test_*_native_parity.py`.

## Modules that MUST stay `python`

Do NOT set any other `REMOTEMIC_NATIVE_CHOICE_*` env vars. The
following Phase 1 + 2 modules keep their `python` default:

- `atvv_protocol` — ATVV frame codec (compute)
- `atvv_control_parse` — control frame parser
- `atvv_control_encode` — control frame encoder
- `adpcm_ima_decode` — IMA ADPCM decoder
- `adpcm_dc_highpass` — DC removal filter
- `adpcm_postprocess` — final PCM post-process
- `adpcm_frame_accumulator` — ADPCM frame buffer

Reason: the C++ `AtvvSession` binding already wires the full PCM
pipeline (`FrameAccumulator → ImaDecoder → DcHighPassFilter →
postprocess`) internally per `atvv/session.cpp`. Flipping those six
Phase 2 modules to `native` independently would create a *second*
native pipeline on top of the one `AtvvSession` already owns — that is
the shadow dual-owner pattern explicitly forbidden in the real path.

## How to launch with the switch set

Open PowerShell, set the three env vars + `PYTHONPATH` in the same
shell where you launch the app, then start the normal launcher. The
venv's `python.exe` does not include `<repo>/apps/windows/rc003/src`
on `sys.path` by default; without `PYTHONPATH`, `python -m ovb_rc003`
fails with `No module named ovb_rc003`.

```powershell
$env:REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER = "native"
$env:REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER = "native"
$env:REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION = "native"
$env:PYTHONPATH = "C:\Users\hanboyd\hanboyd-code\remote_mic_han\apps\windows\rc003\src;C:\Users\hanboyd\hanboyd-code\remote_mic_han\build\Release"
cd C:\Users\hanboyd\hanboyd-code\remote_mic_han\apps\windows\rc003
.\.venv\Scripts\python.exe -m ovb_rc003
```

Replace `<repo>` with the actual repo root if it differs. The
`build\Release` entry is only needed when `_C.cp311-win_amd64.pyd` is
present (otherwise the native shims fall back to the Python baseline
automatically — see `voice_controller_native.py:54-63`).

Do NOT use `--dry-run` — it bypasses `RC003App.run_forever()` and only
probes the bindings; it does not exercise the voice session lifecycle.

## How to verify native is actually running

Single source of truth: `app.log` at
`%LOCALAPPDATA%\RemoteMic\RC003\logs\app.log`. The Python baseline
emits `startup: resolving RC003 identity`, `startup: exactly one RC003
candidate resolved`, and the legacy-key / physicalizer startup lines
(`app.py:215-352`). The native path emits identical log lines (the
state machine itself does not log a "native vs python" line because
the factories don't log — logging is the wrapper layer's
responsibility).

To verify which implementation is actually driving the voice state
machine, use the probe one-liner **before** launching the app, in the
same shell with the env vars set:

```powershell
python -c "import os; \
print('voice_controller ->', os.environ.get('REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER', 'python')); \
print('voice_edge_debouncer ->', os.environ.get('REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER', 'python')); \
print('atvv_session ->', os.environ.get('REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION', 'python')); \
import remotemic_native as rn; \
print('remotemic_native._C_AVAILABLE:', rn._C_AVAILABLE); \
from ovb_rc003.voice_controller_native import make_voice_controller; \
from ovb_rc003.voice_edge_debouncer_native import make_voice_edge_debouncer; \
from ovb_rc003.atvv_session_native import make_atvv_session; \
from ovb_rc003.voice_controller import VoiceTriggerMode; \
c = make_voice_controller(VoiceTriggerMode.HOLD); \
print('voice_controller type:', type(c).__module__ + '.' + type(c).__name__); \
d = make_voice_edge_debouncer(0.200); \
print('voice_edge_debouncer type:', type(d).__module__ + '.' + type(d).__name__); \
s = make_atvv_session(); \
print('atvv_session type:', type(s).__module__ + '.' + type(s).__name__)"
```

Expected (per `_remotemic_native_runtime.py:116-119` returning
`native_impl`):

- `voice_controller type: ovb_rc003.voice_controller_native._NativeVoiceController`
- `voice_edge_debouncer type: ovb_rc003.voice_edge_debouncer_native._NativeVoiceEdgeDebouncer`
- `atvv_session type: ovb_rc003.atvv_session_native._NativeATVVSession`
- `remotemic_native._C_AVAILABLE: True`

If any of the three `type` lines shows the `python` module path
(`ovb_rc003.voice_controller.VoiceController` etc.) instead of the
`_native.*` wrapper, the override did not take effect. Check that
the env var is set in the same shell as the probe and that the value
spelled exactly matches the table above (case-insensitive, but the
spelling matters).

Single-owner check (no shadow dual-owner in real path): the three
`_Native*` wrappers each hold exactly one `_impl` (a C++ object).
Inspect:

```powershell
python -c "from ovb_rc003.voice_controller_native import _NativeVoiceController; \
import remotemic_native as rn; \
print('_NativeVoiceController._impl is C++:', type(_NativeVoiceController.__init__.__globals__['rn'].VoiceController).__name__)"
```

Expected: prints a class name rooted in `remotemic_native._C` (the
pybind11 module). If it prints a pure-Python class name, the binding
did not load and the wrapper silently fell back to python (see
`voice_controller_native.py:54-59`); in that case
`remotemic_native._C_AVAILABLE` would also be `False`.

---

## RC003 manual steps

Pre-reqs: RC003 device paired and bound; Windows VB-Cable or default
endpoint ready; `app.log` tail running in a second terminal:

```powershell
Get-Content "$env:LOCALAPPDATA\RemoteMic\RC003\logs\app.log" -Wait
```

| # | Action | Observation |
|---|--------|-------------|
| 1 | Launch with the three env vars set (above) | `startup: resolving RC003 identity` then `startup: exactly one RC003 candidate resolved` in `app.log`. No import error from `remotemic_native._C`. |
| 2 | Connect RC003 (power on, hold to wake) | `startup: ... candidate resolved` already logged; BLE session reaches `connected` state. `app.log` shows no `on_session_error` lines. |
| 3 | Press the RC003 mic button (HOLD mode default) | Audio starts. `app.log` shows one `AudioStarted` / mic-open emission. Pressing again releases within the 200 ms release window: no spurious second host-edge. |
| 4 | Switch trigger mode to TOGGLE (via config: `voice_trigger_mode = "toggle"`, restart app) | Single press opens, single press closes. No double-trigger on the first edge. `app.log` shows alternating `AudioStarted` / `AudioStopped` events, never two of the same kind in a row without an intervening physical edge. |
| 5 | Release quickly (< 200 ms) | No spurious close. The 200 ms release debouncer (`VoiceEdgeDebouncer`) holds the release for `release_window_seconds = 0.200`; if a second press lands within the window, the release is cancelled. `app.log` shows the cancel by no `AudioStopped` event arriving within 200 ms of the press. |
| 6 | Press and hold for > 2.5 s, then force-kill the ATVV audio path (or unplug VB-Cable briefly to drop frames) | The `LATE_AUDIO_GUARD_SECONDS = 2500` fallback fires. `app.log` shows an `AudioStopped` event arriving ~2500 ms after `AudioStarted` even though no physical release happened. Host recognizer (Typeless / Qianwen) closes its session; no stuck mic. |
| 7 | Move RC003 out of range (or power off) | `on_disconnected` fires. `_voice.reset()` runs at cleanup. `app.log` shows `disconnected` and the supervisor enters reconnect backoff (default 2 s, max 60 s — `config.py`). Host recognizer session also closes (the cleanup path resets the voice state machine). |
| 8 | Bring RC003 back into range | Supervisor reconnects. `_voice` is reconstructed fresh (single-session owner contract: `make_voice_controller()` returns a fresh instance per factory call). `app.log` shows a new `startup: exactly one RC003 candidate resolved` and a working voice session. |
| 9 | Close the app (Ctrl+C in the launching shell) | `_cleanup_once` runs. `_voice_edge_debouncer.shutdown()` cancels any pending release timer. `_ble_session` closes. `_hid_listener` stops. `_legacy_key_suppressor` stops. `app.log` shows clean shutdown lines and no thread leaks (`CleanupIncompleteError` must NOT appear). If it appears, note the exception text and stop — do not auto-fix. |

Each step → record `passed` / `failed` in the table at the end.

## Typeless manual steps

Pre-reqs: Typeless installed and the voice hotkey bound. Set the
default Typeless toggle to the same VK chord the bridge physicalizes
(see `app.py:315-321` — `voice_hotkey_tokens = modifiers + key` from
`config.json["voice_hotkey"]`). Confirm `voice_physicalize_vk_codes`
is non-empty (the suppression of the LLKHF_INJECTED flag is what makes
Typeless react; without it, Typeless silently drops the edge per
`docs/decisions/ADR-0005-voice-hotkey-physicalize.md`).

The four invariants the user explicitly named:

| # | Action | Observation |
|---|--------|-------------|
| 1 | Press the RC003 mic button (HOLD mode) and release within 200 ms | Typeless opens its voice session **once** at the press edge, not twice. No double-trigger on the bounce. If you see Typeless open then close then open within 250 ms, the 200 ms release debounce or the physicalize flag is broken — record `failed` and stop. |
| 2 | Press, hold for 5 s, release | Typeless stays open for the full hold, closes once on release. No premature close during the hold. |
| 3 | Press and release rapidly (3 presses in 1 s) | Typeless toggles cleanly: open → close → open → close. The `VoiceEdgeDebouncer`'s cancel-on-press path must clear the pending release timer on every press edge. No stuck input — if Typeless ignores subsequent presses, record `failed`. |
| 4 | While Typeless is open, switch to another application (e.g. Notepad) and type, then switch back | Typeless session state preserved. No accidental close on focus change. The voice session state lives in the C++ `VoiceController`, not in the foreground app, so focus changes must not disturb it. |

## Qianwen manual steps (千问输入工具 / Windows 输入适配)

**Qianwen in scope:** the existing Qianwen input tool / Windows input
adapter present on the user's interactive Windows desktop (the same
class of host input tool as Typeless). **NOT** DashScope / Qwen ASR API.
No new Qianwen integration is being developed; this verifies that the
native Phase 3 state machine does not break the existing
physicalized-VK delivery path that Qianwen also consumes.

Pre-reqs: Qianwen input tool running. Same physicalize-VK contract as
Typeless — Qianwen must be configured to react to the same VK chord
that the bridge emits (`voice_hotkey_tokens` from
`config.json["voice_hotkey"]`).

| # | Action | Observation |
|---|--------|-------------|
| 1 | Press RC003 mic button (HOLD) → release within 200 ms | Qianwen opens its voice input **once** on the press edge, closes on the release edge. No double-trigger. |
| 2 | Press, hold for 5 s, release | Qianwen input stays open for the full hold. No premature close. The 2.5 s late-audio fallback (`LATE_AUDIO_GUARD_SECONDS`) must not close Qianwen's session mid-hold. |
| 3 | Three rapid press/release cycles in 1 s | Qianwen input toggles cleanly: open / close / open / close / open / close. No stuck input — if Qianwen ignores subsequent presses, the `VoiceEdgeDebouncer` cancel path is broken; record `failed` and stop. |
| 4 | While Qianwen is open, switch to a third app (browser, editor) and type, then return | Qianwen session state preserved across focus changes. No accidental close. State recovery after switching is a property of the C++ `VoiceController` not coupling to the foreground app. |

---

## Restore procedure (return to defaults)

When you are done — whether all steps passed, failed, or were aborted —
restore the production path to the default `python` baseline:

1. Unset the three env vars in the shell you used to launch:

   ```powershell
   Remove-Item Env:REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER
   Remove-Item Env:REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER
   Remove-Item Env:REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION
   ```

2. Close the running app (Ctrl+C). Confirm `app.log` shows clean
   shutdown with no `CleanupIncompleteError`.

3. Relaunch with no env vars set (keep `PYTHONPATH` from step 0 if
   you set it):

   ```powershell
   cd C:\Users\hanboyd\hanboyd-code\remote_mic_han\apps\windows\rc003
   .\.venv\Scripts\python.exe -m ovb_rc003
   ```

4. Confirm the probe one-liner from "How to verify native is actually
   running" reports the `python` module path for all three `type`
   lines:

   - `voice_controller type: ovb_rc003.voice_controller.VoiceController`
   - `voice_edge_debouncer type: ovb_rc003.voice_edge_debouncer.VoiceEdgeDebouncer`
   - `atvv_session type: ovb_rc003.atvv_session.ATVVSession`

5. Run a single smoke press / release on the RC003 to confirm the
   default Python path is still functioning. If anything regresses,
   the env vars are not the cause — they were unset. Report the
   regression per Rule 1 (fix → retest → decide per Rule 1/2); do
   not pre-bump to `0.4.1-candidate`.

---

## Recording template

After each real run, fill one row per item. `deferred` stays valid for
items not yet exercised; only mark `passed` after actual observation.

| Item | Result | Notes (date, observed behaviour, log excerpts) |
|------|--------|------------------------------------------------|
| RC003 — connect | | |
| RC003 — mic open | | |
| RC003 — HOLD / TOGGLE | | |
| RC003 — 200 ms release debounce | | |
| RC003 — AUDIO_STOP 2500 ms fallback | | |
| RC003 — disconnect | | |
| RC003 — reconnect | | |
| RC003 — app exit cleanup | | |
| Typeless — no double trigger | | |
| Typeless — no missed stop | | |
| Typeless — no stuck input | | |
| Typeless — state recovery after switching | | |
| Qianwen — no double trigger | | |
| Qianwen — no missed stop | | |
| Qianwen — no stuck input | | |
| Qianwen — state recovery after switching | | |

After each item: if `failed`, **stop the run, paste the
`app.log` excerpt and a 1-line symptom in the Notes column.** Do not
auto-fix. Report to me; the next step depends on what failed.

Once all three sections (RC003, Typeless, Qianwen) are filled in with
actual observations, paste the table back. I will update
`apps/windows/rc003/CHANGELOG.md` and
`docs/decisions/ADR-0013-...md` accordingly — not before.