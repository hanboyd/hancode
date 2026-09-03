# ADR-0018: Phase 8 native-default source switch and rollback boundary

- Status: Amended for first usable release
- Date: 2026-09-01

## 2026-09-03 amendment

Real RC003 + Typeless acceptance passed through the Python coordinator. The
native coordinator reached the target endpoint and accepted buffers, but its
WASAPI route remained silent on the acceptance machine while PortAudio on the
same endpoint was audible. The user chose to defer that native WASAPI issue.

Therefore the first usable release defaults `application_coordinator` to
`python`. `REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=native` remains an
explicit diagnostic/development opt-in. This avoids requiring an environment
variable to reach the only end-to-end path actually accepted by the user.

This amendment changes only the release default. It does not delete or weaken
the native coordinator, its tests, or the one-switch ownership boundary.

## Decision

The original 0.8 source candidate defaulted the top-level
`application_coordinator` to `native`. The amendment above supersedes that
default for the first usable release; the native coordinator is now opt-in.
The lower-level implementation choices remain `python` and are not mixed
across coordinator ownership graphs.

Developers can explicitly exercise the native ownership graph before process
startup with:

```text
REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR=native
```

Only one coordinator is constructed. `shadow` is rejected because lifecycle,
BLE, audio and input are side-effecting. If the native binding is absent or
incomplete while native is selected, startup fails loudly; it does not silently
claim a native candidate while running Python.

The existing Python behavior tests and golden protocol/audio fixtures are the
archived rollback contract. They remain in the repository even after a later,
separately approved module deletion pass.

## Deferred Phase 8 work

This decision does not authorize or claim any packaging result. The following
remain deferred:

- PyInstaller, portable ZIP and Inno Setup output;
- frozen-process dependency, signing, crash-dump privacy and public-boundary
  verification;
- old-install in-place upgrade, user-data retention and single-entry checks;
- real RC003 voice/PCM, Typeless, Qianwen and sleep/wake acceptance;
- deletion of any Python fallback module before one stable release cycle.

The installer `AppVersion` therefore remains unchanged. A broken native frozen
candidate must be fixed or rolled back to the previous installer candidate; it
must not be repaired by deleting the Python fallback.

## Consequences

- Normal source and release startup exercise the proven Python coordinator.
- Native remains one explicit setting and preserves a single coordinator owner.
- Packaging failures cannot be hidden by an automatic fallback.
- Phase 8 is not complete until its deferred package, upgrade and external
  acceptance gates have actual evidence.
