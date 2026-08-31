#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include <remotemic/input/i_input_source.hpp>
#include <remotemic/input/input_event.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §8: cross-OS test double for IInputSource. Holds a
// recorded snapshot of all events delivered to the sink. NOT Windows-
// only; lives for parity tests and CI on Linux/macOS.
class FakeInputSource final : public IInputSource {
public:
    void set_event_sink(SinkFn sink, void* user_data) noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t dropped_count() const noexcept override;
    std::uint64_t event_count() const noexcept override;

    // Test-only helpers. Producers (tests) inject events through these.
    // They invoke the registered sink under the same threading rules as
    // a real source (single producer at a time).
    void inject_event_for_test(InputEvent event);
    void set_dropped_count_for_test(std::uint64_t dropped) noexcept;

    // Snapshot under mutex; safe to call from any thread.
    std::vector<InputEvent> recorded_events() const;

private:
    SinkFn sink_{nullptr};
    void*  user_data_{nullptr};
    std::uint64_t event_count_{0};
    std::uint64_t dropped_count_{0};
    mutable std::mutex mu_;
    std::deque<InputEvent> recorded_;
};

} // namespace remotemic::input