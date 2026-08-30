# AI Handover

```yaml
last_updated: 2026-08-31T05:15:00+08:00
agent: minimax-m3 handing off to next agent
provider: minimax handing off to next
model: minimax-m3 handing off to next
git_commit_sha: fde9a1e
current_phase: Phase 3 native path "usable after the refactor" — three Phase 3 closeout regressions corrected, real-acceptance partially observed (Step 1/2/7a PASS, Step 6/7b/Typeless/Qianwen deferred or not-reproducible), orphan-source audit restore (qianwen_physicalizer + rc003_battery_windows) completed
current_task: nothing code-side is blocking. Per the user's "快速完成重构 + 软件健壮" balance, future sessions should pick one deferred bug per cycle and iterate, not try to clear the whole list in one pass
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
  - On any new session: git log --oneline -5 to find the Phase 3 corrective commit, then read CURRENT_STATUS.md and this handover before doing anything
  - Per the user's "快速完成重构 + 软件健壮" balance: pick ONE deferred item per cycle, fix it, validate, commit, update CURRENT_STATUS. Do not try to clear the whole list in one pass
  - For Phase 4 entry: requires a fresh ADR per cpp-migration-version-policy.md Rule 1/2; do not start without one. Phase 3's remaining deferred items may be carried forward or marked abandoned, per user direction
  - Do not re-run the full PHASE3-REAL-ACCEPTANCE.md table from scratch unless a Phase 3 source file is changed; the Step 1/2/7a PASS results are durable observations
first_command_for_next_agent: git status --short --untracked-files=all
```
