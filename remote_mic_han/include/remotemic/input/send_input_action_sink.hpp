#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include <remotemic/input/i_host_action_sink.hpp>

#ifdef _WIN32
// HANDLE on Win32 is defined as ``void*``; we use the same
// representation in the header so the file compiles without <windows.h>.
using WinHandle = void*;
#endif

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.7: SendInputActionSink owns the Win32
// ``user32.SendInput`` handle + a worker thread that coalesces
// submitted key events into batched ``SendInput`` syscalls.
//
// ``submit_key`` pushes onto a bounded key queue (capacity 256,
// drop-oldest on overflow) and notifies the worker. The worker
// drains the queue and calls ``SendInput`` once per batch.
//
// ``submit_system_action`` short-circuits the queue and dispatches
// Win32 system commands directly on the caller thread:
//   - volume / mute: ``SendMessage(HWND_BROADCAST, WM_APPCOMMAND, ...)``
//   - show desktop:  ``keybd_event(VK_LWIN, VK_D)``
//   - escape:        ``keybd_event(VK_ESCAPE, ...)``
//
// Extended keys (arrows, rctrl, ralt, rwin) carry ``KEYEVENTF_EXTENDEDKEY``;
// modifier keys use the physical scan-code path (per
// ``win32_input.py:_PHYSICAL_SCAN_CODES``) so the left/right identity
// is preserved across host applications.
class SendInputActionSink final : public IHostActionSink {
public:
    SendInputActionSink();
    explicit SendInputActionSink(std::vector<std::uint16_t> physicalize_vk_codes);
    ~SendInputActionSink() override;

    SendInputActionSink(const SendInputActionSink&) = delete;
    SendInputActionSink& operator=(const SendInputActionSink&) = delete;

    bool submit_key(std::uint16_t vk_code, bool key_down,
                    std::chrono::milliseconds deadline) noexcept override;
    bool submit_system_action(SystemAction action) noexcept override;
    void cancel_pending() noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t submit_error_count() const noexcept override;
    std::uint64_t submitted_count() const noexcept override;

private:
    static constexpr std::size_t kQueueCapacity = 256;

    void WorkerThreadMain();
    void ClearQueueLocked();

    std::atomic<bool> started_{false};
    std::atomic<bool> stop_flag_{false};

#ifdef _WIN32
    WinHandle thread_{nullptr};
    unsigned long thread_id_{0};  // DWORD on Win32
#endif

    std::mutex queue_mu_;
    std::condition_variable queue_cv_;
    std::vector<std::pair<std::uint16_t, bool>> key_queue_;
    // Only these bridge-owned voice-chord VKs receive the private marker
    // consumed by LegacyKeySuppressor. Ordinary mapped keys remain ordinary
    // SendInput events even though they share this sink.
    std::vector<std::uint16_t> physicalize_vk_codes_;

    std::atomic<std::uint64_t> submitted_count_{0};
    std::atomic<std::uint64_t> submit_error_count_{0};
};

} // namespace remotemic::input
