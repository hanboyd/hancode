// Phase 3 / ADR-0013 §3.3: Session — real implementation.
//
// Replaces the step-1 stub. Behavior matches
// apps/windows/rc003/src/ovb_rc003/atvv_session.py:149-249:
//   - handle_control dispatches on payload[0] and updates the
//     state machine (caps / mic_open / pending_sync / last_mic_off_at).
//   - handle_audio runs the late-audio guard, then the existing
//     Phase 2 PCM pipeline (FrameAccumulator -> ImaDecoder ->
//     DcHighPassFilter -> postprocess).
//   - mic_open_command / mic_close_command carry the negotiated
//     version + last-seen session_id forward, matching the Python
//     surface byte-for-byte.
//
// The ADPCM pipeline is owned by the Session and rebuilt in place
// when caps change the negotiated sample rate; reset on AUDIO_START.
// Pre-caps fallback uses the Python baseline's defaults
// (16000 Hz, 20 Hz cutoff).

#include "remotemic/atvv/session.hpp"

#include <stdexcept>
#include <utility>

#include "remotemic/adpcm/dc_highpass.hpp"
#include "remotemic/adpcm/frame_accumulator.hpp"
#include "remotemic/adpcm/ima_decoder.hpp"
#include "remotemic/adpcm/postprocess.hpp"
#include "remotemic/atvv/capabilities.hpp"
#include "remotemic/atvv/control.hpp"

namespace remotemic::atvv {

namespace {

std::chrono::milliseconds monotonic_clock_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

// Default sample rate / cutoff used when no CAPS has arrived yet.
// Matches the Python baseline's defaults
// (atvv_protocol.py:213 SUPPORTED_SAMPLE_RATE_HZ = 16000, cutoff 20).
constexpr double kDefaultSampleRate = 16000.0;
constexpr double kDefaultCutoffHz = 20.0;

}  // namespace

Session::Session(double gain_db)
    : gain_db_(gain_db),
      late_audio_guard_(std::chrono::milliseconds(2500)),
      clock_(&monotonic_clock_now),
      decoder_(std::make_unique<adpcm::ImaDecoder>()),
      dc_filter_(std::make_unique<adpcm::DcHighPassFilter>(
          kDefaultSampleRate, kDefaultCutoffHz)),
      accumulator_(std::make_unique<adpcm::FrameAccumulator>()) {}

Session::Session(double gain_db,
                 std::chrono::milliseconds late_audio_guard,
                 ClockFn clock)
    : gain_db_(gain_db),
      late_audio_guard_(late_audio_guard),
      clock_(std::move(clock)),
      decoder_(std::make_unique<adpcm::ImaDecoder>()),
      dc_filter_(std::make_unique<adpcm::DcHighPassFilter>(
          kDefaultSampleRate, kDefaultCutoffHz)),
      accumulator_(std::make_unique<adpcm::FrameAccumulator>()) {}

const Capabilities* Session::capabilities() const noexcept {
    return caps_ ? &*caps_ : nullptr;
}

ControlEvent Session::handle_control(
    std::span<const std::uint8_t> payload) {
    if (payload.empty()) {
        throw std::invalid_argument("empty control payload");
    }

    const auto opcode = payload[0];

    if (opcode == static_cast<std::uint8_t>(Opcode::Caps)) {
        auto parsed = parse(payload);
        if (!parsed) {
            return UnknownControl{opcode};
        }
        const bool sample_rate_changed =
            !caps_ || caps_->sample_rate != parsed->sample_rate;
        caps_ = *parsed;
        version_ = parsed->version;
        frame_size_ = parsed->frame_size;
        if (sample_rate_changed) {
            dc_filter_ = std::make_unique<adpcm::DcHighPassFilter>(
                parsed->sample_rate, kDefaultCutoffHz);
        }
        return CapsReceived{*parsed};
    }

    if (opcode == static_cast<std::uint8_t>(Opcode::MicButton)) {
        return MicButtonPressed{};
    }

    if (opcode == static_cast<std::uint8_t>(Opcode::AudioStart)) {
        std::optional<std::uint8_t> session_id;
        if (payload.size() >= 4) {
            session_id = payload[3];
        }
        last_session_id_ = session_id;
        mic_open_ = true;
        last_mic_off_at_.reset();
        pending_sync_.reset();
        decoder_->reset(0, 0);
        dc_filter_->reset();
        accumulator_->reset();
        return AudioStarted{session_id};
    }

    if (opcode == static_cast<std::uint8_t>(Opcode::AudioStop)) {
        mic_open_ = false;
        last_mic_off_at_ = clock_();
        accumulator_->reset();
        return AudioStopped{};
    }

    if (opcode == static_cast<std::uint8_t>(Opcode::AudioSync)) {
        if (payload.size() >= 7) {
            const auto predictor = static_cast<std::int16_t>(
                (static_cast<std::uint16_t>(payload[4]) << 8) |
                static_cast<std::uint16_t>(payload[5]));
            const auto step_index = payload[6];
            pending_sync_ = std::make_pair(predictor, step_index);
            return AudioSynced{};
        }
        return UnknownControl{opcode};
    }

    return UnknownControl{opcode};
}

std::vector<std::int16_t> Session::handle_audio(
    std::span<const std::uint8_t> payload) {
    if (!mic_open_) {
        if (last_mic_off_at_.has_value() &&
            (clock_() - *last_mic_off_at_) < late_audio_guard_) {
            return {};
        }
    }

    const auto frames = accumulator_->append(payload, frame_size_);
    std::vector<std::int16_t> samples;
    for (const auto& frame : frames) {
        if (pending_sync_.has_value()) {
            decoder_->reset(pending_sync_->first, pending_sync_->second);
            dc_filter_->reset();
            pending_sync_.reset();
        }
        const auto decoded = decoder_->decode(frame);
        const auto centered = dc_filter_->process(decoded);
        const auto post = adpcm::postprocess(centered, gain_db_);
        samples.insert(samples.end(), post.begin(), post.end());
    }
    return samples;
}

std::vector<std::uint8_t> Session::mic_open_command() const {
    return ::remotemic::atvv::mic_open_command(version_);
}

std::vector<std::uint8_t> Session::mic_close_command() const {
    return ::remotemic::atvv::mic_close_command(
        version_, last_session_id_.value_or(0));
}

}  // namespace remotemic::atvv