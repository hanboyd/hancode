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
        RemoteMicError,
        VersionInfo,
        Counter,
        AtvvCapabilities,
        ImaDecoder,
        DcHighPassFilter,
        FrameAccumulator,
        atvv_capabilities_parse,
        atvv_control_parse,
        atvv_mic_open_command,
        atvv_mic_close_command,
        postprocess,
        probe_value_type,
        probe_shared_ptr,
        probe_callback,
        probe_throw,
        # Phase 3 / ADR-0013 step 3: voice controller / release-window
        # debouncer / ATVV session state machines. The bridge wrappers
        # in ovb_rc003/voice_controller_native.py /
        # voice_edge_debouncer_native.py / atvv_session_native.py import
        # these via the public remotemic_native package rather than the
        # private ``_C`` module so the package wrapper remains the
        # single import surface for product code (ADR-0011).
        VoiceTriggerMode,
        VoiceHostAction,
        VoiceController,
        VoiceEdgeDebouncer,
        AtvvSession,
        # Phase 4 / ADR-0014 step 3+5: IAudioRoute + WASAPI + PcmFormat.
        # The bridge wrapper in ovb_rc003/audio_route_native.py
        # (``_NativeAudioRoute.__init__``) accesses these via the
        # public remotemic_native package; keeping them on the
        # package surface (rather than forcing callers to reach into
        # ``_C``) preserves the single-import-surface rule (ADR-0011).
        PcmFormat,
        WasapiAudioRoute,
        # Phase 5 / ADR-0015 step 3: input layer binding seam. The
        # bridge wrappers in ovb_rc003/input_source_native.py /
        # host_action_sink_native.py access these via the public
        # remotemic_native package; same single-import-surface rule
        # (ADR-0011). Windows-only symbols (RawInputSource /
        # LowLevelKeyboardHook / FridaHidTapSource /
        # SendInputActionSink) are also exported from _C when
        # compiled on Windows; the public package re-exports them
        # with the same ``name is None when binding missing``
        # convention.
        InputSourceKind,
        InputEventKind,
        SystemAction,
        ButtonId,
        ResolvedActionKind,
        InputEvent,
        ResolvedAction,
        IInputSource,
        FakeInputSource,
        IHostActionSink,
        FakeHostActionSink,
        ActionResolver,
        DefaultActionResolver,
        HotkeyPhysicalizer,
        # Phase 6: bounded-mailbox C++/WinRT GATT owner. Discovery remains
        # in Python so existing identity selection and diagnostics stay
        # unchanged; the connection itself has exactly one owner.
        WinRTBleTransport,
        CoordinatorState,
        CoordinatorCommandKind,
        CoordinatorCommandStatus,
        CoordinatorEventKind,
        CoordinatorCommandResult,
        ApplicationCoordinator,
        WindowsVoiceAudioPolicyLease,
        UiSettingsState,
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
    RemoteMicError = None  # type: ignore[assignment,misc]
    VersionInfo = None  # type: ignore[assignment,misc]
    Counter = None  # type: ignore[assignment,misc]
    AtvvCapabilities = None  # type: ignore[assignment,misc]
    ImaDecoder = None  # type: ignore[assignment,misc]
    DcHighPassFilter = None  # type: ignore[assignment,misc]
    FrameAccumulator = None  # type: ignore[assignment,misc]
    probe_value_type = None  # type: ignore[assignment,misc]
    probe_shared_ptr = None  # type: ignore[assignment,misc]
    probe_callback = None  # type: ignore[assignment,misc]
    probe_throw = None  # type: ignore[assignment,misc]
    atvv_capabilities_parse = None  # type: ignore[assignment,misc]
    atvv_control_parse = None  # type: ignore[assignment,misc]
    atvv_mic_open_command = None  # type: ignore[assignment,misc]
    atvv_mic_close_command = None  # type: ignore[assignment,misc]
    postprocess = None  # type: ignore[assignment,misc]
    VoiceTriggerMode = None  # type: ignore[assignment,misc]
    VoiceHostAction = None  # type: ignore[assignment,misc]
    VoiceController = None  # type: ignore[assignment,misc]
    VoiceEdgeDebouncer = None  # type: ignore[assignment,misc]
    AtvvSession = None  # type: ignore[assignment,misc]
    PcmFormat = None  # type: ignore[assignment,misc]
    WasapiAudioRoute = None  # type: ignore[assignment,misc]
    InputSourceKind = None  # type: ignore[assignment,misc]
    InputEventKind = None  # type: ignore[assignment,misc]
    SystemAction = None  # type: ignore[assignment,misc]
    ButtonId = None  # type: ignore[assignment,misc]
    ResolvedActionKind = None  # type: ignore[assignment,misc]
    InputEvent = None  # type: ignore[assignment,misc]
    ResolvedAction = None  # type: ignore[assignment,misc]
    IInputSource = None  # type: ignore[assignment,misc]
    FakeInputSource = None  # type: ignore[assignment,misc]
    IHostActionSink = None  # type: ignore[assignment,misc]
    FakeHostActionSink = None  # type: ignore[assignment,misc]
    ActionResolver = None  # type: ignore[assignment,misc]
    DefaultActionResolver = None  # type: ignore[assignment,misc]
    HotkeyPhysicalizer = None  # type: ignore[assignment,misc]
    WinRTBleTransport = None  # type: ignore[assignment,misc]
    CoordinatorState = None  # type: ignore[assignment,misc]
    CoordinatorCommandKind = None  # type: ignore[assignment,misc]
    CoordinatorCommandStatus = None  # type: ignore[assignment,misc]
    CoordinatorEventKind = None  # type: ignore[assignment,misc]
    CoordinatorCommandResult = None  # type: ignore[assignment,misc]
    ApplicationCoordinator = None  # type: ignore[assignment,misc]
    WindowsVoiceAudioPolicyLease = None  # type: ignore[assignment,misc]
    UiSettingsState = None  # type: ignore[assignment,misc]


# ``CounterSink`` is the C++ typedef used by ``probe_callback``'s argument
# signature (a ``std::function<void(std::int64_t)>``). pybind11 auto-handles
# std::function in function signatures without needing a separate
# ``py::class_`` registration, so it is intentionally NOT re-exported here.

__all__ = [
    "_C_AVAILABLE",
    "__version__",
    "ErrorCode",
    "RemoteMicError",
    "VersionInfo",
    "Counter",
    "AtvvCapabilities",
    "ImaDecoder",
    "DcHighPassFilter",
    "FrameAccumulator",
    "atvv_capabilities_parse",
    "atvv_control_parse",
    "atvv_mic_open_command",
    "atvv_mic_close_command",
    "postprocess",
    "probe_value_type",
    "probe_shared_ptr",
    "probe_callback",
    "probe_throw",
    "VoiceTriggerMode",
    "VoiceHostAction",
    "VoiceController",
    "VoiceEdgeDebouncer",
    "AtvvSession",
    "PcmFormat",
    "WasapiAudioRoute",
    "InputSourceKind",
    "InputEventKind",
    "SystemAction",
    "ButtonId",
    "ResolvedActionKind",
    "InputEvent",
    "ResolvedAction",
    "IInputSource",
    "FakeInputSource",
    "IHostActionSink",
    "FakeHostActionSink",
    "ActionResolver",
    "DefaultActionResolver",
    "HotkeyPhysicalizer",
    "WinRTBleTransport",
    "CoordinatorState",
    "CoordinatorCommandKind",
    "CoordinatorCommandStatus",
    "CoordinatorEventKind",
    "CoordinatorCommandResult",
    "ApplicationCoordinator",
    "WindowsVoiceAudioPolicyLease",
    "UiSettingsState",
]
