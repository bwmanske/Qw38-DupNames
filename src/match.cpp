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

}  // namespace dn
