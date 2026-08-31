// Phase 5 / ADR-0015 §3.6 step 2 sub-pass B: real Frida HID tap loopback
// socket reader. Mirrors ``apps/windows/rc003/src/ovb_rc003/
// frida_hid_tap_runtime.py``.
//
// 1. Connect to ``127.0.0.1:REMOTE_MIC_RC003_HID_TAP_PORT`` (default
//    30684). The Frida Gadget injected into the WUDF host pushes
//    newline-delimited JSON messages of the shape
//    ``{"kind":"gatt_read","raw":"<hex>"}`` over this socket.
// 2. The IO thread blocks in ``recv()`` and accumulates a line buffer.
// 3. On newline, parse the JSON payload. For ``gatt_read`` messages
//    extract the 9-byte raw report (RC003 HID report) and translate
//    each active usage ID into an ``InputEvent`` with
//    ``SourceKind::FridaHidTap``.
// 4. Push the events onto a lock-free SPSC ring buffer drained on
//    shutdown.
//
// Socket failures or disconnect -> IO thread exits and start() must
// be called again to reconnect. The first connection attempt may
// legitimately fail when no Frida Gadget is running; in that case
// ``start()`` returns ``false`` (fail-closed) per ADR-0015 §3.6.

#include <remotemic/input/frida_hid_tap_source.hpp>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace remotemic::input {

namespace {

constexpr SOCKET kInvalidSocket = static_cast<SOCKET>(~0ull);

// RC003 HID Usage ID -> button-name (from device_profile.py:43-57).
struct UsageIdEntry {
    std::uint16_t usage;
    const char*   button;
};

constexpr UsageIdEntry kUsageIdTable[] = {
    {0x003E, "mic"},
    {0x00F1, "back"},
    {0x0028, "ok"},
    {0x0035, "tv"},
    {0x004A, "home"},
    {0x004F, "right"},
    {0x0050, "left"},
    {0x0051, "down"},
    {0x0052, "up"},
    {0x0065, "menu"},
    {0x0066, "power"},
    {0x007F, "volume_mute"},
    {0x0080, "volume_up"},
    {0x0081, "volume_down"},
};

// Convert two ASCII hex characters to a byte. Returns 0 on parse
// failure (caller validates the entire string before calling).
inline int HexPairToByte(char hi, char lo) {
    auto nyb = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    const int hi_n = nyb(hi);
    const int lo_n = nyb(lo);
    if (hi_n < 0 || lo_n < 0) return -1;
    return (hi_n << 4) | lo_n;
}

// Parse a JSON object (very small subset) - just enough to read
// ``{"kind":"gatt_read","raw":"<hex>"}``. Anything else returns
// false. Hand-rolled to avoid pulling in a JSON dependency for the
// single-message hot path on the IO thread.
bool ParseGattRead(const std::string& line, std::vector<std::uint8_t>& out_bytes) {
    // Locate ``"raw":"`` substring.
    const std::string key = "\"raw\":\"";
    auto pos = line.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    pos += key.size();
    auto end = line.find('"', pos);
    if (end == std::string::npos || end <= pos) {
        return false;
    }
    const std::string hex = line.substr(pos, end - pos);
    if (hex.size() % 2 != 0) {
        return false;
    }
    out_bytes.clear();
    out_bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int b = HexPairToByte(hex[i], hex[i + 1]);
        if (b < 0) {
            return false;
        }
        out_bytes.push_back(static_cast<std::uint8_t>(b));
    }
    return !out_bytes.empty();
}

}  // namespace

FridaHidTapSource::FridaHidTapSource() = default;

FridaHidTapSource::~FridaHidTapSource() {
    stop();
}

void FridaHidTapSource::set_event_sink(SinkFn sink, void* user_data) noexcept {
    sink_ = sink;
    user_data_ = user_data;
}

void FridaHidTapSource::EnqueueEvent(InputEvent ev) {
    const std::size_t w = write_idx_.load(std::memory_order_relaxed);
    const std::size_t r = read_idx_.load(std::memory_order_acquire);
    if ((w - r) >= kQueueCapacity) {
        read_idx_.store(r + 1, std::memory_order_release);
        dropped_count_.fetch_add(1, std::memory_order_relaxed);
    }
    ring_[w & mask_] = ev;
    write_idx_.store(w + 1, std::memory_order_release);
}

void FridaHidTapSource::IoThreadMain() {
    // Look up REMOTE_MIC_RC003_HID_TAP_PORT if set (matches Python).
    // Use _dupenv_s to avoid MSVC C4996 deprecation warning on getenv.
    char* env_buf = nullptr;
    size_t env_len = 0;
    if (_dupenv_s(&env_buf, &env_len, "REMOTE_MIC_RC003_HID_TAP_PORT") == 0 &&
        env_buf != nullptr && env_len > 1) {
        const int parsed = std::atoi(env_buf);
        if (parsed > 0 && parsed < 65536) {
            port_ = parsed;
        }
        free(env_buf);
    }

    // Initialize Winsock (idempotent).
    WSADATA wsadata;
    const int ws_init = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (ws_init != 0) {
        started_.store(false);
        return;
    }

    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        started_.store(false);
        return;
    }
    sock_ = static_cast<std::uintptr_t>(s);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port_));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(s, reinterpret_cast<sockaddr*>(&addr),
                  static_cast<int>(sizeof(addr))) == SOCKET_ERROR) {
        ::closesocket(s);
        sock_ = static_cast<std::uintptr_t>(kInvalidSocket);
        WSACleanup();
        started_.store(false);
        return;
    }

    started_.store(true);

    std::string line_buffer;
    line_buffer.reserve(256);

    while (!stop_flag_.load(std::memory_order_acquire)) {
        char chunk[256];
        const int received = ::recv(s, chunk, sizeof(chunk), 0);
        if (received <= 0) {
            break;  // disconnect or error
        }
        line_buffer.append(chunk, static_cast<std::size_t>(received));

        // Drain complete lines (newline-delimited JSON).
        std::size_t start = 0;
        while (true) {
            const auto nl = line_buffer.find('\n', start);
            if (nl == std::string::npos) {
                line_buffer.erase(0, start);
                break;
            }
            std::string line = line_buffer.substr(start, nl - start);
            start = nl + 1;
            // Strip optional trailing CR.
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }

            // Only gatt_read messages generate InputEvents; ready /
            // heartbeat / error / others are ignored.
            if (line.find("\"gatt_read\"") == std::string::npos) {
                continue;
            }

            std::vector<std::uint8_t> bytes;
            if (!ParseGattRead(line, bytes)) {
                continue;
            }

            // The 9-byte RC003 HID report layout (from upstream):
            //   bytes[0]: modifier bitfield (we don't decode here)
            //   bytes[1]: reserved
            //   bytes[2..N]: active usage IDs (one byte each)
            // We treat any non-zero byte in bytes[2..] as an active
            // usage ID and emit a KeyDown InputEvent for it. A second
            // release byte (zero) isn't part of the protocol so the
            // consumer de-duplicates on demand.
            for (std::size_t i = 2; i < bytes.size(); ++i) {
                if (bytes[i] == 0) {
                    continue;
                }
                // Map usage -> button name (informational only).
                const char* button = nullptr;
                for (const auto& entry : kUsageIdTable) {
                    if (entry.usage == bytes[i]) {
                        button = entry.button;
                        break;
                    }
                }
                (void)button;

                InputEvent ev{};
                ev.timestamp = std::chrono::steady_clock::now();
                ev.source    = InputEvent::SourceKind::FridaHidTap;
                ev.kind      = InputEvent::EventKind::KeyDown;
                ev.usage_id  = bytes[i];
                ev.scan_code = 0;
                ev.vk_code   = 0;
                EnqueueEvent(ev);
            }
        }
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

    if (sock_ != static_cast<std::uintptr_t>(kInvalidSocket)) {
        ::closesocket(static_cast<SOCKET>(sock_));
        sock_ = static_cast<std::uintptr_t>(kInvalidSocket);
    }
    WSACleanup();
}

bool FridaHidTapSource::start() noexcept {
    if (started_.load(std::memory_order_acquire)) {
        return true;
    }
    stop_flag_.store(false);
    thread_ = CreateThread(nullptr, 0,
                           [](LPVOID param) -> DWORD {
                               auto* self = static_cast<FridaHidTapSource*>(param);
                               self->IoThreadMain();
                               return 0;
                           },
                           this, 0, nullptr);
    if (thread_ == nullptr) {
        return false;
    }

    // Wait briefly for connect() to complete (or fail).
    for (int i = 0; i < 100; ++i) {
        if (started_.load(std::memory_order_acquire)) {
            return true;
        }
        if (!started_.load(std::memory_order_acquire) &&
            thread_ != nullptr) {
            // Try waiting for thread exit to detect fail-closed early.
            DWORD wait_result = WaitForSingleObject(thread_, 0);
            if (wait_result == WAIT_OBJECT_0) {
                CloseHandle(thread_);
                thread_ = nullptr;
                return false;
            }
        }
        Sleep(10);
    }
    // Timed out without start() flipping -> assume still in connect
    // wait (long-running initial negotiation). Return true; the IO
    // thread will exit cleanly on stop().
    return true;
}

void FridaHidTapSource::stop() noexcept {
    stop_flag_.store(true);
    if (thread_ != nullptr) {
        // Force recv() to return by shutting down the socket.
        if (sock_ != static_cast<std::uintptr_t>(kInvalidSocket)) {
            ::shutdown(static_cast<SOCKET>(sock_),
                       SD_BOTH);
        }
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    sock_ = static_cast<std::uintptr_t>(kInvalidSocket);
    started_.store(false);
}

std::uint64_t FridaHidTapSource::dropped_count() const noexcept {
    return dropped_count_.load(std::memory_order_relaxed);
}

std::uint64_t FridaHidTapSource::event_count() const noexcept {
    return event_count_.load(std::memory_order_relaxed);
}

}  // namespace remotemic::input

#else  // !_WIN32

// Non-Windows hosts: fail-closed per ADR-0015 §2.

namespace remotemic::input {

FridaHidTapSource::FridaHidTapSource() = default;
FridaHidTapSource::~FridaHidTapSource() = default;

void FridaHidTapSource::set_event_sink(SinkFn, void*) noexcept {}
bool FridaHidTapSource::start() noexcept { return false; }
void FridaHidTapSource::stop() noexcept {}

std::uint64_t FridaHidTapSource::dropped_count() const noexcept { return 0; }
std::uint64_t FridaHidTapSource::event_count() const noexcept { return 0; }

}  // namespace remotemic::input

#endif  // _WIN32
