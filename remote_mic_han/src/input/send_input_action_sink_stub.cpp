// Phase 5 / ADR-0015 step 1 stub: SendInputActionSink rejects every
// submit_* call so red-state tests can fail without touching Win32
// APIs. Step 2 replaces with the real user32.SendInput adapter.

#include <remotemic/input/send_input_action_sink.hpp>

namespace remotemic::input {

bool SendInputActionSink::submit_key(std::uint16_t /*vk_code*/, bool /*key_down*/,
                                     std::chrono::milliseconds /*deadline*/) noexcept {
    ++submit_error_count_;
    return false;
}

bool SendInputActionSink::submit_system_action(SystemAction /*action*/) noexcept {
    ++submit_error_count_;
    return false;
}

void SendInputActionSink::cancel_pending() noexcept {}

bool SendInputActionSink::start() noexcept {
    started_ = false;
    return false;
}

void SendInputActionSink::stop() noexcept {
    started_ = false;
}

std::uint64_t SendInputActionSink::submit_error_count() const noexcept {
    return submit_error_count_;
}
std::uint64_t SendInputActionSink::submitted_count() const noexcept {
    return submitted_count_;
}

} // namespace remotemic::input