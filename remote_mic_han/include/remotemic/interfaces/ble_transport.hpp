#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace remotemic {

class IBleTransport {
public:
    enum class Channel : std::uint8_t {
        audio,
        control,
    };

    using BytesReceived =
        std::function<void(Channel, std::span<const std::uint8_t>)>;
    using Disconnected = std::function<void()>;

    virtual ~IBleTransport() = default;
    virtual void set_bytes_received(BytesReceived callback) = 0;
    virtual void set_disconnected(Disconnected callback) = 0;
    virtual bool connect(std::string_view device_id) = 0;
    virtual void disconnect() noexcept = 0;
    virtual bool write(std::span<const std::uint8_t> bytes) = 0;
    [[nodiscard]] virtual bool connected() const noexcept = 0;
    [[nodiscard]] virtual std::size_t dropped_notification_count() const noexcept = 0;
};

} // namespace remotemic
