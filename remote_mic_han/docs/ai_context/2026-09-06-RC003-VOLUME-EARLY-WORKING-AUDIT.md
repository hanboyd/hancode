# RC003 "early native volume worked, now broken" — environment audit

Date: 2026-09-06. Read-only history audit. No code changes, no driver, no
TESTSIGNING, no BIOS changes.

## Decision

`historical software path confirmed`

The user's "early Volume± controlled Windows volume" experience is fully
explained by the **Frida HID tap being connected on this machine during
2026-08-23 .. 08-31**: the three usages reached the app through the tap,
and the app dispatched the then-active mapping (system volume at that
time). Windows itself never translated the keyboard-page 0x80/0x81 usages —
there is no native path to regress.

## Machine timeline (evidence)

- 2026-07-08: Windows 11 build 26200 installed (OS has been 26200 the whole
  time).
- 2026-07-09: KB5054156; other BT devices paired 7/9-7/13 (BTHPORT records).
- 2026-08-12: KB5120708 / KB5121003 / KB5123304 installed.
- 2026-08-22: first RemoteMic runs (app.log starts 15:52; tap gadget not yet
  downloaded). 16:13 elevated tap probe: injection denied, WinError 5.
  Raw probes: Back/Vol± invisible to WM_INPUT already.
- 2026-08-23 13:49+: **tap connected**; direct HID usage events flowing
  (volume_down / volume_up down+up at 15:05).
- 2026-08-29 19:41-19:43: `RC003 HID TAP ATTACHED pid=6584`,
  `READY io_verified=true`; **back=down raw=010000f10000000000** captured.
- 2026-08-30 17:54 / 18:26: ATTACHED/READY again (same pid 6584).
- 2026-08-31 04:45-04:50: **volume_up=down raw=010000800000000000**,
  **volume_down=down raw=010000810000000000**, ok/arrows/mic captures,
  counters `gatt_read=83 decoded=83` — the last direct HID events in the log.
- 2026-09-03: injection WinError 5 again (both current and quarantined
  builds, per handover).
- 2026-09-05: RC003 re-paired (GATT `FingerprintTimestamp` 9/5,
  FingerprintVersion 3, LastConnected 9/6).
- 2026-09-06: all five WUDFHost instances deny even
  PROCESS_QUERY_LIMITED_INFORMATION (err 5) from a standard user.

## Wire-level captures ON THIS MACHINE (from app.log, late August)

```
2026-08-29 RC003 HID TAP back=down        raw=010000f10000000000   (0x00F1)
2026-08-31 RC003 HID TAP volume_up=down   raw=010000800000000000   (0x0080)
2026-08-31 RC003 HID TAP volume_down=down raw=010000810000000000   (0x0081)
2026-08-31 RC003 HID TAP ok=down          raw=010000280000000000   (0x0028)
```

Format `01 00 00 <usage> 00 ...` (9-byte read buffer) — usage at byte 3,
matching QL-4/RemoteMapper's independent kernel captures exactly. So the
three target usages were wire-level confirmed on this machine historically;
they are not currently observable because the tap can no longer attach.

## old environment (Volume± appeared to work) = ?

This machine + **connected Frida tap (WUDFHost pid 6584, injectable)** +
app dispatching the volume mapping. The OSD was real but came from the
app's injected system-volume action, not from Windows' keyboard stack.

## current environment (failed) = ?

Same machine + **WUDFHost injection denied** (all instances protected;
OpenProcess err 5 even elevated with SeDebugPrivilege, and
QUERY_LIMITED denied for standard users) + re-paired RC003 (fingerprint
refreshed 9/5, descriptor unchanged) + no consumer TLC.

## Unique difference set, ranked by likelihood

1. **WUDFHost injection boundary flipped** (allowed 8/23-8/31 for pid 6584;
   denied 8/22, and 9/3+ for all pids). Same OS build throughout, so this
   is not a Windows-update one-way regression; it toggled across a reboot
   in the 8/31-9/3 window. Candidate mechanisms (unverified, in order):
   AV/Defender kernel-callback protection state, a servicing/dynamic update
   not visible in Get-HotFix, or per-boot host protection differences.
   This is the operative difference — the only one that changed the input
   path itself.
2. **Re-pairing (9/5)** — postdates the tap failure (9/3); GATT fingerprint
   refreshed; no descriptor/interface change observed (same hardware IDs,
   same caps as August; matches QL-4's captures).
3. **Windows update batches** — the 8/12 batch predates the working window;
   no recorded update in the 8/31-9/3 break window. Not the trigger.
4. **Native kbdhid translation of 0x80/0x81** — ruled out: no evidence it
   ever existed on Windows (QL-4's independent analysis agrees; macOS is
   the platform with native handling). There is no "native volume" path to
   restore — the historical working path was the tap.

## Conclusions

- Q1 on this machine: upgraded — all three usages have **wire-level captures
  in this machine's own logs** (8/29/8/31); current live observation is
  blocked only because the tap cannot attach.
- The "early native volume" was not native: no Windows-side regression to
  chase. The single blocker for the three keys is the WUDFHost access
  boundary, which demonstrably flipped on this machine before.
- Open question for a future round: what made WUDFHost pid 6584 injectable
  on 8/29-8/31 (and blocked before/after) — AV state, per-boot host
  configuration, or an update outside Get-HotFix. Worth one targeted
  investigation before concluding the filter/TESTSIGNING path is the only
  route.
- The paused install/capture plan remains unchanged; no code, BIOS,
  TESTSIGNING, or driver changes were made this round.
