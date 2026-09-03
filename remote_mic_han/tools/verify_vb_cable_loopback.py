"""Hardware integration probe: synthetic PCM -> CABLE Input -> CABLE Output.

No audio file is written. The probe emits a short deterministic tone and
reports only sample statistics, so it is safe to keep as an explicit manual
gate without storing voice data.
"""

from __future__ import annotations

import math
import sys
import time

import numpy as np
import sounddevice as sd


def _find_wasapi_device(fragment: str, channel_key: str) -> int:
    hostapis = sd.query_hostapis()
    matches = []
    for index, item in enumerate(sd.query_devices()):
        host = hostapis[item["hostapi"]]["name"]
        if host == "Windows WASAPI" and fragment.lower() in item["name"].lower() \
                and int(item[channel_key]) > 0 and "16ch" not in item["name"].lower():
            matches.append(index)
    if len(matches) != 1:
        raise RuntimeError(f"expected one WASAPI {fragment!r} device, found {len(matches)}")
    return matches[0]


def main() -> int:
    rate = 48_000
    frames = rate
    output = _find_wasapi_device("CABLE Input", "max_output_channels")
    capture = _find_wasapi_device("CABLE Output", "max_input_channels")
    phase = np.arange(frames, dtype=np.float64) / rate
    tone = (0.20 * np.sin(2.0 * math.pi * 997.0 * phase)).astype(np.float32)
    recorded = np.zeros((frames, 1), dtype=np.float32)
    with sd.InputStream(device=capture, samplerate=rate, channels=1,
                        dtype="float32") as input_stream:
        with sd.OutputStream(device=output, samplerate=rate, channels=1,
                             dtype="float32") as output_stream:
            time.sleep(0.10)
            output_stream.write(tone.reshape(-1, 1))
            recorded[:], overflowed = input_stream.read(frames)
    peak = float(np.max(np.abs(recorded)))
    rms = float(np.sqrt(np.mean(np.square(recorded, dtype=np.float64))))
    passed = not overflowed and peak >= 0.05 and rms >= 0.01
    print(f"vb-cable-loopback: peak={peak:.6f} rms={rms:.6f} overflow={overflowed} result={'passed' if passed else 'failed'}")
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"vb-cable-loopback: failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
