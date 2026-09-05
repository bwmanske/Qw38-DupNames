#include <doctest.h>

#include <string>
#include <vector>

#include "dn/compare.hpp"
#include "dn/types.hpp"

using namespace dn;

TEST_CASE("compare: damerau-levenshtein distance") {
    CHECK(damerau_levenshtein("", "") == 0);
    CHECK(damerau_levenshtein("abc", "abc") == 0);
    CHECK(damerau_levenshtein("a", "b") == 1);
    CHECK(damerau_levenshtein("abc", "abcd") == 1);       // insertion
    CHECK(damerau_levenshtein("ab", "ba") == 1);          // transposition costs 1
    CHECK(damerau_levenshtein("godfather", "godfater") == 1);  // one deletion
    CHECK(damerau_levenshtein("kitten", "sitting") == 3);
}

TEST_CASE("compare: token similarity") {
    CHECK(token_similarity("matrix", "matrix") == doctest::Approx(1.0));
    CHECK(token_similarity("godfather", "godfater") == doctest::Approx(1.0 - 1.0 / 9.0));
    CHECK(token_similarity("ab", "ba") == doctest::Approx(0.5));
    CHECK(token_similarity("", "") == doctest::Approx(1.0));
}

TEST_CASE("compare: year score") {
    CHECK(year_score(1999, 1999) == doctest::Approx(1.0));
    CHECK(year_score(1999, 2003) == doctest::Approx(0.0));
    CHECK(year_score(1999, -1) == doctest::Approx(0.5));
    CHECK(year_score(-1, 1999) == doctest::Approx(0.5));
    CHECK(year_score(-1, -1) == doctest::Approx(0.5));
}

static NormalizedName nm(std::vector<std::string> tokens, int year) {
    NormalizedName n;
    n.tokens = std::move(tokens);
    n.year = year;
    return n;
}

TEST_CASE("compare: identical names score 1.0 and MATCH") {
    const Config cfg;
    const auto a = nm({"matrix"}, 1999);
    const auto b = nm({"matrix"}, 1999);
    const auto ps = score_pair(a, b, cfg);
    CHECK(ps.score == doctest::Approx(1.0));
    CHECK(ps.class_ == MatchClass::Match);
    CHECK_FALSE(ps.year_cap_applied);
}

TEST_CASE("compare: differing years are capped at 0.50") {
    const Config cfg;
    const auto a = nm({"matrix"}, 1999);
    const auto b = nm({"matrix"}, 2003);
    const auto ps = score_pair(a, b, cfg);
    CHECK(ps.year_cap_applied);
    CHECK(ps.score == doctest::Approx(0.50));
    CHECK(ps.class_ == MatchClass::NoMatch);
}

TEST_CASE("compare: a missing significant word yields CLOSE") {
    const Config cfg;
    // "the" is junk, so A reduces to {shawshank, redemption}; B is missing one.
    const auto a = nm({"shawshank", "redemption"}, 1994);
    const auto b = nm({"shawshank"}, 1994);
    const auto ps = score_pair(a, b, cfg);
    CHECK(ps.token_score == doctest::Approx(0.5));
    CHECK(ps.score == doctest::Approx(0.30 + 0.70 * 0.5));
    CHECK(ps.class_ == MatchClass::Close);
}

TEST_CASE("compare: classification thresholds") {
    Config cfg;
    cfg.match_threshold = 0.85;
    cfg.close_threshold = 0.60;
    // token_score 1.0, no year -> 0.30*0.5 + 0.70*1.0 = 0.85 -> MATCH
    const auto a = nm({"abc"}, -1);
    const auto b = nm({"abc"}, -1);
    CHECK(score_pair(a, b, cfg).class_ == MatchClass::Match);
}
