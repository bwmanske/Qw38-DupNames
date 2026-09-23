#pragma once
#include <cstddef>
#include <vector>

#include "dn/types.hpp"

namespace dn {

// Result of planning a deletion from a user selection.
struct DeletePlan {
    std::vector<std::size_t> to_delete;         // indices into entries to remove
    std::vector<std::size_t> kept;              // one kept per cluster whose non-protected members were all selected
    std::vector<std::size_t> skipped_protected; // selected but in a protected dir (never deleted)
};

// Compute the safe deletion set from a user selection. `selected[i]` is true
// when the user marked entry i for deletion. Safety rules:
//   - a protected entry is never deleted (reported in `skipped_protected`);
//   - a copy of each cluster always survives: a protected member already
//     guarantees that, so all selected common members may be deleted; only
//     when a cluster has no protected member is a non-protected one kept (the
//     anchor, or the first member) if the user selected every common member.
// Entries not belonging to any cluster are deleted when selected and
// non-protected (no keeper rule applies to lone files).
DeletePlan plan_deletion(const std::vector<FileEntry>& entries,
                         const std::vector<Cluster>& clusters,
                         const std::vector<bool>& selected);

}  // namespace dn
