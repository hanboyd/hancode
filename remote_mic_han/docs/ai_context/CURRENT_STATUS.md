# Current Status

- `last_updated`: 2026-08-31T05:15:00+08:00
- `updated_by`: minimax-m3 (claude code)
- `git_commit_sha`: b802a33
- `current_phase`: Phase 3 implementation complete; three Phase 3 closeout regressions corrected in this session; native path real-device acceptance partially observed (3 PASS, 2 deferred, 1 not-reproducible)
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
| Typeless | **deferred** | Bridge + RC003 native path verified end-to-end; the Typeless third-party input tool is out of Phase 3 scope. |
| Qianwen | **deferred** | Same as Typeless. |

## In progress

- Nothing — Phase 3 native path is "usable after the refactor". Next iteration cycles per the user's "快速完成重构 + 软件健壮" balance: pick up one bug at a time from the deferred list as it surfaces, do not try to perfect everything in one session.

## Deferred

- Back, volume-up, volume-down: do not reach Raw Input or the low-level keyboard hook on this Windows host. Elevated WUDFHost injection and direct HID-over-GATT characteristic access are both denied by Windows. Not a regression; product must surface the gap explicitly rather than ship a SYSTEM workaround.
- Step 6 late-audio guard: not reproducible in healthy RC003 + software VB-Cable; covered by unit-test parity only.
- Step 7b KeyboardInterrupt: cannot reproduce cleanly from Claude Code background-task harness; functionally equivalent to 7a.
- Typeless third-party input tool: deferred — bridge's physicalized-VK chord delivery is verified via `voice physical mic trigger` / `closing held host shortcut` lines; the Typeless app's reaction is out of Phase 3 scope.
- Qianwen third-party input tool: same as Typeless.
- Installer uninstall/residue validation: pending explicit user authorization to remove the installed candidate.
- Release signing: pending both acceptance completion and an available signing identity.
- Phase 4 (audio WASAPI), Phase 5 (Windows input), Phase 6 (BLE / WinRT), Phase 7 (app coordinator), Phase 8 (packaging switch), Phase 9 (UI decision): not started; Phase 3 must complete remaining real acceptance first per `docs/architecture/cpp-migration-execution-plan.md` §3 rule 9, OR the user may enter Phase 4 with the existing deferred items carried forward (executive call).

## Next

1. On any new session: `git log --oneline -5` to find the Phase 3 corrective commit SHA (this session's landing), then read this file + `AI_HANDOVER.md` before doing anything.
2. Per the user's "一点点改 bug" cadence: pick one deferred item per session cycle, fix it, validate, commit, update CURRENT_STATUS. Don't try to clear the whole list in one pass.
3. Do not re-run the full `PHASE3-REAL-ACCEPTANCE.md` table from scratch unless the user explicitly requests it; the Step 1/2/7a PASS results are durable observations that only need re-validation if a Phase 3 source file is changed.
4. Uninstall/residue check + signing only with explicit user authorization.
5. Phase 4 entry requires a fresh ADR per [[cpp-migration-version-policy]] Rule 1/2; do not start without one.
