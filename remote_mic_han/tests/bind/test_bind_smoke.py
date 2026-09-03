"""Hardware-free smoke test for the remotemic_native binding.

Drives the four translation categories required by ADR-0011 gate 1:

    1. value-type round trip (VersionInfo)
    2. shared_ptr round trip (Counter)
    3. py::function callback
    4. thrown remotemic::Error, one per code

Runs via CMake's add_test(NAME remotemic_bind_smoke ...). Kept deliberately
small: any growth here is a sign that the smoke scaffold is doing real
work, which is not its job.
"""

from __future__ import annotations

import unittest

import remotemic_native as rn


class BindSmokeTests(unittest.TestCase):
    def test_value_type_round_trip(self) -> None:
        info = rn.probe_value_type()
        self.assertEqual(info.product, "RemoteMicWindows")
        self.assertEqual(info.version, "1.0.0")
        self.assertEqual(info.build_number, 1)

    def test_shared_ptr_round_trip(self) -> None:
        counter = rn.probe_shared_ptr()
        self.assertEqual(counter.value(), 0)
        counter.increment()
        self.assertEqual(counter.value(), 1)
        counter.increment(5)
        self.assertEqual(counter.value(), 6)

    def test_callback(self) -> None:
        seen: list[int] = []
        result = rn.probe_callback(seen.append, 42)
        self.assertEqual(result, 42)
        self.assertEqual(seen, [42])

    def test_error_translation_per_code(self) -> None:
        for code in (
            rn.ErrorCode.InvalidArgument,
            rn.ErrorCode.NotFound,
            rn.ErrorCode.Timeout,
            rn.ErrorCode.Internal,
        ):
            with self.subTest(code=code):
                with self.assertRaises(RuntimeError) as ctx:
                    rn.probe_throw(code)
                self.assertTrue(str(ctx.exception))
                self.assertIsInstance(ctx.exception, rn.RemoteMicError)
                self.assertEqual(ctx.exception.code, int(code))
                self.assertEqual(ctx.exception.category, "remotemic")


if __name__ == "__main__":
    unittest.main()
