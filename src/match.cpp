#include "dn/match.hpp"

#include <utility>
#include <vector>

#include "dn/blocking.hpp"
#include "dn/cluster.hpp"
#include "dn/compare.hpp"
#include "dn/normalize.hpp"

namespace dn {

std::vector<Cluster> match(const std::vector<FileEntry>& entries, const Config& cfg) {
    const std::size_t n = entries.size();
    if (n < 2) return {};

    std::vector<NormalizedName> names;
    names.reserve(n);
    for (const auto& e : entries) names.push_back(normalize(e.name, cfg));

    std::vector<PairScore> pairs;
    for (const auto& pr : candidate_pairs(names, cfg)) {
        PairScore ps = score_pair(names[pr.first], names[pr.second], cfg);
        ps.a = pr.first;
        ps.b = pr.second;
        pairs.push_back(std::move(ps));
    }

    return cluster(pairs, n, cfg);
}

std::vector<double> member_scores(const std::vector<FileEntry>& entries,
                                  const Cluster& cluster, const Config& cfg) {
    std::vector<double> out;
    out.reserve(cluster.members.size());
    if (cluster.members.empty() || cluster.anchor >= entries.size()) return out;
    const NormalizedName anchor = normalize(entries[cluster.anchor].name, cfg);
    for (const std::size_t m : cluster.members) {
        if (m == cluster.anchor) {
            out.push_back(1.0);  // the anchor is identical to itself
            continue;
        }
        if (m >= entries.size()) {
            out.push_back(0.0);
            continue;
        }
        out.push_back(score_pair(anchor, normalize(entries[m].name, cfg), cfg).score);
    }
    return out;
}

}  // namespace dn
