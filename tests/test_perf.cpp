#include <doctest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "dn/match.hpp"
#include "dn/types.hpp"

using namespace dn;

namespace {

FileEntry entry(const std::string& name, std::size_t dirIndex = 0) {
    FileEntry e;
    e.name = name;
    e.path = std::filesystem::path("dir") / name;
    e.dirIndex = dirIndex;
    return e;
}

// Generate `count` synthetic file names. The first token cycles through 1000
// distinct values (so each blocking bucket holds ~count/1000 files) and the
// rest of the name varies. This exercises normalize + blocking + scoring at
// the NFR-1 scale (10,000 files across 10 directories).
std::vector<FileEntry> make_names(std::size_t count) {
    std::vector<FileEntry> entries;
    entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::string first = "tok" + std::to_string(i % 1000);
        const std::string name = first + ".part" + std::to_string(i / 1000) +
                                 "." + std::to_string(1900 + static_cast<int>(i % 100)) +
                                 ".1080p.x264";
        entries.push_back(entry(name, i % 10));  // spread across 10 directories
    }
    return entries;
}

}  // namespace

TEST_CASE("perf: 10,000 synthetic names complete in under 60 s (NFR-1)") {
    const std::vector<FileEntry> entries = make_names(10000);
    const Config cfg;

    const auto start = std::chrono::steady_clock::now();
    const auto clusters = match(entries, cfg);
    const auto end = std::chrono::steady_clock::now();

    const double seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    MESSAGE("matched 10000 names in " << seconds << " s (" << clusters.size()
                                      << " clusters)");

#if defined(__SANITIZE_ADDRESS__) || defined(__ASAN__)
    // Under a sanitizer the instrumentation overhead (10x-100x) makes the NFR-1
    // wall-clock bound meaningless; still run the pipeline to exercise it, but
    // only assert it completed (the bound is validated in the normal build).
    MESSAGE("sanitizer build: skipping the 60 s NFR-1 bound");
    CHECK(clusters.size() > 0);
#else
    CHECK(seconds < 60.0);
#endif
}
