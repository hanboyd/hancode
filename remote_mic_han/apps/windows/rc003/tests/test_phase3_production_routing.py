"""Phase 3 / ADR-0013 §5 production routing tests.

Verifies the three production call sites route through the Phase 3
factories instead of constructing the python classes directly:

  1. ``app.py``                    -> ``make_voice_controller(...)``
  2. ``app.py``                    -> ``make_voice_edge_debouncer(...)``
  3. ``ble_transport_winrt.py``    -> ``make_atvv_session(...)``

Invariants verified here:

  * Default choice (no env vars) = ``python`` -> ordinary users get the
    python baseline unchanged (the migration plan §1 rule 4 / ADR-0011
    fallback). No native code is silently constructed alongside.
  * Setting the three ``REMOTEMIC_NATIVE_CHOICE_*`` keys to ``native``
    routes the real product path through the bridge shim. The shim
    holds exactly one ``_impl``; no parallel python instance is also
    constructed.
  * Unsetting the env vars and rebuilding the app returns it to the
    python baseline. No residue.
  * Each factory call returns a fresh instance (single-session owner
    contract; reconnect / cleanup never reuses a stale session).

Production / test parity:

  In production, the user sets the env vars BEFORE launching
  ``python -m ovb_rc003``. The factory functions are bound at
  module-load time (``make_voice_controller = choose_implementation(...)``
  in the factory module's top-level scope). To exercise the
  native-path branches, these tests set env vars in ``setUp`` and then
  ``importlib.reload`` the three factory modules + their consumers
  (``app``, ``ble_transport_winrt``) so the rebinding takes effect -
  exactly the order production sees, but driven from inside the test
  process instead of from the shell that launched it. ``tearDown``
  unsets the env vars and reloads the modules again to restore the
  python baseline; the next test then sees a clean state.

  Because ``importlib.reload`` re-creates the wrapper classes
  (``_NativeVoiceController`` etc.) as new objects, any test that
  compares an instance to one of those classes must look up the class
  reference through the (reloaded) module AFTER reload, not the cached
  top-of-file import. See ``_shim_classes()``.

C++ binding availability:

  The native shim silently falls back to its python baseline when
  ``remotemic_native._C_AVAILABLE`` is False (i.e. ``_C.pyd`` is not
  built on this machine - the normal case for a developer who has not
  built the C++ extension locally). The factory still routes correctly;
  only the final hop from the shim to the C++ instance is replaced by
  the python class. Tests assert the shim class was constructed
  (proves dispatch routing) and separately check ``_is_native`` to
  prove whether the C++ side actually ran - so a real Windows runner
  with ``_C.pyd`` built proves the full native path, and a dev box
  without it still proves the routing wiring is correct.

Env-leak safety: every env override is set inside setUp and restored in
tearDown (NOT at module top), matching the corrective fix pattern from
commit 5ce9bd5. The temp config root + logging-handler cleanup mirrors
the ``_AppWiringTestCase`` pattern from ``test_app_wiring.py``
(XRBM-023 / XRBM-026).
"""

from __future__ import annotations

import asyncio
import importlib
import logging
import os
import tempfile
import unittest
from pathlib import Path

from ovb_rc003 import (
    atvv_session,
    config,
    logging_setup,
    voice_controller,
    voice_edge_debouncer,
)
from ovb_rc003.ble_transport_winrt import RC003BleSession


_PHASE3_KEYS: tuple[str, ...] = (
    "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER",
    "REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER",
    "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION",
)

_RELOAD_TARGETS: tuple[str, ...] = (
    "ovb_rc003.voice_controller_native",
    "ovb_rc003.voice_edge_debouncer_native",
    "ovb_rc003.atvv_session_native",
    "ovb_rc003.app",
    "ovb_rc003.ble_transport_winrt",
)


def _shim_classes() -> tuple[type, type, type]:
    """Return the current (post-reload) wrapper classes. Importing via
    ``importlib.import_module`` rather than the top-of-file ``from ...
    import`` is mandatory: ``importlib.reload`` re-creates the class
    object, so a cached reference from the original import would never
    match instances built after a reload."""
    vc_native = importlib.import_module("ovb_rc003.voice_controller_native")
    ed_native = importlib.import_module("ovb_rc003.voice_edge_debouncer_native")
    atvv_native = importlib.import_module("ovb_rc003.atvv_session_native")
    return (
        vc_native._NativeVoiceController,
        ed_native._NativeVoiceEdgeDebouncer,
        atvv_native._NativeATVVSession,
    )


def _build_app(tmp_root: Path):
    """Redirect config_root at a throwaway temp dir so RC003App.__init__
    can complete without touching the real machine's config / log
    locations. Mirrors ``test_app_wiring.py:_build_app``."""
    app_mod = importlib.import_module("ovb_rc003.app")
    original = config.config_root
    config.config_root = lambda: tmp_root
    try:
        return app_mod.RC003App()
    finally:
        config.config_root = original


def _build_ble_session() -> "tuple[RC003BleSession, asyncio.AbstractEventLoop]":
    """``RC003BleSession.__init__`` does not touch any WinRT API; only
    ``connect()`` does. Build it off-Windows with a no-op
    ``on_pcm_frame`` and an owned event loop (XRBM-026-style). The
    caller owns the loop and must close it after inspecting
    ``sess.session``."""
    bt_mod = importlib.import_module("ovb_rc003.ble_transport_winrt")
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    sess = bt_mod.RC003BleSession(on_pcm_frame=lambda samples: None)
    return sess, loop


def _reload_phase3_modules() -> None:
    """Reload the three factory modules + their consumers so the
    module-level ``make_*`` function objects are re-bound under the
    CURRENT ``REMOTEMIC_NATIVE_CHOICE_*`` env values. Call this after
    setting (or unsetting) those env vars to make the change effective
    in the running process. Mirrors what production gets implicitly
    by launching ``python -m ovb_rc003`` after exporting the env vars."""
    for name in _RELOAD_TARGETS:
        importlib.import_module(name)
        importlib.reload(importlib.import_module(name))


class _EnvCase(unittest.TestCase):
    """Base: snapshot+restore Phase 3 env vars + own a temp config root
    + own a per-test event loop. Subclasses override setUp to set the
    env vars (and reload the modules) after calling ``super().setUp()``.
    """

    def setUp(self) -> None:
        self._snap = {k: os.environ.get(k) for k in _PHASE3_KEYS}
        for k in _PHASE3_KEYS:
            os.environ.pop(k, None)
        _reload_phase3_modules()
        self._tmp = tempfile.TemporaryDirectory()
        # XRBM-026: own a per-test event loop so ConnectionSupervisor's
        # ``loop or asyncio.get_event_loop()`` capture inside
        # RC003App.__init__ binds to a loop we control.
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)

    def tearDown(self) -> None:
        # Unset env, reload, so any later test that imports these
        # modules gets the python baseline again.
        for k in _PHASE3_KEYS:
            os.environ.pop(k, None)
        _reload_phase3_modules()
        # Restore the original env from snapshot (in case the calling
        # process had any of these set).
        for k, v in self._snap.items():
            if v is not None:
                os.environ[k] = v
        # XRBM-023: close logging handler so Windows can clean up the
        # temp dir.
        logger = logging.getLogger(logging_setup.LOGGER_NAME)
        for handler in list(logger.handlers):
            handler.close()
            logger.removeHandler(handler)
        logging_setup._configured = False
        self._tmp.cleanup()
        asyncio.set_event_loop(None)
        self._loop.close()

    def _build_app(self):
        return _build_app(Path(self._tmp.name))


class DefaultDispatchTests(_EnvCase):
    """No env vars set: production path returns python baseline."""

    def test_app_voice_defaults_to_python_baseline(self) -> None:
        app = self._build_app()
        NativeVC, _, _ = _shim_classes()
        self.assertIsInstance(app._voice, voice_controller.VoiceController)
        self.assertNotIsInstance(app._voice, NativeVC)

    def test_app_edge_debouncer_defaults_to_python_baseline(self) -> None:
        app = self._build_app()
        _, NativeED, _ = _shim_classes()
        self.assertIsInstance(
            app._voice_edge_debouncer, voice_edge_debouncer.VoiceEdgeDebouncer
        )
        self.assertNotIsInstance(
            app._voice_edge_debouncer, NativeED
        )

    def test_ble_session_defaults_to_python_baseline(self) -> None:
        sess, loop = _build_ble_session()
        try:
            _, _, NativeATVV = _shim_classes()
            self.assertIsInstance(sess.session, atvv_session.ATVVSession)
            self.assertNotIsInstance(sess.session, NativeATVV)
        finally:
            asyncio.set_event_loop(None)
            loop.close()
            asyncio.set_event_loop(self._loop)


class NativeDispatchTests(_EnvCase):
    """All three env vars = ``native``: production path returns the bridge
    shim. The shim itself either wraps a C++ ``_impl`` (when
    ``_C.pyd`` is built) or transparently falls back to the python
    baseline (when not). Either way the shim CLASS is constructed -
    that proves the factory routed correctly. ``_is_native`` on the
    shim distinguishes the two outcomes for the C++-build assertion."""

    def setUp(self) -> None:
        super().setUp()
        for k in _PHASE3_KEYS:
            os.environ[k] = "native"
        _reload_phase3_modules()

    def test_app_voice_routes_through_native_shim(self) -> None:
        app = self._build_app()
        NativeVC, _, _ = _shim_classes()
        self.assertIsInstance(app._voice, NativeVC)

    def test_app_edge_debouncer_routes_through_native_shim(self) -> None:
        app = self._build_app()
        _, NativeED, _ = _shim_classes()
        self.assertIsInstance(app._voice_edge_debouncer, NativeED)

    def test_ble_session_routes_through_native_shim(self) -> None:
        sess, loop = _build_ble_session()
        try:
            _, _, NativeATVV = _shim_classes()
            self.assertIsInstance(sess.session, NativeATVV)
        finally:
            asyncio.set_event_loop(None)
            loop.close()
            asyncio.set_event_loop(self._loop)

    def test_voice_shim_is_native_when_cpp_binding_is_built(self) -> None:
        # When ``_C.pyd`` is built and available, the shim must
        # actually reach the C++ side (``_is_native == True``) instead
        # of silently falling back to the python baseline. A developer
        # running these tests without ``_C.pyd`` skips this assertion
        # but still proves the factory routing above.
        app = self._build_app()
        NativeVC, _, _ = _shim_classes()
        if not isinstance(app._voice, NativeVC):
            self.skipTest(
                "voice factory did not route to native shim; "
                "check REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"
            )
        import remotemic_native as _rn

        if not getattr(_rn, "_C_AVAILABLE", False):
            self.skipTest(
                "remotemic_native._C not available on this machine; "
                "re-run on a Windows runner with _C.pyd built"
            )
        self.assertTrue(getattr(app._voice, "_is_native", False))


class RestoreAfterUnsetTests(_EnvCase):
    """Set native (proves native path works), then unset env and reload
    (proves restore to python baseline)."""

    def test_app_returns_to_python_baseline_after_env_unset(self) -> None:
        for k in _PHASE3_KEYS:
            os.environ[k] = "native"
        _reload_phase3_modules()
        NativeVC, NativeED, _ = _shim_classes()
        native_app = self._build_app()
        self.assertIsInstance(native_app._voice, NativeVC)
        self.assertIsInstance(native_app._voice_edge_debouncer, NativeED)

        for k in _PHASE3_KEYS:
            os.environ.pop(k, None)
        _reload_phase3_modules()
        NativeVC, NativeED, _ = _shim_classes()
        py_app = self._build_app()
        self.assertIsInstance(py_app._voice, voice_controller.VoiceController)
        self.assertNotIsInstance(py_app._voice, NativeVC)
        self.assertIsInstance(
            py_app._voice_edge_debouncer, voice_edge_debouncer.VoiceEdgeDebouncer
        )
        self.assertNotIsInstance(py_app._voice_edge_debouncer, NativeED)

    def test_ble_session_returns_to_python_baseline_after_env_unset(self) -> None:
        for k in _PHASE3_KEYS:
            os.environ[k] = "native"
        _reload_phase3_modules()
        _, _, NativeATVV = _shim_classes()
        native_sess, native_loop = _build_ble_session()
        try:
            self.assertIsInstance(native_sess.session, NativeATVV)
        finally:
            asyncio.set_event_loop(None)
            native_loop.close()
            asyncio.set_event_loop(self._loop)

        for k in _PHASE3_KEYS:
            os.environ.pop(k, None)
        _reload_phase3_modules()
        _, _, NativeATVV = _shim_classes()
        py_sess, py_loop = _build_ble_session()
        try:
            self.assertIsInstance(py_sess.session, atvv_session.ATVVSession)
            self.assertNotIsInstance(py_sess.session, NativeATVV)
        finally:
            asyncio.set_event_loop(None)
            py_loop.close()
            asyncio.set_event_loop(self._loop)


class SingleOwnerTests(_EnvCase):
    """No shadow dual-owner: under native, the wrapper holds exactly one
    ``_impl``; under python, no native side is silently constructed
    alongside. Fresh-instance contract: each factory call returns a new
    object so reconnect / cleanup never reuses a stale session."""

    def test_native_shim_holds_exactly_one_impl(self) -> None:
        for k in _PHASE3_KEYS:
            os.environ[k] = "native"
        _reload_phase3_modules()
        app = self._build_app()
        # Each bridge shim stores exactly one wrapped instance on
        # ``_impl`` (C++ if ``_C.pyd`` is built, python baseline
        # otherwise - both are still a single owner).
        self.assertTrue(hasattr(app._voice, "_impl"))
        self.assertTrue(hasattr(app._voice_edge_debouncer, "_impl"))

    def test_ble_native_shim_holds_exactly_one_impl(self) -> None:
        for k in _PHASE3_KEYS:
            os.environ[k] = "native"
        _reload_phase3_modules()
        sess, loop = _build_ble_session()
        try:
            self.assertTrue(hasattr(sess.session, "_impl"))
        finally:
            asyncio.set_event_loop(None)
            loop.close()
            asyncio.set_event_loop(self._loop)

    def test_python_baseline_has_no_native_side_constructed(self) -> None:
        # Default env: no native shim is built; the python class is the
        # only thing that exists.
        app = self._build_app()
        self.assertFalse(hasattr(app._voice, "_impl"))
        self.assertFalse(hasattr(app._voice_edge_debouncer, "_impl"))

    def test_ble_python_baseline_has_no_native_side_constructed(self) -> None:
        sess, loop = _build_ble_session()
        try:
            self.assertFalse(hasattr(sess.session, "_impl"))
        finally:
            asyncio.set_event_loop(None)
            loop.close()
            asyncio.set_event_loop(self._loop)

    def test_fresh_factory_calls_produce_independent_instances(self) -> None:
        # Single-session owner contract: each make_*() call returns a
        # fresh object; two RC003Apps must never share a controller /
        # debouncer.
        a = self._build_app()
        b = self._build_app()
        self.assertIsNot(a._voice, b._voice)
        self.assertIsNot(a._voice_edge_debouncer, b._voice_edge_debouncer)
        # State isolation: a's mutations must not be visible from b.
        a._voice.on_mic_button_pressed()
        self.assertTrue(a._voice.active)
        self.assertFalse(b._voice.active)


class ProductionSourceRoutingTests(_EnvCase):
    """Source-level proof that the production call sites reference the
    factory functions and not the python classes directly. Defends
    against a future regression that re-introduces a direct class
    reference (the Phase 3 routing gap this whole change closed).
    """

    def test_app_init_does_not_construct_voice_controller_directly(self) -> None:
        import inspect
        app_mod = importlib.import_module("ovb_rc003.app")
        src = inspect.getsource(app_mod.RC003App.__init__)
        self.assertNotIn("voice_controller.VoiceController(", src)
        self.assertIn("make_voice_controller(", src)

    def test_app_init_does_not_construct_edge_debouncer_directly(self) -> None:
        import inspect
        app_mod = importlib.import_module("ovb_rc003.app")
        src = inspect.getsource(app_mod.RC003App.__init__)
        self.assertNotIn("voice_edge_debouncer.VoiceEdgeDebouncer(", src)
        self.assertIn("make_voice_edge_debouncer(", src)

    def test_ble_session_init_does_not_construct_atvv_session_directly(self) -> None:
        import inspect
        bt_mod = importlib.import_module("ovb_rc003.ble_transport_winrt")
        src = inspect.getsource(bt_mod.RC003BleSession.__init__)
        self.assertNotIn("atvv_session.ATVVSession(", src)
        self.assertIn("make_atvv_session(", src)


if __name__ == "__main__":
    unittest.main()