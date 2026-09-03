#include "remotemic/ble/winrt_ble_transport.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <objbase.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>
#endif

namespace remotemic::ble {

class WinRTBleTransport::Impl final
    : public std::enable_shared_from_this<WinRTBleTransport::Impl> {
public:
    void set_bytes_received(BytesReceived callback) {
        std::lock_guard lock{callback_mutex_};
        bytes_received_ = std::move(callback);
    }

    void set_disconnected(Disconnected callback) {
        std::lock_guard lock{callback_mutex_};
        disconnected_ = std::move(callback);
    }

    bool connect(std::string_view device_id) {
#ifdef _WIN32
        if (device_id.empty()) {
            return false;
        }
        std::lock_guard lifecycle_lock{lifecycle_mutex_};
        disconnect_locked();
        try {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            } catch (const winrt::hresult_changed_state&) {
                // The host may already own this thread's COM apartment.
            }

            using namespace winrt::Windows::Devices::Bluetooth;
            using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

            device_ = BluetoothLEDevice::FromIdAsync(
                winrt::to_hstring(device_id)).get();
            if (!device_) {
                disconnect_locked();
                return false;
            }

            const auto weak = weak_from_this();
            connection_token_ = device_.ConnectionStatusChanged(
                [weak](const BluetoothLEDevice& sender, const auto&) noexcept {
                    if (const auto self = weak.lock();
                        self && sender.ConnectionStatus() == BluetoothConnectionStatus::Disconnected) {
                        self->connected_.store(false);
                        self->enqueue(Event{.disconnected = true});
                    }
                });
            connection_handler_registered_ = true;

            const auto services = device_.GetGattServicesForUuidAsync(
                voice_service_uuid(), BluetoothCacheMode::Uncached).get();
            if (services.Status() != GattCommunicationStatus::Success ||
                services.Services().Size() == 0) {
                disconnect_locked();
                return false;
            }
            service_ = services.Services().GetAt(0);

            const auto characteristics = service_.GetCharacteristicsAsync(
                BluetoothCacheMode::Uncached).get();
            if (characteristics.Status() != GattCommunicationStatus::Success) {
                disconnect_locked();
                return false;
            }
            for (const auto& characteristic : characteristics.Characteristics()) {
                if (characteristic.Uuid() == voice_tx_uuid()) {
                    tx_ = characteristic;
                } else if (characteristic.Uuid() == voice_audio_uuid()) {
                    audio_ = characteristic;
                } else if (characteristic.Uuid() == voice_control_uuid()) {
                    control_ = characteristic;
                }
            }
            if (!tx_ || !audio_ || !control_) {
                disconnect_locked();
                return false;
            }

            audio_token_ = audio_.ValueChanged(
                [weak](const auto&, const GattValueChangedEventArgs& args) noexcept {
                    if (const auto self = weak.lock()) {
                        self->copy_and_enqueue(Channel::audio, args.CharacteristicValue());
                    }
                });
            audio_handler_registered_ = true;
            control_token_ = control_.ValueChanged(
                [weak](const auto&, const GattValueChangedEventArgs& args) noexcept {
                    if (const auto self = weak.lock()) {
                        self->copy_and_enqueue(Channel::control, args.CharacteristicValue());
                    }
                });
            control_handler_registered_ = true;

            constexpr auto notify =
                GattClientCharacteristicConfigurationDescriptorValue::Notify;
            if (audio_.WriteClientCharacteristicConfigurationDescriptorAsync(notify).get()
                    != GattCommunicationStatus::Success ||
                control_.WriteClientCharacteristicConfigurationDescriptorAsync(notify).get()
                    != GattCommunicationStatus::Success) {
                disconnect_locked();
                return false;
            }

            start_dispatcher();
            connected_.store(true);
            static constexpr std::uint8_t get_capabilities[]{
                0x0A, 0x01, 0x00, 0x00, 0x03, 0x03};
            if (!write_locked(get_capabilities)) {
                disconnect_locked();
                return false;
            }
            return true;
        } catch (...) {
            disconnect_locked();
            return false;
        }
#else
        (void)device_id;
        return false;
#endif
    }

    void disconnect() noexcept {
        std::lock_guard lifecycle_lock{lifecycle_mutex_};
        disconnect_locked();
    }

    bool write(std::span<const std::uint8_t> bytes) {
        std::lock_guard lifecycle_lock{lifecycle_mutex_};
        if (!connected_.load()) {
            return false;
        }
        return write_locked(bytes);
    }

    [[nodiscard]] bool connected() const noexcept {
        return connected_.load();
    }

    [[nodiscard]] std::size_t dropped_notification_count() const noexcept {
        return dropped_.load();
    }

private:
    struct Event {
        Channel channel{Channel::control};
        std::vector<std::uint8_t> payload;
        bool disconnected{false};
    };

    static constexpr std::size_t max_queue_depth = 64;

#ifdef _WIN32
    static winrt::guid voice_service_uuid() noexcept {
        return {0xab5e0001, 0x5a21, 0x4f05, {0xbc, 0x7d, 0xaf, 0x01, 0xf6, 0x17, 0xb6, 0x64}};
    }
    static winrt::guid voice_tx_uuid() noexcept {
        return {0xab5e0002, 0x5a21, 0x4f05, {0xbc, 0x7d, 0xaf, 0x01, 0xf6, 0x17, 0xb6, 0x64}};
    }
    static winrt::guid voice_audio_uuid() noexcept {
        return {0xab5e0003, 0x5a21, 0x4f05, {0xbc, 0x7d, 0xaf, 0x01, 0xf6, 0x17, 0xb6, 0x64}};
    }
    static winrt::guid voice_control_uuid() noexcept {
        return {0xab5e0004, 0x5a21, 0x4f05, {0xbc, 0x7d, 0xaf, 0x01, 0xf6, 0x17, 0xb6, 0x64}};
    }

    void copy_and_enqueue(
        Channel channel,
        const winrt::Windows::Storage::Streams::IBuffer& buffer) noexcept {
        try {
            auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
            std::vector<std::uint8_t> payload(buffer.Length());
            reader.ReadBytes(payload);
            enqueue(Event{.channel = channel, .payload = std::move(payload)});
        } catch (...) {
            ++dropped_;
        }
    }
#endif

    void enqueue(Event event) noexcept {
        {
            std::lock_guard lock{queue_mutex_};
            if (queue_.size() == max_queue_depth) {
                queue_.pop_front();
                ++dropped_;
            }
            queue_.push_back(std::move(event));
        }
        queue_cv_.notify_one();
    }

    void start_dispatcher() {
        if (dispatcher_.joinable()) {
            return;
        }
        dispatcher_ = std::jthread([this](std::stop_token stop) {
            // Coordinator callbacks run on this thread, and the audio route
            // creates its MMDeviceEnumerator here. Without an apartment,
            // CoCreateInstance fails with CO_E_NOTINITIALIZED and every
            // AudioStarted turns into "audio start failed".
#ifdef _WIN32
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
            while (!stop.stop_requested()) {
                Event event;
                {
                    std::unique_lock lock{queue_mutex_};
                    queue_cv_.wait(lock, stop, [this] { return !queue_.empty(); });
                    if (stop.stop_requested()) {
                        break;
                    }
                    event = std::move(queue_.front());
                    queue_.pop_front();
                }

                BytesReceived bytes_callback;
                Disconnected disconnected_callback;
                {
                    std::lock_guard lock{callback_mutex_};
                    bytes_callback = bytes_received_;
                    disconnected_callback = disconnected_;
                }
                try {
                    if (event.disconnected) {
                        if (disconnected_callback) {
                            disconnected_callback();
                        }
                    } else if (bytes_callback) {
                        bytes_callback(event.channel, event.payload);
                    }
                } catch (...) {
                    // A consumer callback cannot terminate the transport thread.
                }
            }
#ifdef _WIN32
            CoUninitialize();
#endif
        });
    }

    bool write_locked(std::span<const std::uint8_t> bytes) noexcept {
#ifdef _WIN32
        if (!tx_ || bytes.empty()) {
            return false;
        }
        try {
            winrt::Windows::Storage::Streams::DataWriter writer;
            writer.WriteBytes(bytes);
            const auto result = tx_.WriteValueWithResultAsync(writer.DetachBuffer()).get();
            return result.Status() == winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::Success;
        } catch (...) {
            return false;
        }
#else
        (void)bytes;
        return false;
#endif
    }

    void disconnect_locked() noexcept {
        connected_.store(false);
#ifdef _WIN32
        using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
        constexpr auto none =
            GattClientCharacteristicConfigurationDescriptorValue::None;
        try {
            if (audio_) {
                (void)audio_.WriteClientCharacteristicConfigurationDescriptorAsync(none).get();
            }
        } catch (...) {}
        try {
            if (control_) {
                (void)control_.WriteClientCharacteristicConfigurationDescriptorAsync(none).get();
            }
        } catch (...) {}
        try {
            if (audio_ && audio_handler_registered_) {
                audio_.ValueChanged(audio_token_);
            }
        } catch (...) {}
        try {
            if (control_ && control_handler_registered_) {
                control_.ValueChanged(control_token_);
            }
        } catch (...) {}
        try {
            if (device_ && connection_handler_registered_) {
                device_.ConnectionStatusChanged(connection_token_);
            }
        } catch (...) {}
        audio_handler_registered_ = false;
        control_handler_registered_ = false;
        connection_handler_registered_ = false;
        try { if (service_) service_.Close(); } catch (...) {}
        try { if (device_) device_.Close(); } catch (...) {}
        tx_ = nullptr;
        audio_ = nullptr;
        control_ = nullptr;
        service_ = nullptr;
        device_ = nullptr;
#endif
        if (dispatcher_.joinable()) {
            dispatcher_.request_stop();
            queue_cv_.notify_all();
            dispatcher_.join();
        }
        std::lock_guard queue_lock{queue_mutex_};
        queue_.clear();
    }

    mutable std::mutex lifecycle_mutex_;
    mutable std::mutex callback_mutex_;
    std::mutex queue_mutex_;
    std::condition_variable_any queue_cv_;
    std::deque<Event> queue_;
    std::jthread dispatcher_;
    BytesReceived bytes_received_;
    Disconnected disconnected_;
    std::atomic<bool> connected_{false};
    std::atomic<std::size_t> dropped_{0};

#ifdef _WIN32
    winrt::Windows::Devices::Bluetooth::BluetoothLEDevice device_{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceService service_{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic tx_{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic audio_{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic control_{nullptr};
    winrt::event_token connection_token_{};
    winrt::event_token audio_token_{};
    winrt::event_token control_token_{};
    bool connection_handler_registered_{false};
    bool audio_handler_registered_{false};
    bool control_handler_registered_{false};
#endif
};

WinRTBleTransport::WinRTBleTransport() : impl_(std::make_shared<Impl>()) {}

WinRTBleTransport::~WinRTBleTransport() {
    impl_->disconnect();
}

void WinRTBleTransport::set_bytes_received(BytesReceived callback) {
    impl_->set_bytes_received(std::move(callback));
}

void WinRTBleTransport::set_disconnected(Disconnected callback) {
    impl_->set_disconnected(std::move(callback));
}

bool WinRTBleTransport::connect(std::string_view device_id) {
    return impl_->connect(device_id);
}

void WinRTBleTransport::disconnect() noexcept {
    impl_->disconnect();
}

bool WinRTBleTransport::write(std::span<const std::uint8_t> bytes) {
    return impl_->write(bytes);
}

bool WinRTBleTransport::connected() const noexcept {
    return impl_->connected();
}

std::size_t WinRTBleTransport::dropped_notification_count() const noexcept {
    return impl_->dropped_notification_count();
}

} // namespace remotemic::ble
