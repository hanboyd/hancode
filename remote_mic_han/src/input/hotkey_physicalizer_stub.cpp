// Phase 5 / ADR-0015 step 1 stub: HotkeyPhysicalizer physicalize()
// returns false (stub path doesn't resolve any chord). Step 2 replaces
// with a real chord-name -> VK sequence table matching
// ``apps/windows/rc003/src/ovb_rc003/hotkey.py``.

#include <cstring>

#include <remotemic/input/hotkey_physicalizer.hpp>

namespace remotemic::input {

HotkeyPhysicalizer::HotkeyPhysicalizer(IHostActionSink& sink) noexcept
    : sink_(sink) {}

bool HotkeyPhysicalizer::physicalize(const char* tokens) noexcept {
    if (tokens == nullptr || std::strlen(tokens) == 0) {
        ++physicalize_error_count_;
        return false;
    }
    // Stub: never resolves. Step 2 will parse slash-separated chord
    // names ("ralt", "lctrl+lalt", ...) and emit a VK down/up sequence.
    ++physicalize_error_count_;
    return false;
}

void HotkeyPhysicalizer::release_held() noexcept {
    // Stub: no-op. Step 2 will track currently-held VK codes and
    // submit the inverse on release.
}

} // namespace remotemic::input