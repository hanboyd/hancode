#include "remotemic/ble/fake_ble_transport.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    using remotemic::IBleTransport;
    remotemic::ble::FakeBleTransport transport;
    std::vector<std::uint8_t> received;
    auto channel = IBleTransport::Channel::control;
    bool disconnected = false;

    transport.set_bytes_received([&](auto value, auto bytes) {
        channel = value;
        received.assign(bytes.begin(), bytes.end());
    });
    transport.set_disconnected([&] { disconnected = true; });

    require(!transport.connect(""), "empty device id must fail closed");
    require(transport.connect("opaque-test-id"), "connect should succeed");
    require(transport.connected(), "transport should report connected");

    const std::array<std::uint8_t, 3> packet{1, 2, 3};
    transport.emit(IBleTransport::Channel::audio, packet);
    require(channel == IBleTransport::Channel::audio, "channel must round-trip");
    require(received == std::vector<std::uint8_t>({1, 2, 3}), "payload must round-trip");
    require(transport.write(packet), "connected write should succeed");
    require(transport.writes().size() == 1, "write must be recorded once");

    transport.fail_next_write();
    require(!transport.write(packet), "injected write failure should fail");
    require(transport.write(packet), "write failure must be one-shot");

    transport.simulate_remote_disconnect();
    require(disconnected, "disconnect callback must fire");
    require(!transport.connected(), "remote disconnect must clear state");
    require(!transport.write(packet), "write after disconnect must fail");

    transport.disconnect();
    transport.disconnect();
    require(transport.dropped_notification_count() == 0, "fake never drops notifications");
    std::cout << "PASS: FakeBleTransport lifecycle and callbacks\n";
    return 0;
}
