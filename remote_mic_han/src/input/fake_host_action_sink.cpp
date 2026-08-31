#include <remotemic/input/fake_host_action_sink.hpp>

namespace remotemic::input {

bool FakeHostActionSink::submit_key(std::uint16_t vk_code, bool key_down,
                                    std::chrono::milliseconds /*deadline*/) noexcept {
    if (submit_fails_) {
        ++submit_error_count_;
        return false;
    }
    std::lock_guard<std::mutex> lock(mu_);
    keys_.push_back({vk_code, key_down});
    ++submitted_count_;
    return true;
}

bool FakeHostActionSink::submit_system_action(SystemAction action) noexcept {
    if (submit_fails_) {
        ++submit_error_count_;
        return false;
    }
    std::lock_guard<std::mutex> lock(mu_);
    sys_.push_back(action);
    ++submitted_count_;
    return true;
}

void FakeHostActionSink::cancel_pending() noexcept {
    std::lock_guard<std::mutex> lock(mu_);
    keys_.clear();
    sys_.clear();
}

bool FakeHostActionSink::start() noexcept { return true; }
void FakeHostActionSink::stop() noexcept {}

std::uint64_t FakeHostActionSink::submit_error_count() const noexcept {
    return submit_error_count_;
}
std::uint64_t FakeHostActionSink::submitted_count() const noexcept {
    return submitted_count_;
}

std::vector<FakeHostActionSink::KeyEntry>
FakeHostActionSink::recorded_keys() const {
    std::lock_guard<std::mutex> lock(mu_);
    return std::vector<KeyEntry>(keys_.begin(), keys_.end());
}

std::vector<FakeHostActionSink::SysEntry>
FakeHostActionSink::recorded_system_actions() const {
    std::lock_guard<std::mutex> lock(mu_);
    return std::vector<SysEntry>(sys_.begin(), sys_.end());
}

std::size_t FakeHostActionSink::pending_count() const {
    std::lock_guard<std::mutex> lock(mu_);
    return keys_.size() + sys_.size();
}

void FakeHostActionSink::set_submit_fails_for_test(bool fails) noexcept {
    submit_fails_ = fails;
}

} // namespace remotemic::input