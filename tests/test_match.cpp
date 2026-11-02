#include <doctest.h>

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

}  // namespace

TEST_CASE("match: groups same-work names into clusters") {
    const std::vector<FileEntry> entries = {
        entry("The Matrix (1999)"),
        entry("the.matrix.1999.extended.1080p.x264"),
        entry("Inception (2010)"),
        entry("inception.2010.1080p.x264"),
    };
    const auto clusters = match(entries, Config{});
    REQUIRE(clusters.size() == 2);
    for (const auto& c : clusters) CHECK(c.members.size() == 2);
}

TEST_CASE("match: different years are not clustered (year cap)") {
    const std::vector<FileEntry> entries = {
        entry("The Matrix (1999)"),
        entry("the.matrix.reloaded.2003"),
    };
    const auto clusters = match(entries, Config{});
    CHECK(clusters.empty());
}

TEST_CASE("match: a single file yields no cluster") {
    const std::vector<FileEntry> entries = {entry("The Matrix (1999)")};
    const auto clusters = match(entries, Config{});
    CHECK(clusters.empty());
}

TEST_CASE("match: three same-work names form one cluster") {
    const std::vector<FileEntry> entries = {
        entry("The Shawshank Redemption (1994)"),
        entry("shawshank.redemption.1994"),
        entry("shawshank.redemption.1994.1080p"),
    };
    const auto clusters = match(entries, Config{});
    REQUIRE(clusters.size() == 1);
    CHECK(clusters[0].members.size() == 3);
}

TEST_CASE("match: member indices refer back to the right entries") {
    const std::vector<FileEntry> entries = {
        entry("Inception (2010)"),
        entry("Unrelated (1950)"),
        entry("inception.2010.1080p.x264"),
    };
    const auto clusters = match(entries, Config{});
    REQUIRE(clusters.size() == 1);
    const auto& m = clusters[0].members;
    CHECK(m.size() == 2);
    CHECK(m[0] == 0);
    CHECK(m[1] == 2);
}

TEST_CASE("member_scores: one score per member, anchor is 1.0") {
    const std::vector<FileEntry> entries = {
        entry("The Shawshank Redemption (1994)"),
        entry("shawshank.redemption.1994"),
        entry("shawshank.redemption.1994.1080p"),
    };
    const auto clusters = match(entries, Config{});
    REQUIRE(clusters.size() == 1);
    const auto scores = member_scores(entries, clusters[0], Config{});
    REQUIRE(scores.size() == clusters[0].members.size());
    // The anchor (members.front()) scores exactly 1.0 against itself.
    const std::size_t anchorPos = clusters[0].members.front() == 0 ? 0
                              : clusters[0].members.front() == 1 ? 1 : 2;
    CHECK(scores[anchorPos] == 1.0);
    // All three names normalize to the same tokens + year, so every member
    // (including the anchor) scores 1.0 against the anchor.
    for (const double s : scores) CHECK(s == 1.0);
}

TEST_CASE("member_scores: scores are in [0,1] and order-aligned with members") {
    const std::vector<FileEntry> entries = {
        entry("Inception (2010)"),
        entry("inception.2010.1080p.x264"),
    };
    const auto clusters = match(entries, Config{});
    REQUIRE(clusters.size() == 1);
    const auto scores = member_scores(entries, clusters[0], Config{});
    REQUIRE(scores.size() == 2);
    CHECK(scores[0] == 1.0);  // anchor is members.front()
    CHECK(scores[1] >= 0.0);
    CHECK(scores[1] <= 1.0);
}

TEST_CASE("member_scores: empty cluster yields no scores") {
    const std::vector<FileEntry> entries = {entry("The Matrix (1999)")};
    const Cluster empty;
    const auto scores = member_scores(entries, empty, Config{});
    CHECK(scores.empty());
}
