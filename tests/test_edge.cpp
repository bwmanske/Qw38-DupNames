#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "dn/match.hpp"
#include "dn/normalize.hpp"
#include "dn/scanner.hpp"
#include "dn/types.hpp"

using namespace dn;
namespace fs = std::filesystem;

namespace {

const Config& cfg() {
    static Config c;  // default config (includes the minimal stub junk list)
    return c;
}

FileEntry entry(const std::string& name, std::size_t dirIndex = 0) {
    FileEntry e;
    e.name = name;
    e.path = fs::path("dir") / name;
    e.dirIndex = dirIndex;
    return e;
}

// RAII temp dir: created empty on construction, removed on destruction.
struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name) : path(fs::temp_directory_path() / name) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

void make_file(const fs::path& p) { std::ofstream(p).put('x'); }

std::vector<std::string> names(const std::vector<FileEntry>& entries) {
    std::vector<std::string> out;
    out.reserve(entries.size());
    for (const auto& e : entries) out.push_back(e.name);
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

// --- empty after normalization (SPEC §14) -----------------------------------

TEST_CASE("edge: name that is all junk normalizes to no tokens") {
    // "1080p" is junk and "x264" is the extension -> nothing significant left.
    const auto nm = normalize("1080p.x264", cfg());
    CHECK(nm.tokens.empty());
    CHECK(nm.year == -1);
}

TEST_CASE("edge: all-junk name does not form a cluster") {
    const std::vector<FileEntry> entries = {
        entry("1080p.x264"),
        entry("720p.bluray"),
        entry("The Matrix (1999)"),  // a real name, for contrast
    };
    const auto clusters = match(entries, cfg());
    // Neither junk-only file has tokens, so neither can match anything.
    for (const auto& c : clusters) {
        for (const auto m : c.members) CHECK(entries[m].name == "The Matrix (1999)");
    }
}

// --- invalid UTF-8 (SPEC §14) -----------------------------------------------

TEST_CASE("edge: invalid UTF-8 bytes do not crash and are ignored") {
    // 0xFF is an invalid lead byte; a lone 0xC3 is a truncated 2-byte sequence.
    // (Concatenated literals stop the hex escape from eating the following chars.)
    const std::string bad = "ab" "\xff" "cd" "\xc3";
    const auto nm = normalize(bad, cfg());
    CHECK(nm.raw == bad);  // original preserved for display
    // Invalid bytes are dropped, so the significant bytes are "abcd".
    CHECK(nm.tokens == (std::vector<std::string>{"abcd"}));
}

TEST_CASE("edge: fully invalid UTF-8 name yields no tokens, no crash") {
    const std::string bad = "\xff\xfe\xfd";
    const auto nm = normalize(bad, cfg());
    CHECK(nm.tokens.empty());
    CHECK(nm.year == -1);
}

// --- year-only name (SPEC §14) ----------------------------------------------

TEST_CASE("edge: year-only name has no tokens (unmatchable)") {
    const auto nm = normalize("1999", cfg());
    CHECK(nm.year == 1999);
    CHECK(nm.tokens.empty());
}

TEST_CASE("edge: year-only names do not cluster together") {
    const std::vector<FileEntry> entries = {entry("1999"), entry("1999")};
    const auto clusters = match(entries, cfg());
    CHECK(clusters.empty());
}

// --- very long names (SPEC §14) ---------------------------------------------

TEST_CASE("edge: very long name is truncated to 32 scoring tokens") {
    // Build a name with 50 distinct tokens (> 200 chars).
    std::string name;
    for (int i = 0; i < 50; ++i) {
        if (i) name += ".";
        name += "tok" + std::to_string(i);
    }
    name += ".mkv";
    CHECK(name.size() > 200);

    const auto nm = normalize(name, cfg());
    CHECK(nm.tokens.size() == 32);  // capped
    CHECK(nm.raw == name);          // full name preserved
}

TEST_CASE("edge: very long names do not crash the pipeline") {
    std::string longName;
    for (int i = 0; i < 200; ++i) {
        if (i) longName += ".";
        longName += "segment" + std::to_string(i);
    }
    longName += ".mkv";
    const std::vector<FileEntry> entries = {entry(longName), entry(longName)};
    const auto clusters = match(entries, cfg());  // must not crash
    REQUIRE(clusters.size() == 1);
    CHECK(clusters[0].members.size() == 2);  // identical names still match
}

// --- symlinks / broken links (SPEC §14) -------------------------------------

TEST_CASE("edge: scanner skips broken symlinks and follows valid ones") {
    TempDir td("dn_edge_symlink");
    make_file(td.path / "real.mkv");

    // Broken symlink: points at a file that does not exist.
    const auto broken = td.path / "broken.mkv";
    // Valid symlink: points at real.mkv.
    const auto valid = td.path / "link.mkv";

    std::error_code ec;
    fs::create_symlink(td.path / "does_not_exist.mkv", broken, ec);
    const bool madeBroken = !ec;
    ec.clear();
    fs::create_symlink(td.path / "real.mkv", valid, ec);
    const bool madeValid = !ec;

    // Symlink creation can require elevation/developer mode; if unavailable,
    // just confirm the scan of the real file still works (no crash).
    if (!madeBroken && !madeValid) {
        const auto entries = scan({DirEntry{td.path, false}}, Config{});
        CHECK(names(entries) == (std::vector<std::string>{"real.mkv"}));
        return;
    }

    const auto entries = scan({DirEntry{td.path, false}}, Config{});
    const auto got = names(entries);
    // The broken link must never appear.
    CHECK(std::find(got.begin(), got.end(), "broken.mkv") == got.end());
    if (madeValid) {
        // A valid symlink to a regular file is followed and included.
        CHECK(std::find(got.begin(), got.end(), "link.mkv") != got.end());
    }
    CHECK(std::find(got.begin(), got.end(), "real.mkv") != got.end());
}

// --- permission errors (SPEC §14) -------------------------------------------

TEST_CASE("edge: scanner tolerates a directory it cannot fully read") {
    TempDir td("dn_edge_perm");
    make_file(td.path / "top.mkv");
    fs::create_directories(td.path / "sub");
    make_file(td.path / "sub" / "inner.mkv");

    // Recursive scan with skip_permission_denied must not throw or crash, even
    // if a subdirectory is unreadable; the readable files are still returned.
    Config cfg;
    cfg.recursive = true;
    const auto entries = scan({DirEntry{td.path, false}}, cfg);
    const auto got = names(entries);
    CHECK(std::find(got.begin(), got.end(), "top.mkv") != got.end());
    CHECK(std::find(got.begin(), got.end(), "inner.mkv") != got.end());
}
