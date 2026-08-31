# Current Status

- `last_updated`: 2026-08-31T11:55:00+08:00
- `updated_by`: minimax-m3 (claude code)
- `git_commit_sha`: b5c4d9b
- `current_phase`: Phase 4 step 3 complete (binding seam + G3 bind smoke green). Steps 1+2 landed earlier this session (efa6684, d443d03). Phase 3 native path stable (G7 verifier 19/19, ctest 31/31 Debug + Release).
- `hardware_available`: true

## Completed

- Phase 0 / Phase 1 / Phase 2 C++ migration (Python baseline frozen, `remotemic_native` binding live, ATVV 协议 / ADPCM 4 区 字节 / 样本级一致). See [CHANGELOG.md](../../apps/windows/rc003/CHANGELOG.md) `[0.3.0-candidate]` entry.
- Phase 3 implementation complete (ADR-0013 accepted):
  - `bf0818e` step 1: ADR-0013 + voice/session headers + TDD red-state unit tests
  - `207eb70` steps 2+3: real C++ state machines + pybind11 bindings (`remotemic::voice::VoiceController`, `remotemic::voice::VoiceEdgeDebouncer`, `remotemic::atvv::Session`)
  - `d7e3c5c` step 4: shadow parity tests + helper script (24/24 ctest Debug + 24/24 ctest Release)
  - `cd9148f` step 5: native switch + fake-backend verification
  - `11f58bd` step 6 closeout: version bumped `0.3.0-candidate → 0.4.0-candidate` (CMake / Python `__version__` / `pyproject.toml` in lockstep per [[cpp-migration-version-policy]] Rule 2; Inno Setup `AppVersion` deliberately left untouched per Rule 1 until phase 8)
  - `8cc0c4c` closeout: production path now routes through `make_voice_controller` / `make_voice_edge_debouncer` / `make_atvv_session`. `app.py:132` and `ble_transport_winrt.py:218` no longer reference the python classes directly. Default choice stays `python`; `REMOTEMIC_NATIVE_CHOICE_*=native` now actually reaches the real product path.
  - `cfebb9c` verify fix: `tools/verify_phase3_production_routing.py` puts `build/Release` ahead of `apps/windows/rc003/src` on `PYTHONPATH` so `_C.cp311-win_amd64.pyd` is found when present.
- G7 production-routing gate green end-to-end with `_C.pyd` exercised:
  - `python tools/verify_phase3_production_routing.py` (venv 3.11.15): **19/19 assertions PASS**, including the three `_is_native=True` checks (C++ side actually reached).
  - `python -m unittest discover -s apps/windows/rc003/tests -t apps/windows/rc003 -p test_phase3_production_routing.py -v`: **17/17 PASS**, including `test_voice_shim_is_native_when_cpp_binding_is_built`.
- Real-acceptance procedure documented: `docs/testing/PHASE3-REAL-ACCEPTANCE.md` covers RC003 + Typeless + Qianwen manual steps and the env-var switch pattern. Procedure is now reachable because production routing is closed.
- Stale handover closed: this document + `AI_HANDOVER.md` were last updated 2026-08-23 at commit `f3db758` (pre-Phase 3); both refreshed to commit `cfebb9c`.
- Phase 3 closeout regressions corrected in this session (see Phase 3 corrective + real-acceptance section below).

## Phase 3 corrective + real-acceptance (2026-08-31, this session)

Three regressions introduced by the Phase 3 closeout commit chain (`bf0818e → 8cc0c4c → cfebb9c`) were blocking the source bridge from running under `REMOTEMIC_NATIVE_CHOICE_*=native`. All three are now fixed via corrective restore from `19a0004` baseline:

1. **`bridge_control_windows.py` missing from working tree** — file existed in `19a0004` (149 lines, Windows kernel32 `CreateEventW/OpenEventW/SetEvent/WaitForSingleObject` stop-event primitive) but was lost from the working tree without a git delete event (`git log --diff-filter=D` is empty for that path). `__main__.py:213/:230` still imported it. **Fix**: restored byte-for-byte from `19a0004` (SHA-256 verified identical modulo CRLF).
2. **`app.main(stop_signal)` signature dropped in `app.py`** — `app.py:1442` was `def main() -> None:` but `__main__.py:232` still called `app.main(stop_signal)`, throwing `TypeError`. **Fix**: restored `app.main(stop_signal=None)` + `async _run(stop_signal=None)` (the full body including `_wait_for_stop` + `asyncio.wait(run_task, stop_task, return_when=FIRST_COMPLETED)` machinery) byte-for-byte from `19a0004`.
3. **`_NativeVoiceController.trigger_mode` missing** — Phase 3 closeout wired production through `make_voice_controller()` but the resulting `_NativeVoiceController` wrapper did not expose `trigger_mode`, even though the python baseline `voice_controller.py:46` does. `app.py:583/1163/1176/1193` read `self._voice.trigger_mode` and threw `AttributeError` on the native path. **Fix**: added `self.trigger_mode = trigger_mode` in `_NativeVoiceController.__init__` for both the native branch and the python fallback branch.

### Gates after corrective fixes

- `python tools/verify_phase3_production_routing.py` (venv 3.11.15, with `_C.pyd`): **19/19 PASS** (was 19/19 already before; regressions don't touch the gating surface).
- `python -m unittest discover -s apps/windows/rc003/tests -t apps/windows/rc003 -p test_phase3_production_routing.py`: **17/17 PASS**.
- `--dry-run` smoke: `dry-run: all ovb_rc003 modules imported successfully` + native probe lines.

### Real-acceptance observations (2026-08-31, on hardware)

Bridge launched via `apps\windows\rc003\.venv\Scripts\python.exe -m ovb_rc003 --bridge` with the three `REMOTEMIC_NATIVE_CHOICE_*=native` env vars per `docs/testing/PHASE3-REAL-ACCEPTANCE.md`.

| Step | Result | Evidence (`%LOCALAPPDATA%\RemoteMic\RC003\logs\app.log`) |
|---|---|---|
| 1. Short-press RC003 mic (<200ms) | **PASS** | `voice physical mic trigger received before audio start` × 1, `voice playback opened: host_api=Windows WASAPI sample_rate=48000 channels=2`, PCM frames 1→800 peak=5129 rms=85.2; two subsequent `voice legacy F5 trigger` lines during hold correctly answered with `voice physical trigger ignored: voice session already active` (debouncer working); physical release wrote `voice physical mic released; closing held host shortcut`. |
| 2. Long-press ~27s | **PASS** | `trigger received before audio start`, PCM frames 1000→1600 peak=32768 rms=503-558 (real speech signal), mid-hold F5 repeat at 04:58:29 ignored, physical release at 04:58:35 cleanly closed host tap. `LATE_AUDIO_GUARD_SECONDS` fallback NOT triggered because ATVV audio path stayed healthy throughout. |
| 6. Long-press + force-kill ATVV audio path | **not_reproducible_in_healthy_setup** | VB-Cable is a software virtual audio driver (not a USB device), so the procedure's "unplug VB-Cable" workaround cannot be executed. The `LATE_AUDIO_GUARD_SECONDS=2500` fallback fires only on ATVV BLE notification absence, not on Windows audio sink state — disabling the CABLE Input endpoint does not trigger it. A genuine RC003 firmware fault is required to exercise this path on real hardware. Guard logic is covered by Phase 3 shadow parity unit tests (24/24 ctest Debug + 24/24 Release per `[0.4.0-candidate]` CHANGELOG entry). |
| 7a. `request_bridge_stop()` from external process | **PASS** | `outcome=requested` returned by external `request_bridge_stop()`. Bridge wrote `bridge stop requested by settings; cleaning up` then `cleanup: attempted release of hotkey state and BLE/HID/audio`. Process exited 0. No `CleanupIncompleteError`, no `Traceback`. |
| 7b. Ctrl+C / KeyboardInterrupt | **deferred** | Claude Code background-task harness does not share a Windows console with the spawned bridge, so `CTRL_BREAK_EVENT` / `CTRL_C_EVENT` from Python's `subprocess.Popen` does not reach Python's `SetConsoleCtrlHandler` (we tried both `CREATE_NEW_PROCESS_GROUP` and `CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE`). Functionally equivalent to 7a (same `app.stop()` path, same `finally: await app.stop()` cleanup); 7a's evidence covers cleanup correctness. |
| Typeless | **PARTIAL — step 4 only** | Bridge launched with the three `REMOTEMIC_NATIVE_CHOICE_*=native` env vars per the updated `docs/testing/PHASE3-REAL-ACCEPTANCE.md`. User exercised **only step 4** (focus-switch + Notepad input): RC003 voice key triggers Typeless, switching to Notepad and typing does not disturb Typeless session state. Step 4 user-reported: "跑通了，使用rc语音键，可以调起". Steps 1 (short-press, no double-trigger), 2 (long-press ~5s hold), and 3 (3× rapid press/release cycles) NOT exercised in this session. No `Traceback`, no `CleanupIncompleteError`. The chord delivery + state-preservation paths are end-to-end clean for the cases observed. |
| Qianwen | **deferred (structural)** | Bridge has only one `voice_hotkey` at a time; Typeless (`lctrl+lalt`) and Qianwen (`ralt`) target mutually exclusive code paths (`_apply_voice_action` vs `_emit_legacy_voice_key`). Even with `voice_trigger_mode=hold` + `voice_hotkey=ralt` config, the bridge's RAlt events are dropped by the user's installed `QianwenIMEUiClient.exe` (2026-08-28 build, 9.07 MB at `C:\Program Files\qianwenime\`) because its elevated `WH_KEYBOARD_LL` callback verifies `LLKHF_INJECTED` + extra-info marker. `qianwen_physicalizer.py:28` is version-locked to SHA-256 `2ef313df4fce58b067a0b4751e47c1ce547dd25b35891efdc55ba397c6ae1b56`; user's installed EXE is `a6ab353a54f3cee288cefa421c4753e20058c6833eca2961426c6c52f9882af5` — mismatch fails the adapter closed at `qianwen_physicalizer.py:191-193`. Per "不自动修" + "don't write Frida adapters for unverified builds" rule, this is a structural gap, not a regression. New adapter requires (1) re-discovering the RVA of the KBDLLHOOKSTRUCT callback in the new build, (2) re-locking the SHA-256, (3) Frida-session verification matrix — out of Phase 3 scope. |

### Audit + orphan-source restoration (this session, continuation)

A systematic regression audit (file inventory diff + signature/attribute compatibility + `__main__.py` import surface, all `19a0004 → HEAD`) found no further Phase 3 closeout regressions beyond the three already fixed above. **Audit 1 (file inventory diff)**: HEAD has 5 intentional Phase 3 native shims added (`_remotemic_native_runtime.py`, `atvv_native_bridge.py`, `atvv_session_native.py`, `voice_controller_native.py`, `voice_edge_debouncer_native.py`) and 2 orphan source files missing (`qianwen_physicalizer.py`, `rc003_battery_windows.py`). **Audit 2 (signature/attribute compatibility)**: `python -m ovb_rc003 --dry-run` (venv 3.11.15) passes end-to-end; `__main__.py` diff vs19a0004 is +74 lines of intentional Phase 1 native probe scaffold only. **Audit 3 (`__main__` import surface)**: every name referenced from `_run_bridge` / `_run_settings` / `main()` resolves at the receiving module (the three corrective fixes restored `app.main(stop_signal)`, `_NativeVoiceController.trigger_mode`, and `bridge_control_windows.BridgeStopSignal`).

The two orphan files are NOT Phase 3 regressions — they were lost from the working tree between `19a0004` and `bf0818e` (the first Phase 3 commit; no git delete event). Per user direction ("先拿出重构之后可用的，一点点改bug"), restored byte-for-byte from `19a0004` baseline to match the `bridge_control_windows.py` corrective-restore precedent:

| File | Lines (19a0004) | 19a0004 blob | Restored blob | Status |
|---|---|---|---|---|
| `apps/windows/rc003/src/ovb_rc003/qianwen_physicalizer.py` | 254 | `eadc0ed08c2b212aea2e1315a3d5444143d143ac` | `eadc0ed08c2b212aea2e1315a3d5444143d143ac` | byte-identical |
| `apps/windows/rc003/src/ovb_rc003/rc003_battery_windows.py` | 201 | `a6656f6fda3183db9f9932bf9a895c3af321412f` | `a6656f6fda3183db9f9932bf9a895c3af321412f` | byte-identical |

Post-restore verification:
- `python -m ovb_rc003 --dry-run` (venv 3.11.15): **PASS** — both modules importable, no regression on the existing 27-module import surface
- Direct `from ovb_rc003 import qianwen_physicalizer, rc003_battery_windows`: **PASS** — public attributes match 19a0004 (`QianwenPhysicalizer`, `start_physicalizer`, `physicalizer_status`, `physicalizer_error`; `BatteryProbeUnavailableError`, `DEVPROPKEY`, `SP_DEVINFO_DATA`)
- `qianwen_physicalizer.py` depends only on stdlib (`hashlib`, `logging`, `threading`, etc.)
- `rc003_battery_windows.py` depends on `from . import device_profile`; `device_profile.py` is still present in HEAD

Both files remain unimported by any other code in HEAD (no caller needs them yet). The Qianwen third-party integration path is still in the deferred list; this restore simply returns the source module to the working tree so future Qianwen work has its existing implementation available. The battery module was never wired into the bridge UI; it remains available as a future-feature scaffold.

## In progress

- Nothing — Phase 3 native path is "usable after the refactor". Next iteration cycles per the user's "快速完成重构 + 软件健壮" balance: pick up one bug at a time from the deferred list as it surfaces, do not try to perfect everything in one session.

## Deferred

- Back, volume-up, volume-down: do not reach Raw Input or the low-level keyboard hook on this Windows host. Elevated WUDFHost injection and direct HID-over-GATT characteristic access are both denied by Windows. Not a regression; product must surface the gap explicitly rather than ship a SYSTEM workaround.
- Step 6 late-audio guard: not reproducible in healthy RC003 + software VB-Cable; covered by unit-test parity only.
- Step 7b KeyboardInterrupt: cannot reproduce cleanly from Claude Code background-task harness; functionally equivalent to 7a.
- Typeless third-party input tool: **PARTIAL — step 4 only verified this session** (focus-switch + Notepad input preserves Typeless session; chord delivery via `lctrl+lalt` confirmed working). Steps 1 (no double-trigger on short press), 2 (5s HOLD mode), and 3 (3× rapid toggle) remain unverified. The bridge path is exercised; the per-step Typeless app response matrix needs follow-up if any of those edge cases surfaces a regression.
- Qianwen third-party input tool: **deferred (structural, not Phase 3 in scope)** — bridge's RAlt path is correct, but the user's installed `QianwenIMEUiClient.exe` SHA-256 (`a6ab353a54f3cee288cefa421c4753e20058C6833eca2961426c6c52f9882af5`, 2026-08-28 build) does not match `qianwen_physicalizer.py:28`'s locked SHA-256 (`2ef313df4fce58b067a0b4751e47c1ce547dd25b35891efdc55ba397c6ae1b56`). The Frida adapter that bypasses the elevated KBDLLHOOKSTRUCT LLKHF_INJECTED check fails closed at `qianwen_physicalizer.py:191-193`. Unblocking requires re-discovering the new build's callback RVA + re-locking the SHA + Frida-session verification matrix — out of Phase 3 scope, would need its own ADR.
- Installer uninstall/residue validation: pending explicit user authorization to remove the installed candidate.
- Release signing: pending both acceptance completion and an available signing identity.
- Phase 4 (audio WASAPI), Phase 5 (Windows input), Phase 6 (BLE / WinRT), Phase 7 (app coordinator), Phase 8 (packaging switch), Phase 9 (UI decision): not started; Phase 3 must complete remaining real acceptance first per `docs/architecture/cpp-migration-execution-plan.md` §3 rule 9, OR the user may enter Phase 4 with the existing deferred items carried forward (executive call).

## Phase 4 (audio WASAPI) — in progress (2026-08-31)

Per user direction 2026-08-31, Phase 4 entry is an executive call: the existing Phase 3 deferred items (Typeless step 1/2/3, Step 6 late-audio, Step 7b KeyboardInterrupt, Qianwen structural) are carried forward as out-of-scope for Phase 4. Per ADR-0014 §10, real-acceptance gate (G6) is Typeless + RC003 only; Qianwen Frida adapter work is explicitly NOT in scope.

- `efa6684` step 1: ADR-0014 (proposed) + 5 module headers + 5 stub .cpp + 5 red-state unit tests. `IAudioRoute` extended with `drain(timeout)` + `close()` per ADR §4; original `stop()` is now strictly "tell writer to exit"; the previous overloaded `stop()` that did both is split intentionally.
- `d443d03` step 2: 5 stub .cpp replaced with real implementations (mutex-protected drop-oldest BoundedPcmQueue, 20 ms PcmChunker with silence-padded flush, 3-tap linear Upsample16kTo48k byte-aligned with `audio_playback.py:154-172`, recording FakeAudioRoute with atomic counters, Windows-only WASAPI backend with COM init + 48 kHz fallback + atomic stop flag + jthread writer pulling via PcmChunker+Upsample). 30/30 ctest green (Debug + Release).
- `b5c4d9b` step 3 (this session): pybind11 binding seam + Python shim + G3 bind smoke. `bind_module.cpp` exposes `PcmFormat` (POD), `IAudioRoute` (trampoline base), `WasapiAudioRoute`, `FakeAudioRoute`. `audio_route_native.py` provides `make_audio_route(endpoint_name, host_api_name)` dispatching via `choose_implementation`. Default = python (matches migration plan §1 rule 4); native opt-in via `REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native`. `shadow` is NOT exposed per plan §3 rule 5 (real WASAPI device handle is side-effecting). `FakeAudioRoute.write_calls_` now increments before the started-guard so the counter reflects every invocation including rejected ones; operators can compute `1 - dropped_/write_calls_` success rate. CMakeLists links `remotemic_audio` into `remotemic_native_c` and registers ctest target `remotemic_audio_route_bind_smoke` (G3).

### Gates after Phase 4 step 3 (this session)

- `ctest -C Debug remotemic_audio_route_bind_smoke`: **PASS** (10/10 assertions: PcmFormat round-trip × 3, FakeAudioRoute isinstance + lifecycle counters + last_format + stop idempotent, WasapiAudioRoute isinstance + default endpoint + drop/write_error counters, drain int-timeout).
- Full Debug ctest: **31/31 PASS** (was 30/30 at `d443d03`; +1 G3 test).
- Full Release ctest: **31/31 PASS**.
- `tools/verify_phase3_production_routing.py` (G7 verifier): **19/19 PASS**; new `audio_route` choice entry does not regress existing dispatch paths (voice / edge_debouncer / atvv_session).
- `python -m ovb_rc003 --dry-run`: **PASS** (all ovb_rc003 modules including `audio_route_native` import successfully).

### Phase 4 step 3 deferred / not yet wired

- `_NativeAudioRoute.open()` requires a real WASAPI endpoint (`CABLE Output` etc.); CI / headless shells fail closed at `_resolve_device_index`. Step 4's `FakeAudioRoute` shadow parity harness is the build-time proof point. Real-device validation (G6) happens at step 5 or step 6.
- WASAPI's single-owner rule (plan §3 rule 5) is honored: the C++ side owns the device handle; Python only drives the lifecycle (`start/write/drain/stop/close`). No `numpy` or Python-side audio buffer slicing crosses the binding seam yet.

## Next

1. On any new session: `git log --oneline -5` to find the latest Phase 4 commit SHA, then read this file + `AI_HANDOVER.md` before doing anything.
2. Phase 4 has 3 steps remaining: step 4 (shadow parity harness with FakeAudioRoute), step 5 (native switch + verify), step 6 (closeout: ADR-0014 accepted + version bump `0.4.0-candidate → 0.5.0-candidate`). Pick one per cycle, validate, commit, update CURRENT_STATUS.
3. Do not start Phase 5 (Windows input) until Phase 4 reaches step 6 closeout.
4. Carry-forward from Phase 3: Typeless steps 1/2/3, Step 6 late-audio, Step 7b KeyboardInterrupt, Qianwen SHA mismatch — all unchanged. Phase 4 real-acceptance (G6) is Typeless + RC003 only per ADR-0014 §10.
5. Uninstall/residue check + signing only with explicit user authorization.
