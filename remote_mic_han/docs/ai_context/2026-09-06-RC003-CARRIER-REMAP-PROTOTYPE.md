# RC003 three-button carrier-remap prototype — implementation round

Date: 2026-09-06. Implementation + offline verification only. No driver
installed, no TESTSIGNING/Secure Boot/BIOS changes, no certificate
installation, no commit.

## Decision

`remap prototype ready for controlled hardware test`

## Changed files

Driver (`apps/windows/rc003/driver/rc003_hid_filter/`, untracked prototype
tree):
- `src/remap.h` / `src/remap.c` (new) — pure, kernel-free, equal-length
  usage replacement
- `src/read_capture.c` (modified) — calls the remap after capture, before
  the report continues to kbdhid
- `rc003_hid_filter.vcxproj` (modified) — builds remap.c
- `rc003_hid_filter.inf` (modified) — DriverVer 0.3.0.0 only; device
  targeting and extension model unchanged
- `tests/remap_fixtures.py` (new) + `run_remap_dll.bat` (new) — fixture
  tests against the production remap.c
- `README.md` (updated)

RemoteMic (tracked, uncommitted):
- `apps/windows/rc003/src/ovb_rc003/raw_input_windows.py` (modified) —
  three carrier entries added to KEYBOARD_VK_TO_BUTTON
- `apps/windows/rc003/tests/test_carrier_remap.py` (new) — 9 tests

## Exact report transformation

Input (historical wire-level captures from this machine's app.log):
`[01][00][00][usage_lo][usage_hi][00][00] ...` (padded; a single pressed
key lands at bytes 3-4).

- `010000800000000000` -> `010000680000000000` (0x0080 -> 0x0068 F13)
- `010000810000000000` -> `010000690000000000` (0x0081 -> 0x0069 F14)
- `010000f10000000000` -> `0100006a0000000000` (0x00F1 -> 0x006A F15)

Rules enforced by the transform:
- report ID unchanged (only report ID 1 is considered)
- report length unchanged (in-place, equal-length)
- only the matched 16-bit slot changes; other slots byte-identical
- release reports (all-zero array) unchanged - no stuck keys
- fail-open: NULL/rid!=1/length<7/vendor reports return 0 and leave the
  buffer untouched
- no suppression, no injection; capture ring still records the original
  wire bytes (remap runs after capture)

## RemoteMic logical mapping path

- Driver: 0x0080/0x0081/0x00F1 -> F13/F14/F15 (carrier usages)
- kbdhid translates carriers to VK_F13 (0x7C) / VK_F14 (0x7D) / VK_F15
  (0x7E); they arrive as RC003 Raw Input keyboard events
- `KEYBOARD_VK_TO_BUTTON` additions (raw_input_windows.py):
  0x7C -> "volume_up", 0x7D -> "volume_down", 0x7E -> "back"
- these are the same logical button ids the saved bindings already
  reference (verified against device_profile.BUTTON_USAGE_IDS) - Back ->
  Delete, Volume+ -> Ctrl+C, Volume- -> Ctrl+V apply without user
  reconfiguration
- carriers are never shown in the UI (existing display names unchanged)
- device scoping: every event is checked against the exact RC003 device
  path before the VK table is consulted - physical F13-F15 keyboards are
  unaffected

## Tests

- `tests/remap_fixtures.py` (driver): 13/13 PASS - runs the production
  remap.c (compiled to a bare CRT-free x64 DLL) via ctypes against the
  real historical raw reports: 0x80->F13, 0x81->F14, 0xF1->F15, OK/up/
  down/mic unchanged, release unchanged, multi-slot partial rewrite,
  two-target report, vendor rid-6 untouched, short buffer untouched,
  non-target byte-identical.
- `tests/test_carrier_remap.py` (RemoteMic): 9/9 PASS - carrier VK ->
  logical button mapping, saved-binding id compatibility, press/release
  edges, repeat collapse, inert orphan release, working-key table
  unchanged, unknown VK inert.
- Regression: test_raw_input_pure + test_key_testing 35/35 PASS;
  test_app_wiring included in the combined run 104 tests OK (1 environment
  skip).

## Build / static-analysis results

- Debug build: PASS (msbuild exit 0, zero errors, zero warnings; /W4 +
  warnings-as-errors + SDLCheck + PREfast)
- Release build: PASS (same)
- InfVerif: PASS - unchanged extension-INF model, RC003 hardware ID match,
  AddFilter Lower, Minimum OS 10.0.18362, DriverVer 0.3.0.0
- remap fixture DLL: /W4 /WX clean, x64

## Remaining hardware-test step

Install the package under the approved test-signing environment, verify:
(1) the filter attaches only to the RC003 keyboard TLC; (2) Back/Vol+/Vol-
presses produce F13/F14/F15 (capture ring shows original bytes) and the
existing RemoteMic bindings fire; (3) direction/OK/Mic keys and ordinary
keyboards are unaffected; (4) rollback restores the original stack.
