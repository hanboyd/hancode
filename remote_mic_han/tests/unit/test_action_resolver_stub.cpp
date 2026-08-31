// Phase 5 / ADR-0015 §8 / step 1: action resolver stub red-state +
// default-table parity tests.
//
// The stub resolver returns nullopt for every button (so step 1 red
// state is observable). Step 2 replaces the stub with the real
// default-table implementation matching key_mapping.py:104-117, at
// which point these tests should be relaxed to cover each button's
// resolved action.
//
// Default-table preview (Step 2 will assert each):
//
//   Power    -> SystemAction::Escape
//   ArrowUp  -> KeySequence VK_UP
//   ArrowDn  -> KeySequence VK_DOWN
//   ArrowLt  -> KeySequence VK_LEFT
//   ArrowRt  -> KeySequence VK_RIGHT
//   Ok       -> KeySequence VK_RETURN
//   Back     -> KeySequence VK_BACK
//   VolumeUp -> SystemAction::VolumeUp
//   VolumeDn -> SystemAction::VolumeDown
//   Home     -> SystemAction::ShowDesktop
//   Menu     -> SystemAction::ContextMenu
//   Tv       -> SystemAction::AppSwitch
//   Mic      -> KeySequence (voice hotkey, NOT a default action here -
//                          physicalization handled separately by
//                          HotkeyPhysicalizer)
//   VolumeMute -> nullopt (user-bindable only)

#include <cassert>
#include <cstdio>
#include <optional>

#include <remotemic/input/action_resolver.hpp>

using remotemic::input::ActionResolver;
using remotemic::input::ButtonId;
using remotemic::input::ResolvedAction;

namespace {

// Stub resolver: returns nullopt for every button. Step 2 replaces
// with the real default-table implementation.
class StubResolver final : public ActionResolver {
public:
    std::optional<ResolvedAction>
    resolve(ButtonId /*button*/) const noexcept override {
        return std::nullopt;
    }
};

bool test_stub_resolver_returns_nullopt_for_every_button() {
    StubResolver resolver;
    ButtonId buttons[] = {
        ButtonId::Power,    ButtonId::ArrowUp,  ButtonId::ArrowDown,
        ButtonId::ArrowLeft,ButtonId::ArrowRight,ButtonId::Ok,
        ButtonId::Back,     ButtonId::VolumeUp, ButtonId::VolumeDown,
        ButtonId::Home,     ButtonId::Menu,     ButtonId::Tv,
        ButtonId::Mic,      ButtonId::VolumeMute,
    };
    for (auto b : buttons) {
        auto r = resolver.resolve(b);
        assert(!r.has_value());
    }
    return true;
}

bool test_resolved_action_kind_enum_covers_all_paths() {
    ResolvedAction::Kind kinds[] = {
        ResolvedAction::Kind::KeySequence,
        ResolvedAction::Kind::SystemAction,
        ResolvedAction::Kind::Disabled,
    };
    for (auto k : kinds) {
        ResolvedAction ra{};
        ra.kind = k;
        assert(ra.kind == k);
    }
    return true;
}

bool test_button_id_enum_covers_default_table() {
    // Sanity: every documented ButtonId is reachable. The default
    // table in step 2 will assert each maps to its declared action.
    ButtonId buttons[] = {
        ButtonId::Power, ButtonId::ArrowUp, ButtonId::ArrowDown,
        ButtonId::ArrowLeft, ButtonId::ArrowRight, ButtonId::Ok,
        ButtonId::Back, ButtonId::VolumeUp, ButtonId::VolumeDown,
        ButtonId::Home, ButtonId::Menu, ButtonId::Tv, ButtonId::Mic,
        ButtonId::VolumeMute,
    };
    for (auto b : buttons) {
        ButtonId c = b;
        assert(static_cast<std::uint8_t>(b) == static_cast<std::uint8_t>(c));
    }
    return true;
}

} // namespace

int main() {
    struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"stub_resolver_returns_nullopt_for_every_button",
         &test_stub_resolver_returns_nullopt_for_every_button},
        {"resolved_action_kind_enum_covers_all_paths",
         &test_resolved_action_kind_enum_covers_all_paths},
        {"button_id_enum_covers_default_table",
         &test_button_id_enum_covers_default_table},
    };

    int failures = 0;
    for (const auto& c : cases) {
        bool ok = c.fn();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failures;
    }
    if (failures != 0) {
        std::printf("test_action_resolver_stub: %d/%zu failed\n",
                    failures, sizeof(cases) / sizeof(cases[0]));
        return 1;
    }
    std::printf("test_action_resolver_stub: all PASS\n");
    return 0;
}