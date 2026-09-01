# AI Handover

```yaml
last_updated: 2026-09-01T05:30:00+08:00
agent: sonnet handing off to next agent
provider: minimax handing off to next
model: MiniMax-M3 handing off to next
git_commit_sha: 0d394ea (Phase 5 step 3 source closeout) + pending docs refresh
current_phase: **Phase 5 step 3 source closeout committed at 0d394ea; native-input gaps (items 1+2) verified closed in this session.** Step 3 source code: bind_module.cpp section 13 (InputSourceKind / InputEventKind / SystemAction / ButtonId / ResolvedActionKind enums + InputEvent / ResolvedAction POD + IInputSource / IHostActionSink trampoline interfaces + FakeInputSource / FakeHostActionSink recording doubles + ActionResolver / DefaultActionResolver + HotkeyPhysicalizer + RawInputSource / LowLevelKeyboardHook / FridaHidTapSource / SendInputActionSink behind #ifdef _WIN32) + set_event_sink trampoline (bind_module.cpp:54-220, SinkHolder + input_source_sink_trampoline + g_input_source_sink_registry + atexit drain) + HotkeyPhysicalizer::release_held (hotkey_physicalizer.cpp:374-385, iterate held_keys_ + emit inverse up) + remotemic_native/__init__.py re-exports (ADR-0011) + 2 bridge wrappers (input_source_native.py / host_action_sink_native.py) with defensive getattr fallbacks + app.py RC003App.__init__ wiring + 2 new ctest targets (remotemic_phase5_input_native_switch 13 子测试 + remotemic_phase5_input_production_routing 3 source-level tests) + test_input_bind_smoke.py 17 tests. tools/verify_phase5_native_switch.py 4 conditions PASS. scripts/append_bindings.py deleted. ADR-0015 flipped proposed → accepted. Version bumped 0.5.0-candidate → 0.6.0-candidate (CMakeLists.txt / Python __version__ / pyproject.toml / test_bind_smoke.py lockstep; Inno Setup AppVersion deliberately untouched per Rule 1). 43/43 ctest Debug + 43/43 ctest Release. Phase 5 closed at 0.6.0-candidate; Phase 6 (BLE / WinRT) is the next entry per cpp-migration-execution-plan.md §6.
current_task: Phase 5 fully closed (source committed + native-input gaps verified + docs refresh in this session). Next is Phase 6 (BLE / WinRT) per cpp-migration-execution-plan.md §6. Before starting Phase 6, the user must authorize Phase 6 entry (executive call).
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
  - Phase 5 step 2 sub-pass B (bab62b5): real Win32 adapters replacing 4 stubs — LowLevelKeyboardHook (WH_KEYBOARD_LL + SPSC ring + 5 us QPC budget + message-pump thread), RawInputSource (RIDEV_INPUTSINK for usage 0x01/0x0C + RC003 VID/PID filter + RIM_TYPEKEYBOARD/HID decode + SPSC ring), SendInputActionSink (bounded queue + worker thread + user32.SendInput batch + physical scan-code modifiers + system action dispatch via SendMessage/keybd_event), FridaHidTapSource (loopback TCP socket 127.0.0.1:30684 + JSON gatt_read parser + 9-byte HID report decode + SPSC ring). All 4 have #ifdef _WIN32 / #else fail-closed non-Windows stubs. CMakeLists.txt updated. 3 test files updated (test_i_input_source: windows_stubs_now_real, test_i_host_action_sink: send_input_starts_on_windows, test_low_level_keyboard_hook_stub: inlined assert for /W4 C4189). 21/21 ctest Debug + 21/21 ctest Release; /W4 clean (only C4324 alignment padding); G7 verifier 19/19; step 4 parity 2/2; step 5 helper 4/4; --dry-run passes.
  - Phase 5 step 3 closeout (commit 0d394ea, plus docs refresh in this session): bind_module.cpp section 13 (input layer binding seam) + remotemic_native/__init__.py re-exports + two Python bridge wrappers (input_source_native.py / host_action_sink_native.py) with defensive getattr fallbacks + app.py RC003App.__init__ constructs the factories + two new ctest targets (13 + 3 tests) + tools/verify_phase5_native_switch.py (4 conditions) + scripts/append_bindings.py deleted + ADR-0015 flipped proposed → accepted + version bump 0.5.0-candidate → 0.6.0-candidate (lockstep CMake / Python __version__ / pyproject.toml / test_bind_smoke.py; Inno Setup AppVersion NOT bumped per Rule 1) + CHANGELOG [0.6.0-candidate] entry with full G1/G2/G3/G5 gate table + G6 deferred row. Critical bug caught during closeout: original export_values() on InputEventKind / SystemAction would have caused m.SystemAction double-registration; export_values() removed to match existing ErrorCode / VoiceTriggerMode convention. 43/43 ctest Debug + 43/43 ctest Release; G3 version sync (info.version == "0.6.0") confirmed; G7 Phase 3 / Phase 4 regressions all green; --dry-run passes.
  - **Native-input gaps CLOSED in this session**: items 1+2 from the explicit deferred list verified against fresh Debug `_C.pyd` (commit 0d394ea source + rebuilt pyd from this session's Python):
    - **Item 1 — `IInputSource::set_event_sink` native binding**: `bind_module.cpp:54-220` implements per-source `SinkHolder` (mutex + py::object + atomic armed) + `input_source_sink_trampoline` (C trampoline taking GIL on pump thread, NOT the WH_KEYBOARD_LL callback path per ADR-0015 §3.6 5 us budget) + process-wide `g_input_source_sink_registry` + atexit drain handler. Verified by `test_input_bind_smoke.py` 5 tests (test_fake_input_source_set_event_sink_dispatches_event, test_set_event_sink_none_clears_previous_sink, test_set_event_sink_replaces_previous_sink, test_sink_exception_is_swallowed, test_release_sink_drops_callable) — 17/17 PASS.
    - **Item 2 — `HotkeyPhysicalizer::release_held()`**: `hotkey_physicalizer.cpp:374-385` iterates `held_keys_` and emits inverse up events through the bound sink; idempotent (clears held_keys_ at end). Verified by 4 C++ tests in `tests/unit/test_hotkey_physicalizer.cpp` + `test_input_bind_smoke.py` `test_hotkey_physicalizer_release_held_is_noop_after_tap` — 19/19 C++ PASS + 17/17 Python PASS.
  - **Phase 5 status at this commit**: implementation complete on C++ side (interfaces + recording doubles + pure-logic + 4 real Win32 adapters + pybind11 binding seam + set_event_sink trampoline + release_held safety net) + Python bridge side (two module-level switch factories with defensive fallbacks); automated gates all green (43/43 Debug + 43/43 Release; G3 version sync; G5 native switch 13 子测试; G5 production routing 4/4 + 3/3; G7 Phase 3 / Phase 4 regressions; test_input_bind_smoke 17/17; HotkeyPhysicalizer 19/19). **Real acceptance (G6) DEFERRED only — see known_problems + next below.**
tests_run:
  - command: ctest -C Debug
    result: passed (43/43)
  - command: ctest -C Release
    result: passed (43/43)
  - command: build/Debug/remotemic_hotkey_physicalizer_tests.exe
    result: passed (19/19 incl. 4 release_held tests)
  - command: build/Debug/remotemic_i_input_source_tests.exe
    result: passed (4/4 incl. windows_stubs_now_real)
  - command: build/Debug/remotemic_i_host_action_sink_tests.exe
    result: passed (6/6 incl. send_input_starts_on_windows)
  - command: python -m unittest tests.bind.test_input_bind_smoke
    result: passed (17/17 incl. set_event_sink + release_held + ActionResolver + HotkeyPhysicalizer)
  - command: python tools/verify_phase3_production_routing.py
    result: passed (19/19)
  - command: python tools/verify_phase4_native_switch.py
    result: passed (4/4)
  - command: python tools/verify_phase4_audio_parity.py
    result: passed (2/2)
  - command: python tools/verify_phase5_native_switch.py
    result: passed (4/4)
  - command: python -m ovb_rc003 --dry-run
    result: passed (all ovb_rc003 modules imported successfully)
known_problems:
  - **IInputSource::set_event_sink native binding: CLOSED 2026-09-01** — bind_module.cpp:54-220 implements SinkHolder + trampoline + registry + atexit drain. Verified by test_input_bind_smoke.py 5 tests.
  - **HotkeyPhysicalizer::release_held(): CLOSED 2026-09-01** — hotkey_physicalizer.cpp:374-385 emits inverse up events. Verified by 4 C++ tests + 1 Python test.
  - **RC003 real-device acceptance: deferred** — no physical RC003 + VB-Cable + Typeless simultaneously available this session; bridge-side path exercised but per-step RC003 app response matrix awaits hardware per PHASE3-REAL-ACCEPTANCE.md + PHASE4-REAL-ACCEPTANCE.md.
  - **Notepad acceptance: deferred** — Notepad focus + chord input is part of the Typeless step 4 acceptance matrix; not exercised against the native path on real hardware.
  - **Typeless acceptance: deferred (PARTIAL — step 4 only observed 2026-08-31)** — steps 1 (no double-trigger on short press), 2 (5s HOLD mode), and 3 (3× rapid toggle) remain unverified against the native path.
  - **Qianwen acceptance: deferred (structural, NOT Phase 5 in scope)** — qianwen_physicalizer.py:28 locks SHA-256 2ef313df...; user's installed QianwenIMEUiClient.exe SHA-256 a6ab353a... mismatch → adapter fails closed. Unblocking requires re-discover callback RVA + re-lock SHA + Frida-session verification matrix.
  - Back / volume-up / volume-down do not reach Raw Input or the low-level keyboard hook; elevated WUDFHost injection and direct HID-over-GATT characteristic access are denied by Windows; surface this as a gap rather than ship a SYSTEM workaround
  - Step 6 late-audio guard cannot be triggered in healthy RC003 + software VB-Cable (the procedure's "unplug VB-Cable" workaround was wrong about the mechanism — VB-Cable is the audio sink, not the ATVV notification source). Genuine RC003 firmware fault would be needed; guard logic is unit-test covered
  - Step 7b KeyboardInterrupt path cannot be cleanly triggered from Claude Code background-task harness (Windows console signal semantics do not reach Python's SetConsoleCtrlHandler). 7a's evidence covers the same `app.stop()` cleanup
  - Installer uninstall/residue check + release signing need explicit user authorization
do_not_change:
  - Do not produce PyInstaller / Inno / portable ZIP artifacts before Phase 8 (cpp-migration-version-policy.md Rule 1)
  - Do not bump voice_release_debounce_seconds default or its [0.050, 0.500] clamp band without a fresh ADR; the value is pinned at three layers on purpose
  - Do not flip any REMOTEMIC_NATIVE_CHOICE_* env var to native for normal users; default stays python
  - Do not auto-fix a failed real-acceptance row; stop, paste app.log excerpt + 1-line symptom
  - Do not start a C++ rewrite of working product functionality outside the phase plan
  - Do not add export_values() to any of the input-layer enums in bind_module.cpp section 13; removing it was deliberate to avoid m.SystemAction double-registration (InputEventKind.SystemAction value + SystemAction enum type both at module scope). See CURRENT_STATUS.md "Phase 5 step 3 — closeout" section for the bug history.
next:
  - On any new session: git log --oneline -5 to find the latest commit SHA, then read CURRENT_STATUS.md and this handover before doing anything
  - **Phase 5 status: fully shipped.** Implementation complete on C++ side + Python bridge side; automated gates all green (43/43 ctest Debug + Release + test_input_bind_smoke 17/17 + HotkeyPhysicalizer 19/19 + 4 verify scripts). Remaining deferred items are G6 real-device acceptance only:
    1. ~~IInputSource::set_event_sink native binding~~ — **CLOSED 2026-09-01** (commit 0d394ea + this session verification)
    2. ~~HotkeyPhysicalizer::release_held()~~ — **CLOSED 2026-09-01** (commit 0d394ea + this session verification)
    3. RC003 real-device acceptance (per PHASE3-REAL-ACCEPTANCE.md / PHASE4-REAL-ACCEPTANCE.md)
    4. Notepad acceptance (Notepad focus + chord input, part of Typeless step 4 matrix)
    5. Typeless acceptance (PARTIAL — step 4 only; steps 1/2/3 not exercised on native path)
    6. Qianwen acceptance (structural SHA-256 mismatch, NOT Phase 5 in scope)
  - **Phase 6 (BLE / WinRT) is unblocked and ready to start** per `cpp-migration-execution-plan.md` §6, subject to explicit user authorization. Native-input gaps no longer block entry. Phase 6 scope: BLE transport + WinRT, mirroring Phase 3 / Phase 4 / Phase 5 shape (interfaces + stubs + red-state tests → real impls → real Win32 adapters → binding seam → native switch → production routing → closeout).
  - Phase 3 deferred items (Typeless steps 1/2/3, Step 6 late-audio, Step 7b KeyboardInterrupt, Qianwen SHA mismatch) carry forward unchanged. Phase 4 G6 (RC003 + VB-Cable + Typeless) per PHASE4-REAL-ACCEPTANCE.md is still deferred. Phase 5 G6 same.
  - Do not re-run the full PHASE3-REAL-ACCEPTANCE.md table from scratch unless a Phase 3 source file is changed; the Step 1/2/7a PASS results are durable observations.
  - Do not re-run Phase 5 step 1 / step 2 sub-pass A / step 2 sub-pass B / step 3 tests unless a corresponding Phase 5 source file changes; results are durable.
first_command_for_next_agent: git log --oneline -5 && git status --short --untracked-files=all
```