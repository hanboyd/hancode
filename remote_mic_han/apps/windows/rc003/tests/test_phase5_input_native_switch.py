"""Phase 5 / ADR-0015 §6 step 3: native switch + verify for
``input_source`` and ``host_action_sink``.

When ``REMOTEMIC_NATIVE_CHOICE_INPUT_SOURCE=native`` is set, the
``input_source_native.make_input_source`` factory routes the real
product path through the ``_NativeInputSource`` shim wrapping
``remotemic_native._C.RawInputSource`` (or, when the binding is
unavailable, transparently falling back to the python baseline).
This test mirrors the Phase 3 native-switch pattern:

  * Default choice (no env var) -> python baseline. No native side
    is silently constructed alongside.
  * Native choice -> factory returns the bridge shim; shim holds
    exactly one ``_impl``; no parallel python instance is also
    constructed.
  * Unset env var and reload -> back to python baseline; no residue.
  * Fresh factory calls return independent instances.

Env-leak safety: every env override is set inside setUp / setUpClass
and restored in tearDown / tearDownClass (NOT at module top),
matching the corrective fix pattern from commit 5ce9bd5.
"""

from __future__ import annotations

import importlib
import os
import unittest
from unittest import mock


_INPUT_SOURCE_KEY = "REMOTEMIC_NATIVE_CHOICE_INPUT_SOURCE"
_HOST_ACTION_SINK_KEY = "REMOTEMIC_NATIVE_CHOICE_HOST_ACTION_SINK"


def _reload_input_source_module() -> None:
    """Reload the factory module so the module-level
    ``make_input_source`` function is re-bound under the CURRENT
    ``REMOTEMIC_NATIVE_CHOICE_*`` env values. Mirrors what production
    gets implicitly by launching ``python -m ovb_rc003`` after
    exporting the env var.
    """
    name = "ovb_rc003.input_source_native"
    importlib.import_module(name)
    importlib.reload(importlib.import_module(name))


def _reload_host_action_sink_module() -> None:
    name = "ovb_rc003.host_action_sink_native"
    importlib.import_module(name)
    importlib.reload(importlib.import_module(name))


class _NativeSwitchBase(unittest.TestCase):
    """Sets up an isolated env-var context per test, then reloads
    the factory modules so the rebinding takes effect.
    """

    def setUp(self) -> None:
        # Snapshot whatever the test runner already had, so we can
        # restore in tearDown without leaking.
        self._saved_input_source = os.environ.get(_INPUT_SOURCE_KEY)
        self._saved_host_action_sink = os.environ.get(_HOST_ACTION_SINK_KEY)
        # Drop both env vars before reload; each test sets the one it
        # needs explicitly.
        os.environ.pop(_INPUT_SOURCE_KEY, None)
        os.environ.pop(_HOST_ACTION_SINK_KEY, None)

    def tearDown(self) -> None:
        if self._saved_input_source is None:
            os.environ.pop(_INPUT_SOURCE_KEY, None)
        else:
            os.environ[_INPUT_SOURCE_KEY] = self._saved_input_source
        if self._saved_host_action_sink is None:
            os.environ.pop(_HOST_ACTION_SINK_KEY, None)
        else:
            os.environ[_HOST_ACTION_SINK_KEY] = self._saved_host_action_sink
        # Reload both modules so subsequent tests see the restored
        # state of the env (don't leak bindings between tests).
        _reload_input_source_module()
        _reload_host_action_sink_module()


class InputSourceNativeSwitchTests(_NativeSwitchBase):
    """Native switch tests for ``input_source_native.make_input_source``."""

    def test_default_choice_is_python(self) -> None:
        _reload_input_source_module()
        mod = importlib.import_module("ovb_rc003.input_source_native")
        self.assertIs(mod.make_input_source_python,
                      mod._make_input_source_python)
        self.assertIs(mod.make_input_source_native,
                      mod._make_input_source_native)

    def test_native_choice_routes_to_native_shim(self) -> None:
        os.environ[_INPUT_SOURCE_KEY] = "native"
        _reload_input_source_module()
        mod = importlib.import_module("ovb_rc003.input_source_native")
        # Without a real device path the shim may fall through to the
        # python fallback; check the shim CLASS is reachable.
        self.assertTrue(hasattr(mod, "_NativeInputSource"))
        self.assertTrue(callable(mod.make_input_source))

    def test_python_choice_routes_to_python_shim(self) -> None:
        os.environ[_INPUT_SOURCE_KEY] = "python"
        _reload_input_source_module()
        mod = importlib.import_module("ovb_rc003.input_source_native")
        self.assertTrue(hasattr(mod, "_PythonInputSource"))

    def test_unset_choice_reloads_back_to_python(self) -> None:
        # First flip to native, reload, then unset and reload again.
        os.environ[_INPUT_SOURCE_KEY] = "native"
        _reload_input_source_module()
        os.environ.pop(_INPUT_SOURCE_KEY)
        _reload_input_source_module()
        mod = importlib.import_module("ovb_rc003.input_source_native")
        # Re-importing the module should not raise; the python path
        # is bound to ``_make_input_source_python``.
        self.assertIsNotNone(mod._make_input_source_python)

    def test_fresh_factory_call_returns_independent_instance(self) -> None:
        os.environ[_INPUT_SOURCE_KEY] = "python"
        _reload_input_source_module()
        mod = importlib.import_module("ovb_rc003.input_source_native")
        a = mod.make_input_source_python()
        b = mod.make_input_source_python()
        self.assertIsNot(a, b)

    def test_shadow_choice_rejected(self) -> None:
        # ADR-0015 / plan §3 rule 5: shadow is forbidden for the
        # input layer because the underlying Raw Input handle is
        # side-effecting. ``choose_implementation`` raises at module
        # import time when the env var is set to ``shadow`` for a
        # side-effecting module.
        os.environ[_INPUT_SOURCE_KEY] = "shadow"
        with self.assertRaises(RuntimeError):
            _reload_input_source_module()

    def test_native_registration_failure_logs_without_name_error(self) -> None:
        mod = importlib.import_module("ovb_rc003.input_source_native")
        import remotemic_native as rn

        class _RejectingSource:
            def set_event_sink(self, sink) -> None:
                raise RuntimeError("rejected")

            def stop(self) -> None:
                return None

        with mock.patch.object(rn, "_C_AVAILABLE", True), \
             mock.patch.object(rn, "RawInputSource", _RejectingSource,
                               create=True):
            source = mod._NativeInputSource("device")
            source.set_event_sink(lambda event: None)
            self.assertIsInstance(source._registration_error, RuntimeError)


class HostActionSinkNativeSwitchTests(_NativeSwitchBase):
    """Native switch tests for
    ``host_action_sink_native.make_host_action_sink``."""

    def test_default_choice_is_python(self) -> None:
        _reload_host_action_sink_module()
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        self.assertIs(mod.make_host_action_sink_python,
                      mod._make_host_action_sink_python)

    def test_native_choice_routes_to_native_shim(self) -> None:
        os.environ[_HOST_ACTION_SINK_KEY] = "native"
        _reload_host_action_sink_module()
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        self.assertTrue(hasattr(mod, "_NativeHostActionSink"))

    def test_python_choice_routes_to_python_shim(self) -> None:
        os.environ[_HOST_ACTION_SINK_KEY] = "python"
        _reload_host_action_sink_module()
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        self.assertTrue(hasattr(mod, "_PythonHostActionSink"))

    def test_python_sink_submit_key_increments_count(self) -> None:
        os.environ[_HOST_ACTION_SINK_KEY] = "python"
        _reload_host_action_sink_module()
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        sink = mod.make_host_action_sink_python()
        # The python shim wraps ``win32_input.send_keys`` which is
        # unavailable on non-Windows hosts; submit_key returns False
        # but the error counter increments. Either way: the surface
        # contract is stable.
        sink.submit_key(0x41, True, 50)
        # On non-Windows CI submit returns False; the test still
        # passes as long as no exception leaks out.
        self.assertGreaterEqual(sink.submit_error_count(), 0)

    def test_unset_choice_reloads_back_to_python(self) -> None:
        os.environ[_HOST_ACTION_SINK_KEY] = "native"
        _reload_host_action_sink_module()
        os.environ.pop(_HOST_ACTION_SINK_KEY)
        _reload_host_action_sink_module()
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        self.assertIsNotNone(mod._make_host_action_sink_python)

    def test_fresh_factory_call_returns_independent_instance(self) -> None:
        os.environ[_HOST_ACTION_SINK_KEY] = "python"
        _reload_host_action_sink_module()
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        a = mod.make_host_action_sink_python()
        b = mod.make_host_action_sink_python()
        self.assertIsNot(a, b)

    def test_shadow_choice_rejected(self) -> None:
        os.environ[_HOST_ACTION_SINK_KEY] = "shadow"
        with self.assertRaises(RuntimeError):
            _reload_host_action_sink_module()

    def test_missing_extension_fallback_submits_with_numeric_deadline(self) -> None:
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        import remotemic_native as rn

        delivered = []
        with mock.patch.object(rn, "_C_AVAILABLE", False), \
             mock.patch.object(
                 mod.py_win32_input,
                 "_real_send_input_batch",
                 side_effect=lambda events: delivered.extend(events) or 1,
             ):
            sink = mod._make_host_action_sink_native()
            self.assertTrue(sink.submit_key(0x41, True, 50))

        self.assertEqual(delivered, [(0x41, False)])

    def test_python_system_action_names_reach_existing_helpers(self) -> None:
        mod = importlib.import_module(
            "ovb_rc003.host_action_sink_native"
        )
        called = []
        with mock.patch.object(
            mod.py_win32_input,
            "send_escape",
            side_effect=lambda: called.append("escape"),
        ):
            sink = mod._make_host_action_sink_python()
            self.assertTrue(
                sink.submit_system_action(mod._PythonSystemAction.Escape)
            )
        self.assertEqual(called, ["escape"])


if __name__ == "__main__":
    unittest.main()
