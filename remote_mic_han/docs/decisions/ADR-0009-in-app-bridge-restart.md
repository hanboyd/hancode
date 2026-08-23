# ADR-0009: In-App Bridge Restart for Preset Switching

- Status: accepted, manually verified
- Date: 2026-08-23
- Related: ADR-0006, ADR-0007, ADR-0008

## Context

The settings button saved the selected Typeless or Qianwen preset and then
started another bridge process. If a bridge was already running, its
single-instance mutex rejected the new process. The old bridge continued with
the configuration loaded at its own startup, so changing the preset in the UI
had no runtime effect.

## Decision

Remote Mic uses a private, per-logon-session named Windows event for graceful
bridge control. The bridge owns the event for its entire lifetime. After a
successful settings save, the UI signals the event, waits for the old bridge to
finish normal BLE/HID/audio cleanup and release its mutex, and only then starts
the replacement bridge. If stop signalling or bounded shutdown fails, the
operation fails closed and never starts a second bridge.

The mechanism does not enumerate or terminate arbitrary processes and does not
match generic `python.exe` names. Typeless and Qianwen preset definitions are
unchanged.

## Consequences

- The settings action is now “保存并切换桥接” and applies a preset without an
  agent editing configuration or manually managing the bridge.
- A bridge started from code predating this ADR has no stop event. One launch
  through the latest source entry is required to cross that upgrade boundary;
  subsequent switches are entirely in-app.
- Hardware and target-application behavior required a user-driven switch from
  Typeless to Qianwen and back; that acceptance is recorded below.

## Acceptance

The user completed the in-app Typeless/Qianwen switching flow and confirmed
that changing the preset through “保存并切换桥接” takes effect without an agent
manually stopping or replacing the bridge.
