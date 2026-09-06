# 2026-09-06 DeepSeek implementation brief

## Priority and primary task

Today's primary task is to restore the three failing ordinary physical RC003
buttons: Back → Delete, Volume Up → Ctrl+C, and Volume Down → Ctrl+V.

The voice-edge reconnect queue change already in the working tree is
**secondary**. Do not allow it to delay the button investigation.

Known evidence: saved mappings exist; direction/OK/Home/Menu/TV/Power work;
the current and quarantined builds both fail for these three buttons; Raw Input
and the low-level hook did not see them; the optional HID tap did not connect
because Windows denied access to WUDFHost (`WinError 5`); direct HID-over-GATT
access was also denied. Do not call this a code regression without new proof.

## Required first actions

Before editing, run and report:

```powershell
git log --oneline -5
git status --short --untracked-files=all
Get-Content -Raw AGENTS.md
Get-Content -Raw docs/ai_context/INDEX.md
Get-Content -Raw docs/ai_context/PROJECT_CONTEXT.md
Get-Content -Raw docs/ai_context/CURRENT_STATUS.md
Get-Content -Raw docs/ai_context/AI_HANDOVER.md
Get-Content -Raw docs/decisions/ADR-0020-known-bugs-input-and-voice-audio-policy.md
Get-Content -Raw docs/testing/HARDWARE-VALIDATION.md
```

Treat `work/`, sibling `../notegenMCP` deletions, release artifacts, and the
existing voice-edge diff as pre-existing and outside the primary task.

## Execution order

1. Trace the configured action route for each failing button: bindings,
   resolver, input sources, low-level hook, Raw Input, HID tap, and injection.
   Compare each boundary with a working direction/OK key.
2. Gather fresh, bounded, privacy-safe evidence to locate the first missing
   boundary: Windows input, Raw Input, HID source, resolution, or injection.
3. If a product-accessible software defect is proven, make the smallest
   user-mode fix, add a focused regression, and test it.
4. If the event never reaches product-accessible input, stop. Report the
   blocked boundary and a separately approvable next option; do not fake a
   success.
5. Only after steps 1–4, review and verify the secondary reconnect-worker diff.

## Constraints

- Do not globally suppress Back, Volume Up/Down, arrows, or normal keyboard
  keys. Any remap must remain RC003/device-scoped.
- Do not add a driver, request elevation, bypass WUDFHost/HID permissions,
  inject another process, or add permanent polling without separate approval.
- Keep Python as the release-default coordinator. Do not change native-routing
  defaults, Typeless timing, Qianwen, native WASAPI, packaging, installer,
  signing, pairing, or release artifacts.
- Keep hook/audio callbacks bounded; no blocking I/O, process, or file work.
- Preserve unrelated changes; do not commit unless separately instructed.

## Acceptance and verification

Call the button defect `passed` only after actual RC003 observation confirms
each mapping fires once and a direction/OK comparison still works. Unit tests,
logs, configuration, and a successful build are insufficient.

After a code change, run focused affected tests and at least:

```powershell
cd apps/windows/rc003
..\\..\\..\\.venv\\Scripts\\python.exe -m unittest tests.test_app_wiring -v
git -C ..\\..\\.. diff --check
```

If physical testing is unavailable, mark it `deferred`. If Windows permission
denial blocks the input, mark the attempted route `failed` and include exact
evidence.

## Secondary task: reconnect worker

After the primary work, review the current `app.py` / `test_app_wiring.py`
diff. It removes stale in-band worker sentinels through event-only shutdown and
a fresh bounded queue per reconnect. It must retain the 64-item queue,
0.1-second wait, 75-ms F5 timing, and accepted voice path. Report its evidence
separately; it is not a button fix.

## Deliverable to ChatGPT

Return changed files; per-button input-boundary evidence; exact commands,
exit codes and test totals; real-device observations; excluded scope; and a
separate secondary-worker result. Update `CURRENT_STATUS.md` and
`AI_HANDOVER.md` before handoff only when observed status actually changes.
