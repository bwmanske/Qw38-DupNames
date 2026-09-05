#include <doctest.h>

#include <string>
#include <vector>

#include "dn/normalize.hpp"
#include "dn/types.hpp"

using namespace dn;

static const Config& cfg() {
    static Config c;  // default config (includes the minimal stub junk list)
    return c;
}

TEST_CASE("normalize: spaced curated name") {
    const auto nm = normalize("The Matrix (1999)", cfg());
    CHECK(nm.year == 1999);
    CHECK(nm.tokens == (std::vector<std::string>{"matrix"}));  // "the" is junk
    CHECK(nm.junk == (std::vector<std::string>{"the"}));
    CHECK(nm.raw == "The Matrix (1999)");
}

TEST_CASE("normalize: dot-separated uncurated name with junk") {
    const auto nm = normalize("the.matrix.1999.extended.1080p.x264", cfg());
    CHECK(nm.year == 1999);
    CHECK(nm.tokens == (std::vector<std::string>{"matrix"}));
    // "the", "extended", "1080p" are junk; "x264" is stripped as the extension
    CHECK(nm.junk.size() == 3);
}

TEST_CASE("normalize: year glued to letters is split") {
    const auto nm = normalize("1984remastered", cfg());
    CHECK(nm.year == 1984);
    CHECK(nm.tokens == (std::vector<std::string>{"remastered"}));
}

TEST_CASE("normalize: letters glued to a year are split") {
    const auto nm = normalize("matrix1999", cfg());
    CHECK(nm.year == 1999);
    CHECK(nm.tokens == (std::vector<std::string>{"matrix"}));
}

TEST_CASE("normalize: a 5-digit run is not a year") {
    const auto nm = normalize("movie19991", cfg());
    CHECK(nm.year == -1);
    CHECK(nm.tokens == (std::vector<std::string>{"movie19991"}));
}

TEST_CASE("normalize: out-of-range 4-digit number is not a year") {
    const auto nm = normalize("movie1080", cfg());
    CHECK(nm.year == -1);
    CHECK(nm.tokens == (std::vector<std::string>{"movie1080"}));
}

TEST_CASE("normalize: first year wins, later years stay tokens") {
    const auto nm = normalize("film.1999.2003", cfg());
    CHECK(nm.year == 1999);
    CHECK(nm.tokens == (std::vector<std::string>{"film", "2003"}));
}

TEST_CASE("normalize: diacritics and case folded") {
    const auto nm = normalize("Am\xc3\xa9lie (2001)", cfg());
    CHECK(nm.year == 2001);
    CHECK(nm.tokens == (std::vector<std::string>{"amelie"}));
}

TEST_CASE("normalize: no year detected") {
    const auto nm = normalize("Some Movie", cfg());
    CHECK(nm.year == -1);
    CHECK(nm.tokens == (std::vector<std::string>{"some", "movie"}));
}

TEST_CASE("normalize: extension that is a year is preserved") {
    // "The.Matrix.1999" has no real extension; the trailing 1999 is the year.
    const auto nm = normalize("The.Matrix.1999", cfg());
    CHECK(nm.year == 1999);
    CHECK(nm.tokens == (std::vector<std::string>{"matrix"}));
}
