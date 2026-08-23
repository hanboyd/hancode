# ADR-0002: Initial C++ Interface Boundaries

- Status: accepted
- Date: 2026-08-22

## Decision

Use C++ pure virtual interfaces and explicit ownership for system boundaries. Start with `IBleTransport` and `IAudioRoute`; add protocol and input interfaces only alongside executable offline tests.

WinRT-specific types must remain inside infrastructure implementations. APO is not treated as a standalone virtual-microphone solution; any custom endpoint requires a separately approved driver plan.

