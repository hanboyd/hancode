# RC003 HID carrier-remap filter prototype

Lower filter for the RC003 Bluetooth LE keyboard TLC.  It rewrites the
three keyboard-page usages that kbdhid cannot translate into standard
keyboard usages (carrier keys), equal-length and in place:

- 0x0080 (Volume Up)  -> 0x0068 (F13)
- 0x0081 (Volume Down)-> 0x0069 (F14)
- 0x00F1 (Back)       -> 0x006A (F15)

Only the matched 16-bit slot in report ID 1 changes; report ID, length,
and every other byte stay identical.  Non-matching reports pass through
byte-identical (fail-open).  No suppression, no injection.  The capture
ring still records the original wire bytes for hardware verification.

RemoteMic maps the RC003-scoped carrier VKs (F13/F14/F15) back to the
logical volume_up / volume_down / back buttons before the existing
binding/resolver pipeline, so saved bindings apply unchanged.

## Attachment point

`AddFilter` + `FilterPosition=Lower` on the RC003 keyboard TLC devnode
(service `kbdhid`): the filter sits **below kbdhid and above the HIDCLASS
collection PDO**, so IRP_MJ_READ completions carry raw input reports before
kbdhid usage→scan-code translation.

## Layout

- `src/driver.c` — DriverEntry, EvtDeviceAdd (filter setup), unload
- `src/read_capture.c` — IRP_MJ_READ interception, observation ring,
  carrier remap after capture
- `src/remap.c` / `src/remap.h` — pure, kernel-free usage replacement
- `src/control.c` — diagnostic control device `\\.\Rc003HidCapture`
  (IOCTL dump/clear)
- `rc003_hid_filter.inf` — extension INF matching the RC003 keyboard TLC
  hardware ID (PID-qualified service-UUID form) only
- `tools/rc003_capture_dump.py` — user-mode dump tool
- `tests/remap_fixtures.py` — replays the machine's real historical raw
  reports against the production remap.c (built as a bare DLL by
  `run_remap_dll.bat`)

## Build (Enterprise WDK)

```bat
LaunchBuildEnv.cmd
SetupVSEnv
msbuild rc003_hid_filter.vcxproj /p:Configuration=Debug;Platform=x64
msbuild rc003_hid_filter.vcxproj /p:Configuration=Release;Platform=x64
```

Outputs land in `bin\x64\<Configuration>\`.

## Install / rollback plan (NOT executed by the build step)

1. Dev machine: Secure Boot off, `bcdedit /set testsigning on`, import test
   certificate, reboot.
2. `pnputil /add-driver rc003_hid_filter.inf /install` (applies the lower
   filter to the matched RC003 keyboard TLC on next device start).
3. Capture: press Back / Volume Up / Volume Down / OK on the RC003, then
   `python tools\rc003_capture_dump.py --watch 1`.
4. Rollback: `pnputil /delete-driver rc003_hid_filter.inf /uninstall /force`
   (or `oemNN.inf`), reboot — the device returns to its original stack.

## Capture contract

Only reports with a target/comparison usage are stored (max 256 entries,
drop-oldest); ordinary traffic only bumps counters. No user text is ever
logged.
