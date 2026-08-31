"""Phase 5 / ADR-0015 §6 step 3: production routing gate.

Source-level checks that ``app.py`` references the new input
factory helpers (``make_input_source`` /
``make_host_action_sink``) and does NOT bypass them with raw
python class instantiations outside the python shim. Mirrors the
Phase 3 / Phase 4 production-routing tests at
``test_phase3_production_routing.py`` /
``test_phase4_audio_route_production_routing.py``.
"""

from __future__ import annotations

import ast
import os
import unittest


_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "..")
)
_APP_PATH = os.path.join(
    _REPO_ROOT,
    "apps", "windows", "rc003", "src", "ovb_rc003", "app.py",
)
_TARGET_CLASS = "RC003App"


def _load_app_module_ast():
    with open(_APP_PATH, encoding="utf-8") as fh:
        return ast.parse(fh.read(), filename=_APP_PATH)


def _find_init_method(tree):
    """Return the ``__init__`` of ``_TARGET_CLASS`` (top-level only)."""
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == _TARGET_CLASS:
            for item in node.body:
                if isinstance(item, ast.FunctionDef) and item.name == "__init__":
                    return item
    return None


class InputProductionRoutingTests(unittest.TestCase):
    def test_app_imports_input_factory_modules(self) -> None:
        tree = _load_app_module_ast()
        imported = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.ImportFrom):
                if node.module and node.module.endswith(
                    ("input_source_native", "host_action_sink_native")
                ):
                    for alias in node.names:
                        imported.add(alias.name)
        self.assertIn("make_input_source", imported)
        self.assertIn("make_host_action_sink", imported)

    def test_app_constructs_input_source_and_host_action_sink(self) -> None:
        # Source-level: RC003App.__init__ must call ``make_input_source()``
        # and ``make_host_action_sink()`` so the env-var switch
        # actually takes effect at startup.
        tree = _load_app_module_ast()
        init_method = _find_init_method(tree)
        self.assertIsNotNone(
            init_method,
            f"{_TARGET_CLASS}.__init__ not found in {_APP_PATH}",
        )
        factory_calls = set()
        for sub in ast.walk(init_method):
            if isinstance(sub, ast.Call):
                func = sub.func
                if isinstance(func, ast.Name) and func.id in (
                    "make_input_source", "make_host_action_sink",
                ):
                    factory_calls.add(func.id)
        self.assertEqual(
            factory_calls,
            {"make_input_source", "make_host_action_sink"},
            f"{_TARGET_CLASS}.__init__ did not call both factories; "
            f"got {factory_calls}",
        )

    def test_app_does_not_bypass_with_native_class_directly(self) -> None:
        # App must NOT construct ``remotemic_native._C.RawInputSource``
        # or ``remotemic_native._C.SendInputActionSink`` directly
        # (those reach the C++ side; the factory's native shim does it
        # and exposes the env-var switch). Look for direct
        # instantiations outside the factory module.
        tree = _load_app_module_ast()
        offenders = []
        for node in ast.walk(tree):
            if not isinstance(node, ast.Call):
                continue
            func = node.func
            if (
                isinstance(func, ast.Attribute)
                and func.attr in ("RawInputSource", "SendInputActionSink")
            ):
                offenders.append((node.lineno, func.attr))
        self.assertEqual(
            offenders, [],
            f"app.py still constructs the native input classes "
            f"directly at line(s) {offenders}; route via "
            f"make_input_source() / make_host_action_sink() factory so "
            f"the env-var switch takes effect.",
        )


if __name__ == "__main__":
    unittest.main()