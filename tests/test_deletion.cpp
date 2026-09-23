#include <doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "dn/deletion.hpp"
#include "dn/types.hpp"

using namespace dn;

namespace {

FileEntry entry(const std::string& name, bool prot = false) {
    FileEntry e;
    e.name = name;
    e.path = std::filesystem::path(prot ? "prot" : "common") / name;
    e.protected_ = prot;
    return e;
}

Cluster cluster_of(std::size_t anchor, std::vector<std::size_t> members) {
    Cluster c;
    c.anchor = anchor;
    c.members = std::move(members);
    return c;
}

}  // namespace

TEST_CASE("deletion: selected common files are deleted") {
    const std::vector<FileEntry> entries = {entry("a.mp4"), entry("a.1080p.mp4"),
                                            entry("b.mp4")};
    const std::vector<Cluster> clusters = {cluster_of(0, {0, 1})};
    const std::vector<bool> selected = {true, false, false};
    const auto plan = plan_deletion(entries, clusters, selected);
    REQUIRE(plan.to_delete.size() == 1);
    CHECK(plan.to_delete[0] == 0);
    CHECK(plan.kept.empty());
    CHECK(plan.skipped_protected.empty());
}

TEST_CASE("deletion: selecting every common copy keeps the anchor") {
    const std::vector<FileEntry> entries = {entry("a.mp4"), entry("a.1080p.mp4"),
                                            entry("a.x264.mp4")};
    const std::vector<Cluster> clusters = {cluster_of(0, {0, 1, 2})};
    const std::vector<bool> selected = {true, true, true};
    const auto plan = plan_deletion(entries, clusters, selected);
    REQUIRE(plan.to_delete.size() == 2);
    CHECK(plan.to_delete[0] == 1);
    CHECK(plan.to_delete[1] == 2);
    REQUIRE(plan.kept.size() == 1);
    CHECK(plan.kept[0] == 0);  // anchor kept
    CHECK(plan.skipped_protected.empty());
}

TEST_CASE("deletion: a protected copy lets all common copies be deleted") {
    const std::vector<FileEntry> entries = {entry("a.mp4", true), entry("a.1080p.mp4")};
    const std::vector<Cluster> clusters = {cluster_of(0, {0, 1})};
    const std::vector<bool> selected = {true, true};
    const auto plan = plan_deletion(entries, clusters, selected);
    REQUIRE(plan.to_delete.size() == 1);
    CHECK(plan.to_delete[0] == 1);  // common copy deleted; protected one remains
    CHECK(plan.kept.empty());
    REQUIRE(plan.skipped_protected.size() == 1);
    CHECK(plan.skipped_protected[0] == 0);
}

TEST_CASE("deletion: all-protected cluster deletes nothing") {
    const std::vector<FileEntry> entries = {entry("a.mp4", true), entry("a.1080p.mp4", true)};
    const std::vector<Cluster> clusters = {cluster_of(0, {0, 1})};
    const std::vector<bool> selected = {true, true};
    const auto plan = plan_deletion(entries, clusters, selected);
    CHECK(plan.to_delete.empty());
    CHECK(plan.kept.empty());
    REQUIRE(plan.skipped_protected.size() == 2);
}

TEST_CASE("deletion: mixed cluster deletes all common, skips protected") {
    const std::vector<FileEntry> entries = {entry("a.mp4", true),  // 0 protected
                                            entry("a.1080p.mp4"),   // 1 common
                                            entry("a.x264.mp4")};   // 2 common
    const std::vector<Cluster> clusters = {cluster_of(0, {0, 1, 2})};
    const std::vector<bool> selected = {true, true, true};
    const auto plan = plan_deletion(entries, clusters, selected);
    REQUIRE(plan.to_delete.size() == 2);
    CHECK(plan.to_delete[0] == 1);
    CHECK(plan.to_delete[1] == 2);
    CHECK(plan.kept.empty());  // protected copy 0 remains
    REQUIRE(plan.skipped_protected.size() == 1);
    CHECK(plan.skipped_protected[0] == 0);
}

TEST_CASE("deletion: lone selected file (no cluster) is deleted") {
    const std::vector<FileEntry> entries = {entry("a.mp4"), entry("b.mp4")};
    const std::vector<Cluster> clusters = {};
    const std::vector<bool> selected = {true, false};
    const auto plan = plan_deletion(entries, clusters, selected);
    REQUIRE(plan.to_delete.size() == 1);
    CHECK(plan.to_delete[0] == 0);
}

TEST_CASE("deletion: empty selection deletes nothing") {
    const std::vector<FileEntry> entries = {entry("a.mp4"), entry("a.1080p.mp4")};
    const std::vector<Cluster> clusters = {cluster_of(0, {0, 1})};
    const std::vector<bool> selected = {false, false};
    const auto plan = plan_deletion(entries, clusters, selected);
    CHECK(plan.to_delete.empty());
    CHECK(plan.kept.empty());
    CHECK(plan.skipped_protected.empty());
}

TEST_CASE("deletion: multiple clusters keep one per all-common cluster") {
    const std::vector<FileEntry> entries = {entry("a.mp4"), entry("a.1080p.mp4"),  // cl 0
                                            entry("b.mp4"), entry("b.1080p.mp4")};  // cl 1
    const std::vector<Cluster> clusters = {cluster_of(0, {0, 1}), cluster_of(2, {2, 3})};
    const std::vector<bool> selected = {true, true, true, true};
    const auto plan = plan_deletion(entries, clusters, selected);
    REQUIRE(plan.to_delete.size() == 2);
    CHECK(plan.to_delete[0] == 1);
    CHECK(plan.to_delete[1] == 3);
    REQUIRE(plan.kept.size() == 2);
    CHECK(plan.kept[0] == 0);
    CHECK(plan.kept[1] == 2);
}
