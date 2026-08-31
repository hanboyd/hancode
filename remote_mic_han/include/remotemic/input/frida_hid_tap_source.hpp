#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <remotemic/input/i_input_source.hpp>

#ifdef _WIN32
// HANDLE on Win32 is defined as ``void*``; we use the same
// representation in the header so the file compiles without <windows.h>.
using WinHandle = void*;
#endif

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.6: FridaHidTapSource reads the upstream
// ``remote-bridge-hub`` Frida IPC socket and republishes the events
// as ``SourceKind::FridaHidTap`` InputEvents. The real implementation
// in step 2 sub-pass B opens a loopback TCP socket to
// ``127.0.0.1:REMOTE_MIC_RC003_HID_TAP_PORT`` (default 30684), spawns
// an IO thread that reads newline-delimited JSON messages of the
// shape ``{"kind":"gatt_read","raw":"<hex>"}``, decodes the 9-byte
// RC003 HID report to a HID usage ID, and pushes a corresponding
// ``InputEvent`` onto a lock-free SPSC ring buffer consumed by the
// socket reader's own thread.
//
// Back / volume+ / volume- keys reach Windows ONLY via this path on
// machines where elevated WUDFHost injection is blocked. G6 will
// verify whether the path actually delivers on real hardware.
class FridaHidTapSource final : public IInputSource {
public:
    FridaHidTapSource();
    ~FridaHidTapSource() override;

    FridaHidTapSource(const FridaHidTapSource&) = delete;
    FridaHidTapSource& operator=(const FridaHidTapSource&) = delete;

    void set_event_sink(SinkFn sink, void* user_data) noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t dropped_count() const noexcept override;
    std::uint64_t event_count() const noexcept override;

private:
    static constexpr std::size_t kQueueCapacity = 256;

    void IoThreadMain();
    void EnqueueEvent(InputEvent ev);

    SinkFn sink_{nullptr};
    void*  user_data_{nullptr};

    std::atomic<bool> started_{false};
    std::atomic<bool> stop_flag_{false};

    // Win32 socket handle. INVALID_SOCKET when not connected.
    // forward-declared as ``std::uintptr_t`` to avoid pulling
    // <winsock2.h> into the header.
    std::uintptr_t sock_{static_cast<std::uintptr_t>(-1)};
#ifdef _WIN32
    WinHandle thread_{nullptr};
#endif

    int port_{30684};

    std::atomic<std::uint64_t> event_count_{0};
    std::atomic<std::uint64_t> dropped_count_{0};

    alignas(64) std::atomic<std::size_t> write_idx_{0};
    alignas(64) std::atomic<std::size_t> read_idx_{0};
    static constexpr std::size_t mask_ = kQueueCapacity - 1;
    std::array<InputEvent, kQueueCapacity> ring_{};
};

} // namespace remotemic::input
