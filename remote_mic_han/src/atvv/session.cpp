// Phase 3 / ADR-0013 §3.3: Session stub.
//
// Step 1: ``handle_control`` returns a freshly-defaulted
// ``ControlEvent`` for every payload and ``handle_audio`` returns
// ``{}``. Step 2 wires the real state transitions and PCM pipeline.

#include "remotemic/atvv/session.hpp"

#include <stdexcept>
#include <utility>

namespace remotemic::atvv {

namespace {

std::chrono::milliseconds monotonic_clock_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

}  // namespace

Session::Session(double gain_db)
    : gain_db_(gain_db),
      late_audio_guard_(std::chrono::milliseconds(2500)),
      clock_(&monotonic_clock_now) {}

Session::Session(double gain_db,
                 std::chrono::milliseconds late_audio_guard,
                 ClockFn clock)
    : gain_db_(gain_db),
      late_audio_guard_(late_audio_guard),
      clock_(std::move(clock)) {}

const Capabilities* Session::capabilities() const noexcept {
    return caps_ ? &*caps_ : nullptr;
}

ControlEvent Session::handle_control(
    std::span<const std::uint8_t> payload) {
    if (payload.empty()) {
        throw std::invalid_argument("empty control payload");
    }
    // STUB: any non-empty payload returns UnknownControl. Step 2
    // dispatches on payload[0] and updates the state machine.
    return UnknownControl{payload[0]};
}

std::vector<std::int16_t> Session::handle_audio(
    std::span<const std::uint8_t> /*payload*/) {
    return {};
}

std::vector<std::uint8_t> Session::mic_open_command() const {
    return {};
}

std::vector<std::uint8_t> Session::mic_close_command() const {
    return {};
}

}  // namespace remotemic::atvv
