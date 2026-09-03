# AI Handover

## Active handover — 2026-09-03: package first usable release as 1.0.0

- The user explicitly selected `1.0.0`, not `0.8.0`, for the first usable
  release after the incremental C++ refactor. Describe it accurately as a
  hybrid release: normal users remain on the real-device-accepted Python
  coordinator; the native coordinator is an explicit diagnostic opt-in.
- The freshly rebuilt frozen program passed real RC003 + VB-CABLE + Typeless
  acceptance before the version-only rebuild: one shortcut around 75 ms after
  each voice-key edge, signal-bearing PCM, and no false second trigger.
- Back -> Delete, Volume Up -> Ctrl+C, and Volume Down -> Ctrl+V are a recorded
  non-blocking known issue. The saved mappings are present. Both the current
  and quarantined prior packages failed in the present Windows state because
  the optional HID tap did not connect and verified WUDFHost access returned
  WinError 5. The user explicitly deferred repair until after packaging.
- Qianwen and native WASAPI audibility remain deferred and must not be included
  in the 1.0.0 acceptance claim. The build is unsigned.
- Final artifacts are under `apps/windows/rc003/dist/release-1.0.0`:
  installer SHA-256 `0e1b3c18f6d8af99ea3d83097e6dd677a787a588619c9764f5c1a7ca3da708eb`;
  portable ZIP SHA-256
  `8f41f94187bdaf1e00e6c680ab22a3cf3c76ed5e3977c65926a9a9e0bdab8bd1`.
  Silent install returned 0; installed executable and uninstall registration
  both report 1.0.0, and the old frozen `app` subtree is absent.
- The user subsequently accepted the installed 1.0.0 as the formal program
  for current personal use. Short Typeless use was normal and did not reproduce
  the prior missed-open or spontaneous post-release reopen symptoms. Keep the
  existing deferred boundaries explicit; do not reinterpret this as Qianwen,
  native WASAPI, or Back/Volume-key acceptance.

## Active handover — 2026-09-03: packaged Typeless path accepted

- The first usable `0.8.0-candidate` PyInstaller build passed a real RC003 +
  VB-CABLE + Typeless run on the release-default Python coordinator. Press
  opened Typeless, release closed the input, PCM contained signal, both host
  shortcut dispatches were approximately 76-77 ms after their physical edges,
  and no uncommanded second trigger appeared after release.
- The apparent package failure immediately before the passing run was a live
  configuration mismatch: RemoteMic sent `RightAlt`, while Typeless expected
  `LeftCtrl+LeftAlt`. RemoteMic now uses `lctrl+lalt`; the user independently
  restored Typeless to the same shortcut and selected `CABLE Output`.
- The accepted packaged bridge was launched only after the source bridge
  stopped cleanly. It remains the active usable process at handover unless the
  operator has since stopped it.
- Qianwen and native `WasapiAudioRoute` audibility remain explicitly deferred.
  Do not fold either into the acceptance claim.
- The installer/portable artifacts are unsigned. Install, uninstall, expected
  user-data retention, clean reinstall and the final installed-build Typeless
  run all passed on this machine.
- Final candidate files are grouped under
  `apps/windows/rc003/dist/release-0.8.0-candidate`. The installer SHA-256 is
  `B2ADAB04A98A494EE4814E3E8F872E6F9790A6BEE660EE3D939FC1C0CA2FE2B1`; the
  portable ZIP SHA-256 is
  `FD16D2504E9E2EDF8737C5C7F289EC8548038B4377CDE24578ACC3AB18CB779A`.
  Inno compilation is warning-free and the packaging contract suite passed
  116/116. Silent install returned 0; uninstall removed process, registration,
  program files and shortcuts while preserving configuration and key bindings;
  clean reinstall returned 0 and passed the final RC003 + Typeless run. The
  files remain unsigned. The final installer was repacked afterward only to
  update its bundled readme with these results; application bytes and
  installer logic are unchanged.

## Active handover — 2026-09-03: first usable release path pinned

- Default `application_coordinator` is now `python`; this is the real RC003 +
  Typeless accepted path and no longer requires an environment override.
- Native coordinator remains available only when explicitly selected with
  `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=native`. Its WASAPI silence
  and Qianwen integration are intentionally deferred by user direction; do not
  silently re-enable either for the first usable release.
- ADR-0018 was amended rather than deleting native code or its tests. Focused
  routing/entry tests passed 34 tests with 2 binding-environment skips; the
  full package-level Python suite passed 1090 tests with 21 skips.
- This default was subsequently exercised through both a source restart and
  the packaged build; both passed the real RC003 + Typeless path. See the
  newer active handover above.
- Git inspection and any future commit must be restricted to this repository
  worktree directory; do not include sibling paths from a wider Git root.

## Active handover — 2026-09-03: Typeless timing/retrigger fix accepted

This section supersedes the older active handovers for the immediate voice-key
task. The working tree still contains substantial unrelated, uncommitted
Phase 6–9 work; preserve it and do not reset, clean, rebase, or overwrite it.

Current proven product path:

- Launch with `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=python` and
  `PYTHONPATH=apps\windows\rc003\src;build\Release` (ordering equivalent is
  acceptable as long as both the source package and `_C.pyd` resolve).
- Physical F5 down and up are the only Typeless toggle owners. Each edge is
  queued with a monotonic timestamp, and the voice-edge worker begins one
  complete Typeless shortcut at edge + 75 ms. Auto-repeat does nothing;
  ATVV control/audio events cannot toggle Typeless without the physical latch.

Root cause and correction:

- F5 was incorrectly entering the generic Raw Input correlation wait even
  though F5 is deliberately never armed there. The serialized low-level hook
  waited 62–78 ms on every long-hold repeat, creating a repeat backlog that
  survived KeyUp, delayed the closing shortcut, and produced a false new press.
- `LegacyKeySuppressor.consume_armed_key_event()` now returns immediately for
  F5, leaving F5 to the dedicated voice suppressor/state latch. The attempted
  800 ms post-release window was removed because live evidence showed a
  retrigger after it expired and because it could suppress a valid rapid press.

Evidence:

- Before fix: KeyUp `06:25:37.769`; closing tap completed
  `06:25:38.422`; queued repeats continued; a false new press was accepted at
  `06:25:38.608`; unauthorized third tap completed `06:25:39.179`.
- After fix: press dispatch 78 ms after its F5 edge; release dispatch 76 ms
  after KeyUp; the full 70 ms closing chord completed 151 ms after KeyUp; PCM
  had signal; no later F5/shortcut/audio-start appeared in the observation
  window. The user confirmed normal press timing and no post-release Typeless
  reopening without another voice-key press. Mark this RC003 + Typeless row
  `passed`, not merely software-verified.
- Software: the focused 76-test set passed three consecutive runs (1 skip per
  run). The expanded `test_app_wiring`, `test_voice_controller`,
  `test_atvv_session`, `test_ble_transport_contract`, and
  `test_legacy_key_suppressor` set passed 137 tests with 1 environment skip.
  The correct package-level `python -m unittest` full suite passed 1090 tests
  with 21 environment skips. A prior bare discovery invocation produced two
  collection errors because it removed the `tests` package context; that
  invocation is not a product-test failure and was superseded by the passing
  package-level run.

Important timing interpretation:

- The code controls a 75 ms scheduling target from low-level F5 callback to
  the start of the Typeless chord. Windows/Python scheduling is not hard real
  time; the accepted run measured 76–78 ms. `send_voice_key_combo_tap()` then
  intentionally holds the chord for 70 ms before releasing it.

Next safe work:

1. Keep the bridge on the Python coordinator while the native WASAPI route is
   silent. Do not silently switch the accepted product back to native.
2. If continuing the native track, isolate `WasapiAudioRoute` audibility with
   its existing diagnostics; do not change this accepted input path unless a
   regression supplies new evidence.
3. Packaging remains deferred/prohibited by the existing project direction.

## Active handover — 2026-09-02 evening: acceptance passes on the Python rollback

This section supersedes the older active handovers. The working tree is
intentionally uncommitted Phase 6/7/8/9 work plus this session's fixes;
do not reset, clean, rebase, or overwrite unrelated files.

What this session established:

- The two-state-machine key design (physical hold -> Typeless toggle taps)
  is verified correct on real hardware. The final untested edit
  (external voice-edge/host-action ownership on the native worker) is
  the right key logic; its only failure was audio.
- The native coordinator's audio route (`WasapiAudioRoute`) is silent on
  this machine. Evidence chain: "audio start failed: already started"
  (route never closed after a hold -> every later start failed -> all
  frames dropped) was fixed; then COM MTA init on the BLE dispatcher
  thread fixed CO_E_NOTINITIALIZED; GetMixFormat negotiation (48 kHz
  stereo float32), float32 conversion, channel duplication, device-paced
  writer (0 errors) and session volume 1.0 were all verified — yet a
  simultaneous PortAudio stream on the same CABLE endpoint is audible
  while the native session is not. Root cause unidentified. Diagnostic
  getters exist on the binding (`last_error`, `matched_endpoint_id`,
  `mix_channels`, `mix_bits_per_sample`, `mix_is_float`,
  `session_volume_info`, `chunks_pushed_count`, `output_sample_rate_hz`).
- The product currently works end-to-end via the ADR-0018 rollback:
  launch the bridge with
  `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=python` and PYTHONPATH
  `build\Release;apps\windows\rc003\src`. Real acceptance with this path:
  press opens Typeless once, holds of 35-50 s stream PCM, release sends
  the closing toggle promptly, Typeless commits text at the focused
  window after its own 20-25 s finalization (third-party latency, not
  app-controllable), no Notepad date/time, repeated rounds clean,
  graceful stop exits in ~1 s.
- Native-side fixes kept for the next attempt: coordinator closes the
  audio route on release, tolerates fragmented AudioStarted, Error events
  no longer trigger reconnect (a reconnect stops the F5 guard -> the
  observed date/time leak), the voice-edge worker uses a stop event (no
  sentinel), survives native-call exceptions, and skips the close tap
  when the open tap failed.
- Tests: extended `tests/unit/test_application_coordinator.cpp` with the
  external-owner scenario (Debug+Release pass); 4 new voice-edge-worker
  tests in `apps/windows/rc003/tests/test_phase7_coordinator_routing.py`
  (12/12); targeted ctest 4/4. Python source unchanged for the baseline
  app itself this session.

Next session priorities:

1. Identify why the native WASAPI session is silent while PortAudio on the
   same endpoint is audible (tone-probe scripts were scratch files under
   the temp dir; the diagnostic getters above are the reproducible entry
   point). If it cannot be fixed quickly, formally pin the Python
   coordinator for this release cycle per ADR-0018.
2. Real acceptance already passed on the Python path; do not re-test the
   whole matrix unless a voice/audio source file changes.
3. Update CURRENT_STATUS.md and this file before switching models.
4. Packaging remains prohibited. Music ducking remains deferred.

## Active handover — 2026-09-02 Typeless physical-edge correction

This section supersedes the older active handover for the immediate task.
The working tree is intentionally uncommitted and contains substantial prior
Phase 6/7/8/9 work. Preserve it; do not reset, clean, rebase, or overwrite
unrelated files.

The required behavior is two coupled but different state machines:

1. RC003 voice hardware is hold-to-talk: physical down opens the remote mic,
   repeats while held are ignored, physical up closes the remote mic.
2. Typeless is toggle-on-shortcut: physical down sends one complete shortcut
   tap to open; physical up sends a second complete shortcut tap to close and
   commit text.

Latest human evidence before the final edit:

- passed: Notepad did not receive date/time (physical F5 leak suppressed);
- passed: direction keys move exactly one character per press;
- failed: Typeless stayed closed throughout a long hold, opened only on
  release, then closed on the next voice-key press;
- deferred by instruction: music ducking;
- prohibited: packaging/installer work.

The final code edit, intentionally not tested at the user's request:

- `application_coordinator_native.py` now takes authoritative down/up from the
  suppressed low-level-hook F5 callback, collapses auto-repeat, applies the
  existing 200 ms release debounce, and serializes edges on
  `native-voice-edge-worker`.
- That worker now calls `win32_input.send_voice_key_combo_tap()` **before** it
  calls `ApplicationCoordinator.handle_physical_mic_edge()`. This keeps the
  Typeless toggle outside the native BLE/audio mutex that delayed it until
  release in the real run.
- Native `CoordinatorConfig` has `external_voice_edge_owner` and
  `external_voice_host_action_owner`. The Windows binding sets both true.
  Raw Input and ATVV are no longer allowed to own the physical mic lifecycle,
  and C++ must not send a duplicate Typeless shortcut.
- `RawInputSource` now drains live events instead of retaining them until
  shutdown. `ApplicationCoordinator::stop()` releases the mutex before joining
  the input pump, addressing the observed stop hang.

Receiver procedure:

1. Read `AGENTS.md`, `docs/ai_context/INDEX.md`, `PROJECT_CONTEXT.md`,
   `CURRENT_STATUS.md`, this file, ADR-0017/0018/0020, and inspect
   `git status --short` plus the focused diff before editing.
2. Restate the two-state-machine contract above. Do not redesign it and do not
   switch back to ATVV timing as the host-toggle owner.
3. Review the final untested diff first, especially worker lifecycle, shortcut
   failure rollback, reconnect queue state, native voice-controller state, and
   shutdown ordering. Make the smallest corrective changes needed.
4. Build and run only the smallest relevant automated checks first. Then ask
   for one real Notepad/Typeless hold test: down opens immediately once; hold
   stays open and records audio; up closes once and commits; no date/time.
5. Record real hardware results as passed/failed/deferred. Do not infer success
   from builds or process state. Do not package.

Last automated evidence predates the final host-shortcut-owner edit: targeted
CTest Debug 5/5, Release 5/5, Python 83 passed / 3 skipped / 1 unrelated
Python-3.14-specific test deselected. The final edit is `unverified`.

## Active handover — 2026-09-02, Codex

The active source base is `8fb6506` plus the current uncommitted Phase 6/7/8/9 working tree. Phase 6, Phase 7 coordinator source, the Phase 8 native-default source switch, and the Phase 9 copied UI/native-state route are implemented. Do not restart them from the older YAML snapshot below.

- ADR-0020 closes the code-owned parts of three reported bugs. Default
  direction/OK identity mappings now use one physical owner. The native voice
  coordinator acquires `WindowsVoiceAudioPolicyLease` before the host hotkey,
  temporarily selecting CABLE Output for all capture roles and ducking mode
  `Do nothing`, then restoring on voice stop/disconnect/shutdown.
- Local integration evidence: synthetic CABLE loopback passed
  (`peak=0.141422`, `rms=0.026593`); normal lease restore passed; forced-exit
  recovery restored a missing ducking value and removed the marker. Final
  registry state contains neither marker nor ducking override.
- Current regression: 51/51 CTest Debug and Release, 1107 Python tests with 7
  skipped, 400-file public-boundary scan, `git diff --check`. Hardware/target
  application acceptance remains deferred; custom remap suppression still has
  the existing Hook/Raw Input device-identification limitation.

- Phase 9 chose the retained-QML route in ADR-0019. All seven QML files are
  byte-identical to the accepted UI and protected by the copy-contract hash
  fixture. `UiSettingsState` now owns the connection/mapping selection state
  in C++; PySide6 remains the rendering, model-notification and OS-adapter client.
- Phase 9 verification passed: 51/51 CTest Debug, 51/51 CTest Release,
  1105 Python tests (7 skipped), 400-file public-boundary scan and
  `git diff --check`. Source-only fallback without `_C.pyd` also passed.

- Phase 8 source candidate now defaults `application_coordinator` to native and
  bumps source metadata to `0.8.0-candidate`. The complete rollback is
  `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=python`; shadow and silent
  native-binding fallback fail closed. See ADR-0018 and
  `test_phase8_native_default.py`.
- Phase 8 source verification passed: 49/49 CTest Debug, 49/49 CTest Release,
  1100 Python tests (7 skipped), 395-file public-boundary scan and
  `git diff --check`. Direct construction selected `NativeCoordinatorApp` by
  default and `RC003App` under the rollback override. A Release-only test UB
  in `test_edge_debouncer.cpp` was fixed by removing the post-destruction raw
  pointer read.
- Packaging is still prohibited by the user's current instruction. Installer
  metadata was not changed and no frozen, portable or installer artifact was
  created. Keep the Python core and behavior fixtures for at least one stable
  release cycle.

- Added the C++/WinRT BLE transport, bounded callback mailbox, fake transport, pybind11 seam, Python native session bridge, production routing, unit/binding/switch tests, and accepted ADR-0016.
- Fixed typed native ATVV event conversion and deterministic native-binding shutdown; also closed four pre-existing full-suite blockers exposed by the release build.
- Phase 7 added ADR-0017, `ApplicationCoordinator`, numbered idempotent commands, rollback/cleanup, C++ event mailbox, pybind API, `NativeCoordinatorApp`, existing supervisor integration, and Python gesture/binding/statistics adapter routing for ordinary input events.
- Phase 0–5 corrective audit completed after the Phase 7 source work. Corrected Phase 2 FIR sample drift and DC argument parity; Phase 3 stale debounce pending state plus CAPS/8 kHz rejection ABI; Phase 4 oversized queue UB/counter races/lifecycle/capacity/fallback; and Phase 5 input logging plus host-action fallback/dispatch/teardown accounting. Regression tests were added for every reproduced defect.
- Evidence after the corrective audit: 48/48 Debug CTest, 48/48 Release CTest, 1095 Python tests passed (7 skipped), and the 393-file public-boundary scan passed. `git diff --check` passed.
- Real RC003 evidence: discovery and identity selection passed; native connection, control notification, `GET_CAPS` write, cleanup, and two reconnect cycles passed; dropped native mailbox events remained zero. Audio notification is deferred because nobody was present to press the voice key.
- Packaging is explicitly deferred. A disposable freeze check exited 0 and proved `_C.pyd` collection, after which `dist` and `build/pyinstaller-work` were deleted. Do not rebuild an installer, portable ZIP, or frozen release until all code phases and the joint review are complete.
- Real Phase 7 bridge entry passed native start plus external stop/cleanup with exit 0; direct hardware smoke observed events and zero drops. Physical voice/audio, Typeless/Qianwen response and sleep/wake remain deferred because they were not observed.
- Do not recreate package artifacts yet. Next work is verification of the Phase
  8 source switch plus remaining real-interaction acceptance when a human is
  available. Phase 0–5 automated corrective audit is complete; do not revert
  its regression coverage.

The older YAML handover is retained below for detailed Phase 0–5 provenance; its Phase 6 entry instruction is superseded by this section.

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
