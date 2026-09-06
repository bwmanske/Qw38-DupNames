#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "dn/types.hpp"

namespace dn {

// Progress callback invoked after each file is recorded: (running file count,
// path just scanned). May be empty.
using ProgressFn = std::function<void(std::size_t, const std::filesystem::path&)>;

// Scan the directories in `dirs` and return every matching file as a FileEntry.
// Honors cfg.recursive, cfg.include, cfg.exclude, and cfg.skip_hidden. Each
// FileEntry's dirIndex is the index of its source directory in `dirs` and
// protected_ mirrors that directory's protection flag.
// Read-only: never creates, modifies, or deletes anything (NFR-5).
std::vector<FileEntry> scan(const std::vector<DirEntry>& dirs, const Config& cfg,
                            const ProgressFn& progress = {});

// True if any path component is dot-prefixed, or (on Windows) the file carries
// the hidden or system attribute.
bool has_hidden_component(const std::filesystem::path& p);

// Case-insensitive glob match supporting '*' (any run, incl. empty) and '?'
// (exactly one char). An empty pattern matches everything.
bool glob_match(const std::string& pattern, const std::string& text);

}  // namespace dn
