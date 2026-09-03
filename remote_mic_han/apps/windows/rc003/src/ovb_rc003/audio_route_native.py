"""Phase 4 / ADR-0014 §6: module-level switch for IAudioRoute.

Exposes ``make_audio_route(endpoint_name, host_api_name="") -> IAudioRoute``
which dispatches via ``choose_implementation`` to either:

  * ``python``: ``ovb_rc003.audio_playback.EndpointPlaybackSink`` (the
    pure-Python PortAudio-backed implementation; the single-session
    owner in production today)
  * ``native``: ``remotemic_native._C.WasapiAudioRoute`` (the pybind11
    binding; the C++ side owns the WASAPI device handle and the
    BoundedPcmQueue)

The default is ``python`` (per migration plan §1 rule 4). Switch via:

    REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native
    REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=python   # default

``shadow`` is NOT supported here per plan §3 rule 5: WASAPI is
side-effecting (a real device handle is opened / closed), so a python
shadow would actually open *two* audio streams at once, distorting
the user-facing latency measurement. The parity test (step 4) uses
``FakeAudioRoute`` to drive both python and native through an
in-process recording harness, which has no side effects.

Both python and native returns share the same Python surface
(``open() / drain() / write() / stop() / close()``), translated 1:1
to the underlying ``IAudioRoute`` lifecycle per ADR-0014 §4.
"""

from __future__ import annotations

from typing import List

from . import audio_playback as py_mod
from ._remotemic_native_runtime import choose_implementation


class _NativeAudioRoute:
    """Thin shim over ``remotemic_native._C.WasapiAudioRoute`` that exposes
    the same Python surface as
    ``ovb_rc003.audio_playback.EndpointPlaybackSink`` so the bridge
    wrapper can stay implementation-agnostic.

    Native semantics map 1:1 onto the C++ IAudioRoute:

      * open()             -> start(PcmFormat)
      * write(samples)     -> write(span<int16>)
      * drain(timeout_s)   -> drain(milliseconds(timeout_s * 1000))
      * close()            -> stop() + close() (idempotent)
    """

    def __init__(self, endpoint_name: str, host_api_name: str = "") -> None:
        import remotemic_native as _rn  # type: ignore[import-not-found]

        if not _rn._C_AVAILABLE:
            self._impl = py_mod.EndpointPlaybackSink(
                endpoint_name, host_api_name
            )
            self._is_native = False
            return
        self._impl = _rn.WasapiAudioRoute(endpoint_name, host_api_name)
        self._is_native = True

    def open(self) -> None:
        if not self._is_native:
            self._impl.open()
            return
        # PcmFormat: 16 kHz, mono, int16 (matches BLE ATVV source).
        fmt = _rn_module().PcmFormat()
        ok = self._impl.start(fmt)
        if not ok:
            from .audio_output import AudioOutputUnavailableError

            raise AudioOutputUnavailableError(
                f"failed to open WASAPI route "
                f"(endpoint_name={self._impl.current_format()!r})"
            )

    def write(self, samples: List[int]) -> None:
        # pybind11 accepts a Python list of int and copies into a
        # std::vector<int16_t>; the C++ side handles drop-oldest
        # internally.
        result = self._impl.write(list(samples))
        if self._is_native and result is False:
            from .audio_output import AudioOutputUnavailableError

            raise AudioOutputUnavailableError(
                "native WASAPI route rejected PCM write"
            )

    def drain(self, timeout_seconds: float) -> None:
        self._impl.drain(int(timeout_seconds * 1000))

    def close(self) -> None:
        self._impl.close()


def _rn_module():
    import remotemic_native as _rn  # type: ignore[import-not-found]

    return _rn


def _make_audio_route_python(
    endpoint_name: str, host_api_name: str = ""
) -> py_mod.EndpointPlaybackSink:
    sink = py_mod.EndpointPlaybackSink(endpoint_name, host_api_name)
    sink.open()
    return sink


def _make_audio_route_native(
    endpoint_name: str, host_api_name: str = ""
) -> _NativeAudioRoute:
    route = _NativeAudioRoute(endpoint_name, host_api_name)
    route.open()
    return route


make_audio_route_python = _make_audio_route_python
make_audio_route_native = _make_audio_route_native


# Module-level binding (Phase 3 / ADR-0011 pattern): the env var is
# captured AT IMPORT TIME so the chosen factory is fixed for the
# lifetime of the process. Production pattern: launch
# ``python -m ovb_rc003`` AFTER exporting the env var, exactly the
# same as Phase 3's ``make_voice_controller`` /
# ``make_atvv_session`` binding. Tests use ``importlib.reload`` to
# re-capture after mutating the env var mid-process.
make_audio_route = choose_implementation(
    "audio_route",
    python_impl=_make_audio_route_python,
    native_impl=_make_audio_route_native,
    # shadow is forbidden per plan §3 rule 5 (real WASAPI device
    # handle), so side_effect_free stays False. ``choose_implementation``
    # will reject any attempt to dispatch into shadow mode at runtime.
    side_effect_free=False,
)


__all__ = [
    "make_audio_route",
    "make_audio_route_python",
    "make_audio_route_native",
]
