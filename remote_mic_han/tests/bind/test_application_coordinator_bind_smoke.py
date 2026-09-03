from __future__ import annotations

import unittest

import remotemic_native as native


@unittest.skipUnless(native._C_AVAILABLE and native.ApplicationCoordinator, "binding unavailable")
class ApplicationCoordinatorBindSmoke(unittest.TestCase):
    def test_construct_and_idempotent_stop(self) -> None:
        service = native.ApplicationCoordinator("not-connected", "CABLE Input")
        result = service.execute(1, native.CoordinatorCommandKind.Stop)
        self.assertTrue(result.ok)
        duplicate = service.execute(1, native.CoordinatorCommandKind.Stop)
        self.assertEqual(duplicate.status, native.CoordinatorCommandStatus.Duplicate)


if __name__ == "__main__":
    unittest.main()
