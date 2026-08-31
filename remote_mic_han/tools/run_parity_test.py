"""Helper script to invoke a single parity test with PYTHONPATH set
correctly for both the C++ binding build directory and the source
tree where ``ovb_rc003`` lives.

CMake's ``set_tests_properties(... ENVIRONMENT ...)`` only passes a
single string, and ``;`` is treated as a CMake list separator. Using
this wrapper avoids the multi-path PYTHONPATH problem entirely: the
wrapper prepends both directories to ``PYTHONPATH``, then runs
unittest programmatically so the C++ extension loaded at import time
stays loaded for the duration of the test (avoids the Windows
``os.execvpe`` segfault seen when re-execing after the .pyd is
loaded).

CMake calls this wrapper as
``python run_parity_test.py -m unittest discover -s ... -t ... -p ... -v``.
The wrapper strips the leading ``-m unittest`` and runs
``unittest.main`` with the rest of argv, using the
``TestLoader.discover`` codepath when ``discover`` is the first
positional argument.
"""

from __future__ import annotations

import os
import sys
import unittest


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
    # sys.path is initialized from PYTHONPATH at interpreter start;
    # mutating os.environ afterward doesn't affect imports. Update
    # sys.path directly so the discover() codepath can find the
    # source-tree modules the same way the re-exec pattern used to.
    for entry in reversed(_extra):
        if entry and entry not in sys.path:
            sys.path.insert(0, entry)


def _run_unittest(argv: list[str]) -> int:
    """Invoke unittest with ``argv`` (the CLI args after the wrapper
    script name). Supports both the ``discover`` subcommand and
    direct module paths. Returns unittest's exit code.
    """
    if not argv:
        return 2
    # Strip the ``-m unittest`` prefix CMake passes if present.
    if argv[:2] == ["-m", "unittest"]:
        argv = argv[2:]
    verbose = "-v" in argv
    loader = unittest.TestLoader()
    # Discover mode is signaled by EITHER the explicit ``discover``
    # subcommand OR argv starting with a flag (``-s`` / ``-t`` /
    # ``-p`` / ``-v``). CMake invocations like
    # ``-m unittest -s ... -t ... -p ... -v`` start with a flag and
    # are discover mode even though they omit the ``discover``
    # keyword; TestProgram would reject ``-s`` / ``-p`` with
    # argparse errors if we handed them through.
    is_discover_mode = bool(argv) and (
        argv[0] == "discover" or argv[0].startswith("-")
    )
    if is_discover_mode:
        # Mirror ``unittest discover`` option parsing. ``-v`` is a
        # runner verbosity flag, not a discover() kwarg, so strip
        # it before calling discover() and remember its intent for
        # the runner below.
        kwargs = {}
        i = 1 if argv[0] == "discover" else 0
        while i < len(argv):
            tok = argv[i]
            if tok == "-v":
                i += 1
            elif tok == "-s":
                kwargs["start_dir"] = argv[i + 1]
                i += 2
            elif tok == "-t":
                kwargs["top_level_dir"] = argv[i + 1]
                i += 2
            elif tok == "-p":
                kwargs["pattern"] = argv[i + 1]
                i += 2
            else:
                i += 1
        try:
            suite = loader.discover(**kwargs)
        except Exception as exc:  # pragma: no cover - error path
            print(f"discover failed: {exc}", file=sys.stderr)
            return 2
    else:
        # Direct module paths or no discover - fall through to
        # TestProgram which handles argv natively.
        sys.argv = [sys.argv[0]] + argv
        try:
            unittest.main(module=None, argv=sys.argv, exit=False)
        except SystemExit as exc:
            return int(exc.code or 0)
        return 0
    runner = unittest.TextTestRunner(verbosity=2 if verbose else 1)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    rc = _run_unittest(list(sys.argv[1:]))
    sys.exit(rc)