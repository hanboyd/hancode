"""Phase 9 native state behind the unchanged PySide6/QML settings UI."""

from __future__ import annotations

from typing import Sequence

import remotemic_native as native

from ._remotemic_native_runtime import implementation_choice


class _PythonUiSettingsState:
    """Source-run rollback with the exact public native-state surface."""

    _is_native = False

    def __init__(
        self,
        hotkey_text: str,
        trigger_mode_index: int,
        trigger_hotkeys: Sequence[str],
        endpoint_count: int,
        selected_endpoint_index: int,
        device_ids: Sequence[str],
        selected_device_id: str,
        button_ids: Sequence[str],
        selected_button_id: str,
    ) -> None:
        if not trigger_hotkeys or not 0 <= trigger_mode_index < len(trigger_hotkeys):
            raise ValueError("invalid trigger-mode state")
        if endpoint_count < 0 or not -1 <= selected_endpoint_index < endpoint_count:
            raise ValueError("invalid endpoint selection")
        if not device_ids or not selected_device_id:
            raise ValueError("invalid device state")
        if selected_button_id not in button_ids:
            raise ValueError("invalid selected button id")
        self.hotkey_text = hotkey_text
        self.trigger_mode_index = trigger_mode_index
        self._trigger_hotkeys = tuple(trigger_hotkeys)
        self._endpoint_count = endpoint_count
        self.selected_endpoint_index = selected_endpoint_index
        self._device_ids = tuple(device_ids)
        self.selected_device_id = selected_device_id
        self.selected_device_index = (
            self._device_ids.index(selected_device_id)
            if selected_device_id in self._device_ids
            else -1
        )
        self._button_ids = tuple(button_ids)
        self.selected_button_id = selected_button_id

    def set_hotkey_text(self, value: str) -> bool:
        if value == self.hotkey_text:
            return False
        self.hotkey_text = value
        return True

    def set_trigger_mode_index(self, value: int, replace_hotkey: bool = True) -> bool:
        if not 0 <= value < len(self._trigger_hotkeys) or value == self.trigger_mode_index:
            return False
        self.trigger_mode_index = value
        if replace_hotkey:
            self.hotkey_text = self._trigger_hotkeys[value]
        return True

    def set_trigger_mode_preserving_hotkey(self, value: int) -> bool:
        return self.set_trigger_mode_index(value, False)

    def set_endpoint_selection(self, endpoint_count: int, selected_index: int) -> bool:
        if endpoint_count < 0 or not -1 <= selected_index < endpoint_count:
            return False
        changed = (
            endpoint_count != self._endpoint_count
            or selected_index != self.selected_endpoint_index
        )
        self._endpoint_count = endpoint_count
        self.selected_endpoint_index = selected_index
        return changed

    def set_selected_endpoint_index(self, value: int) -> bool:
        if not -1 <= value < self._endpoint_count or value == self.selected_endpoint_index:
            return False
        self.selected_endpoint_index = value
        return True

    def set_selected_device_index(self, value: int) -> bool:
        if not 0 <= value < len(self._device_ids) or value == self.selected_device_index:
            return False
        self.selected_device_index = value
        self.selected_device_id = self._device_ids[value]
        return True

    def select_button(self, button_id: str) -> bool:
        if button_id not in self._button_ids or button_id == self.selected_button_id:
            return False
        self.selected_button_id = button_id
        return True


def make_ui_settings_state(*args: object) -> object:
    choice = implementation_choice("ui_settings_state")
    if choice == "shadow":
        raise RuntimeError("ui_settings_state shadow mode is not a product UI owner")
    if choice == "native" and native._C_AVAILABLE and native.UiSettingsState is not None:
        return native.UiSettingsState(*args)
    return _PythonUiSettingsState(*args)  # type: ignore[arg-type]


__all__ = ["make_ui_settings_state"]
