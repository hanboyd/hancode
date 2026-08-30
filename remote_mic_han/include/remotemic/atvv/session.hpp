// Phase 3 / ADR-0013 §3.3: Session — transport-agnostic ATVV session
// state machine. Pure value-class over the existing Phase 2 decoders
// (Capabilities, parse_control_message, mic_open_command /
// mic_close_command, ImaDecoder, DcHighPassFilter, postprocess,
// FrameAccumulator).
//
// Stub state: ``handle_control`` returns a freshly-defaulted
// ``ControlEvent`` for every payload and ``handle_audio`` returns
// ``{}``. Step 2 wires the actual state transitions per the Phase 3
// state-machine contract.

#ifndef REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_SESSION_HPP
#define REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_SESSION_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <variant>
#include <vector>

#include "remotemic/atvv/capabilities.hpp"
#include "remotemic/atvv/control.hpp"

namespace remotemic::atvv {

// Same five event kinds as Python
// apps/windows/rc003/src/ovb_rc003/atvv_session.py:36-62 plus the
// ``UnknownControl`` carry for opcodes we do not handle.
struct CapsReceived {
    Capabilities capabilities;
};
struct MicButtonPressed {};
struct AudioStarted {
    std::optional<std::uint8_t> session_id;
};
struct AudioStopped {};
struct AudioSynced {};
struct UnknownControl {
    std::uint8_t opcode;
};

using ControlEvent = std::variant<
    CapsReceived,
    MicButtonPressed,
    AudioStarted,
    AudioStopped,
    AudioSynced,
    UnknownControl
>;

using ClockFn = std::function<std::chrono::milliseconds()>;

class Session {
public:
    explicit Session(double gain_db = 10.0);

    Session(double gain_db,
            std::chrono::milliseconds late_audio_guard,
            ClockFn clock);

    // ---- queries ------------------------------------------------------
    const Capabilities* capabilities() const noexcept;
    bool mic_open() const noexcept { return mic_open_; }

    // ---- event handlers ------------------------------------------------
    // Pure field/state handler. Returns the typed event; the caller
    // decides what to do with it (e.g. dispatch to the
    // VoiceController). Throws ``std::invalid_argument`` on a
    // zero-length payload (matches Python ``ATVVProtocolError``).
    ControlEvent handle_control(std::span<const std::uint8_t> payload);

    // Decode one audio notification. Returns ``{}`` while the mic is
    // closed OR while inside the late-audio guard window. The guard
    // is configurable at construction time; production default is
    // 2500 ms.
    std::vector<std::int16_t> handle_audio(
        std::span<const std::uint8_t> payload);

    // Host -> device encoders; carry the negotiated protocol version
    // and last-seen session id forward.
    std::vector<std::uint8_t> mic_open_command() const;
    std::vector<std::uint8_t> mic_close_command() const;

private:
    double gain_db_;
    std::chrono::milliseconds late_audio_guard_;
    ClockFn clock_;

    std::optional<Capabilities> caps_;
    std::uint16_t version_ = 0;
    std::uint16_t frame_size_ = 120;

    // Pending AUDIO_SYNC values, applied to the next frame boundary.
    std::optional<std::pair<std::int16_t, std::uint8_t>> pending_sync_;

    bool mic_open_ = false;
    std::optional<std::chrono::milliseconds> last_mic_off_at_;
    std::optional<std::uint8_t> last_session_id_;
};

}  // namespace remotemic::atvv

#endif  // REMOTEMIC_INCLUDE_REMOTEMIC_ATVV_SESSION_HPP
