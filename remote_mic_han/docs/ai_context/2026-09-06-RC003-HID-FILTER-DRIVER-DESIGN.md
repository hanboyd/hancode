# RC003 HID filter-driver design (Back / Volume Up / Volume Down)

Date: 2026-09-06. Design + feasibility only. No `.sys`, no INF file, no WDK
install, no RemoteMic product changes, no filter installed.

## 1. Evidence wording (corrected)

Back / Volume Up / Volume Down:

`strongly supported present / not directly observable on this machine`

Basis: the report descriptor's report-ID-1 keyboard array allows 0x0080/0x0081/
0x00F1 (array usage space 0x0000..0x00FE); they share report 1 with working
keys; upstream taps provide report-level corroboration; but no wire-level
bytes were captured on this Win11 machine. "wire-level confirmed on this
machine" must not be used.

## 2. Attachment points (Microsoft guidance comparison)

Microsoft guidance ([Keyboard and Mouse HID Client Drivers](https://learn.microsoft.com/windows-hardware/drivers/hid/keyboard-and-mouse-hid-client-drivers)):
value-add keyboard drivers are filters layered over the existing stack, and
the listed positions are "upper filter to kbdhid/mouhid" and "upper filter to
kbdclass/mouclass". Not recommended: "lower filter to the HID transport" and
"filter between HIDCLASS and HID transport minidriver". Also: "Avoid filter
drivers unless critical."

### A. Upper filter to kbdhid (above kbdhid, below kbdclass)
- Sees: translated keyboard packets (scan codes via the keyboard connect
  interface / KEYBOARD_INPUT_DATA), not HID usages.
- Can it observe 0x80/0x81/0xF1? **No** — kbdhid has already dropped them
  before its upper edge.
- MS guidance: recommended position — but structurally unable to solve this
  problem (can only act on keys Windows already sees).
- Device scope: possible (per-keyboard connect data), but pointless here.

### B. Upper filter to kbdclass
- Sees: scan-code stream per keyboard.
- Can it observe the three usages? **No** (dropped earlier).
- MS guidance: recommended position — same structural blindness as A.
- Device scope: possible via per-device connect data, but irrelevant.

### C. Upper filter on the hidclass keyboard collection PDO
  (== lower filter on the `HID\{...}` keyboard TLC child node, below kbdhid)
- Sees: **raw HID reports** — the filter sits in the IRP path of
  `IOCTL_HID_READ_REPORT` completions between the collection PDO (hidclass)
  and the kbdhid FDO. This is the first point above the HID class driver,
  still below kbdhid's translation.
- Can it observe 0x80/0x81/0xF1? **Yes** — pre-translation, if the reports
  carry them.
- MS guidance: not on the recommended list, but **not** on the not-recommended
  list either ("between HIDCLASS and the transport minidriver" is D, not C).
  Choosing it violates no explicit guidance statement; the two recommended
  positions (A/B) are provably unable to see dropped usages, which is the
  justification for C. Risk mitigation below (fail-open, minimal logic).
- Device scope: natural — binds to the RC003 keyboard TLC child node only.
- Bluetooth HOGP: applicable — the RC003 stack is
  BTHLE → mshidumdf → hidclass → (TLC child, service kbdhid) → kbdclass, and
  the TLC child node exists (verified live in round 2).
- KMDF: yes — it is an ordinary device filter (firefly-style), not a HID
  minidriver.

### D. Filter between hidclass and the HOGP transport minidriver
  (upper filter on the `BTHLEDevice\{00001812...}` HIDClass node, above
  mshidumdf)
- Sees: raw reports too, but at the HID minidriver interface (internal
  IOCTLs, WDM dispatch ownership issues; KMDF cannot implement the HID
  minidriver interface per the hidusbfx2 documentation).
- MS guidance: explicitly not recommended. Blast radius includes the whole
  BLE HID pairing/connection stack. **Rejected** on guidance + risk grounds.

### E. HIDCLASS class-upper filter (HidHide pattern)
- Attaches at D's location for every HID device; device scope only via
  runtime instance checks. Feasible but broader than needed. Mentioned as
  fallback only.

**Recommendation: C**, with the justification above.

## 3. Translation strategy — do not assume report rewrite

### Back (0x00F1)
Raw-usage rewrite (C) vs keyboard-packet generation (A/B): at A/B the press
is invisible, so "generate Delete scan code at keyboard-packet level" cannot
be driven by the physical key without an observer elsewhere — it would
require C anyway. At C, the options are:
1. rewrite usage in the report (0xF1 → 0x4C "Keyboard Delete Forward" —
   single-usage swap, kbdhid maps it to Delete scan 0x53); or
2. carry the press downstream (Mode B, §6) and let user mode emit Delete.
Both are single-usage operations — no chord semantics involved.

### Volume Up/Down (0x0080/0x0081) — chord question
Writing {0xE0 (LCtrl), 0x06 (C)} into the 3-slot array: kbdhid maps each
array usage via its usage table (0xE0 → scan 0x1D LCtrl make; 0x06 → scan
0x2E C make) and processes array slots in order, so {0xE0, 0x06} with the
modifier in an **earlier slot** should yield LCtrl-down before C-down — the
standard HID way chords are carried. It is *not* guaranteed by "three slots
fit"; the design therefore ranks this **plausible, verify-by-prototype** and
does not make it the primary strategy. The safer primary strategy is the
brief's suggested alternative:

`detect target usage → emit standard keyboard input downstream`

realized as Mode B (§6): the driver only performs 1:1 single-usage swaps
(0x80 → 0x69 etc.) — no chords, no slot ordering, no modifier semantics in
kernel. The chord (Ctrl+C / Ctrl+V) is assembled in user mode by the already
working SendInput/Physicalizer path.

## 4. Device-scoped installation

- Bind: the **keyboard TLC child node only** —
  `HID\{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&012717_PID&32b8...`
  (service `kbdhid`). Not the BTHLEDevice parent, not other TLCs, not other
  devices.
- INF hardware ID: `HID\{00001812-0000-1000-8000-00805F9B34FB}_Dev_VID&012717_PID&32B8`
  (optionally the `_REV&00A4` full form first, shorter form as compatible
  ID). This ID is built from the GATT HID service UUID + VID/PID and is
  **stable across re-pairing**: re-pairing creates a new device instance but
  the hardware ID is unchanged, so the INF re-applies automatically.
- Do NOT use: the transient instance path; the `LOCALMFG&0002` ID; the generic
  collection ID `HID\VID_2717&UP:0001_U:0006` (could match another
  VID 2717 keyboard collection); `HID_DEVICE_SYSTEM_KEYBOARD` (too broad).
- Installation: INF `AddFilter` directive (Win10 1903+) with
  `FilterPosition=Lower` on the matched keyboard node (= attachment point C).
  Legacy fallback for older builds: `DDInstall.HW` AddReg `LowerFilters`.
- Scope proof for the prototype: filter device-extension checks at
  AddDevice/prepare-hardware that the parent instance path contains the BLE
  instance (`BTHLEDevice\{00001812-...}`) and the collection is the keyboard
  TLC (usage page 1 / usage 6 via descriptor); mismatch → fail-open pass
  through, never touch reports.

## 5. Input semantics

Targets: Back → Delete; VolUp → Ctrl+C; VolDown → Ctrl+V.

- KeyDown/KeyUp: Mode B carries the physical state faithfully — usage present
  while held (translated to the carrier usage), absent on release. Down/up
  edges are the user-mode layer's responsibility (existing RemoteMic edge
  logic).
- Hold/repeat: **default = repeat**, at RemoteMic's existing policy
  (button_gesture REPEAT_DELAY 0.350 s / REPEAT_INTERVAL 0.100 s, Back
  0.050 s; `action_allows_repeat` already returns True for keyboard actions).
  This matches current RemoteMic behavior for held direction keys and keeps
  VolUp-hold → repeated Ctrl+C consistent with the reference app's
  "keyboard/system actions can repeat" rule. The driver performs **no**
  repeat synthesis.
- Simultaneous keys: the 3-slot array holds up to 3 usages; the three target
  keys fit together (0xF1+0x80+0x81 = 3 slots). Mixed with normal keys
  (e.g. VolUp+OK) → both usages carried; user mode handles multi-button state
  per its existing logic. Driver swaps each occurrence independently.
- Reconnect / sleep / resume / Bluetooth re-pair: the driver is **stateless
  per report** — every report is transformed independently, no down-state,
  no timers, so reconnect/resume need no special handling. Stuck-carrier-key
  risk on a lost release equals the risk any physical keyboard has, and
  RemoteMic already owns release cleanup (`HotkeyPhysicalizer.release_held`,
  disconnect cleanup).
- Filter unload/restart: no state to lose; passthrough resumes immediately.

## 6. Kernel/user-mode split — Mode A vs Mode B

| | Mode A (kernel maps to final keys) | Mode B (kernel surfaces carrier keys) |
|---|---|---|
| Kernel work | 0xF1→0x4C; 0x80→{0xE0,0x06}; 0x81→{0xE0,0x19} | 0xF1→0x68 (F13); 0x80→0x69 (F14); 0x81→0x6A (F15) |
| Chord in kernel | Yes (slot order must be proven) | No — 1:1 usage swaps only |
| Repeat synthesis | Kernel or kbdclass repeat | Existing user-mode policy applies |
| Business mapping in kernel | Yes | No |
| Flexibility / rebinding | Rebuild+resign driver | Existing RemoteMic bindings UI |
| User-mode testability | Only via final keys | F13/F14/F15 observable in Raw Input probes |
| Driver complexity | Higher (chord state, edge detection) | Minimal (stateless swap) |

Mode B is preferred: the business mapping stays in RemoteMic. The existing
user-mode pipeline already device-scopes RC003 Raw Input by VID/PID, resolves
buttons via ActionResolver, dispatches chords through the SendInput/
Physicalizer path, and holds the user's saved Back→Delete / VolUp→Ctrl+C /
VolDown→Ctrl+V bindings — Mode B makes those bindings fire as-is. Carrier
keys F13–F15 (HID keyboard usages 0x68–0x6A; kbdhid translates F13–F24) are
produced by no normal keyboard and by no other RC003 key; they are
"ordinary, stable, device-scoped virtual keys" as required.

Kernel/user split (Mode B):
- Driver: only the 1:1 usage swap in report-ID-1 completions; everything
  else passes through. No suppress, no mapping, no state.
- RemoteMic (later round, not now): map RC003-scoped VK_F13/F14/F15 to
  back/volume_up/volume_down ButtonIds; everything downstream (bindings,
  resolver, repeat policy, duplication suppression) is unchanged.

## 7. Safety and failure modes (fail-open)

Default on any of the following: pass the report through byte-identical.

- crash → device re-enumeration; after uninstall the stack is original;
- driver initialization failure (AddDevice/PrepareHardware errors) →
  continue as pure pass-through filter;
- descriptor parse failure or unexpected descriptor shape → never rewrite;
- unknown report ID / report length ≠ expected → never rewrite;
- usage pair not in the fixed {0xF1,0x80,0x81}→{0x68,0x69,0x6A} table →
  leave that slot unchanged;
- unsupported firmware (e.g. future consumer-TLC firmware) → descriptor
  check fails → pure pass-through.

Direction/OK/Mic and all other keys are never touched by construction. The
filter never suppresses a report. Completion routine is allocation-free and
lookup-table driven (no failure path needed inside the hot path).

## 8. Development and signing

### Development
- WDK 10.0.26100 (or latest), Visual Studio 2022, KMDF 1.33+; `/W4`
  warnings-as-errors.
- Self-signed test certificate (New-SelfSignedCertificate / makecert), catalog
  via inf2cat, SignTool `/fd sha256`.
- Test machine: `bcdedit /set testsigning on`; **Secure Boot must be
  disabled** on the dev/test machine (test-signed drivers cannot load with
  Secure Boot on). Recommended: a dedicated test PC (BLE HOGP pairing is
  unreliable in VMs).
- Alternative for Secure Boot machines: attestation-signed dev builds
  (requires the production Dev Center account).

### Production
- Microsoft Hardware Dev Center account + **EV code-signing certificate**
  (hardware-token private key) — required for attestation.
- Attestation signing path: submit the driver package (CAB: INF + .sys +
  PDB + catalog, files in subdirectories) via Partner Center; no HLK testing
  is required for attestation; HLK applies only if retail WHQL certification
  is pursued. Do not purchase certificates or open accounts now.
- Installation: `pnputil /add-driver <pkg>.inf /install` (or a tiny
  installer wrapping it); uninstall/rollback:
  `pnputil /delete-driver <oemN>.inf /uninstall /force` — the device then
  returns to its original filter-free stack.

## 9. Official sample comparison

### kbfiltr (input/kbfiltr)
- What it is: PS/2 (i8042prt) keyboard filter — upper filter of kbdclass for
  i8042 keyboard devices; hooks IOCTL_INTERNAL_KEYBOARD_CONNECT/DISCONNECT,
  chains a filter service callback, ISR hook.
- Reusable: the connect-data/service-callback/KEYBOARD_INPUT_DATA patterns —
  **only** if attaching at A/B (scan-code level), which cannot see our three
  usages.
- Not applicable to BLE HOGP RC003: no i8042prt, no ISR, and the usages are
  dropped before scan-code level. **Cannot be copied as the BLE keyboard
  solution.**

### firefly (hid/firefly)
- What it is: KMDF filter driver for a HID device; opens the HID collection
  I/O target in kernel, sends internal IOCTLs (feature reports, descriptor).
- Reusable: KMDF filter skeleton (WdfFdoInitSetFilter), kernel-mode HID
  collection access, IOCTL_HID_GET_REPORT_DESCRIPTOR validation at init,
  completion-routine transform structure.
- Needs adaptation: attach as `FilterPosition=Lower` filter on the kbdhid
  TLC child instead of a function driver; add IOCTL_HID_READ_REPORT
  completion handling (firefly does not transform input reports); add the
  usage-swap table.

## 10. Final design output

### Decision
`prototype design ready`

### Recommended attachment point
C — upper filter on the hidclass keyboard collection PDO (lower filter of
the `HID\{00001812-...}` kbdhid TLC child node).

### Data visible at that point
raw HID report (pre-kbdhid translation), report ID 1, 3×16-bit usage array.

### Device scope
INF AddFilter (FilterPosition=Lower) matched to
`HID\{00001812-0000-1000-8000-00805F9B34FB}_Dev_VID&012717_PID&32B8` on the
keyboard TLC child; runtime double-check of the collection identity; stable
across re-pairing; touches no other TLC, device, or keyboard.

### Translation strategy
- Back: 0x00F1 → 0x0068 (F13)
- Volume Up: 0x0080 → 0x0069 (F14)
- Volume Down: 0x0081 → 0x006A (F15)
1:1 usage swaps in the same report; final Delete/Ctrl+C/Ctrl+V mapping stays
in RemoteMic (Mode B). Chord-in-kernel (Mode A) is rejected as the primary
strategy; the {0xE0,0x06} array-chord question remains an unverified
alternative, superseded by Mode B's single-usage swaps.

### Kernel/user-mode split
Driver: stateless usage swaps + fail-open passthrough only. RemoteMic
(later round): F13/F14/F15 → ButtonIds → existing bindings/resolver/repeat
policy.

### Failure behavior
Fail-open: any anomaly (crash, init error, descriptor mismatch, unknown
report, unexpected length) → original report passes through byte-identical;
no suppression ever; non-target keys never touched.

### Signing/deployment
Dev: WDK 26100 + VS2022 + KMDF 1.33+, self-signed test cert, testsigning on,
Secure Boot off on the test box. Prod: Dev Center account + EV cert (not
purchased now), attestation signing, pnputil install/uninstall.

### Prototype scope
Capture-only prototype:
- Install the filter at attachment point C on the RC003 keyboard TLC child
  only (device-scoped INF).
- Observe and log (WPP/ETW or control-device IOCTL readout) report-ID-1
  reports containing 0x0080 / 0x0081 / 0x00F1, with timestamps.
- No key modification, no suppression, no installation on ordinary
  keyboards, no effect on any other key.
- Success criterion: pressing Back / VolUp / VolDown produces logged
  occurrences of the three usages in raw reports, while all other keys and
  devices are unaffected.

Only after this capture-only prototype passes may the actual usage-swap
remap be implemented.

### Code changes
`none`

### Next step
Run the capture-only prototype at attachment point C (requires a separate
approval to build/install a test driver on a dev machine).
