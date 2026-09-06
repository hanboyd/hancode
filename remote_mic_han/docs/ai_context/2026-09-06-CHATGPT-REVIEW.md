# 2026-09-06 ChatGPT review brief

## Priority

Review DeepSeek's ordinary RC003-button work first: Back → Delete, Volume Up
→ Ctrl+C, Volume Down → Ctrl+V. The reconnect voice-edge worker change is a
secondary review item only after the button result.

Pre-task evidence says mappings are saved and other ordinary buttons work, but
the three failing keys do not reach Raw Input or the low-level hook. Optional
HID tap and direct HID-over-GATT access were denied by Windows. Reject any
claim that treats configuration, compilation, or unit tests as physical fix
proof.

## Required review order

1. Read `AGENTS.md`, the documents named by `docs/ai_context/INDEX.md`, this
   brief, the DeepSeek brief, ADR-0020, and `HARDWARE-VALIDATION.md`.
2. Inspect `git status --short --untracked-files=all`, scoped diff, and
   `git diff --check` before tests.
3. Confirm `work/`, release artifacts, and sibling `../notegenMCP` deletions
   were excluded.
4. Audit each failing button boundary against one working direction/OK key.

## Primary audit checklist

- The first missing boundary is identified for each failing key; missing final
  action alone is not enough.
- Any remap/suppression remains RC003/device-scoped and cannot alter normal
  keyboard Back, Volume, or arrow behavior.
- No driver, global hook, elevation bypass, process injection, HID/GATT
  permission bypass, or persistent polling service was added.
- Accepted Python default, F5 timing, Typeless path, native WASAPI/Qianwen
  deferrals, and packaging boundary remain unchanged.
- Tests cover the modified production input path.
- A `passed` physical claim has actual one-time Back/Delete, Volume Up/Ctrl+C,
  Volume Down/Ctrl+V observations plus a working direction/OK comparison.
- If inaccessible or untested, result is `failed`/`deferred` with evidence and
  a minimal separately approvable next option.

## Independent verification

Run the smallest relevant input tests yourself, plus:

```powershell
cd apps/windows/rc003
..\\..\\..\\.venv\\Scripts\\python.exe -m unittest tests.test_app_wiring -v
git -C ..\\..\\.. diff --check
```

Record a replacement interpreter instead of silently changing the claim. Do
not label HID, target-app, or installer behavior `passed` from automated tests.

## Secondary review: reconnect worker

After the primary decision, confirm the worker change removes reliance on an
in-band `None` sentinel, retains the 0.1-second wake-up and 64-entry queue,
replaces the queue only after bounded stop handling, and cleans up tests
deterministically. It is not a solution to the three ordinary buttons.

## Review report

1. `Decision`: primary approved / changes requested / diagnosis only.
2. `Scope`: reviewed files and unrelated exclusions.
3. `Input-boundary evidence`: Back, Volume Up, Volume Down, working comparison.
4. `Verification`: commands, exit codes, test totals, real-device observations.
5. `Findings`: actionable defects only, or `none`.
6. `Secondary worker`: reviewed / deferred, with separate evidence.
7. `Boundary status`: automated regression, physical RC003, HID access,
   Typeless, installer using `passed`, `failed`, `deferred`, or
   `not_applicable`.
8. `Next step`: one minimal executable action. Privileged access requires user
   direction; do not assume it.
