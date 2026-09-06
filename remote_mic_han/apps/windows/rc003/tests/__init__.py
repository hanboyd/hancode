"""Test-suite input-isolation guard (P0 fix).

The full unit suite must never emit real keyboard input into the user's
desktop: no SendInput, no keybd_event, no WM_APPCOMMAND, no held keys, no
IME side effects.  Importing the tests package installs a sentinel that
replaces win32_input's three raw injection backends with functions that
FAIL the test immediately if anything calls them.  Because the package is
imported by the suite itself, the sentinel also covers the qt-lifecycle
inner subprocess run and every unittest discover invocation.

Escape hatch: a deliberately live input smoke can run only when the
operator sets REMOTEMIC_ALLOW_LIVE_INPUT_TESTS=1 explicitly.  Packaging
and release validation never set it.
"""

import os

_ALLOW_LIVE_INPUT = os.environ.get("REMOTEMIC_ALLOW_LIVE_INPUT_TESTS") == "1"


def _forbidden_real_input_backend(*_args, **_kwargs):
    raise AssertionError(
        "live input injection blocked: a test called the real win32 input "
        "backend (SendInput/keybd_event) during the unit suite. Use a fake "
        "sink, a monkeypatch, or set REMOTEMIC_ALLOW_LIVE_INPUT_TESTS=1 for "
        "an explicit opt-in live smoke."
    )


def _install_guard() -> None:
    if _ALLOW_LIVE_INPUT:
        return
    try:
        from ovb_rc003 import win32_input
    except ImportError:
        return  # non-Windows or source layout without the module
    for _name in (
        "_real_send_input_batch",
        "_real_keybd_event",
        "_real_voice_event",
    ):
        if hasattr(win32_input, _name):
            setattr(win32_input, _name, _forbidden_real_input_backend)


_install_guard()
