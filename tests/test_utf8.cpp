#include <doctest.h>

#include <string>

#include "dn/utf8.hpp"

using namespace dn;

TEST_CASE("utf8: fold accented latin letters to base") {
    CHECK(fold_string("caf\xc3\xa9") == "cafe");     // e-acute
    CHECK(fold_string("L\xc3\xa9on") == "leon");      // L + e-acute
    CHECK(fold_string("na\xc3\xafve") == "naive");    // i-diaeresis
    CHECK(fold_string("Z\xc3\xbcrich") == "zurich");  // u-diaeresis
    CHECK(fold_string("se\xc3\xb1or") == "senor");    // n-tilde
    CHECK(fold_string("J\xc3\xb6rg") == "jorg");      // o-umlat + casefold
}

TEST_CASE("utf8: ASCII casefolding") {
    CHECK(fold_string("THE") == "the");
    CHECK(fold_string("Matrix") == "matrix");
    CHECK(fold_string("aBc") == "abc");
}

TEST_CASE("utf8: combining marks are dropped") {
    // "e" + U+0301 (combining acute) folds to "e"
    CHECK(fold_string("e\xcc\x81") == "e");
    // "cafe" with a combining acute on the final e folds to "cafe"
    CHECK(fold_string("cafe\xcc\x81") == "cafe");
}

TEST_CASE("utf8: ligatures expand to base letters") {
    CHECK(fold_string("\xc3\x86rger") == "aerger");  // AE -> ae
    CHECK(fold_string("oe") == "oe");                // plain ascii unchanged
}

TEST_CASE("utf8: non-latin codepoints are kept unchanged") {
    const std::string cjk = "\xe7\x94\xb5";  // U+7535
    CHECK(fold_string(cjk) == cjk);
    // Mixed: latin folds, CJK stays
    CHECK(fold_string("a\xe7\x94\xb5") == "a\xe7\x94\xb5");
}

TEST_CASE("utf8: fold_codepoint single codepoint") {
    CHECK(fold_codepoint(0x00E9) == "e");   // e-acute
    CHECK(fold_codepoint(0x0041) == "a");   // 'A'
    CHECK(fold_codepoint(0x0301) == "");    // combining acute dropped
    CHECK(fold_codepoint(0x0061) == "a");   // 'a' unchanged
}
