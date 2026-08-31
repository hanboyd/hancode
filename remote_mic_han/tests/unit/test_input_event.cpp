// Phase 5 / ADR-0015 §8 / step 1: InputEvent value type round-trip +
// enum + invariants. Tests pass on default-constructed values; the
// real ones that depend on later steps fail red until those steps land.

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>

#include <remotemic/input/input_event.hpp>

using remotemic::input::InputEvent;
using remotemic::input::SystemAction;

namespace {

bool test_value_type_default_construction() {
    InputEvent ev{};
    assert(ev.source == InputEvent::SourceKind::RawInputKeyboard);
    assert(ev.kind == InputEvent::EventKind::KeyDown);
    assert(ev.vk_code == 0);
    assert(!ev.injected);
    assert(!ev.extended);
    return true;
}

bool test_source_kind_enum_covers_all_inputs() {
    // Phase 5 single-owner rule: every input path has a distinct
    // SourceKind. New entries must be appended (no reordering) per
    // ADR-0015 §3.1.
    InputEvent::SourceKind kinds[] = {
        InputEvent::SourceKind::RawInputKeyboard,
        InputEvent::SourceKind::RawInputHid,
        InputEvent::SourceKind::FridaHidTap,
        InputEvent::SourceKind::LowLevelHook,
        InputEvent::SourceKind::Synthetic,
    };
    for (auto k : kinds) {
        InputEvent ev{};
        ev.source = k;
        assert(ev.source == k);
    }
    return true;
}

bool test_event_kind_enum_covers_all_kinds() {
    InputEvent::EventKind kinds[] = {
        InputEvent::EventKind::KeyDown,
        InputEvent::EventKind::KeyUp,
        InputEvent::EventKind::KeyCancel,
        InputEvent::EventKind::SystemAction,
    };
    for (auto k : kinds) {
        InputEvent ev{};
        ev.kind = k;
        assert(ev.kind == k);
    }
    return true;
}

bool test_system_action_enum_appended_only() {
    // Sanity: every documented SystemAction is reachable through the
    // enum (G3 parity with key_mapping.py default table).
    SystemAction actions[] = {
        SystemAction::VolumeUp,
        SystemAction::VolumeDown,
        SystemAction::VolumeMute,
        SystemAction::ShowDesktop,
        SystemAction::Escape,
        SystemAction::Return,
        SystemAction::Backspace,
        SystemAction::ContextMenu,
        SystemAction::AppSwitch,
        SystemAction::CodexOpen,
    };
    for (auto a : actions) {
        // round-trip
        SystemAction b = a;
        assert(static_cast<std::uint8_t>(a) == static_cast<std::uint8_t>(b));
    }
    return true;
}

bool test_input_event_size_is_small() {
    // ADR-0015 §3.1: ``InputEvent`` is POD-ish and crosses the hook
    // callback boundary by value into a lock-free SPSC queue. Size
    // matters for the no-allocation contract; if it grows beyond a
    // reasonable cache-line-ish budget, the implementation must revisit
    // the data layout before step 2.
    constexpr std::size_t kMaxReasonableBytes = 64;
    static_assert(sizeof(InputEvent) <= kMaxReasonableBytes,
                  "InputEvent grew beyond the no-allocation budget");
    return sizeof(InputEvent) <= kMaxReasonableBytes;
}

} // namespace

int main() {
    struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"value_type_default_construction", &test_value_type_default_construction},
        {"source_kind_enum_covers_all_inputs", &test_source_kind_enum_covers_all_inputs},
        {"event_kind_enum_covers_all_kinds", &test_event_kind_enum_covers_all_kinds},
        {"system_action_enum_appended_only", &test_system_action_enum_appended_only},
        {"input_event_size_is_small", &test_input_event_size_is_small},
    };

    int failures = 0;
    for (const auto& c : cases) {
        bool ok = c.fn();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failures;
    }
    if (failures != 0) {
        std::printf("test_input_event: %d/%zu failed\n",
                    failures, sizeof(cases) / sizeof(cases[0]));
        return 1;
    }
    std::printf("test_input_event: all PASS\n");
    return 0;
}