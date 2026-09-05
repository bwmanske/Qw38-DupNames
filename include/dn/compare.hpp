#pragma once
#include "dn/types.hpp"

namespace dn {

// Optimal String Alignment distance (adjacent transpositions cost 1, not 2).
int damerau_levenshtein(const std::string& a, const std::string& b);

// Similarity of two tokens in [0,1]: 1 - DL(a,b)/max(len(a),len(b)).
double token_similarity(const std::string& a, const std::string& b);

// Multiset-aware, transposition-tolerant token score (SPEC FR-5).
double token_score(const NormalizedName& a, const NormalizedName& b);

// Year component score (SPEC FR-5).
double year_score(int year_a, int year_b);

// Score a pair of normalized names and classify (SPEC FR-5/FR-6). The caller
// sets PairScore::a / ::b (indices); this fills the rest.
PairScore score_pair(const NormalizedName& a, const NormalizedName& b, const Config& cfg);

}  // namespace dn
