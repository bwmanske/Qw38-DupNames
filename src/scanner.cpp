#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "dn/scanner.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <utility>

namespace dn {

namespace fs = std::filesystem;

namespace {

bool icase_eq(char a, char b) {
    return std::toupper(static_cast<unsigned char>(a)) ==
           std::toupper(static_cast<unsigned char>(b));
}

// Recursive glob match for '*' and '?', case-insensitive.
bool glob_match_impl(const char* pat, const char* text) {
    while (*pat) {
        if (*pat == '*') {
            while (*pat == '*') ++pat;
            if (*pat == '\0') return true;
            do {
                if (glob_match_impl(pat, text)) return true;
            } while (*text && ++text);
            return glob_match_impl(pat, text);
        }
        if (*text == '\0') return false;
        if (*pat == '?') {
            ++pat;
            ++text;
        } else if (icase_eq(*pat, *text)) {
            ++pat;
            ++text;
        } else {
            return false;
        }
    }
    return *text == '\0';
}

// True if the file/dir carries the Windows hidden or system attribute.
bool has_win_hidden_attr(const fs::path& p) {
    const DWORD attrs = GetFileAttributesW(p.wstring().c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

// Convert a FILETIME (100-ns ticks since 1601-01-01) to Unix epoch seconds.
std::int64_t filetime_to_epoch(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    constexpr std::int64_t kWindowsToUnixTicks = 11824452000000000LL;
    return (static_cast<std::int64_t>(uli.QuadPart) - kWindowsToUnixTicks) / 10000000LL;
}

std::int64_t last_modified_epoch(const fs::path& p) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(p.wstring().c_str(), GetFileExInfoStandard, &data))
        return 0;
    return filetime_to_epoch(data.ftLastWriteTime);
}

void consider_entry(const fs::directory_entry& de, std::size_t dirIndex, bool protected_,
                    const Config& cfg, std::vector<FileEntry>& out, const ProgressFn& progress) {
    std::error_code ec;
    const auto st = de.status(ec);
    if (ec || !fs::is_regular_file(st)) return;

    const fs::path path = de.path();
    if (cfg.skip_hidden && has_hidden_component(path)) return;

    const std::string name = path.filename().string();
    if (!glob_match(cfg.include, name)) return;
    if (!cfg.exclude.empty() && glob_match(cfg.exclude, name)) return;

    FileEntry fe;
    fe.path = path;
    fe.name = name;
    fe.dirIndex = dirIndex;
    fe.protected_ = protected_;
    fe.size = de.file_size(ec);
    if (ec) fe.size = 0;
    fe.lastModified = last_modified_epoch(path);
    out.push_back(std::move(fe));
    if (progress) progress(out.size(), path);
}

}  // namespace

bool glob_match(const std::string& pattern, const std::string& text) {
    if (pattern.empty()) return true;
    return glob_match_impl(pattern.c_str(), text.c_str());
}

bool has_hidden_component(const fs::path& p) {
    for (const auto& comp : p) {
        const auto s = comp.string();
        if (!s.empty() && s.front() == '.') return true;
    }
    return has_win_hidden_attr(p);
}

std::vector<FileEntry> scan(const std::vector<DirEntry>& dirs, const Config& cfg,
                            const ProgressFn& progress) {
    std::vector<FileEntry> out;
    const fs::directory_options opts = fs::directory_options::skip_permission_denied;
    for (std::size_t di = 0; di < dirs.size(); ++di) {
        const auto& dir = dirs[di];
        std::error_code ec;
        if (ec || !fs::is_directory(dir.path, ec)) continue;

        if (cfg.recursive) {
            fs::recursive_directory_iterator it(dir.path, opts, ec);
            if (ec) continue;
            const fs::recursive_directory_iterator end;
            for (; it != end; it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                consider_entry(*it, di, dir.protected_, cfg, out, progress);
            }
        } else {
            fs::directory_iterator it(dir.path, opts, ec);
            if (ec) continue;
            const fs::directory_iterator end;
            for (; it != end; it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                consider_entry(*it, di, dir.protected_, cfg, out, progress);
            }
        }
    }
    return out;
}

}  // namespace dn
