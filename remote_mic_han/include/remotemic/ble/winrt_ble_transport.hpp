#pragma once

#include <memory>

#include "remotemic/interfaces/ble_transport.hpp"

namespace remotemic::ble {

class WinRTBleTransport final : public IBleTransport {
public:
    WinRTBleTransport();
    ~WinRTBleTransport() override;

    WinRTBleTransport(const WinRTBleTransport&) = delete;
    WinRTBleTransport& operator=(const WinRTBleTransport&) = delete;

    void set_bytes_received(BytesReceived callback) override;
    void set_disconnected(Disconnected callback) override;
    bool connect(std::string_view device_id) override;
    void disconnect() noexcept override;
    bool write(std::span<const std::uint8_t> bytes) override;
    [[nodiscard]] bool connected() const noexcept override;
    [[nodiscard]] std::size_t dropped_notification_count() const noexcept override;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace remotemic::ble
