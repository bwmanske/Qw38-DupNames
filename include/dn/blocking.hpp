#pragma once
#include <cstddef>
#include <utility>
#include <vector>

#include "dn/types.hpp"

namespace dn {

// Generate candidate index pairs (i, j), i < j, that should be scored, using
// blocking to avoid the full O(n^2) comparison (SPEC §7). Two files are
// candidates when their first significant tokens are equal or within Damerau
// distance 1 (catches transposed/misspelled first words). The year is left to
// the scorer (different-year pairs are capped there). Pairs are unique and
// sorted.
std::vector<std::pair<std::size_t, std::size_t>> candidate_pairs(
    const std::vector<NormalizedName>& names, const Config& cfg);

}  // namespace dn
