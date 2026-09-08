#pragma once
#include <cstddef>
#include <vector>

#include "dn/types.hpp"

namespace dn {

// Run the full matching pipeline on scanned files:
//   normalize each name -> block candidate pairs -> score -> cluster.
// Returns MATCH clusters (and CLOSE when cfg.merge_close is set), each with
// member indices into `entries`. Files that end up in no cluster are omitted.
std::vector<Cluster> match(const std::vector<FileEntry>& entries, const Config& cfg);

}  // namespace dn
