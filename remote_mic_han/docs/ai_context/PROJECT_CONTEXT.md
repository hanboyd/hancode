# Project Context

## Goal

Ship a usable Windows RC003 remote microphone, then migrate validated modules toward a C++20 core without discarding working behavior.

## Delivery constraint

The first deliverable is a two-day sprint. GPT and OpenCode/MiniMax M3 are expected to work in consecutive shifts. Handover is a normal milestone.

## Current technical direction

- Product baseline: existing validated Windows behavior.
- New foundation: C++20, CMake, interface-driven modules, hardware-free fixtures.
- First device: Xiaomi Bluetooth Remote 2 Pro (RC003).
- Audio baseline: 16 kHz mono, 16-bit PCM after IMA/DVI ADPCM decoding.
- Short-term virtual microphone: compatible third-party virtual audio device.
- UI: do not rewrite during the two-day delivery unless the existing UI blocks delivery.

## Non-goals for the current sprint

- Custom virtual audio or HID drivers.
- WinUI 3 rewrite.
- C++ replacement of working production modules.
- Multi-device, RC001, auto-update, or ARM64 expansion.

## Installer upgrade invariant

- A newer installer must upgrade the existing per-user installation in place;
  the user must not be asked to uninstall or manually delete the old version.
- Keep one stable Inno Setup `AppId` and one install location. Stop the running
  settings/bridge process before replacing the versioned application payload.
- Preserve runtime user data across upgrades: configuration, key mappings,
  usage statistics and logs. Windows must show only one installed-app entry.
- This is an installer contract, not an online background-update feature.
  Portable ZIP builds do not provide automatic replacement.
