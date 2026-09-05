#include <doctest.h>

#include <vector>

#include "dn/cluster.hpp"
#include "dn/types.hpp"

using namespace dn;

static PairScore ps(std::size_t a, std::size_t b, MatchClass c) {
    PairScore p;
    p.a = a;
    p.b = b;
    p.class_ = c;
    return p;
}

TEST_CASE("cluster: transitive MATCH pairs form one cluster") {
    const Config cfg;
    const std::vector<PairScore> pairs = {ps(0, 1, MatchClass::Match),
                                          ps(1, 2, MatchClass::Match)};
    const auto clusters = cluster(pairs, 3, cfg);
    REQUIRE(clusters.size() == 1);
    CHECK(clusters[0].members == (std::vector<std::size_t>{0, 1, 2}));
    CHECK(clusters[0].anchor == 0);
}

TEST_CASE("cluster: disjoint MATCH pairs form separate clusters") {
    const Config cfg;
    const std::vector<PairScore> pairs = {ps(0, 1, MatchClass::Match),
                                          ps(2, 3, MatchClass::Match)};
    const auto clusters = cluster(pairs, 4, cfg);
    REQUIRE(clusters.size() == 2);
    CHECK(clusters[0].members == (std::vector<std::size_t>{0, 1}));
    CHECK(clusters[1].members == (std::vector<std::size_t>{2, 3}));
}

TEST_CASE("cluster: CLOSE pairs are not merged by default") {
    const Config cfg;
    const std::vector<PairScore> pairs = {ps(0, 1, MatchClass::Close)};
    const auto clusters = cluster(pairs, 2, cfg);
    CHECK(clusters.empty());
}

TEST_CASE("cluster: CLOSE pairs merge when merge_close is set") {
    Config cfg;
    cfg.merge_close = true;
    const std::vector<PairScore> pairs = {ps(0, 1, MatchClass::Close)};
    const auto clusters = cluster(pairs, 2, cfg);
    REQUIRE(clusters.size() == 1);
    CHECK(clusters[0].members == (std::vector<std::size_t>{0, 1}));
}

TEST_CASE("cluster: NO MATCH pairs are never merged") {
    const Config cfg;
    const std::vector<PairScore> pairs = {ps(0, 1, MatchClass::NoMatch)};
    const auto clusters = cluster(pairs, 2, cfg);
    CHECK(clusters.empty());
}

TEST_CASE("cluster: a lone file is not a cluster") {
    const Config cfg;
    const std::vector<PairScore> pairs;
    const auto clusters = cluster(pairs, 1, cfg);
    CHECK(clusters.empty());
}
