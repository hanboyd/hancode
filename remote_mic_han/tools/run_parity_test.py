"""Helper script to invoke a single parity test with PYTHONPATH set
correctly for both the C++ binding build directory and the source
tree where ``ovb_rc003`` lives.

CMake's ``set_tests_properties(... ENVIRONMENT ...)`` only passes a
single string, and ``;`` is treated as a CMake list separator. Using
this wrapper avoids the multi-path PYTHONPATH problem entirely: the
wrapper re-execs the same Python interpreter with both directories
prepended to ``PYTHONPATH``, then forwards the original CLI args
straight to ``python -m unittest``.
"""

from __future__ import annotations

import os
import sys


_BUILD_DIR = os.environ.get("REMOTEMIC_BUILD_DIR", "")
_SRC_DIR = os.environ.get("REMOTEMIC_SRC_DIR", "")

_extra = []
if _BUILD_DIR:
    _extra.append(_BUILD_DIR)
if _SRC_DIR:
    _extra.append(_SRC_DIR)

if _extra:
    existing = os.environ.get("PYTHONPATH", "")
    if existing:
        os.environ["PYTHONPATH"] = os.pathsep.join(
            _extra + [existing]
        )
    else:
        os.environ["PYTHONPATH"] = os.pathsep.join(_extra)

if __name__ == "__main__":
    # Drop this script's argv[0]; the rest are unittest args that
    # CMake passed after the script path. Re-exec the same python
    # interpreter with ``-m unittest`` so the test runner sees them
    # as its own argv (unittest rejects ``-m unittest`` as its own
    # args otherwise).
    args = [sys.executable, "-m", "unittest"] + sys.argv[1:]
    os.execvpe(sys.executable, args, os.environ)