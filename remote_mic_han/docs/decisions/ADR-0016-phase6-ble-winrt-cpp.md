# ADR-0016: Phase 6 BLE / WinRT native transport boundary

- Status: Accepted
- Date: 2026-09-01

## Decision

Keep the hardware-proven Python WinRT enumeration and fail-closed RC003
identity selection. After one candidate is selected, give its opaque WinRT
device id to one C++ `WinRTBleTransport`, which exclusively owns the GATT
device, ATVV service and characteristics, notification tokens, TX writes,
disconnect observation and cleanup.

WinRT callbacks copy payloads into a bounded drop-oldest queue and return.
A native dispatcher moves copied audio/control payloads into a second bounded
binding mailbox; Python polls that mailbox from the existing BLE worker. No
Python object, borrowed `span`, raw cross-thread pointer, device id, UUID or
address is retained in a callback log.

`REMOTEMIC_NATIVE_CHOICE_BLE_TRANSPORT=python` remains the rollback. Shadow
mode is forbidden because it would connect two owners to one RC003. Frozen
0.7 release candidates prefer native; source imports without `_C.pyd` fall
back to the Python transport.

## Consequences

- Discovery diagnostics and identity behavior do not change.
- `FakeBleTransport` covers connect/write/disconnect/callback failures without
  hardware.
- Real RC003 discovery, GATT connection, control notifications, TX write,
  cleanup and two reconnect cycles are required before accepting this ADR.
- Audio notification requires a physical voice-key interaction and remains
  deferred when nobody is present to press it.
