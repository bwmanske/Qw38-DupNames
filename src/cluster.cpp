#include "dn/cluster.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace dn {

namespace {

class DSU {
public:
    explicit DSU(std::size_t n) : parent_(n), rank_(n, 0) {
        for (std::size_t i = 0; i < n; ++i) parent_[i] = static_cast<int>(i);
    }
    int find(int x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];  // path halving
            x = parent_[x];
        }
        return x;
    }
    void unite(int a, int b) {
        int ra = find(a);
        int rb = find(b);
        if (ra == rb) return;
        if (rank_[ra] < rank_[rb]) std::swap(ra, rb);
        parent_[rb] = ra;
        if (rank_[ra] == rank_[rb]) ++rank_[ra];
    }

private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

}  // namespace

std::vector<Cluster> cluster(const std::vector<PairScore>& pairs, std::size_t n,
                             const Config& cfg) {
    DSU dsu(n);
    for (const PairScore& p : pairs) {
        const bool merge = (p.class_ == MatchClass::Match) ||
                           (cfg.merge_close && p.class_ == MatchClass::Close);
        if (merge) dsu.unite(static_cast<int>(p.a), static_cast<int>(p.b));
    }

    std::unordered_map<int, std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < n; ++i)
        groups[dsu.find(static_cast<int>(i))].push_back(i);

    std::vector<Cluster> clusters;
    for (auto& kv : groups) {
        std::vector<std::size_t>& members = kv.second;
        if (members.size() < 2) continue;  // a lone file is not a cluster
        std::sort(members.begin(), members.end());
        Cluster c;
        c.anchor = members.front();
        c.members = std::move(members);
        clusters.push_back(std::move(c));
    }
    std::sort(clusters.begin(), clusters.end(),
              [](const Cluster& a, const Cluster& b) { return a.anchor < b.anchor; });
    return clusters;
}

}  // namespace dn
