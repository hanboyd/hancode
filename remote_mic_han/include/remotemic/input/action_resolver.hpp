#pragma once

#include <optional>

#include <remotemic/input/input_event.hpp>

namespace remotemic::input {

// Phase 5 / ADR-0015 §3.5: pure-logic action resolver. Maps a
// (RC003 button identifier, optional user binding override) to a
// concrete `` ``ResolvedAction`` `` value (a VK sequence or a
// SystemAction). Does NOT read JSON / YAML / any file; does NOT
// inspect process names / window titles / installed apps.
//
// Phase 5 ships with the empty override set (no user keymap); the
// default-table behavior is the only resolution path. Phase 7
// Application coordinator will own the user-binding surface.
enum class ButtonId : std::uint8_t {
    Power        = 0,
    ArrowUp      = 1,
    ArrowDown    = 2,
    ArrowLeft    = 3,
    ArrowRight   = 4,
    Ok           = 5,
    Back         = 6,
    VolumeUp     = 7,
    VolumeDown   = 8,
    Home         = 9,
    Menu         = 10,
    Tv           = 11,
    Mic          = 12,
    VolumeMute   = 13,  // user-bindable only; not in default table
};

struct ResolvedAction {
    enum class Kind : std::uint8_t {
        KeySequence,   // one or more VK + down/up pairs
        SystemAction,  // mapped to a SystemAction
        Disabled,      // user explicitly mapped to nothing
    };

    Kind kind{Kind::Disabled};
    std::uint16_t vk_code{0};
    SystemAction system_action{SystemAction::Escape};
    bool key_down{true};  // for KeySequence
};

class ActionResolver {
public:
    virtual ~ActionResolver() = default;

    // Returns nullopt when the button is unbound in the default table
    // AND no override is provided (e.g. VolumeMute without a user
    // binding). Resolution MUST be O(1) — no I/O, no allocation.
    virtual std::optional<ResolvedAction>
    resolve(ButtonId button) const noexcept = 0;
};

} // namespace remotemic::input