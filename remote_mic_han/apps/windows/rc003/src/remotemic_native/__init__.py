"""Public Python wrapper around the C++ extension ``_C`` (ADR-0011).

The binding layer is consumed via the private pybind11 module
``remotemic_native._C``. This package is the ONLY symbol importable
from product code; ``_C`` is an implementation detail and must never
appear in any product import path. The package also gracefully degrades
when the compiled extension is unavailable (e.g. a source-tree import
without a CMake build), exposing ``_C_AVAILABLE = False`` so callers
can branch instead of crashing.
"""

from __future__ import annotations

try:
    from ._C import (  # type: ignore[import-not-found]
        __version__,
        ErrorCode,
        VersionInfo,
        Counter,
        AtvvCapabilities,
        ImaDecoder,
        atvv_capabilities_parse,
        atvv_control_parse,
        atvv_mic_open_command,
        atvv_mic_close_command,
        probe_value_type,
        probe_shared_ptr,
        probe_callback,
        probe_throw,
    )
    _C_AVAILABLE = True
except ImportError:
    # The compiled extension is not co-located with this __init__.py:
    # either a source-tree import without a CMake build, or a frozen
    # bundle whose _C.pyd was removed. The package itself stays
    # importable so static analysis and docs do not see it as missing;
    # bound names are reported as None so callers can branch.
    _C_AVAILABLE = False
    __version__ = "0.0.0+unknown"
    ErrorCode = None  # type: ignore[assignment,misc]
    VersionInfo = None  # type: ignore[assignment,misc]
    Counter = None  # type: ignore[assignment,misc]
    AtvvCapabilities = None  # type: ignore[assignment,misc]
    ImaDecoder = None  # type: ignore[assignment,misc]
    probe_value_type = None  # type: ignore[assignment,misc]
    probe_shared_ptr = None  # type: ignore[assignment,misc]
    probe_callback = None  # type: ignore[assignment,misc]
    probe_throw = None  # type: ignore[assignment,misc]
    atvv_capabilities_parse = None  # type: ignore[assignment,misc]
    atvv_control_parse = None  # type: ignore[assignment,misc]
    atvv_mic_open_command = None  # type: ignore[assignment,misc]
    atvv_mic_close_command = None  # type: ignore[assignment,misc]


# ``CounterSink`` is the C++ typedef used by ``probe_callback``'s argument
# signature (a ``std::function<void(std::int64_t)>``). pybind11 auto-handles
# std::function in function signatures without needing a separate
# ``py::class_`` registration, so it is intentionally NOT re-exported here.

__all__ = [
    "_C_AVAILABLE",
    "__version__",
    "ErrorCode",
    "VersionInfo",
    "Counter",
    "AtvvCapabilities",
    "ImaDecoder",
    "atvv_capabilities_parse",
    "atvv_control_parse",
    "atvv_mic_open_command",
    "atvv_mic_close_command",
    "probe_value_type",
    "probe_shared_ptr",
    "probe_callback",
    "probe_throw",
]