#include "dn/deletion.hpp"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace dn {

DeletePlan plan_deletion(const std::vector<FileEntry>& entries,
                         const std::vector<Cluster>& clusters,
                         const std::vector<bool>& selected) {
    DeletePlan plan;
    const std::size_t n = entries.size();
    const auto sel = [&](std::size_t i) { return i < selected.size() && selected[i]; };

    // Candidate set: selected and non-protected.
    std::unordered_set<std::size_t> candidates;
    for (std::size_t i = 0; i < n; ++i)
        if (sel(i) && !entries[i].protected_) candidates.insert(i);

    // Per cluster: a protected member always survives, so it alone satisfies
    // "keep a copy". Only when the cluster has no protected member do we need
    // to keep a non-protected one; if every non-protected member is selected,
    // keep the anchor (or the first member) instead.
    for (const auto& cl : clusters) {
        std::vector<std::size_t> nonprot;
        bool has_protected = false;
        for (std::size_t m : cl.members) {
            if (entries[m].protected_) has_protected = true;
            else nonprot.push_back(m);
        }
        if (nonprot.empty() || has_protected) continue;
        int cand = 0;
        for (std::size_t m : nonprot)
            if (candidates.count(m)) ++cand;
        if (cand < static_cast<int>(nonprot.size())) continue;  // a copy already remains
        std::size_t keeper = nonprot.front();
        for (std::size_t m : nonprot)
            if (m == cl.anchor) { keeper = m; break; }
        candidates.erase(keeper);
        plan.kept.push_back(keeper);
    }

    for (std::size_t i : candidates) plan.to_delete.push_back(i);
    std::sort(plan.to_delete.begin(), plan.to_delete.end());
    std::sort(plan.kept.begin(), plan.kept.end());

    for (std::size_t i = 0; i < n; ++i)
        if (sel(i) && entries[i].protected_) plan.skipped_protected.push_back(i);
    return plan;
}

}  // namespace dn
