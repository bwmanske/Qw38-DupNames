#include "dn/compare.hpp"

#include <algorithm>
#include <vector>

namespace dn {

int damerau_levenshtein(const std::string& a, const std::string& b) {
    const int m = static_cast<int>(a.size());
    const int n = static_cast<int>(b.size());
    if (m == 0) return n;
    if (n == 0) return m;
    std::vector<std::vector<int>> d(m + 1, std::vector<int>(n + 1, 0));
    for (int i = 0; i <= m; ++i) d[i][0] = i;
    for (int j = 0; j <= n; ++j) d[0][j] = j;
    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int best = std::min({d[i - 1][j - 1] + cost,
                                 d[i][j - 1] + 1,
                                 d[i - 1][j] + 1});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                best = std::min(best, d[i - 2][j - 2] + 1);
            d[i][j] = best;
        }
    }
    return d[m][n];
}

double token_similarity(const std::string& a, const std::string& b) {
    const std::size_t max_len = std::max(a.size(), b.size());
    if (max_len == 0) return 1.0;
    const int dist = damerau_levenshtein(a, b);
    return 1.0 - static_cast<double>(dist) / static_cast<double>(max_len);
}

double token_score(const NormalizedName& a, const NormalizedName& b) {
    const std::size_t denom = std::max(a.tokens.size(), b.tokens.size());
    if (denom == 0) return 1.0;  // both empty: identical
    std::vector<bool> used(b.tokens.size(), false);
    double sum = 0.0;
    for (const std::string& x : a.tokens) {
        int best = -1;
        double best_sim = -1.0;
        for (std::size_t j = 0; j < b.tokens.size(); ++j) {
            if (used[j]) continue;
            const double s = token_similarity(x, b.tokens[j]);
            if (s > best_sim) {
                best_sim = s;
                best = static_cast<int>(j);
            }
        }
        if (best >= 0) {
            used[best] = true;
            sum += best_sim;
        }
    }
    return sum / static_cast<double>(denom);
}

double year_score(int year_a, int year_b) {
    const bool a_has = year_a >= 0;
    const bool b_has = year_b >= 0;
    if (a_has && b_has) return (year_a == year_b) ? 1.0 : 0.0;
    return 0.5;  // exactly one has a year, or neither does
}

PairScore score_pair(const NormalizedName& a, const NormalizedName& b, const Config& cfg) {
    PairScore ps;
    ps.year_score = year_score(a.year, b.year);
    ps.token_score = token_score(a, b);
    ps.score = cfg.w_year * ps.year_score + cfg.w_tokens * ps.token_score;
    if (a.year >= 0 && b.year >= 0 && a.year != b.year) {
        ps.year_cap_applied = true;
        if (ps.score > cfg.year_cap) ps.score = cfg.year_cap;
    }
    if (ps.score >= cfg.match_threshold)
        ps.class_ = MatchClass::Match;
    else if (ps.score >= cfg.close_threshold)
        ps.class_ = MatchClass::Close;
    else
        ps.class_ = MatchClass::NoMatch;
    return ps;
}

}  // namespace dn
