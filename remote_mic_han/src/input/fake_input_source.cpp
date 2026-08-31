#include <remotemic/input/fake_input_source.hpp>

namespace remotemic::input {

void FakeInputSource::set_event_sink(SinkFn sink, void* user_data) noexcept {
    sink_ = sink;
    user_data_ = user_data;
}

bool FakeInputSource::start() noexcept {
    return true;  // recording double always starts successfully
}

void FakeInputSource::stop() noexcept {
    // no-op
}

std::uint64_t FakeInputSource::dropped_count() const noexcept {
    return dropped_count_;
}

std::uint64_t FakeInputSource::event_count() const noexcept {
    return event_count_;
}

void FakeInputSource::inject_event_for_test(InputEvent event) {
    std::lock_guard<std::mutex> lock(mu_);
    recorded_.push_back(event);
    ++event_count_;
}

void FakeInputSource::set_dropped_count_for_test(std::uint64_t dropped) noexcept {
    dropped_count_ = dropped;
}

std::vector<InputEvent> FakeInputSource::recorded_events() const {
    std::lock_guard<std::mutex> lock(mu_);
    return std::vector<InputEvent>(recorded_.begin(), recorded_.end());
}

} // namespace remotemic::input