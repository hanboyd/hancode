# AI Handover

```yaml
last_updated: 2026-08-23T18:30:03+08:00
agent: codex (GPT-5)
provider: openai
model: GPT-5
git_commit_sha: a32a345 plus uncommitted ADR-0006 through ADR-0009 implementation
current_phase: Phase 3 real-device validation, primary Typeless and Qianwen paths usable
current_task: In-app preset switching is manually accepted. OpenDesign has been ported into native QML through reusable design primitives and one-to-one page layouts without touching bridge/audio/shortcut logic. Independent screenshot comparison passed; one user manual latest-source inspection remains, and foreground-commit reliability stays explicitly deferred.
deadline: two-day delivery window
hardware_validation:
  status: partial
  details: BLE and ATVV voice passed; Typeless TAP/audio/TAP and RC003 -> VB-CABLE transcription passed by user observation (latest correlated signal 1573 frames / 23.595 seconds); editing and Notepad F5-leak checks passed; Qianwen isolated ralt TAP/TAP window lifecycle passed with 495 frames / 7.425 seconds signal; 9/12 ordinary keys passed; back/volume blocked by Windows HID boundary
completed:
  - Ported OpenDesign to native QML instead of embedding HTML/WebView; shared card/status/divider primitives now carry the design while existing controllers and runtime logic remain authoritative
  - Rebuilt connection and mapping pages to the design hierarchy, retained DJI/RC003 conditional behavior, and corrected false-positive state colors, card overflow, field truncation, dense record controls and raw English diagnostics
  - Rendered all five pages through a Windows-native screenshot harness and passed independent final visual review with no blocking differences
  - Added a verified latest-source launcher that refuses an already-open stale settings window and never loads installed/dist/package code
  - Passed 196 focused UI/statistics/layout/diagnostics tests with 2 environment skips after the final visual corrections
  - User manually accepted in-app Typeless/Qianwen switching; ordinary preset changes no longer require agent-side bridge management
  - Added a current-effective-preset field that updates only after LaunchOutcome.STARTED and retains the previous confirmed value after a failed switch
  - Added exact-name SetupAPI RC003 battery reading without exposing a device path or Bluetooth identifier; the real device node returned 100%
  - Reworked the left device card from stale “设备状态/仍需真机验收” copy to “连接概况”, system recognition, and battery
  - Passed 245 focused battery/diagnostics/settings/bridge tests with 2 environment skips in the project Python 3.11 environment
  - Diagnosed the no-op preset switch: settings saved correctly, but launch-only behavior hit the bridge single-instance guard and left the old process using its startup config
  - Implemented ADR-0009 private Windows stop event and fail-closed save-stop-wait-relaunch orchestration; no generic process enumeration or Python termination
  - Passed 219 relevant tests with 1 environment skip and a real Windows stop-event create/discover/signal/wait/release round trip
  - Added ADR-0008 continuous blocking virtual-audio output: one prepared writer thread emits 20 ms audio/silence chunks, uses a bounded two-second queue, and drains before the closing host shortcut
  - Passed 82 focused tests with 1 environment skip and a clean six-burst CABLE Output check with peak 0.244141
  - User accepted Qianwen as usable after six numbered real-device attempts: 6/6 recognized and transcribed; 4/6 inserted directly into Notepad; attempts 1 and 3 required manual paste
  - User completed six numbered Typeless attempts: 6/6 appeared in Typeless history and six matching bridge sessions logged signal; Notepad visibly contained attempts 1, 3 and 5
  - User deferred the shared Typeless/Qianwen foreground-commit issue; no clipboard fallback, forced paste, focus stealing or close-timing change is authorized
  - Accepted ADR-0010: every future installer must upgrade in place without asking the user to uninstall/delete the old version; keep the stable AppId, replace only the dedicated program payload, preserve configuration/key mappings/statistics/logs, and retain one installed-app entry
  - Packaged the first usable unsigned installer and portable ZIP from the current source; final regression passed 1,041 tests with 7 skips, public-boundary scan passed 235 files, fresh portable extraction passed dry-run, and artifact hashes match their sidecars
  - Added repository governance and Git exclusions
  - Measured opening delivery at 2.070 seconds from first AUDIO_STARTED and 2.003 seconds from Windows F5 in the accepted run; WASAPI playback open accounted for the interval and the opening TAP followed in the same millisecond
  - Removed the 60 ms global-hook stall for default arrows/Enter even when direct HID injection is unavailable: RC003 uses the one native Windows edge, custom mappings retain correlation, F5 is swallowed before the wait, and direct HID still uses exact zero-wait armed suppression
  - Added manual presets with TOGGLE protocols: Typeless uses lctrl+lalt; Qianwen uses ralt plus a verified target-local callback physicalizer
  - Corrected the Qianwen callback from a mismatched shipped-PDB address to the exact installed EXE's SetWindowsHookExW WH_KEYBOARD_LL callback at RVA 0x85684; startup waits for post-intercept readiness and fails closed on Frida script errors
  - User accepted the Qianwen remote-button window lifecycle: one marked ralt TAP opened the overlay on press, one marked ralt TAP closed it on release, and the matching RC003 stream carried 495 frames / 7.425 seconds of signal; Typeless remained on its separate lctrl+lalt path
  - Rebuilt and overwrite-installed the current preset UI/adapter candidate; 1,014 tests passed with 7 environment skips, frozen and installed dry-runs exit 0, and installed/build executable SHA-256 values match exactly
  - Corrected earlier target-audio evidence after learning it used the headset microphone, then passed a new user-driven Typeless run with CABLE Output selected and correlated 1573 RC003 frames / 23.595 seconds of signal
  - Added C++20/CMake core, diagnostic CLI, logger, runtime paths, and unit tests
  - Built successfully with MSVC 19.44 and Windows SDK 10.0.26100.0
  - Passed CTest 1/1 and CLI version/diagnostic checks
  - Imported pinned Windows snapshot 271ed794 with license and source provenance
  - Installed every pinned Python runtime/dev dependency in an ignored local environment
  - Added a synthetic ATVV/ADPCM golden fixture
  - Passed 941 baseline tests with 7 live/system skips and zero failures
  - Passed the public-boundary scan after packaging changes
  - Built the PyInstaller directory candidate and passed frozen executable dry-run
  - Built the unsigned portable ZIP and passed a fresh-extraction dry-run
  - Compiled the intentionally unsigned Inno Setup installer with Inno Setup 6.7.3
  - Recorded candidate sizes and SHA-256 hashes in docs/baseline/CANDIDATE-ARTIFACTS.md
  - Installed and exercised the earlier unsigned candidate through the real installer UI
  - Fixed settings-window single-instance behavior and verified repeated launches leave one window
  - Extracted a reusable Mac-derived product design system under opendesign/design-systems/remote-mic-product
  - Passed independent 900x680 OpenDesign visual verification
  - Reworked QML with a narrow side rail, page titles, larger Chinese type, RC003 image, and two-column connection layout
  - Inspected all four source-rendered pages and launched the rebuilt frozen GUI visibly
  - Reconfirmed the complete baseline after the UI change
  - Rebuilt and hashed the unsigned portable and installer candidates
  - Passed a fresh portable extraction dry-run and frozen settings single-instance check
  - Overwrite-installed the rebuilt UI candidate through the real Inno Setup flow
  - Verified installed and rebuilt executable SHA-256 values are identical
  - Verified the installed redesigned UI and one-window/one-process settings behavior
  - Added one shared multi-size microphone product icon for Qt, executable, taskbar, shortcuts and installer
  - Deduplicated playback devices across Windows host APIs, hid default aliases and backend jargon, and preferred WASAPI
  - Rebuilt the frozen executable, portable ZIP and Inno installer after the refinement
  - Unified enabled UI text on the Windows foreground color and removed diagnostic text overlap
  - Connected one real RC003 and passed ATVV capabilities, control and aggregate PCM signal validation
  - Verified direction, OK, Home, Menu, TV and Power press/release events on real hardware
  - Installed official hash-pinned VB-CABLE Basic Pack45 and observed healthy playback/recording endpoints before reboot
  - Rechecked both VB-CABLE endpoints after reboot and passed the project diagnostic
  - Fixed duplicate-backend handling in the diagnostics auto-select action and passed 78 focused tests
  - Persisted and exactly resolved the live WASAPI CABLE Input endpoint
  - Passed an in-memory synthetic CABLE Input to CABLE Output loopback without saving audio
  - Visually confirmed the source UI text color and diagnostics layout fixes on Windows
  - Configured Typeless microphone to CABLE Output and confirmed the selected label
  - Started the source bridge without rebuilding; live RC003 discovery and 16 kHz/frame-size-120 capabilities passed
  - Recorded 9.36 seconds of RC003 signal through VB-CABLE, but user rejected the first two-complete-tap interaction as still conflicting
  - Reviewed macOS upstream revision 1796b149: custom voice chords use HID KeyDown/KeyUp, while ATVV controls are separately debounced
  - Reworked Typeless integration to HOLD lctrl+lalt: KeyDown on physical press, retain through intermediate AUDIO_STOP, KeyUp on physical release
  - Passed 111 focused app/voice/ATVV/BLE tests for the Mac-style held-chord path
  - Proved from the live log that one hold produced ATVV AUDIO_STARTED, translated F5 with many repeat key-downs, and Raw Input duplicates
  - Made direct HID the authoritative microphone edge when available and swallowed F5 the fallback owner; disabled Raw Input mic dispatch and HOLD-mode ATVV shortcut injection
  - Passed 129 focused tests including single-owner direct-HID and F5-fallback sequences, then restarted the source bridge without rebuilding a package
  - Observed one real post-change hold with exactly one logical F5 trigger, 462 PCM frames / 6.93 seconds signal, deferred AUDIO_STOP while held, and a later physical release closing the shortcut
  - Stopped the source bridge for a clean OpenCode/MiniMax M3 handover; no package or installer was rebuilt
  - Imported the entire `remote_mic_han/` tree as the monorepo's first clean baseline at commit `2906b38 chore: import remote_mic_han Phase 0/1/2 initial baseline` (195 files, 39 856 insertions); .gitignore covers `/.claude/` and the previously-leaking `apps/windows/rc003/artifacts/`
  - Implemented ADR-0003 Fix A in `src/ovb_rc003/app.py` (lock-free LL-hook → `voice-edge-worker` queue, Fix B release debounce wired into the worker, Fix C drain before closing host edge); commit `f3db758 feat(adr-0003): widen voice release debounce to 200 ms and make it configurable`
  - Pinned the production 200 ms window in three independent surfaces (`voice_edge_debouncer.VoiceEdgeDebouncer(release_window_seconds=0.200)`, the application-side read of `voice_release_debounce_seconds`, and `_normalize_voice_release_debounce`'s fallback); 11 new unit tests cover config clamping, config round-trip and 50/100/200/350 ms configurable-window behaviour
  - Updated `docs/ai_context/CURRENT_STATUS.md` and this handover to reflect that ADR-0003 is implemented and the source bridge has been stopped for clean handover; real-device acceptance against commit `f3db758` is the next step, not code
tests_run:
  - command: scripts/test-baseline.ps1 after ADR-0010 and packaging-script compatibility changes
    result: passed (1,041 tests, 7 environment skips); frozen dry-run also passed
  - command: apps/windows/rc003/build/check-public-boundary.ps1
    result: passed (235 files scanned)
  - command: Inno Setup 6.7.3 compile plus scripts/package-baseline-portable.ps1 and scripts/package-baseline-installer.ps1
    result: passed; fresh portable extraction dry-run passed, hashes matched sidecars, installer signature status NotSigned
  - command: PYTHONPATH=apps/windows/rc003/src .venv/Scripts/python.exe -m unittest apps.windows.rc003.tests.test_qt_settings_app apps.windows.rc003.tests.test_usage_statistics apps.windows.rc003.tests.test_remote_layout apps.windows.rc003.tests.test_windows_diagnostics
    result: passed (196 tests, 2 environment skips); final QML/controller mapping, DJI branch, screenshot-load contract, statistics projection, RC003 layout and localized diagnostics
  - command: apps/windows/rc003/tools/render_settings_pages.py --output artifacts/ui-review-final plus independent OpenDesign comparison
    result: passed; all five Windows-native screenshots rendered and final review found no blocking visual differences
  - command: apps/windows/rc003/tools/run_latest_source_settings.ps1 -VerifyOnly
    result: passed; resolves only current checkout source and its project virtual environment, excluding installed/dist/package code
  - command: PYTHONPATH=src .venv/Scripts/python.exe -m unittest tests.test_rc003_battery_windows tests.test_windows_diagnostics tests.test_qt_settings_app tests.test_settings_ui_helpers tests.test_bridge_launcher
    result: passed (245 tests, 2 environment skips); current-effective preset truthfulness, SetupAPI battery reading, diagnostics projection, QML load and bridge-switch regressions
  - command: PYTHONPATH=src .venv/Scripts/python.exe -c "from ovb_rc003.rc003_battery_windows import read_rc003_battery_percent; print(read_rc003_battery_percent())"
    result: passed; real Windows RC003 device node returned 100 without exposing its device path or Bluetooth identifier
  - command: build/build-candidate.ps1 -SkipDependencyInstall with ignored historical artifacts and the standalone administrator HID probe temporarily isolated, then restored unchanged
    result: passed; public-boundary scan 344 files, 1,014 tests with 7 skips, PyInstaller build and frozen dry-run
  - command: Inno Setup 6.7.3 compile plus silent overwrite install and installed --dry-run
    result: passed; installer exit 0, installed executable matched frozen SHA-256 38518E226EC0C0738D0884AFACACAC92F18034EFCA5F409B02D7D036B86258F5, no bridge auto-started
  - command: PYTHONPATH=src python -m unittest tests.test_qianwen_physicalizer tests.test_app_wiring tests.test_voice_controller tests.test_atvv_session tests.test_ble_transport_contract tests.test_legacy_key_suppressor tests.test_voice_edge_debouncer tests.test_audio_playback_drain tests.test_config tests.test_hotkey tests.test_settings_ui_helpers tests.test_qt_settings_app tests.test_build_artifacts
    result: passed (470 tests, 1 environment skip); exact-EXE Qianwen callback, async fail-closed readiness, preset isolation and existing Typeless/input/build regressions
  - command: PYTHONPATH=src python -m unittest tests.test_app_wiring tests.test_voice_controller tests.test_atvv_session tests.test_ble_transport_contract tests.test_legacy_key_suppressor tests.test_voice_edge_debouncer tests.test_audio_playback_drain tests.test_config tests.test_hotkey tests.test_settings_ui_helpers tests.test_qt_settings_app tests.test_build_artifacts
    result: passed (465 tests, 1 environment skip); full focused regression after ADR-0007
  - command: PYTHONPATH=src python -m unittest tests.test_hotkey tests.test_config tests.test_settings_ui_helpers tests.test_qt_settings_app tests.test_app_wiring tests.test_legacy_key_suppressor
    result: passed (265 tests, 1 environment skip); covers Qianwen preset migration and native transform selection while retaining Typeless regressions
  - command: PYTHONPATH=src python -m unittest tests.test_app_wiring tests.test_voice_controller tests.test_atvv_session tests.test_ble_transport_contract tests.test_legacy_key_suppressor tests.test_voice_edge_debouncer tests.test_audio_playback_drain tests.test_config tests.test_hotkey tests.test_settings_ui_helpers tests.test_qt_settings_app tests.test_build_artifacts
    result: passed (464 tests, 1 environment skip); covers both application presets, ralt+toggle persistence, voice/input regressions and QML artifact contracts
  - command: PYTHONPATH=src python -m unittest tests.test_app_wiring tests.test_voice_controller tests.test_atvv_session tests.test_ble_transport_contract tests.test_legacy_key_suppressor tests.test_voice_edge_debouncer tests.test_audio_playback_drain tests.test_config tests.test_settings_ui_helpers tests.test_qt_settings_app
    result: passed (328 tests, 1 environment skip); includes Raw Input native pass-through, direct-HID zero-wait and armed-edge suppression
  - command: scripts/build.ps1
    result: passed
  - command: scripts/test.ps1
    result: passed (1/1)
  - command: build/Debug/remotemic.exe --diagnose
    result: passed
  - command: scripts/test-baseline.ps1
    result: passed (933 tests, 7 skipped)
  - command: apps/windows/rc003/build/check-public-boundary.ps1
    result: passed
  - command: scripts/build-baseline-candidate.ps1
    result: passed; frozen executable dry-run exit 0
  - command: scripts/package-baseline-portable.ps1
    result: passed; fresh extraction dry-run exit 0
  - command: scripts/package-baseline-installer.ps1
    result: passed; signature status NotSigned
  - command: source and frozen Windows UI inspection
    result: passed for all four source pages and rebuilt frozen connection page
  - command: OpenDesign 900x680 independent visual QA
    result: passed
  - command: rebuilt installer overwrite plus installed executable hash comparison
    result: passed; installed executable matched rebuilt SHA-256
  - command: repeated installed settings launch
    result: passed; one visible window and one process
  - command: apps/windows/rc003/build/build-candidate.ps1 -SkipDependencyInstall
    result: passed; 941 tests, public boundary scan, PyInstaller build and frozen dry-run
  - command: Inno Setup 6.7.3 compile
    result: passed; intentionally unsigned refined installer
  - command: rebuilt frozen settings launch and executable icon extraction
    result: passed; responsive window, embedded icon present
  - command: python -m unittest tests.test_qt_settings_app
    result: passed (78/78)
  - command: post-reboot VB-CABLE enumeration, persistence and exact resolution
    result: passed; preferred Windows WASAPI CABLE Input selected
  - command: in-memory synthetic CABLE Input to CABLE Output loopback
    result: passed; rms 0.0566, peak 0.0800, no audio saved
  - command: source UI and Typeless settings inspection
    result: passed; UI fixes visible and Typeless selected CABLE Output
  - command: source bridge launch and app.log inspection
    result: passed for process, unique RC003 discovery and ATVV capabilities; real phrase pending
  - command: python -m unittest tests.test_app_wiring tests.test_voice_controller tests.test_atvv_session tests.test_ble_transport_contract
    result: superseded by the 111-test Mac-style held-chord run
  - command: real RC003 long-press through source bridge and VB-CABLE into Typeless
    result: failed product acceptance; signal/transcription occurred but the two-tap interaction conflict remained
  - command: Mac-style held-chord focused regression
    result: passed (111 tests, 1 environment skip); real RC003 confirmation pending
  - command: python -m unittest tests.test_app_wiring tests.test_voice_controller tests.test_atvv_session tests.test_ble_transport_contract tests.test_legacy_key_suppressor
    result: passed (129 tests, 1 environment skip); covers ATVV-before-F5, F5 repeat collapse, Raw Input duplicate rejection, and one direct-HID/F5 owner
  - command: restarted source bridge after single-owner routing change
    result: passed for process, unique RC003 discovery, ATVV capabilities, F5 guard and HID tap startup; real physical press pending
  - command: one real RC003 long-hold after single-owner routing change
    result: passed for program-side ordering and audio signal (one logical F5 trigger, 462 frames, 6.93 seconds, AUDIO_STOP deferred until physical release); foreground Notepad and Typeless acceptance still pending
  - command: import the entire remote_mic_han tree as the monorepo's first clean baseline
    result: passed at commit 2906b38 (195 files, 39 856 insertions); .gitignore cover was extended with /.claude/ and apps/windows/rc003/.gitignore's artifacts/ before commit
  - command: git commit --amend the baseline to remove accidentally-staged build artefacts then add artifacts/ back to .gitignore
    result: passed at commit 2906b38 (final hash) — the two RemoteMicRC003Setup candidate binaries were unstaged and the commit was rewritten clean
  - command: implement and test ADR-0003 Fix A worker + Fix B release debounce + Fix C drain
    result: passed; code lives in src/ovb_rc003/app.py + voice_edge_debouncer.py + audio_playback.drain(); 11 new tests cover configurable window and config clamping; commit f3db758
  - command: PYTHONPATH=src python -m unittest tests.test_app_wiring tests.test_voice_controller tests.test_atvv_session tests.test_ble_transport_contract tests.test_legacy_key_suppressor tests.test_voice_edge_debouncer tests.test_audio_playback_drain tests.test_config
    result: passed (186 passing, 1 environment skip, 1 pre-existing Python 3.14 ResourceWarning event-loop failure unrelated to this work)
known_problems:
  - Qianwen foreground commit is intermittent: the latest acceptance produced 6/6 transcriptions but only 4/6 automatic Notepad insertions; attempts 1 and 3 required manual paste
  - Typeless foreground commit is intermittent: the latest acceptance produced 6/6 transcriptions but only 3/6 visible automatic Notepad insertions; this and the Qianwen commit issue are explicitly deferred
  - Back and volume buttons are not delivered through Raw Input or the low-level keyboard hook
  - Elevated WUDFHost injection and direct HID-over-GATT characteristic access are denied by Windows
  - Typeless and Qianwen are accepted as usable; Qianwen's 4/6 automatic insertion rate remains a non-blocking target-application issue
  - Uninstall/residue checks need explicit user authorization; accepted candidate remains installed
do_not_change:
  - Do not start a C++ rewrite of working product functionality during the two-day sprint
  - Do not report all HID buttons or target-app validation as passed
  - Do not ship a SYSTEM service or weaken Windows protections to recover missing HID usages
  - Do not touch `voice_release_debounce_seconds`'s 0.200 default or its [0.050, 0.500] clamp band without a fresh ADR; the value is pinned at three layers on purpose
  - Do not replace the installer upgrade with a broad `{app}\*` deletion, change the stable AppId casually, or require a manual uninstall; read ADR-0010 before modifying installer layout
next:
  - Preserve the current source-only accepted behavior; do not package or alter audio/shortcut/foreground-commit timing unless the user asks
  - Launch the latest source settings once and visually confirm the effective-preset and battery fields; compare the percentage with Windows Bluetooth settings
  - Do not manually edit the live config or stop/start the bridge for an ordinary preset switch; agent intervention is diagnostic-only if the in-app workflow fails
  - If foreground commit reliability is later reopened, diagnose target focus/clipboard/commit behavior separately from RC003 and VB-CABLE audio
  - Preserve Typeless/Qianwen preset isolation and read ADR-0006 through ADR-0008 before changing voice semantics
  - Make unsupported back/volume state explicit in the product (deferred, not a regression)
  - Run uninstall/residue checks only with explicit authorization
  - For every packaged update, verify the installer replaces the old program payload while preserving config/key mappings/statistics/logs and leaving one installed-app entry; portable ZIPs are outside this contract
first_command_for_next_agent: git status --short --untracked-files=all -- .
```
