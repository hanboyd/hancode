# AI Handover

```yaml
last_updated: 2026-08-31T22:27:00+08:00
agent: hermes-agent handing off to next agent
provider: xiaomi-token-plan-cn handing off to next
model: mimo-v2.5 handing off to next
git_commit_sha: bab62b5
current_phase: **Phase 5 step 2 sub-pass B closed** at bab62b5. Real Win32 adapters replacing 4 stubs: LowLevelKeyboardHook (WH_KEYBOARD_LL + SPSC ring + 5 us budget), RawInputSource (RIDEV_INPUTSINK + RC003 VID/PID filter + RIM_TYPEKEYBOARD/HID decoding), SendInputActionSink (bounded queue + worker thread + physical scan-code path + system action dispatch), FridaHidTapSource (loopback TCP + JSON gatt_read parser + usage ID decode). All 4 have #ifdef _WIN32 / #else fail-closed stubs. 21/21 ctest Debug + 21/21 ctest Release. /W4 clean. Zero behavior change (production still uses python baseline). Phase 5 has 1 step remaining: step 3 (native switch + production routing closeout + G6 real-device validation per ADR-0015 §9 + version bump 0.5.0-candidate → 0.6.0-candidate).
current_task: Phase 5 step 2 sub-pass B closed. Next is step 3 closeout: native switch + production routing closeout + G6 real-device validation per ADR-0015 §9 + version bump 0.5.0-candidate → 0.6.0-candidate (CMakeLists.txt / Python __version__ / pyproject.toml lockstep per Rule 2; Inno Setup AppVersion NOT bumped per Rule 1).
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
  - Phase 4 step 3 (b5c4d9b): pybind11 binding seam — bind_module.cpp exposes PcmFormat/IAudioRoute/WasapiAudioRoute/FakeAudioRoute; audio_route_native.py module-level switch; G3 bind smoke 10/10; 31/31 ctest Debug + Release
  - Phase 4 step 4 (0545cfd): G3 byte-exact parity harness — upsample + audio_route parity; tools/run_parity_test.py rewritten; 33/33 ctest
  - Phase 4 step 5 (80126cd): native switch + production routing — audio_route_native.py module-level dispatch; G5 verification 4/4; 35/35 ctest
  - Phase 4 step 6 (18320f7): closeout — ADR-0014 accepted, version 0.5.0-candidate, CHANGELOG [0.5.0-candidate]; 35/35 ctest
  - Phase 5 step 1 (3a547e5): ADR-0015 (proposed) + IInputSource/IHostActionSink/ActionResolver/InputEvent contracts + 5 stubs + 2 recording doubles + 5 ctest targets; 40/40 ctest
  - Phase 5 step 2 sub-pass A (7ffc269): real DefaultActionResolver + real HotkeyPhysicalizer; 41/41 ctest
  - Phase 5 step 2 sub-pass B (bab62b5, this session): real Win32 adapters replacing 4 stubs — LowLevelKeyboardHook (WH_KEYBOARD_LL + SPSC ring + 5 us QPC budget + message-pump thread), RawInputSource (RIDEV_INPUTSINK for usage 0x01/0x0C + RC003 VID/PID filter + RIM_TYPEKEYBOARD/HID decode + SPSC ring), SendInputActionSink (bounded queue + worker thread + user32.SendInput batch + physical scan-code modifiers + system action dispatch via SendMessage/keybd_event), FridaHidTapSource (loopback TCP socket 127.0.0.1:30684 + JSON gatt_read parser + 9-byte HID report decode + SPSC ring). All 4 have #ifdef _WIN32 / #else fail-closed non-Windows stubs. CMakeLists.txt updated. 3 test files updated (test_i_input_source: windows_stubs_now_real, test_i_host_action_sink: send_input_starts_on_windows, test_low_level_keyboard_hook_stub: inlined assert for /W4 C4189). 21/21 ctest Debug + 21/21 ctest Release; /W4 clean (only C4324 alignment padding); G7 verifier 19/19; step 4 parity 2/2; step 5 helper 4/4; --dry-run passes.
tests_run:
  - command: ctest -C Debug
    result: passed (21/21)
  - command: ctest -C Release
    result: passed (21/21)
  - command: python tools/verify_phase3_production_routing.py
    result: passed (19/19)
  - command: python tools/verify_phase4_native_switch.py
    result: passed (4/4)
  - command: python tools/verify_phase4_audio_parity.py
    result: passed (2/2)
  - command: python -m ovb_rc003 --dry-run
    result: passed (all ovb_rc003 modules imported successfully)
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
next:
  - On any new session: git log --oneline -5 to find the latest commit SHA (bab62b5), then read CURRENT_STATUS.md and this handover before doing anything
  - **Phase 5 step 2 sub-pass B closed.** Phase 5 has 1 step remaining: step 3 closeout (native switch + production routing closeout + G6 real-device validation per ADR-0015 §9 + version bump 0.5.0-candidate → 0.6.0-candidate).
  - Step 3 involves: (a) Python bridge wrappers for input_source / host_action_sink / key_suppressor modules, (b) production routing tests + native switch tests, (c) ADR-0015 flip to accepted, (d) version bump CMakeLists.txt / Python __version__ / pyproject.toml lockstep, (e) CHANGELOG [0.6.0-candidate] entry with G1/G2/G3/G5 gate table + G6 deferred row.
  - Phase 3 deferred items (Typeless steps 1/2/3, Step 6 late-audio, Step 7b KeyboardInterrupt, Qianwen SHA mismatch) carry forward unchanged. Phase 4 G6 (RC003 + VB-Cable + Typeless) per PHASE4-REAL-ACCEPTANCE.md is still deferred.
  - Do not re-run the full PHASE3-REAL-ACCEPTANCE.md table from scratch unless a Phase 3 source file is changed; the Step 1/2/7a PASS results are durable observations.
  - Do not re-run Phase 5 step 1 tests unless a Phase 5 input source file changes; results are durable at 7ffc269. The step 2 sub-pass A tests (action_resolver / hotkey_physicalizer) are durable at 7ffc269. The step 2 sub-pass B test updates are durable at bab62b5.
first_command_for_next_agent: git status --short --untracked-files=all
```
