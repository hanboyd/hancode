// Phase 5 / ADR-0015 step 1 stub: RawInputSource returns false from
// start() so red-state tests can fail without touching Win32 APIs.
// Step 2 replaces with the real RegisterRawInputDevices adapter.

#include <remotemic/input/raw_input_source.hpp>

namespace remotemic::input {

void RawInputSource::set_event_sink(SinkFn sink, void* user_data) noexcept {
    sink_ = sink;
    user_data_ = user_data;
}

bool RawInputSource::start() noexcept {
    started_ = false;
    return false;
}

void RawInputSource::stop() noexcept {
    started_ = false;
}

std::uint64_t RawInputSource::dropped_count() const noexcept {
    return dropped_count_;
}

std::uint64_t RawInputSource::event_count() const noexcept {
    return event_count_;
}

} // namespace remotemic::input