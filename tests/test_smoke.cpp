#include <doctest.h>

#include "dn/version.hpp"

TEST_CASE("smoke: version is non-empty") {
    REQUIRE(!dn::version().empty());
}
