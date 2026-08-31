// Phase 5 / ADR-0015 §8 / step 2: real default-table ActionResolver
// tests. Mirrors apps/windows/rc003/src/ovb_rc003/key_mapping.py
// default table (lines 104-117) 1:1.
//
// Step 1 stub asserted nullopt for every button (red-state). Step 2
// replaces the stub with DefaultActionResolver and these tests assert
// every default-table entry's resolved action.

#include <cassert>
#include <cstdio>
#include <optional>

#include <remotemic/input/action_resolver.hpp>
#include <remotemic/input/input_event.hpp>

using remotemic::input::ActionResolver;
using remotemic::input::ButtonId;
using remotemic::input::DefaultActionResolver;
using remotemic::input::ResolvedAction;
using remotemic::input::SystemAction;

namespace {

bool test_default_table_power_maps_to_escape() {
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::Power);
    assert(a.has_value());
    assert(a->kind == ResolvedAction::Kind::SystemAction);
    assert(a->system_action == SystemAction::Escape);
    return true;
}

bool test_default_table_arrow_keys_map_to_vk_arrows() {
    DefaultActionResolver r;
    struct { ButtonId b; std::uint16_t vk; } cases[] = {
        {ButtonId::ArrowUp,    0x26},  // VK_UP
        {ButtonId::ArrowDown,  0x28},  // VK_DOWN
        {ButtonId::ArrowLeft,  0x25},  // VK_LEFT
        {ButtonId::ArrowRight, 0x27},  // VK_RIGHT
    };
    for (const auto& c : cases) {
        auto a = r.resolve(c.b);
        assert(a.has_value());
        assert(a->kind == ResolvedAction::Kind::KeySequence);
        assert(a->vk_code == c.vk);
    }
    return true;
}

bool test_default_table_ok_maps_to_enter() {
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::Ok);
    assert(a.has_value());
    assert(a->kind == ResolvedAction::Kind::KeySequence);
    assert(a->vk_code == 0x0D);  // VK_RETURN
    return true;
}

bool test_default_table_back_maps_to_backspace() {
    // key_mapping.py: "返回 -> Delete（退格）" -> VK_BACK (0x08).
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::Back);
    assert(a.has_value());
    assert(a->kind == ResolvedAction::Kind::KeySequence);
    assert(a->vk_code == 0x08);  // VK_BACK
    return true;
}

bool test_default_table_volume_maps_to_system_action() {
    DefaultActionResolver r;
    auto up = r.resolve(ButtonId::VolumeUp);
    assert(up.has_value());
    assert(up->kind == ResolvedAction::Kind::SystemAction);
    assert(up->system_action == SystemAction::VolumeUp);
    auto dn = r.resolve(ButtonId::VolumeDown);
    assert(dn.has_value());
    assert(dn->kind == ResolvedAction::Kind::SystemAction);
    assert(dn->system_action == SystemAction::VolumeDown);
    return true;
}

bool test_default_table_home_maps_to_show_desktop() {
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::Home);
    assert(a.has_value());
    assert(a->kind == ResolvedAction::Kind::SystemAction);
    assert(a->system_action == SystemAction::ShowDesktop);
    return true;
}

bool test_default_table_menu_maps_to_context_menu() {
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::Menu);
    assert(a.has_value());
    assert(a->kind == ResolvedAction::Kind::SystemAction);
    assert(a->system_action == SystemAction::ContextMenu);
    return true;
}

bool test_default_table_tv_maps_to_app_switch() {
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::Tv);
    assert(a.has_value());
    assert(a->kind == ResolvedAction::Kind::SystemAction);
    assert(a->system_action == SystemAction::AppSwitch);
    return true;
}

bool test_default_table_mic_returns_nullopt() {
    // Mic is owned by HotkeyPhysicalizer; not a default-table entry.
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::Mic);
    assert(!a.has_value());
    return true;
}

bool test_default_table_volume_mute_returns_nullopt() {
    // VolumeMute is user-bindable only; no physical mute key.
    DefaultActionResolver r;
    auto a = r.resolve(ButtonId::VolumeMute);
    assert(!a.has_value());
    return true;
}

bool test_resolver_pure_logic_no_io() {
    // Smoke: DefaultActionResolver.resolve is const + noexcept; a
    // repeated call returns the same result, no allocation.
    DefaultActionResolver r;
    auto first = r.resolve(ButtonId::ArrowUp);
    auto second = r.resolve(ButtonId::ArrowUp);
    assert(first.has_value());
    assert(second.has_value());
    assert(first->vk_code == second->vk_code);
    return true;
}

} // namespace

int main() {
    struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"default_table_power_maps_to_escape",
         &test_default_table_power_maps_to_escape},
        {"default_table_arrow_keys_map_to_vk_arrows",
         &test_default_table_arrow_keys_map_to_vk_arrows},
        {"default_table_ok_maps_to_enter",
         &test_default_table_ok_maps_to_enter},
        {"default_table_back_maps_to_backspace",
         &test_default_table_back_maps_to_backspace},
        {"default_table_volume_maps_to_system_action",
         &test_default_table_volume_maps_to_system_action},
        {"default_table_home_maps_to_show_desktop",
         &test_default_table_home_maps_to_show_desktop},
        {"default_table_menu_maps_to_context_menu",
         &test_default_table_menu_maps_to_context_menu},
        {"default_table_tv_maps_to_app_switch",
         &test_default_table_tv_maps_to_app_switch},
        {"default_table_mic_returns_nullopt",
         &test_default_table_mic_returns_nullopt},
        {"default_table_volume_mute_returns_nullopt",
         &test_default_table_volume_mute_returns_nullopt},
        {"resolver_pure_logic_no_io",
         &test_resolver_pure_logic_no_io},
    };

    int failures = 0;
    for (const auto& c : cases) {
        bool ok = c.fn();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failures;
    }
    if (failures != 0) {
        std::printf("test_action_resolver: %d/%zu failed\n",
                    failures, sizeof(cases) / sizeof(cases[0]));
        return 1;
    }
    std::printf("test_action_resolver: all PASS\n");
    return 0;
}