"""Step 7b helper: launch the Phase 3 source bridge with
``CREATE_NEW_PROCESS_GROUP`` so we can send ``CTRL_BREAK_EVENT`` later
(mimicking Ctrl+C). Verifies the KeyboardInterrupt path of the restored
``_run(stop_signal)`` body: cleanup runs without the named-event log
line ("bridge stop requested by settings; cleaning up") and exits 0.
"""

from __future__ import annotations

import os
import signal
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
PY = REPO_ROOT / "apps" / "windows" / "rc003" / ".venv" / "Scripts" / "python.exe"
LOG_PATH = Path(os.environ["LOCALAPPDATA"]) / "RemoteMic" / "RC003" / "logs" / "app.log"

start_size = LOG_PATH.stat().st_size
print(f"LOG_START_LINE={start_size}")

env = os.environ.copy()
env["REMOTEMIC_NATIVE_CHOICE_VOICE_CONTROLLER"] = "native"
env["REMOTEMIC_NATIVE_CHOICE_VOICE_EDGE_DEBOUNCER"] = "native"
env["REMOTEMIC_NATIVE_CHOICE_ATVV_SESSION"] = "native"
env["PYTHONPATH"] = os.pathsep.join([
    str(REPO_ROOT / "build" / "Release"),
    str(REPO_ROOT / "apps" / "windows" / "rc003" / "src"),
])

CREATE_NEW_PROCESS_GROUP = 0x00000200
CREATE_NEW_CONSOLE = 0x00000010

print(f"launching: {PY} -m ovb_rc003 --bridge")
proc = subprocess.Popen(
    [str(PY), "-m", "ovb_rc003", "--bridge"],
    env=env,
    creationflags=CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE,
)
print(f"PID={proc.pid}")

# Wait for BLE discovery + ATVV caps — should be ~10s on a warm Windows host.
print("waiting 14s for startup...")
time.sleep(14)

# Confirm we're past the gate (look for the canonical startup line in the log)
with LOG_PATH.open("rb") as fh:
    fh.seek(start_size)
    tail_text = fh.read().decode("utf-8", errors="replace")
if "startup: exactly one RC003 candidate resolved" in tail_text:
    print("STARTUP_OK: BLE candidate resolved")
else:
    print("STARTUP_MISSING: did not see expected startup line, sending break anyway")

print(f"sending CTRL_BREAK_EVENT to PID {proc.pid}")
proc.send_signal(signal.CTRL_BREAK_EVENT)

# Wait for the process to exit
try:
    exit_code = proc.wait(timeout=15)
    print(f"EXIT_CODE={exit_code}")
except subprocess.TimeoutExpired:
    print("TIMEOUT: process did not exit within 15s of CTRL_BREAK_EVENT; terminating")
    proc.kill()
    exit_code = proc.wait()

# Read log tail after shutdown
with LOG_PATH.open("rb") as fh:
    fh.seek(start_size)
    tail_text = fh.read().decode("utf-8", errors="replace")

print("=== key signals in post-startup log ===")
markers = [
    "startup: exactly one RC003 candidate resolved",
    "startup: RC003 voice legacy-key guard enabled",
    "bridge stop requested by settings; cleaning up",
    "cleanup: attempted release of hotkey state and BLE/HID/audio",
    "CleanupIncompleteError",
    "Traceback (most recent call last)",
]
for m in markers:
    print(f"  count({m!r}) = {tail_text.count(m)}")

sys.exit(0 if exit_code == 0 else exit_code)
