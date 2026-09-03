from __future__ import annotations

import asyncio
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP_SRC = ROOT / "apps" / "windows" / "rc003" / "src"
BUILD = ROOT / "build" / "Debug"
sys.path[:0] = [str(BUILD), str(APP_SRC)]

import remotemic_native as native  # noqa: E402
from ovb_rc003 import ble_transport_winrt, config, hotkey, identity, win32_keys  # noqa: E402


async def main() -> int:
    candidates = await ble_transport_winrt.discover_candidates()
    candidate = identity.select_single_candidate(candidates)
    device_id = str(candidate.handle.id)
    values = config.load_config(config.config_path(config.config_root()))
    spec = hotkey.HotkeySpec.parse(str(values["voice_hotkey"]))
    voice_keys = list(win32_keys.resolve_vk_codes((*spec.modifiers, spec.key)))
    mode = (
        native.VoiceTriggerMode.Hold
        if values["voice_trigger_mode"] == "hold"
        else native.VoiceTriggerMode.Toggle
    )
    service = native.ApplicationCoordinator(
        device_id,
        str(values.get("output_endpoint_name") or ""),
        str(values.get("output_endpoint_host_api") or "Windows WASAPI"),
        voice_keys,
        mode,
        float(values["gain_db"]),
    )
    started = await asyncio.to_thread(
        service.execute, 1, native.CoordinatorCommandKind.Start
    )
    if not started.ok:
        print(f"phase7 hardware smoke: start failed: {started.message}")
        return 1
    deadline = time.monotonic() + 2.0
    observed = []
    while time.monotonic() < deadline:
        event = service.poll_event()
        if event is not None:
            observed.append(event["kind"])
        await asyncio.sleep(0.05)
    stopped = await asyncio.to_thread(
        service.execute, 2, native.CoordinatorCommandKind.Stop
    )
    print(
        "phase7 hardware smoke:",
        f"started={started.ok}",
        f"stopped={stopped.ok}",
        f"events={len(observed)}",
        f"dropped={service.dropped_event_count}",
    )
    return 0 if stopped.ok else 1


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
