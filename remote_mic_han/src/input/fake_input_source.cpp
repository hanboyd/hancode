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
    // Drop the registered sink so a subsequent inject_event_for_test
    // does not invoke a callback on a "stopped" source. Mirrors the
    // real Win32 adapters' stop() semantics: after stop() no further
    // callbacks fire.
    std::lock_guard<std::mutex> lock(mu_);
    sink_ = nullptr;
    user_data_ = nullptr;
}

std::uint64_t FakeInputSource::dropped_count() const noexcept {
    std::lock_guard<std::mutex> lock(mu_);
    return dropped_count_;
}

std::uint64_t FakeInputSource::event_count() const noexcept {
    std::lock_guard<std::mutex> lock(mu_);
    return event_count_;
}

void FakeInputSource::inject_event_for_test(InputEvent event) {
    // Snapshot the sink under the lock and invoke OUTSIDE the lock so a
    // misbehaving sink cannot deadlock the recording double. The
    // interface contract is that the sink returns promptly and never
    // calls back into the source.
    SinkFn sink_copy = nullptr;
    void* user_data_copy = nullptr;
    {
        std::lock_guard<std::mutex> lock(mu_);
        recorded_.push_back(event);
        ++event_count_;
        sink_copy = sink_;
        user_data_copy = user_data_;
    }
    if (sink_copy != nullptr) {
        sink_copy(event, user_data_copy);
    }
}

void FakeInputSource::set_dropped_count_for_test(std::uint64_t dropped) noexcept {
    std::lock_guard<std::mutex> lock(mu_);
    dropped_count_ = dropped;
}

std::vector<InputEvent> FakeInputSource::recorded_events() const {
    std::lock_guard<std::mutex> lock(mu_);
    return std::vector<InputEvent>(recorded_.begin(), recorded_.end());
}

} // namespace remotemic::input