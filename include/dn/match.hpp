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

// Similarity of each member of `cluster` to the cluster's anchor (the group's
// reference file), in the same order as cluster.members. The anchor's own
// score is 1.0. Used by the GUI's Score column.
std::vector<double> member_scores(const std::vector<FileEntry>& entries,
                                  const Cluster& cluster, const Config& cfg);

}  // namespace dn
