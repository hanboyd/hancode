#include "remotemic/app/application_coordinator.hpp"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>
#include <variant>

namespace {
std::optional<remotemic::input::ButtonId> button_from_event(
    const remotemic::input::InputEvent& event) noexcept {
    using remotemic::input::ButtonId;
    switch (event.vk_code) {
    case 0x74: return ButtonId::Mic;
    case 0x27: return ButtonId::ArrowRight;
    case 0x25: return ButtonId::ArrowLeft;
    case 0x28: return ButtonId::ArrowDown;
    case 0x26: return ButtonId::ArrowUp;
    case 0x0D: return ButtonId::Ok;
    case 0x24: return ButtonId::Home;
    case 0x5D: return ButtonId::Menu;
    case 0xC0: return ButtonId::Tv;
    case 0x5F: return ButtonId::Power;
    case 0xAD: return ButtonId::VolumeMute;
    case 0xAF: return ButtonId::VolumeUp;
    case 0xAE: return ButtonId::VolumeDown;
    default: break;
    }
    if (event.vk_code == 0xFF) {
        switch (event.scan_code) {
        case 0x5E: return ButtonId::Power;
        case 0x6A: return ButtonId::Back;
        case 0x30: return ButtonId::VolumeUp;
        case 0x2E: return ButtonId::VolumeDown;
        case 0x20: return ButtonId::VolumeMute;
        default: break;
        }
    }
    return std::nullopt;
}

std::string_view button_name(remotemic::input::ButtonId button) noexcept {
    using remotemic::input::ButtonId;
    switch (button) {
    case ButtonId::Power: return "power";
    case ButtonId::ArrowUp: return "up";
    case ButtonId::ArrowDown: return "down";
    case ButtonId::ArrowLeft: return "left";
    case ButtonId::ArrowRight: return "right";
    case ButtonId::Ok: return "ok";
    case ButtonId::Back: return "back";
    case ButtonId::VolumeUp: return "volume_up";
    case ButtonId::VolumeDown: return "volume_down";
    case ButtonId::Home: return "home";
    case ButtonId::Menu: return "menu";
    case ButtonId::Tv: return "tv";
    case ButtonId::Mic: return "mic";
    case ButtonId::VolumeMute: return "volume_mute";
    }
    return "unknown";
}
} // namespace

namespace remotemic::app {

ApplicationCoordinator::ApplicationCoordinator(
    std::shared_ptr<IBleTransport> ble, std::shared_ptr<IAudioRoute> audio,
    std::shared_ptr<input::IInputSource> input_source,
    std::shared_ptr<input::IHostActionSink> host_sink, CoordinatorConfig config,
    std::shared_ptr<ISystemStateLease> voice_system_lease)
    : ble_(std::move(ble)), audio_(std::move(audio)),
      input_source_(std::move(input_source)), host_sink_(std::move(host_sink)),
      voice_system_lease_(std::move(voice_system_lease)),
      config_(std::move(config)), session_(config_.gain_db),
      voice_(config_.trigger_mode) {}

ApplicationCoordinator::~ApplicationCoordinator() { stop(); }

CommandResult ApplicationCoordinator::execute(std::uint64_t sequence,
                                               CommandKind kind) noexcept {
    std::unique_lock lock(mutex_);
    if (sequence == last_command_sequence_ && kind == last_command_kind_) {
        return CommandResult{sequence, CommandStatus::duplicate, last_result_.message};
    }
    if (sequence <= last_command_sequence_) {
        return CommandResult{sequence, CommandStatus::failed, "out-of-order command"};
    }
    CommandResult result{};
    if (kind == CommandKind::start) {
        result = start_command(sequence);
    } else {
        // stop() must be able to release the coordinator mutex while joining
        // the Raw Input pump. Keeping execute()'s recursive lock here can
        // deadlock against an in-flight input callback during shutdown.
        lock.unlock();
        result = stop_command(sequence);
        lock.lock();
    }
    last_command_sequence_ = sequence;
    last_command_kind_ = kind;
    last_result_ = result;
    return result;
}

CommandResult ApplicationCoordinator::start_command(std::uint64_t sequence) noexcept {
    if (state_ == CoordinatorState::running) {
        return {sequence, CommandStatus::completed, "already running"};
    }
    if (!ble_ || !audio_ || !input_source_ || !host_sink_ || config_.device_id.empty()) {
        state_ = CoordinatorState::faulted;
        push_event(CoordinatorEventKind::error, "invalid coordinator configuration");
        return {sequence, CommandStatus::failed, "invalid coordinator configuration"};
    }
    state_ = CoordinatorState::starting;
    if (voice_system_lease_ && !voice_system_lease_->recover_stale()) {
        state_ = CoordinatorState::faulted;
        push_event(CoordinatorEventKind::error, "stale voice system policy recovery failed");
        return {sequence, CommandStatus::failed, "stale voice system policy recovery failed"};
    }
    ble_->set_bytes_received([this](auto channel, auto bytes) { on_bytes(channel, bytes); });
    ble_->set_disconnected([this] { on_disconnected(); });
    input_source_->set_event_sink(&ApplicationCoordinator::input_trampoline, this);

    if (!host_sink_->start()) {
        state_ = CoordinatorState::faulted;
        push_event(CoordinatorEventKind::error, "host action sink start failed");
        return {sequence, CommandStatus::failed, "host action sink start failed"};
    }
    if (!input_source_->start()) {
        host_sink_->stop();
        state_ = CoordinatorState::faulted;
        push_event(CoordinatorEventKind::error, "input source start failed");
        return {sequence, CommandStatus::failed, "input source start failed"};
    }
    if (!ble_->connect(config_.device_id)) {
        input_source_->stop();
        host_sink_->stop();
        state_ = CoordinatorState::faulted;
        push_event(CoordinatorEventKind::error, "BLE connect failed");
        return {sequence, CommandStatus::failed, "BLE connect failed"};
    }
    state_ = CoordinatorState::running;
    push_event(CoordinatorEventKind::started);
    return {sequence, CommandStatus::completed, "started"};
}

CommandResult ApplicationCoordinator::stop_command(std::uint64_t sequence) noexcept {
    stop();
    return {sequence, CommandStatus::completed, "stopped"};
}

void ApplicationCoordinator::stop() noexcept {
    std::unique_lock lock(mutex_);
    if (state_ == CoordinatorState::stopped || state_ == CoordinatorState::stopping) return;
    state_ = CoordinatorState::stopping;
    if (ble_) {
        if (session_.mic_open()) {
            const auto close = session_.mic_close_command();
            (void)ble_->write(close);
        }
        ble_->set_bytes_received({});
        ble_->set_disconnected({});
        ble_->disconnect();
    }
    if (input_source_) {
        input_source_->set_event_sink(nullptr, nullptr);
    }
    // Callback ownership is now detached. Release the mutex before joining
    // backend threads so an already-entered callback can observe `stopping`
    // and return instead of deadlocking the shutdown path.
    lock.unlock();
    if (input_source_) input_source_->stop();
    if (audio_) {
        audio_->drain(std::chrono::milliseconds{500});
        audio_->stop();
        audio_->close();
    }
    if (voice_system_lease_) voice_system_lease_->restore();
    audio_started_ = false;
    physical_mic_down_ = false;
    if (host_sink_) {
        if (const auto action = voice_.reset();
            action && !config_.external_voice_host_action_owner)
            (void)deliver_voice_action(*action);
        host_sink_->cancel_pending();
        host_sink_->stop();
    }
    lock.lock();
    state_ = CoordinatorState::stopped;
    push_event(CoordinatorEventKind::stopped);
}

CoordinatorState ApplicationCoordinator::state() const noexcept {
    std::lock_guard lock(mutex_);
    return state_;
}

bool ApplicationCoordinator::poll_event(CoordinatorEvent& event) noexcept {
    std::lock_guard lock(mutex_);
    if (events_.empty()) return false;
    event = std::move(events_.front());
    events_.pop_front();
    return true;
}

std::size_t ApplicationCoordinator::dropped_event_count() const noexcept {
    std::lock_guard lock(mutex_);
    return dropped_events_;
}

std::uint64_t ApplicationCoordinator::audio_bytes_received() const noexcept {
    return audio_bytes_received_.load(std::memory_order_relaxed);
}

void ApplicationCoordinator::input_trampoline(input::InputEvent event,
                                               void* self) noexcept {
    if (self) static_cast<ApplicationCoordinator*>(self)->on_input(event);
}

void ApplicationCoordinator::on_input(input::InputEvent event) noexcept {
    std::lock_guard lock(mutex_);
    if (state_ != CoordinatorState::running) return;
    const auto button = button_from_event(event);
    const bool pressed = event.kind == input::InputEvent::EventKind::KeyDown;
    push_event(CoordinatorEventKind::input,
               button ? std::string(button_name(*button)) + (pressed ? ":down" : ":up")
                      : "unknown");
    if (!button) return;
    if (*button == input::ButtonId::Mic) {
        if (config_.external_voice_edge_owner) return;
        apply_physical_mic_edge(
            event.kind == input::InputEvent::EventKind::KeyDown);
        return;
    }
    if (!config_.dispatch_default_input_actions) return;
    const auto action = action_resolver_.resolve(*button);
    if (!action || action->kind == input::ResolvedAction::Kind::Disabled) return;
    if (action->kind == input::ResolvedAction::Kind::SystemAction) {
        if (event.kind == input::InputEvent::EventKind::KeyDown &&
            !host_sink_->submit_system_action(action->system_action))
            push_event(CoordinatorEventKind::error, "system action submit failed");
        return;
    }
    const bool down = event.kind == input::InputEvent::EventKind::KeyDown;
    const bool up = event.kind == input::InputEvent::EventKind::KeyUp ||
                    event.kind == input::InputEvent::EventKind::KeyCancel;
    if ((down || up) && !host_sink_->submit_key(
            action->vk_code, down, std::chrono::milliseconds{100}))
        push_event(CoordinatorEventKind::error, "key action submit failed");
}

void ApplicationCoordinator::handle_physical_mic_edge(bool pressed) noexcept {
    std::lock_guard lock(mutex_);
    if (state_ != CoordinatorState::running) return;
    push_event(CoordinatorEventKind::input, pressed ? "mic:down" : "mic:up");
    apply_physical_mic_edge(pressed);
}

void ApplicationCoordinator::apply_physical_mic_edge(bool pressed) noexcept {
    if (pressed) {
        // Windows repeats F5 key-down while the remote remains held.
        // Only the first physical edge opens Typeless and MIC_OPEN.
        if (physical_mic_down_) return;
        physical_mic_down_ = true;
        if (!voice_.active()) (void)open_voice_session();
        return;
    }
    if (!physical_mic_down_) return;
    physical_mic_down_ = false;
    close_voice_session();
}

void ApplicationCoordinator::on_disconnected() noexcept {
    std::lock_guard lock(mutex_);
    if (state_ == CoordinatorState::running) state_ = CoordinatorState::faulted;
    physical_mic_down_ = false;
    if (voice_system_lease_) voice_system_lease_->restore();
    push_event(CoordinatorEventKind::disconnected);
}

void ApplicationCoordinator::on_bytes(IBleTransport::Channel channel,
                                      std::span<const std::uint8_t> bytes) noexcept {
    std::lock_guard lock(mutex_);
    if (state_ != CoordinatorState::running) return;
    try {
        if (channel == IBleTransport::Channel::audio) {
            audio_bytes_received_.fetch_add(bytes.size(), std::memory_order_relaxed);
            auto samples = session_.handle_audio(bytes);
            if (!samples.empty() && audio_started_ && !audio_->write(samples))
                push_event(CoordinatorEventKind::error, "audio write failed");
            return;
        }
        auto event = session_.handle_control(bytes);
        if (const auto* caps = std::get_if<atvv::CapsReceived>(&event)) {
            push_event(CoordinatorEventKind::capabilities,
                       std::to_string(static_cast<int>(caps->capabilities.sample_rate)));
        } else if (std::holds_alternative<atvv::MicButtonPressed>(event)) {
            push_event(CoordinatorEventKind::mic_button);
            // The physical F5 down owns the host toggle. ATVV is a duplicate
            // protocol signal for the same hold and must not toggle Typeless
            // again. Retain it only as a fallback when no physical edge was
            // observed on this Windows/device combination.
            if (!config_.external_voice_edge_owner &&
                !physical_mic_down_ && !voice_.active())
                (void)open_voice_session();
        } else if (const auto* started = std::get_if<atvv::AudioStarted>(&event)) {
            (void)started;
            if (!audio_started_) {
                const auto* caps = session_.capabilities();
                PcmFormat format{};
                if (caps) format.sample_rate = static_cast<std::uint32_t>(caps->sample_rate);
                audio_started_ = audio_->start(format);
                if (!audio_started_ &&
                    audio_->last_error() == "already started") {
                    // The RC003 emits a fragmented AudioStopped/AudioStarted
                    // pair mid-hold. The route is still up from that same
                    // hold; frames can flow into it.
                    audio_started_ = true;
                }
                if (!audio_started_)
                    push_event(CoordinatorEventKind::error,
                               "audio start failed: " + audio_->last_error());
            }
            if (!config_.external_voice_edge_owner && !voice_.active()) {
                (void)open_voice_session();
            }
            push_event(CoordinatorEventKind::audio_started);
        } else if (std::holds_alternative<atvv::AudioStopped>(event)) {
            audio_started_ = false;
            // RC003 can emit a short/fragmented stop while the physical
            // button is still held. Keep Typeless open until the real up.
            if (!config_.external_voice_edge_owner && !physical_mic_down_)
                close_voice_session();
            push_event(CoordinatorEventKind::audio_stopped);
        }
    } catch (const std::exception& exc) {
        push_event(CoordinatorEventKind::error, exc.what());
    } catch (...) {
        push_event(CoordinatorEventKind::error, "unknown coordinator callback failure");
    }
}

bool ApplicationCoordinator::open_voice_session() noexcept {
    if (voice_.active()) return true;
    if (voice_system_lease_ && !voice_system_lease_->acquire()) {
        push_event(CoordinatorEventKind::error, "voice system policy acquire failed");
        return false;
    }
    const auto action = voice_.on_mic_button_pressed();
    if (!config_.external_voice_host_action_owner &&
        !deliver_voice_action(action)) {
        voice_.cancel_pending();
        if (voice_system_lease_) voice_system_lease_->restore();
        push_event(CoordinatorEventKind::error, "voice host shortcut delivery failed");
        return false;
    }
    if (!ble_->write(session_.mic_open_command())) {
        if (const auto close = voice_.reset();
            close && !config_.external_voice_host_action_owner)
            (void)deliver_voice_action(*close);
        if (voice_system_lease_) voice_system_lease_->restore();
        push_event(CoordinatorEventKind::error, "MIC_OPEN write failed");
        return false;
    }
    return true;
}

void ApplicationCoordinator::close_voice_session() noexcept {
    if (!voice_.active()) return;
    if (audio_) {
        audio_->drain(std::chrono::milliseconds{500});
        // Fully tear the route down so the next hold can start it fresh.
        // Leaving it running makes every later start() fail with
        // "already started" and silently drops all audio frames.
        audio_->stop();
        audio_->close();
        audio_started_ = false;
    }
    if (const auto action = voice_.on_audio_stopped()) {
        if (!config_.external_voice_host_action_owner &&
            !deliver_voice_action(*action)) {
            voice_.restore_pending(*action);
            push_event(CoordinatorEventKind::error,
                       "voice closing shortcut delivery failed");
            return;
        }
    }
    if (voice_system_lease_) voice_system_lease_->restore();
}

bool ApplicationCoordinator::deliver_voice_action(voice::VoiceHostAction action) noexcept {
    constexpr auto deadline = std::chrono::milliseconds{100};
    // Typeless is toggle-on-chord: each open/close action must be a complete
    // key tap with a real hold window. Queueing down+up in one SendInput batch
    // is observably ignored. Keep the accepted Python baseline's 70 ms window
    // while the RC003 itself remains hold-to-talk.
    constexpr auto tap_hold = std::chrono::milliseconds{70};
    if (config_.voice_keys.empty()) return false;
    const auto press_chord = [&]() noexcept {
        std::size_t pressed = 0;
        for (; pressed < config_.voice_keys.size(); ++pressed) {
            if (!host_sink_->submit_key(config_.voice_keys[pressed], true, deadline)) {
                while (pressed > 0) {
                    --pressed;
                    (void)host_sink_->submit_key(config_.voice_keys[pressed], false, deadline);
                }
                return false;
            }
        }
        return true;
    };
    const auto release_chord = [&]() noexcept {
        bool ok = true;
        for (auto it = config_.voice_keys.rbegin(); it != config_.voice_keys.rend(); ++it)
            ok = host_sink_->submit_key(*it, false, deadline) && ok;
        return ok;
    };
    switch (action) {
    case voice::VoiceHostAction::Tap: {
        if (!press_chord()) return false;
        std::this_thread::sleep_for(tap_hold);
        return release_chord();
    }
    case voice::VoiceHostAction::KeyDown:
        return press_chord();
    case voice::VoiceHostAction::KeyUp:
        return release_chord();
    }
    return false;
}

void ApplicationCoordinator::push_event(CoordinatorEventKind kind,
                                        std::string detail) noexcept {
    if (events_.size() == event_capacity_) {
        events_.pop_front();
        ++dropped_events_;
    }
    events_.push_back({next_event_sequence_++, kind, std::move(detail)});
}

} // namespace remotemic::app
