#include "remotemic/ui/settings_state.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace remotemic::ui {

SettingsState::SettingsState(
    std::string hotkey_text,
    int trigger_mode_index,
    std::vector<std::string> trigger_hotkeys,
    int endpoint_count,
    int selected_endpoint_index,
    std::vector<std::string> device_ids,
    std::string selected_device_id,
    std::vector<std::string> button_ids,
    std::string selected_button_id)
    : hotkey_text_(std::move(hotkey_text)),
      trigger_mode_index_(trigger_mode_index),
      trigger_hotkeys_(std::move(trigger_hotkeys)),
      endpoint_count_(endpoint_count),
      selected_endpoint_index_(selected_endpoint_index),
      device_ids_(std::move(device_ids)),
      selected_device_id_(std::move(selected_device_id)),
      button_ids_(std::move(button_ids)),
      selected_button_id_(std::move(selected_button_id)) {
    if (trigger_hotkeys_.empty() || trigger_mode_index_ < 0 ||
        trigger_mode_index_ >= static_cast<int>(trigger_hotkeys_.size())) {
        throw std::invalid_argument("invalid trigger-mode state");
    }
    if (endpoint_count_ < 0 || selected_endpoint_index_ < -1 ||
        selected_endpoint_index_ >= endpoint_count_) {
        throw std::invalid_argument("invalid endpoint selection");
    }
    if (device_ids_.empty()) {
        throw std::invalid_argument("device id list must not be empty");
    }
    selected_device_index_ = find_index(device_ids_, selected_device_id_);
    if (selected_device_index_ < 0 && selected_device_id_.empty()) {
        throw std::invalid_argument("selected device id must not be empty");
    }
    if (button_ids_.empty() ||
        find_index(button_ids_, selected_button_id_) < 0) {
        throw std::invalid_argument("invalid selected button id");
    }
}

const std::string& SettingsState::hotkey_text() const noexcept {
    return hotkey_text_;
}

int SettingsState::trigger_mode_index() const noexcept {
    return trigger_mode_index_;
}

int SettingsState::selected_endpoint_index() const noexcept {
    return selected_endpoint_index_;
}

int SettingsState::selected_device_index() const noexcept {
    return selected_device_index_;
}

const std::string& SettingsState::selected_device_id() const noexcept {
    return selected_device_id_;
}

const std::string& SettingsState::selected_button_id() const noexcept {
    return selected_button_id_;
}

bool SettingsState::set_hotkey_text(std::string value) {
    if (value == hotkey_text_) return false;
    hotkey_text_ = std::move(value);
    return true;
}

bool SettingsState::set_trigger_mode_index(int value, bool replace_hotkey) {
    if (value < 0 || value >= static_cast<int>(trigger_hotkeys_.size()) ||
        value == trigger_mode_index_) {
        return false;
    }
    trigger_mode_index_ = value;
    if (replace_hotkey) hotkey_text_ = trigger_hotkeys_[value];
    return true;
}

bool SettingsState::set_trigger_mode_preserving_hotkey(int value) {
    return set_trigger_mode_index(value, false);
}

bool SettingsState::set_endpoint_selection(int endpoint_count,
                                           int selected_index) {
    if (endpoint_count < 0 || selected_index < -1 ||
        selected_index >= endpoint_count) {
        return false;
    }
    const bool changed = endpoint_count != endpoint_count_ ||
                         selected_index != selected_endpoint_index_;
    endpoint_count_ = endpoint_count;
    selected_endpoint_index_ = selected_index;
    return changed;
}

bool SettingsState::set_selected_endpoint_index(int value) {
    if (value < -1 || value >= endpoint_count_ ||
        value == selected_endpoint_index_) {
        return false;
    }
    selected_endpoint_index_ = value;
    return true;
}

bool SettingsState::set_selected_device_index(int value) {
    if (value < 0 || value >= static_cast<int>(device_ids_.size()) ||
        value == selected_device_index_) {
        return false;
    }
    selected_device_index_ = value;
    selected_device_id_ = device_ids_[value];
    return true;
}

bool SettingsState::select_button(const std::string& button_id) {
    if (button_id == selected_button_id_ ||
        find_index(button_ids_, button_id) < 0) {
        return false;
    }
    selected_button_id_ = button_id;
    return true;
}

int SettingsState::find_index(const std::vector<std::string>& values,
                              const std::string& value) noexcept {
    const auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) return -1;
    return static_cast<int>(std::distance(values.begin(), it));
}

} // namespace remotemic::ui
