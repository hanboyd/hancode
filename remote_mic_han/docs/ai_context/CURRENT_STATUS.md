# Current Status

- `last_updated`: 2026-08-23T10:30:00+08:00
- `updated_by`: minimax-m3 (opencode)
- `git_commit_sha`: f3db758 (ADR-0003 revision on top of 2906b38 baseline)
- `current_phase`: Phase 3 real-device validation, awaiting hardware acceptance
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

## In progress

- ADR-0003 Fix A/B/C code is now in `main` at commit `f3db758`; the source bridge has been started and stopped cleanly under the new worker, but no fresh real-device acceptance run has been performed against the new code path yet. Both Notepad F5 and Typeless multi-session remain marked `failed` on hardware until one Notepad long-press confirms no date insertion and one Typeless long-hold confirms exactly one voice window with one complete transcription under commit `f3db758`. The previous holding run (recorded on the bridge before this commit) still satisfies the "one logical F5 trigger, 462 PCM frames, AUDIO_STOP deferred" half but is now stale relative to the new worker. Reconfirmation is required before either entry may move to `passed`.

## Deferred

- Return, volume-up and volume-down do not reach Raw Input or the low-level keyboard hook. Elevated WUDFHost injection and direct HID-over-GATT characteristic access are both denied by Windows.
- Qianwen target-application validation remains deferred.
- Installer uninstall/residue validation remains pending explicit authorization; the accepted candidate is intentionally left installed.

## Next

1. Keep return/volume unavailable unless a safe, supportable Windows input path is found; do not ship a SYSTEM/WUDFHost injection workaround.
2. OpenCode/MiniMax M3 must first read the required context files, inspect Git state, restate this pending acceptance task, and start the source bridge as its first safe operational command.
3. In Notepad, long-press the RC003 microphone once and confirm no date/text is inserted; then confirm Typeless receives exactly one held Ctrl+Alt KeyDown/KeyUp session before marking it passed.
3. Validate Qianwen separately only if it remains a required target.
3.5. Implement ADR-0003 Fix A (LL hook thread decoupled from `_voice_trigger_lock`) and Fix B (50 ms release debounce). Verify with one real Notepad long-press (no date insertion) and one real Typeless long-hold (one continuous voice window, one complete transcription). Re-run the existing 129 focused tests plus the new `test_voice_edge_debounce.py`.
4. Run uninstall/residue acceptance only when the user explicitly authorizes removing the installed candidate.
5. Sign release artifacts only after acceptance and only if a signing identity is available.
