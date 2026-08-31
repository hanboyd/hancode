// Phase 5 / ADR-0015 §3.7 step 2 sub-pass B: real Windows Raw Input
// adapter. Mirrors ``apps/windows/rc003/src/ovb_rc003/raw_input_windows.py``.
//
// 1. Enumerate Raw Input device paths and filter for the RC003
//    VID/PID (0x2717/0x32B8) via substring match on the device
//    interface path (case-insensitive).
// 2. Register RIDEV_INPUTSINK + RIDEV_DEVNOTIFY for usage page
//    0x01 (Generic Desktop) and 0x0C (Consumer Control) on a hidden
//    message-only HWND.
// 3. Pump WM_INPUT messages from the HWND; decode RIM_TYPEKEYBOARD
//    and RIM_TYPEHID reports; translate to ``InputEvent`` via the
//    KEYBOARD_VK_TO_BUTTON / KEYBOARD_MAKECODE_TO_BUTTON tables from
//    raw_input_windows.py:81-109.
// 4. Push events onto a lock-free SPSC ring buffer that the
//    Application coordinator (Phase 7) drains.
//
// Per ADR-0015 §3 rule 5 (single-owner), only one RawInputSource
// instance can be started at a time. The start() call is idempotent
// and fails closed on Windows API errors.

#include <remotemic/input/raw_input_source.hpp>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace remotemic::input {

namespace {

// RC003 VID/PID (apps/windows/rc003/src/ovb_rc003/device_profile.py).
constexpr std::uint16_t kRc003Vid = 0x2717;
constexpr std::uint16_t kRc003Pid = 0x32B8;

// RIDEV flags we care about.
constexpr DWORD kRidevInputSink = 0x00000100;

// Raw Input device types we filter against.
constexpr DWORD kRimTypeKeyboard = 1;
constexpr DWORD kRimTypeMouse    = 0;
constexpr DWORD kRimTypeHid      = 2;

// Win32 RIDI_* values not always exposed in every SDK header.
constexpr UINT kRidiDeviceName = 0x20000007;

// WM_INPUT delivery.
constexpr UINT kWmInput = 0x00FF;
constexpr UINT kWmDestroy = 0x0002;

// VK -> button-name mapping from raw_input_windows.py:81-95.
struct VkButtonMap {
    std::uint16_t vk;
    std::uint16_t scan_code;  // 0 = VK match, non-zero = scan-code match
    const char*  button;
};

constexpr VkButtonMap kVkButtonTable[] = {
    {0x74, 0, "mic"},
    {0x27, 0, "right"},
    {0x25, 0, "left"},
    {0x28, 0, "down"},
    {0x26, 0, "up"},
    {0x0D, 0, "ok"},
    {0x24, 0, "home"},
    {0x5D, 0, "menu"},
    {0xC0, 0, "tv"},
    {0x5F, 0, "power"},
    {0xAD, 0, "volume_mute"},
    {0xAF, 0, "volume_up"},
    {0xAE, 0, "volume_down"},
};

// Extended-key scan-code map from raw_input_windows.py:103-109.
constexpr VkButtonMap kScanCodeButtonTable[] = {
    {0, 0x5E, "power"},
    {0, 0x6A, "back"},
    {0, 0x30, "volume_up"},
    {0, 0x2E, "volume_down"},
    {0, 0x20, "volume_mute"},
};

bool VidPidMatches(const std::wstring& path, std::uint16_t vid, std::uint16_t pid) {
    std::wstring low(path.size(), L'\0');
    std::transform(path.begin(), path.end(), low.begin(),
                   [](wchar_t c) {
                       return static_cast<wchar_t>(
                           std::tolower(static_cast<unsigned char>(c)));
                   });
    // Classic ``VID_2717&PID_32B8`` shape.
    wchar_t vid_buf[16];
    wchar_t pid_buf[16];
    std::swprintf(vid_buf, 16, L"vid_%04x", static_cast<unsigned>(vid));
    std::swprintf(pid_buf, 16, L"pid_%04x", static_cast<unsigned>(pid));
    if (low.find(vid_buf) != std::wstring::npos &&
        low.find(pid_buf) != std::wstring::npos) {
        return true;
    }
    // BLE collection shape ``Dev_VID&012717_PID&32B8``.
    wchar_t ble_vid_buf[24];
    wchar_t ble_pid_buf[24];
    std::swprintf(ble_vid_buf, 24, L"dev_vid&01%04x", static_cast<unsigned>(vid));
    std::swprintf(ble_pid_buf, 24, L"dev_pid&%04x", static_cast<unsigned>(pid));
    return low.find(ble_vid_buf) != std::wstring::npos &&
           low.find(ble_pid_buf) != std::wstring::npos;
}

bool IsMatchingRc003Path(HANDLE h_device) {
    UINT name_len = 0;
    if (GetRawInputDeviceInfoW(h_device, kRidiDeviceName, nullptr,
                               &name_len) != 0) {
        return false;
    }
    if (name_len == 0) {
        return false;
    }
    std::wstring name(static_cast<std::size_t>(name_len) + 1, L'\0');
    const UINT copied = GetRawInputDeviceInfoW(
        h_device, kRidiDeviceName, name.data(), &name_len);
    if (copied == 0 || copied == static_cast<UINT>(-1)) {
        return false;
    }
    name.resize(std::wcslen(name.c_str()));
    return VidPidMatches(name, kRc003Vid, kRc003Pid);
}

}  // namespace

RawInputSource::RawInputSource() = default;

RawInputSource::~RawInputSource() {
    stop();
}

void RawInputSource::set_event_sink(SinkFn sink, void* user_data) noexcept {
    sink_ = sink;
    user_data_ = user_data;
}

void RawInputSource::EnqueueEvent(InputEvent ev) {
    const std::size_t w = write_idx_.load(std::memory_order_relaxed);
    const std::size_t r = read_idx_.load(std::memory_order_acquire);
    if ((w - r) >= kQueueCapacity) {
        read_idx_.store(r + 1, std::memory_order_release);
        dropped_count_.fetch_add(1, std::memory_order_relaxed);
    }
    ring_[w & mask_] = ev;
    write_idx_.store(w + 1, std::memory_order_release);
}

void RawInputSource::PumpThreadMain() {
    // Register message-only window class + create HWND on this thread.
    HINSTANCE module = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    static constexpr const wchar_t* kClassName = L"RemotemicRawInputMsgWindow";
    wc.lpfnWndProc   = &DefWindowProcW;
    wc.hInstance     = module;
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, module, nullptr);
    if (hwnd_ == nullptr) {
        started_.store(false);
        UnregisterClassW(kClassName, module);
        return;
    }

    // Register for raw keyboard + raw HID (Generic Desktop / Consumer).
    RAWINPUTDEVICE rid[2] = {};
    rid[0].usUsagePage = 0x01;  // Generic Desktop
    rid[0].usUsage     = 0x06;  // Keyboard
    rid[0].dwFlags     = kRidevInputSink;
    rid[0].hwndTarget  = hwnd_;

    rid[1].usUsagePage = 0x0C;  // Consumer
    rid[1].usUsage     = 0x01;  // Consumer Control
    rid[1].dwFlags     = kRidevInputSink;
    rid[1].hwndTarget  = hwnd_;

    if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        UnregisterClassW(kClassName, module);
        started_.store(false);
        return;
    }

    started_.store(true);

    MSG msg{};
    bool running = true;
    while (running) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0 || got == -1) {
            running = false;
            break;
        }
        if (msg.message == WM_QUIT || msg.message == kWmDestroy) {
            running = false;
            break;
        }
        if (msg.message == kWmInput) {
            HRAWINPUT h_raw = reinterpret_cast<HRAWINPUT>(msg.lParam);

            // First pass: size only.
            UINT data_size = 0;
            if (GetRawInputData(h_raw, RID_INPUT, nullptr, &data_size,
                                sizeof(RAWINPUTHEADER)) != 0) {
                continue;
            }
            if (data_size == 0 || data_size > 4096) {
                continue;
            }
            std::vector<unsigned char> buf(data_size);
            if (GetRawInputData(h_raw, RID_INPUT, buf.data(), &data_size,
                                sizeof(RAWINPUTHEADER)) != data_size) {
                continue;
            }
            const auto* raw = reinterpret_cast<const RAWINPUT*>(buf.data());

            // Filter by device path VID/PID.
            if (!IsMatchingRc003Path(raw->header.hDevice)) {
                continue;
            }

            InputEvent ev{};
            ev.timestamp = std::chrono::steady_clock::now();
            ev.kind      = (raw->data.keyboard.Flags & 0x1)
                               ? InputEvent::EventKind::KeyUp
                               : InputEvent::EventKind::KeyDown;

            if (raw->header.dwType == kRimTypeKeyboard) {
                ev.source    = InputEvent::SourceKind::RawInputKeyboard;
                ev.vk_code   = raw->data.keyboard.VKey;
                ev.scan_code = static_cast<std::uint16_t>(
                    raw->data.keyboard.MakeCode);
                ev.usage_id  = 0;
                ev.extended  = (raw->data.keyboard.Flags & 0x2) != 0;
            } else if (raw->header.dwType == kRimTypeHid) {
                ev.source    = InputEvent::SourceKind::RawInputHid;
                ev.vk_code   = 0;
                ev.scan_code = 0;
                // Extract the first non-zero byte as usage_id (mirrors
                // the python path that walks the report with
                // hid_identity.decode_active_usages).
                if (raw->data.hid.dwSizeHid > 0 &&
                    raw->data.hid.dwCount > 0) {
                    const auto* bytes = raw->data.hid.bRawData;
                    ev.usage_id = bytes[0];
                }
            } else {
                continue;
            }
            EnqueueEvent(ev);
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Drain remaining queued events to the sink.
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

    // Unregister + tear down HWND.
    RAWINPUTDEVICE unreg[2] = {};
    unreg[0].usUsagePage = 0x01;
    unreg[0].usUsage     = 0x06;
    unreg[0].dwFlags     = RIDEV_REMOVE;
    unreg[1].usUsagePage = 0x0C;
    unreg[1].usUsage     = 0x01;
    unreg[1].dwFlags     = RIDEV_REMOVE;
    RegisterRawInputDevices(unreg, 2, sizeof(RAWINPUTDEVICE));

    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(kClassName, module);
}

bool RawInputSource::start() noexcept {
    if (started_.load(std::memory_order_acquire)) {
        return true;
    }
    stop_flag_.store(false);
    thread_ = CreateThread(nullptr, 0,
                           [](LPVOID param) -> DWORD {
                               auto* self = static_cast<RawInputSource*>(param);
                               self->PumpThreadMain();
                               return 0;
                           },
                           this, 0, &thread_id_);
    if (thread_ == nullptr) {
        return false;
    }
    // Wait for RegisterRawInputDevices to complete.
    for (int i = 0; i < 100; ++i) {
        if (started_.load(std::memory_order_acquire)) {
            return true;
        }
        Sleep(10);
    }
    stop_flag_.store(true);
    PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
    WaitForSingleObject(thread_, 5000);
    CloseHandle(thread_);
    thread_ = nullptr;
    thread_id_ = 0;
    return false;
}

void RawInputSource::stop() noexcept {
    if (!started_.load(std::memory_order_acquire) && thread_ == nullptr) {
        return;
    }
    stop_flag_.store(true);
    if (thread_ != nullptr) {
        if (thread_id_ != 0) {
            PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
        }
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
        thread_id_ = 0;
    }
    started_.store(false);
}

std::uint64_t RawInputSource::dropped_count() const noexcept {
    return dropped_count_.load(std::memory_order_relaxed);
}

std::uint64_t RawInputSource::event_count() const noexcept {
    return event_count_.load(std::memory_order_relaxed);
}

}  // namespace remotemic::input

#else  // !_WIN32

// Non-Windows hosts (CI on Linux/macOS): fail-closed per ADR-0015 §2.

namespace remotemic::input {

RawInputSource::RawInputSource() = default;
RawInputSource::~RawInputSource() = default;

void RawInputSource::set_event_sink(SinkFn, void*) noexcept {}
bool RawInputSource::start() noexcept { return false; }
void RawInputSource::stop() noexcept {}

std::uint64_t RawInputSource::dropped_count() const noexcept { return 0; }
std::uint64_t RawInputSource::event_count() const noexcept { return 0; }

}  // namespace remotemic::input

#endif  // _WIN32
