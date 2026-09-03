#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "remotemic/atvv/session.hpp"
#include "remotemic/interfaces/audio_route.hpp"
#include "remotemic/interfaces/ble_transport.hpp"
#include "remotemic/interfaces/system_state_lease.hpp"
#include "remotemic/input/i_host_action_sink.hpp"
#include "remotemic/input/i_input_source.hpp"
#include "remotemic/input/action_resolver.hpp"
#include "remotemic/voice/voice_controller.hpp"

namespace remotemic::app {

enum class CoordinatorState : std::uint8_t { stopped, starting, running, stopping, faulted };
enum class CommandKind : std::uint8_t { start, stop };
enum class CommandStatus : std::uint8_t { completed, duplicate, failed };
enum class CoordinatorEventKind : std::uint8_t {
    started, stopped, capabilities, mic_button, audio_started, audio_stopped,
    disconnected, input, error
};

struct CoordinatorConfig {
    std::string device_id;
    std::vector<std::uint16_t> voice_keys{0x74}; // ordered chord, default VK_F5
    voice::VoiceTriggerMode trigger_mode{voice::VoiceTriggerMode::Toggle};
    double gain_db{10.0};
    bool dispatch_default_input_actions{true};
    // The Windows bridge can use its low-level F5 guard as the authoritative
    // RC003 mic edge source. In that mode Raw Input and ATVV mic messages are
    // duplicates/fallbacks and must not compete with the physical hold latch.
    bool external_voice_edge_owner{false};
    // When the external edge owner must deliver the Typeless shortcut before
    // it can wait on the coordinator/audio mutex, the native coordinator owns
    // BLE/audio state only and must not emit a duplicate host shortcut.
    bool external_voice_host_action_owner{false};
};

struct CommandResult {
    std::uint64_t sequence{0};
    CommandStatus status{CommandStatus::failed};
    std::string message;
    [[nodiscard]] bool ok() const noexcept {
        return status == CommandStatus::completed || status == CommandStatus::duplicate;
    }
};

struct CoordinatorEvent {
    std::uint64_t sequence{0};
    CoordinatorEventKind kind{CoordinatorEventKind::error};
    std::string detail;
};

class ApplicationCoordinator final {
public:
    ApplicationCoordinator(std::shared_ptr<IBleTransport> ble,
                           std::shared_ptr<IAudioRoute> audio,
                           std::shared_ptr<input::IInputSource> input_source,
                           std::shared_ptr<input::IHostActionSink> host_sink,
                           CoordinatorConfig config,
                           std::shared_ptr<ISystemStateLease> voice_system_lease = {});
    ~ApplicationCoordinator();

    ApplicationCoordinator(const ApplicationCoordinator&) = delete;
    ApplicationCoordinator& operator=(const ApplicationCoordinator&) = delete;

    CommandResult execute(std::uint64_t sequence, CommandKind kind) noexcept;
    void stop() noexcept;
    void handle_physical_mic_edge(bool pressed) noexcept;
    [[nodiscard]] CoordinatorState state() const noexcept;
    [[nodiscard]] bool poll_event(CoordinatorEvent& event) noexcept;
    [[nodiscard]] std::size_t dropped_event_count() const noexcept;
    // Raw BLE audio-channel bytes observed since start (diagnostic surface).
    [[nodiscard]] std::uint64_t audio_bytes_received() const noexcept;

private:
    static void input_trampoline(input::InputEvent event, void* self) noexcept;
    CommandResult start_command(std::uint64_t sequence) noexcept;
    CommandResult stop_command(std::uint64_t sequence) noexcept;
    void on_bytes(IBleTransport::Channel channel,
                  std::span<const std::uint8_t> bytes) noexcept;
    void on_disconnected() noexcept;
    void on_input(input::InputEvent event) noexcept;
    void apply_physical_mic_edge(bool pressed) noexcept;
    bool open_voice_session() noexcept;
    void close_voice_session() noexcept;
    bool deliver_voice_action(voice::VoiceHostAction action) noexcept;
    void push_event(CoordinatorEventKind kind, std::string detail = {}) noexcept;

    std::shared_ptr<IBleTransport> ble_;
    std::shared_ptr<IAudioRoute> audio_;
    std::shared_ptr<input::IInputSource> input_source_;
    std::shared_ptr<input::IHostActionSink> host_sink_;
    std::shared_ptr<ISystemStateLease> voice_system_lease_;
    CoordinatorConfig config_;
    atvv::Session session_;
    voice::VoiceController voice_;
    input::DefaultActionResolver action_resolver_;

    mutable std::recursive_mutex mutex_;
    CoordinatorState state_{CoordinatorState::stopped};
    std::uint64_t last_command_sequence_{0};
    CommandKind last_command_kind_{CommandKind::stop};
    CommandResult last_result_{};
    std::uint64_t next_event_sequence_{1};
    std::deque<CoordinatorEvent> events_;
    std::size_t dropped_events_{0};
    bool audio_started_{false};
    std::atomic<std::uint64_t> audio_bytes_received_{0};
    // RC003 is physically hold-to-talk even when the host recognizer uses a
    // toggle shortcut. This latch collapses F5 auto-repeat and makes the
    // final physical key-up, not an intermediate ATVV notification, own the
    // closing Typeless tap.
    bool physical_mic_down_{false};
    static constexpr std::size_t event_capacity_ = 128;
};

} // namespace remotemic::app
