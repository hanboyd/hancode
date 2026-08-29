"""Hardware-free tests for the Phase 1 module-level switch framework.

Exercises every branch of ``implementation_choice`` and
``choose_implementation`` without touching any real product module:
ATVV / ADPCM migration is phase 2, not phase 1.
"""

from __future__ import annotations

import os
import unittest

from ovb_rc003._remotemic_native_runtime import (
    ImplementationChoice,
    choose_implementation,
    implementation_choice,
)


class ImplementationChoiceTests(unittest.TestCase):
    def test_default_is_python(self) -> None:
        # No env var, no policy entry -> 'python' is the authoritative
        # fallback for every module until phase 8 per plan §1 rule 4.
        self.assertEqual(implementation_choice("atvv_protocol"), "python")
        self.assertEqual(implementation_choice("adpcm_decoder"), "python")
        self.assertEqual(implementation_choice("nonexistent_module"), "python")

    def test_env_override_native(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"] = "native"
        try:
            self.assertEqual(
                implementation_choice("atvv_protocol"), "native"
            )
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"] = old

    def test_env_override_shadow(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = "shadow"
        try:
            self.assertEqual(
                implementation_choice("adpcm_decoder"), "shadow"
            )
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = old

    def test_env_override_invalid_rejected(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"] = "cobol"
        try:
            with self.assertRaises(ValueError):
                implementation_choice("atvv_protocol")
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"] = old

    def test_empty_module_name_rejected(self) -> None:
        with self.assertRaises(ValueError):
            implementation_choice("")


class ChooseImplementationTests(unittest.TestCase):
    def _save_env(self, key: str) -> None:
        self._old_env = os.environ.get(key)
        os.environ.pop(key, None)

    def _restore_env(self, key: str) -> None:
        if self._old_env is None:
            os.environ.pop(key, None)
        else:
            os.environ[key] = self._old_env

    def test_python_choice_returns_python_impl(self) -> None:
        self._save_env("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL")
        try:
            impl = choose_implementation(
                "atvv_protocol",
                python_impl=lambda x: ("py", x),
                native_impl=lambda x: ("na", x),
            )
            self.assertEqual(impl(7), ("py", 7))
        finally:
            self._restore_env("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL")

    def test_native_choice_returns_native_impl(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"] = "native"
        try:
            impl = choose_implementation(
                "atvv_protocol",
                python_impl=lambda x: ("py", x),
                native_impl=lambda x: ("na", x),
            )
            self.assertEqual(impl(7), ("na", 7))
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL"] = old

    def test_shadow_requires_side_effect_free(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = "shadow"
        try:
            with self.assertRaises(RuntimeError):
                choose_implementation(
                    "adpcm_decoder",
                    python_impl=lambda x: x,
                    native_impl=lambda x: x,
                    side_effect_free=False,
                )
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = old

    def test_shadow_passes_on_match(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = "shadow"
        try:
            impl = choose_implementation(
                "adpcm_decoder",
                python_impl=lambda x: x * 2,
                native_impl=lambda x: x * 2,
                side_effect_free=True,
            )
            self.assertEqual(impl(5), 10)
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = old

    def test_shadow_raises_on_mismatch(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = "shadow"
        try:
            impl = choose_implementation(
                "adpcm_decoder",
                python_impl=lambda x: x * 2,
                native_impl=lambda x: x + 1,
                side_effect_free=True,
            )
            with self.assertRaises(RuntimeError):
                impl(5)
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = old

    def test_shadow_raises_on_native_exception(self) -> None:
        old = os.environ.get("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER")
        os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = "shadow"
        try:
            def _native_boom(_: int) -> int:
                raise ValueError("simulated native failure")

            impl = choose_implementation(
                "adpcm_decoder",
                python_impl=lambda x: x * 2,
                native_impl=_native_boom,
                side_effect_free=True,
            )
            with self.assertRaises(RuntimeError):
                impl(5)
        finally:
            if old is None:
                os.environ.pop("REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER", None)
            else:
                os.environ["REMOTEMIC_NATIVE_CHOICE_ADPCM_DECODER"] = old


if __name__ == "__main__":
    unittest.main()