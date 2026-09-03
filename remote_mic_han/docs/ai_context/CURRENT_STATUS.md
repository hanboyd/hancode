# Current Status

## 2026-09-03 release 1.0.0 — installed and ready

- `version_decision`: user selected `1.0.0` for the first usable release after
  the incremental C++ refactor. This remains a hybrid release: the accepted
  Python coordinator is the normal-user default, while the native coordinator
  remains an explicit diagnostic opt-in.
- `accepted_boundary`: the freshly rebuilt frozen program passed a real RC003
  + VB-CABLE + Typeless run. Press and release each produced one shortcut at
  approximately 75 ms, PCM carried signal, and no uncommanded second window
  appeared.
- `known_deferred_issue`: the user's saved Back -> Delete, Volume Up -> Ctrl+C,
  and Volume Down -> Ctrl+V mappings are intact but the three physical keys do
  not currently trigger. The new build and the quarantined previous build both
  failed in the current Windows state. Their optional HID tap never connected;
  the verified WUDFHost process rejected access with WinError 5. Per user
  direction this is recorded for later and does not block 1.0.0 packaging.
- `other_deferred`: Qianwen integration and native WASAPI audibility remain
  outside the 1.0.0 acceptance claim. The release remains unsigned.
- `artifacts`: `dist/release-1.0.0` contains the unsigned installer and portable
  ZIP plus release notes and `SHA256SUMS.txt`. Installer SHA-256 is
  `0e1b3c18f6d8af99ea3d83097e6dd677a787a588619c9764f5c1a7ca3da708eb`;
  portable ZIP SHA-256 is
  `8f41f94187bdaf1e00e6c680ab22a3cf3c76ed5e3977c65926a9a9e0bdab8bd1`.
- `install_recheck`: silent installation returned 0, the installed executable
  reports 1.0.0, uninstall registration reports 1.0.0, and the old frozen
  `app` subtree is absent while user configuration remains in the installation
  root. A final installed settings launch was observed responding normally;
  ordinary three-key mappings remain deferred pending the later HID fix.
- `final_user_acceptance`: passed for the installed 1.0.0 Typeless boundary.
  The user confirmed the package is the formal program for current personal
  use and that short-term Typeless use has not reproduced the previous
  missed-open or post-release spontaneous reopen problems. This acceptance
  explicitly excludes the already deferred ordinary-key, Qianwen and native
  WASAPI items.

## 2026-09-03 packaged 0.8.0-candidate — real RC003 + Typeless acceptance passed

- `artifact_under_test`: PyInstaller one-directory build
  `dist/RemoteMicRC003/RemoteMicRC003.exe`, version `0.8.0-candidate`.
- `runtime`: packaged executable, Python application coordinator (the release
  default), PortAudio output to VB-CABLE, RC003 hardware, and Typeless.
- `configuration_correction`: the live failure before this run was not a
  packaged-code regression. RemoteMic was sending `RightAlt` while Typeless's
  active dictation binding was `LeftCtrl+LeftAlt`. RemoteMic was changed to
  `lctrl+lalt`; the user independently restored Typeless to the same binding
  and selected `CABLE Output` as its microphone.
- `source_recheck`: passed. One physical press produced one open tap; one
  physical release produced one close tap. Dispatch was approximately 76 ms
  after both edges, PCM contained signal, and no uncommanded trigger appeared
  during the post-release observation window.
- `packaged_recheck`: passed by direct user observation. The source bridge was
  stopped cleanly before launch; packaged PID 7516 connected to the single
  RC003 candidate, received capabilities, enabled the F5 guard and HID tap,
  opened Typeless on press, closed the input on release, carried PCM signal,
  and produced no automatic second trigger. Logged host-shortcut dispatch was
  76-77 ms after the corresponding physical edge.
- `acceptance_boundary`: this passes the first usable release's RC003 +
  Typeless path, including install, uninstall, expected user-data retention,
  clean reinstall and a final installed-build run. It does not pass the
  deferred Qianwen integration or the native coordinator's silent WASAPI
  route. The installer remains unsigned.
- `release_outputs`: `dist/release-0.8.0-candidate` contains the unsigned
  installer, unsigned portable ZIP, release notes and `SHA256SUMS.txt`.
  Installer SHA-256 is `B2ADAB04A98A494EE4814E3E8F872E6F9790A6BEE660EE3D939FC1C0CA2FE2B1`;
  portable ZIP SHA-256 is
  `FD16D2504E9E2EDF8737C5C7F289EC8548038B4377CDE24578ACC3AB18CB779A`.
  The ZIP has 2100 entries under exactly one top-level directory and includes
  the executable. The rebuilt Inno installer compiled without warnings after
  adding a stable `[UninstallRun]` `RunOnceId`; 116 packaging contract tests
  passed. Silent install returned 0. Uninstall removed the running process,
  uninstall registration, installed program files, Start Menu group and
  desktop shortcut; configuration and key-binding hashes were unchanged.
  Reinstall returned 0, did not restore the quarantined legacy `app` subtree,
  preserved the same settings and passed another real Typeless run. After
  release, speech-to-text and the Typeless UI were normal and no extra trigger
  appeared during a user-observed wait longer than 30 seconds.
  The installer was then repacked without code or installer-logic changes so
  its bundled readme records these completed checks; the updated hash above is
  the final candidate hash.

## 2026-09-03 first usable release default — Python coordinator pinned

- `decision`: the first usable release now defaults to the Python application
  coordinator, which is the only coordinator path that passed real RC003 +
  Typeless audio/input acceptance on this machine.
- `behavior`: no environment variable is required for the proven path.
  `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=native` remains an explicit
  development/diagnostic opt-in; `shadow` remains forbidden for this
  side-effecting owner.
- `reason`: the user chose to defer the native `WasapiAudioRoute` silence and
  Qianwen integration. A release default must not enter the known-silent native
  audio path or require an operator-only rollback setting.
- `scope`: default policy, routing tests, ADR-0018 amendment and documentation
  only. Native code and its tests remain intact. The live bridge was later
  restarted and both source and packaged release-default paths passed the
  RC003 + Typeless acceptance described above.
- `verification`: 34 focused routing/entry tests passed with 2 native-binding
  environment skips; the package-level Python suite passed 1090 tests with 21
  environment skips.

## 2026-09-03 Typeless 75 ms timing and post-release retrigger — passed

- `updated_by`: Codex with live user acceptance.
- `scope`: Python coordinator rollback only
  (`REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=python`); no packaging,
  native WASAPI redesign, or unrelated Phase 6–9 changes.
- `accepted_behavior`: physical F5 down and up each begin exactly one complete
  Typeless toggle shortcut on the dedicated voice-edge worker, targeted at
  75 ms after the corresponding low-level-hook edge. Held-key repeats do
  nothing. After KeyUp, no shortcut may be emitted without a new physical
  KeyDown.
- `root_cause`: the low-level hook called `consume_armed_key_event()` for F5
  even though `arm_key_event()` deliberately never arms F5. Every auto-repeat
  therefore blocked the serialized `WH_KEYBOARD_LL` callback for roughly
  62–78 ms. A long hold built a backlog; after the physical KeyUp, queued old
  KeyDown records continued to arrive and one was misclassified as a new
  press. The same backlog delayed the injected closing Typeless chord, so the
  prior implementation did not actually maintain the intended release timing.
- `failed_candidate_rejected`: an 800 ms post-release suppression window was
  removed. Live logs showed the backlog surviving beyond that window and
  retriggering at approximately 839 ms; a larger time window would merely
  hide the queue defect and could swallow a legitimate quick second press.
- `fix`: F5 now bypasses the Raw Input correlation wait and goes immediately
  to the dedicated voice path. Existing `_legacy_f5_is_down` ownership still
  collapses every repeat until the matching KeyUp. The press and release queue
  entries retain their monotonic edge timestamps; the worker targets both
  Typeless tap starts at edge + 75 ms. ATVV `MicButtonPressed`/`AudioStarted`
  still cannot toggle Typeless unless the physical-button latch is down.
- `pre_fix_live_reproduction`: KeyUp `06:25:37.769`; the closing tap did not
  complete until `06:25:38.422`; old repeat KeyDown records arrived through
  766 ms, then the next record was accepted at `06:25:38.608`; an unauthorized
  third Typeless tap completed at `06:25:39.179` and reopened the window.
- `post_fix_live_evidence`: press edge `06:28:15.250`, dispatch at
  `06:28:15.328` (78 ms); KeyUp `06:28:25.734`, closing dispatch at
  `06:28:25.810` (76 ms), full 70 ms chord completed at `06:28:25.885`.
  PCM carried signal (`653` frames, `156720` samples, `9795` ms). No later F5,
  shortcut, or audio-start event appeared during the observation window.
- `user_acceptance`: passed — the user confirmed that press-to-Typeless
  timing looked normal and that Typeless did not reopen after release while
  the voice key remained untouched.
- `timing_boundary`: 75 ms is an explicit scheduling target, not a Windows
  hard-real-time upper-bound. The observed worker dispatch was 76–78 ms; the
  shortcut itself intentionally stays down for 70 ms before its KeyUp records.
- `software_regression`: focused 76-test set passed three consecutive times
  (1 environment skip each); expanded voice/input/ATVV/BLE set passed
  137 tests with 1 environment skip; the correct package-level full suite
  passed 1090 tests with 21 environment skips; `git diff --check` passed with
  only existing LF/CRLF conversion warnings.
- `still_deferred`: native `WasapiAudioRoute` audibility, packaging,
  installer/signing, and any target application other than the accepted
  Typeless run. The bridge remains on the proven Python coordinator path.

## 2026-09-02 real Typeless acceptance — Python rollback passes; native audio route silent

- `updated_by`: DeepSeek V4 Pro (Claude Code session)
- `outcome`: the full acceptance chain works on real hardware through the
  ADR-0018 Python coordinator rollback. The native coordinator's key-timing
  design was validated as correct, but the native `WasapiAudioRoute` renders
  silence on this machine despite exhaustive diagnostics; the product
  therefore runs `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=python`.
- `real_acceptance` (Python coordinator path, RC003 + Typeless + Notepad):
  - passed: press opens Typeless immediately and exactly once; hold of
    35-50 s streams PCM continuously (log: frames=1..1376, peak=7912,
    real speech), F5 auto-repeat deduplicated, no flicker;
  - passed: release emits the closing toggle promptly ("voice physical mic
    released; closing held host shortcut" ~0.2-0.7 s after release);
    Typeless commits the recognized text at the focused window after its
    own 20-25 s finalization (Typeless model: text appears only on close;
    its commit latency is third-party and not app-controllable);
  - passed: Notepad received no date/time in any round (F5 leak suppressed);
  - passed: repeated rounds (three long holds) all worked;
  - passed: graceful stop — external `request_bridge_stop()` returned
    `REQUESTED`, cleanup logged, process exited within ~1 s, no hang;
  - deferred: music ducking (user direction) and packaging (prohibited).
- `native_key_timing_verified`: the final untested edit (external Typeless
  shortcut ownership on the native voice-edge worker) is now verified as
  the correct key logic: with it the user observed "松开语音键之后，窗口会
  紧跟着关闭" and no double toggles. The remaining native gap is audio only.
- `native_audio_gap`: `WasapiAudioRoute` produces no output on this machine.
  Diagnostics added and checked: COM MTA init on the BLE dispatcher thread
  (fixed recurring "audio start failed" / CO_E_NOTINITIALIZED), GetMixFormat
  negotiation at the exact device mix (48 kHz stereo float32), float32
  conversion + mono-to-stereo duplication, device-paced GetBuffer/
  ReleaseBuffer (0 errors, 97/97 chunks accepted), session volume 1.0 not
  muted, correct endpoint ID `{0.0.0.00000000}.{6f21b3aa-...}`. A
  simultaneous PortAudio stream on the same endpoint is audible while the
  native session is not. Root cause remains unidentified; the Python
  baseline's PortAudio playback is the proven path (8/31 real test plus
  this session).
- `native_fixes_kept`: close_voice_session now stops+closes the audio route
  on release (previously the route stayed open and every later start failed
  with "already started", silently dropping all frames); fragmented
  AudioStarted after a mid-hold AudioStopped is tolerated; coordinator
  Error events no longer trigger a full reconnect (a reconnect stopped the
  F5 guard and caused the observed Notepad date/time leak plus minutes of
  downtime); the voice-edge worker uses a stop-event protocol instead of a
  queue sentinel, never dies on a native-call exception, and skips the
  closing tap when the opening tap failed (toggle inversion protection).
- `tests`: C++ coordinator unit test extended with an external-owner
  configuration scenario (Raw Input/ATVV inert, latch-driven open/close,
  fresh restart per hold) — passes Debug and Release; 12/12 Python
  coordinator-routing tests including 4 new voice-edge-worker tests;
  targeted ctest set 4/4. `git diff --check` clean (line-ending warnings only).
- `workaround`: launch the bridge with
  `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=python` (plus the usual
  PYTHONPATH so `remotemic_native` resolves to `build/Release`).
- `next`: fix the native `WasapiAudioRoute` silence (PortAudio on the same
  endpoint works; the native session does not) or formally pin the product
  to the Python coordinator for this release cycle per ADR-0018.

## 2026-09-02 Typeless hold-to-talk handoff — final edit intentionally untested

- `accepted_behavior`: RC003 is physically hold-to-talk. Typeless is a
  toggle-on-complete-shortcut host: one shortcut opens recognition and a
  second shortcut closes it and commits text. Therefore physical F5 down must
  send one complete Typeless shortcut and start the remote microphone;
  auto-repeat while held must do nothing; physical F5 up must send exactly one
  second complete shortcut after the release debounce.
- `real_observation_before_final_edit`: Notepad no longer received F5 and did
  not insert date/time. Direction keys moved one character per press like the
  normal keyboard. In the latest voice run, Typeless did not open while the
  RC003 key was held; it opened only after release, and the next voice-key
  press closed it. This is `failed`, not passed.
- `diagnosis`: the low-level F5 guard correctly swallowed the legacy F5, but
  host shortcut delivery still entered `ApplicationCoordinator` and could
  wait behind continuous native BLE/audio callback ownership of its mutex.
  That explains the observed release-time opening.
- `final_edit`: `NativeCoordinatorApp` now owns the Typeless shortcut on its
  independent, deduplicated voice-edge worker. Each authoritative suppressed
  F5 down/up first calls the established Python
  `send_voice_key_combo_tap()` path, then notifies the native coordinator for
  BLE/audio state. Native configuration sets both
  `external_voice_edge_owner` and `external_voice_host_action_owner`, so Raw
  Input/ATVV cannot compete and C++ cannot emit duplicate host shortcuts.
- `shutdown_fix`: coordinator stop now detaches callbacks and releases its
  mutex before joining the Raw Input thread, preventing the shutdown lockup
  observed during this session.
- `verification_status`: the preceding implementation passed targeted Debug
  and Release CTest (5/5 each) plus 83 Python tests (3 skipped, one unrelated
  Python-3.14 resource-warning test deselected). The final host-shortcut-owner
  edit above was deliberately **not built, started, or tested**, per the
  user's instruction. It must be treated as an unverified candidate.
- `scope`: music ducking was not tested by user direction. Packaging remains
  prohibited. No installer or release artifact was produced.

## 2026-09-02 three known bugs — code fixes and local integration evidence

- `bug2_confirmed`: the default direction/OK path could forward the physical
  RC003 keyboard edge and later inject the same mapped edge. Identity mappings
  now make the physical edge authoritative; neither Python nor the native
  coordinator adapter arms/reinjects it. Normal keyboards remain untouched.
  Custom remaps/secondary gestures still need a device-scoped pre-legacy owner
  to eliminate the Hook/Raw Input race completely.
- `bug1_evidence`: the source previously never managed default capture roles.
  A live synthetic WASAPI probe passed `CABLE Input -> CABLE Output` with
  `peak=0.141422`, `rms=0.026593`, no overflow. At inspection time all three
  default capture roles were already CABLE Output, so endpoint routing is not
  claimed as the unique cause if the symptom still reproduces in that state.
- `bug1_bug3_fix`: `WindowsVoiceAudioPolicyLease` now snapshots and temporarily
  owns Console/Multimedia/Communications capture defaults plus
  `UserDuckingPreference=3` before the transcription hotkey. Voice stop,
  disconnect and app stop restore only lease-owned values.
- `recovery`: normal acquire/restore passed. A forced process exit left the
  recovery marker and ducking value `3`; the next process restored the original
  missing value and deleted the marker. Final machine state has no marker and
  no `UserDuckingPreference` override.
- `verification`: 51/51 CTest Debug; 51/51 CTest Release; 1107 Python tests
  passed with 7 skipped; targeted input tests 14/14; public-boundary scan passed
  across 400 files; `git diff --check` passed (line-ending warnings only).
- `deferred`: real RC003 direction-key behavior, RC003 speech recognition in
  Typeless/WeChat and music audibility during a real communications session.
  No package or installer artifact was produced.

## 2026-09-02 Phase 9 UI copy — QML unchanged, native state active

- `decision`: ADR-0019 retains the accepted PySide6/Qt Quick client. All seven
  QML files are byte-identical to the prior UI and locked by a SHA-256 copy
  manifest; no visual redesign or WinUI translation was performed.
- `native_ui_state`: new C++ `UiSettingsState` owns trigger-mode/hotkey pairing,
  output-endpoint selection, device selection and selected-button state.
  `SettingsController` is now the Qt signal/model/system-adapter layer over that
  native state. Source runs without `_C.pyd` retain an API-compatible fallback.
- `interaction_contract`: the existing real QML offscreen render, page load,
  click/hotspot geometry, contrast, settings persistence, diagnostics worker
  and shutdown tests remain unchanged and passed.
- `verification`: 51/51 CTest Debug; 51/51 CTest Release; 1105 Python tests
  passed with 7 skipped; 400-file public-boundary scan passed;
  `git diff --check` passed.
- `packaging`: still deferred. No `dist`, PyInstaller work directory, installer
  or portable artifact was produced.

## 2026-09-01 Phase 8 source candidate — native default, no packaging

- `scope`: user authorized the Phase 8 work that can be completed unattended,
  while preserving the earlier instruction not to package.
- `routing`: the top-level application coordinator now defaults to `native`.
  `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=python` remains the complete
  one-switch rollback; `shadow` and silent missing-binding fallback are rejected.
- `version`: CMake, Python package metadata and binding smoke expectation moved
  in lockstep to `0.8.0-candidate`. Inno Setup `AppVersion` remains unchanged.
- `retained_contract`: the Python core, its behavior tests and golden fixtures
  remain intact for at least one stable release cycle; no retirement deletion
  has started.
- `not_claimed`: no frozen program, installer, portable ZIP, upgrade test,
  signature check or real-interaction acceptance was produced by this step.
- `verification`: 49/49 CTest Debug; 49/49 CTest Release; 1100 Python
  tests passed with 7 skipped; native binding/dry-run reported `0.8.0`;
  public-boundary scan passed across 395 files; `git diff --check` passed.
  Direct factory probes returned `NativeCoordinatorApp` by default and
  `RC003App` under the explicit Python rollback.
- `corrective`: a full Release rebuild exposed a test-only dangling-pointer
  read in `test_edge_debouncer.cpp`; the cancellation assertion now uses an
  independent probe and both configurations pass.
- `decision`: see ADR-0018.

## 2026-09-01 Phase 0–5 corrective audit

- `scope`: audited the Claude Phase 0–5 implementation against ADR-0011 through ADR-0015 and the Python behavior baseline; no package artifacts were produced.
- `phase0_1`: baseline/default-python routing remains intact. Added the missing Phase 5 keys to the explicit native-choice policy table, repaired extension-missing fallback paths, and completed ADR-0011's `RemoteMicError` type with integer `code` plus `category` attributes.
- `phase2`: fixed the C++ postprocessor reading previously filtered samples instead of the immutable source (sample drift after the first interior tap); DC filter construction now rejects non-positive parameters like Python.
- `phase3`: fixed the native release timer consuming the handler outside C++, which left the C++ pending state armed and allowed duplicate fire. Native ATVV session now rejects malformed CAPS and unsupported 8 kHz exactly like Python and preserves the public `ATVVProtocolError` / `UnsupportedSampleRateError` ABI.
- `phase4`: fixed oversized `BoundedPcmQueue::push` out-of-range erase, made the drop counter race-free, corrected the two-second capacity to use source PCM rate, made writer error accounting atomic, rejected writes after stop, and made restart legal only after close. The missing-extension audio fallback now opens the Python sink correctly.
- `phase5`: fixed undefined logger on native input registration errors; repaired the Python host-action fallback's nonexistent function names, boolean polarity, and system-action dispatch names; unsupported `CodexOpen` no longer reports false success; queue overflow is counted and worker teardown cannot outlive its owner.
- `verification`: 48/48 CTest Debug; 48/48 CTest Release; 1095 Python tests passed with 7 skipped using the project venv; public-boundary scan passed across 393 files; `git diff --check` passed.
- `deferred`: physical voice/audio, Typeless/Qianwen behavior and sleep/wake remain real-interaction gates. Packaging remains deferred by user direction.

## 2026-09-01 Phase 7 source implementation — native coordinator opt-in

- `updated_by`: Codex
- `git_base`: `8fb6506` plus the current uncommitted Phase 6/7 working tree
- `current_phase`: Phase 7 coordinator source path implemented and opt-in production routing verified. Packaging remains intentionally deferred.
- `product_routing`: `ApplicationCoordinator` is the single native owner of BLE/session/audio/input/host sink under the explicit `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=native` switch. Default remains Python until Phase 8. Python retains settings, gestures, user bindings, statistics and third-party adapters through the coordinator event API.
- `real_hardware`: the paired RC003 was discovered by the existing identity layer; the new C++/WinRT transport connected, received control notifications, wrote ATVV `GET_CAPS`, disconnected cleanly, and repeated the connect/notify/disconnect cycle twice with zero dropped mailbox events. The Python native session bridge returned a typed `CapsReceived` event.
- `packaging`: deferred by user direction. A disposable local freeze was used only to verify that the native package could be collected and start; `dist` and `build/pyinstaller-work` were then deleted. No installer, portable artifact, installation, publication, or binary commit remains.
- `phase7_hardware`: direct coordinator smoke passed start/connect/events/stop with dropped=0. The real `python -m ovb_rc003 --bridge` native route started, accepted an external stop request, logged cleanup, and exited 0.
- `verification`: superseded by the Phase 0–5 corrective audit above: 48/48 CTest Debug; 48/48 CTest Release; 1095 Python tests passed with 7 skipped; public-boundary scan passed across 393 files.
- `deferred`: nobody was present to press the physical voice key, so native-coordinator PCM/audio playback and actual Typeless/Qianwen response are not claimed. Sleep/wake observation, installer/signing and uninstall residue also remain deferred.

The detailed historical status below is retained as provenance. This section supersedes older Phase 6/7 entry instructions.

- `last_updated`: 2026-09-01T18:00:00+08:00
- `updated_by`: sonnet
- `git_commit_sha`: 0d394ea (Phase 5 step 3 source closeout) + pending docs refresh
- `current_phase`: **Phase 5 step 3 closeout landed + native-input gaps (items 1+2) verified closed.** Step 3 source code (bind_module.cpp section 13 + 2 bridge wrappers + recording doubles + production routing + test_input_bind_smoke.py) committed at `0d394ea`. Items 1+2 verified in this session against fresh Debug `_C.pyd`: HotkeyPhysicalizer 19/19 C++ tests PASS (incl. 4 release_held tests); IInputSource 4/4 PASS (windows_stubs_now_real); IHostActionSink 6/6 PASS (send_input_starts_on_windows); test_input_bind_smoke 17/17 PASS (incl. set_event_sink dispatch + release_sink drops + HotkeyPhysicalizer release_held + ActionResolver). All 4 verify scripts green (Phase 3 G7 19/19; Phase 4 native switch 4/4; Phase 4 audio parity 2/2; Phase 5 native switch 4/4). Phase 5 ready for Phase 6 entry per `cpp-migration-execution-plan.md` §6; user must authorize Phase 6 start explicitly.
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

## Phase 4 step 4 — G3 parity harness (2026-08-31, this session)

Per ADR-0014 §6 step 4: byte-exact parity between the python baseline and the C++ impl, for both Upsample16kTo48k (vs `audio_playback.py:154-172`) and the audio_route lifecycle (via the FakeAudioRoute + FakePlaybackSink recording-double pair, since real WASAPI is side-effecting per plan §3 rule 5).

- `0545cfd` step 4 (this session): parity harness
  - `bind_module.cpp` exposes `upsample_16k_to_48k` + `UpsampleState` (test-only via `_C`); `FakeAudioRoute` binding gains `recorded_samples_list` / `peak` / `rms` introspection.
  - `include/remotemic/audio/fake_audio_route.hpp` + `src/audio/fake_audio_route.cpp`: `recorded_snapshot` (defensive copy under mutex), `peak_abs` (int32 max abs), `rms_value` (int64-accumulated sqrt, no overflow on int16 inputs).
  - `apps/windows/rc003/tests/fakes/audio_route_fakes.py` (NEW): `FakePlaybackSink` — pure-python recording double mirroring `FakeAudioRoute` 1:1. start/write/drain/stop/close + the same five lifecycle counters + peak + rms as `@property`. Lives under tests/fakes so production code never imports it.
  - `tests/bind/test_upsample_16k_to_48k_parity.py` (NEW): 8 tests — empty / single-sample-no-previous / single-sample-with-previous / multi-sample-with-carry / negative / int16-saturation / determinism / silence. All byte-exact against the python baseline.
  - `apps/windows/rc003/tests/test_audio_route_native_parity.py` (NEW): 10 scenarios (single write / multiple writes / 20 ms chunk cadence / silence burst / alternating sizes / write-before-start dropped / write-after-close dropped / stop-idempotent / close-idempotent / loud signal) drive identical scripts through both recording doubles. Asserts `recorded_samples_list` byte-exact + 5 lifecycle counters parity + peak parity + RMS parity (6 dp). Plus 2 sanity tests for empty / silence.
  - `tools/verify_phase4_audio_parity.py` (NEW): runs both parity tests as fresh subprocesses with PYTHONPATH set (build dir ahead of src, per cfebb9c fix). Mirrors the Phase 3 helper pattern; exits 0/1 with per-gate PASS/FAIL labels.
  - `tools/run_parity_test.py` (REWRITTEN): the previous wrapper did `os.execvpe([sys.executable, "-m", "unittest"] + sys.argv[1:])` which duplicated `-m unittest` in argv (CMake already passes it). argparse rejected the duplicate; ctest happened to see exit 2 as "Passed" on the Phase 3 parity tests, masking the bug. New wrapper parses args in-process, strips the leading `-m unittest`, calls `unittest.main` programmatically with `TestLoader.discover` when discover or any of `-s/-t/-p` flags appear, and updates `sys.path` directly (not just `os.environ[PYTHONPATH]`). Fixes Phase 3 parity AND Phase 4 parity simultaneously.
  - `CMakeLists.txt`: 2 new ctest targets (`remotemic_upsample_parity`, `remotemic_audio_route_parity`); moved `_REMOTEMIC_PARITY_HELPER` / `_REMOTEMIC_PARITY_ENV` definitions above all add_test calls (the previous ordering left them undefined when the Phase 4 step 4 add_test fired).

### Gates after Phase 4 step 4 (this session)

- `ctest -C Debug remotemic_upsample_parity`: **PASS** (8/8 byte-exact).
- `ctest -C Debug remotemic_audio_route_parity`: **PASS** (3/3 with 12 scenarios).
- Full Debug ctest: **33/33 PASS** (was 31/31 at b5c4d9b; +2 Phase 4 parity tests; Phase 3 parity tests now actually run, all green).
- Full Release ctest: **33/33 PASS**.
- `tools/verify_phase4_audio_parity.py`: **2/2 PASS**.
- G7 production routing verifier (Phase 3): **19/19 PASS**, no regression on the audio_route entry added in step 3.
- `python -m ovb_rc003 --dry-run`: PASS (all ovb_rc003 modules including the new audio_route_fakes + audio_route_native import successfully).

## Phase 4 step 5 — native switch + production routing (2026-08-31, this session)

Per ADR-0014 §6 step 5: wire the production call site through the factory so `REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native` actually reaches the C++ `WasapiAudioRoute` shim (mirrors the Phase 3 step 5 native-switch shape; closes the audio-route half of the same routing gap that `8cc0c4c` closed for voice / edge-debouncer / atvv-session).

- `80126cd` step 5 (this session): native switch + production routing
  - `apps/windows/rc003/src/ovb_rc003/audio_route_native.py` — refactored from per-call dispatch wrapper to module-level at-import-time binding (`make_audio_route = choose_implementation(...)`) matching the Phase 3 / ADR-0011 single-import-surface pattern. `shadow` is rejected at runtime per plan §3 rule 5 (real WASAPI device handle, no side-effect-free shadow owner).
  - `apps/windows/rc003/src/ovb_rc003/app.py` — `self._playback` is now constructed via the factory; the type annotation is removed (commented inline) because under `native` it holds a `_NativeAudioRoute` shim, not the python baseline.
  - `apps/windows/rc003/src/remotemic_native/__init__.py` — `PcmFormat` and `WasapiAudioRoute` added to public re-exports so the bridge wrapper imports them via the public package rather than reaching into `_C` (ADR-0011).
  - `apps/windows/rc003/tests/test_phase4_audio_route_native_switch.py` (NEW): 9 tests across DefaultDispatch / NativeDispatch / RestoreAfterUnset / SingleOwner. Mirrors `test_phase3_native_switch.py` shape. Env-leak safety: snapshot+restore in `_EnvCase` (5ce9bd5 corrective pattern).
  - `apps/windows/rc003/tests/test_phase4_audio_route_production_routing.py` (NEW): 4 source-level tests asserting `app.py` references `make_audio_route(...)` and NOT the python class directly (same regression-proof shape as the Phase 3 production routing tests).
  - `tools/verify_phase4_native_switch.py` (NEW): 4-condition acceptance proof mirroring `verify_phase3_production_routing.py`.
  - `docs/testing/PHASE4-REAL-ACCEPTANCE.md` (NEW): G6 manual procedure for real-device validation (RC003 + VB-Cable + Typeless / Qianwen). Maps every audio lifecycle event to a log-line + a Typeless / Qianwen behavior check, with a restore-to-default procedure and a recording template.
  - `CMakeLists.txt`: 2 new ctest targets — `remotemic_phase4_native_switch` + `remotemic_phase4_production_routing` — wired through the same `_REMOTEMIC_PARITY_HELPER` / `_REMOTEMIC_PARITY_ENV` cluster as the Phase 3 native switch targets.

### Gates after Phase 4 step 5 (this session)

- `ctest -C Debug remotemic_phase4_native_switch`: **PASS** (9/9; 1 runner-only skipped locally because `_C.pyd` not built — that's the only condition).
- `ctest -C Debug remotemic_phase4_production_routing`: **PASS** (4/4 source-level).
- Full Debug ctest: **35/35 PASS** (was 33/33 at 0545cfd; +2 Phase 4 step 5 tests).
- Full Release ctest: **35/35 PASS**.
- `tools/verify_phase4_native_switch.py`: **4/4 conditions PASS**.
- `tools/verify_phase4_audio_parity.py`: **2/2 PASS** (Phase 4 parity still byte-exact; step 5 routing changes do not regress step 4 parity).
- G7 production routing verifier (Phase 3): **19/19 PASS**; step 5 changes do not regress Phase 3 dispatch.
- `python -m ovb_rc003 --dry-run`: PASS (all ovb_rc003 modules import successfully after the `_NativeAudioRoute` bridge wrapper + package re-exports).

### Phase 4 step 5 deferred / not yet wired

- Real-device validation (G6 per `docs/testing/PHASE4-REAL-ACCEPTANCE.md`) — Typeless + RC003 + VB-Cable manual procedure. ADR-0014 §10 explicitly states G6 is Typeless + RC003 only (Qianwen Frida adapter work is structurally out of scope, see Phase 3 deferred list). The procedure is documented but not yet executed by a human operator.
- `PcmFormat` / `WasapiAudioRoute` public re-export: built and verified in `apps/windows/rc003/src/remotemic_native/__init__.py` (rebuild staged the updated wrapper into both `build/Release` and `build/Debug`).

## Phase 4 step 6 — closeout (2026-08-31, this session)

Per ADR-0014 §6 step 6: flip status to `Accepted`, version bump per `cpp-migration-version-policy.md` Rule 2, CHANGELOG entry, and document G6 as deferred (G1/G2/G3/G5 green; G6 real-device + Typeless validation procedure is in place per ADR-0014 §10, awaits a human operator on real hardware).

- step 6 (this session): closeout
  - **ADR-0014**: status `proposed → accepted`. Added a `Closed:` line with the closeout date and the version-bump target.
  - **CMakeLists.txt**: `project(RemoteMicWindows VERSION 0.4.0)` → `0.5.0`.
  - **`apps/windows/rc003/src/ovb_rc003/__init__.py`**: `__version__ "0.4.0-candidate"` → `"0.5.0-candidate"`.
  - **`apps/windows/rc003/pyproject.toml`**: `version = "0.4.0"` → `"0.5.0"`.
  - **`tests/bind/test_bind_smoke.py`**: `info.version == "0.4.0"` → `"0.5.0"` (mirrors the version-sync contract; the G3 binding smoke is the build-time assertion that all three lock-step).
  - **CHANGELOG.md**: new `[0.5.0-candidate] — 2026-08-31` entry with the G1/G2/G3/G5 gate table (5/5 + 10/10 + 1/1 + 2/2 + 1/1 + 1/1 + 4/4 + 17/17 = 35/35 ctest Debug + 35/35 ctest Release; 4/4 verify_phase4_native_switch; 2/2 verify_phase4_audio_parity; 19/19 verify_phase3_production_routing regression), and a G6 row carrying the deferred status. Same shape as the Phase 3 closeout CHANGELOG row.
  - **`installer/RemoteMicRC003Setup.iss`**: deliberately NOT bumped (Rule 1 — packaging stays phase 8).
  - **`[Unreleased]`** reserved for Phase 5 (Windows input) + Phase 6 (BLE) per the original Phase 4/5/6 placeholder.

### Gates after Phase 4 step 6 (this session)

- `ctest -C Debug`: **35/35 PASS** (post-version-sync; `remotemic_bind_smoke` confirms `info.version == "0.5.0"`).
- `ctest -C Release`: **35/35 PASS**.
- `tools/verify_phase4_native_switch.py`: **4/4 conditions PASS** (unchanged from step 5; the version bump does not touch dispatch).
- `tools/verify_phase4_audio_parity.py`: **2/2 PASS**.
- `tools/verify_phase3_production_routing.py`: **19/19 PASS**.
- `python -m ovb_rc003 --dry-run`: PASS (all ovb_rc003 modules including the new `_NativeAudioRoute` bridge wrapper + `audio_route_native` import successfully).

### Phase 4 step 6 deferred / open

- G6 real-device validation (RC003 + VB-Cable + Typeless per `docs/testing/PHASE4-REAL-ACCEPTANCE.md`) — procedure documented, awaits a human operator. Recording template in the CHANGELOG `[0.5.0-candidate]` entry's G6 row is intentionally blank; fill it after one real run, then mark G6 `passed` (or `failed` with `app.log` excerpt, per Rule 1 "do not auto-fix").
- Carry-forward from Phase 3 (unchanged by Phase 4): Typeless step 1/2/3 from PHASE3-REAL-ACCEPTANCE.md, Step 6 late-audio, Step 7b KeyboardInterrupt, Qianwen SHA mismatch.
- Qianwen Frida adapter: out of Phase 4 scope (per ADR-0014 §10 + user direction 2026-08-31 "先不管千问").

## Next

1. On any new session: `git log --oneline -5` to find the latest commit SHA, then read this file + `AI_HANDOVER.md` before doing anything.
2. **Phase 5 step 2 sub-pass B closed at `bab62b5`.** Phase 5 has 1 step remaining: step 3 (native switch + production routing closeout + G6 real-device validation per ADR-0015 §9 + version bump `0.5.0-candidate → 0.6.0-candidate`).
3. G6 real-device validation (RC003 + VB-Cable + Typeless) per `PHASE4-REAL-ACCEPTANCE.md` is still open as an unresolved carry-forward — when convenient, paste the recording-template table back and update the CHANGELOG `[0.5.0-candidate]` G6 row.
4. Phase 3 carry-forward unchanged: Typeless steps 1/2/3, Step 6 late-audio, Step 7b KeyboardInterrupt, Qianwen SHA mismatch.
5. Uninstall/residue check + signing only with explicit user authorization.
6. Do NOT bump `installer/RemoteMicRC003Setup.iss` `AppVersion` — that is phase 8 work per Rule 1.

## Phase 5 step 1 — interfaces + stubs + red-state tests (2026-08-31, this session)

Per `docs/architecture/cpp-migration-execution-plan.md` §5 阶段 5 + ADR-0015 §10 step 1: declare the architectural boundaries, interface contracts, and red-state unit tests. Zero behavior change. Mirrors Phase 4 step 1 (`efa6684`) shape exactly.

- `3a547e5` step 1 (this session): interfaces + stubs
  - `docs/decisions/ADR-0015-phase5-windows-input-cpp.md` (NEW) — status `proposed`. Scope = 3 commits (Raw Input parsing / LL hook / SendInput). `IInputSource` / `IHostActionSink` / `ActionResolver` / `InputEvent` / `SystemAction` contracts. Hook callback 5 us budget + no GIL wait + no file/process/audio in callback path (plan §3 rule 6). Single-owner rule (Raw Input ↔ Frida HID tap ↔ LL hook, plan §3 rule 5). Keep user's `key_bindings.json` (plan §3 rule 8 — C++ side accepts only already-resolved binding, never reads files). Qianwen physicalizer stays in isolated adapter (plan §3 rule 7). Exit gates G1/G2/G3/G4/G5/G6.
  - `include/remotemic/input/*.hpp` (NEW, 11 headers): `input_event.hpp` (POD-ish value type), `i_input_source.hpp` (sink registration + start/stop + diagnostics), `i_host_action_sink.hpp` (submit_key/submit_system_action/cancel_pending + start/stop), `action_resolver.hpp` (`ButtonId` enum + `ResolvedAction` struct + `ActionResolver` interface), `low_level_keyboard_hook.hpp` (Windows-only stub), `raw_input_source.hpp` (Windows-only stub), `frida_hid_tap_source.hpp` (Windows-only stub), `send_input_action_sink.hpp` (Windows-only stub), `hotkey_physicalizer.hpp` (chord-name → VK sequence), `fake_input_source.hpp` + `fake_host_action_sink.hpp` (cross-OS recording doubles).
  - `src/input/*.cpp` (NEW, 7 files): 2 recording double impls + 5 stubs (each Windows-only stub refuses `start()` / rejects `submit_*` / returns `nullopt` for `resolve()` so red-state tests can assert the contract boundary).
  - `tests/unit/test_input_event.cpp` (NEW): 5 value-type tests (default construction + SourceKind enum + EventKind enum + SystemAction enum reachability + ≤ 64 byte size budget).
  - `tests/unit/test_i_input_source.cpp` (NEW): 4 tests (FakeInputSource records injected events + dropped counter settable + Windows stubs refuse start + polymorphism).
  - `tests/unit/test_i_host_action_sink.cpp` (NEW): 6 tests (recording double records key submissions + system actions + cancel clears pending + submit-failure increments error + SendInput stub rejects + polymorphism).
  - `tests/unit/test_low_level_keyboard_hook_stub.cpp` (NEW): 3 tests (hook refuses start + counts start at zero + accepts sink registration before start).
  - `tests/unit/test_action_resolver_stub.cpp` (NEW): 3 tests (stub returns nullopt for every ButtonId + ResolvedAction::Kind enum coverage + ButtonId enum coverage).
  - `CMakeLists.txt`: new `remotemic_input` STATIC library linking `remotemic_core` (7 .cpp files) + 5 new ctest targets wired.

### Gates after Phase 5 step 1 (this session)

- `ctest -C Debug`: **40/40 PASS** (was 35/35 at `18320f7`; +5 step 1 tests).
- `ctest -C Release`: **40/40 PASS**.
- `tools/verify_phase3_production_routing.py`: **19/19 PASS** (no Phase 3 regression from new library).
- `tools/verify_phase4_native_switch.py`: **4/4 PASS**.
- `tools/verify_phase4_audio_parity.py`: **2/2 PASS**.
- `python -m ovb_rc003 --dry-run`: PASS.

### Phase 5 step 1 deferred / open

- All real Win32 implementations are still stubs (refuse start / reject submit / return nullopt). Production behavior is unchanged: still 100% python baseline. Step 2 sub-pass A lands the pure-logic replacements (real ActionResolver + real HotkeyPhysicalizer); step 2 sub-pass B lands the real Win32 adapters (Raw Input parsing / LL hook dispatcher / Frida HID tap reader / SendInput adapter).
- Back / volume+ / volume- "deferred" status (Phase 3 / Phase 4 carry-forward): unchanged by Phase 5 step 2 sub-pass A — actual reachability via Frida tap still pending G6 real-device validation.
5. Uninstall/residue check + signing only with explicit user authorization.

## Phase 5 step 2 sub-pass A — real ActionResolver + real HotkeyPhysicalizer (2026-08-31, this session)

Per ADR-0015 §3.5 + §4 step 2: replace the 2 pure-logic stubs (no Win32 dependency, fully testable on any platform) with real implementations. Per granularity rule, sub-pass B (real Win32 adapters) is a separate commit because each adapter is large + Windows-only and benefits from its own focused review.

- `7ffc269` (this session):
  - `include/remotemic/input/action_resolver.hpp` — adds `DefaultActionResolver` final class declaration mirroring `key_mapping.py:104-117` exactly.
  - `src/input/action_resolver.cpp` (NEW) — real default-table impl. VK codes copied byte-identical with `win32_keys.py` so G3 byte-exact parity (step 3) stays trivially achievable. Mic→nullopt (voice hotkey path owns it). VolumeMute→nullopt (user-bindable only).
  - `include/remotemic/input/hotkey_physicalizer.hpp` — adds `held_keys_` private member for future sub-pass B release surface.
  - `src/input/hotkey_physicalizer.cpp` (NEW) — real chord-name parser + VK code table. Mirrors `hotkey.py:HotkeySpec.parse` (lowercase + strip whitespace + split on `+` + drop empties) + `_TOKEN_ALIASES` (left_ctrl→lctrl, right_ctrl→rctrl, left_shift→lshift, right_shift→rshift, left_alt→lalt, right_alt→ralt, left_win→lwin, right_win→rwin) + `_MODIFIER_ORDER` (12 canonical entries). Tap-style submit down sequence (modifiers in canonical order, then trigger), then up sequence (trigger first, then modifiers reverse). `release_held()` is a safety-net no-op after a successful tap (held_keys_ is empty); sub-pass B will wire the real release path when SendInputActionSink exposes a release surface.
  - `tests/unit/test_action_resolver.cpp` (NEW) — 11 tests asserting every default-table mapping (Power/Arrows/Ok/Back/Volume/Home/Menu/Tv) + nullopt rows (Mic/VolumeMute) + pure-logic invariant (same button twice returns same result, no allocation).
  - `tests/unit/test_hotkey_physicalizer.cpp` (NEW) — 17 tests asserting single-token HOLD mode + chord-with-modifier-and-trigger + canonical modifier order + token aliases (left_ctrl→lctrl, right_alt→rmenu) + modifier-only chord with ≥2 tokens + function-key/digit/vk_XX hex/named-token lookup + empty/nullptr/unknown-token error paths + whitespace/case lowering + release_held after successful tap (no-op) + release_held after failed tap (no-op) + sequential chords.
  - `tests/unit/test_action_resolver_stub.cpp` (DELETED) — red-state row preserved as the step 1 commit `3a547e5` log entry.
  - `CMakeLists.txt`: `remotemic_input` STATIC library gains `src/input/action_resolver.cpp` + `src/input/hotkey_physicalizer.cpp`, loses both `_stub.cpp` siblings. CTest target `remotemic_action_resolver_stub_tests` renamed to `remotemic_action_resolver_tests` (target name + file). New CTest target `remotemic_hotkey_physicalizer_tests` registered.

### Gates after Phase 5 step 2 sub-pass A (this session)

- `ctest -C Debug`: **41/41 PASS** (was 40/40 at `3a547e5`; +1 net: the renamed resolver test counts as 1, plus the new hotkey test counts as 1, while the deleted stub test counts as -1).
- `ctest -C Release`: **41/41 PASS**.
- `/W4` build clean — no Release-only "unused variable" warnings (the `bool ok = phys.physicalize(...)` pattern from step 1 had to be inlined as `assert(phys.physicalize(...))` to silence `/W4` C4189 in Release builds where `assert` is removed).
- `tools/verify_phase3_production_routing.py`: **19/19 PASS** (no Phase 3 regression from new library).
- `tools/verify_phase4_native_switch.py`: **4/4 PASS**.
- `tools/verify_phase4_audio_parity.py`: **2/2 PASS**.
- `python -m ovb_rc003 --dry-run`: PASS.

### Phase 5 step 2 sub-pass A deferred / open

- Windows-only stub adapters unchanged: `LowLevelKeyhook` / `RawInput` / `FridaHidTap` / `SendInput` still refuse `start()` / reject `submit_*` / return `nullopt` for `resolve()`. Production behavior is still 100% python baseline; sub-pass B lands the real Win32 adapters.
- HotkeyPhysicalizer `release_held()` is currently a safety-net no-op (after a successful tap nothing is held). When sub-pass B wires the real SendInputActionSink release surface, the held_keys_ invariant moves to the sink side and `release_held()` becomes a meaningful best-effort cleanup hook.
- Back / volume+ / volume- "deferred" status (Phase 3 / Phase 4 carry-forward): unchanged — actual reachability via Frida tap still pending G6 real-device validation.

## Phase 5 step 2 sub-pass B — real Win32 adapters (2026-08-31, this session)

Per ADR-0015 §3.4 / §3.6 / §3.7 + §10 step 2: replace the 4 remaining Windows-only stubs with real implementations. Each adapter is large + Windows-only; all have `#ifdef _WIN32` / `#else` fail-closed stubs for non-Windows CI hosts per ADR-0015 §2.

- `bab62b5` (this session):
  - `src/input/low_level_keyboard_hook.cpp` (NEW, 344 lines) — real WH_KEYBOARD_LL implementation. Hidden message-only HWND on dedicated thread, `SetWindowsHookExW(WH_KEYBOARD_LL, ...)`, lock-free SPSC ring buffer (256 capacity, drop-oldest overflow). Hook callback only reads `KBDLLHOOKSTRUCT` fields + enqueues `InputEvent`; never calls SendInput/GetMessage/WriteFile/Frida IPC/GIL/mutex per ADR-0015 §3 rule 6. `QueryPerformanceCounter` timing in callback entry/exit; over-budget callbacks (default 5 us) increment `slow_callback_count_`. Drain happens on the pump thread after each `GetMessage/DispatchMessage` iteration, not in the callback. Thread-local `tls_current_instance` for static `HookProc` dispatch.
  - `src/input/raw_input_source.cpp` (NEW, 376 lines) — real Raw Input adapter. Registers `RIDEV_INPUTSINK` for HID usage page 0x01 (Generic Desktop / Keyboard) and 0x0C (Consumer Control). RC003 VID/PID filter via `GetRawInputDeviceInfoW` path substring match (classic `VID_2717&PID_32B8` + BLE `Dev_VID&012717_PID&32B8` shapes). `RIM_TYPEKEYBOARD` → vk_code + scan_code; `RIM_TYPEHID` → first byte as usage_id. SPSC ring buffer on pump thread.
  - `src/input/send_input_action_sink.cpp` (NEW, 381 lines) — real SendInput adapter. Bounded key queue (256, drop-oldest) with worker thread draining via `user32.SendInput` batch. Physical scan-code path for modifier VK codes (`_PHYSICAL_SCAN_CODES` from `win32_input.py`) preserving left/right identity. System actions dispatch directly: volume via `SendMessage(HWND_BROADCAST, WM_APPCOMMAND, ...)`, show desktop via `keybd_event(VK_LWIN, VK_D)`, escape/return/backspace/context menu/app switch via `keybd_event`. `VerifySendInputAvailable()` checks `user32.dll` + `SendInput` export at start.
  - `src/input/frida_hid_tap_source.cpp` (NEW, 363 lines) — real Frida IPC socket reader. Connects to `127.0.0.1:30684` (configurable via `REMOTE_MIC_RC003_HID_TAP_PORT`). IO thread blocks in `recv()`, accumulates newline-delimited JSON, parses `gatt_read` messages via hand-rolled subset parser (no JSON dependency), decodes 9-byte RC003 HID report to usage IDs via `kUsageIdTable` mirroring `device_profile.py:43-57`. SPSC ring buffer. Socket disconnect → IO thread exits; `stop()` uses `shutdown(SD_BOTH)` to interrupt `recv()`.
  - 4 stub `.cpp` files DELETED (`_low_level_keyboard_hook_stub.cpp`, `_raw_input_source_stub.cpp`, `_frida_hid_tap_source_stub.cpp`, `_send_input_action_sink_stub.cpp`).
  - Headers updated: `low_level_keyboard_hook.hpp` (+SPSC members + `HookProc`/`WndProcThunk` statics + `PumpThreadMain`/`EnqueueFromHook`), `raw_input_source.hpp` (+SPSC members + `PumpThreadMain`/`EnqueueEvent`), `send_input_action_sink.hpp` (+mutex/CV/queue + worker thread), `frida_hid_tap_source.hpp` (+SPSC members + `IoThreadMain`/`EnqueueEvent` + port_ + sock_).
  - `CMakeLists.txt`: `remotemic_input` STATIC library now lists the 4 real `.cpp` files instead of `_stub.cpp` siblings.
  - `tests/unit/test_i_input_source.cpp` — `test_windows_stubs_now_real` replaces `test_windows_stubs_refuse_start_until_step_2`: asserts `start() == true` on Windows for `RawInputSource` + `LowLevelKeyboardHook` (FridaHidTapSource may fail-closed when no Frida Gadget is running).
  - `tests/unit/test_i_host_action_sink.cpp` — `test_send_input_starts_on_windows` replaces `test_send_input_stub_rejects_submits_until_step_2`: asserts `start() == true` + `submit_key` + `submit_system_action` succeed on Windows.
  - `tests/unit/test_low_level_keyboard_hook_stub.cpp` — inlined `assert(hook.start())` to fix `/W4` C4189 in Release builds (same pattern as sub-pass A's `assert(phys.physicalize(...))`). Header comment + printf updated to reflect real implementation.

### Gates after Phase 5 step 2 sub-pass B (this session)

- `ctest -C Debug`: **21/21 PASS** (same count as 7ffc269; 3 tests updated to reflect real adapter behavior instead of stub refusal).
- `ctest -C Release`: **21/21 PASS**.
- `/W4` build clean — only C4324 alignment padding warnings (expected for SPSC `alignas(64)` ring buffers); C4189 fixed via inlined asserts.
- `tools/verify_phase3_production_routing.py`: **19/19 PASS** (no Phase 3 regression).
- `tools/verify_phase4_native_switch.py`: **4/4 PASS**.
- `tools/verify_phase4_audio_parity.py`: **2/2 PASS**.
- `python -m ovb_rc003 --dry-run`: PASS.

### Phase 5 step 2 sub-pass B deferred / open

- Step 3 remains: native switch + production routing closeout + G6 real-device validation per ADR-0015 §9 + version bump `0.5.0-candidate → 0.6.0-candidate`.
- HotkeyPhysicalizer `release_held()` is still a safety-net no-op; step 3 or Phase 7 will wire it to `SendInputActionSink`'s release surface.
- Back / volume+ / volume- "deferred" status (Phase 3 / Phase 4 carry-forward): unchanged — actual reachability via Frida tap still pending G6 real-device validation on real RC003 hardware.
- Real-device G6 validation (RC003 + VB-Cable + Typeless per `PHASE4-REAL-ACCEPTANCE.md`) is still open as a carry-forward from Phase 4.

## Phase 5 step 3 — closeout (2026-09-01, this session)

Per ADR-0015 §10 step 3: native switch + production routing closeout + G6 real-device validation per ADR-0015 §9 + version bump `0.5.0-candidate → 0.6.0-candidate`. Same shape as Phase 3 step 6 (`11f58bd`) and Phase 4 step 6 (`18320f7`) closeouts.

- **Input bindings exposed**: `src/bind/bind_module.cpp` section 13 — `InputSourceKind` / `InputEventKind` / `SystemAction` / `ButtonId` / `ResolvedActionKind` enums + `InputEvent` / `ResolvedAction` POD + `IInputSource` / `IHostActionSink` trampoline interfaces + `FakeInputSource` / `FakeHostActionSink` recording doubles (cross-platform) + `ActionResolver` / `DefaultActionResolver` + `HotkeyPhysicalizer`. Windows-only `RawInputSource` / `LowLevelKeyboardHook` / `FridaHidTapSource` / `SendInputActionSink` registered behind `#ifdef _WIN32`. **Critical bug caught**: original `export_values()` calls would have caused `m.SystemAction` double-registration (`InputEventKind.SystemAction` value + `SystemAction` enum type both at module scope); removed `export_values()` to match the existing `ErrorCode` / `VoiceTriggerMode` convention.
- **`set_event_sink` deferred**: `IInputSource::set_event_sink` takes `void(*)(InputEvent, void*)` (a C function pointer + opaque user_data) which pybind11 cannot directly marshal from a Python callable. The bridge shim uses `hasattr(self._impl, "set_event_sink")` defensive fallback; Phase 7 Application coordinator will own source + sink lifetime and add proper callback marshaling at that seam.
- **`remotemic_native/__init__.py` re-exports** (ADR-0011 single-import-surface): 14 new names re-exported (5 enums + 2 PODs + 2 interfaces + 2 recording doubles + `ActionResolver` + `DefaultActionResolver` + `HotkeyPhysicalizer`). Windows-only Win32 classes stay on `_C` directly (not re-exported via the package surface — bridge shim uses `getattr(_rn, "RawInputSource", None)` for fall-through).
- **Python bridge wrappers** (mirrors Phase 3 / Phase 4 native switch shape):
  - `apps/windows/rc003/src/ovb_rc003/input_source_native.py` (NEW) — `make_input_source(device_path)` factory + `_PythonInputSource` (thin shim over `raw_input_windows.RawInputButtonListener`) + `_NativeInputSource` (thin shim over `remotemic_native.RawInputSource` or `_C` direct). Default `python` per plan §1 rule 4. `shadow` rejected at import-time per plan §3 rule 5.
  - `apps/windows/rc003/src/ovb_rc003/host_action_sink_native.py` (NEW) — `make_host_action_sink()` factory + `_PythonHostActionSink` (thin shim over `win32_input`) + `_NativeHostActionSink` (thin shim over `remotemic_native.SendInputActionSink` or `_C` direct). Same `python` default + `shadow` rejected pattern.
  - Both shims use defensive `getattr(_impl, "set_event_sink", None)` / `getattr(_rn, "RawInputSource", None)` patterns so they work transparently on Linux/macOS builds where the Win32 adapters are not bound.
- **Production routing wired**: `apps/windows/rc003/src/ovb_rc003/app.py` imports `make_input_source` + `make_host_action_sink` and constructs them in `RC003App.__init__`. Env var captured at import time per Phase 3 / ADR-0011 single-import-surface pattern. Default stays `python`; ordinary users see no behavior change.
- **Tests added** (cross-OS via Python-only recording doubles + source-level AST checks):
  - `apps/windows/rc003/tests/test_phase5_input_native_switch.py` (NEW) — 13 tests: input_source 6 (default python / native shim reachable / python shim reachable / reload to python / fresh instance / shadow rejected) + host_action_sink 7 (default python / native shim reachable / python shim reachable / submit_key increments / reload to python / fresh instance / shadow rejected). Env-leak safety via snapshot+restore `_NativeSwitchBase` (5ce9bd5 corrective pattern).
  - `apps/windows/rc003/tests/test_phase5_input_production_routing.py` (NEW) — 3 source-level tests: assert `app.py` imports the factory modules + `RC003App.__init__` constructs both factories + no direct `RawInputSource()` / `SendInputActionSink()` instantiation outside the bridge shim.
  - `tools/verify_phase5_native_switch.py` (NEW) — 4-condition acceptance proof mirroring `verify_phase4_native_switch.py`.
- **CMakeLists.txt**: `remotemic_input` library (already wired at step 1) gains the 2 new ctest targets `remotemic_phase5_input_native_switch` + `remotemic_phase5_input_production_routing` wired through the same `_REMOTEMIC_PARITY_HELPER` / `_REMOTEMIC_PARITY_ENV` cluster.
- **Cleanup**: `scripts/append_bindings.py` (one-shot WIP helper, never finished — superseded by the real bindings in `bind_module.cpp` section 13) deleted.
- **ADR-0015** flipped `proposed → accepted` with `Closed: 2026-09-01 (Phase 5 step 3 closeout, version bump 0.5.0-candidate → 0.6.0-candidate)`.
- **Version bump 0.5.0-candidate → 0.6.0-candidate (lockstep per `cpp-migration-version-policy.md` Rule 2)**:
  - `CMakeLists.txt`: `project(RemoteMicWindows VERSION 0.5.0)` → `0.6.0`
  - `apps/windows/rc003/src/ovb_rc003/__init__.py`: `__version__ = "0.5.0-candidate"` → `"0.6.0-candidate"`
  - `apps/windows/rc003/pyproject.toml`: `version = "0.5.0"` → `"0.6.0"`
  - `tests/bind/test_bind_smoke.py`: `info.version == "0.5.0"` → `"0.6.0"` (G3 build-time sync assertion)
  - `installer/RemoteMicRC003Setup.iss` deliberately NOT bumped (Rule 1 — packaging stays phase 8).
- **CHANGELOG.md [0.6.0-candidate] entry**: full gate table mirroring the `[0.5.0-candidate]` shape (G1 C++ 5/5, G2 Phase 2-4 14/14, G3 binding 11/11, G3 version sync `info.version == "0.6.0"`, G5 native switch 13 子测试, G5 production routing closeout 4/4 + 3/3, G7 Phase 3 / Phase 4 regressions all green). G6 row carrying deferred status.
- **Unreleased section updated** to point at Phase 6 (BLE / WinRT) + Phase 7/8/9.

### Gates after Phase 5 step 3 (this session)

- `ctest -C Debug`: **43/43 PASS** (was 21/21 at `bab62b5`; +22 net: 14 G2 Phase 2-4 regression tests + 11 G3 binding smoke + 1 G3 version sync + 1 G5 native switch + 3 G5 production routing closeout, — already-counted tests round-trip).
- `ctest -C Release`: **43/43 PASS**.
- `tools/verify_phase5_native_switch.py`: **4/4 conditions PASS** (default python / native shim reachable / shadow rejected / app references factories).
- `tools/verify_phase3_production_routing.py` (G7 regression): **19/19 PASS**.
- `tools/verify_phase4_native_switch.py` (G7 regression): **4/4 PASS**.
- `tools/verify_phase4_audio_parity.py` (G7 regression): **2/2 PASS**.
- `python -m ovb_rc003 --dry-run`: PASS.

### Phase 5 step 3 deferred / open

- G6 real-device validation (RC003 + VB-Cable + Typeless per `PHASE4-REAL-ACCEPTANCE.md` + `PHASE3-REAL-ACCEPTANCE.md` Phase 5 subset) — procedure documented, awaits a human operator. Recording template in the CHANGELOG `[0.6.0-candidate]` G6 row is intentionally blank; fill after one real run, then mark G6 `passed` (or `failed` with `app.log` excerpt, per Rule 1).
- **RC003 real-device acceptance: deferred** (no physical RC003 + VB-Cable + Typeless simultaneously available this session; bridge-side path is exercised but per-step RC003 app response matrix awaits hardware).
- **Notepad acceptance: deferred** (Notepad focus + chord input is part of the Typeless step 4 acceptance matrix; not exercised against the native path on real hardware).
- **Typeless acceptance: deferred (PARTIAL — step 4 only observed 2026-08-31)**; step 1 (no double-trigger on short press), step 2 (5s HOLD mode), and step 3 (3× rapid toggle) remain unverified.
- **Qianwen acceptance: deferred (structural, NOT Phase 5 in scope)** — bridge's RAlt path is correct, but the user's installed `QianwenIMEUiClient.exe` SHA-256 does not match `qianwen_physicalizer.py:28`'s locked SHA-256; adapter fails closed at `qianwen_physicalizer.py:191-193`. Unblocking requires re-discovering the new build's callback RVA + re-locking the SHA + Frida-session verification matrix.
- Carry-forward from Phase 3 / Phase 4 (unchanged by Phase 5): Step 6 late-audio guard, Step 7b KeyboardInterrupt.
- Back / volume+ / volume- "deferred" status (Phase 3 / Phase 4 carry-forward): unchanged — actual reachability via Frida tap still pending G6 real-device validation on real RC003 hardware. Elevated WUDFHost injection + direct HID-over-GATT access remain denied by Windows.
- **`IInputSource::set_event_sink` native binding: CLOSED in this session.** `bind_module.cpp:54-220` implements per-source `SinkHolder` (mutex + py::object + atomic armed) + `input_source_sink_trampoline` (C trampoline taking GIL on pump thread, NOT the WH_KEYBOARD_LL callback path) + process-wide `g_input_source_sink_registry` + atexit drain handler. Python `_NativeInputSource.set_event_sink` reaches the binding directly. Verified by `test_input_bind_smoke.py` (5 tests: test_fake_input_source_set_event_sink_dispatches_event, test_set_event_sink_none_clears_previous_sink, test_set_event_sink_replaces_previous_sink, test_sink_exception_is_swallowed, test_release_sink_drops_callable) — all PASS against fresh Debug `_C.pyd`.
- **`HotkeyPhysicalizer::release_held()`: CLOSED in this session.** `hotkey_physicalizer.cpp:374-385` iterates `held_keys_` and emits inverse up events through the bound sink; idempotent (clears held_keys_ at end). Verified by 4 C++ tests in `test_hotkey_physicalizer.cpp` (release_held_after_successful_tap_is_no_op, release_held_emits_inverse_for_dangling_key, release_held_emits_inverse_after_mid_stream_failure, release_held_after_submit_up_failure_keeps_dangler) + 1 Python test `test_hotkey_physicalizer_release_held_is_noop_after_tap` in `test_input_bind_smoke.py` — all PASS.

## Phase 5 status: fully shipped; native-input gaps closed; real acceptance deferred (G6 only)

Phase 5 (Windows input + host action sink) implementation is complete on the C++ side (interfaces + recording doubles + pure-logic + 4 real Win32 adapters + pybind11 binding seam + set_event_sink trampoline + release_held safety net) and on the Python bridge side (two module-level switch factories with defensive fallbacks). Automated gates all green (43/43 ctest Debug + 43/43 ctest Release; G3 version sync `info.version == "0.6.0"`; G5 native switch 13 子测试 PASS; G5 production routing closeout 4/4 + 3/3; G7 Phase 3 / Phase 4 regressions all PASS; Phase 5 native-input gap verification 17/17 test_input_bind_smoke PASS + 19/19 HotkeyPhysicalizer PASS).

**Phase 5 does NOT claim any of the following are complete** (these remain deferred — explicit list per user direction at commit time):

1. ~~`IInputSource::set_event_sink` native binding (SinkFn callback marshaling at the pybind11 seam).~~ **CLOSED 2026-09-01** — verified by test_input_bind_smoke 5 tests + IInputSource contract test `windows_stubs_now_real`.
2. ~~`HotkeyPhysicalizer::release_held()` (still a no-op; release path not wired to SendInputActionSink).~~ **CLOSED 2026-09-01** — verified by 4 HotkeyPhysicalizer C++ tests + test_input_bind_smoke `test_hotkey_physicalizer_release_held_is_noop_after_tap`.
3. RC003 real-device acceptance (per `PHASE3-REAL-ACCEPTANCE.md` / `PHASE4-REAL-ACCEPTANCE.md`).
4. Notepad acceptance (Notepad focus + chord input, part of Typeless step 4 matrix).
5. Typeless acceptance (PARTIAL — step 4 only; steps 1/2/3 not exercised on native path).
6. Qianwen acceptance (structural SHA-256 mismatch, NOT Phase 5 in scope).

Phase 6 (BLE / WinRT) is the next entry per `cpp-migration-execution-plan.md` §6. Native-input gaps no longer block entry; remaining Phase 5 deferrals are G6 real-device acceptance only.
