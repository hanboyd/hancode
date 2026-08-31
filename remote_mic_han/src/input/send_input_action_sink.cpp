// Phase 5 / ADR-0015 §3.7 step 2 sub-pass B: real SendInput adapter.
// Mirrors ``apps/windows/rc003/src/ovb_rc003/win32_input.py``.
//
// submit_key() pushes a (vk, key_down) pair onto a bounded queue
// (capacity 256, drop-oldest on overflow) and notifies a worker
// thread. The worker thread drains the queue and calls
// ``user32.SendInput`` once per batch.
//
// submit_system_action() bypasses the queue and dispatches Win32
// system commands directly on the caller thread:
//   - VolumeUp/Down/Mute: SendMessage(HWND_BROADCAST, WM_APPCOMMAND, ...)
//   - ShowDesktop:        keybd_event(kVkLWin, kVkD) (Win+D shortcut)
//   - Escape:             keybd_event(kVkEscape, ...)
//   - Return / Backspace: send_key equivalent
//   - ContextMenu:        keybd_event(kVkApps, ...)
//   - AppSwitch:          keybd_event(VK_LMENU, kVkTab)
//   - CodexOpen:          best-effort: keybd_event(kVkLWin, ...). Falls
//                         through to submit_key for non-modifier keys.
//
// Physical scan-code path is used for the modifier VK codes
// (ctrl / lctrl / rctrl / shift / lshift / rshift / alt / lalt /
// ralt / win / rwin) so left/right identity survives (per
// win32_input.py:_PHYSICAL_SCAN_CODES). Other keys use the virtual
// VK path. Extended keys (arrows, rctrl, ralt, rwin) carry
// KEYEVENTF_EXTENDEDKEY.

#include <remotemic/input/send_input_action_sink.hpp>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace remotemic::input {

namespace {

// Physical scan-code mapping from win32_input.py:_PHYSICAL_SCAN_CODES
// (mirrors win32_keys.VK_CODES so we use the same numeric VK constants).
constexpr std::uint16_t kVkCtrl  = 0x11;
constexpr std::uint16_t kVkShift = 0x10;
constexpr std::uint16_t kVkAlt   = 0x12;
constexpr std::uint16_t kVkLCtrl = 0xA2;
constexpr std::uint16_t kVkRCtrl = 0xA3;
constexpr std::uint16_t kVkLShift = 0xA0;
constexpr std::uint16_t kVkRShift = 0xA1;
constexpr std::uint16_t kVkLAlt  = 0xA4;
constexpr std::uint16_t kVkRAlt  = 0xA5;
constexpr std::uint16_t kVkLWin  = 0x5B;
constexpr std::uint16_t kVkRWin  = 0x5C;
constexpr std::uint16_t kVkLeft  = 0x25;
constexpr std::uint16_t kVkUp    = 0x26;
constexpr std::uint16_t kVkRight = 0x27;
constexpr std::uint16_t kVkDown  = 0x28;
constexpr std::uint16_t kVkApps  = 0x5D;
constexpr std::uint16_t kVkEscape = 0x1B;
constexpr std::uint16_t kVkTab    = 0x09;
constexpr std::uint16_t kVkD      = 0x44;

// Extended keys per win32_input.py:_EXTENDED_KEYS.
bool IsExtendedKey(std::uint16_t vk) noexcept {
    switch (vk) {
        case kVkLeft:
        case kVkUp:
        case kVkRight:
        case kVkDown:
        case kVkRCtrl:
        case kVkRAlt:
        case kVkRWin:
            return true;
        default:
            return false;
    }
}

// Returns (scan_code, is_extended) for the modifier VK codes; returns
// (0, false) for non-modifier keys (caller uses the VK path).
std::pair<WORD, bool> PhysicalScanCode(std::uint16_t vk) noexcept {
    switch (vk) {
        case kVkCtrl:
        case kVkLCtrl: return {static_cast<WORD>(0x1D), false};
        case kVkRCtrl: return {static_cast<WORD>(0x1D), true};
        case kVkShift:
        case kVkLShift: return {static_cast<WORD>(0x2A), false};
        case kVkRShift: return {static_cast<WORD>(0x36), false};
        case kVkAlt:
        case kVkLAlt:  return {static_cast<WORD>(0x38), false};
        case kVkRAlt:  return {static_cast<WORD>(0x38), true};
        case kVkLWin:  return {static_cast<WORD>(0x5B), true};
        case kVkRWin:  return {static_cast<WORD>(0x5C), true};
        default:       return {0, false};
    }
}

struct ScancodeRecord {
    std::uint16_t vk;
    bool          extended;
    WORD          scan_code;  // 0 means use VK path
};

// Build one INPUT per event. Returns false if memory exhausted.
bool AppendInput(std::vector<INPUT>& out, std::uint16_t vk, bool key_up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    auto& ki = in.ki;
    DWORD flags = key_up ? KEYEVENTF_KEYUP : 0;

    auto scan = PhysicalScanCode(vk);
    if (scan.first != 0) {
        // Physical scan-code path.
        ki.wVk = 0;
        ki.wScan = scan.first;
        if (scan.second) {
            flags |= KEYEVENTF_EXTENDEDKEY;
        }
        flags |= KEYEVENTF_SCANCODE;
    } else {
        ki.wVk = vk;
        ki.wScan = 0;
        if (IsExtendedKey(vk)) {
            flags |= KEYEVENTF_EXTENDEDKEY;
        }
    }
    ki.dwFlags = flags;
    ki.time = 0;
    ki.dwExtraInfo = 0;
    out.push_back(in);
    return true;
}

// Dispatches a batch of events through user32.SendInput. Returns the
// number of inputs accepted by Windows (may be < events.size()).
UINT SendInputBatch(const std::vector<std::pair<std::uint16_t, bool>>& events) {
    if (events.empty()) return 0;
    std::vector<INPUT> inputs;
    inputs.reserve(events.size());
    for (const auto& [vk, key_down] : events) {
        if (!AppendInput(inputs, vk, /*key_up=*/!key_down)) {
            break;
        }
    }
    if (inputs.empty()) return 0;
    return ::SendInput(static_cast<UINT>(inputs.size()),
                       inputs.data(),
                       static_cast<int>(sizeof(INPUT)));
}

// System action dispatch (run on caller thread).
void DispatchSystemAction(SystemAction action) {
    switch (action) {
        case SystemAction::VolumeUp:
            // SendMessage HWND_BROADCAST WM_APPCOMMAND 0 lparam=APPCOMMAND_VOLUME_UP<<16.
            ::SendMessageW(HWND_BROADCAST, WM_APPCOMMAND, 0,
                           MAKELPARAM(0, APPCOMMAND_VOLUME_UP));
            break;
        case SystemAction::VolumeDown:
            ::SendMessageW(HWND_BROADCAST, WM_APPCOMMAND, 0,
                           MAKELPARAM(0, APPCOMMAND_VOLUME_DOWN));
            break;
        case SystemAction::VolumeMute:
            ::SendMessageW(HWND_BROADCAST, WM_APPCOMMAND, 0,
                           MAKELPARAM(0, APPCOMMAND_VOLUME_MUTE));
            break;
        case SystemAction::ShowDesktop:
            // Win + D shortcut.
            ::keybd_event(static_cast<BYTE>(kVkLWin), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(kVkD), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(kVkD), 0, KEYEVENTF_KEYUP, 0);
            ::keybd_event(static_cast<BYTE>(kVkLWin), 0, KEYEVENTF_KEYUP, 0);
            break;
        case SystemAction::Escape:
            ::keybd_event(static_cast<BYTE>(kVkEscape), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(kVkEscape), 0, KEYEVENTF_KEYUP, 0);
            break;
        case SystemAction::Return:
            ::keybd_event(static_cast<BYTE>(VK_RETURN), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(VK_RETURN), 0, KEYEVENTF_KEYUP, 0);
            break;
        case SystemAction::Backspace:
            ::keybd_event(static_cast<BYTE>(VK_BACK), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(VK_BACK), 0, KEYEVENTF_KEYUP, 0);
            break;
        case SystemAction::ContextMenu:
            ::keybd_event(static_cast<BYTE>(kVkApps), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(kVkApps), 0, KEYEVENTF_KEYUP, 0);
            break;
        case SystemAction::AppSwitch:
            ::keybd_event(static_cast<BYTE>(VK_LMENU), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(kVkTab), 0, 0, 0);
            ::keybd_event(static_cast<BYTE>(kVkTab), 0, KEYEVENTF_KEYUP, 0);
            ::keybd_event(static_cast<BYTE>(VK_LMENU), 0, KEYEVENTF_KEYUP, 0);
            break;
        case SystemAction::CodexOpen:
            // No canonical Win32 binding for "open codex"; surface as
            // a no-op rather than guessing. Phase 7 Application may
            // register a richer handler that overrides this.
            break;
    }
}

bool VerifySendInputAvailable() {
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr) {
        // Try LoadLibrary for completeness.
        user32 = ::LoadLibraryW(L"user32.dll");
    }
    if (user32 == nullptr) {
        return false;
    }
    return ::GetProcAddress(user32, "SendInput") != nullptr;
}

}  // namespace

SendInputActionSink::SendInputActionSink() = default;

SendInputActionSink::~SendInputActionSink() {
    stop();
}

void SendInputActionSink::ClearQueueLocked() {
    key_queue_.clear();
}

void SendInputActionSink::WorkerThreadMain() {
    std::vector<std::pair<std::uint16_t, bool>> batch;
    batch.reserve(16);

    while (!stop_flag_.load(std::memory_order_acquire)) {
        {
            std::unique_lock<std::mutex> lk(queue_mu_);
            queue_cv_.wait_for(lk, std::chrono::milliseconds(50),
                               [this] {
                                   return stop_flag_.load(std::memory_order_acquire) ||
                                          !key_queue_.empty();
                               });
            batch.clear();
            for (const auto& item : key_queue_) {
                batch.push_back(item);
            }
            key_queue_.clear();
        }
        if (batch.empty()) {
            continue;
        }
        const UINT sent = SendInputBatch(batch);
        if (sent == batch.size()) {
            submitted_count_.fetch_add(batch.size(),
                                       std::memory_order_relaxed);
        } else {
            const std::size_t failed = batch.size() - sent;
            submit_error_count_.fetch_add(failed,
                                          std::memory_order_relaxed);
            submitted_count_.fetch_add(sent,
                                       std::memory_order_relaxed);
        }
    }
}

bool SendInputActionSink::start() noexcept {
    if (started_.load(std::memory_order_acquire)) {
        return true;
    }
    if (!VerifySendInputAvailable()) {
        started_.store(false);
        return false;
    }
    stop_flag_.store(false);
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        key_queue_.clear();
    }
    thread_ = CreateThread(nullptr, 0,
                           [](LPVOID param) -> DWORD {
                               auto* self = static_cast<SendInputActionSink*>(param);
                               self->WorkerThreadMain();
                               return 0;
                           },
                           this, 0, &thread_id_);
    if (thread_ == nullptr) {
        started_.store(false);
        return false;
    }
    started_.store(true);
    return true;
}

void SendInputActionSink::stop() noexcept {
    if (!started_.load(std::memory_order_acquire) && thread_ == nullptr) {
        return;
    }
    stop_flag_.store(true);
    queue_cv_.notify_all();
    if (thread_ != nullptr) {
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
        thread_id_ = 0;
    }
    cancel_pending();
    started_.store(false);
}

bool SendInputActionSink::submit_key(std::uint16_t vk_code, bool key_down,
                                     std::chrono::milliseconds /*deadline*/) noexcept {
    if (!started_.load(std::memory_order_acquire)) {
        ++submit_error_count_;
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        if (key_queue_.size() >= kQueueCapacity) {
            // Drop oldest to keep the queue bounded (per
            // win32_input.py:248-269 "partial delivery" handling).
            key_queue_.erase(key_queue_.begin());
        }
        key_queue_.emplace_back(vk_code, key_down);
    }
    queue_cv_.notify_one();
    return true;
}

bool SendInputActionSink::submit_system_action(SystemAction action) noexcept {
    if (!started_.load(std::memory_order_acquire)) {
        ++submit_error_count_;
        return false;
    }
    DispatchSystemAction(action);
    ++submitted_count_;
    return true;
}

void SendInputActionSink::cancel_pending() noexcept {
    std::lock_guard<std::mutex> lk(queue_mu_);
    ClearQueueLocked();
}

std::uint64_t SendInputActionSink::submit_error_count() const noexcept {
    return submit_error_count_.load(std::memory_order_relaxed);
}

std::uint64_t SendInputActionSink::submitted_count() const noexcept {
    return submitted_count_.load(std::memory_order_relaxed);
}

}  // namespace remotemic::input

#else  // !_WIN32

// Non-Windows hosts: fail-closed per ADR-0015 §2.

namespace remotemic::input {

SendInputActionSink::SendInputActionSink() = default;
SendInputActionSink::~SendInputActionSink() = default;

bool SendInputActionSink::submit_key(std::uint16_t, bool,
                                     std::chrono::milliseconds) noexcept {
    ++submit_error_count_;
    return false;
}
bool SendInputActionSink::submit_system_action(SystemAction) noexcept {
    ++submit_error_count_;
    return false;
}
void SendInputActionSink::cancel_pending() noexcept {}
bool SendInputActionSink::start() noexcept { return false; }
void SendInputActionSink::stop() noexcept {}

std::uint64_t SendInputActionSink::submit_error_count() const noexcept { return 0; }
std::uint64_t SendInputActionSink::submitted_count() const noexcept { return 0; }

}  // namespace remotemic::input

#endif  // _WIN32
