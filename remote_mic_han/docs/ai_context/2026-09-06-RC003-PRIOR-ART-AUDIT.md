# RC003 prior-art / historical implementation audit

Date: 2026-09-06. Research only. No code changes, no driver install, no
TESTSIGNING, no BIOS changes. All five repositories shallow-cloned into
`work/prior-art/` (untracked).

## Decision

`historical user-mode path blocked by current Windows`

## 1. QL-4/RemoteMapper (the definitive RC003 prior art)

Repo: [QL-4/RemoteMapper](https://github.com/QL-4/RemoteMapper) —
`main` = full feature set with a KMDF HID lower filter;
`driverless-keymap` = no driver, 9 visible keys only.

### driverless-keymap: why Back / Vol± are missing
README line 77 states it verbatim:
`不可用键：返回/音量±（被 Windows kbdhid.sys 丢弃，无法映射）`
The branch has no driver directory, and its `_archive/` holds failed
experiments (`RawInputSniffer.cs`, `Decoder.cs`, `Diag.cs`, `ProbeHdr.cs`
…) — evidence that user-mode attempts (Raw Input sniffer, decode
experiments) were tried and never yielded the three usages. No early
no-driver commit ever captured 0xF1/0x80/0x81.

### main's filter: which boundary it solves
`driver/MiRemoteHidFilter/` — Extension INF (`Class=Extension`,
`ExtensionId`, `Models.NTamd64.10.0...18362`, AddFilter +
`FilterPosition=Lower`, hardware ID
`HID\{00001812-...}_Dev_VID&012717_PID&32b8_REV&00a4`, KMDF 1.15).
It intercepts IRP_MJ_READ completions and rewrites `report[3]` in place:
0x80→0x68 (F13), 0x81→0x69 (F14), 0xF1→0x6A (F15), plus 0x4A→F16,
0x65→F17, 0x35→F18, 0x66→F19, 0x3E→F20. This is exactly the
kbdhid-drop boundary (and exactly our Mode B carrier design — the same
F13/F14/F15 choice, independently reached).

### Their kernel-diagnostic wire captures (NOTES.md, real hardware)
```
方向上  01 00 00 52 00 ...
音量加  01 00 00 80 00 ...
音量减  01 00 00 81 00 ...
返回    01 00 00 F1 00 ...
```
Key conclusions stated by QL-4: usage lives at `report[3]` (not `report[1]`
as the HID parser's synthetic view suggests — their debugging-critical
finding); `InputReportByteLength=121`; report ID 0x01; TLC 0x0001/0x0006.
These match our round-2 enumeration on this machine exactly, and give
independent wire-level confirmation that the RC003 emits 0x0080/0x0081/
0x00F1 in report 1. Their filter passed real-device acceptance with
HVCI/Memory Integrity enabled (WDK test-signed package, TESTSIGNING).

## 2. Historical Frida / WUDFHost HID tap

### Repos
- [zhaozhuque/MIC-RC003-Windows](https://github.com/zhaozhuque/MIC-RC003-Windows):
  setup notes for RC003-MS (pure Bluetooth, VID 2717/PID 32B8), verified
  **on Windows 10 on 2026-08-18/19**. "预览版含 Frida / WUDFHost 旁路";
  troubleshooting: "右 / 主页 / 音量− 没反应 → …没开 Frida 旁路 → …
  **管理员启动**" — the tap delivered Back/volume keys with admin on Win10.
- [miaomiaozii/windows-remote-mic-app](https://github.com/miaomiaozii/windows-remote-mic-app)
  (Windows community preview v0.1.0, released via HD838A): the tap's source
  lineage. Its `frida_hid_tap_injector.py` and `frida_hid_tap_runtime.py`
  are byte-identical to this repo's except our `--result-path` JSON logging.

### Architecture (identical to ours)
- WUDFHost selection: registry
  `HKLM\SYSTEM\CurrentControlSet\Enum\BTHLEDevice\{00001812-...}\<instance>\Device Parameters\WUDFDiagnosticInfo\HostPid`
  (the host of the RC003 HID-over-GATT UMDF driver).
- Injection: elevated LoadLibraryW remote-thread injector (SeDebugPrivilege,
  OpenProcess VM_READ|VM_WRITE|VM_OPERATION|CREATE_THREAD), Frida Gadget
  pinned by SHA-256.
- Hook: `ntdll!NtDeviceIoControlFile`, filtering IOCTL `0x80018483`
  (READ_CHARACTERISTIC) in WUDFHost; captures the 9-byte output buffer
  (`01 00 00` + 6-byte payload) on successful completion.
- Transport: loopback TCP 127.0.0.1:30684 → app-side tap reader.
- Permissions: administrator (elevated start), Frida Gadget DLL staged with
  locked-down ACLs.

### Why it worked then / why it fails now
- Then (Windows 10, 2026-08): elevated injection into WUDFHost succeeded;
  Back/volume keys were delivered through the tap (zhaozhuque verified).
- Now (Windows 11 10.0.26200, this machine): OpenProcess on WUDFHost fails
  with ERROR_ACCESS_DENIED even elevated with SeDebugPrivilege (recorded
  2026-09-03), and even `PROCESS_QUERY_LIMITED_INFORMATION` from a standard
  user is denied for all five WUDFHost instances (this round's probe). That
  is the protected-process (PPL) / locked process-DACL boundary: kernel
  policy allows only same-or-higher-signer processes to open/inject (PPL
  semantics per
  [Windows integrity documentation](https://github.com/adanto/winlow/blob/main/part2/04-trust-integrity-stack.md));
  SeDebugPrivilege alone is insufficient. The same code that worked on Win10
  cannot attach on this Win11 build.

## 3. artchizhov/MiRemote_for_Windows

- pywinusb user-mode raw HID: `HidDeviceFilter(VID 0x2717, PID 0x32B9)`
  → `open()` → `set_raw_data_handler` — pure user-mode raw input reports.
- Target device: **Xiaomi Mi TV Stick remote MDZ-24-AA, PID 0x32B9 — not
  the RC003 (0x32B8)**. Report format differs (usage at bytes 1-2, mask at
  byte 3).
- Why it can read user-mode: that remote's collection is not system-claimed
  by kbdhid (Windows does not exclusively open it), so GENERIC_READ
  succeeds.
- Transferability to RC003: **none** — the RC003 keyboard TLC is
  system-claimed (round-2 empirical: FILE_READ_DATA/GENERIC_READ/GENERIC_RW
  all ERROR_ACCESS_DENIED; only access=0 query handles open). The RC003's
  vendor link collection (report IDs 6/7/8) lives inside the same TLC and
  is equally unreadable. The identical pywinusb open fails for 0x32B8.

## 4. Other remappers

- [HiMindAi/DeviceMapper](https://github.com/HiMindAi/DeviceMapper):
  Raw Input based (RegisterRawInputDevices + hid.dll parsing of RIM_TYPEHID
  reports; RIM_TYPEKEYBOARD vkeys for keyboards). Works after kbdhid —
  **not applicable to the RC003 three keys**.
- [dero/HIDeous](https://github.com/dero/HIDeous): Raw Input / low-level
  hook per-keyboard mapping, no driver — same, **not applicable**.
- QL-4's DRIVERLESS-ALTERNATIVES.md independently confirms the same for
  Scancode Map, PowerToys, AutoHotkey, WH_KEYBOARD_LL, WM_APPCOMMAND,
  Win32/WinRT HID second-client reads (keyboard page 0x07 = inaccessible),
  and their own failed WinRT GATT HOGP subscription attempt.

## Summary

- exact RC003 prior art: QL-4/RemoteMapper `main` (working KMDF lower
  filter + independent wire captures of 0x80/0x81/0xF1 at `report[3]`);
  zhaozhuque's notes (tap verified working on Windows 10);
  miaomiaozii's repo (tap source lineage, identical to ours).
- no-driver/no-BIOS routes found: **none that work on current Windows 11**.
- historical Frida tap: full architecture documented above; worked on
  Windows 10 with admin; blocked on Windows 11 26200 by WUDFHost process
  protection.
- still-valid user-mode path on current Windows 11: **no** — every
  user-mode observation point (raw HID read, Raw Input, LL hook, WinRT
  GATT, ETW without admin) is either system-claimed, post-kbdhid, or
  privilege-blocked. The capture-only KMDF lower filter remains the only
  verified route (and QL-4's identical filter already proved the whole
  design on real hardware).

## Impact on the RC003 investigation

- Q1 evidence upgraded: independent wire-level confirmation of the three
  usages (QL-4 kernel capture on real RC003 hardware) — while still not
  captured on THIS machine, the descriptor/upstream assumption is now
  corroborated by a second, independent, kernel-level source.
- Our capture prototype's decoder remains compatible with the confirmed
  layout: usage at bytes 3-4 lands in UsageSlot1 (our code reads all three
  16-bit slots), and the 121-byte padded length satisfies the >= 7 check.
- The install/capture plan (test signing + controlled install + rollback)
  remains paused per user direction; no TESTSIGNING or BIOS changes were
  made.
