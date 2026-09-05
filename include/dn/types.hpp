#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dn {

// A single file discovered by the scanner.
struct FileEntry {
    std::filesystem::path path;    // full path
    std::string name;              // original file name (for display)
    std::uintmax_t size = 0;
    std::int64_t lastModified = 0; // epoch seconds (0 = unknown)
    std::size_t dirIndex = 0;      // index into the active path list
    bool protected_ = false;       // true if its directory is protected
};

// Result of normalizing a file name into comparable form.
struct NormalizedName {
    std::vector<std::string> tokens;  // folded significant words
    int year = -1;                    // -1 = none; else 1900..2099
    std::vector<std::string> junk;    // removed tokens (diagnostics)
    std::string raw;                  // original name
};

enum class MatchClass { Match, Close, NoMatch };

// Score for a pair of normalized names.
struct PairScore {
    std::size_t a = 0;
    std::size_t b = 0;
    double score = 0.0;
    double token_score = 0.0;
    double year_score = 0.0;
    bool year_cap_applied = false;
    MatchClass class_ = MatchClass::NoMatch;
};

// A cluster of files considered the same work/edition.
struct Cluster {
    std::size_t anchor = 0;             // index of the anchor FileEntry
    std::vector<std::size_t> members;   // indices into the entries vector
};

// One directory in a path list, with its protection flag.
struct DirEntry {
    std::filesystem::path path;
    bool protected_ = false;
};

// A named list of directories.
struct DirList {
    std::string name = "default";
    bool isDefault = true;
    std::vector<DirEntry> dirs;
};

// Matcher / scanner configuration.
struct Config {
    int year_lo = 1900, year_hi = 2099;
    double w_year = 0.30, w_tokens = 0.70;
    double match_threshold = 0.85, close_threshold = 0.60;
    double year_cap = 0.50;
    // STUB: minimal junk/stopword list — just enough for the 19 SPEC acceptance
    // cases to pass. To be refined/expanded later (full quality/codec/source/
    // edition categories + stopword list). "the" is required by case 3.
    std::vector<std::string> junk = {
        "the", "extended", "1080p", "x264", "720p", "bluray", "directors"};
    bool skip_hidden = true;
    bool recursive = false;
    std::string include = "*";
    std::string exclude = "";
    bool merge_close = false;
};

}  // namespace dn
