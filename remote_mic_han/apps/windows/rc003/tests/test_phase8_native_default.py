from __future__ import annotations

import os
import unittest
from unittest import mock

from ovb_rc003 import app as app_module


_CHOICE_ENV = "REMOTEMIC_NATIVE_CHOICE_APPLICATION_COORDINATOR"


class Phase8ReleaseDefaultTests(unittest.TestCase):
    def test_default_constructs_only_proven_python_coordinator(self) -> None:
        python = object()
        with (
            mock.patch.dict(os.environ, {}, clear=False),
            mock.patch.object(app_module, "NativeCoordinatorApp") as make_native,
            mock.patch.object(app_module, "RC003App", return_value=python) as make_python,
        ):
            os.environ.pop(_CHOICE_ENV, None)
            selected = app_module._make_application()

        self.assertIs(selected, python)
        make_python.assert_called_once_with()
        make_native.assert_not_called()

    def test_explicit_native_override_is_whole_application_opt_in(self) -> None:
        native = object()
        with (
            mock.patch.dict(os.environ, {_CHOICE_ENV: "native"}),
            mock.patch.object(app_module, "NativeCoordinatorApp", return_value=native) as make_native,
            mock.patch.object(app_module, "RC003App") as make_python,
        ):
            selected = app_module._make_application()

        self.assertIs(selected, native)
        make_native.assert_called_once_with()
        make_python.assert_not_called()

    def test_explicit_native_missing_binding_fails_loud(self) -> None:
        with (
            mock.patch.dict(os.environ, {_CHOICE_ENV: "native"}),
            mock.patch.object(
                app_module,
                "NativeCoordinatorApp",
                side_effect=RuntimeError("native application coordinator is unavailable"),
            ),
            mock.patch.object(app_module, "RC003App") as make_python,
        ):
            with self.assertRaisesRegex(RuntimeError, "native application coordinator"):
                app_module._make_application()

        make_python.assert_not_called()

    def test_shadow_is_rejected_before_any_owner_is_constructed(self) -> None:
        with (
            mock.patch.dict(os.environ, {_CHOICE_ENV: "shadow"}),
            mock.patch.object(app_module, "NativeCoordinatorApp") as make_native,
            mock.patch.object(app_module, "RC003App") as make_python,
        ):
            with self.assertRaisesRegex(RuntimeError, "does not support shadow"):
                app_module._make_application()

        make_native.assert_not_called()
        make_python.assert_not_called()


if __name__ == "__main__":
    unittest.main()
