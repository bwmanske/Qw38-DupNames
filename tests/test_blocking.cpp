#include <doctest.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "dn/blocking.hpp"
#include "dn/types.hpp"

using namespace dn;

static NormalizedName nm(std::vector<std::string> tokens, int year = -1) {
    NormalizedName n;
    n.tokens = std::move(tokens);
    n.year = year;
    return n;
}

static bool has_pair(const std::vector<std::pair<std::size_t, std::size_t>>& pairs,
                     std::size_t a, std::size_t b) {
    for (const auto& p : pairs)
        if ((p.first == a && p.second == b) || (p.first == b && p.second == a)) return true;
    return false;
}

TEST_CASE("blocking: same first token is a candidate") {
    const Config cfg;
    const std::vector<NormalizedName> names = {nm({"matrix", "1999"}),
                                               nm({"matrix", "reloaded"})};
    const auto pairs = candidate_pairs(names, cfg);
    CHECK(has_pair(pairs, 0, 1));
}

TEST_CASE("blocking: distant first tokens are not candidates") {
    const Config cfg;
    const std::vector<NormalizedName> names = {nm({"matrix"}), nm({"ocean"})};
    const auto pairs = candidate_pairs(names, cfg);
    CHECK_FALSE(has_pair(pairs, 0, 1));
}

TEST_CASE("blocking: transposed first token is a candidate") {
    const Config cfg;
    const std::vector<NormalizedName> names = {nm({"godfather"}), nm({"godfater"})};
    const auto pairs = candidate_pairs(names, cfg);
    CHECK(has_pair(pairs, 0, 1));
}

TEST_CASE("blocking: three same-token files yield all three pairs") {
    const Config cfg;
    const std::vector<NormalizedName> names = {nm({"alpha"}), nm({"alpha"}),
                                               nm({"alpha"})};
    const auto pairs = candidate_pairs(names, cfg);
    CHECK(has_pair(pairs, 0, 1));
    CHECK(has_pair(pairs, 0, 2));
    CHECK(has_pair(pairs, 1, 2));
    CHECK(pairs.size() == 3);
}

TEST_CASE("blocking: distinct tokens yield no pairs") {
    const Config cfg;
    const std::vector<NormalizedName> names = {nm({"alpha"}), nm({"bravo"}),
                                               nm({"charlie"})};
    const auto pairs = candidate_pairs(names, cfg);
    CHECK(pairs.empty());
}

TEST_CASE("blocking: pairs are unique and ordered i<j") {
    const Config cfg;
    const std::vector<NormalizedName> names = {nm({"alpha"}), nm({"alpha"})};
    const auto pairs = candidate_pairs(names, cfg);
    REQUIRE(pairs.size() == 1);
    CHECK(pairs[0].first < pairs[0].second);
}
