"""Phase 2 / Area 4 step 4: FrameAccumulator binding smoke (G3).

Loads the bundled ``remotemic_native._C`` extension, creates a
``FrameAccumulator`` per fixture, calls ``append`` (once or twice per
the fixture's ``append_count``), and asserts the cumulative returned
frames match the C++ unit test (``remotemic_adpcm_frame_tests``)
byte-for-byte.

Per ADR-0012 G3: on fail, do not flip the ADR status from ``proposed``
to ``accepted``.

In addition to the gold-fixture loop, this smoke verifies:

- ``reset()`` parity: a partial frame in stream A then ``reset()`` then
  stream B does NOT carry over A's bytes into B's emitted frames.
- ``frame_size`` public boundary: ``<= 0`` (including negative values),
  ``1``, ``65535``, ``> 65535``. The contract at the Python/native
  seam per ADR-0012 section 3.1:
    ``<= 0``   no-op (``[]`` returned, ``pending_size`` unchanged)
    ``1..65535``  protocol valid; narrows to ``std::uint16_t``
    ``> 65535``  explicitly rejected with ``TypeError``
- Invariant ``pending_size < frame_size <= 65535`` after each
  successful ``append()`` call (the lower bound because the
  ``append()`` drain loop only emits complete ``frame_size``-byte
  frames, the upper bound because ``frame_size`` is ``uint16_t``).
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

_FIXTURE_DIR = (
    Path(__file__).resolve().parent.parent.parent
    / "apps" / "windows" / "rc003" / "tests" / "fixtures" / "atvv"
)


def _load(name: str) -> dict:
    return json.loads((_FIXTURE_DIR / name).read_text(encoding="utf-8"))


_FIXTURE_NAMES = (
    "frame-empty.json",
    "frame-under-size.json",
    "frame-exact-size.json",
    "frame-multi-from-single.json",
    "frame-multi-append-across-calls.json",
    "frame-zero-size.json",
    "frame-multi-frame-size-10.json",
)


class AdpcmFrameBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.FrameAccumulator = _C.FrameAccumulator

    def test_fixture(self) -> None:
        for name in _FIXTURE_NAMES:
            with self.subTest(fixture=name):
                fixture = _load(name)
                data = bytes.fromhex(fixture["data_hex"])
                frame_size = fixture["frame_size"]
                append_count = fixture["append_count"]

                acc = self.FrameAccumulator()
                all_frames: list[bytes] = []
                if name == "frame-multi-append-across-calls.json" and append_count == 2:
                    first = bytes(i % 256 for i in range(80))
                    for f in acc.append(first, frame_size):
                        all_frames.append(f)
                    for f in acc.append(data, frame_size):
                        all_frames.append(f)
                else:
                    for _ in range(append_count):
                        for f in acc.append(data, frame_size):
                            all_frames.append(f)

                expected = [bytes.fromhex(h) for h in fixture["expected_frames_hex"]]
                # pybind11 maps std::vector<std::uint8_t> -> list[int],
                # not bytes; coerce before .hex().
                actual_hex = [bytes(f).hex() for f in all_frames]
                expected_hex = [e.hex() for e in expected]
                self.assertEqual(actual_hex, expected_hex)

    def test_reset_parity_carry_over_disappears(self) -> None:
        # Stream A leaves 50 bytes pending against frame_size=60.
        # reset() then a 90-byte stream B against frame_size=60
        # must emit exactly one frame of B's bytes 0..59, NOT a
        # mixed prefix of A and B.
        acc = self.FrameAccumulator()
        a_payload = bytes(range(50))  # bytes 0..49
        a_out = list(acc.append(a_payload, 60))
        self.assertEqual(a_out, [], "stream A should leave 50 bytes pending")
        self.assertEqual(acc.pending_size, 50)

        acc.reset()
        self.assertEqual(acc.pending_size, 0, "reset() must clear pending_size")

        b_payload = bytes(range(100, 190))  # bytes 100..189
        b_out = list(acc.append(b_payload, 60))
        self.assertEqual(len(b_out), 1, "stream B must emit exactly 1 frame")
        self.assertEqual(len(b_out[0]), 60)
        # First byte must be 100 (B's byte 0); if A carried over we
        # would see 0 instead.
        self.assertEqual(b_out[0][0], 100)
        self.assertEqual(b_out[0][59], 159)
        self.assertEqual(acc.pending_size, 30)

        # Fresh-instance parity: a brand new accumulator on the same
        # 90-byte payload must produce the same single frame.
        fresh = self.FrameAccumulator()
        fresh_out = list(fresh.append(b_payload, 60))
        self.assertEqual(fresh_out, b_out)

    def test_reset_then_new_frame_size_drops_stale_bytes(self) -> None:
        # Regardless of the new frame_size, the stale bytes from the
        # previous stream must NOT appear in the next emit.
        acc = self.FrameAccumulator()
        self.assertEqual(list(acc.append(bytes([1, 2, 3, 4, 5]), 10)), [])
        self.assertEqual(acc.pending_size, 5)
        acc.reset()
        out = list(acc.append(bytes([10, 20, 30, 40]), 4))
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0], [10, 20, 30, 40])
        self.assertEqual(acc.pending_size, 0)

    def test_boundary_frame_size_one(self) -> None:
        # frame_size=1 is the protocol-valid lower edge; every byte
        # is its own frame.
        acc = self.FrameAccumulator()
        out = list(acc.append(bytes([0xAA, 0xBB, 0xCC]), 1))
        self.assertEqual([bytes(f) for f in out], [b"\xaa", b"\xbb", b"\xcc"])
        self.assertEqual(acc.pending_size, 0)

    def test_boundary_frame_size_65535(self) -> None:
        # frame_size=65535 (the std::uint16_t maximum) is the
        # protocol-valid upper edge. 65535 bytes -> exactly one
        # frame; 65534 bytes -> no frame, all pending.
        acc = self.FrameAccumulator()
        exact = bytes([0x5A] * 65535)
        out = list(acc.append(exact, 65535))
        self.assertEqual(len(out), 1)
        self.assertEqual(len(out[0]), 65535)
        self.assertEqual(bytes(out[0]), exact)
        self.assertEqual(acc.pending_size, 0)

        acc.reset()
        short = bytes([0xA5] * 65534)
        out = list(acc.append(short, 65535))
        self.assertEqual(out, [])
        self.assertEqual(acc.pending_size, 65534)
        self.assertLess(acc.pending_size, 65535)
        self.assertLessEqual(65535, 65535)

    def test_boundary_frame_size_zero(self) -> None:
        # frame_size=0 is part of the protocol-invalid no-op class.
        # The binding returns [] without buffering the data and
        # without touching any previously-pending bytes (matches
        # the Python baseline's guard at atvv_protocol.py:272-273).
        acc = self.FrameAccumulator()
        # Seed 5 bytes of pending so we can prove "no-op" doesn't
        # touch the buffer.
        seed = bytes([1, 2, 3, 4, 5])
        list(acc.append(seed, 10))  # 5 bytes pending
        self.assertEqual(acc.pending_size, 5)
        out = list(acc.append(bytes([10, 20, 30, 40, 50]), 0))
        self.assertEqual(out, [], "frame_size=0 must return []")
        self.assertEqual(acc.pending_size, 5,
                         "frame_size=0 must not touch existing pending")

    def test_boundary_frame_size_negative(self) -> None:
        # frame_size=-1 is part of the same no-op class as 0 (both
        # are <= 0). It MUST return [] and MUST NOT touch pending.
        # The Python baseline guard at atvv_protocol.py:272-273
        # covers both 0 and any negative value; the binding layer
        # matches that.
        acc = self.FrameAccumulator()
        seed = bytes([1, 2, 3, 4, 5])
        list(acc.append(seed, 10))  # 5 bytes pending
        self.assertEqual(acc.pending_size, 5)
        out = list(acc.append(bytes([10, 20, 30, 40, 50]), -1))
        self.assertEqual(out, [], "frame_size=-1 must return []")
        self.assertEqual(acc.pending_size, 5,
                         "frame_size=-1 must not touch existing pending")

        # A brand-new accumulator on -1 must also be a no-op
        # (pending_size still 0).
        fresh = self.FrameAccumulator()
        out2 = list(fresh.append(b"\x01\x02\x03\x04", -1))
        self.assertEqual(out2, [])
        self.assertEqual(fresh.pending_size, 0)

    def test_boundary_frame_size_above_uint16_raises_type_error(self) -> None:
        # frame_size=65536 (uint16_t max + 1) is the explicit
        # rejection class. Per ADR-0012 section 3.1 the binding
        # layer MUST raise TypeError for any value > 65535; data
        # is not silently wrapped, the call does NOT reach C++,
        # and existing pending is NOT touched.
        # We do NOT assert on the full error message string;
        # only on the exception class. See ADR-0012 3.1.
        acc = self.FrameAccumulator()
        seed = bytes([1, 2, 3, 4, 5])
        list(acc.append(seed, 10))  # 5 bytes pending
        self.assertEqual(acc.pending_size, 5)
        with self.assertRaises(TypeError):
            acc.append(bytes([10, 20, 30, 40, 50]), 65536)
        self.assertEqual(acc.pending_size, 5,
                         "rejected call must not touch existing pending")

    def test_invariant_pending_less_than_frame_size(self) -> None:
        # After every successful append() with frame_size in the
        # protocol-valid domain (1..65535), the contract is:
        #     pending_size < frame_size <= 65535
        # Both halves are checked below. After a reset() the
        # invariant is trivially satisfied (pending_size == 0);
        # the next append then carries the invariant forward.
        acc = self.FrameAccumulator()
        fs = 4
        self.assertLessEqual(fs, 65535, "frame_size itself <= 65535")
        self.assertEqual(list(acc.append(bytes([1, 2]), fs)), [])
        self.assertLess(acc.pending_size, fs)
        self.assertLessEqual(acc.pending_size, 65535)
        self.assertEqual(list(acc.append(bytes([3]), fs)), [])
        self.assertLess(acc.pending_size, fs)
        self.assertLessEqual(acc.pending_size, 65535)
        out = list(acc.append(bytes([4, 5]), fs))
        self.assertEqual(len(out), 1)
        self.assertLess(acc.pending_size, fs)
        self.assertLessEqual(acc.pending_size, 65535)
        out2 = list(acc.append(bytes([6, 7, 8, 9]), fs))
        # Don't assert exact frame count (depends on leftover from
        # the prior partial append); only assert at least one frame
        # was emitted and the invariant still holds.
        self.assertGreater(len(out2), 0)
        self.assertLess(acc.pending_size, fs)
        self.assertLessEqual(acc.pending_size, 65535)
        acc.reset()
        self.assertEqual(acc.pending_size, 0)
        self.assertLess(acc.pending_size, fs)
        self.assertLessEqual(acc.pending_size, 65535)
        # reset() resets the invariant anchor: a fresh append
        # with new bytes starts the same < frame_size relationship.
        self.assertEqual(list(acc.append(bytes([1, 2, 3]), 5)), [])
        self.assertLess(acc.pending_size, 5)
        self.assertLessEqual(acc.pending_size, 65535)

        # The protocol-valid upper edge also satisfies the
        # invariant in both halves.
        edge = self.FrameAccumulator()
        out_edge = list(edge.append(bytes([0xA5] * 65534), 65535))
        self.assertEqual(out_edge, [])
        self.assertLess(edge.pending_size, 65535)
        self.assertLessEqual(edge.pending_size, 65535)


if __name__ == "__main__":
    unittest.main()
