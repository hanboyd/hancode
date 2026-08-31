"""Phase 4 / ADR-0014 §6 step 5: production routing tests for
``audio_route`` (G5 of ADR-0014).

Mirrors ``test_phase3_production_routing.py``: source-level proof
that the production call site (``RC003App._open_playback_for_new_session``
in ``app.py``) references the factory function
``make_audio_route(...)`` and not the python class
``audio_playback.EndpointPlaybackSink`` directly.

This is the regression-proof: if a future commit re-introduces a
direct class reference (the Phase 3 routing gap that ``8cc0c4c``
closed for the voice / edge-debouncer / atvv-session trio), this
test fails loudly. The Phase 4 equivalent closes the same gap for
the audio playback path so that
``REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native`` actually reaches
the C++ ``WasapiAudioRoute``.

Pure inspection - no env-var manipulation, no real audio device
required. Mirrors the ``ProductionSourceRoutingTests`` class in
``test_phase3_production_routing.py``.
"""

from __future__ import annotations

import importlib
import inspect
import unittest


class ProductionSourceRoutingTests(unittest.TestCase):
    """Source-level proof that ``app.py`` references the factory
    function and not the python class directly.
    """

    def test_app_open_playback_references_make_audio_route(self) -> None:
        app_mod = importlib.import_module("ovb_rc003.app")
        src = inspect.getsource(app_mod.RC003App._open_playback_for_new_session)
        # The factory call must be present.
        self.assertIn("make_audio_route(", src)
        # The direct python class construction must NOT be present
        # anywhere in this method (the whole reason this test
        # exists is to defend against that regression).
        self.assertNotIn(
            "audio_playback.EndpointPlaybackSink(", src,
            "_open_playback_for_new_session must route through "
            "make_audio_route factory, not the python class "
            "directly. The Phase 3 / 8cc0c4c precedent: production "
            "call sites must NEVER construct the python baseline "
            "directly, otherwise REMOTEMIC_NATIVE_CHOICE_*=native "
            "is silently bypassed.",
        )

    def test_app_module_imports_make_audio_route(self) -> None:
        # The factory module must be imported at module level so
        # the rebinding at import time sees the env var.
        app_mod = importlib.import_module("ovb_rc003.app")
        src = inspect.getsource(app_mod)
        self.assertIn("from .audio_route_native import", src)
        self.assertIn("make_audio_route", src)

    def test_app_module_does_not_instantiate_endpoint_playback_sink(self) -> None:
        # Defense in depth: a grep-style check across the entire
        # app.py source. Production call sites for the audio
        # playback must go through the factory exclusively.
        app_mod = importlib.import_module("ovb_rc003.app")
        src = inspect.getsource(app_mod)
        self.assertNotIn(
            "audio_playback.EndpointPlaybackSink(",
            src,
            "app.py must not construct EndpointPlaybackSink "
            "directly; route through make_audio_route factory.",
        )

    def test_app_playback_attr_is_untyped(self) -> None:
        # The ``_playback`` attribute is intentionally untyped
        # because under ``native`` it holds a ``_NativeAudioRoute``
        # (wrapping ``WasapiAudioRoute`` or the python fallback),
        # and under ``python`` it holds ``EndpointPlaybackSink``.
        # The type annotation was removed in step 5 - the comment
        # in app.py documents the reasoning. This test asserts
        # the annotation is absent so a future "cleanup" doesn't
        # re-introduce it without considering the dispatch surface.
        app_mod = importlib.import_module("ovb_rc003.app")
        src = inspect.getsource(app_mod.RC003App.__init__)
        # The line containing ``self._playback`` must NOT carry
        # an ``: Optional[audio_playback.EndpointPlaybackSink]``
        # type annotation.
        for line in src.splitlines():
            stripped = line.strip()
            if stripped.startswith("self._playback") and "=" in stripped:
                self.assertNotIn(
                    "Optional[audio_playback.EndpointPlaybackSink]",
                    line,
                    "_playback attribute must remain untyped; under "
                    "native it holds _NativeAudioRoute, not the "
                    "python EndpointPlaybackSink.",
                )


if __name__ == "__main__":
    unittest.main()
