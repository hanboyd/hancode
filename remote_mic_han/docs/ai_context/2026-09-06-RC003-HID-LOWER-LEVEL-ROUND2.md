# RC003 Back / Volume Up / Volume Down — lower-level HID investigation, round 2

Date: 2026-09-06. Read-only diagnostics + route confirmation. No product-code
changes, no commit, no driver install, no privilege bypass.

## Decision

`driver likely required`

Live re-enumeration, a user-mode HidD/HidP probe, process-protection probing,
and prior-art research were all completed on the current machine. A legal
user-mode path to observe or obtain the three keys does not exist on this
Windows build. The keys are declared in the device's HID keyboard report;
the first boundary where they disappear is kbdhid's usage translation.

## Device descriptor

- VID/PID: `2717` / `32B8` (Xiaomi RC003, Bluetooth LE voice remote)
- Transport: Bluetooth LE HID over GATT (HOGP). Device instance
  `BTHLE\Dev_c05d39c2c486`. HID service node:
  `BTHLEDevice\{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&012717_PID&32b8_REV&00a4_c05d39c2c486\8&647af85&0&0055`
- Driver stack (live): `BTHLE` → `mshidumdf` (UMDF HID-over-GATT, hosted in
  WUDFHost.exe PID 6828) → `hidclass` → `kbdhid` → `kbdclass` → Raw Input
- TLC: exactly one — Generic Desktop page 0x01 / Keyboard 0x06. **No consumer
  control TLC** (re-verified live; the previous conclusion still holds).
  3 link collection nodes.
- Link collections (from HidP caps, decoded with the Windows 11 API v2.0
  `HIDP_BUTTON_CAPS` layout — ReportCount@16, Reserved[9], union@56, 72-byte
  records):
  - Link 1: page 0x0007, usage 0x0006 (Keyboard on the keyboard page — vendor
    quirk), report ID 1: one 48-bit array field (`bitField=0`, `IsAbsolute=1`,
    `ReportCount=1`), i.e. **3 × 16-bit usage slots**, usage space
    0x0000..0x00FE. This is the only keyboard report.
  - Link 2: page 0xFF00 vendor, usage 0x0000, report IDs 6, 7, 8 (vendor
    arrays; unrelated to the three keys).
- report IDs: 1 (keyboard), 6/7/8 (vendor). `InputReportByteLength=121`
  (max across the vendor reports).
- relevant usages: the keyboard array can carry any usage in
  0x0000..0x00FE — this includes 0x0080 (Keyboard Volume Up), 0x0081
  (Keyboard Volume Down), 0x00F1 (vendor-defined Back), and all working keys
  (0x0028 OK/Enter, 0x0035 TV, 0x003E mic/F5, 0x004A home, 0x004F-0x0052
  arrows, 0x0065 menu, 0x0066 power, 0x007F mute).
- working-key comparison: working and failing keys share the **same input
  report** (report ID 1, same array field). There is no separate report or
  collection for Back/volume.

Descriptor acquisition path: `CreateFile(access=0)` on the HID device
interface succeeded; `HidD_GetPreparsedData` → `HidP_GetCaps` →
`HidP_GetButtonCaps` worked. The raw descriptor itself could not be fetched:
`IOCTL_HID_GET_REPORT_DESCRIPTOR` returned ERROR_INCORRECT_FUNCTION (1) on
query-only handles and read handles are denied (see below).

## Raw report evidence

### Back (usage 0x00F1, vendor-defined on keyboard page)
- report: report ID 1, 6-byte keyboard array (3 × 16-bit usages)
- bytes: not observable from user mode on this build (read open denied, err 5)
- status: `confirmed present` at descriptor/usage-space level; wire-level
  observation comes from upstream prior art (remote-bridge-hub's WUDFHost tap
  decodes exactly this 9-byte read `01 00 00` + 6-byte payload)

### Volume Up (usage 0x0080) / Volume Down (usage 0x0081)
- report: same report ID 1 keyboard array
- bytes: same as Back — not directly observable with current privileges
- status: `confirmed present` (descriptor space; upstream tap evidence)

### OK comparison (usage 0x0028)
- report: same report ID 1 keyboard array
- bytes: surfaces as Enter via kbdhid → kbdclass → WH_KEYBOARD_LL / WM_INPUT
  (verified in the earlier dual-probe round)
- status: works end to end

Q1 answer: `confirmed present` — the three usages are inside the declared
keyboard-array usage space and are observed in the raw report by the upstream
project's tap; they are not observable with current privileges on this
machine (that observation path is separately denied, see Windows access
results).

## First missing boundary

`kbdhid` translation.

The same physical report carries usages that surface (0x28, 0x35, 0x3E, 0x4A,
0x4F-0x52, 0x65, 0x66) and usages that never surface (0x80, 0x81, 0xF1).
hidclass accepts and parses the report (the usages are within the array's
declared space). kbdhid converts only usages it has VK mappings for; it has
no mapping for keyboard-page 0x80/0x81 and drops vendor-defined 0xF1. Nothing
therefore reaches kbdclass → Raw Input → WH_KEYBOARD_LL, and Windows shows no
volume OSD. The RC003 has no consumer-control TLC, so there is no consumer
path that Windows could translate these from either.

Chain: `RC003 report` → `mshidumdf` ✓ → `hidclass` ✓ → `kbdhid` ✗ →
`kbdclass` → `Raw Input`.

## Windows access results

| API/interface | Result | Error | Boundary |
|---|---|---|---|
| SetupDiEnumDeviceInterfaces (GUID_DEVINTERFACE_HID) | RC003 keyboard interface enumerated (`\\?\hid#{00001812...}#...\kbd`) | - | none |
| CreateFile access=0, share=RW | OK | - | none |
| CreateFile FILE_READ_ATTRIBUTES, share=RW | OK | - | none |
| CreateFile FILE_READ_DATA / GENERIC_READ / GENERIC_RW | denied | 5 ACCESS_DENIED | hidclass/kbdhid system-exclusive keyboard open (anti-keylogger, since 1809) |
| HidD_GetPreparsedData + HidP_GetCaps/GetButtonCaps (query-only handle) | OK, full caps decoded | - | none |
| DeviceIoControl IOCTL_HID_GET_REPORT_DESCRIPTOR 0xB000B | denied | 1 INCORRECT_FUNCTION | requires read access — denied above |
| ReadFile (any mode) | unreachable | 5 | no read handle obtainable |
| GetRawInputDeviceInfo RIDI_PREPARSEDDATA (RC003 keyboard) | NULL | 5 | BLE HID read denial |
| GetRawInputDeviceList | RC003 present as RIM_TYPEKEYBOARD | - | none |
| ETW: logman start (Microsoft-Windows-Input-HIDCLASS 6465DA78-...) | denied | "Access is denied" | needs admin (Level B) |
| WinRT GATT access to HID service | denied (prior rounds) | AccessDenied | HOGP restricted service |
| OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION) on 5× WUDFHost | denied | 5 | protected process / locked process DACL |

## Frida HID tap audit (analysis only)

1. Targeted process: WUDFHost.exe **PID 6828** — the host of `mshidumdf` for
   the RC003 HID service, located via
   `...\8&647af85&0&0055\Device Parameters\WUDFDiagnosticInfo\HostPid`
   (matches `frida_hid_tap_runtime.py:313`). Target selection was correct.
2. Process context: LocalSystem, hosted by wudfsvc. On Windows 11, WUDFHost
   runs as a protected process (PPL, Microsoft signer) with a locked process
   object DACL. Verified live: even `PROCESS_QUERY_LIMITED_INFORMATION` from a
   standard user is denied (err 5) for all five WUDFHost instances.
3. Access-denied boundary: `OpenProcess` in `inject_library`
   (`frida_hid_tap_injector.py:174`) requesting
   PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|
   PROCESS_VM_WRITE|PROCESS_VM_READ. The earlier elevated run with
   SeDebugPrivilege enabled still recorded WinError 5 — the denial is the
   kernel's process-protection check, not a token-privilege gap. (If
   SeDebugPrivilege had been missing, the code would have raised
   `PermissionError("SeDebugPrivilege is not assigned")` instead.)
4. What would be needed: SeDebugPrivilege — insufficient; SYSTEM service —
   same protection check applies; PPL bypass — kernel-mode, prohibited.
   Unverified hypothesis only: UMDF debug-mode registry settings may start the
   host without protection — would need its own verification before being
   considered. Verdict: injection is not feasible on this build without
   prohibited kernel-mode work.

## Prior art

1. `xxb26553663-star/remote-bridge-hub` — the upstream project this product's
   tap derives from. Frida Gadget injected (elevated) into the RC003 WUDFHost,
   hooks NtDeviceIoControlFile for the HidOverGatt read IOCTL 0x80018483,
   decodes the 9-byte buffer (`01 00 00` + 6-byte keyboard payload).
   Establishes the report format and that the three usages appear in it.
   Route: user-mode + admin injection. Works only where WUDFHost injection
   succeeds.
2. `mwlt/Voice_VibeCoding` — Rust/Tauri reimplementation, same HID tap +
   WUDFHost injection. Adds volume anti-double-step (swallow native volume
   events after tap takeover) and ATVV repair. Confirms the upstream
   approach's behavior and its pitfalls (AccessDenied races).
3. `nefarius/HidHide` (+ predecessor `azraelrabbit/HidGuardian`) — KMDF
   hidclass class-upper filter with device-instance scoping, control-device
   IOCTL channel, watchdog service. Canonical architecture for a
   device-scoped HID filter on modern Windows 10/11.
4. `microsoft/Windows-driver-samples` `hid/firefly` — official KMDF HID
   filter sample (kernel-mode HID collection open, internal IOCTLs).
5. `hidapi` / `libusb` — user-mode raw HID read; works for USB HID devices,
   explicitly unavailable for system keyboards (Windows exclusive open) and
   BLE HID devices.

## Candidate solutions

### Level A — user-mode, no driver, no injection
- Static descriptor/caps via query-only handle: works (done this round) but
  yields no live reports.
- Live report observation: denied by design (err 5 on all read opens).
- ETW: requires admin → belongs to Level B.
- GATT: restricted.
- Feasibility: **none for the fix itself**. No Level A fix exists.

### Level B — privileged user-mode / system service
- ETW capture (e.g. Microsoft-Windows-Input-HIDCLASS analytic channel): admin
  required; payload coverage unverified — even if it logs reports, it is a
  diagnostic, not a remap path.
- SYSTEM service: adds nothing — the HID read denial and PPL checks are
  identical.
- Feasibility: low; not a fix route by itself.

### Level C — process injection / WUDFHost
- Technically feasible on older Windows builds (proven by upstream projects).
- On this build: blocked by PPL on WUDFHost (verified). Requires kernel-mode
  PPL bypass to restore, which is prohibited and is a driver anyway.
- Stability: poor across Windows updates; high risk.
- Verdict: **not feasible on this build without prohibited work**.

### Level D — HID lower/filter driver
- KMDF (HID filters cannot be UMDF; the HID minidriver interface is WDM).
- Placement: class upper filter on HIDClass (HidHide pattern) or a
  device-specific upper filter on the `BTHLEDevice\{00001812...}` HIDClass
  node (mshidumdf stack). Filter as high as possible: parsed reports are
  available; rewriting the report-ID-1 usage array before kbdhid is the
  mechanism.
- Device-scoped: match hardware ID
  `BTHLEDevice\{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&012717_PID&32b8`.
  All other devices pass through untouched; normal keyboards unaffected.
- Remap sketch: in report-ID-1 array, rewrite 0x00F1 → 0x004C (Keyboard
  Delete Forward); 0x0080 → {0x00E0 LCtrl, 0x0006 C}; 0x0081 → {0x00E0,
  0x0019 V} — three slots fit exactly. All other usages pass unchanged.
- Signing: EV code-signing cert required for Windows 11 x64 production;
  dev/test = self-signed + test-signing mode.
- Risk: confined to the RC003 device stack; normal keyboards unaffected by
  design; Windows-update risk moderate (filter must keep current with
  hidclass internals).
- Effort: moderate — firefly/HidHide-based, approximately 1–2 weeks
  including signing.

## Recommended route

Level D: a device-scoped KMDF HID filter that rewrites the three usages in
report ID 1 before kbdhid sees them. This is the lowest-risk route with
verified feasibility, and it satisfies the device-scope hard constraint.

## Code changes

`none`

## Next step

`Prepare a separately approvable RC003 HID filter-driver design.`

(No driver code is to be written in this step.)
