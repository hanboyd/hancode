#pragma once

#include <remotemic/input/input_event.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.2: input source interface. The implementation
// owns its own thread (Raw Input message loop, Frida socket reader, or
// LL hook dispatcher). Callbacks are invoked on that thread; they must
// return within 5 us and MUST NOT call back into the source.
//
// Single-owner rule (plan §3 rule 5): at most one IInputSource is in
// ``started == true`` state at any moment. Phase 7 Application
// coordinator enforces this; the interface itself does not.
class IInputSource {
public:
    virtual ~IInputSource() = default;

    using SinkFn = void (*)(InputEvent event, void* user_data);

    virtual void set_event_sink(SinkFn sink, void* user_data) noexcept = 0;
    virtual bool start() noexcept = 0;
    virtual void stop() noexcept = 0;

    // Diagnostics. Implementation-defined counts.
    virtual std::uint64_t dropped_count() const noexcept = 0;
    virtual std::uint64_t event_count() const noexcept = 0;
};

} // namespace remotemic::input