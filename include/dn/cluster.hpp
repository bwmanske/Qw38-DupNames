#pragma once
#include <cstddef>
#include <vector>

#include "dn/types.hpp"

namespace dn {

// Merge MATCH pairs (and CLOSE pairs when cfg.merge_close) into clusters via
// union-find. `n` is the number of files (indices 0..n-1). Returns clusters
// with >= 2 members, sorted by anchor; each cluster's members are sorted and
// its anchor is the smallest member index.
std::vector<Cluster> cluster(const std::vector<PairScore>& pairs, std::size_t n,
                             const Config& cfg);

}  // namespace dn
