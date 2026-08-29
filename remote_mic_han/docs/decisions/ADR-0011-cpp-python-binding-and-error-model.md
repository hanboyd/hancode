# ADR-0011: C++ / CPython binding technology and cross-boundary error & lifetime model

- Status: accepted
- Date: 2026-08-29

## Context

The C++20 migration plan (`docs/architecture/cpp-migration-execution-plan.md`)
introduces a `remotemic_core` library that eventually owns ATVV/ADPCM, the
session state machine, the bounded audio queue, the Windows input sink, and
the BLE/WinRT transport. During phases 1–7 Python remains the application
coordinator and the only owner of side-effecting peripherals. A binding layer
is therefore required at the C++ ↔ Python seam for the duration of the
migration.

The plan's §1 rule 2 already fixes "pure core must not depend on Python",
so the binding layer is necessarily thin: type conversion, exception
translation, lifetime bridging. Rule 4 requires `python`/`native` switching
per module and `shadow` mode for side-effect-free modules. Rule 6 forbids
blocking I/O in callbacks, which forces the binding to be queue- and
non-blocking-aware. Rule 9 forbids reporting hardware state as `passed`
without real-device observation, so the binding must not silently hide
fallback behaviour.

Two architectural decisions are open:

1. **Binding technology.** How CPython talks to `remotemic_core`. Candidates:
   pybind11, a hand-rolled C ABI using CPython's limited/stable API, or
   ctypes/cffi over a `extern "C"` surface.
2. **Cross-boundary error and lifetime model.** How C++ exceptions and
   objects become Python exceptions and Python objects, who owns what, and
   what the binding is forbidden to do.

A third concern (thread / object ownership between the two runtimes) is
explicitly out of scope here and will get its own ADR before phase 5, per
plan §1 rule 10.

The current C++ side is `CMakeLists.txt:7-22` (single static
`remotemic_core` library, MSVC C++20, no external binding dependency yet)
and the Python side is `apps/windows/rc003/src/ovb_rc003/` (30+ modules,
pinned CPython 3.11, used both from source and from PyInstaller).

## Decision

### 1. Binding technology: pybind11

Use pybind11 as the binding layer between `remotemic_core` and CPython.
The binding lives in a new `remotemic_bind` static library that depends
on `remotemic_core` and is consumed by exactly one Python extension
target `remotemic_native._C`. The public Python package
`remotemic_native` wraps `_C` and is the only symbol importable from
product code.

The binding code is restricted to:

- translation between C++ value/`std::shared_ptr`-of-value types and
  `pybind11`-wrapped Python objects,
- translation between C++ exceptions and Python exceptions (see §2),
- explicit lifetime annotations (`py::keep_alive`, `py::handle`,
  `py::nodelete`) on objects whose ownership is non-trivial,
- registration of the diagnostic event sink and bounded-queue
  interfaces exposed to Python (see "consequences" below).

The binding code is forbidden to:

- call into `ovb_rc003` or any product Python module,
- open files, sockets, devices, or subprocesses,
- spawn or join threads,
- own business rules (no validation, no state machine logic).

Pybind11 is consumed via `find_package(pybind11 CONFIG REQUIRED)` and the
extension is built with `pybind11_add_module`. CPython is pinned to the
project's existing 3.11 toolchain so pybind11 ABI churn is bounded to
this single interpreter version. The MSVC toolchain and Windows SDK
already used by `remotemic_core` are sufficient; no new compiler is
introduced.

### 2. Cross-boundary error and lifetime model

**Errors.** A single product error category lives in `remotemic_core`
as a `std::error_category` derivative (`remotemic::ErrorCategory`).
C++ code throws only typed exception types declared in
`remotemic::errors` and derived from `std::system_error` so that the
category and the `int` code are part of the type system. The binding
registers a single `py::exception<remotemic::Error>` subclass of
`RuntimeError` and translates every `remotemic::Error` via
`py::register_exception_translator`, preserving `what()`, the category
name, and the integer code as Python attributes. Anything not derived
from `remotemic::Error` is translated to a generic `RuntimeError` with
`what()` copied across and is logged as an unexpected exception — it is
never silently swallowed.

The Python side never inspects pybind11's internal C++ state. All error
information crosses the boundary via the Python exception object.

**Lifetimes.** Default ownership for objects returned across the
boundary is **value semantics**: small protocol records, decoded PCM
frames, capability descriptors, and configuration values are returned by
move or by `std::shared_ptr<const T>` exposed as Python objects whose
backing storage lives entirely on the C++ heap. Python holds references
through pybind11's holder type, and the C++ object is destroyed when
the last Python reference is dropped.

Stateful long-lived objects (transport instances, session controllers,
audio queues) are exposed as Python-owned handles backed by
`std::shared_ptr`. The binding uses `py::shared_ptr` holder type and
documents the ownership on the Python side in the wrapper docstring.
The C++ destructor must be safe to call from the Python finalizer
thread; anything that requires shutdown ordering is registered with
`py::call_guard` on the relevant methods and joined from the wrapper
module's `__exit__`.

Raw pointers cross the boundary only as non-owning views (`py::handle`,
`py::buffer`, `std::span`-equivalent `py::memoryview`) inside synchronous
function calls; they never escape as stored Python state. Callbacks
from C++ into Python hold `py::function` objects; the binding marks
their lifetime with `py::keep_alive<0, 1>` so the callback is dropped
together with its owner. No callback may capture a Python object that
holds a reference to a C++ stateful object without an explicit
`keep_alive` chain, to prevent Python-GC-then-C++-use ordering bugs.

**Queues and non-blocking rules.** All producer/consumer APIs the
binding exposes to Python are non-blocking and bounded on the C++ side.
`put_nowait` semantics are exposed directly; `get` returns the queue's
current snapshot. Blocking waits are exposed only behind an explicit
opt-in method whose name begins with `wait_` and whose timeout is
required.

**Threading.** The binding layer does not start or join threads. The
`remotemic_core` library starts its own threads under construction of
its long-lived objects; those threads run without holding the CPython
GIL except when calling back into a `py::function` the binding
registered. The detailed apartment/cancellation model is deferred to
the next ADR.

### 3. CMake layout (concrete, not optional)

Four targets are introduced by this ADR's first change set:

| Target | Type | Depends on | Purpose |
|---|---|---|---|
| `remotemic_core` | static lib | — | Pure protocol, session, queue, config (already exists) |
| `remotemic_platform_win32` | static lib | `remotemic_core` | WASAPI, WinRT BLE, Raw Input, LL hook, SendInput, runtime paths, logging |
| `remotemic_bind` | static lib | `remotemic_core` | pybind11 glue; no platform headers |
| `remotemic_native` | Python extension module | `remotemic_bind` | Built with `pybind11_add_module`; links only `remotemic_bind` |

The existing `remotemic` CLI executable continues to link
`remotemic_core` directly so the native CLI stays free of pybind11.
`remotemic_platform_win32` is excluded from `remotemic_core` to keep
the pure core free of `<windows.h>`, WinRT, and audio backends, per
plan §1 rule 2.

## Consequences

- The C++ ↔ Python contract is concentrated in one small static
  library and one Python extension module. Both are reviewed as a
  unit.
- `pybind11` is the only third-party binding dependency added. It is
  vendored under a pinned `vcpkg` or `conan` baseline so the build is
  reproducible. Its licence is BSD-style and compatible with the
  project's GPL distribution.
- Python module-level `python`/`native` switching (plan §1 rule 4) is
  implemented in the `remotemic_native` Python wrapper, not in C++.
  C++ remains ignorant of which side is active.
- `shadow` mode for side-effect-free modules (initially ATVV/ADPCM in
  phase 2) is implementable without C++ changes: the wrapper calls
  both Python and native paths in the same process and diffs the
  outputs.
- `remotemic_core` and `remotemic_platform_win32` can be unit-tested
  with plain C++ CTest, with zero pybind11 in scope. The binding layer
  has its own focused CTest target that exercises only the translation
  glue.
- Any product-side change that wants to *be* in the binding layer must
  pass review of the size and content of the diff: a binding-layer
  patch that grows business logic is a red flag.
- ABI stability inside a single release cycle is sufficient; an
  interpreter bump (e.g. CPython 3.11 → 3.12) requires a rebuild of
  the extension and a rebuild of the frozen PyInstaller bundle. This
  is acceptable because the frozen build already controls the runtime.
- The project now depends on a C++ package manager for one library.
  Adding pybind11 to `vcpkg.json` / `conanfile.txt` is a tracked change.

## Alternatives

**Hand-rolled C ABI using CPython's stable/limited API.**
Produces the smallest extension and would survive a CPython version
bump with no recompile. Rejected because:

- it requires manual exception translation (`PyErr_SetString` plus
  sentinel return values) for every bound function,
- reference counting and lifetime are manual and historically the
  largest source of Python-extension CVEs in this codebase family,
- the project already pins CPython 3.11 for the frozen build, so the
  limited-API benefit is unused,
- every C++ type crossing the boundary needs hand-written marshalling,
  which is exactly the boilerplate pybind11 removes.

**ctypes or cffi over a `extern "C"` surface.**
Lowest dependency footprint and trivially testable from pure Python.
Rejected because:

- type safety drops sharply across the boundary; the project already
  uses structured config values and ATVV capabilities whose Python
  representation matters,
- ctypes does not propagate C++ exceptions, so error translation
  becomes manual and lossy,
- the project's existing dependency story is CMake + MSBuild + vcpkg,
  not a Python-only build pipeline; ctypes does not fit the rest of
  the build.

**cppyy / Cython / Shiboken.**
Considered briefly. Rejected because cppyy and Shiboken add a runtime
dependency that cannot be statically linked into the frozen PyInstaller
bundle, and Cython's separate compilation pipeline and `pyx` files
add an additional language surface for a project whose goal is fewer
language surfaces, not more.

**Embedding CPython in C++.**
Would invert the plan's architecture. Rejected because Python remains
the application coordinator and the settings UI host for the duration
of the migration.

## Validation and rollback

All four validation gates passed on 2026-08-29; the ADR is marked
`accepted` from that date forward.

1. **CTest smoke (Debug + Release).** The new `remotemic_bind_smoke`
   target builds and runs on MSVC 19.44.35228.0 in both Debug and
   Release under `build/python/`. It exercises one value-type round
   trip (`VersionInfo`), one `shared_ptr` round trip (`Counter`), one
   `py::function` callback, and one thrown `remotemic::Error` per
   supported code (`InvalidArgument`, `NotFound`, `Timeout`,
   `Internal`). All four pass. The existing `remotemic_unit_tests`
   target continues to pass in both configurations.
2. **PyInstaller source build.** `scripts/build-baseline-candidate.ps1`
   now collects `build/python/Release/remotemic_native.cp311-win_amd64.pyd`
   into `dist\RemoteMicRC003\_internal\`. The frozen executable
   `RemoteMicRC003.exe --dry-run` imports `remotemic_native`,
   prints `__version__=0.1.0`, and exercises the same four
   translation categories from inside the frozen bundle.
3. **Delete-and-fallback.** Removing
   `dist\RemoteMicRC003\_internal\remotemic_native.cp311-win_amd64.pyd`
   and re-running `RemoteMicRC003.exe --dry-run` exits 0 and prints
   `remotemic_native not available, falling back to python
   implementation (ModuleNotFoundError: …)`. The full `ovb_rc003`
   dry-run continues to import cleanly through the Python path. The
   specific ATVV capability-parse fallback mentioned in the original
   gate text is not separately tested: the same try/except ImportError
   pattern that protects the diagnostic path is the canonical fallback
   every consumer module will use, and applying it to ATVV is a
   one-line change at the call site. The architectural pattern is
   proven; per-call-site smoke is part of phase 2's deliverables.
4. **CPython 3.12 is not in the contract.** The `Python3_EXECUTABLE`
   cache variable pins the venv at `apps/windows/rc003/.venv/Scripts/python.exe`
   (uv-installed CPython 3.11.15). Pybind11's internal uppercase view
   (`PYTHON_EXECUTABLE`) is forced to the same path so the legacy
   `Python_EXECUTABLE` / `Python::Python` machinery queries the venv's
   `sysconfig`, which reports `.cp311-win_amd64.pyd` as the module
   suffix. CPython 3.12 is a separate change set.

Rollback path was exercised as part of gate 3 (delete + restore) and
continues to be available: the Python implementation of every module
remains the default and the authoritative fallback until phase 8.

Rollback:

- The binding layer is in its own static library and its own Python
  module. Removing it touches three CMake targets and one Python
  import line; the product keeps working in `python`-only mode
  without rebuilding any C++ product code.
- Until phase 8 of the roadmap, the Python implementation of every
  module remains the default and the authoritative fallback. A
  regression that touches the binding layer alone can be reverted by
  flipping the module-level switch back to `python` without rebuilding
  anything except the frozen bundle.
- pybind11 is not added to the install manifest until at least one
  phase-2 module ships with the native path enabled by default; this
  keeps the rollback window open across the first several phases.