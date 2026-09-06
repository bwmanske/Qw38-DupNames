#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <doctest.h>

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "dn/scanner.hpp"
#include "dn/types.hpp"

using namespace dn;
namespace fs = std::filesystem;

namespace {

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

// Standard fixture tree under `root`:
//   root/a.mkv  root/b.avi  root/.hidden.mkv  root/sub/c.mkv
void make_tree(const fs::path& root) {
    fs::create_directories(root / "sub");
    make_file(root / "a.mkv");
    make_file(root / "b.avi");
    make_file(root / ".hidden.mkv");
    make_file(root / "sub" / "c.mkv");
}

std::vector<std::string> names(const std::vector<FileEntry>& entries) {
    std::vector<std::string> out;
    out.reserve(entries.size());
    for (const auto& e : entries) out.push_back(e.name);
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

// --- glob_match -----------------------------------------------------------

TEST_CASE("glob_match: literal and empty pattern") {
    CHECK(glob_match("", "anything"));
    CHECK(glob_match("movie.mkv", "movie.mkv"));
    CHECK(!glob_match("movie.mkv", "movie.avi"));
    CHECK(glob_match("movie.mkv", "MOVIE.MKV"));  // case-insensitive
}

TEST_CASE("glob_match: star wildcard") {
    CHECK(glob_match("*", "anything"));
    CHECK(glob_match("*.mkv", "movie.mkv"));
    CHECK(!glob_match("*.mkv", "movie.avi"));
    CHECK(!glob_match("*.mkv", "mkv"));  // no dot present
    CHECK(glob_match("movie.*", "movie.mkv"));
    CHECK(glob_match("*matrix*", "The.Matrix.1999"));
}

TEST_CASE("glob_match: question mark wildcard") {
    CHECK(glob_match("movie?.mkv", "movie1.mkv"));
    CHECK(!glob_match("movie?.mkv", "movie12.mkv"));
    CHECK(glob_match("a?c", "abc"));
    CHECK(!glob_match("a?c", "ac"));
}

// --- has_hidden_component -------------------------------------------------

TEST_CASE("has_hidden_component: dot-prefixed component") {
    CHECK(has_hidden_component(fs::path("C:/Users/.hidden/file.txt")));
    CHECK(has_hidden_component(fs::path(".hidden")));
    CHECK(!has_hidden_component(fs::path("C:/Users/visible/file.txt")));
}

TEST_CASE("has_hidden_component: windows hidden attribute") {
    TempDir td("dn_hidden_attr");
    const auto f = td.path / "visible.txt";
    make_file(f);
    CHECK_FALSE(has_hidden_component(f));
    CHECK(SetFileAttributesW(f.wstring().c_str(), FILE_ATTRIBUTE_HIDDEN) != 0);
    CHECK(has_hidden_component(f));
    SetFileAttributesW(f.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
}

// --- scan -----------------------------------------------------------------

TEST_CASE("scan: non-recursive lists files, skips hidden") {
    TempDir td("dn_scan_nonrec");
    make_tree(td.path);
    const auto entries = scan({DirEntry{td.path, false}}, Config{});
    CHECK(names(entries) == (std::vector<std::string>{"a.mkv", "b.avi"}));
}

TEST_CASE("scan: recursive descends into subdirs") {
    TempDir td("dn_scan_rec");
    make_tree(td.path);
    Config cfg;
    cfg.recursive = true;
    const auto entries = scan({DirEntry{td.path, false}}, cfg);
    CHECK(names(entries) == (std::vector<std::string>{"a.mkv", "b.avi", "c.mkv"}));
}

TEST_CASE("scan: include glob filters by name") {
    TempDir td("dn_scan_include");
    make_tree(td.path);
    Config cfg;
    cfg.recursive = true;
    cfg.include = "*.mkv";
    const auto entries = scan({DirEntry{td.path, false}}, cfg);
    CHECK(names(entries) == (std::vector<std::string>{"a.mkv", "c.mkv"}));
}

TEST_CASE("scan: exclude glob removes matches") {
    TempDir td("dn_scan_exclude");
    make_tree(td.path);
    Config cfg;
    cfg.exclude = "*.avi";
    const auto entries = scan({DirEntry{td.path, false}}, cfg);
    CHECK(names(entries) == (std::vector<std::string>{"a.mkv"}));
}

TEST_CASE("scan: skip_hidden=false includes dot files") {
    TempDir td("dn_scan_nohidden");
    make_tree(td.path);
    Config cfg;
    cfg.skip_hidden = false;
    const auto entries = scan({DirEntry{td.path, false}}, cfg);
    CHECK(names(entries) == (std::vector<std::string>{".hidden.mkv", "a.mkv", "b.avi"}));
}

TEST_CASE("scan: records size, dirIndex, and protection flag") {
    TempDir td("dn_scan_meta");
    make_tree(td.path);
    const auto entries = scan({DirEntry{td.path, true}}, Config{});
    CHECK(entries.size() == 2);
    for (const auto& e : entries) {
        CHECK(e.dirIndex == 0);
        CHECK(e.protected_ == true);
        CHECK(e.size == 1);
        CHECK(e.lastModified > 0);
    }
}

TEST_CASE("scan: multiple dirs get distinct dirIndex and flags") {
    TempDir a("dn_scan_dirA");
    TempDir b("dn_scan_dirB");
    make_tree(a.path);
    make_tree(b.path);
    const std::vector<DirEntry> dirs{{a.path, false}, {b.path, true}};
    const auto entries = scan(dirs, Config{});
    int countA = 0, countB = 0;
    for (const auto& e : entries) {
        if (e.dirIndex == 0) {
            ++countA;
            CHECK(e.protected_ == false);
        } else if (e.dirIndex == 1) {
            ++countB;
            CHECK(e.protected_ == true);
        } else {
            FAIL("unexpected dirIndex");
        }
    }
    CHECK(countA == 2);
    CHECK(countB == 2);
}

TEST_CASE("scan: empty directory yields no files") {
    TempDir td("dn_scan_empty");  // created empty, no files
    const auto entries = scan({DirEntry{td.path, false}}, Config{});
    CHECK(entries.empty());
}

TEST_CASE("scan: missing directory is skipped") {
    TempDir td("dn_scan_missing");
    const auto missing = td.path / "does_not_exist";
    const auto entries = scan({DirEntry{missing, false}}, Config{});
    CHECK(entries.empty());
}

TEST_CASE("scan: progress callback fires once per file") {
    TempDir td("dn_scan_progress");
    make_tree(td.path);
    int calls = 0;
    std::size_t lastCount = 0;
    const auto progress = [&](std::size_t count, const fs::path&) {
        ++calls;
        lastCount = count;
    };
    const auto entries = scan({DirEntry{td.path, false}}, Config{}, progress);
    CHECK(calls == static_cast<int>(entries.size()));
    CHECK(lastCount == entries.size());
}
