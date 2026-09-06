# RC003 three-button fix — distribution options (A / B / C)

Date: 2026-09-07. The three-button carrier-remap filter
(`apps/windows/rc003/driver/rc003_hid_filter`) is
`implemented and physically validated` (real-hardware acceptance
2026-09-06: Back → Delete, Volume+ → Ctrl+C, Volume− → Ctrl+V; Frida tap
disabled; HVCI on; ordinary keyboards unaffected; test environment fully
rolled back afterwards). This document records how that implementation is
distributed — the implementation itself is retained in the repository under
all options.

## A — official release without the driver (current decision)

- `rc003_hid_filter.sys` / Extension INF / catalog / test certificate are
  NOT part of the official installer or portable ZIP.
- Secure Boot ON; TESTSIGNING OFF; no BCD changes; no elevation required
  by the installer; normal release experience preserved.
- Consequence: Back / Volume+ / Volume− remain unavailable in the official
  package. The carrier mapping code stays in source and cannot affect any
  device because the filter is absent and the per-event device-path scope
  gate rejects any non-RC003 event before carrier lookup.
- **Decision: A is the official 1.0.2 distribution.**

## B — self-use / test-signed driver (implemented, retained for dev use)

- The test-signed build of the same filter, installed manually on a dev
  machine: Secure Boot OFF + TESTSIGNING ON + test certificate import +
  `pnputil` (elevated). HVCI can remain ON (verified on this machine).
- Physically validated end-to-end on 2026-09-06; the rollback scripts
  exist under `work/` (local only, never committed).
- Not shipped as a normal release; only for development / personal test
  environments. Never instruct ordinary users to enter this mode.

## C — production-signed driver (future preferred production route)

- Same filter architecture, signed via Microsoft attestation (Hardware
  Dev Center / Partner Center): Secure Boot ON + TESTSIGNING OFF, normal
  install (elevated `pnputil` once), HVCI compatible, suitable for
  GitHub Release.
- Currently deferred due signing cost/process: requires an organization
  entity (EV certificates are not issued to individuals) + an EV code
  signing certificate at account level (~1.6k–4.2k CNY/year) + Partner
  Center hardware-program registration; attestation signing itself is
  free for desktop drivers.
- When available, C replaces A as the distribution of the three-button fix
  with no code changes to the filter.

## Status markers

- Driver/remap: `implemented and physically validated, distribution deferred`
  (NOT "prototype unverified").
- Control device `\\.\Rc003HidCapture` fix: `offline validated` — next
  driver-install round still needs physical open verification.
