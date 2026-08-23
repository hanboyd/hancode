# Current Status

- `last_updated`: 2026-08-23T19:47:35+08:00
- `updated_by`: codex (GPT-5)
- `git_commit_sha`: a32a345 plus uncommitted ADR-0006 through ADR-0009 implementation
- `current_phase`: Phase 3 real-device validation, primary Typeless and Qianwen paths usable
- `hardware_available`: true

## Completed

- Final local planning document reviewed.
- Root Git exclusions defined, including local `讨论/`.
- Project governance and AI handover entry points created.
- C++20/CMake core, diagnostic CLI, runtime paths, logging, and unit-test skeleton created.
- Visual Studio Build Tools/MSVC 19.44 build completed successfully.
- Hardware-free CTest suite passed: 1/1 tests, 0 failures.
- CLI `--version` returned `0.1.0`.
- CLI `--diagnose` created `%LOCALAPPDATA%\RemoteMic\logs\runtime.log` and reported the data directory ready.
- Git ignore behavior verified for the complete local `讨论/` directory.
- Clarified repository roles: `remote_mic_han` is the active project; the separate `remote-mic-windows-port` GitHub download is read-only reference material.
- Imported the Windows RC003 product baseline from pinned snapshot `271ed7947eec19c4c691ed3ba97f338461be8051` without importing Mac source or downloaded release artifacts.
- Preserved 113 baseline files plus device profiles, resources, CI, GPL license, copyright, attribution, and third-party notices.
- Created a Python 3.11 virtual environment with all pinned runtime/dev dependencies.
- Added a synthetic ATVV capabilities/ADPCM golden fixture containing no device or voice data.
- Windows baseline suite passed after packaging and UI changes: 933 tests, 0 failures, 7 skips for unavailable live/system boundaries.
- Public-boundary scan passed after packaging changes.
- C++ foundation remained healthy: CTest 1/1 passed after the import.
- Built the PyInstaller directory candidate and verified the frozen GUI-subsystem executable with `--dry-run` exit code 0.
- Built and hashed an unsigned portable ZIP; a fresh extraction also passed the frozen `--dry-run` smoke check.
- Compiled and hashed an intentionally unsigned Inno Setup installer with Inno Setup 6.7.3.
- Recorded candidate paths, byte sizes, hashes, included notices, and deferred boundaries in `docs/baseline/CANDIDATE-ARTIFACTS.md`.
- Installed the earlier unsigned candidate through its real Inno Setup UI and verified the installed settings shortcut and normal launch.
- Reproduced a duplicate settings-window bug, added a dedicated settings mutex plus best-effort foreground activation, and verified repeated launches leave one window.
- Imported the entire `remote_mic_han/` tree as the monorepo's first clean baseline: commit `2906b38 chore: import remote_mic_han Phase 0/1/2 initial baseline` (195 files, 39 856 insertions; `.gitignore` extended with `/.claude/` and `apps/windows/rc003/.gitignore`'s `artifacts/`).
- Implemented ADR-0003 Fix A in `src/ovb_rc003/app.py`: a lock-free `queue.Queue` plus a dedicated `voice-edge-worker` thread now consume the LL-hook edges; `_on_legacy_key_event` only updates atomic flags and `put_nowait`s the event so the WH_KEYBOARD_LL callback never crosses Windows' ~200 ms tolerance.
- Wired ADR-0003 Fix B (release-side debounce) into the new worker via `_dispatch_voice_mic_edge`, and widened the debounce default to 200 ms with a new `voice_release_debounce_seconds` config key (range 0.050–0.500 s, out-of-range and non-numeric values fall back to the 0.200 default); commit `f3db758 feat(adr-0003): widen voice release debounce to 200 ms and make it configurable`.
- Pinned the new default through three independent surfaces (`voice_edge_debouncer.VoiceEdgeDebouncer(release_window_seconds=0.200)`, the application read of `voice_release_debounce_seconds`, and `_normalize_voice_release_debounce`'s fallback) and added 11 unit tests covering config clamping, config round-trip and configurable window boundaries (50/100/200/350 ms).
- Extracted the Mac product UI language into `opendesign/design-systems/remote-mic-product/` and built a 900×680 Windows-adapted mockup.
- Independently verified the OpenDesign mockup at 900×680: no page-level scrollbars or clipping, bridge status visible, Windows chrome represented, and Chinese controls at least 12 pt.
- Reworked the Windows QML to use a narrow side rail, large page titles, layered cards, semantic accent selection, the real RC003 image, and a two-column connection/voice layout.
- Inspected all four source-rendered pages on the real Windows desktop and verified the rebuilt frozen GUI launches visibly with the redesigned connection page.
- Reconfirmed the complete baseline after the UI change; CTest 1/1 and the 205-file public-boundary scan also passed.
- Rebuilt the unsigned portable and installer candidates; a fresh portable extraction passed `--dry-run`.
- Overwrite-installed the rebuilt UI candidate through the real Inno Setup flow with the desktop shortcut enabled.
- Verified the installed executable SHA-256 exactly matches the rebuilt frozen executable.
- Verified the installed Mac-inspired connection page renders and a second settings launch leaves one window and one process.
- Replaced the generic executable/window icon with a shared multi-size blue microphone product icon, wired through Qt, PyInstaller and Inno Setup.
- Reduced the current machine's playback menu from 16 PortAudio rows to 8 logical devices by hiding default aliases, collapsing cross-backend copies and preferring WASAPI; the menu now hides backend jargon.
- Passed the expanded baseline suite: 941 tests, 0 failures, 7 hardware/system skips; rebuilt frozen GUI and installer candidates successfully.
- Verified the rebuilt executable opens a responsive `Remote Mic 设置` window and carries the new embedded icon.
- Fixed diagnostic text overlap with a two-row adaptive result layout and unified enabled text on the Windows system foreground color; 77 Qt settings tests passed.
- Real RC003: discovered exactly one Raw Input path and one paired BLE candidate.
- Real RC003: connected and subscribed to ATVV, negotiated 16 kHz/frame-size-120 capabilities, and received valid voice signal (1,927 frames, about 28.9 seconds, zero transport errors).
- Real RC003: verified complete press/release edges for direction, OK, Home, Menu, TV and Power through Windows keyboard/Raw Input paths (9/12 ordinary keys).
- Installed the hash-pinned official VB-CABLE Basic Pack45 driver after explicit user authorization. Before reboot, the MEDIA device and both CABLE playback/recording endpoints reported OK and the project diagnostic passed.
- After reboot, re-enumerated VB-CABLE and passed the project diagnostic: CABLE Input playback and CABLE Output recording endpoints are both present.
- Fixed the diagnostics auto-select action so cross-backend copies of one CABLE Input are collapsed and the preferred WASAPI endpoint is persisted; the focused Qt settings suite passed 78/78.
- Persisted and reloaded the real WASAPI CABLE Input selection, then resolved it exactly against the live endpoint list.
- Passed an in-memory synthetic CABLE Input -> CABLE Output loopback (RMS 0.0566, peak 0.0800); no audio was saved.
- Inspected the current source UI on Windows: enabled text colors are consistent and the previously overlapping diagnostics layout is no longer present.
- Configured Typeless to use CABLE Output as its microphone and confirmed the selected label in Typeless settings.
- Started the source bridge without rebuilding or packaging. The live log confirms exactly one RC003 candidate and ATVV capabilities at 16 kHz/frame-size 120.
- The first Windows attempt translated remote press/release into two complete Ctrl+Alt taps. Although it produced 9.36 seconds of signal and one Typeless transcription, the user confirmed the interaction conflict remained; that attempt is failed, not accepted.
- Reviewed the current macOS reference at upstream revision `1796b149f752ff2d2fa82fd818f8a5a2bc60802a`. Its custom voice shortcut path sends one held key-down on the real HID press and the balancing key-up on real HID release; ATVV MIC_OPEN/STREAM_START/STREAM_STOP are separately debounced and do not emit duplicate shortcut taps.
- Reworked the Windows source path to match that model for Typeless: custom `lctrl+lalt` is now HOLD mode, sends KeyDown once, stays held across intermediate AUDIO_STOP, and sends KeyUp on physical release. Passed 111 focused app/voice/ATVV/BLE tests.
- Diagnosed the user's Notepad date insertion as a translated F5 leak (Notepad F5 inserts the current date/time). Live logs also proved one physical hold was reaching ATVV AUDIO_STARTED, the low-level F5 path, repeated F5 key-downs, and Raw Input instead of one authoritative edge source.
- Collapsed the Windows microphone routing to the Mac ownership model: direct HID owns the mic edge when available; otherwise the swallowed low-level F5 owns it. Raw Input microphone duplicates are ignored, HOLD-mode ATVV controls only prepare/track audio, and neither path emits an additional host shortcut. Passed 129 focused app/voice/ATVV/BLE/F5-suppressor tests (1 environment skip).
- Restarted the source bridge with the new routing and confirmed unique RC003 discovery, ATVV capabilities at 16 kHz/frame-size 120, the F5 guard, and HID report tap startup. No package or installer rebuild was performed.
- Observed one real post-change RC003 hold: ATVV AUDIO_STARTED waited for the physical edge; exactly one logical F5 trigger opened the voice path; 462 PCM frames / 6.93 seconds of valid signal arrived; AUDIO_STOP was correctly deferred while the button remained physically held; and the later physical release closed the held shortcut. No second logical mic trigger appears in this session's log.
- Accepted ADR-0006 after the user clarified the product contract: the RC003 is physically held for one audio session, while both current host voice tools open on one short shortcut TAP and close/transcribe on a later TAP at physical release.
- Restored `VoiceController` as the single source of the configured host action instead of forcing TAP in application wiring; TOGGLE produces boundary TAP/TAP and HOLD remains a compatibility KEY_DOWN/KEY_UP protocol, including cleanup and failed-close retry ownership.
- Repaired the interrupted `lctrl+lalt + hold` configuration pair to TOGGLE on load/save without changing the recorded shortcut.
- Passed 258 voice/config/settings-helper tests with 1 environment skip and passed 78/78 Qt settings tests after repairing two pre-existing indentation errors that prevented the test module from importing.
- Restarted the source bridge at 11:51 with the ADR-0006 working tree. Live startup passed for `mode=toggle hotkey=lctrl+lalt`, exactly one RC003 candidate, ATVV 16 kHz/frame-size 120 capabilities, the legacy-key guard and the HID report tap.
- User-driven monitored acceptance at 12:05 reproduced the long close delay: opening TAP at 12:05:00.005, valid audio until `AUDIO_STOP` at 12:05:25.654, but Windows physical release and the closing TAP did not arrive until 12:05:50.438 (24.8 seconds later). The Typeless-matched window also remained visible for the monitor's 15-second post-close interval.
- Added a TOGGLE-only stable-audio-stop fallback: wait 2.5 seconds (above the longest observed 2.342-second transient stop/restart gap), cancel if audio restarts, otherwise close without waiting for delayed F5 KeyUp. A post-close latch rejects stale F5/MIC/AUDIO restart events until debounced physical release.
- User-driven confirmation after the fallback change passed: `AUDIO_STOP` at 12:15:59.232, closing TAP at 12:16:01.752 (2.520 seconds), visibly shorter close time, automatic window close and successful text insertion. Stale F5 repeats at 12:16:05/11/18 were rejected and did not reopen the window.
- Allowed a later authoritative ATVV `AUDIO_START` to begin a genuinely new session even if the stale Windows KeyUp has not arrived; ordinary repeated F5 cannot clear that latch.
- Passed the expanded focused regression after the fallback and next-session guard changes: 339 tests, 0 failures, 1 environment skip.
- Restarted the source bridge at 12:20 after the final next-session guard; startup again passed for TOGGLE `lctrl+lalt`, unique RC003 discovery, ATVV capabilities, legacy-key guard and HID report tap.
- Measured the accepted run's press path: first ATVV `AUDIO_STARTED` at 12:15:31.783, Windows F5 edge at 12:15:31.850, WASAPI playback open at 12:15:33.853 and the opening TAP in the same millisecond. That is 2.070 seconds from audio start or 2.003 seconds from F5 to shortcut delivery; the log cannot directly timestamp Typeless' own overlay paint.
- Diagnosed post-transcription correction lag as the global low-level keyboard hook's 60 ms Raw Input correlation wait. Live ordinary Right-arrow and Enter edges each waited 62-78 ms on both down and up even with no RC003 arm, directly explaining the user's sluggish cursor/editing experience.
- Default RC003 arrows and OK/Enter now use their one native Windows key event in the Raw Input fallback instead of swallow-and-reinject. Those shared VKs are excluded from global correlation, so the user's own arrows and Enter pass immediately; custom mappings/secondary gestures retain exact armed suppression. Dedicated F5 is swallowed before ordinary-key correlation so stale repeats cannot monopolise the hook. When direct HID later becomes active, all exact RC003 edges remain pre-armed/consumed with zero wait. Unmatched per-key telemetry was reduced from INFO to DEBUG.
- Passed 328 focused voice/input/config/settings regressions after the native-pass-through and zero-wait changes, with 0 failures and 1 environment skip.
- User confirmed the post-transcription editing check is now fluent: arrow navigation, deletion and mixed editing no longer show the earlier lag. The first two stability checks, including the Notepad F5-leak check, were reported complete without a new failure.
- Corrected the target-audio acceptance record after the user disclosed that earlier Typeless transcriptions had used the headset microphone. Those earlier runs prove shortcut/window/editing behavior only, not RC003-to-target audio.
- User then explicitly selected the VB-CABLE virtual microphone in Typeless and completed another successful transcription with no obvious quality difference from the headset microphone. The matching live bridge session carried 1,573 RC003 frames / 23.595 seconds, `result=signal`, with one opening and one closing `lctrl+lalt` TAP. Typeless RC003 -> VB-CABLE -> target transcription is therefore user-accepted as passed.
- Added manual application presets: `Typeless = lctrl+lalt + toggle`; `千问 = ralt + toggle`.
- Qianwen manual diagnosis proved physical keyboard short/long Right Alt works, while medium/elevated generated Right-Alt and a global F5 hook transform produced no Qianwen overlay session. This is a process-local injected-input boundary, not a delay, audio or setting problem.
- Implemented ADR-0007 against the exact installed EXE: its `SetWindowsHookExW(WH_KEYBOARD_LL, ...)` call points to runtime RVA `0x85684`, whose adjacent code verifies Right Alt (`0xA5`). The separately shipped PDB does not match the EXE runtime layout and is not used as an address authority. The adapter clears injection flags only for RemoteMic-marked Right Alt. Path/hash/attach/readiness mismatch fails closed; no Qianwen file is modified. Typeless is unchanged.
- Fixed the Qianwen runtime callback authority: the initially used PDB address did not match the installed EXE. The exact EXE's `SetWindowsHookExW(WH_KEYBOARD_LL, ...)` registration identifies RVA `0x85684`; adapter startup now waits for an explicit post-intercept ready message and fails closed on an asynchronous Frida script error instead of reporting a false success.
- Passed 470 focused regressions for the target-local adapter and existing voice/input/config/settings/build-artifact paths, with 0 failures and 1 environment skip.
- User accepted the Qianwen remote-button window lifecycle. Correlated evidence: press at 14:17:54 delivered one marked Right-Alt TAP and opened overlay session 13; release at 14:18:01 delivered one marked Right-Alt TAP; Qianwen ended the overlay at 14:18:03. The RC003 stream contained 495 frames / 7.425 seconds of signal. Typeless routing was not selected or modified by this Qianwen-only adapter.
- Replaced per-notification playback with one continuous blocking VB-CABLE writer (ADR-0008): a dedicated thread keeps the stream active, writes 20 ms chunks or silence, bounds queued audio to two seconds, drops the oldest queued chunks on overflow, prepares the endpoint before the opening shortcut, and drains queued audio before close.
- Passed the focused continuous-writer regression: 82 tests, 0 failures, 1 environment skip. A clean six-burst synthetic run reached CABLE Output with peak 0.244141 after unrelated temporary audio-session interference was removed.
- User completed six numbered Qianwen real-device attempts with the virtual microphone selected. All six produced usable transcriptions and the matching bridge log records six `result=signal` RC003 sessions (3.435-4.875 seconds). Notepad received four results automatically; attempts 1 and 3 were available in Qianwen but required manual paste. The user accepts this version as usable.
- User completed six numbered Typeless real-device attempts. Typeless history contains all six transcriptions and the matching bridge log records six `result=signal` RC003 sessions (3.870-4.815 seconds). Notepad visibly received attempts 1, 3 and 5, so automatic foreground insertion was 3/6 if no results were manually removed. The RC003/VB-CABLE/recognition path passed; foreground commit remains intermittent.
- Diagnosed the ineffective in-app preset switch: `saveAndLaunch()` saved the new hotkey but only attempted to launch a second bridge. The existing bridge retained its startup configuration and the single-instance guard rejected the replacement, so the UI selection had no runtime effect.
- Implemented ADR-0009. The bridge now owns a private per-session Windows stop event. “保存并切换桥接” saves first, signals the existing bridge, waits up to eight seconds for normal BLE/HID/audio cleanup and mutex release, and then launches the replacement. Stop failure or timeout fails closed; no process-name enumeration or generic Python termination is used.
- Passed 219 relevant bridge/settings/entrypoint/app tests with 1 environment skip. A real Windows stop-event round trip (create, discover, signal, wait and release) passed. The broader nested full-suite run reached 1,027 tests and failed only the already-recorded public-boundary replay for two historical candidate binaries and the administrator HID probe; no new functional regression appeared.
- User manually accepted the in-app Typeless/Qianwen bridge switch. Ordinary preset changes now stay inside the settings window; agent-side bridge management is diagnostic-only.
- Added truthful connection-page status: “当前生效预设” changes only after a successful bridge replacement, failed switches retain the last confirmed preset, and the stale “仍需真机验收” device copy is replaced by a concise system-recognition summary.
- Added a privacy-bounded SetupAPI RC003 battery probe. It exact-matches only the supported paired-device names, exposes only a 0-100 percentage, and never returns a device path or Bluetooth identifier. The real Windows device property currently returned 100%.
- Passed 245 focused battery/diagnostics/settings/bridge tests with 2 environment skips in the project Python 3.11 environment; the native battery probe also returned 100% on the real RC003 device node.
- Rebuilt the frozen candidate from the current preset/adapter source and passed 1,014 tests with 7 environment skips plus frozen `--dry-run`. The public-boundary gate passes when ignored historical ZIP/installer artifacts and the separately tracked administrator HID probe are temporarily isolated; those three original files were restored unchanged afterward, so the unmodified working tree's boundary replay still reports them.
- Compiled a new unsigned Inno Setup 6.7.3 installer and overwrite-installed it successfully. The installed executable SHA-256 exactly matches the new frozen build (`38518E226EC0C0738D0884AFACACAC92F18034EFCA5F409B02D7D036B86258F5`), installed `--dry-run` exits 0, and the installed QML contains the Typeless/Qianwen preset UI. No bridge was started by the silent install.
- Applied the OpenDesign mockup through native QML design-system mapping rather than HTML/WebView embedding: shared card/status/divider components and one-to-one connection/mapping layouts now carry the design while existing controller, bridge, audio and shortcut ownership remain unchanged.
- Added a Windows-native screenshot harness for all five current-source pages. Independent final comparison found no blocking OpenDesign differences after correcting false-positive state colors, card overflow, mapping-field truncation, dense record controls and an English diagnostic leak.
- Added `打开最新源码设置.cmd`; it resolves only the checked-out source and project virtual environment, refuses an already-open possibly stale settings window, and never loads `dist`, installed or old package code. Source-path verification passed.
- Passed 196 focused UI/statistics/layout/diagnostics tests after final visual corrections (2 environment skips). The broader discovery invocation remains red only at already-known repository/environment boundaries: package-relative discovery form, two historical ignored binaries plus the administrator HID probe in the public-boundary replay, and its nested lifecycle gate.
- Accepted ADR-0010 and made in-place installer upgrade a mandatory project invariant: a stable AppId and install root retain one Windows installed-app entry, the running old version is stopped, only the dedicated versioned payload is replaced, and configuration/key mappings/statistics/logs are preserved. Portable ZIPs remain manually replaceable artifacts.
- Published a privacy-sanitized current snapshot as commit `19a0004` on `origin/codex/remote-mic-public` and opened public Draft PR `hancode#2`. The branch is based directly on current `origin/main`; the eight older unpublished local development commits were deliberately not pushed, preventing their historical one-off probe from entering the public project history. The published snapshot contains 211 files; critical-text, real-MAC and PNG-metadata findings are all zero.

## In progress

- No additional audio/foreground-commit behavior change is requested. Typeless TAP/TAP, RC003 virtual-audio transcription, post-transcription editing and the Notepad F5-leak check are user-accepted. Qianwen's isolated target-local `ralt + toggle` window/audio/transcription path is accepted as usable.
- User-controlled preset switching is implemented and manually accepted. The OpenDesign-to-QML port is source-complete and independently visually accepted. No audio, shortcut or target-app commit behavior was changed during packaging.
- First usable-version packaging is complete after removing confirmed generated outputs/caches and the unused standalone administrator HID probe. The final regression passed 1,041 tests with 7 environment skips; the public-boundary scan passed 235 files; the fresh portable extraction passed `--dry-run`; and both generated artifact hashes match their sidecars.

## Deferred

- Return, volume-up and volume-down do not reach Raw Input or the low-level keyboard hook. Elevated WUDFHost injection and direct HID-over-GATT characteristic access are both denied by Windows.
- Qianwen transcription-content accuracy remains a target-application concern; the shortcut/window lifecycle and RC003 signal path are passed.
- Qianwen foreground commit is intermittent: in the latest six-attempt acceptance, 4/6 transcriptions were inserted into Notepad automatically and 2/6 required manual paste. This is a target-application focus/commit boundary, not an RC003 or VB-CABLE recognition failure.
- Typeless foreground commit is also intermittent: its latest six-attempt acceptance produced 6/6 transcriptions but only 3/6 visible automatic Notepad insertions. The user explicitly chose to defer this issue; do not add clipboard monitoring, fallback paste, focus stealing or close-timing changes unless the issue is reopened.
- Installer uninstall/residue validation remains pending explicit authorization; the accepted candidate is intentionally left installed.

## Next

1. Keep return/volume unavailable unless a safe, supportable Windows input path is found; do not ship a SYSTEM/WUDFHost injection workaround.
2. Preserve strict preset isolation: Typeless must never load the Qianwen adapter; Qianwen must fail closed after any executable update until its hash and callback are revalidated.
3. Run uninstall/residue acceptance only when the user explicitly authorizes removing the installed candidate.
4. Treat Qianwen's 4/6 and Typeless's 3/6 automatic Notepad insertion rates as known deferred issues; do not change the accepted audio/shortcut path unless the user reopens foreground commit reliability.
5. Close any currently open administrator/installed settings window, then use `打开最新源码设置.cmd` for one manual latest-source inspection; confirm the effective preset updates after a successful switch and the displayed RC003 battery agrees with Windows Bluetooth settings.
6. The unsigned first usable-version artifacts are packaged under `artifacts/`; sign a later release only if a signing identity is available.
7. On the next real installer update, verify ADR-0010 against the live installation: one installed-app entry, old payload replaced, and config/mappings/statistics/logs preserved.
8. Continue public GitHub work from `codex/remote-mic-public` / PR #2 or its merged descendant; do not push the current local `main`'s eight unpublished historical commits onto the clean public branch.
