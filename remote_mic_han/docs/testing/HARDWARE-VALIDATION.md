# Hardware Validation Matrix

Physical RC003 hardware became available on 2026-08-22. Results below are from the
user's device on Windows 11 and contain no Bluetooth address, HID path, device token,
or recorded voice.

| Boundary | Automated substitute now | Physical validation later | Status |
|---|---|---|---|
| ATVV parsing | Recorded byte fixtures and malformed packets | Real CAPS and audio start/stop sequence | passed |
| ADPCM decoding | Golden ADPCM-to-PCM/WAV vectors | Aggregate real PCM signal, without retaining audio | passed (signal) |
| BLE | Injectable state-machine events | Discover one candidate, connect, subscribe, receive notifications | passed; reconnect deferred |
| GATT cache | Simulated stale/cache-miss responses | Uncached real connection | partial; pair/unpair recovery deferred |
| HID | Recorded report fixtures | Raw Input and Windows key events | partial: direction/OK/Home/Menu/TV/Power pass; back/volume fail |
| Audio route | Generated PCM and fake sink | Third-party virtual microphone and target apps | deferred |
| Typeless/Qianwen | State-machine timing tests | Real trigger, pre-roll, drain, and text result | deferred |
| Installer | Clean VM or spare Windows profile | Install with actual optional dependencies | pending |

Observed voice evidence: ATVV 1.0, 16 kHz, frame size 120; three audio starts,
two stops, 1,927 PCM frames / 462,480 samples / about 28.9 seconds, peak 27,920,
result `signal`, zero transport errors. Elevated HID tap remained blocked by
WUDFHost (`Access denied`); direct HID-over-GATT characteristic enumeration also
returned access denied. Do not commit raw voice recordings without explicit approval.
