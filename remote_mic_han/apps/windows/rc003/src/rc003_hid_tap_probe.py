"""Time-bounded, passive RC003 HidOverGatt report probe.

Run this helper from an elevated process after ordinary Raw Input and the
broad WM_INPUT probe have both remained silent.  It observes reports only:
no host action is dispatched and no key binding is written.
"""

from __future__ import annotations

import argparse
import contextlib
import ctypes
import sys
import time
from pathlib import Path
from typing import TextIO

from ovb_rc003.frida_compat import RC003HidReportTap


def _is_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except (AttributeError, OSError):
        return False


def _run(seconds: float) -> int:
    reports = 0

    def on_report(_report_id: int, _data: bytes) -> None:
        nonlocal reports
        reports += 1

    print(
        f"RC003 HID PROBE START admin={str(_is_admin()).lower()} "
        f"duration_seconds={seconds:g}",
        flush=True,
    )
    if not _is_admin():
        print("RC003 HID PROBE BLOCKED reason=administrator_required", flush=True)
        return 3

    tap = RC003HidReportTap(on_report)
    if not tap.start():
        print(f"RC003 HID PROBE BLOCKED reason={tap.status}", flush=True)
        return 2

    try:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            time.sleep(min(0.25, max(0.0, deadline - time.monotonic())))
    finally:
        tap.stop()

    print(f"RC003 HID PROBE END reports={reports} status={tap.status}", flush=True)
    return 0 if reports else 1


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=90.0)
    parser.add_argument("--output", type=Path)
    return parser


def main() -> int:
    args = _parser().parse_args()
    if args.seconds <= 0:
        raise SystemExit("--seconds must be greater than zero")

    stream: TextIO | None = None
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        stream = args.output.open("w", encoding="utf-8", buffering=1)
    try:
        if stream is None:
            return _run(args.seconds)
        with contextlib.redirect_stdout(stream), contextlib.redirect_stderr(stream):
            return _run(args.seconds)
    finally:
        if stream is not None:
            stream.close()


if __name__ == "__main__":
    sys.exit(main())
