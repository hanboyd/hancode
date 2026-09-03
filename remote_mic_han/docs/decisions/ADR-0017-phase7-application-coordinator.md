# ADR-0017: Phase 7 C++ application coordinator

- Status: accepted
- Date: 2026-09-01

## Decision

Add a C++ `ApplicationCoordinator` above the Phase 3–6 interfaces. It is the
single lifecycle owner of one BLE transport, audio route, input source and
host-action sink. Python remains the settings/statistics UI and sends numbered
commands through the binding.

Commands carry a monotonically increasing sequence number. Repeating the most
recent sequence is idempotent and cannot start or stop a backend twice;
out-of-order commands fail closed. Start uses ordered acquisition and reverse
rollback. Stop is idempotent and always attempts every cleanup boundary.

BLE callbacks stay in C++. Control notifications feed the native ATVV session;
audio notifications feed the native decoder and audio route. Typed diagnostic
events cross to Python through a bounded polling mailbox. No Python callable is
invoked from a BLE or input callback.

Phase 7 lands incrementally. The Python coordinator remains available as a
rollback path until the complete application routing and real acceptance gates
pass. Phase 8 packaging/default-switch work is explicitly excluded.

## Consequences

- Exactly one component owns side-effecting backends.
- UI retries are safe and observable.
- Hardware-free tests use the existing fake interfaces.
- User configuration, statistics and third-party adapters remain Python-owned.
- Real audio-key, target-application and sleep/wake acceptance remain separate
  observed gates.
