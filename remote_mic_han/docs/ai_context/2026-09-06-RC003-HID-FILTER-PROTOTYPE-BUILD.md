# RC003 HID filter capture-only prototype — build + install-review report

Date: 2026-09-06. Build + INF review only: no driver installed, no
test-signing changes, no Secure Boot changes, no device binding, no
RemoteMic product changes, no capture/read-path code changes.

## Decision

`package ready for controlled install`

## INF device-targeting review (round 2, user re-selected)

User re-selected both targeting decisions; the INF now uses:

- Hardware ID match: `HID\{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&012717_PID&32b8`
  (PID-qualified, no REV - see below)
- INF model: **Extension INF** (`Class=Extension`,
  `ClassGuid={E2F84CE7-8EFA-411C-AA69-97454CA4CB57}`,
  `ExtensionId={0DD45346-23E2-485E-83E4-F3F84672917C}`)

## Actual RC003 TLC identity (live enumeration, this machine)

- instance: `HID\{00001812-0000-1000-8000-00805F9B34FB}_DEV_VID&012717_PID&32B8_REV&00A4_C05D39C2C486\9&37F10909&0&0000`
- `DEVPKEY_Device_HardwareIds`, all 7 entries verbatim:
  1. `HID\{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&012717_PID&32b8_REV&00a4`
  2. `HID\{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&012717_PID&32b8`
  3. `HID\{00001812-0000-1000-8000-00805f9b34fb}_LOCALMFG&0002`
  4. `HID\VID_2717&UP:0001_U:0006`
  5. `HID_DEVICE_SYSTEM_KEYBOARD`
  6. `HID_DEVICE_UP:0001_U:0006`
  7. `HID_DEVICE`
- CompatibleIds: none
- No `HID\VID_2717&PID_32B8` form exists on this device (BLE HOGP TLC
  children carry the PID only in the service-UUID forms 1-2).
- parent: `BTHLEDevice\{00001812-...}_Dev_VID&012717_PID&32b8_REV&00a4_...`
  (mshidumdf); service `kbdhid`; class Keyboard; Upper/LowerFilters: none.
- selected match: entry **2** (`..._Dev_VID&012717_PID&32b8`):
  - RC003-only: constrains VID 2717 + PID 32B8 (no other Xiaomi keyboard
    matches); the `{00001812...}` prefix is the GATT HID service UUID and
    does not add other devices by itself.
  - re-pair stable: contains no Bluetooth address and no instance path.
  - firmware stable: omits `_REV&00a4`, so a future firmware revision does
    not break matching (entry 1 is not used for this reason).

## Attachment

- filter position: `FilterPosition=Lower` via declarative `AddFilter` on the
  matched RC003 keyboard TLC devnode
- expected lower driver: the HIDCLASS keyboard collection PDO (hidclass)
- expected upper driver: kbdhid
- intercepted I/O: IRP_MJ_READ (kbdhid's continuous read of collection input
  reports). The design observes read completions only; if the live stack
  turns out to use a different I/O form, the capture counters will show zero
  reads and the next round records the actual form instead of assuming.
- evidence: MS guidance allows filters above kbdhid/kbdclass; those
  positions cannot see the dropped usages, so the lower-filter position was
  chosen per the approved design doc (attachment point C). KMDF contract
  verified against
  [WdfDeviceInitSetIoInCallerContextCallback](https://learn.microsoft.com/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdeviceinitsetiocallercontextcallback)
  and [WdfDeviceEnqueueRequest](https://learn.microsoft.com/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdeviceenqueuerequest):
  non-read requests are returned to the framework and forwarded to the lower
  driver untouched; reads are forwarded manually with an observation-only
  completion routine.

## Prototype behavior

- modifies reports: no (completion routine reads only; status, length and
  payload always stay byte-identical)
- suppresses input: no
- injects input: no
- capture mechanism: fixed 256-entry ring buffer (drop-oldest) + diagnostic
  counters, exposed through a control device `\\.\Rc003HidCapture` with
  IOCTL dump/clear; per-entry data = sequence, QPC timestamp, report id,
  report length, three usage slots, match flags
- detection: report ID 1 only; usages 0x00F1 (Back), 0x0080 (Volume Up),
  0x0081 (Volume Down) plus OK 0x0028 as working comparison; ordinary
  direction-key traffic only bumps counters (no ring flood); no user text is
  ever logged
- fail-open mechanism: any anomaly (non-success status, no buffer, zero
  length, wrong length, unknown report id) → return without touching the
  request; control-device creation failure does not prevent filter attach;
  no queue is created so all non-read requests forward by default

## INF model (extension vs base)

- model: **Extension INF** — `Class=Extension`,
  `ClassGuid={E2F84CE7-8EFA-411C-AA69-97454CA4CB57}`,
  `ExtensionId={0DD45346-23E2-485E-83E4-F3F84672917C}` present; Models
  section matches the device-specific hardware ID; DDInstall has only
  CopyFiles + `.Filters` (AddFilter, Lower) + `.Services` (AddService of
  the filter only). No `Include`/`Needs` to keyboard.inf, no
  SPSVCINST_ASSOCSERVICE.
- existing Microsoft base driver preserved: **yes, by construction** — an
  extension INF cannot provide a function driver; Windows applies it over
  the base driver package (keyboard.inf/kbdhid), which remains the
  function driver. Uninstalling the extension removes only the filter.
- evidence: InfVerif accepts the package and reports the filter
  registration (see Validation); the extension model is documented in
  [Using an Extension INF File](https://learn.microsoft.com/windows-hardware/drivers/install/using-an-extension-inf-file).

## Expected stack

Before install (current, registry-verified):
`kbdhid` (function driver) → `HIDCLASS` keyboard collection PDO → parent
BTHLEDevice (mshidumdf) stack.

After install (expected):
`kbdhid` → **`rc003_hid_filter` (Lower filter of the devnode)** →
`HIDCLASS` keyboard collection PDO → … — "Lower" is relative to the
matched devnode's function driver (kbdhid); the filter observes
IRP_MJ_READ completions at this level. No claim is made about any finer
HID internal layer.

## Built files

- sys: `bin\x64\Debug\rc003_hid_filter.sys` (20,480 B) and
  `bin\x64\Release\rc003_hid_filter.sys` (17,920 B) — **byte-identical to
  the pre-review build** (SHA-256 re-verified: Debug
  `A54CA5A9…`, Release `24248B0C…`); capture code untouched this round.
- inf: `rc003_hid_filter.inf` (2,914 B in package, extension model)
- cat: `rc003_hid_filter.cat` (1,274 B, unsigned - signing deferred;
  `SignMode=Off`)
- pdb: `bin\x64\<Configuration>\rc003_hid_filter.pdb`
- package folder per configuration:
  `bin\x64\<Configuration>\rc003_hid_filter\{sys, inf, cat}`

## Validation

- Debug build: **PASS** (msbuild 0, zero errors/warnings, /W4 +
  warnings-as-errors + SDLCheck + PREfast)
- Release build: **PASS** (same)
- InfVerif: **PASS** — device `RC003 HID capture filter (prototype,
  capture-only)`, Hardware ID
  `HID\{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&012717_PID&32b8`,
  Minimum OS 10.0.18362, Add Filter `rc003_hid_filter` Position Lower.
- code analysis: PREfast clean on all three sources; no findings.
- .sys unchanged by this round's INF-only changes (hash-verified).

## Scope

New files (all under `apps/windows/rc003/driver/rc003_hid_filter/`):

- `src/driver.h`, `src/driver.c`, `src/read_capture.c`, `src/control.c`
- `rc003_hid_filter.inf`
- `rc003_hid_filter.vcxproj`, `rc003_hid_filter.vcxproj.filters`
- `tools/rc003_capture_dump.py` (py_compile verified; struct layouts
  verified against the C pack(1) layout: entry 22 bytes, header 40 bytes)
- `README.md`
- `.gitignore` (excludes `bin/` and `obj/`)
- untracked build outputs under `bin/` and `obj/` (gitignored)

No existing repo files modified. Toolchain: Enterprise WDK ISO downloaded
and extracted under `work/ewdk/` (self-contained; no system components
installed, no elevation used, no boot/test-signing state changed).

## Git status

Nothing committed. No tracked file modified by this round; only the
prototype files under `apps/windows/rc003/driver/rc003_hid_filter/`
(INF changed; sources unchanged) plus the pre-existing unrelated entries.

## Installation plan (documented only - NOT executed)

### Pre-install snapshot

1. Save `Get-PnpDevice -InstanceId <TLC instance>` + HardwareIds +
   UpperFilters/LowerFilters registry values to `work/` (evidence file).
2. Verify normal keyboard function: direction/OK/Mic keys work, filter
   list is empty.
3. Rollback artifacts: the original `.inf`/`.sys` are not needed for
   rollback - the device stack is fully restored by removing the extension
   package (see below); keep the snapshot evidence for comparison.

### Install (requires separate approval)

1. Test-signing prerequisites: create/import a test certificate, enable
   `bcdedit /set testsigning on`, disable Secure Boot on the dev box,
   reboot (explicitly NOT done this round).
2. Sign `.sys` with the test cert and regenerate the catalog
   (Inf2Cat + SignTool `/fd sha256`).
3. `pnputil /add-driver rc003_hid_filter.inf /install` — the extension
   registers the filter for the matched RC003 TLC devnode.
4. Re-enumerate/restart the RC003 device (devcon/pnputil
   `/scan-devices`, or re-pair) so the new stack is built.
5. Confirm scope: `Get-PnpDeviceProperty` filters show
   `rc003_hid_filter` under the RC003 TLC only; all other keyboards
   (incl. any other BTH/HID devices) have empty filter lists.
6. Verify capture: press Back / VolUp / VolDown / OK and run
   `tools\rc003_capture_dump.py --watch 1`; ordinary typing must remain
   unaffected.

### Rollback

1. `pnputil /enum-drivers` to find the extension's `oem<#>.inf`.
2. `pnputil /delete-driver oem<#>.inf /uninstall /force` — removes the
   filter registration; the Microsoft base driver (kbdhid) is untouched
   by construction.
3. Re-enumerate the RC003 device (`pnputil /scan-devices` or reboot).
4. Verify the stack is kbdhid-only again (filters empty) and normal
   keyboard keys work.
5. If the RC003 keyboard becomes unresponsive while the filter is
   attached, rollback does not depend on the RC003: the pnputil commands
   run from any working keyboard/console. Worst case, `bcdedit`/Safe Mode
   or the recovery console can remove the package from the driver store.

## Next step

`Request explicit approval to enable the required test-signing environment and install the capture-only filter on the RC003 TLC.`
