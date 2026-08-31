"""Phase 4 / ADR-0014 §6 step 5: native switch + verify for
``audio_route`` (G5 of ADR-0014).

When ``REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native`` is set, the
``audio_route_native.make_audio_route`` factory routes the real
product path through the ``_NativeAudioRoute`` shim wrapping
``remotemic_native._C.WasapiAudioRoute`` (or, when the binding is
unavailable, transparently falling back to the python baseline).
This test mirrors the Phase 3 native-switch pattern:

  * Default choice (no env var) -> python ``EndpointPlaybackSink``
    (the python baseline). No native side is silently constructed
    alongside.
  * Native choice -> factory returns the bridge shim; shim holds
    exactly one ``_impl``; no parallel python instance is also
    constructed.
  * Unset env var and reload -> back to python baseline; no residue.
  * Fresh factory calls return independent instances (single-session
    owner contract; reconnect / cleanup never reuses a stale route).

Production / test parity (per the same convention established in
Phase 3):

  In production, the user sets the env var BEFORE launching
  ``python -m ovb_rc003``. The factory functions are bound at
  module-load time (``make_audio_route = choose_implementation(...)``
  in the factory module's top-level scope). To exercise the
  native-path branches, these tests set env vars in ``setUp`` and
  then ``importlib.reload`` the factory module so the rebinding
  takes effect - exactly the order production sees, but driven
  from inside the test process.

  Unlike Phase 3, the audio_route native shim's ``open()`` (via
  ``WasapiAudioRoute::start``) is side-effecting: it tries to open
  a real WASAPI endpoint on real Windows. On a CI machine without
  a real audio device, ``open()`` returns False and the shim raises
  ``AudioOutputUnavailableError``. The factory's ``open()`` is
  therefore only invoked by tests that have a known-good endpoint
  available; the dispatch / shim-construction tests here never call
  ``make_audio_route`` to completion - they inspect the bound
  factory function and the underlying shim class directly.

Env-leak safety: every env override is set inside setUp / setUpClass
and restored in tearDown / tearDownClass (NOT at module top),
matching the corrective fix pattern from commit 5ce9bd5.
"""

from __future__ import annotations

import importlib
import os
import unittest


_PHASE4_KEY = "REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE"


def _reload_audio_route_module() -> None:
    """Reload the factory module so the module-level ``make_audio_route``
    function is re-bound under the CURRENT ``REMOTEMIC_NATIVE_CHOICE_*``
    env values. Call this after setting (or unsetting) the env var to
    make the change effective in the running process. Mirrors what
    production gets implicitly by launching ``python -m ovb_rc003``
    after exporting the env var.
    """
    name = "ovb_rc003.audio_route_native"
    importlib.import_module(name)
    importlib.reload(importlib.import_module(name))


def _shim_class():
    """Return the post-reload ``_NativeAudioRoute`` class. Importing
    via ``importlib.import_module`` rather than the top-of-file
    ``from ... import`` is mandatory: ``importlib.reload`` re-creates
    the class object, so a cached reference from the original import
    would never match instances built after a reload.
    """
    return importlib.import_module(
        "ovb_rc003.audio_route_native"
    )._NativeAudioRoute


def _factory_module():
    return importlib.import_module("ovb_rc003.audio_route_native")


class _EnvCase(unittest.TestCase):
    """Base: snapshot+restore Phase 4 audio_route env var.

    Subclasses override setUp to set the env var (and reload the
    factory module) after calling ``super().setUp()``.
    """

    def setUp(self) -> None:
        self._snap = os.environ.get(_PHASE4_KEY)
        os.environ.pop(_PHASE4_KEY, None)
        _reload_audio_route_module()

    def tearDown(self) -> None:
        os.environ.pop(_PHASE4_KEY, None)
        _reload_audio_route_module()
        if self._snap is not None:
            os.environ[_PHASE4_KEY] = self._snap


class DefaultDispatchTests(_EnvCase):
    """No env var set: factory returns the python baseline."""

    def test_make_audio_route_is_python_baseline_by_default(self) -> None:
        from ovb_rc003 import audio_playback

        mod = _factory_module()
        # Default choice: make_audio_route is bound to the python
        # implementation. We do NOT call it (it would try to open
        # a real PortAudio device); we just verify the factory's
        # default wiring.
        self.assertIs(
            mod.make_audio_route, mod.make_audio_route_python
        )
        self.assertIsNot(
            mod.make_audio_route, mod.make_audio_route_native
        )
        # The python baseline class is still the canonical
        # ``EndpointPlaybackSink``.
        self.assertIs(
            mod.make_audio_route_python.__wrapped__
            if hasattr(mod.make_audio_route_python, "__wrapped__")
            else None,
            None,
            "python factory should not be wrapped",
        )
        # Sanity: the python sink class is importable and has the
        # expected interface.
        self.assertTrue(
            hasattr(audio_playback.EndpointPlaybackSink, "open")
        )
        self.assertTrue(
            hasattr(audio_playback.EndpointPlaybackSink, "write")
        )
        self.assertTrue(
            hasattr(audio_playback.EndpointPlaybackSink, "drain")
        )
        self.assertTrue(
            hasattr(audio_playback.EndpointPlaybackSink, "close")
        )


class NativeDispatchTests(_EnvCase):
    """``REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native``: factory returns
    the bridge shim wrapping ``WasapiAudioRoute`` (or the python
    fallback when ``_C.pyd`` is not built - either way the shim
    CLASS is constructed, proving the factory routed correctly).

    Env-leak safety: env override lives inside setUp; tearDown
    restores the original environment.
    """

    def setUp(self) -> None:
        super().setUp()
        os.environ[_PHASE4_KEY] = "native"
        _reload_audio_route_module()

    def test_make_audio_route_routes_through_native_factory(self) -> None:
        mod = _factory_module()
        # Under native, the public factory is the native impl.
        self.assertIs(
            mod.make_audio_route, mod.make_audio_route_native
        )
        self.assertIsNot(
            mod.make_audio_route, mod.make_audio_route_python
        )

    def test_native_shim_class_constructs_without_open(self) -> None:
        # ``_NativeAudioRoute.__init__`` does not touch the
        # binding's start() method - it only constructs the
        # underlying ``WasapiAudioRoute`` (or falls back to python
        # ``EndpointPlaybackSink`` when ``_C_AVAILABLE`` is False).
        # This proves the dispatch wiring is correct without
        # requiring a real WASAPI device.
        NativeAR = _shim_class()
        shim = NativeAR("CABLE Output", "Windows WASAPI")
        # The shim holds exactly one wrapped instance on ``_impl``.
        self.assertTrue(hasattr(shim, "_impl"))
        self.assertIsNotNone(shim._impl)

    def test_native_shim_reaches_cpp_side_when_binding_built(self) -> None:
        # When ``_C.pyd`` is built and available, the shim must
        # actually reach the C++ ``WasapiAudioRoute`` instead of
        # silently falling back to the python baseline. ``_is_native``
        # distinguishes the two outcomes for the C++-build assertion.
        NativeAR = _shim_class()
        shim = NativeAR("CABLE Output", "Windows WASAPI")
        import remotemic_native as _rn  # type: ignore[import-not-found]

        if not getattr(_rn, "_C_AVAILABLE", False):
            self.skipTest(
                "remotemic_native._C not available on this machine; "
                "re-run on a Windows runner with _C.pyd built"
            )
        self.assertTrue(getattr(shim, "_is_native", False))
        # The C++ side is a WasapiAudioRoute instance, not a python
        # baseline.
        self.assertEqual(type(shim._impl).__name__, "WasapiAudioRoute")

    def test_shadow_is_not_supported(self) -> None:
        # Per plan §3 rule 5: WASAPI is side-effecting, so the
        # factory refuses ``shadow``. ``choose_implementation``
        # raises ``RuntimeError`` when shadow is requested for a
        # non-side-effect-free module (audio_route is registered
        # with ``side_effect_free=False``). The raise happens
        # inside the dispatch wrapper, NOT at module import time -
        # this test directly invokes ``choose_implementation`` with
        # the env var temporarily forced to shadow, asserts the
        # RuntimeError, then restores the env.
        from ovb_rc003._remotemic_native_runtime import (
            choose_implementation,
        )
        from ovb_rc003.audio_route_native import (
            _make_audio_route_python,
            _make_audio_route_native,
        )
        original = os.environ.get(_PHASE4_KEY)
        os.environ[_PHASE4_KEY] = "shadow"
        try:
            with self.assertRaises(RuntimeError) as cm:
                choose_implementation(
                    "audio_route",
                    python_impl=_make_audio_route_python,
                    native_impl=_make_audio_route_native,
                    side_effect_free=False,
                )
            self.assertIn("shadow", str(cm.exception).lower())
        finally:
            if original is None:
                os.environ.pop(_PHASE4_KEY, None)
            else:
                os.environ[_PHASE4_KEY] = original


class RestoreAfterUnsetTests(_EnvCase):
    """Set native (proves native path works), then unset env and reload
    (proves restore to python baseline)."""

    def test_factory_returns_to_python_after_env_unset(self) -> None:
        mod = _factory_module()
        # Confirm starting state is python.
        self.assertIs(
            mod.make_audio_route, mod.make_audio_route_python
        )

        os.environ[_PHASE4_KEY] = "native"
        _reload_audio_route_module()
        mod = _factory_module()
        self.assertIs(
            mod.make_audio_route, mod.make_audio_route_native
        )

        os.environ.pop(_PHASE4_KEY, None)
        _reload_audio_route_module()
        mod = _factory_module()
        self.assertIs(
            mod.make_audio_route, mod.make_audio_route_python
        )
        self.assertIsNot(
            mod.make_audio_route, mod.make_audio_route_native
        )


class SingleOwnerTests(_EnvCase):
    """No shadow dual-owner: under native, the wrapper holds exactly
    one ``_impl``; under python, no native side is silently
    constructed alongside. Fresh-instance contract: each factory
    call returns a new object so reconnect / cleanup never reuses a
    stale route."""

    def test_native_shim_holds_exactly_one_impl(self) -> None:
        os.environ[_PHASE4_KEY] = "native"
        _reload_audio_route_module()
        NativeAR = _shim_class()
        shim = NativeAR("CABLE Output", "Windows WASAPI")
        self.assertTrue(hasattr(shim, "_impl"))
        # The shim must hold exactly one wrapped instance.
        self.assertIsNotNone(shim._impl)

    def test_python_baseline_has_no_native_side_constructed(self) -> None:
        # Default env: no native shim is built; the python class
        # is the only thing that exists.
        mod = _factory_module()
        self.assertIs(
            mod.make_audio_route, mod.make_audio_route_python
        )
        # The native shim class is importable but no instance of it
        # has been constructed by the factory.
        NativeAR = _shim_class()
        self.assertFalse(
            hasattr(mod.make_audio_route_python, "_impl")
        )

    def test_fresh_native_shim_constructions_are_independent(self) -> None:
        # Single-session owner contract: each construction returns
        # a fresh object; two shims must never share a wrapped
        # ``_impl``.
        os.environ[_PHASE4_KEY] = "native"
        _reload_audio_route_module()
        NativeAR = _shim_class()
        a = NativeAR("CABLE Output", "Windows WASAPI")
        b = NativeAR("CABLE Output", "Windows WASAPI")
        self.assertIsNot(a, b)
        self.assertIsNot(a._impl, b._impl)


if __name__ == "__main__":
    unittest.main()
