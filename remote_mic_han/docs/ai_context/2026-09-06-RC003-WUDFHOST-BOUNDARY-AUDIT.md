# RC003 WUDFHost access-boundary historical audit

Date: 2026-09-06. Diagnosis only. No injection experiments, no driver
install, no BIOS/TESTSIGNING/Secure Boot changes, no product-code changes.

## Decision

`environment protection transition confirmed`

(Recovery consequence: `historical path no longer acceptable` — restoring the
August behavior would require weakening a security-product boundary; no
supported user-mode recovery path was found.)

## Last known good

- 2026-08-31 04:50 (last tap event; window 8/23-8/31)
- WUDFHost PID 6584 (RC003 HidOverGatt host, found via registry HostPid —
  same selection mechanism as today)
- Injector: repo source at import commit 2906b38 era; injection mechanics
  identical to current (see Injector comparison)
- Evidence of success: TAP ATTACHED/READY io_verified; gadget hook live;
  `gatt_read=83 decoded=83`; wire captures 0x00F1/0x0080/0x0081
- Host configuration: WUDF stack binaries all serviced 8/12 (wudfhost.exe
  26100.8521, WUDFRd.sys 26100.8972, WUDFPlatform.dll 26100.8521,
  hidclass.sys 26100.7920); UMDF debug flags 0; host GUIDs unchanged

## First known bad

- 2026-09-03 (release-day verification; both the current and the
  2026-08-30-frozen builds failed identically)
- OpenProcess(WUDFHost) = ERROR_ACCESS_DENIED (5) with VM_READ|VM_WRITE|
  VM_OPERATION|CREATE_THREAD, elevated with SeDebugPrivilege
- Today: all five WUDFHost instances deny even
  PROCESS_QUERY_LIMITED_INFORMATION from a standard user

## Differences (evidence-backed only)

Recorded machine-state changes between 8/31 04:50 and 9/3:

1. **2026-09-01 03:37:56 — Microsoft Defender cloud configuration update
   (Windows Defender/Operational event 5007 x5)**: feature control 203 set
   to 0x1 (newly enabled), control 301 set to 0x2710, CoreService config
   hash 0x411ED13D -> 0x2479EA7B, ECS config ETag rotated,
   MpFC_AgentParallelScanTimeoutMs set. Cloud-delivered, no reboot, no
   version bump. Machine had been continuously on since the 8/24 21:44
   boot.
2. **2026-09-01 13:51 / 14:27 — reboots** (first boots after the working
   window; no pending updates existed).
3. **2026-09-03 14:38 — Microsoft Defender antimalware PLATFORM update
   KB4052623 to 4.18.26080.3** (WU client event Id 41) — live service/
   mini-filter reload, same day as the recorded failure.
4. Nothing else in the window: no Windows KB installs (WindowsUpdateClient
   log shows only store-app and Defender signature updates), all WUDF/HID/
   BT binaries still 8/12 timestamps, WUDF registry host configs unchanged
   (debug flags 0), no HVCI/Device Guard state-change events, Smart App
   Control off (VerifiedAndReputablePolicyState=0), vulnerable-driver
   blocklist unchanged.

## Injector comparison

- same: git diff 2906b38..82e8665 over
  `frida_hid_tap_injector.py` / `frida_hid_tap_runtime.py` /
  `frida_compat.py` shows only the `--result-path` JSON logging addition;
  target selection (registry HostPid), OpenProcess desired-access mask,
  SeDebugPrivilege handling, x64 LoadLibraryW path, and gadget staging are
  byte-identical in mechanics.
- Decisive: the 2026-08-30-frozen build (built while the tap worked)
  failed identically on 9/3 → the same injector was rejected by the
  environment.

## Windows/security comparison

- Windows updates: none between the two states (8/12 batch predates the
  working window).
- Defender: the ONLY two environment changes in the window are
  Defender-side (cloud feature config 9/1 03:37; platform 4.18.26080.3 on
  9/3).
- HVCI: unchanged (no DeviceGuard log events in the window).
- Code Integrity: no relevant events beyond routine noise.
- WUDF: binaries + host configs identical.
- Bluetooth/HID: binaries identical; RC003 re-pair (9/5) postdates the
  failure.

## First changed boundary

The process-access boundary on WUDFHost (OpenProcess → ACCESS_DENIED),
first observed 9/3. By elimination, the transition correlates with the only
recorded environment changes: the Defender cloud feature-config update
(2026-09-01 03:37) and/or the Defender platform update (2026-09-03 14:38).
The exact enforcing component (WdFilter Ob-callback vs process-DACL vs
PPL-related CI state) is **not directly observable** at current privilege —
recorded as the strongest correlate, not as a proven mechanism.

## Safe recovery feasibility

`unlikely / not acceptable`
- No supported configuration difference was found (no UMDF debug mode, no
  host-mode change, no injector regression to fix).
- Any recovery of the August behavior would require weakening a
  security-product boundary (Defender feature controls / platform
  rollback) — out of scope for the product and explicitly disallowed this
  round.

## Driver fallback

The capture/remap filter prototype remains available and validated at the
package level; installation stays deferred per user direction.

## Code changes

`none` (no commits, no tracked-file changes).

## Next step

Await user direction: proceed with the deferred filter install-review
route, or commission one further read-only investigation of the specific
Defender feature control (203) semantics before deciding.
