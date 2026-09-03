#include "remotemic/ble/fake_ble_transport.hpp"

#include <utility>

namespace remotemic::ble {

void FakeBleTransport::set_bytes_received(BytesReceived callback) {
    std::lock_guard lock{mutex_};
    bytes_received_ = std::move(callback);
}

void FakeBleTransport::set_disconnected(Disconnected callback) {
    std::lock_guard lock{mutex_};
    disconnected_ = std::move(callback);
}

bool FakeBleTransport::connect(std::string_view device_id) {
    std::lock_guard lock{mutex_};
    if (fail_next_connect_ || device_id.empty()) {
        fail_next_connect_ = false;
        return false;
    }
    selected_device_id_.assign(device_id);
    connected_ = true;
    return true;
}

void FakeBleTransport::disconnect() noexcept {
    std::lock_guard lock{mutex_};
    connected_ = false;
}

bool FakeBleTransport::write(std::span<const std::uint8_t> bytes) {
    std::lock_guard lock{mutex_};
    if (!connected_ || fail_next_write_) {
        fail_next_write_ = false;
        return false;
    }
    writes_.emplace_back(bytes.begin(), bytes.end());
    return true;
}

bool FakeBleTransport::connected() const noexcept {
    std::lock_guard lock{mutex_};
    return connected_;
}

std::size_t FakeBleTransport::dropped_notification_count() const noexcept {
    return 0;
}

void FakeBleTransport::emit(Channel channel, std::span<const std::uint8_t> bytes) {
    BytesReceived callback;
    {
        std::lock_guard lock{mutex_};
        if (!connected_) {
            return;
        }
        callback = bytes_received_;
    }
    if (callback) {
        callback(channel, bytes);
    }
}

void FakeBleTransport::simulate_remote_disconnect() {
    Disconnected callback;
    {
        std::lock_guard lock{mutex_};
        if (!connected_) {
            return;
        }
        connected_ = false;
        callback = disconnected_;
    }
    if (callback) {
        callback();
    }
}

void FakeBleTransport::fail_next_connect(bool enabled) noexcept {
    std::lock_guard lock{mutex_};
    fail_next_connect_ = enabled;
}

void FakeBleTransport::fail_next_write(bool enabled) noexcept {
    std::lock_guard lock{mutex_};
    fail_next_write_ = enabled;
}

std::vector<std::vector<std::uint8_t>> FakeBleTransport::writes() const {
    std::lock_guard lock{mutex_};
    return writes_;
}

std::string FakeBleTransport::selected_device_id() const {
    std::lock_guard lock{mutex_};
    return selected_device_id_;
}

} // namespace remotemic::ble
