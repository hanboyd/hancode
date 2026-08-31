#pragma once

#include <cstdint>

#include <remotemic/input/i_input_source.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.7 RawInputSource: Windows Raw Input adapter for
// RC003 ordinary buttons. The real implementation in step 2 will
// register ``RIDI_DEVICENAME`` matches against the RC003 VID/PID via
// ``hid_identity`` and dispatch ``RIM_TYPEKEYBOARD`` / ``RIM_TYPEHID``
// events. Step 1 stub returns false from start.
class RawInputSource final : public IInputSource {
public:
    void set_event_sink(SinkFn sink, void* user_data) noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t dropped_count() const noexcept override;
    std::uint64_t event_count() const noexcept override;

private:
    SinkFn sink_{nullptr};
    void*  user_data_{nullptr};
    bool   started_{false};
    std::uint64_t event_count_{0};
    std::uint64_t dropped_count_{0};
};

} // namespace remotemic::input