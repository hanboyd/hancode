// Phase 5 / ADR-0015 §3.5 step 2: real default-table ActionResolver
// implementation. Pure-logic; no I/O; thread-safe by construction.
// Mirrors ``apps/windows/rc003/src/ovb_rc003/key_mapping.py``
// default table (lines 104-117) one-to-one.

#include <remotemic/input/action_resolver.hpp>

namespace remotemic::input {

namespace {

// Win32 VK codes referenced by the default table. Values copied from
// apps/windows/rc003/src/ovb_rc003/win32_keys.py:VK_CODES so the C++
// table and the python baseline stay byte-identical.
constexpr std::uint16_t VK_BACK   = 0x08;
constexpr std::uint16_t VK_RETURN = 0x0D;
constexpr std::uint16_t VK_LEFT   = 0x25;
constexpr std::uint16_t VK_UP     = 0x26;
constexpr std::uint16_t VK_RIGHT  = 0x27;
constexpr std::uint16_t VK_DOWN   = 0x28;

ResolvedAction key_seq(std::uint16_t vk, bool key_down = true) noexcept {
    ResolvedAction ra{};
    ra.kind = ResolvedAction::Kind::KeySequence;
    ra.vk_code = vk;
    ra.key_down = key_down;
    return ra;
}

ResolvedAction sys(SystemAction action) noexcept {
    ResolvedAction ra{};
    ra.kind = ResolvedAction::Kind::SystemAction;
    ra.system_action = action;
    return ra;
}

} // namespace

std::optional<ResolvedAction>
DefaultActionResolver::resolve(ButtonId button) const noexcept {
    switch (button) {
    case ButtonId::Power:
        return sys(SystemAction::Escape);
    case ButtonId::ArrowUp:
        return key_seq(VK_UP);
    case ButtonId::ArrowDown:
        return key_seq(VK_DOWN);
    case ButtonId::ArrowLeft:
        return key_seq(VK_LEFT);
    case ButtonId::ArrowRight:
        return key_seq(VK_RIGHT);
    case ButtonId::Ok:
        return key_seq(VK_RETURN);
    case ButtonId::Back:
        // key_mapping.py default: "返回 -> Delete（退格）" -> VK_BACK.
        return key_seq(VK_BACK);
    case ButtonId::VolumeUp:
        return sys(SystemAction::VolumeUp);
    case ButtonId::VolumeDown:
        return sys(SystemAction::VolumeDown);
    case ButtonId::Home:
        return sys(SystemAction::ShowDesktop);
    case ButtonId::Menu:
        return sys(SystemAction::ContextMenu);
    case ButtonId::Tv:
        return sys(SystemAction::AppSwitch);
    case ButtonId::Mic:
        // Mic is owned by the voice hotkey path
        // (HotkeyPhysicalizer); not a default table entry.
        return std::nullopt;
    case ButtonId::VolumeMute:
        // Per key_mapping.py convention: no physical mute key on the
        // RC003; VolumeMute is user-bindable only.
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace remotemic::input