"""Phase 5 / ADR-0015 §6 step 3: native switch + production routing gate.

Mirror of ``verify_phase4_native_switch.py`` for the input layer.
Asserts:

  1. Default ``python`` choice routes through the python shim
     (``_PythonInputSource`` / ``_PythonHostActionSink``) and
     produces an instance that the bridge wrapper can use.
  2. ``native`` choice routes through the native shim
     (``_NativeInputSource`` / ``_NativeHostActionSink``) and the
     factory returns a real object (or, when ``_C_AVAILABLE`` is
     false, transparently falls back to the python shim — but the
     shape stays the same).
  3. ``shadow`` choice raises ``RuntimeError`` per plan §3 rule 5
     (Raw Input handle + SendInput dispatch are both
     side-effecting; running both would double-fire).
  4. Production source-level: ``app.py`` imports
     ``make_input_source`` / ``make_host_action_sink`` and
     constructs them in ``App.__init__`` so the env-var switch
     takes effect at startup.

Returns exit 0 when all 4 conditions PASS, exit 1 otherwise. The
script never mutates ``os.environ`` permanently; every override is
wrapped in try / finally that restores the prior state.
"""

from __future__ import annotations

import importlib
import os
import sys


_INPUT_KEY = "REMOTEMIC_NATIVE_CHOICE_INPUT_SOURCE"
_HOST_KEY = "REMOTEMIC_NATIVE_CHOICE_HOST_ACTION_SINK"


def _ensure_repo_on_path() -> None:
    repo_root = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..")
    )
    src = os.path.join(
        repo_root, "apps", "windows", "rc003", "src"
    )
    build_release = os.path.join(repo_root, "build", "Release")
    for p in (build_release, src):
        if p not in sys.path:
            sys.path.insert(0, p)


def _reload(mod_name: str) -> object:
    importlib.import_module(mod_name)
    return importlib.reload(importlib.import_module(mod_name))


def _with_env(name: str, value: str | None, fn):
    saved = os.environ.get(name)
    if value is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = value
    try:
        return fn()
    finally:
        if saved is None:
            os.environ.pop(name, None)
        else:
            os.environ[name] = saved


def _check_python_choice() -> bool:
    """Default choice = python shim reachable + constructs an
    instance with the IInputSource/IHostActionSink surface.
    """
    mod = _reload("ovb_rc003.input_source_native")
    src = mod.make_input_source_python()
    if not hasattr(src, "set_event_sink"):
        return False
    if not hasattr(src, "start"):
        return False
    if not hasattr(src, "stop"):
        return False

    mod = _reload("ovb_rc003.host_action_sink_native")
    sink = mod.make_host_action_sink_python()
    if not hasattr(sink, "submit_key"):
        return False
    if not hasattr(sink, "submit_system_action"):
        return False
    if not hasattr(sink, "cancel_pending"):
        return False
    return True


def _check_native_choice() -> bool:
    """Native choice = native shim class reachable + factory
    callable. On non-Windows / no-`_C.pyd` hosts the native shim
    transparently falls back to the python shim; the check here
    only asserts the SHIM is constructible, not that the C++
    side actually started (which requires a real Windows host).
    """
    mod = _with_env(_INPUT_KEY, "native",
                    lambda: _reload("ovb_rc003.input_source_native"))
    src = mod.make_input_source()
    if not (hasattr(src, "set_event_sink") and hasattr(src, "start")):
        return False

    mod = _with_env(_HOST_KEY, "native",
                    lambda: _reload("ovb_rc003.host_action_sink_native"))
    sink = mod.make_host_action_sink()
    if not (hasattr(sink, "submit_key") and hasattr(sink, "start")):
        return False
    return True


def _check_shadow_rejected() -> bool:
    """Shadow is forbidden per plan §3 rule 5: Raw Input +
    SendInput are both side-effecting. ``choose_implementation``
    raises at MODULE IMPORT time when the env var is set to
    ``shadow`` for a side-effecting module.
    """
    def _expect_runtime_error_at_reload():
        try:
            _reload("ovb_rc003.input_source_native")
        except RuntimeError:
            return True
        return False

    if not _with_env(_INPUT_KEY, "shadow", _expect_runtime_error_at_reload):
        return False

    def _expect_runtime_error_at_reload_sink():
        try:
            _reload("ovb_rc003.host_action_sink_native")
        except RuntimeError:
            return True
        return False

    if not _with_env(_HOST_KEY, "shadow",
                     _expect_runtime_error_at_reload_sink):
        return False
    return True


def _check_production_routing() -> bool:
    """Source-level check: ``app.py`` imports the factory modules
    + constructs them in ``App.__init__`` so the env-var switch
    takes effect at startup.
    """
    import ast

    repo_root = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..")
    )
    app_path = os.path.join(
        repo_root,
        "apps", "windows", "rc003", "src",
        "ovb_rc003", "app.py",
    )
    with open(app_path, encoding="utf-8") as fh:
        tree = ast.parse(fh.read(), filename=app_path)

    imported = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom):
            if node.module and node.module.endswith(
                ("input_source_native", "host_action_sink_native")
            ):
                for alias in node.names:
                    imported.add(alias.name)
    if "make_input_source" not in imported:
        return False
    if "make_host_action_sink" not in imported:
        return False

    init_method = None
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "RC003App":
            for item in node.body:
                if (isinstance(item, ast.FunctionDef)
                        and item.name == "__init__"):
                    init_method = item
                    break
            break
    if init_method is None:
        return False
    factory_calls = set()
    for sub in ast.walk(init_method):
        if isinstance(sub, ast.Call):
            func = sub.func
            if isinstance(func, ast.Name) and func.id in (
                "make_input_source", "make_host_action_sink",
            ):
                factory_calls.add(func.id)
    return factory_calls == {"make_input_source", "make_host_action_sink"}


def main() -> int:
    _ensure_repo_on_path()
    results = [
        ("python_choice_routes_to_python_shim", _check_python_choice()),
        ("native_choice_routes_to_native_shim", _check_native_choice()),
        ("shadow_choice_rejected", _check_shadow_rejected()),
        ("app_references_input_factories", _check_production_routing()),
    ]
    failures = 0
    for name, ok in results:
        marker = "PASS" if ok else "FAIL"
        print(f"[{marker}] {name}")
        if not ok:
            failures += 1
    print()
    if failures == 0:
        print(f"verify_phase5_native_switch: all 4 PASS")
        return 0
    print(f"verify_phase5_native_switch: {failures}/4 failed")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())