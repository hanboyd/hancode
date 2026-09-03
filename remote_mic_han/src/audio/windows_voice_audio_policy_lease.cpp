#include "remotemic/audio/windows_voice_audio_policy_lease.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <optional>
#include <string>

namespace remotemic::audio {
namespace {

constexpr wchar_t kMarkerPath[] = L"Software\\RemoteMic\\RC003\\Recovery\\VoiceAudioPolicy";
constexpr wchar_t kAudioPath[] = L"Software\\Microsoft\\Multimedia\\Audio";
constexpr wchar_t kDuckingName[] = L"UserDuckingPreference";
constexpr DWORD kDoNothing = 3;

// IPolicyConfig is the Windows shell's endpoint-role policy interface. It is
// intentionally isolated in this translation unit; no undocumented COM type
// leaks into the core or binding API.
struct __declspec(uuid("F8679F50-850A-41CF-9C72-430F290290C8")) IPolicyConfig : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

const CLSID CLSID_PolicyConfigClient =
    {0x870af99c,0x171d,0x4f9e,{0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}};

template<class T> void release(T*& value) noexcept {
    if (value) { value->Release(); value = nullptr; }
}

class ComScope {
public:
    ComScope() noexcept {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        usable_ = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
        uninit_ = SUCCEEDED(hr);
    }
    ~ComScope() { if (uninit_) CoUninitialize(); }
    [[nodiscard]] bool usable() const noexcept { return usable_; }
private:
    bool usable_{false};
    bool uninit_{false};
};

bool write_dword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD value) noexcept {
    HKEY key{};
    if (RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const auto rc = RegSetValueExW(key, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

std::optional<DWORD> read_dword(HKEY root, const wchar_t* path, const wchar_t* name) noexcept {
    DWORD value{}, size = sizeof(value), type{};
    if (RegGetValueW(root, path, name, RRF_RT_REG_DWORD, &type, &value, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return value;
}

bool write_string(const wchar_t* name, const std::wstring& value) noexcept {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kMarkerPath, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const auto bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const auto rc = RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()), bytes);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

std::optional<std::wstring> read_string(const wchar_t* name) noexcept {
    DWORD bytes{};
    if (RegGetValueW(HKEY_CURRENT_USER, kMarkerPath, name, RRF_RT_REG_SZ,
                     nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes < sizeof(wchar_t))
        return std::nullopt;
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kMarkerPath, name, RRF_RT_REG_SZ,
                     nullptr, value.data(), &bytes) != ERROR_SUCCESS)
        return std::nullopt;
    value.resize(wcslen(value.c_str()));
    return value;
}

bool marker_exists() noexcept {
    return read_dword(HKEY_CURRENT_USER, kMarkerPath, L"Version").has_value();
}

void delete_marker() noexcept {
    (void)RegDeleteTreeW(HKEY_CURRENT_USER, kMarkerPath);
}

bool delete_ducking_value() noexcept {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAudioPath, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return true;
    const auto rc = RegDeleteValueW(key, kDuckingName);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
}

bool make_enumerator(IMMDeviceEnumerator*& enumerator) noexcept {
    return SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator),
                                      reinterpret_cast<void**>(&enumerator)));
}

std::optional<std::wstring> default_capture_id(ERole role) noexcept {
    IMMDeviceEnumerator* e{}; IMMDevice* d{}; LPWSTR id{};
    if (!make_enumerator(e) || FAILED(e->GetDefaultAudioEndpoint(eCapture, role, &d)) ||
        FAILED(d->GetId(&id))) { if (id) CoTaskMemFree(id); release(d); release(e); return std::nullopt; }
    std::wstring result(id);
    CoTaskMemFree(id); release(d); release(e);
    return result;
}

std::optional<std::wstring> cable_output_id() noexcept {
    IMMDeviceEnumerator* e{}; IMMDeviceCollection* c{};
    if (!make_enumerator(e) || FAILED(e->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &c))) {
        release(c); release(e); return std::nullopt;
    }
    UINT count{}; (void)c->GetCount(&count);
    std::optional<std::wstring> result;
    for (UINT i = 0; i < count && !result; ++i) {
        IMMDevice* d{}; IPropertyStore* store{}; LPWSTR id{}; PROPVARIANT value{};
        PropVariantInit(&value);
        if (SUCCEEDED(c->Item(i, &d)) && SUCCEEDED(d->OpenPropertyStore(STGM_READ, &store)) &&
            SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) &&
            value.vt == VT_LPWSTR && value.pwszVal && SUCCEEDED(d->GetId(&id))) {
            std::wstring name(value.pwszVal), low(name);
            std::transform(low.begin(), low.end(), low.begin(), towlower);
            if (low.find(L"cable output") != std::wstring::npos &&
                low.find(L"16ch") == std::wstring::npos) result = std::wstring(id);
        }
        if (id) CoTaskMemFree(id);
        PropVariantClear(&value); release(store); release(d);
    }
    release(c); release(e);
    return result;
}

bool set_default(const std::wstring& id, ERole role) noexcept {
    IPolicyConfig* policy{};
    if (FAILED(CoCreateInstance(CLSID_PolicyConfigClient, nullptr, CLSCTX_ALL,
                                __uuidof(IPolicyConfig), reinterpret_cast<void**>(&policy))))
        return false;
    const bool ok = SUCCEEDED(policy->SetDefaultEndpoint(id.c_str(), role));
    release(policy);
    return ok;
}

constexpr std::array<ERole,3> roles{eConsole, eMultimedia, eCommunications};
constexpr std::array<const wchar_t*,3> original_names{L"Console",L"Multimedia",L"Communications"};

bool restore_marker_state() noexcept {
    ComScope com;
    if (!com.usable()) return false;
    bool ok = true;
    const auto target = read_string(L"Target");
    for (std::size_t i = 0; i < roles.size(); ++i) {
        const auto original = read_string(original_names[i]);
        const auto current = default_capture_id(roles[i]);
        // Preserve an explicit user change made during the lease.
        if (original && target && current && *current == *target)
            ok = set_default(*original, roles[i]) && ok;
    }
    const auto original_exists = read_dword(HKEY_CURRENT_USER, kMarkerPath, L"DuckingExists");
    const auto original_duck = read_dword(HKEY_CURRENT_USER, kMarkerPath, L"DuckingValue");
    const auto current_duck = read_dword(HKEY_CURRENT_USER, kAudioPath, kDuckingName);
    if (original_exists && current_duck && *current_duck == kDoNothing) {
        if (*original_exists != 0 && original_duck)
            ok = write_dword(HKEY_CURRENT_USER, kAudioPath, kDuckingName, *original_duck) && ok;
        else
            ok = delete_ducking_value() && ok;
    }
    if (ok) delete_marker();
    return ok;
}

} // namespace

WindowsVoiceAudioPolicyLease::~WindowsVoiceAudioPolicyLease() { restore(); }

bool WindowsVoiceAudioPolicyLease::recover_stale() noexcept {
    if (!marker_exists()) return true;
    const bool ok = restore_marker_state();
    if (ok) active_.store(false);
    return ok;
}

bool WindowsVoiceAudioPolicyLease::acquire() noexcept {
    if (active_.load()) return true;
    if (!recover_stale()) return false;
    ComScope com;
    if (!com.usable()) return false;
    const auto target = cable_output_id();
    if (!target) return false;
    std::array<std::wstring,3> originals;
    for (std::size_t i = 0; i < roles.size(); ++i) {
        const auto id = default_capture_id(roles[i]);
        if (!id) return false;
        originals[i] = *id;
    }
    const auto duck = read_dword(HKEY_CURRENT_USER, kAudioPath, kDuckingName);
    if (!write_dword(HKEY_CURRENT_USER, kMarkerPath, L"Version", 1) ||
        !write_dword(HKEY_CURRENT_USER, kMarkerPath, L"DuckingExists", duck ? 1 : 0) ||
        !write_dword(HKEY_CURRENT_USER, kMarkerPath, L"DuckingValue", duck.value_or(0)) ||
        !write_string(L"Target", *target)) { delete_marker(); return false; }
    for (std::size_t i = 0; i < roles.size(); ++i)
        if (!write_string(original_names[i], originals[i])) { delete_marker(); return false; }

    bool ok = write_dword(HKEY_CURRENT_USER, kAudioPath, kDuckingName, kDoNothing);
    for (const auto role : roles) ok = set_default(*target, role) && ok;
    for (const auto role : roles) {
        const auto current = default_capture_id(role);
        ok = current && *current == *target && ok;
    }
    if (!ok) { (void)restore_marker_state(); return false; }
    active_.store(true);
    return true;
}

void WindowsVoiceAudioPolicyLease::restore() noexcept {
    if (!active_.load() && !marker_exists()) return;
    if (restore_marker_state()) active_.store(false);
}

bool WindowsVoiceAudioPolicyLease::active() const noexcept { return active_.load(); }

bool WindowsVoiceAudioPolicyLease::defaults_are_cable_output() const noexcept {
    ComScope com;
    if (!com.usable()) return false;
    const auto target = cable_output_id();
    if (!target) return false;
    for (const auto role : roles) {
        const auto current = default_capture_id(role);
        if (!current || *current != *target) return false;
    }
    return true;
}

} // namespace remotemic::audio

#else

namespace remotemic::audio {
WindowsVoiceAudioPolicyLease::~WindowsVoiceAudioPolicyLease() = default;
bool WindowsVoiceAudioPolicyLease::recover_stale() noexcept { return false; }
bool WindowsVoiceAudioPolicyLease::acquire() noexcept { return false; }
void WindowsVoiceAudioPolicyLease::restore() noexcept { active_.store(false); }
bool WindowsVoiceAudioPolicyLease::active() const noexcept { return false; }
bool WindowsVoiceAudioPolicyLease::defaults_are_cable_output() const noexcept { return false; }
}

#endif
