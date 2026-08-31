"""Phase 4 / ADR-0014 §6 step 4 acceptance proof script.

Runs both parity tests as two fresh subprocesses so each gets a clean
``ovb_rc003`` import (the factory dispatch is bound at module load
time; env vars must be set BEFORE Python starts, mirroring the
production pattern of ``python -m ovb_rc003`` after exporting
``REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=shadow``).

  1. Upsample parity: python 3-tap linear interpolation
     (audio_playback.py:154-172) vs C++ ``upsample_16k_to_48k`` -
     asserted byte-exact for empty / single / multi-sample / carry
     scenarios.
  2. Audio route parity: python ``FakePlaybackSink`` vs C++
     ``FakeAudioRoute`` - sample-count / peak / RMS / drop-count /
     drain-order + lifecycle counters all match.

The script prints one labelled PASS/FAIL line per test method and
exits non-zero if any fails. Re-run after any edit to:
  * ``src/audio/upsample_16k_to_48k.cpp``
  * ``src/audio/fake_audio_route.cpp``
  * ``apps/windows/rc003/tests/fakes/audio_route_fakes.py``
  * ``apps/windows/rc003/tests/test_audio_route_native_parity.py``
  * ``tests/bind/test_upsample_16k_to_48k_parity.py``

Usage (from the repo root):

    python tools/verify_phase4_audio_parity.py

PYTHONPATH ordering (build dir must come before src so the freshly
built ``_C.pyd`` is found ahead of any source-tree stubs). Mirrors
the cfebb9c fix from Phase 3.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
RC003_SRC = REPO_ROOT / "apps" / "windows" / "rc003" / "src"
RC003_TESTS = REPO_ROOT / "apps" / "windows" / "rc003" / "tests"
BUILD_RELEASE = REPO_ROOT / "build" / "Release"
BUILD_DEBUG = REPO_ROOT / "build" / "Debug"


def _venv_python() -> str:
    """Return the venv python.exe path. Falls back to sys.executable."""
    candidate = (
        REPO_ROOT
        / "apps"
        / "windows"
        / "rc003"
        / ".venv"
        / "Scripts"
        / "python.exe"
    )
    if candidate.exists():
        return str(candidate)
    return sys.executable


def _run_subprocess(label: str, args: list[str], env: dict) -> bool:
    """Run ``args`` and print one PASS/FAIL line for ``label``.

    Returns True on success. The subprocess inherits stdout/stderr
    directly so the unittest verbose output is visible - on failure
    the unfiltered output is what the user needs to diagnose.
    """
    print(f"--- {label} ---")
    result = subprocess.run(args, env=env)
    if result.returncode == 0:
        print(f"[PASS] {label}")
        return True
    print(f"[FAIL] {label} (exit={result.returncode})")
    return False


def main() -> int:
    py = _venv_python()

    # Per cfebb9c fix: build dir ahead of src on PYTHONPATH so the
    # freshly built _C.pyd is found before any source-tree stub.
    py_path_parts = [str(BUILD_RELEASE), str(BUILD_DEBUG), str(RC003_SRC)]
    py_path = os.pathsep.join(py_path_parts)

    env = os.environ.copy()
    # Force shadow so the audio_route parity test exercises both
    # sides even if the user has a different value in their shell.
    env["REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE"] = "shadow"
    env["PYTHONPATH"] = (
        py_path + os.pathsep + env.get("PYTHONPATH", "")
    )

    results = []

    # 1. Upsample parity (C++ vs python baseline, byte-exact).
    results.append(
        _run_subprocess(
            "upsample_16k_to_48k parity (python baseline vs _C)",
            [
                py,
                "-m",
                "unittest",
                "discover",
                "-s",
                str(REPO_ROOT / "tests" / "bind"),
                "-t",
                str(REPO_ROOT / "tests" / "bind"),
                "-p",
                "test_upsample_16k_to_48k_parity.py",
                "-v",
            ],
            env,
        )
    )

    # 2. Audio route parity (FakePlaybackSink vs FakeAudioRoute).
    results.append(
        _run_subprocess(
            "audio_route parity (FakePlaybackSink vs _C.FakeAudioRoute)",
            [
                py,
                "-m",
                "unittest",
                "discover",
                "-s",
                str(RC003_TESTS),
                "-t",
                str(REPO_ROOT / "apps" / "windows" / "rc003"),
                "-p",
                "test_audio_route_native_parity.py",
                "-v",
            ],
            env,
        )
    )

    if all(results):
        print()
        print(f"ALL {len(results)} PHASE 4 PARITY GATES PASS")
        return 0
    print()
    failed = sum(1 for ok in results if not ok)
    print(f"{failed}/{len(results)} PHASE 4 PARITY GATES FAILED")
    return 1


if __name__ == "__main__":
    sys.exit(main())
