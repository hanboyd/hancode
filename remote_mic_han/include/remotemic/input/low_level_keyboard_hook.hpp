#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <remotemic/input/i_input_source.hpp>

#ifdef _WIN32
// Forward-declare Win32 types so the header stays platform-independent.
// The implementation file (low_level_keyboard_hook.cpp) pulls in
// <windows.h>; the test build on non-Windows hosts compiles without it.
// Types used in the class interface (HookProc / WndProcThunk signatures)
// must be forward-declared here so the compiler can parse the declarations.
using LRESULT  = long long;
using WPARAM   = unsigned long long;
using LPARAM   = long long;
using UINT     = unsigned int;
#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

struct HWND__;
struct HHOOK__;
struct tagMSG;
using HWND  = HWND__*;
using HHOOK = HHOOK__*;
using MSG   = tagMSG;
// HANDLE on Win32 is defined as ``void*``; we use the same
// representation in the header so the file compiles without <windows.h>.
using WinHandle = void*;
#endif  // _WIN32

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.4: Windows-only low-level keyboard hook.
//
// The implementation in step 2 sub-pass B installs ``WH_KEYBOARD_LL``,
// maintains an atomic suppression table keyed by VK, and pushes events
// through a lock-free SPSC queue of capacity 256 (drop-oldest overflow).
// The hook callback runs on Windows' own message-pump thread and is
// bound to a 5 us latency budget per ADR-0015 §5 (slow callbacks are
// counted in ``slow_callback_count_`` but never block).
class LowLevelKeyboardHook final : public IInputSource {
public:
    LowLevelKeyboardHook();
    ~LowLevelKeyboardHook() override;

    LowLevelKeyboardHook(const LowLevelKeyboardHook&) = delete;
    LowLevelKeyboardHook& operator=(const LowLevelKeyboardHook&) = delete;

    void set_event_sink(SinkFn sink, void* user_data) noexcept override;
    bool start() noexcept override;
    void stop() noexcept override;

    std::uint64_t dropped_count() const noexcept override;
    std::uint64_t event_count() const noexcept override;

    // Diagnostic — the real hook records any callback that exceeds the
    // 5 us latency budget. Step 1 stub returns 0.
    std::uint64_t slow_callback_count() const noexcept;

private:
    static constexpr std::size_t kQueueCapacity = 256;

#ifdef _WIN32
    // The hook callback is declared static so the Win32 ``SetWindowsHookEx``
    // signature is satisfied; the thread-local ``current_instance_``
    // indirection lets us route the event back to the right object.
    static LRESULT CALLBACK HookProc(int n_code, WPARAM w_param, LPARAM l_param);
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg,
                                         WPARAM w_param, LPARAM l_param);

    // Hook callback context — registered with SetWindowsHookEx and
    // resolved back to the owning object via current_instance_.
    LRESULT DispatchHook(int n_code, WPARAM w_param, LPARAM l_param);
#endif

    // Drain the SPSC queue on the message-pump thread. The hook callback
    // NEVER invokes the registered sink directly (per ADR-0015 §3.4
    // non-blocking rule); the message thread does.
    void PumpThreadMain();
    void EnqueueFromHook(InputEvent ev);

    SinkFn sink_{nullptr};
    void*  user_data_{nullptr};

    // Cross-thread state set by start()/stop() and read by PumpThreadMain.
    std::atomic<bool> started_{false};
    std::atomic<bool> stop_flag_{false};

#ifdef _WIN32
    HWND       hwnd_{nullptr};
    HHOOK      hook_{nullptr};
    WinHandle  thread_{nullptr};
    unsigned long thread_id_{0};  // DWORD on Win32
#endif

    // Counters — atomic so the hook callback path can update them without
    // a lock.
    std::atomic<std::uint64_t> event_count_{0};
    std::atomic<std::uint64_t> dropped_count_{0};
    std::atomic<std::uint64_t> slow_callback_count_{0};

    // Lock-free SPSC ring buffer. Producer = hook callback on the OS
    // dispatch thread; consumer = PumpThreadMain on the message-pump
    // thread. ``mask_ = kQueueCapacity - 1`` (power-of-two capacity).
    alignas(64) std::atomic<std::size_t> write_idx_{0};
    alignas(64) std::atomic<std::size_t> read_idx_{0};
    static constexpr std::size_t mask_ = kQueueCapacity - 1;
    std::array<InputEvent, kQueueCapacity> ring_{};
};

} // namespace remotemic::input
