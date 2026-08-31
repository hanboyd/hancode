# AI Handover

```yaml
last_updated: 2026-08-31T15:50:00+08:00
agent: minimax-m3 handing off to next agent
provider: minimax handing off to next
model: minimax-m3 handing off to next
git_commit_sha: 80126cd
current_phase: Phase 4 step 5 complete (G5 native switch + production routing gates green at 80126cd). 35/35 ctest Debug + 35/35 ctest Release; G7 verifier 19/19; step 5 helper 4/4. Phase 4 has 1 step remaining: step 6 closeout (flip ADR-0014 to Accepted + version bump 0.4.0-candidate -> 0.5.0-candidate + record G6 observations from PHASE4-REAL-ACCEPTANCE.md into CHANGELOG once a human runs the procedure).
current_task: Phase 4 step 6 closeout next — flip ADR-0014 to Accepted + version bump 0.4.0-candidate → 0.5.0-candidate (CMake / Python __version__ / pyproject.toml in lockstep per cpp-migration-version-policy.md Rule 2; Inno Setup AppVersion deliberately NOT bumped per Rule 1 until phase 8) + record G6 observations from PHASE4-REAL-ACCEPTANCE.md into CHANGELOG. Per the user's "快速完成重构 + 软件健壮" balance, ship step 6 in one focused pass, but only AFTER G6 is exercised on real hardware — the closeout's CHANGELOG row needs the observation table.
deadline: none hard; current sprint has shipped a working snapshot
hardware_validation:
  status: partial — Phase 3 native path verified end-to-end on real RC003 for short-press / long-press / graceful-stop; late-audio guard, KeyboardInterrupt path, Typeless/Qianwen integration not exercised
  details: see CURRENT_STATUS.md "Phase 3 corrective + real-acceptance (2026-08-31)" section for the full observation table with app.log excerpts
completed:
  - Phase 0/1/2 C++ migration landed and frozen at 0.3.0-candidate (provenance verified, 4 areas × 6 gates passed, byte/sample parity with Python baseline)
  - Phase 3 step 1: ADR-0013 + voice/session headers + TDD red-state unit tests (bf0818e)
  - Phase 3 steps 2+3: C++ state machines + pybind11 bindings — VoiceController, VoiceEdgeDebouncer, Atvv::Session (207eb70)
  - Phase 3 step 4: shadow parity tests + helper script, 24/24 ctest Debug + 24/24 ctest Release (d7e3c5c)
  - Phase 3 step 5: native switch + fake-backend verification (cd9148f)
  - Phase 3 step 6 closeout: ADR-0013 accepted, version bumped to 0.4.0-candidate in CMake / Python __version__ / pyproject.toml (11f58bd)
  - Production routing closed at 8cc0c4c: app.py:132 + ble_transport_winrt.py:218 now use make_* factories; REMOTEMIC_NATIVE_CHOICE_*=native actually reaches the real product path
  - G7 gate registered: ctest target remotemic_phase3_production_routing + tools/verify_phase3_production_routing.py + 17 in-tree unit tests (8cc0c4c)
  - G7 verify PYTHONPATH fix so _C.cp311-win_amd64.pyd is found ahead of source-tree stubs (cfebb9c)
  - PHASE3-REAL-ACCEPTANCE.md procedure written for RC003 + Typeless + Qianwen manual steps
  - Phase 4 step 1 (efa6684): ADR-0014 (proposed) + IAudioRoute/drain/close + 5 audio headers + 5 stub .cpp + 5 red-state tests
  - Phase 4 step 2 (d443d03): 5 stub .cpp replaced with real C++ impls (BoundedPcmQueue mutex drop-oldest, PcmChunker 20 ms silence-padded flush, Upsample16kTo48k 3-tap byte-aligned with audio_playback.py:154-172, FakeAudioRoute recording double with atomic counters, WasapiAudioRoute Windows-only COM init + 48 kHz fallback + jthread writer)
  - Phase 4 step 3 (b5c4d9b, this session): pybind11 binding seam — bind_module.cpp exposes PcmFormat/IAudioRoute/WasapiAudioRoute/FakeAudioRoute; audio_route_native.py module-level switch (make_audio_route dispatching via choose_implementation; default python, native opt-in); G3 bind smoke (10/10 PASS); FakeAudioRoute.write_calls_ now increments before started-guard so dropped/total ratio is accurate. CMakeLists links remotemic_audio into remotemic_native_c and registers ctest target remotemic_audio_route_bind_smoke. G7 verifier still 19/19; --dry-run still passes; full ctest 31/31 Debug + 31/31 Release.
  - Phase 4 step 4 (0545cfd, this session): G3 byte-exact parity harness — bind_module.cpp exposes upsample_16k_to_48k + UpsampleState; FakeAudioRoute binding gains recorded_samples_list/peak/rms introspection (C++ impl: recorded_snapshot + peak_abs + rms_value with int64-accumulated squares, no int32 overflow on int16 inputs). apps/windows/rc003/tests/fakes/audio_route_fakes.py NEW: FakePlaybackSink pure-python recording double mirroring FakeAudioRoute 1:1 (start/write/drain/stop/close + 5 lifecycle counters + recorded_samples + recorded_samples_list + peak + rms as @property). tests/bind/test_upsample_16k_to_48k_parity.py NEW: 8 tests byte-exact against audio_playback.py:154-172 python baseline. apps/windows/rc003/tests/test_audio_route_native_parity.py NEW: 10 scenarios drive identical scripts through both recording doubles, asserting byte-exact recorded_samples_list + 5 lifecycle counters parity + peak parity + RMS parity (6 dp) + 2 sanity tests. tools/verify_phase4_audio_parity.py NEW: helper script mirroring verify_phase3_production_routing.py pattern. tools/run_parity_test.py REWRITTEN: previous wrapper duplicated -m unittest in argv (CMake already passes it); ctest saw exit 2 as Passed on Phase 3 parity tests, masking the bug. New wrapper parses args in-process, strips leading -m unittest, calls unittest.main programmatically with TestLoader.discover when discover or any of -s/-t/-p flags appear, updates sys.path directly. Fixes Phase 3 parity AND Phase 4 parity simultaneously. CMakeLists: 2 new ctest targets (remotemic_upsample_parity, remotemic_audio_route_parity); moved _REMOTEMIC_PARITY_HELPER/_REMOTEMIC_PARITY_ENV definitions above all add_test calls (previous ordering left them undefined when Phase 4 step 4 add_test fired). 33/33 ctest Debug + 33/33 ctest Release; helper script 2/2 PASS; G7 verifier still 19/19; --dry-run still passes.
  - Phase 4 step 5 (80126cd, this session): native switch + production routing — apps/windows/rc003/src/ovb_rc003/audio_route_native.py refactored from per-call dispatch wrapper to module-level at-import-time binding (make_audio_route = choose_implementation(...)) matching the Phase 3 / ADR-0011 single-import-surface pattern; shadow rejected at runtime per plan §3 rule 5 (real WASAPI device handle). apps/windows/rc003/src/ovb_rc003/app.py — self._playback now constructed via the factory; type annotation removed (commented inline) because under native it holds a _NativeAudioRoute shim, not the python baseline. apps/windows/rc003/src/remotemic_native/__init__.py — PcmFormat + WasapiAudioRoute added to public re-exports (ADR-0011 single-import-surface). apps/windows/rc003/tests/test_phase4_audio_route_native_switch.py NEW: 9 tests across DefaultDispatch / NativeDispatch / RestoreAfterUnset / SingleOwner; mirrors test_phase3_native_switch.py shape; env-leak safety via snapshot+restore _EnvCase (5ce9bd5 pattern). apps/windows/rc003/tests/test_phase4_audio_route_production_routing.py NEW: 4 source-level tests asserting app.py references make_audio_route(...) and NOT the python class directly. tools/verify_phase4_native_switch.py NEW: 4-condition acceptance proof mirroring verify_phase3_production_routing.py. docs/testing/PHASE4-REAL-ACCEPTANCE.md NEW: G6 manual procedure for real-device validation (RC003 + VB-Cable + Typeless / Qianwen); every audio lifecycle event mapped to a log-line + a Typeless / Qianwen behavior check; restore-to-default procedure + recording template included. CMakeLists: 2 new ctest targets (remotemic_phase4_native_switch + remotemic_phase4_production_routing). 35/35 ctest Debug + 35/35 ctest Release (was 33/33 at 0545cfd; +2); step 5 helper 4/4 PASS; step 4 parity still 2/2 PASS; G7 verifier still 19/19; --dry-run still passes.
  - CHANGELOG [0.4.0-candidate] entry published with G7 row
  - Phase 3 corrective (this session, about to commit):
      * bridge_control_windows.py restored byte-for-byte from 19a0004 (was lost from working tree, no git delete event)
      * app.main(stop_signal=None) and async _run(stop_signal=None) restored byte-for-byte from 19a0004 (Phase 3 closeout dropped the parameter without updating __main__.py:232)
      * _NativeVoiceController.__init__ now sets self.trigger_mode = trigger_mode in both branches (mirrors voice_controller.py:46 python baseline surface)
  - Real-device acceptance observations (this session):
      * Step 1 short-press PASS — 1 trigger + audio open + 2 ignored F5 repeats + clean release
      * Step 2 long-press ~27s PASS — HOLD mode held without false close, mid-hold F5 repeat ignored
      * Step 7a request_bridge_stop() PASS — named-event cleanup path, exit 0
  - CURRENT_STATUS.md + this handover refreshed with corrective context + observation table
  - Phase 3 corrective audit (this session, continuation at fde9a1e):
      * file-inventory diff 19a0004 -> HEAD found no further Phase 3 closeout regressions beyond the three above
      * signature/attribute audit + `__main__.py` import surface audit both clean (--dry-run smoke passes; __main__.py diff vs19a0004 is +74 lines of intentional Phase 1 native probe scaffold only)
      * two orphan source modules lost from working tree between 19a0004 and bf0818e (no git delete event): `qianwen_physicalizer.py` (254 lines) + `rc003_battery_windows.py` (201 lines) — restored byte-for-byte from 19a0004 to match the bridge_control_windows.py corrective-restore precedent; SHA verified identical; --dry-run + direct import both PASS
  - Third-party validation this session (continued at b063ca2):
      * Typeless — step 4 only verified (RC003 voice key opens Typeless; Notepad focus-switch + typing preserves Typeless session state). User-reported: "跑通了，使用rc语音键，可以调起". Steps 1 (no double-trigger on short press), 2 (5s HOLD mode), 3 (3× rapid toggle) NOT exercised in this session.
      * Qianwen — deferred (structural). User's installed `QianwenIMEUiClient.exe` SHA-256 (`a6ab353a54f3cee288cefa421c4753e20058c6833eca2961426c6c52f9882af5`, 2026-08-28 build) does not match `qianwen_physicalizer.py:28`'s locked SHA-256 (`2ef313df4fce58b067a0b4751e47c1ce547dd25b35891efdc55ba397c6ae1b56`). The Frida adapter that bypasses the elevated KBDLLHOOKSTRUCT LLKHF_INJECTED check fails closed at `qianwen_physicalizer.py:191-193`. Unblocking requires a new Frida adapter + verification matrix — out of Phase 3 scope, would need its own ADR.
  - PHASE3-REAL-ACCEPTANCE.md launch command fix (b063ca2):
      * added `$env:PYTHONPATH = "<repo>/apps/windows/rc003/src;<repo>/build/Release"` to the launch-with-switch + restore-to-default sections. Without PYTHONPATH, the venv's python.exe cannot find `ovb_rc003` and the bridge fails with `No module named ovb_rc003`. Discovered while running the Typeless validation; doc-only fix.
tests_run:
  - command: python tools/verify_phase3_production_routing.py (under apps/windows/rc003/.venv python 3.11.15)
    result: passed (19/19 assertions, including three _is_native=True C++-side checks; no NOTE/skipped rows)
  - command: python -m unittest discover -s apps/windows/rc003/tests -t apps/windows/rc003 -p test_phase3_production_routing.py -v
    result: passed (17/17, including test_voice_shim_is_native_when_cpp_binding_is_built)
  - command: python -m ovb_rc003 --dry-run
    result: passed (dry-run: all ovb_rc003 modules imported successfully + native probe lines)
  - command: python -c "from ovb_rc003.voice_controller_native import _NativeVoiceController; from ovb_rc003.voice_controller import VoiceTriggerMode; c=_NativeVoiceController(VoiceTriggerMode.HOLD); print(c.trigger_mode, c._is_native)"
    result: passed (HOLD True)
  - command: live bridge with REMOTEMIC_NATIVE_CHOICE_*=native — Step 1 / Step 2 / Step 7a real-hardware observations
    result: passed per CURRENT_STATUS.md observation table
known_problems:
  - Back / volume-up / volume-down do not reach Raw Input or the low-level keyboard hook; elevated WUDFHost injection and direct HID-over-GATT characteristic access are denied by Windows; surface this as a gap rather than ship a SYSTEM workaround
  - Step 6 late-audio guard cannot be triggered in healthy RC003 + software VB-Cable (the procedure's "unplug VB-Cable" workaround was wrong about the mechanism — VB-Cable is the audio sink, not the ATVV notification source). Genuine RC003 firmware fault would be needed; guard logic is unit-test covered
  - Step 7b KeyboardInterrupt path cannot be cleanly triggered from Claude Code background-task harness (Windows console signal semantics do not reach Python's SetConsoleCtrlHandler). 7a's evidence covers the same `app.stop()` cleanup
  - Typeless third-party input tool has not been independently validated on native path; bridge's physicalized-VK chord delivery IS verified in Step 1/2 logs
  - Qianwen third-party input tool has not been independently validated
  - Installer uninstall/residue check + release signing need explicit user authorization
do_not_change:
  - Do not produce PyInstaller / Inno / portable ZIP artifacts before Phase 8 (cpp-migration-version-policy.md Rule 1)
  - Do not bump voice_release_debounce_seconds default or its [0.050, 0.500] clamp band without a fresh ADR; the value is pinned at three layers on purpose
  - Do not flip any REMOTEMIC_NATIVE_CHOICE_* env var to native for normal users; default stays python
  - Do not auto-fix a failed real-acceptance row; stop, paste app.log excerpt + 1-line symptom
  - Do not start a C++ rewrite of working product functionality outside the phase plan
  - The three Phase 3 corrective fixes in this session are deliberately byte-for-byte restores from 19a0004; do not "improve" them while iterating on separate bugs
next:
  - On any new session: git log --oneline -5 to find the latest Phase 4 step 5 commit (80126cd), then read CURRENT_STATUS.md and this handover before doing anything
  - Phase 4 has 1 step remaining: step 6 closeout. It has three sub-steps:
      1. G6 execution — human operator runs `docs/testing/PHASE4-REAL-ACCEPTANCE.md` against RC003 + VB-Cable + Typeless (or Qianwen), fills the recording template, returns the table.
      2. ADR-0014 status flip — set to `accepted` once G6 has actual observations (not before).
      3. version bump — 0.4.0-candidate → 0.5.0-candidate in CMake / Python __version__ / pyproject.toml in lockstep per cpp-migration-version-policy.md Rule 2; Inno Setup AppVersion deliberately NOT bumped per Rule 1 until phase 8. CHANGELOG row carries the G6 observation table.
  - Phase 3 deferred items (Typeless steps 1/2/3, Step 6 late-audio, Step 7b KeyboardInterrupt, Qianwen SHA mismatch) carry forward unchanged. Phase 4 real-acceptance (G6) is Typeless + RC003 only per ADR-0014 §10; Qianwen Frida adapter work is explicitly NOT in scope.
  - Do not re-run the full PHASE3-REAL-ACCEPTANCE.md table from scratch unless a Phase 3 source file is changed; the Step 1/2/7a PASS results are durable observations.
  - Do not re-run G3 (remotemic_audio_route_bind_smoke, remotemic_upsample_parity, remotemic_audio_route_parity, remotemic_phase4_native_switch, remotemic_phase4_production_routing) or full ctest unless a Phase 4 source file changes; 80126cd's 35/35 results are durable.
first_command_for_next_agent: git status --short --untracked-files=all
```
