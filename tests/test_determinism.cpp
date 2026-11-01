#include <doctest.h>

#include <filesystem>
#include <sstream>
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

// Serialize clusters into a canonical string so two runs can be compared
// byte-for-byte (NFR-2). Mirrors the original plan's "byte-compare the
// pipeline outputs" now that the report files are gone and the pipeline is
// dn::match().
std::string serialize(const std::vector<Cluster>& clusters) {
    std::ostringstream os;
    for (const auto& c : clusters) {
        os << "cluster anchor=" << c.anchor << " members=[";
        for (std::size_t i = 0; i < c.members.size(); ++i) {
            if (i) os << ",";
            os << c.members[i];
        }
        os << "] ";
    }
    return os.str();
}

// A fixture with enough variety to exercise blocking (exact + Damerau-1 first
// tokens), year handling, junk removal, and clustering.
std::vector<FileEntry> fixture() {
    return {
        entry("The Matrix (1999)"),
        entry("the.matrix.1999.extended.1080p.x264"),
        entry("Inception (2010)"),
        entry("inception.2010.1080p.x264"),
        entry("The Shawshank Redemption (1994)"),
        entry("shawshank.redemption.1994"),
        entry("shawshank.redemption.1994.1080p"),
        entry("The Godfather (1972)"),
        entry("thegodfater.1972"),  // transposed first word
        entry("Jurassic Park (1993)"),
        entry("jurassic.park.1993"),
        entry("The Matrix (1999)"),  // duplicate name, different dir
        entry("Unrelated (1950)"),
        entry("Another Unrelated Film (1961)"),
    };
}

}  // namespace

TEST_CASE("determinism: match pipeline is byte-for-byte reproducible (NFR-2)") {
    const std::vector<FileEntry> entries = fixture();
    const Config cfg;

    // Run the pipeline several times; every run must produce identical output.
    const std::string first = serialize(match(entries, cfg));
    REQUIRE_FALSE(first.empty());  // the fixture does produce clusters
    for (int i = 0; i < 5; ++i) {
        CHECK(serialize(match(entries, cfg)) == first);
    }
}

TEST_CASE("determinism: merge_close config is also reproducible") {
    const std::vector<FileEntry> entries = fixture();
    Config cfg;
    cfg.merge_close = true;

    const std::string first = serialize(match(entries, cfg));
    for (int i = 0; i < 5; ++i) {
        CHECK(serialize(match(entries, cfg)) == first);
    }
}
