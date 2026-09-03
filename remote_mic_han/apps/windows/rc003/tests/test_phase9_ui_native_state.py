from __future__ import annotations

import hashlib
import json
from pathlib import Path
import unittest

import remotemic_native as native

from ovb_rc003.ui_settings_state_native import make_ui_settings_state


class Phase9UiNativeStateTests(unittest.TestCase):
    def _make(self):
        return make_ui_settings_state(
            "lctrl+lalt",
            0,
            ["lctrl+lalt", "ralt"],
            2,
            0,
            ["xiaomi-rc003", "dji-mic-2"],
            "xiaomi-rc003",
            ["ok", "power", "mic"],
            "ok",
        )

    def test_compiled_build_routes_ui_state_to_cpp(self) -> None:
        state = self._make()
        if native._C_AVAILABLE:
            self.assertEqual(type(state).__name__, "UiSettingsState")

    def test_connection_page_state_transitions_match_existing_contract(self) -> None:
        state = self._make()
        self.assertTrue(state.set_trigger_mode_index(1))
        self.assertEqual(state.hotkey_text, "ralt")
        self.assertTrue(state.set_hotkey_text("lctrl+lwin"))
        self.assertTrue(state.set_trigger_mode_preserving_hotkey(0))
        self.assertEqual(state.hotkey_text, "lctrl+lwin")
        self.assertTrue(state.set_selected_endpoint_index(1))
        self.assertFalse(state.set_selected_endpoint_index(2))

    def test_mapping_and_device_selection_are_single_owner(self) -> None:
        state = self._make()
        self.assertTrue(state.select_button("power"))
        self.assertEqual(state.selected_button_id, "power")
        self.assertFalse(state.select_button("unknown"))
        self.assertTrue(state.set_selected_device_index(1))
        self.assertEqual(state.selected_device_id, "dji-mic-2")


class Phase9QmlCopyContractTests(unittest.TestCase):
    def test_every_qml_file_is_byte_identical_to_the_accepted_ui(self) -> None:
        test_dir = Path(__file__).resolve().parent
        expected = json.loads(
            (test_dir / "fixtures" / "phase9-qml-copy-contract.json").read_text(
                encoding="utf-8"
            )
        )
        qml_dir = test_dir.parent / "src" / "ovb_rc003" / "qml"
        actual_names = {
            path.relative_to(qml_dir).as_posix() for path in qml_dir.rglob("*.qml")
        }
        self.assertEqual(actual_names, set(expected))
        for name, digest in expected.items():
            with self.subTest(name=name):
                self.assertEqual(
                    hashlib.sha256((qml_dir / name).read_bytes()).hexdigest(), digest
                )


if __name__ == "__main__":
    unittest.main()
