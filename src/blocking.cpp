#include "dn/blocking.hpp"

#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "dn/compare.hpp"

namespace dn {

std::vector<std::pair<std::size_t, std::size_t>> candidate_pairs(
    const std::vector<NormalizedName>& names, const Config& cfg) {
    (void)cfg;  // year-based bucket refinement is a later performance pass
    const std::size_t n = names.size();
    std::vector<std::pair<std::size_t, std::size_t>> out;
    std::set<std::pair<std::size_t, std::size_t>> seen;

    auto add = [&](std::size_t i, std::size_t j) {
        if (i == j) return;
        const std::pair<std::size_t, std::size_t> p =
            (i < j) ? std::make_pair(i, j) : std::make_pair(j, i);
        if (seen.insert(p).second) out.push_back(p);
    };

    auto first_token = [](const NormalizedName& nm) -> std::string {
        return nm.tokens.empty() ? std::string() : nm.tokens.front();
    };

    // Bucket files by first significant token (exact).
    std::map<std::string, std::vector<std::size_t>> buckets;
    for (std::size_t i = 0; i < n; ++i) buckets[first_token(names[i])].push_back(i);

    // 1. Compare every pair within a bucket.
    for (auto& kv : buckets) {
        const std::vector<std::size_t>& v = kv.second;
        for (std::size_t a = 0; a < v.size(); ++a)
            for (std::size_t b = a + 1; b < v.size(); ++b) add(v[a], v[b]);
    }

    // 2. Compare across buckets whose first tokens differ by Damerau distance
    //    <= 1 (transposed/misspelled first words). Only length-compatible token
    //    groups can be within distance 1, so group by length and test each pair
    //    of groups once (gi <= gj).
    std::map<std::size_t, std::vector<std::pair<std::string, std::vector<std::size_t>>>>
        by_len;
    for (auto& kv : buckets)
        by_len[kv.first.size()].push_back({kv.first, std::move(kv.second)});

    std::vector<std::size_t> lengths;
    lengths.reserve(by_len.size());
    for (const auto& kv : by_len) lengths.push_back(kv.first);

    for (std::size_t gi = 0; gi < lengths.size(); ++gi) {
        for (std::size_t gj = gi; gj < lengths.size(); ++gj) {
            const std::size_t li = lengths[gi];
            const std::size_t lj = lengths[gj];
            if (std::abs(static_cast<long>(li) - static_cast<long>(lj)) > 1) continue;
            std::vector<std::pair<std::string, std::vector<std::size_t>>>& vi = by_len[li];
            std::vector<std::pair<std::string, std::vector<std::size_t>>>& vj = by_len[lj];
            for (std::size_t ia = 0; ia < vi.size(); ++ia) {
                for (std::size_t ib = 0; ib < vj.size(); ++ib) {
                    if (li == lj && ia >= ib) continue;  // same group: avoid dup work
                    const std::string& ta = vi[ia].first;
                    const std::string& tb = vj[ib].first;
                    if (ta == tb) continue;  // equal tokens already handled in step 1
                    if (damerau_levenshtein(ta, tb) <= 1) {
                        for (std::size_t i : vi[ia].second)
                            for (std::size_t j : vj[ib].second) add(i, j);
                    }
                }
            }
        }
    }

    return out;
}

}  // namespace dn
