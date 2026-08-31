#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <remotemic/input/i_input_source.hpp>

#ifdef _WIN32
// Forward-declare Win32 types so the header stays platform-independent.
struct HWND__;
using HWND = HWND__*;
// HANDLE on Win32 is defined as ``void*``; we use the same
// representation in the header so the file compiles without <windows.h>.
using WinHandle = void*;
#endif

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.7 RawInputSource: Windows Raw Input adapter for
// RC003 ordinary buttons.
//
// The real implementation in step 2 sub-pass B:
// 1. Registers RIDEV_INPUTSINK for HID usage page 0x01 (Generic Desktop)
//    and 0x07 (Keyboard).
// 2. Creates a hidden message-only HWND and pumps WM_INPUT messages.
// 3. For each WM_INPUT, decodes RIM_TYPEKEYBOARD / RIM_TYPEHID, filters
//    by the device path matching the RC003 VID/PID (0x2717/0x32B8),
//    translates to ``InputEvent`` via the ``KEYBOARD_VK_TO_BUTTON``
//    table from ``raw_input_windows.py:81-95``, and pushes onto a
//    lock-free SPSC ring buffer consumed by the message-pump thread.
class RawInputSource final : public IInputSource {
public:
    RawInputSource();
    ~RawInputSource() override;

    RawInputSource(const RawInputSource&) = delete;
    RawInputSource& operator=(const RawInputSource&) = delete;

    void set_event_sink(SinkFn sink, void* user_data) noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t dropped_count() const noexcept override;
    std::uint64_t event_count() const noexcept override;

private:
    static constexpr std::size_t kQueueCapacity = 256;

    void PumpThreadMain();
    void EnqueueEvent(InputEvent ev);

    SinkFn sink_{nullptr};
    void*  user_data_{nullptr};

    std::atomic<bool> started_{false};
    std::atomic<bool> stop_flag_{false};

#ifdef _WIN32
    HWND       hwnd_{nullptr};
    WinHandle  thread_{nullptr};
    unsigned long thread_id_{0};  // DWORD on Win32
#endif

    std::atomic<std::uint64_t> event_count_{0};
    std::atomic<std::uint64_t> dropped_count_{0};

    alignas(64) std::atomic<std::size_t> write_idx_{0};
    alignas(64) std::atomic<std::size_t> read_idx_{0};
    static constexpr std::size_t mask_ = kQueueCapacity - 1;
    std::array<InputEvent, kQueueCapacity> ring_{};
};

} // namespace remotemic::input
