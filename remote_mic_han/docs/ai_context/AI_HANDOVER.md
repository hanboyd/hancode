# AI Handover

```yaml
last_updated: 2026-08-22T17:49:11+08:00
agent: codex handing off to opencode
provider: openai handing off to minimax
model: gpt handing off to minimax-m3
git_commit_sha: uncommitted-initial-framework
current_phase: Phase 3 real-device validation and Windows HID boundary
current_task: Diagnosed Notepad F5 leak as WH_KEYBOARD_LL hook thread blocked on `_voice_trigger_lock` while ATVV AudioStarted opens PortAudio (~486 ms), and Typeless multi-session flicker as RC003 firmware bounce not debounced; design fix captured in ADR-0003; awaiting authorization to implement
deadline: two-day delivery window
hardware_validation:
  status: partial
  details: BLE and ATVV voice passed; 9/12 ordinary keys passed; back/volume blocked by Windows HID boundary; Notepad F5 leak and Typeless multi-session flicker root-caused but not yet fixed
completed:
  - Added repository governance and Git exclusions
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
tests_run:
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
known_problems:
  - Back and volume buttons are not delivered through Raw Input or the low-level keyboard hook
  - Elevated WUDFHost injection and direct HID-over-GATT characteristic access are denied by Windows
  - Qianwen remains unverified
  - Single-owner sequencing passed one real hardware hold in logs, but elimination of foreground Notepad F5/date leakage and Typeless interaction are not yet user-confirmed
  - Uninstall/residue checks need explicit user authorization; accepted candidate remains installed
do_not_change:
  - Do not start a C++ rewrite of working product functionality during the two-day sprint
  - Do not report all HID buttons or target-app validation as passed
  - Do not ship a SYSTEM service or weaken Windows protections to recover missing HID usages
next:
  - Read all required context and inspect Git state before editing; do not redesign the project
  - Implement ADR-0003 Fix A (decouple `_on_legacy_key_event` from `_voice_trigger_lock` via lock-free queue + dedicated worker) and Fix B (50 ms release debounce in the worker)
  - Add `tests/test_voice_edge_debounce.py` covering 5 ms / 50 ms / 200 ms edge windows
  - Re-run the 129 existing focused tests; all must still pass
  - Start the source bridge from apps/windows/rc003 with PYTHONPATH=src and run one real Notepad long-press and one real Typeless long-hold before reporting pass
  - Long-press the RC003 microphone in Notepad and verify no F5 date/text appears
  - Validate Typeless with HOLD lctrl+lalt; require exactly one voice window with one complete transcription per physical hold, even when the firmware bounces
  - Make unsupported back/volume state explicit in the product
  - Validate Qianwen only if it remains a required target after Typeless passes
  - Run uninstall/residue checks only with explicit authorization
first_command_for_next_agent: git status --short --untracked-files=all
```
