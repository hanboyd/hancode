#pragma once

#include <cstdint>

#include <remotemic/input/i_input_source.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.6: FridaHidTapSource reads the upstream
// ``remote-bridge-hub`` Frida IPC socket and republishes the events
// as ``SourceKind::FridaHidTap`` InputEvents. The real implementation
// in step 2 will open the socket on its own IO thread + drain into a
// lock-free SPSC queue. Step 1 stub returns false from start.
//
// Back / volume+ / volume- keys reach Windows ONLY via this path on
// machines where elevated WUDFHost injection is blocked. G6 will
// verify whether the path actually delivers on real hardware.
class FridaHidTapSource final : public IInputSource {
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