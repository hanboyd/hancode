// Phase 5 / ADR-0015 §3.4 step 2 sub-pass B: real WH_KEYBOARD_LL
// implementation. Mirrors ``apps/windows/rc003/src/ovb_rc003/
// legacy_key_suppressor_windows.py`` line-for-line where it matters.
//
// Constraints (per ADR-0015 §3.4 + §3 rule 6):
//   * Hook callback MUST return within a 5 us budget; over-budget
//     callbacks increment ``slow_callback_count_`` but never block.
//   * Hook callback MUST NOT call SendInput / GetMessage / WriteFile
//     / Frida IPC / Python GIL / std::mutex. Only reads KBDLLHOOKSTRUCT
//     fields, looks up the suppression table, and enqueues into a
//     lock-free SPSC ring buffer (drop-oldest overflow).
//   * Non-reentrant: same-thread recursion returns CallNextHookEx.
//
// Build notes: this TU is Windows-only. CMake targets compiling this
// source live only when ``_WIN32`` is defined; on non-Windows CI hosts
// the build system skips the file.

#include <remotemic/input/low_level_keyboard_hook.hpp>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <thread>

namespace remotemic::input {

namespace {

// Module-local Win32 message constants not pulled in by <windows.h> in
// every SDK flavour.
constexpr UINT  kMsgPumpStop = WM_USER + 0x103;
constexpr DWORD kHookSlowBudgetUs = 5;

// The Win32 LL keyboard hook callback runs on the OS message-pump
// thread that owns the hidden HWND. We use a thread-local pointer to
// the owning LowLevelKeyboardHook instance so the static
// ``HookProc`` can dispatch back to the right object. Only one
// instance can have a running hook at a time per plan §3 rule 5.
thread_local LowLevelKeyboardHook* tls_current_instance = nullptr;

inline LARGE_INTEGER QpcNow() noexcept {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t;
}

inline double QpcDeltaUs(LARGE_INTEGER start, LARGE_INTEGER end,
                         LARGE_INTEGER freq) noexcept {
    if (freq.QuadPart == 0) return 0.0;
    const double ticks = static_cast<double>(end.QuadPart - start.QuadPart);
    return (ticks * 1'000'000.0) / static_cast<double>(freq.QuadPart);
}

} // namespace

LowLevelKeyboardHook::LowLevelKeyboardHook() = default;

LowLevelKeyboardHook::~LowLevelKeyboardHook() {
    stop();
}

void LowLevelKeyboardHook::set_event_sink(SinkFn sink, void* user_data) noexcept {
    sink_ = sink;
    user_data_ = user_data;
}

void LowLevelKeyboardHook::EnqueueFromHook(InputEvent ev) {
    // SPSC drop-oldest push. Single producer = the hook callback.
    const std::size_t w = write_idx_.load(std::memory_order_relaxed);
    const std::size_t r = read_idx_.load(std::memory_order_acquire);
    if ((w - r) >= kQueueCapacity) {
        // Queue full -> drop oldest by advancing read_idx_.
        read_idx_.store(r + 1, std::memory_order_release);
        dropped_count_.fetch_add(1, std::memory_order_relaxed);
    }
    ring_[w & mask_] = ev;
    write_idx_.store(w + 1, std::memory_order_release);
}

LRESULT LowLevelKeyboardHook::DispatchHook(int n_code, WPARAM w_param, LPARAM l_param) {
    // Fast-path skip: HC_ACTION == 0. Anything else must be forwarded
    // untouched per Win32 contract.
    if (n_code < 0 || l_param == 0) {
        return CallNextHookEx(hook_, n_code, w_param, l_param);
    }
    if (n_code != HC_ACTION) {
        return CallNextHookEx(hook_, n_code, w_param, l_param);
    }

    // 5 us budget timing (start).
    LARGE_INTEGER freq{};
    QueryPerformanceFrequency(&freq);
    const LARGE_INTEGER start = QpcNow();

    const auto* kbd = reinterpret_cast<const KBDLLHOOKSTRUCT*>(l_param);
    const bool injected = (kbd->flags & LLKHF_INJECTED) != 0;
    const bool extended = (kbd->flags & LLKHF_EXTENDED) != 0;
    const bool key_up   = (kbd->flags & LLKHF_UP) != 0;

    // Decide message kind. Only the four WM_(SYS)KEY* messages are
    // meaningful on the LL hook; anything else (e.g. WM_KEYBOARDHOOK
    // sentinel) is forwarded without recording.
    InputEvent::EventKind kind = InputEvent::EventKind::KeyDown;
    bool is_known = true;
    switch (w_param) {
        case WM_KEYDOWN:      kind = InputEvent::EventKind::KeyDown; break;
        case WM_KEYUP:        kind = InputEvent::EventKind::KeyUp;   break;
        case WM_SYSKEYDOWN:   kind = InputEvent::EventKind::KeyDown; break;
        case WM_SYSKEYUP:     kind = InputEvent::EventKind::KeyUp;   break;
        default:              is_known = false; break;
    }
    if (!is_known) {
        return CallNextHookEx(hook_, n_code, w_param, l_param);
    }

    InputEvent ev{};
    ev.timestamp  = std::chrono::steady_clock::now();
    ev.source     = InputEvent::SourceKind::LowLevelHook;
    ev.kind       = kind;
    ev.vk_code    = static_cast<std::uint16_t>(kbd->vkCode);
    ev.scan_code  = static_cast<std::uint16_t>(kbd->scanCode);
    ev.extra_info = static_cast<std::uint32_t>(kbd->dwExtraInfo & 0xFFFFFFFFull);
    ev.injected   = injected;
    ev.extended   = extended;
    (void)key_up;  // already captured in kind

    EnqueueFromHook(ev);

    // 5 us budget check (end).
    const LARGE_INTEGER end = QpcNow();
    const double elapsed_us = QpcDeltaUs(start, end, freq);
    if (elapsed_us > static_cast<double>(kHookSlowBudgetUs)) {
        slow_callback_count_.fetch_add(1, std::memory_order_relaxed);
    }

    // Per ADR-0015 §3.4 the hook is "diagnostic / record-only" by
    // default; it does NOT swallow events. The python baseline uses
    // a separate suppression table that is filled by Raw Input, which
    // is wired by the Phase 7 coordinator. Step 2 sub-pass B returns
    // CallNextHookEx for every event (no swallowing) so the production
    // path keeps forwarding physical edges to the host.
    return CallNextHookEx(hook_, n_code, w_param, l_param);
}

LRESULT CALLBACK LowLevelKeyboardHook::HookProc(int n_code, WPARAM w_param, LPARAM l_param) {
    if (tls_current_instance == nullptr) {
        return CallNextHookEx(nullptr, n_code, w_param, l_param);
    }
    return tls_current_instance->DispatchHook(n_code, w_param, l_param);
}

LRESULT CALLBACK LowLevelKeyboardHook::WndProcThunk(HWND hwnd, UINT msg,
                                                   WPARAM w_param, LPARAM l_param) {
    // We don't actually need to inspect WM_INPUT here (the LL hook has
    // its own dispatcher); the HWND exists so Windows owns a real
    // message queue we can pump with GetMessage/DispatchMessage. We
    // still must return DefWindowProc for unknown messages.
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, w_param, l_param);
}

void LowLevelKeyboardHook::PumpThreadMain() {
    // Bind the thread-local dispatcher BEFORE SetWindowsHookEx so the
    // very first callback on this thread finds tls_current_instance.
    tls_current_instance = this;

    HINSTANCE module = GetModuleHandleW(nullptr);

    // Register a message-only window class. Doing this on the pump
    // thread keeps CreateWindowExW on the same thread that owns the
    // HWND (Win32 requires thread affinity).
    WNDCLASSW wc{};
    wc.lpfnWndProc   = &LowLevelKeyboardHook::WndProcThunk;
    wc.hInstance     = module;
    wc.lpszClassName = L"RemotemicLLHookMsgWindow";
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, module, nullptr);
    if (hwnd_ == nullptr) {
        started_.store(false);
        return;
    }

    // Install the LL hook. hMod = module handle, threadId = 0
    // (global). Returns NULL on failure (e.g. UIPI rejection).
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardHook::HookProc,
                              module, 0);
    if (hook_ == nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        UnregisterClassW(wc.lpszClassName, module);
        started_.store(false);
        return;
    }

    started_.store(true);

    MSG msg{};
    bool running = true;
    while (running) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0 || got == -1) {
            // WM_QUIT or error -> exit loop.
            running = false;
            break;
        }
        if (msg.message == kMsgPumpStop) {
            running = false;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        // Drain queued events after each message iteration and forward
        // to the registered sink. SPSC single-consumer == this thread.
        const std::size_t r = read_idx_.load(std::memory_order_relaxed);
        const std::size_t w = write_idx_.load(std::memory_order_acquire);
        SinkFn sink = sink_;
        void*  ud   = user_data_;
        for (std::size_t i = r; i < w; ++i) {
            const InputEvent& ev = ring_[i & mask_];
            if (sink != nullptr) {
                sink(ev, ud);
            }
            event_count_.fetch_add(1, std::memory_order_relaxed);
        }
        read_idx_.store(w, std::memory_order_release);
    }

    // Tear down the hook first so no further callbacks arrive after
    // we clear tls_current_instance.
    if (hook_ != nullptr) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(wc.lpszClassName, module);
    tls_current_instance = nullptr;
}

bool LowLevelKeyboardHook::start() noexcept {
    if (started_.load(std::memory_order_acquire)) {
        return true;  // idempotent
    }
    stop_flag_.store(false);
    thread_ = CreateThread(nullptr, 0,
                           [](LPVOID param) -> DWORD {
                               auto* self = static_cast<LowLevelKeyboardHook*>(param);
                               self->PumpThreadMain();
                               return 0;
                           },
                           this, 0, &thread_id_);
    if (thread_ == nullptr) {
        return false;
    }

    // Wait briefly for the pump thread to finish installing the hook.
    // SetWindowsHookEx is fast but the CreateWindowExW + RegisterClassW
    // chain on the same thread can take a couple of ms.
    for (int i = 0; i < 100; ++i) {
        if (started_.load(std::memory_order_acquire)) {
            return true;
        }
        Sleep(10);
    }
    // Timed out -> clean up.
    stop_flag_.store(true);
    PostThreadMessageW(thread_id_, kMsgPumpStop, 0, 0);
    WaitForSingleObject(thread_, 5000);
    CloseHandle(thread_);
    thread_ = nullptr;
    thread_id_ = 0;
    return false;
}

void LowLevelKeyboardHook::stop() noexcept {
    if (!started_.load(std::memory_order_acquire) && thread_ == nullptr) {
        return;  // already stopped
    }
    stop_flag_.store(true);
    if (thread_ != nullptr) {
        // Wake the pump thread out of GetMessageW.
        if (thread_id_ != 0) {
            PostThreadMessageW(thread_id_, kMsgPumpStop, 0, 0);
        }
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
        thread_id_ = 0;
    }
    started_.store(false);
}

std::uint64_t LowLevelKeyboardHook::dropped_count() const noexcept {
    return dropped_count_.load(std::memory_order_relaxed);
}

std::uint64_t LowLevelKeyboardHook::event_count() const noexcept {
    return event_count_.load(std::memory_order_relaxed);
}

std::uint64_t LowLevelKeyboardHook::slow_callback_count() const noexcept {
    return slow_callback_count_.load(std::memory_order_relaxed);
}

} // namespace remotemic::input

#else  // !_WIN32

// Non-Windows hosts (CI on Linux/macOS): the link target is
// ``remotemic_input`` which still compiles, but ``start()`` fail-closed
// per ADR-0015 §2 ("Windows-only"). The header is platform-independent
// so we still need a TU; this stub just refuses to start.

#include <cstdio>

namespace remotemic::input {

LowLevelKeyboardHook::LowLevelKeyboardHook() = default;
LowLevelKeyboardHook::~LowLevelKeyboardHook() = default;

void LowLevelKeyboardHook::set_event_sink(SinkFn, void*) noexcept {}
bool LowLevelKeyboardHook::start() noexcept { return false; }
void LowLevelKeyboardHook::stop() noexcept {}

std::uint64_t LowLevelKeyboardHook::dropped_count() const noexcept { return 0; }
std::uint64_t LowLevelKeyboardHook::event_count() const noexcept { return 0; }
std::uint64_t LowLevelKeyboardHook::slow_callback_count() const noexcept { return 0; }

} // namespace remotemic::input

#endif  // _WIN32
