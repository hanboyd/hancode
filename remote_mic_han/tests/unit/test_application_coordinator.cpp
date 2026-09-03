#include "remotemic/app/application_coordinator.hpp"
#include "remotemic/audio/fake_audio_route.hpp"
#include "remotemic/audio/fake_system_state_lease.hpp"
#include "remotemic/ble/fake_ble_transport.hpp"
#include "remotemic/input/fake_host_action_sink.hpp"
#include "remotemic/input/fake_input_source.hpp"

#include <cassert>
#include <chrono>
#include <memory>

using namespace remotemic;

int main() {
    auto ble = std::make_shared<ble::FakeBleTransport>();
    auto audio = std::make_shared<audio::FakeAudioRoute>();
    auto input = std::make_shared<input::FakeInputSource>();
    auto sink = std::make_shared<input::FakeHostActionSink>();
    auto system_lease = std::make_shared<audio::FakeSystemStateLease>();
    app::CoordinatorConfig config{.device_id = "paired-device"};
    app::ApplicationCoordinator coordinator{ble, audio, input, sink, config, system_lease};

    const auto started = coordinator.execute(1, app::CommandKind::start);
    assert(started.ok());
    assert(coordinator.state() == app::CoordinatorState::running);
    assert(ble->connected());

    const auto duplicate = coordinator.execute(1, app::CommandKind::start);
    assert(duplicate.status == app::CommandStatus::duplicate);
    assert(sink->submitted_count() == 0);

    const std::uint8_t caps[] = {0x0b, 0x01, 0x00, 0x02, 0x00, 0x00, 0x78};
    ble->emit(IBleTransport::Channel::control, caps);
    input::InputEvent mic_edge{};
    mic_edge.vk_code = 0x74;
    mic_edge.kind = input::InputEvent::EventKind::KeyDown;
    const std::uint8_t mic[] = {0x08};
    const auto open_tap_started = std::chrono::steady_clock::now();
    input->inject_event_for_test(mic_edge);
    // Auto-repeat and the matching ATVV control message are the same
    // physical hold and must not toggle Typeless a second time.
    input->inject_event_for_test(mic_edge);
    ble->emit(IBleTransport::Channel::control, mic);
    const auto open_tap_elapsed = std::chrono::steady_clock::now() - open_tap_started;
    assert(system_lease->acquire_count == 1);
    assert(system_lease->active());
    assert(sink->recorded_keys().size() == 2);
    assert(open_tap_elapsed >= std::chrono::milliseconds{60});
    assert(!ble->writes().empty());

    input::InputEvent arrow{};
    arrow.vk_code = 0x27;
    arrow.kind = input::InputEvent::EventKind::KeyDown;
    input->inject_event_for_test(arrow);
    arrow.kind = input::InputEvent::EventKind::KeyUp;
    input->inject_event_for_test(arrow);
    const auto keys_after_arrow = sink->recorded_keys();
    assert(keys_after_arrow.size() == 4);
    assert(keys_after_arrow[2].first == 0x27 && keys_after_arrow[2].second);
    assert(keys_after_arrow[3].first == 0x27 && !keys_after_arrow[3].second);
    app::CoordinatorEvent observed{};
    bool saw_arrow_down = false;
    while (coordinator.poll_event(observed))
        saw_arrow_down = saw_arrow_down ||
            (observed.kind == app::CoordinatorEventKind::input &&
             observed.detail == "right:down");
    assert(saw_arrow_down);

    const std::uint8_t audio_start[] = {0x04, 0x00, 0x00, 0x42};
    ble->emit(IBleTransport::Channel::control, audio_start);
    const std::uint8_t audio_stop[] = {0x00};
    ble->emit(IBleTransport::Channel::control, audio_stop);
    // Intermediate audio stop while the remote is still physically held
    // must not close the host recognizer.
    assert(sink->recorded_keys().size() == 4);
    assert(system_lease->active());
    mic_edge.kind = input::InputEvent::EventKind::KeyUp;
    input->inject_event_for_test(mic_edge);
    const auto keys_after_release = sink->recorded_keys();
    assert(keys_after_release.size() == 6);
    assert(keys_after_release[4].first == 0x74 && keys_after_release[4].second);
    assert(keys_after_release[5].first == 0x74 && !keys_after_release[5].second);
    assert(system_lease->restore_count == 1);
    assert(!system_lease->active());

    const auto stopped = coordinator.execute(2, app::CommandKind::stop);
    assert(stopped.ok());
    assert(coordinator.state() == app::CoordinatorState::stopped);
    assert(!ble->connected());

    const auto stale = coordinator.execute(1, app::CommandKind::stop);
    assert(!stale.ok());

    auto failed_ble = std::make_shared<ble::FakeBleTransport>();
    failed_ble->fail_next_connect();
    app::ApplicationCoordinator failed{failed_ble, audio, input, sink, config};
    assert(!failed.execute(1, app::CommandKind::start).ok());
    assert(failed.state() == app::CoordinatorState::faulted);

    auto chord_ble = std::make_shared<ble::FakeBleTransport>();
    auto chord_sink = std::make_shared<input::FakeHostActionSink>();
    chord_sink->set_fail_after_count_for_test(1);
    app::CoordinatorConfig chord_config{
        .device_id = "paired-device", .voice_keys = {0xA2, 0xA4}};
    app::ApplicationCoordinator chord{chord_ble, audio, input, chord_sink, chord_config};
    assert(chord.execute(1, app::CommandKind::start).ok());
    chord_ble->emit(IBleTransport::Channel::control, caps);
    chord_ble->emit(IBleTransport::Channel::control, mic);
    const auto partial = chord_sink->recorded_keys();
    assert(partial.size() == 1);
    assert(partial.front().first == 0xA2 && partial.front().second);
    chord.stop();

    // Windows bridge configuration: the Python low-level-hook owner delivers
    // the Typeless toggle and the physical mic latch. Raw Input, ATVV and
    // stop() must not open/close the voice session or emit any host shortcut.
    auto ext_ble = std::make_shared<ble::FakeBleTransport>();
    auto ext_audio = std::make_shared<audio::FakeAudioRoute>();
    auto ext_input = std::make_shared<input::FakeInputSource>();
    auto ext_sink = std::make_shared<input::FakeHostActionSink>();
    auto ext_lease = std::make_shared<audio::FakeSystemStateLease>();
    app::CoordinatorConfig ext_config{
        .device_id = "paired-device",
        .voice_keys = {0xA2, 0xA4},
        .dispatch_default_input_actions = false,
        .external_voice_edge_owner = true,
        .external_voice_host_action_owner = true};
    app::ApplicationCoordinator ext{ext_ble, ext_audio, ext_input, ext_sink,
                                    ext_config, ext_lease};
    assert(ext.execute(1, app::CommandKind::start).ok());
    input::InputEvent ext_mic_down{};
    ext_mic_down.vk_code = 0x74;
    ext_mic_down.kind = input::InputEvent::EventKind::KeyDown;
    ext_input->inject_event_for_test(ext_mic_down);  // Raw Input must be inert
    ext_ble->emit(IBleTransport::Channel::control, caps);
    ext_ble->emit(IBleTransport::Channel::control, mic);       // ATVV mic inert
    ext_ble->emit(IBleTransport::Channel::control, audio_start);  // ATVV start inert
    assert(ext_sink->recorded_keys().empty());
    assert(!ext_lease->active());
    assert(ext_ble->writes().empty());

    // The Python hook owner drives the latch; auto-repeat collapses.
    ext.handle_physical_mic_edge(true);
    ext.handle_physical_mic_edge(true);
    assert(ext_lease->acquire_count == 1);
    assert(ext_lease->active());
    assert(ext_sink->recorded_keys().empty());  // no duplicate host tap
    assert(!ext_ble->writes().empty());         // MIC_OPEN reached BLE

    // An intermediate ATVV audio stop while held must not close anything.
    ext_ble->emit(IBleTransport::Channel::control, audio_stop);
    assert(ext_lease->active());

    ext.handle_physical_mic_edge(false);
    assert(ext_lease->restore_count == 1);
    assert(!ext_lease->active());
    assert(ext_sink->recorded_keys().empty());

    // A second hold must start the audio route fresh. Before the fix the
    // route stayed open after release, so every later AudioStarted failed
    // with "already started" and all frames were silently dropped.
    ext.handle_physical_mic_edge(true);
    assert(ext_lease->acquire_count == 2);
    ext_ble->emit(IBleTransport::Channel::control, audio_start);
    ext.handle_physical_mic_edge(false);
    assert(ext_lease->restore_count == 2);
    assert(ext_sink->recorded_keys().empty());

    assert(ext.execute(2, app::CommandKind::stop).ok());
    return 0;
}
