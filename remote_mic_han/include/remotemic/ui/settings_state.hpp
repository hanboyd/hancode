#pragma once

#include <string>
#include <vector>

namespace remotemic::ui {

// Qt-independent state used by the unchanged QML settings client. Keeping
// this class free of QObject/Qt headers lets the existing PySide6 runtime own
// rendering while product state moves into the native core.
class SettingsState final {
public:
    SettingsState(std::string hotkey_text,
                  int trigger_mode_index,
                  std::vector<std::string> trigger_hotkeys,
                  int endpoint_count,
                  int selected_endpoint_index,
                  std::vector<std::string> device_ids,
                  std::string selected_device_id,
                  std::vector<std::string> button_ids,
                  std::string selected_button_id);

    [[nodiscard]] const std::string& hotkey_text() const noexcept;
    [[nodiscard]] int trigger_mode_index() const noexcept;
    [[nodiscard]] int selected_endpoint_index() const noexcept;
    [[nodiscard]] int selected_device_index() const noexcept;
    [[nodiscard]] const std::string& selected_device_id() const noexcept;
    [[nodiscard]] const std::string& selected_button_id() const noexcept;

    bool set_hotkey_text(std::string value);
    bool set_trigger_mode_index(int value, bool replace_hotkey = true);
    bool set_trigger_mode_preserving_hotkey(int value);
    bool set_endpoint_selection(int endpoint_count, int selected_index);
    bool set_selected_endpoint_index(int value);
    bool set_selected_device_index(int value);
    bool select_button(const std::string& button_id);

private:
    [[nodiscard]] static int find_index(
        const std::vector<std::string>& values,
        const std::string& value) noexcept;

    std::string hotkey_text_;
    int trigger_mode_index_{0};
    std::vector<std::string> trigger_hotkeys_;
    int endpoint_count_{0};
    int selected_endpoint_index_{-1};
    std::vector<std::string> device_ids_;
    int selected_device_index_{-1};
    std::string selected_device_id_;
    std::vector<std::string> button_ids_;
    std::string selected_button_id_;
};

} // namespace remotemic::ui
