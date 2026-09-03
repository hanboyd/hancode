#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "remotemic/interfaces/ble_transport.hpp"

namespace remotemic::ble {

class FakeBleTransport final : public IBleTransport {
public:
    void set_bytes_received(BytesReceived callback) override;
    void set_disconnected(Disconnected callback) override;
    bool connect(std::string_view device_id) override;
    void disconnect() noexcept override;
    bool write(std::span<const std::uint8_t> bytes) override;
    [[nodiscard]] bool connected() const noexcept override;
    [[nodiscard]] std::size_t dropped_notification_count() const noexcept override;

    void emit(Channel channel, std::span<const std::uint8_t> bytes);
    void simulate_remote_disconnect();
    void fail_next_connect(bool enabled = true) noexcept;
    void fail_next_write(bool enabled = true) noexcept;

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> writes() const;
    [[nodiscard]] std::string selected_device_id() const;

private:
    mutable std::mutex mutex_;
    BytesReceived bytes_received_;
    Disconnected disconnected_;
    std::vector<std::vector<std::uint8_t>> writes_;
    std::string selected_device_id_;
    bool connected_{false};
    bool fail_next_connect_{false};
    bool fail_next_write_{false};
};

} // namespace remotemic::ble
