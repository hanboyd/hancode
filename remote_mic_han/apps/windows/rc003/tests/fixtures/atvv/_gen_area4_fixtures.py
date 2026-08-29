"""Generate the Phase 2 / Area 4 (DCHighPassFilter + postprocess +
FrameAccumulator) JSON golden fixtures from the Python baseline.

Per ADR-0012 section 4: the fixtures are the single source of truth
shared between the C++ unit tests and the Python shadow parity tests;
this script is build-time only - its output is checked into git and
never run during test discovery.

Hard rule from the phase 2 entry scope (no tolerance, byte-exact /
sample-exact): every integer in ``expected_*`` must match the C++
implementation's output exactly.

Run:
    PYTHONPATH=apps/windows/rc003/src apps/windows/rc003/.venv/Scripts/python.exe \\
        apps/windows/rc003/tests/fixtures/atvv/_gen_area4_fixtures.py
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import List

from ovb_rc003 import atvv_protocol as proto


FIXTURE_DIR = Path(__file__).resolve().parent


_SOURCE = (
    "synthetic; generated from ovb_rc003.atvv_protocol via "
    "_gen_area4_fixtures.py; contains no captured device or voice data"
)


def _write(name: str, body: dict) -> None:
    body.setdefault("source", _SOURCE)
    body["fixture_kind"] = "area4"
    out = FIXTURE_DIR / name
    out.write_text(json.dumps(body, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out.relative_to(FIXTURE_DIR.parent.parent.parent.parent.parent)}")


# ---------------------------------------------------------------------------
# DCHighPassFilter fixtures
# ---------------------------------------------------------------------------


def dc_empty() -> None:
    f = proto.DCHighPassFilter()
    f.reset()
    out = f.process([])
    _write(
        "dc-empty.json",
        {
            "description": "Empty sample list -> empty filtered list. "
                           "Boundary case for the inner loop.",
            "function": "DcHighPassFilter.process",
            "samples": [],
            "expected_filtered": out,
        },
    )


def dc_single_sample() -> None:
    """First sample initializes the filter (no output delta). Output
    equals input by construction."""
    f = proto.DCHighPassFilter()
    f.reset()
    out = f.process([100])
    _write(
        "dc-single-sample.json",
        {
            "description": "Single sample [100]; filter self-initializes "
                           "on the first sample. Output equals input "
                           "by construction (no previous input/output).",
            "function": "DcHighPassFilter.process",
            "samples": [100],
            "expected_filtered": out,
        },
    )


def dc_two_samples() -> None:
    """Two samples; the second sample is the first 'real' filtering
    operation. alpha = exp(-2*pi*20/16000) ~= 0.9922."""
    f = proto.DCHighPassFilter()
    f.reset()
    out = f.process([100, 200])
    _write(
        "dc-two-samples.json",
        {
            "description": "Two samples [100, 200]; second sample uses "
                           "alpha = exp(-2*pi*20/16000) ~= 0.99221. "
                           "Output[1] = current - prev_input + alpha * prev_output "
                           "= 200 - 100 + alpha * 100 = 100 + 99.221 = 199.221.",
            "function": "DcHighPassFilter.process",
            "samples": [100, 200],
            "expected_filtered": out,
        },
    )


def dc_dc_blocked() -> None:
    """DC offset (constant input) should be removed; output should
    converge toward 0. alpha = exp(-2*pi*20/16000) ~= 0.99221."""
    f = proto.DCHighPassFilter()
    f.reset()
    out = f.process([1000] * 20)
    _write(
        "dc-dc-blocked.json",
        {
            "description": "Constant DC offset [1000]*20; the high-pass "
                           "filter removes the DC component so output "
                           "converges toward 0 from the first sample.",
            "function": "DcHighPassFilter.process",
            "samples": [1000] * 20,
            "expected_filtered": out,
        },
    )


def dc_ac_passes() -> None:
    """A 100 Hz tone at 16 kHz sample rate (160 samples per cycle)
    should pass through the 20 Hz high-pass with some attenuation.
    After warmup, output should be roughly periodic."""
    f = proto.DCHighPassFilter()
    f.reset()
    n = 160 * 4  # 4 cycles of a 100 Hz tone
    samples = [
        int(1000 * math.sin(2 * math.pi * i / 160)) for i in range(n)
    ]
    out = f.process(samples)
    _write(
        "dc-ac-passes.json",
        {
            "description": "Synthetic 100 Hz tone (160 samples per cycle "
                           "at 16 kHz sample rate); 4 cycles. The 20 Hz "
                           "high-pass passes it through with mild "
                           "attenuation (after the DC removal warmup).",
            "function": "DcHighPassFilter.process",
            "samples": samples,
            "expected_filtered": out,
        },
    )


# ---------------------------------------------------------------------------
# postprocess fixtures (pure function)
# ---------------------------------------------------------------------------


def post_empty() -> None:
    out = proto.postprocess([])
    _write(
        "postprocess-empty.json",
        {
            "description": "Empty sample list -> empty output (boundary).",
            "function": "postprocess",
            "samples": [],
            "gain_db": 10.0,
            "expected_output": out,
        },
    )


def post_single_default_gain() -> None:
    """Single sample [100] with default gain 10 dB. No smoothing
    because len < 3; gain = 10^(10/20) = ~3.162. Scaled value =
    round(100 * 3.162) = 316."""
    out = proto.postprocess([100])
    _write(
        "postprocess-single-default-gain.json",
        {
            "description": "Single sample [100] with default gain 10 dB. "
                           "len < 3 -> no 3-tap smoothing. gain = "
                           "10^(10/20) ~= 3.162. Scaled = round(100 * 3.162) "
                           "= 316.",
            "function": "postprocess",
            "samples": [100],
            "gain_db": 10.0,
            "expected_output": out,
        },
    )


def post_zero_gain() -> None:
    """Multiple samples with gain 0 dB -> identity (after 3-tap
    smoothing)."""
    out = proto.postprocess([1, 2, 3, 4, 5], gain_db=0.0)
    _write(
        "postprocess-zero-gain.json",
        {
            "description": "5 samples with gain_db=0.0 -> gain=1.0. "
                           "3-tap smoothing: filtered[1]=(1+2*2+3)/4=2, "
                           "filtered[2]=(2+2*3+4)/4=3, filtered[3]=(3+2*4+5)/4=4. "
                           "Endpoints stay as input.",
            "function": "postprocess",
            "samples": [1, 2, 3, 4, 5],
            "gain_db": 0.0,
            "expected_output": out,
        },
    )


def post_max_gain() -> None:
    """gain_db = 24 dB (max). gain = 10^(24/20) = ~15.849."""
    out = proto.postprocess([100, 200, 300, 400], gain_db=24.0)
    _write(
        "postprocess-max-gain.json",
        {
            "description": "4 samples with gain_db=24.0 (max). "
                           "gain = 10^(24/20) ~= 15.849. "
                           "Endpoints unsmoothed; middle samples smoothed "
                           "by 3-tap then scaled.",
            "function": "postprocess",
            "samples": [100, 200, 300, 400],
            "gain_db": 24.0,
            "expected_output": out,
        },
    )


def post_min_gain() -> None:
    """gain_db = -24 dB (min). gain = 10^(-24/20) = ~0.0631."""
    out = proto.postprocess([1000, 2000, 3000, 4000], gain_db=-24.0)
    _write(
        "postprocess-min-gain.json",
        {
            "description": "4 samples with gain_db=-24.0 (min). "
                           "gain = 10^(-24/20) ~= 0.0631. "
                           "Same smoothing then scaled down.",
            "function": "postprocess",
            "samples": [1000, 2000, 3000, 4000],
            "gain_db": -24.0,
            "expected_output": out,
        },
    )


def post_gain_clamps_above_24() -> None:
    """gain_db = 100 (above max) -> clamped to 24."""
    out = proto.postprocess([100, 200, 300], gain_db=100.0)
    _write(
        "postprocess-gain-clamps-above-24.json",
        {
            "description": "gain_db=100.0 clamped to 24.0 -> same as "
                           "postprocess-max-gain's scaling.",
            "function": "postprocess",
            "samples": [100, 200, 300],
            "gain_db": 100.0,
            "expected_output": out,
        },
    )


def post_gain_nan() -> None:
    """gain_db = NaN -> treated as 0 dB (identity after smoothing)."""
    out = proto.postprocess([10, 20, 30, 40], gain_db=float("nan"))
    _write(
        "postprocess-gain-nan.json",
        {
            "description": "gain_db=NaN -> finite_gain_db=0.0 (NaN check "
                           "in Python baseline); gain = 10^0 = 1.0. "
                           "Output is just the 3-tap smoothing.",
            "function": "postprocess",
            "samples": [10, 20, 30, 40],
            "gain_db": "NaN",
            "expected_output": out,
        },
    )


def post_gain_inf() -> None:
    """gain_db = +inf -> treated as 0 dB."""
    out = proto.postprocess([10, 20, 30, 40], gain_db=float("inf"))
    _write(
        "postprocess-gain-inf.json",
        {
            "description": "gain_db=+inf -> finite_gain_db=0.0 (abs check "
                           "in Python baseline); gain = 10^0 = 1.0. "
                           "Output is just the 3-tap smoothing.",
            "function": "postprocess",
            "samples": [10, 20, 30, 40],
            "gain_db": "Infinity",
            "expected_output": out,
        },
    )


def post_clamp_to_int16() -> None:
    """Samples large enough that post-gain scaling exceeds int16 range.
    The output should clamp to [-32768, 32767]."""
    out = proto.postprocess([30000, 30000, 30000], gain_db=24.0)
    _write(
        "postprocess-clamp-to-int16.json",
        {
            "description": "3 samples [30000, 30000, 30000] with "
                           "gain_db=24.0 -> scaled ~ 475389, far above "
                           "int16 max. Output clamps to +32767.",
            "function": "postprocess",
            "samples": [30000, 30000, 30000],
            "gain_db": 24.0,
            "expected_output": out,
        },
    )


def post_two_samples_no_smoothing() -> None:
    """Two samples (len < 3) -> no 3-tap smoothing applied. Output is
    just the scaled input."""
    out = proto.postprocess([1000, 2000], gain_db=0.0)
    _write(
        "postprocess-two-samples-no-smoothing.json",
        {
            "description": "Two samples (len < 3) -> 3-tap smoothing "
                           "skipped. Output is the input scaled by "
                           "gain=1.0 (gain_db=0.0): [1000, 2000].",
            "function": "postprocess",
            "samples": [1000, 2000],
            "gain_db": 0.0,
            "expected_output": out,
        },
    )


# ---------------------------------------------------------------------------
# FrameAccumulator fixtures
# ---------------------------------------------------------------------------


def frame_empty() -> None:
    fa = proto.FrameAccumulator()
    out = fa.append(b"", 120)
    _write(
        "frame-empty.json",
        {
            "description": "Empty data, frame_size=120 -> empty frame list.",
            "function": "FrameAccumulator.append",
            "data_hex": "",
            "frame_size": 120,
            "append_count": 1,
            "expected_frames_hex": [],
        },
    )


def frame_under_size() -> None:
    """Data shorter than frame_size -> []."""
    fa = proto.FrameAccumulator()
    out = fa.append(bytes.fromhex("0102030405"), 120)
    _write(
        "frame-under-size.json",
        {
            "description": "5 bytes (< frame_size 120) -> empty list. "
                           "Bytes remain pending for the next append.",
            "function": "FrameAccumulator.append",
            "data_hex": "0102030405",
            "frame_size": 120,
            "append_count": 1,
            "expected_frames_hex": [x.hex() for x in out],
        },
    )


def frame_exact_size() -> None:
    """Data exactly frame_size -> 1 frame."""
    fa = proto.FrameAccumulator()
    data = bytes(range(120))
    out = fa.append(data, 120)
    _write(
        "frame-exact-size.json",
        {
            "description": "120 bytes (== frame_size 120) -> exactly one "
                           "frame, pending cleared.",
            "function": "FrameAccumulator.append",
            "data_hex": data.hex(),
            "frame_size": 120,
            "append_count": 1,
            "expected_frames_hex": [x.hex() for x in out],
        },
    )


def frame_multi_from_single() -> None:
    """One large append yielding multiple frames + remainder."""
    fa = proto.FrameAccumulator()
    data = bytes((i % 256) for i in range(120 * 3 + 5))  # 365 bytes
    out = fa.append(data, 120)
    _write(
        "frame-multi-from-single.json",
        {
            "description": "365 bytes, frame_size=120 -> 3 frames of "
                           "120 bytes each + 5 bytes pending. Returns "
                           "[frame0_hex, frame1_hex, frame2_hex].",
            "function": "FrameAccumulator.append",
            "data_hex": data.hex(),
            "frame_size": 120,
            "append_count": 1,
            "expected_frames_hex": [x.hex() for x in out],
        },
    )


def frame_multi_append_across_calls() -> None:
    """Two appends that together yield a frame."""
    fa = proto.FrameAccumulator()
    out1 = fa.append(bytes((i % 256) for i in range(80)), 120)
    out2 = fa.append(bytes((i % 256) for i in range(80, 200)), 120)
    _write(
        "frame-multi-append-across-calls.json",
        {
            "description": "Two appends: first 80 bytes (under size, "
                           "no frames emitted); second 120 bytes -> "
                           "first 40 bytes complete a frame, remaining "
                           "80 bytes pending. Emits 1 frame on the "
                           "second call.",
            "function": "FrameAccumulator.append",
            "data_hex": bytes((i % 256) for i in range(80, 200)).hex(),
            "frame_size": 120,
            "append_count": 2,
            "expected_frames_hex": [x.hex() for x in (out1 + out2)],
        },
    )


def frame_zero_size() -> None:
    """frame_size = 0 -> empty list (frame_size <= 0 guard)."""
    fa = proto.FrameAccumulator()
    out = fa.append(bytes.fromhex("0102030405"), 0)
    _write(
        "frame-zero-size.json",
        {
            "description": "frame_size=0 -> empty list (the "
                           "FrameAccumulator refuses to emit zero-width "
                           "frames).",
            "function": "FrameAccumulator.append",
            "data_hex": "0102030405",
            "frame_size": 0,
            "append_count": 1,
            "expected_frames_hex": [],
        },
    )


def frame_multi_frame_size_10() -> None:
    """Small frame_size to exercise multiple frames."""
    fa = proto.FrameAccumulator()
    data = bytes((i % 256) for i in range(25))  # 25 bytes
    out = fa.append(data, 10)
    _write(
        "frame-multi-frame-size-10.json",
        {
            "description": "25 bytes, frame_size=10 -> 2 complete frames "
                           "(bytes 0..9 and 10..19) + 5 bytes pending. "
                           "Returns 2 frame hex strings.",
            "function": "FrameAccumulator.append",
            "data_hex": data.hex(),
            "frame_size": 10,
            "append_count": 1,
            "expected_frames_hex": [x.hex() for x in out],
        },
    )


def main() -> None:
    # DCHighPassFilter
    dc_empty()
    dc_single_sample()
    dc_two_samples()
    dc_dc_blocked()
    dc_ac_passes()

    # postprocess
    post_empty()
    post_single_default_gain()
    post_zero_gain()
    post_max_gain()
    post_min_gain()
    post_gain_clamps_above_24()
    post_gain_nan()
    post_gain_inf()
    post_clamp_to_int16()
    post_two_samples_no_smoothing()

    # FrameAccumulator
    frame_empty()
    frame_under_size()
    frame_exact_size()
    frame_multi_from_single()
    frame_multi_append_across_calls()
    frame_zero_size()
    frame_multi_frame_size_10()


if __name__ == "__main__":
    main()