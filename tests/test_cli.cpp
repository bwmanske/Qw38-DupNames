#include <doctest.h>

#include <string>
#include <vector>

#include "dn/cli.hpp"

using namespace dn;

TEST_CASE("cli: no arguments yields clean defaults") {
    const auto a = parse_cli({});
    CHECK_FALSE(a.error);
    CHECK(a.ini_path.empty());
    CHECK_FALSE(a.has_match);
    CHECK_FALSE(a.has_close);
}

TEST_CASE("cli: --ini sets the INI path") {
    const auto a = parse_cli({L"--ini", L"C:\\custom\\my.ini"});
    CHECK_FALSE(a.error);
    CHECK(a.ini_path == L"C:\\custom\\my.ini");
}

TEST_CASE("cli: --match and --close set thresholds") {
    const auto a = parse_cli({L"--match", L"0.9", L"--close", L"0.7"});
    CHECK_FALSE(a.error);
    CHECK(a.has_match);
    CHECK(a.match == doctest::Approx(0.9));
    CHECK(a.has_close);
    CHECK(a.close == doctest::Approx(0.7));
}

TEST_CASE("cli: all three options together") {
    const auto a = parse_cli({L"--ini", L"x.ini", L"--match", L"0.8", L"--close", L"0.5"});
    CHECK_FALSE(a.error);
    CHECK(a.ini_path == L"x.ini");
    CHECK(a.match == doctest::Approx(0.8));
    CHECK(a.close == doctest::Approx(0.5));
}

TEST_CASE("cli: unknown option is an error") {
    const auto a = parse_cli({L"--bogus", L"1"});
    CHECK(a.error);
    CHECK_FALSE(a.error_msg.empty());
}

TEST_CASE("cli: missing value for --ini is an error") {
    const auto a = parse_cli({L"--ini"});
    CHECK(a.error);
}

TEST_CASE("cli: missing value for --match is an error") {
    const auto a = parse_cli({L"--match"});
    CHECK(a.error);
}

TEST_CASE("cli: non-numeric threshold is an error") {
    CHECK(parse_cli({L"--match", L"abc"}).error);
    CHECK(parse_cli({L"--close", L"0.5xyz"}).error);  // trailing junk rejected
}

TEST_CASE("cli: non-finite threshold is an error") {
    CHECK(parse_cli({L"--match", L"inf"}).error);
    CHECK(parse_cli({L"--close", L"nan"}).error);
}

TEST_CASE("cli: repeated option uses the last value") {
    const auto a = parse_cli({L"--match", L"0.5", L"--match", L"0.9"});
    CHECK_FALSE(a.error);
    CHECK(a.has_match);
    CHECK(a.match == doctest::Approx(0.9));
}
