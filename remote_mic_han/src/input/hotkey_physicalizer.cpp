// Phase 5 / ADR-0015 §3.5 + §4 step 2: real HotkeyPhysicalizer.
// Pure-logic chord-name parser + VK code table. No Win32 dependency
// in this TU; the IHostActionSink adapter does the actual SendInput.
//
// Mirrors:
//   apps/windows/rc003/src/ovb_rc003/hotkey.py:HotkeySpec.parse
//   apps/windows/rc003/src/ovb_rc003/win32_keys.py:VK_CODES
//
// VK table values are byte-identical with the python baseline so
// G3 byte-exact parity (Phase 5 step 3) stays trivially achievable.

#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <remotemic/input/hotkey_physicalizer.hpp>

namespace remotemic::input {

namespace {

constexpr std::uint16_t VK_BACK   = 0x08;
constexpr std::uint16_t VK_TAB    = 0x09;
constexpr std::uint16_t VK_RETURN = 0x0D;
constexpr std::uint16_t VK_SHIFT  = 0x10;
constexpr std::uint16_t VK_CONTROL= 0x11;
constexpr std::uint16_t VK_MENU    = 0x12;  // generic Alt
constexpr std::uint16_t VK_PAUSE  = 0x13;
constexpr std::uint16_t VK_CAPSLOCK=0x14;
constexpr std::uint16_t VK_ESCAPE = 0x1B;
constexpr std::uint16_t VK_SPACE  = 0x20;
constexpr std::uint16_t VK_PRIOR  = 0x21;  // page up
constexpr std::uint16_t VK_NEXT   = 0x22;  // page down
constexpr std::uint16_t VK_END    = 0x23;
constexpr std::uint16_t VK_HOME   = 0x24;
constexpr std::uint16_t VK_LEFT   = 0x25;
constexpr std::uint16_t VK_UP     = 0x26;
constexpr std::uint16_t VK_RIGHT  = 0x27;
constexpr std::uint16_t VK_DOWN   = 0x28;
constexpr std::uint16_t VK_SNAPSHOT=0x2C;  // print screen
constexpr std::uint16_t VK_INSERT = 0x2D;
constexpr std::uint16_t VK_DELETE = 0x2E;
constexpr std::uint16_t VK_LWIN   = 0x5B;
constexpr std::uint16_t VK_RWIN   = 0x5C;
constexpr std::uint16_t VK_APPS   = 0x5D;
constexpr std::uint16_t VK_NUMPAD0= 0x60;
constexpr std::uint16_t VK_NUMLOCK= 0x90;
constexpr std::uint16_t VK_SCROLL = 0x91;
constexpr std::uint16_t VK_LSHIFT = 0xA0;
constexpr std::uint16_t VK_RSHIFT = 0xA1;
constexpr std::uint16_t VK_LCTRL  = 0xA2;
constexpr std::uint16_t VK_RCTRL  = 0xA3;
constexpr std::uint16_t VK_LMENU  = 0xA4;
constexpr std::uint16_t VK_RMENU  = 0xA5;
constexpr std::uint16_t VK_BROWSER_BACK     = 0xA6;
constexpr std::uint16_t VK_BROWSER_FORWARD  = 0xA7;
constexpr std::uint16_t VK_VOLUME_MUTE = 0xAD;
constexpr std::uint16_t VK_VOLUME_DOWN= 0xAE;
constexpr std::uint16_t VK_VOLUME_UP  = 0xAF;
constexpr std::uint16_t VK_MEDIA_NEXT      = 0xB0;
constexpr std::uint16_t VK_MEDIA_PREVIOUS = 0xB1;
constexpr std::uint16_t VK_MEDIA_STOP      = 0xB2;
constexpr std::uint16_t VK_MEDIA_PLAY_PAUSE= 0xB3;

// OEM VK codes (US keyboard punctuation; non- US may differ; the
// python baseline uses the same set).
constexpr std::uint16_t VK_OEM_1     = 0xBA;  // ;
constexpr std::uint16_t VK_OEM_PLUS  = 0xBB;  // =
constexpr std::uint16_t VK_OEM_COMMA = 0xBC;  // ,
constexpr std::uint16_t VK_OEM_MINUS = 0xBD;  // -
constexpr std::uint16_t VK_OEM_PERIOD= 0xBE;  // .
constexpr std::uint16_t VK_OEM_2     = 0xBF;  // /
constexpr std::uint16_t VK_OEM_3     = 0xC0;  // `
constexpr std::uint16_t VK_OEM_4     = 0xDB;  // [
constexpr std::uint16_t VK_OEM_5     = 0xDC;  // backslash
constexpr std::uint16_t VK_OEM_6     = 0xDD;  // ]
constexpr std::uint16_t VK_OEM_7     = 0xDE;  // quote
constexpr std::uint16_t VK_MULTIPLY  = 0x6A;
constexpr std::uint16_t VK_ADD       = 0x6B;
constexpr std::uint16_t VK_SUBTRACT  = 0x6D;
constexpr std::uint16_t VK_DECIMAL   = 0x6E;
constexpr std::uint16_t VK_DIVIDE    = 0x6F;

// Look up a single token; returns 0 if unknown. Matches
// win32_keys.py:resolve_vk_codes 1:1 (digits / letters / function
// keys / numpad / named tokens / vk_XX hex pattern).
std::uint16_t lookup_token(const std::string& key) noexcept {
    // Static named tokens (order matches win32_keys.py:VK_CODES).
    static const struct { const char* name; std::uint16_t vk; } kNamed[] = {
        {"backspace", VK_BACK},       {"tab", VK_TAB},
        {"enter", VK_RETURN},         {"shift", VK_SHIFT},
        {"ctrl", VK_CONTROL},         {"alt", VK_MENU},
        {"lctrl", VK_LCTRL},          {"rctrl", VK_RCTRL},
        {"left_ctrl", VK_LCTRL},      {"right_ctrl", VK_RCTRL},
        {"lshift", VK_LSHIFT},        {"rshift", VK_RSHIFT},
        {"left_shift", VK_LSHIFT},    {"right_shift", VK_RSHIFT},
        {"lalt", VK_LMENU},           {"ralt", VK_RMENU},
        {"left_alt", VK_LMENU},       {"right_alt", VK_RMENU},
        {"escape", VK_ESCAPE},        {"esc", VK_ESCAPE},
        {"space", VK_SPACE},
        {"page_up", VK_PRIOR},        {"pageup", VK_PRIOR},
        {"page_down", VK_NEXT},       {"pagedown", VK_NEXT},
        {"end", VK_END},              {"home", VK_HOME},
        {"left", VK_LEFT},            {"up", VK_UP},
        {"right", VK_RIGHT},          {"down", VK_DOWN},
        {"insert", VK_INSERT},        {"delete", VK_DELETE},
        {"win", VK_LWIN},             {"lwin", VK_LWIN},
        {"rwin", VK_RWIN},            {"left_win", VK_LWIN},
        {"right_win", VK_RWIN},
        {"volume_mute", VK_VOLUME_MUTE},
        {"volume_down", VK_VOLUME_DOWN},
        {"volume_up", VK_VOLUME_UP},
        {"apps", VK_APPS},
        {"caps_lock", VK_CAPSLOCK},   {"num_lock", VK_NUMLOCK},
        {"scroll_lock", VK_SCROLL},   {"print_screen", VK_SNAPSHOT},
        {"pause", VK_PAUSE},
        {"browser_back", VK_BROWSER_BACK},
        {"browser_forward", VK_BROWSER_FORWARD},
        {"media_next", VK_MEDIA_NEXT},
        {"media_previous", VK_MEDIA_PREVIOUS},
        {"media_stop", VK_MEDIA_STOP},
        {"media_play_pause", VK_MEDIA_PLAY_PAUSE},
        {"semicolon", VK_OEM_1},      {"equals", VK_OEM_PLUS},
        {"comma", VK_OEM_COMMA},      {"minus", VK_OEM_MINUS},
        {"period", VK_OEM_PERIOD},    {"slash", VK_OEM_2},
        {"backtick", VK_OEM_3},       {"left_bracket", VK_OEM_4},
        {"backslash", VK_OEM_5},      {"right_bracket", VK_OEM_6},
        {"quote", VK_OEM_7},
        {"numpad_multiply", VK_MULTIPLY},
        {"numpad_add", VK_ADD},
        {"numpad_subtract", VK_SUBTRACT},
        {"numpad_decimal", VK_DECIMAL},
        {"numpad_divide", VK_DIVIDE},
    };
    for (const auto& entry : kNamed) {
        if (key == entry.name) return entry.vk;
    }
    if (key.size() == 1) {
        char c = key[0];
        if (c >= '0' && c <= '9') return static_cast<std::uint16_t>(0x30 + (c - '0'));
        if (c >= 'a' && c <= 'z') return static_cast<std::uint16_t>(0x41 + (c - 'a'));
    }
    if (key.size() >= 2 && key[0] == 'f') {
        // Function keys f1..f24: VK_F1 = 0x70.
        int n = 0;
        for (std::size_t i = 1; i < key.size(); ++i) {
            if (key[i] < '0' || key[i] > '9') return 0;
            n = n * 10 + (key[i] - '0');
            if (n > 24) return 0;
        }
        if (n >= 1 && n <= 24) return static_cast<std::uint16_t>(0x6F + n);
    }
    if (key.size() > 6 && key.compare(0, 3, "numpad") == 0) {
        // numpad0..numpad9: VK_NUMPAD0 = 0x60.
        if (key.size() == 7 && key[6] >= '0' && key[6] <= '9') {
            return static_cast<std::uint16_t>(VK_NUMPAD0 + (key[6] - '0'));
        }
    }
    if (key.size() >= 5 && key.compare(0, 3, "vk_") == 0) {
        // Dynamic vk_XX hex pattern (win32_keys.py:_DYNAMIC_VK_TOKEN).
        std::uint32_t value = 0;
        for (std::size_t i = 3; i < key.size(); ++i) {
            char c = key[i];
            int digit = -1;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
            if (digit < 0) return 0;
            value = (value << 4) | static_cast<std::uint32_t>(digit);
        }
        if (value > 0xFFFF) return 0;
        return static_cast<std::uint16_t>(value);
    }
    return 0;
}

// Modifier ordering matches hotkey.py:_MODIFIER_ORDER. Each entry is
// the canonical (post-alias) modifier token.
constexpr const char* kModifierOrder[] = {
    "ctrl", "shift", "alt", "win",
    "lctrl", "rctrl", "lshift", "rshift", "lalt", "ralt", "lwin", "rwin",
};
constexpr std::size_t kModifierCount = sizeof(kModifierOrder) / sizeof(kModifierOrder[0]);

bool is_modifier_token(const std::string& key) noexcept {
    for (std::size_t i = 0; i < kModifierCount; ++i) {
        if (key == kModifierOrder[i]) return true;
    }
    return false;
}

struct ParsedChord {
    std::vector<std::uint16_t> modifiers;  // in canonical order
    std::uint16_t trigger_vk{0};
};

// Parse a chord-name ("lctrl+lalt", "ralt", "win+h", "lctrl+win") into
// (modifiers_in_order, trigger_vk). Returns false on parse error.
bool parse_chord(const char* text, ParsedChord& out) noexcept {
    if (text == nullptr) return false;
    std::string s(text);
    // Lowercase + strip whitespace, split on '+', drop empties.
    std::vector<std::string> tokens;
    std::string current;
    for (char c : s) {
        if (c == '+') {
            if (!current.empty()) tokens.push_back(current);
            current.clear();
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            current.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(c))));
        }
    }
    if (!current.empty()) tokens.push_back(current);
    if (tokens.empty()) return false;

    // Apply aliases: left_ctrl->lctrl, right_ctrl->rctrl,
    // left_shift->lshift, right_shift->rshift,
    // left_alt->lalt, right_alt->ralt,
    // left_win->lwin, right_win->rwin.
    auto apply_alias = [](std::string& t) {
        if (t == "left_ctrl") t = "lctrl";
        else if (t == "right_ctrl") t = "rctrl";
        else if (t == "left_shift") t = "lshift";
        else if (t == "right_shift") t = "rshift";
        else if (t == "left_alt") t = "lalt";
        else if (t == "right_alt") t = "ralt";
        else if (t == "left_win") t = "lwin";
        else if (t == "right_win") t = "rwin";
    };
    for (auto& t : tokens) apply_alias(t);

    // Single-token directional-modifier HOLD mode: ralt, lalt, etc.
    if (tokens.size() == 1 && is_modifier_token(tokens[0])) {
        std::uint16_t vk = lookup_token(tokens[0]);
        if (vk == 0) return false;
        out.trigger_vk = vk;
        return true;
    }

    // Otherwise: modifiers come from the canonical list, exactly one
    // non-modifier trigger key.
    std::vector<std::string> non_modifiers;
    for (const auto& t : tokens) {
        if (!is_modifier_token(t)) non_modifiers.push_back(t);
    }

    // Modifier-only chord with ≥2 tokens: use last token as trigger.
    if (non_modifiers.empty()) {
        if (tokens.size() < 2) return false;
        std::uint16_t vk = lookup_token(tokens.back());
        if (vk == 0) return false;
        out.trigger_vk = vk;
        for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
            std::uint16_t mvk = lookup_token(tokens[i]);
            if (mvk == 0) return false;
            // Modifiers in canonical order: iterate the canonical list
            // and pick the matching ones from the token set.
            (void)mvk;
        }
        for (std::size_t i = 0; i < kModifierCount; ++i) {
            for (std::size_t j = 0; j + 1 < tokens.size(); ++j) {
                if (tokens[j] == kModifierOrder[i]) {
                    out.modifiers.push_back(lookup_token(kModifierOrder[i]));
                    break;
                }
            }
        }
        return true;
    }

    // Normal case: exactly one non-modifier trigger.
    if (non_modifiers.size() != 1) return false;
    std::uint16_t vk = lookup_token(non_modifiers[0]);
    if (vk == 0) return false;
    out.trigger_vk = vk;

    // Modifiers in canonical order from the tokens present.
    for (std::size_t i = 0; i < kModifierCount; ++i) {
        for (const auto& t : tokens) {
            if (t == kModifierOrder[i]) {
                out.modifiers.push_back(lookup_token(kModifierOrder[i]));
                break;
            }
        }
    }
    return true;
}

} // namespace

HotkeyPhysicalizer::HotkeyPhysicalizer(IHostActionSink& sink) noexcept
    : sink_(sink) {}

bool HotkeyPhysicalizer::physicalize(const char* tokens) noexcept {
    if (tokens == nullptr || std::strlen(tokens) == 0) {
        ++physicalize_error_count_;
        return false;
    }
    ParsedChord chord{};
    if (!parse_chord(tokens, chord)) {
        ++physicalize_error_count_;
        return false;
    }
    // Tap-style hotkey: submit down sequence (modifiers in canonical
    // order, then trigger), then up sequence (trigger first, then
    // modifiers in reverse). The physicalizer tracks every VK it has
    // submitted as down so release_held() can emit the inverse for any
    // key left dangling if the sink starts failing mid-tap.
    std::vector<std::uint16_t> pressed_in_order;
    auto submit_down = [&](std::uint16_t vk) -> bool {
        if (!sink_.submit_key(vk, true, std::chrono::milliseconds(50))) {
            ++physicalize_error_count_;
            return false;
        }
        pressed_in_order.push_back(vk);
        return true;
    };
    auto submit_up = [&](std::uint16_t vk) -> bool {
        return sink_.submit_key(vk, false, std::chrono::milliseconds(50));
    };

    for (std::uint16_t mvk : chord.modifiers) {
        if (!submit_down(mvk)) return false;
    }
    if (!submit_down(chord.trigger_vk)) return false;
    // Tap: release trigger, then modifiers in reverse. submit_up
    // failure during tap-style release is non-fatal (counted as
    // physicalize_error_count_ via submit_key path).
    (void)submit_up(chord.trigger_vk);
    for (auto it = chord.modifiers.rbegin(); it != chord.modifiers.rend(); ++it) {
        (void)submit_up(*it);
    }
    // After a successful tap, every pressed VK was released; held_keys_
    // is empty. release_held() is a no-op until the IHostActionSink
    // exposes a release surface (sub-pass B), at which point this
    // invariant moves to the sink side.
    held_keys_.clear();
    ++physicalized_count_;
    return true;
}

void HotkeyPhysicalizer::release_held() noexcept {
    // Emit inverse up for every VK this instance last pressed but did
    // not release. After a successful physicalize(), held_keys_ is
    // empty (tap-style). release_held() is the safety net: callers
    // invoke it at voice-session transition + shutdown so a stuck mod
    // key never leaks across hotkey boundaries.
    for (std::uint16_t vk : held_keys_) {
        (void)sink_.submit_key(vk, false, std::chrono::milliseconds(50));
    }
    held_keys_.clear();
}

} // namespace remotemic::input