// Phase 5 / ADR-0015 step 1 stub: FridaHidTapSource returns false from
// start() so red-state tests can fail without opening a Frida IPC
// socket. Step 2 replaces with the real socket reader.

#include <remotemic/input/frida_hid_tap_source.hpp>

namespace remotemic::input {

void FridaHidTapSource::set_event_sink(SinkFn sink, void* user_data) noexcept {
    sink_ = sink;
    user_data_ = user_data;
}

bool FridaHidTapSource::start() noexcept {
    started_ = false;
    return false;
}

void FridaHidTapSource::stop() noexcept {
    started_ = false;
}

std::uint64_t FridaHidTapSource::dropped_count() const noexcept {
    return dropped_count_;
}

std::uint64_t FridaHidTapSource::event_count() const noexcept {
    return event_count_;
}

} // namespace remotemic::input