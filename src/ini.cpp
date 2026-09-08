#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "dn/ini.hpp"

#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <system_error>

namespace dn {

namespace fs = std::filesystem;

namespace {

constexpr DWORD kBufChars = 32768;  // Win32 INI max value length is 32K.

// Create the parent directory of `file` if it does not already exist.
void ensure_parent(const std::wstring& file) {
    const fs::path parent = fs::path(file).parent_path();
    if (parent.empty()) return;
    std::error_code ec;
    fs::create_directories(parent, ec);  // best effort; a missing dir is reported by the write
}

// The numeric suffix of `key` if it is `prefix` followed by one or more digits,
// otherwise -1.
long key_number(const std::wstring& key, const std::wstring& prefix) {
    if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) return -1;
    const std::wstring num = key.substr(prefix.size());
    for (const wchar_t c : num)
        if (c < L'0' || c > L'9') return -1;
    return std::wcstol(num.c_str(), nullptr, 10);
}

}  // namespace

std::wstring ini_get_string(const std::wstring& file, const std::wstring& section,
                            const std::wstring& key, const std::wstring& def) {
    wchar_t buf[kBufChars] = {};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), def.c_str(), buf, kBufChars,
                             file.c_str());
    return buf;
}

void ini_set_string(const std::wstring& file, const std::wstring& section,
                    const std::wstring& key, const std::wstring& value) {
    ensure_parent(file);
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), file.c_str());
}

double ini_get_double(const std::wstring& file, const std::wstring& section,
                      const std::wstring& key, double def) {
    const std::wstring s = ini_get_string(file, section, key, L"");
    if (s.empty()) return def;
    try {
        std::size_t pos = 0;
        return std::stod(s, &pos);
    } catch (...) {
        return def;
    }
}

void ini_set_double(const std::wstring& file, const std::wstring& section,
                    const std::wstring& key, double value) {
    wchar_t buf[64];
    swprintf(buf, 64, L"%.10g", value);
    ini_set_string(file, section, key, buf);
}

std::vector<std::wstring> ini_section_keys(const std::wstring& file,
                                           const std::wstring& section) {
    std::vector<std::wstring> keys;
    wchar_t buf[kBufChars] = {};
    GetPrivateProfileSectionW(section.c_str(), buf, kBufChars, file.c_str());
    // buf is "key1=val1\0key2=val2\0...\0\0": null-separated key=value pairs.
    // Keep only the key (the part before '=').
    for (wchar_t* p = buf; *p != L'\0'; p += std::wcslen(p) + 1) {
        const wchar_t* eq = std::wcschr(p, L'=');
        if (eq)
            keys.emplace_back(p, static_cast<std::size_t>(eq - p));
        else
            keys.emplace_back(p);
    }
    return keys;
}

std::wstring default_ini_path() {
    wchar_t appdata[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH) == 0)
        return L"DupNames.ini";  // no roaming dir; fall back to the CWD
    return std::wstring(appdata) + L"\\DupNames\\DupNames.ini";
}

void ini_load_state(const std::wstring& file, Config& cfg) {
    const std::wstring m = ini_get_string(file, L"InitState", L"MatchThreshold", L"");
    if (!m.empty()) cfg.match_threshold = ini_get_double(file, L"InitState", L"MatchThreshold", cfg.match_threshold);
    const std::wstring c = ini_get_string(file, L"InitState", L"CloseThreshold", L"");
    if (!c.empty()) cfg.close_threshold = ini_get_double(file, L"InitState", L"CloseThreshold", cfg.close_threshold);
}

void ini_save_state(const std::wstring& file, const Config& cfg) {
    ini_set_double(file, L"InitState", L"MatchThreshold", cfg.match_threshold);
    ini_set_double(file, L"InitState", L"CloseThreshold", cfg.close_threshold);
}

Config resolve_config(const std::wstring& ini_path, const CliArgs& cli) {
    Config cfg;  // built-in defaults
    ini_load_state(ini_path, cfg);  // <- INI (only present keys override)
    if (cli.has_match) {
        cfg.match_threshold = cli.match;  // <- CLI wins
        ini_set_double(ini_path, L"InitState", L"MatchThreshold", cli.match);  // write-back
    }
    if (cli.has_close) {
        cfg.close_threshold = cli.close;
        ini_set_double(ini_path, L"InitState", L"CloseThreshold", cli.close);
    }
    return cfg;
}

std::vector<DirEntry> ini_load_paths(const std::wstring& file) {
    struct Item {
        long n;
        std::wstring path;
        bool prot;
    };
    std::vector<Item> prot, comm;
    for (const auto& key : ini_section_keys(file, L"PathList")) {
        const long pn = key_number(key, L"ProtectedPath");
        const long cn = key_number(key, L"CommonPath");
        if (pn < 0 && cn < 0) continue;
        const std::wstring v = ini_get_string(file, L"PathList", key, L"");
        if (v.empty()) continue;
        if (pn >= 0)
            prot.push_back({pn, v, true});
        else
            comm.push_back({cn, v, false});
    }
    std::sort(prot.begin(), prot.end(), [](const Item& a, const Item& b) { return a.n < b.n; });
    std::sort(comm.begin(), comm.end(), [](const Item& a, const Item& b) { return a.n < b.n; });

    std::vector<DirEntry> out;
    out.reserve(prot.size() + comm.size());
    for (const auto& it : prot) out.push_back(DirEntry{fs::path(it.path), true});
    for (const auto& it : comm) out.push_back(DirEntry{fs::path(it.path), false});
    return out;
}

void ini_save_paths(const std::wstring& file, const std::vector<DirEntry>& dirs) {
    // Drop any stale keys so the section exactly reflects `dirs`.
    for (const auto& key : ini_section_keys(file, L"PathList"))
        WritePrivateProfileStringW(L"PathList", key.c_str(), nullptr, file.c_str());

    int pn = 1, cn = 1;
    for (const auto& d : dirs)
        if (d.protected_)
            ini_set_string(file, L"PathList", L"ProtectedPath" + std::to_wstring(pn++),
                           d.path.wstring());
    for (const auto& d : dirs)
        if (!d.protected_)
            ini_set_string(file, L"PathList", L"CommonPath" + std::to_wstring(cn++),
                           d.path.wstring());
}

}  // namespace dn
