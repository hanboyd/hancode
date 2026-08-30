"""Phase 3 / ADR-0013 §5 acceptance proof script.

Runs the four acceptance conditions as four fresh subprocesses so each
gets a clean ``ovb_rc003`` import (the factory functions are bound at
import time, so env vars must be set BEFORE Python starts - this is the
exact production pattern of ``python -m ovb_rc003`` after exporting
env vars in the shell).

  1. Default launch path = python
  2. Three native env vars set -> production call sites get native shim
  3. Restore env vars -> back to python
  4. No double owner (single instance per call site; no shadow)

The script prints one labelled PASS/FAIL line per condition and exits
non-zero if any fails. Re-run after any edit to app.py,
ble_transport_winrt.py, or the three factory modules.

Usage (from the repo root):

    python tools/verify_phase3_production_routing.py

The script must be run with the same Python that has
``ovb_rc003`` on sys.path; PYTHONPATH=src (when run from
apps/windows/rc003) or PYTHONPATH=apps/windows/rc003/src (from repo
root) is handled below.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
RC003_SRC = REPO_ROOT / "apps" / "windows" / "rc003" / "src"

# Each scenario is a one-liner Python script. Each constructs a fresh
# RC003App (with a temp config root + owned event loop, mirroring the
# test harness pattern from test_app_wiring.py:_build_app) and prints
# exactly one line: the JSON-encoded type/module info for the three
# production call sites. The wrapper parses the line and asserts.

_SCENARIO_SCRIPT = r'''
import asyncio
import json
import logging
import os
import sys
import tempfile
from pathlib import Path

from ovb_rc003 import (
    atvv_session,
    config,
    logging_setup,
    voice_controller,
    voice_edge_debouncer,
)
from ovb_rc003.ble_transport_winrt import RC003BleSession
from ovb_rc003.voice_controller_native import _NativeVoiceController
from ovb_rc003.voice_edge_debouncer_native import _NativeVoiceEdgeDebouncer
from ovb_rc003.atvv_session_native import _NativeATVVSession

# Build a real RC003App with a throwaway config root so the env vars
# we're testing actually drive the production __init__ path (no mocks).
tmp = tempfile.TemporaryDirectory()
loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)
original_config_root = config.config_root
config.config_root = lambda: Path(tmp.name)
try:
    app = None
    try:
        from ovb_rc003 import app as app_mod
        app = app_mod.RC003App()
        bt = RC003BleSession(on_pcm_frame=lambda samples: None)
        bt_loop = asyncio.new_event_loop()
        # bt already built with the current loop; snapshot attrs.
        voice_type = type(app._voice).__module__ + "." + type(app._voice).__name__
        debouncer_type = type(app._voice_edge_debouncer).__module__ + "." + type(app._voice_edge_debouncer).__name__
        session_type = type(bt.session).__module__ + "." + type(bt.session).__name__
        voice_is_native = getattr(app._voice, "_is_native", False)
        debouncer_is_native = getattr(app._voice_edge_debouncer, "_is_native", False)
        session_is_native = getattr(bt.session, "_is_native", False)
        result = {
            "voice_type": voice_type,
            "debouncer_type": debouncer_type,
            "session_type": session_type,
            "voice_is_native": voice_is_native,
            "debouncer_is_native": debouncer_is_native,
            "session_is_native": session_is_native,
            "voice_has_impl": hasattr(app._voice, "_impl"),
            "debouncer_has_impl": hasattr(app._voice_edge_debouncer, "_impl"),
            "session_has_impl": hasattr(bt.session, "_impl"),
            "env": {
                "voice": os.environ.get("REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER", "unset"),
                "debouncer": os.environ.get("REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER", "unset"),
                "session": os.environ.get("REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION", "unset"),
            },
            "_c_available": getattr(__import__("remotemic_native"), "_C_AVAILABLE", False),
        }
        print("RESULT:" + json.dumps(result))
    finally:
        if app is not None:
            try:
                # Mirror test_app_wiring cleanup so the tmp dir is
                # removable on Windows (XRBM-023).
                logger = logging.getLogger(logging_setup.LOGGER_NAME)
                for handler in list(logger.handlers):
                    handler.close()
                    logger.removeHandler(handler)
                logging_setup._configured = False
            except Exception:
                pass
        try:
            tmp.cleanup()
        except Exception:
            pass
        asyncio.set_event_loop(None)
        loop.close()
finally:
    config.config_root = original_config_root
'''


def _run_scenario(label: str, env_overrides: dict[str, str] | None) -> dict:
    env = os.environ.copy()
    env["PYTHONPATH"] = str(RC003_SRC) + os.pathsep + env.get("PYTHONPATH", "")
    if env_overrides:
        for k, v in env_overrides.items():
            env[k] = v
    else:
        # Make sure none of the three Phase 3 keys leak into the test.
        for k in (
            "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER",
            "REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER",
            "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION",
        ):
            env.pop(k, None)
    result = subprocess.run(
        [sys.executable, "-c", _SCENARIO_SCRIPT],
        env=env,
        capture_output=True,
        text=True,
        timeout=60,
    )
    if result.returncode != 0:
        print(f"[FAIL] {label}: subprocess crashed")
        print("stdout:", result.stdout)
        print("stderr:", result.stderr)
        sys.exit(2)
    line = [ln for ln in result.stdout.splitlines() if ln.startswith("RESULT:")]
    if len(line) != 1:
        print(f"[FAIL] {label}: no RESULT line in output")
        print("stdout:", result.stdout)
        print("stderr:", result.stderr)
        sys.exit(2)
    import json
    return json.loads(line[0][len("RESULT:"):])


def _check(label: str, condition: bool, detail: str) -> bool:
    tag = "PASS" if condition else "FAIL"
    print(f"[{tag}] {label}: {detail}")
    return condition


def main() -> int:
    all_ok = True

    # ---- 1. Default launch path = python -------------------------------
    r = _run_scenario(
        "condition 1 (default -> python)",
        env_overrides=None,
    )
    all_ok &= _check(
        "voice defaults to python",
        r["voice_type"] == "ovb_rc003.voice_controller.VoiceController",
        f"type={r['voice_type']!r} env={r['env']['voice']!r}",
    )
    all_ok &= _check(
        "edge_debouncer defaults to python",
        r["debouncer_type"] == "ovb_rc003.voice_edge_debouncer.VoiceEdgeDebouncer",
        f"type={r['debouncer_type']!r} env={r['env']['debouncer']!r}",
    )
    all_ok &= _check(
        "atvv_session defaults to python",
        r["session_type"] == "ovb_rc003.atvv_session.ATVVSession",
        f"type={r['session_type']!r} env={r['env']['session']!r}",
    )

    # ---- 2. Native env vars -> production gets native shim -------------
    r = _run_scenario(
        "condition 2 (native env -> shim)",
        env_overrides={
            "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER": "native",
            "REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER": "native",
            "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION": "native",
        },
    )
    all_ok &= _check(
        "voice routed to native shim",
        r["voice_type"] == "ovb_rc003.voice_controller_native._NativeVoiceController",
        f"type={r['voice_type']!r} env={r['env']['voice']!r}",
    )
    all_ok &= _check(
        "edge_debouncer routed to native shim",
        r["debouncer_type"] == "ovb_rc003.voice_edge_debouncer_native._NativeVoiceEdgeDebouncer",
        f"type={r['debouncer_type']!r} env={r['env']['debouncer']!r}",
    )
    all_ok &= _check(
        "atvv_session routed to native shim",
        r["session_type"] == "ovb_rc003.atvv_session_native._NativeATVVSession",
        f"type={r['session_type']!r} env={r['env']['session']!r}",
    )

    # ---- 3. Restore env vars -> back to python -------------------------
    r = _run_scenario(
        "condition 3 (restore env -> python)",
        env_overrides=None,
    )
    all_ok &= _check(
        "voice back to python after env unset",
        r["voice_type"] == "ovb_rc003.voice_controller.VoiceController",
        f"type={r['voice_type']!r} env={r['env']['voice']!r}",
    )
    all_ok &= _check(
        "edge_debouncer back to python after env unset",
        r["debouncer_type"] == "ovb_rc003.voice_edge_debouncer.VoiceEdgeDebouncer",
        f"type={r['debouncer_type']!r} env={r['env']['debouncer']!r}",
    )
    all_ok &= _check(
        "atvv_session back to python after env unset",
        r["session_type"] == "ovb_rc003.atvv_session.ATVVSession",
        f"type={r['session_type']!r} env={r['env']['session']!r}",
    )

    # ---- 4. No double owner -------------------------------------------
    # Under default env: each production call site holds a python class
    # directly; no parallel native instance, no shadow side-channel.
    r = _run_scenario("condition 4a (default -> no native side)", env_overrides=None)
    all_ok &= _check(
        "voice has no _impl attribute under default (no shim constructed)",
        not r["voice_has_impl"],
        f"type={r['voice_type']!r}",
    )
    all_ok &= _check(
        "edge_debouncer has no _impl attribute under default",
        not r["debouncer_has_impl"],
        f"type={r['debouncer_type']!r}",
    )
    all_ok &= _check(
        "atvv_session has no _impl attribute under default",
        not r["session_has_impl"],
        f"type={r['session_type']!r}",
    )
    # Under native: production call site is the shim, which holds exactly
    # one C++ impl (or python fallback if _C.pyd not built locally).
    # Either way exactly one _impl - never two, never a parallel python.
    r = _run_scenario(
        "condition 4b (native -> single owner)",
        env_overrides={
            "REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER": "native",
            "REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER": "native",
            "REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION": "native",
        },
    )
    # The shim holds exactly one _impl; the underlying class is either
    # the C++ binding (when _C.pyd is built) or the python baseline
    # (silent fallback when not). In both cases there is exactly ONE
    # owner per call site - no shadow dual execution.
    all_ok &= _check(
        "voice shim constructed (one owner)",
        r["voice_type"] == "ovb_rc003.voice_controller_native._NativeVoiceController",
        f"type={r['voice_type']!r}",
    )
    all_ok &= _check(
        "edge_debouncer shim constructed (one owner)",
        r["debouncer_type"] == "ovb_rc003.voice_edge_debouncer_native._NativeVoiceEdgeDebouncer",
        f"type={r['debouncer_type']!r}",
    )
    all_ok &= _check(
        "atvv_session shim constructed (one owner)",
        r["session_type"] == "ovb_rc003.atvv_session_native._NativeATVVSession",
        f"type={r['session_type']!r}",
    )
    # Optional: when _C.pyd IS built, the shim reached the C++ side.
    # Locally without _C.pyd the shim silently falls back to python
    # (per voice_controller_native.py:54-59) - that is not dual-owner,
    # that is the documented single-owner-with-fallback contract.
    if r.get("_c_available"):
        all_ok &= _check(
            "voice shim reached C++ side (_is_native=True)",
            bool(r["voice_is_native"]),
            f"_is_native={r['voice_is_native']!r}",
        )
        all_ok &= _check(
            "edge_debouncer shim reached C++ side",
            bool(r["debouncer_is_native"]),
            f"_is_native={r['debouncer_is_native']!r}",
        )
        all_ok &= _check(
            "atvv_session shim reached C++ side",
            bool(r["session_is_native"]),
            f"_is_native={r['session_is_native']!r}",
        )
    else:
        print(
            "[NOTE] remotemic_native._C.pyd not built locally; "
            "single-owner assertions for the C++ side are skipped. "
            "Re-run on a Windows runner with _C.pyd built to verify "
            "the C++ side actually executed."
        )

    print()
    if all_ok:
        print("ALL FOUR CONDITIONS PASS")
        return 0
    print("ONE OR MORE CONDITIONS FAILED")
    return 1


if __name__ == "__main__":
    sys.exit(main())