# IMPLEMENTATION.md — How to Build the File Name Matcher (C++17 / CMake)

This document explains **how** to implement the behavior specified in
[SPEC.md](SPEC.md). It is a build guide: project layout, pipeline design,
data structures, algorithms with reference C++17 code, and a testing
strategy.

**Stack:** C++17, CMake ≥ 3.20, standard library only at runtime.
Header-only third-party (fetched at build time, no install step):
`nlohmann/json` for `clusters.json`, `doctest` for tests.
Compiles on GCC, Clang, and MSVC.

---

## 1. Repository Layout

```
namematch/
├── CMakeLists.txt
├── config.example.txt
├── include/nm/
│   ├── types.hpp          # FileEntry, NormalizedName, PairScore, Config
│   ├── utf8.hpp           # utf8_decode, fold_codepoint
│   ├── normalize.hpp      # normalize()
│   ├── compare.hpp        # damerau, token_similarity, score_pair, DSU
│   ├── scanner.hpp        # scan()
│   ├── report.hpp         # write_reports()
│   └── config.hpp         # load_config()
├── src/
│   ├── main.cpp           # CLI parsing, wiring
│   ├── utf8.cpp
│   ├── normalize.cpp
│   ├── compare.cpp
│   ├── scanner.cpp
│   ├── report.cpp
│   └── config.cpp
├── tests/
│   ├── test_utf8.cpp
│   ├── test_normalize.cpp
│   ├── test_compare.cpp
│   └── test_pipeline.cpp  # integration: temp dirs → reports
└── tools/
    └── gen_fold_table/
        └── main.cpp       # one-off C++ tool: generates the Latin fold table
```

Core logic lives in a static library `nm_core` so tests and the CLI share
exactly the same code.

---

## 2. CMake Setup

```cmake
cmake_minimum_required(VERSION 3.20)
project(namematch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Werror)
endif()

add_library(nm_core
  src/utf8.cpp
  src/normalize.cpp
  src/compare.cpp
  src/scanner.cpp
  src/report.cpp
  src/config.cpp
)
target_include_directories(nm_core PUBLIC include)

# Header-only JSON (output only; no runtime install).
include(FetchContent)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(nm_core PUBLIC nlohmann_json)

add_executable(namematch src/main.cpp)
target_link_libraries(namematch PRIVATE nm_core)

# One-off codegen tool (C++): reads Unicode NFKD data, emits
# include/nm/fold_table.hpp. Run manually; output is checked in.
add_executable(gen_fold_table tools/gen_fold_table/main.cpp)

option(NAMEMATCH_TESTS "Build tests" ON)
if(NAMEMATCH_TESTS)
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/releases/download/v2.4.11/doctest.h)
  FetchContent_MakeAvailable(doctest)

  add_executable(nm_tests
    tests/test_utf8.cpp
    tests/test_normalize.cpp
    tests/test_compare.cpp
    tests/test_pipeline.cpp)
  target_link_libraries(nm_tests PRIVATE nm_core doctest)

  include(CTest)
  add_test(NAME nm_tests COMMAND nm_tests)
endif()
```

Notes:
- `FetchContent` with direct URLs keeps the build hermetic and dependency-free
  on the host (no pip, no vcpkg).
- Debug builds can add `-fsanitize=address,undefined` (GCC/Clang) for the
  table-driven UTF-8 code.

---

## 3. Data Structures

```cpp
// include/nm/types.hpp
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nm {

struct FileEntry {
    std::filesystem::path path;   // full path
    std::string name;             // original file name, for display
    std::uintmax_t size = 0;
};

struct NormalizedName {
    std::vector<std::string> tokens;  // folded significant words
    int year = -1;                    // -1 = none; else 1900..2099
    std::vector<std::string> junk;   // removed tokens, for diagnostics
    std::string raw;                 // original name
};

enum class MatchClass { Match, Close, NoMatch };

struct PairScore {
    std::size_t a = 0;              // index into the entries vector
    std::size_t b = 0;
    double score = 0.0;
    double token_score = 0.0;
    double year_score = 0.0;
    bool year_cap_applied = false;
    MatchClass class_ = MatchClass::NoMatch;
};

struct Config {
    int year_lo = 1900, year_hi = 2099;
    double w_year = 0.30, w_tokens = 0.70;
    double match_threshold = 0.85, close_threshold = 0.60;
    double year_cap = 0.50;
    std::vector<std::string> junk;
    bool skip_hidden = true;
    bool recursive = false;
    std::string include = "*";
    std::string exclude = "";
    bool merge_close = false;
};

}  // namespace nm
```

Value semantics everywhere (no shared mutable state) keeps the pipeline
trivially deterministic (NFR-2) and thread-safe if we ever parallelize
pair scoring.

---

## 4. UTF-8 and Folding (the C++-specific part)

C++ has no built-in Unicode normalization, so we own this layer. v1 scope
(SPEC FR-2a): decode UTF-8, fold the Latin ranges to ASCII, drop combining
marks, casefold ASCII.

```cpp
// include/nm/utf8.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nm {

// Decode UTF-8 into code points. Invalid sequences become U+FFFD.
std::vector<std::uint32_t> utf8_decode(const std::string& s);

// Map one code point to its folded ASCII form.
// Returns 0 when the code point has no ASCII equivalent;
// the caller turns 0 into a separator.
char fold_codepoint(std::uint32_t cp);

}  // namespace nm
```

```cpp
// src/utf8.cpp (abridged)
namespace nm {

std::vector<std::uint32_t> utf8_decode(const std::string& s) {
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < s.size();) {
        auto uc = static_cast<unsigned char>(s[i]);
        std::uint32_t cp = 0;
        int extra = 0;
        if      (uc < 0x80) cp = uc;
        else if (uc < 0xC0) { cp = 0xFFFD; }                    // stray continuation
        else if (uc < 0xE0) { cp = uc & 0x1F; extra = 1; }
        else if (uc < 0xF0) { cp = uc & 0x0F; extra = 2; }
        else if (uc < 0xF8) { cp = uc & 0x07; extra = 3; }
        else                { cp = 0xFFFD; }
        bool ok = true;
        for (int k = 0; k < extra && ok; ++k) {
            if (i + 1 + k >= s.size() ||
                (static_cast<unsigned char>(s[i + 1 + k]) & 0xC0) != 0x80)
                ok = false;
            else
                cp = (cp << 6) | (static_cast<unsigned char>(s[i + 1 + k]) & 0x3F);
        }
        if (!ok) { cp = 0xFFFD; extra = 0; }
        out.push_back(cp);
        i += 1 + extra;
    }
    return out;
}

char fold_codepoint(std::uint32_t cp) {
    if (cp < 0x80) {
        if (cp >= 'A' && cp <= 'Z') return char(cp - 'A' + 'a');  // casefold
        return char(cp);   // lowercase letters, digits, punctuation
    }
    if (cp >= 0x0300 && cp <= 0x036F) return 0;   // combining marks → drop
    // Latin-1 Supplement + Latin Extended-A/B (U+00C0..U+024F) → ASCII.
    // kFoldTable is generated by the C++ tool `gen_fold_table`
    // (tools/gen_fold_table/main.cpp) from Unicode NFKD data;
    // entries without an ASCII decomposition are 0 (→ separator).
    if (cp >= 0x00C0 && cp <= 0x024F)
        return kFoldTable[cp - 0x00C0];
    return 0;   // anything else (CJK, Greek, …) → separator in v1
}

}  // namespace nm
```

Design points:
- **Table, not algorithm.** A 400-byte static table is faster and simpler
  than runtime NFKD, and it is exactly the v1 scope. The generator is a
  C++ tool (`gen_fold_table` CMake target, `tools/gen_fold_table/main.cpp`)
  that reads Unicode's NFKD data file and emits `include/nm/fold_table.hpp`
  containing `kFoldTable[]`. The header is checked in as a generated file,
  so the main build stays dependency-free — and the whole project is
  Python-free.
- **Extension path:** full NFKD (fullwidth `ＡＢＣ`, CJK compatibility) is
  an ICU-backed `fold_codepoint()` behind the same header. Nothing else in
  the codebase changes.
- Unknown code points become **separators**, not errors: `The 攻速 Matrix`
  normalizes to `the matrix` on both sides and still matches.

---

## 5. Normalizer

### 5.1 Pipeline order matters

```cpp
// src/normalize.cpp
NormalizedName normalize(const std::string& raw, const Config& cfg) {
    const std::string stem = strip_extension(raw);

    // Fold + separator pass in one loop: any code point without an ASCII
    // fold becomes a space. After this, `text` is pure ASCII.
    std::string text;
    for (auto cp : utf8_decode(stem)) {
        char f = fold_codepoint(cp);
        text.push_back(f ? f : ' ');
    }

    std::vector<std::string> tokens = split_collapse(text, ' ');
    auto [sig, year] = extract_year(std::move(tokens), cfg);
    auto [kept, junk] = remove_junk(std::move(sig), cfg);
    return NormalizedName{std::move(kept), year, std::move(junk), raw};
}
```

**Why this order:**
1. *Extension first* — otherwise `.mp4` becomes a token and junk detection
   gets noisy.
2. *Fold before tokenizing* — after folding the text is plain ASCII, so
   "is this a separator?" is a one-byte test; without folding, `é` would
   survive as a token and `café` would never match `cafe`.
3. *Year before junk* — a year glued to junk (`1984remastered`) must be
   split before the junk list is consulted, so `remastered` can then be
   recognized as junk.
4. *Junk last* — junk matching is done on clean tokens.

### 5.2 Extension stripping

```cpp
std::string strip_extension(const std::string& name) {
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return name;
    // The dot-segment must be the final path component, 1–8 ASCII alnum.
    auto tail = name.substr(dot + 1);
    if (tail.size() > 8) return name;
    for (char c : tail)
        if (!std::isalnum(static_cast<unsigned char>(c))) return name;
    return name.substr(0, dot);
}
```

Edge cases: `file.tar.gz` → strips only `gz` (acceptable for v1);
`1999.1080p` → `1080p` is not a real extension, but stripping it is
harmless because `1080p` is in the junk list anyway.

### 5.3 Year extraction

```cpp
bool is_year_token(const std::string& t, const Config& cfg, int& out) {
    if (t.size() != 4) return false;
    for (char c : t)
        if (c < '0' || c > '9') return false;
    int v = std::stoi(t);
    if (v < cfg.year_lo || v > cfg.year_hi) return false;
    out = v;
    return true;
}

// Returns (tokens-without-year, year or -1). First candidate wins.
std::pair<std::vector<std::string>, int>
extract_year(std::vector<std::string> tokens, const Config& cfg) {
    // Pass 1: standalone tokens.
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        int y;
        if (is_year_token(tokens[i], cfg, y)) {
            tokens.erase(tokens.begin() + i);
            return {std::move(tokens), y};
        }
    }
    // Pass 2: years glued to letters, e.g. "1984remastered".
    // The 4 digits must not be part of a longer digit run.
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        for (std::size_t p = 0; p + 4 <= t.size(); ++p) {
            if (p > 0 && std::isdigit(static_cast<unsigned char>(t[p - 1]))) continue;
            if (p + 4 < t.size() && std::isdigit(static_cast<unsigned char>(t[p + 4]))) continue;
            int y;
            if (!is_year_token(t.substr(p, 4), cfg, y)) continue;
            std::vector<std::string> rest;
            if (p > 0) rest.push_back(t.substr(0, p));
            for (std::size_t k = 0; k < tokens.size(); ++k)
                if (k != i) rest.push_back(tokens[k]);
            if (p + 4 < t.size()) rest.push_back(t.substr(p + 4));
            return {std::move(rest), y};
        }
    }
    return {std::move(tokens), -1};
}
```

Rules enforced (SPEC FR-3): range check rejects `1080`, `720`, `20250`;
longer digit runs never match (`19991`); first candidate wins.

### 5.4 Junk removal

```cpp
std::pair<std::vector<std::string>, std::vector<std::string>>
remove_junk(std::vector<std::string> tokens, const Config& cfg) {
    std::vector<std::string> kept, junk;
    for (auto& t : tokens)
        (std::find(cfg.junk.begin(), cfg.junk.end(), t) != cfg.junk.end()
             ? junk : kept).push_back(std::move(t));
    return {std::move(kept), std::move(junk)};
}
```

Default junk list (quality/resolution, codecs, source, edition words) lives
in `config.example.txt` and is user-extendable. Removed tokens are kept in
`NormalizedName::junk` for `--verbose` output and a future strict mode.

---

## 6. Comparator

### 6.1 Damerau–Levenshtein (transposition-tolerant)

Optimal-string-alignment variant (adjacent transpositions only) — this is
what makes `godfather` vs `godfater` cost 1 instead of 2:

```cpp
// src/compare.cpp
int damerau_levenshtein(const std::string& a, const std::string& b) {
    const int la = static_cast<int>(a.size()), lb = static_cast<int>(b.size());
    std::vector<std::vector<int>> d(la + 1, std::vector<int>(lb + 1, 0));
    for (int i = 0; i <= la; ++i) d[i][0] = i;
    for (int j = 0; j <= lb; ++j) d[0][j] = j;
    for (int i = 1; i <= la; ++i)
        for (int j = 1; j <= lb; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            d[i][j] = std::min({d[i - 1][j] + 1,      // deletion
                                d[i][j - 1] + 1,      // insertion
                                d[i - 1][j - 1] + cost});  // substitution
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);  // transposition
        }
    return d[la][lb];
}
```

**Memoize** token-pair results — the same pairs recur constantly across a
corpus:

```cpp
struct PairHash {
    std::size_t operator()(const std::pair<std::string, std::string>& p) const {
        return std::hash<std::string>{}(p.first) ^
               (std::hash<std::string>{}(p.second) << 1);
    }
};
static std::unordered_map<std::pair<std::string, std::string>, int, PairHash> g_dl_cache;
```

Tokens are short (≤ ~32 chars after truncation), so the DP table is tiny;
the cache is the win.

### 6.2 Token similarity (greedy bipartite matching)

```cpp
double token_similarity(const std::vector<std::string>& a,
                        const std::vector<std::string>& b) {
    if (a.empty() && b.empty()) return 1.0;
    if (a.empty() || b.empty()) return 0.0;

    struct Cand { double s; int i, j; };
    std::vector<Cand> cands;
    cands.reserve(a.size() * b.size());
    for (int i = 0; i < static_cast<int>(a.size()); ++i)
        for (int j = 0; j < static_cast<int>(b.size()); ++j) {
            const int dist = damerau_levenshtein_cached(a[i], b[j]);
            const double s = 1.0 - dist /
                static_cast<double>(std::max(a[i].size(), b[j].size()));
            cands.push_back({s, i, j});
        }
    std::sort(cands.begin(), cands.end(),
              [](const Cand& x, const Cand& y) { return x.s > y.s; });

    std::vector<bool> used_i(a.size()), used_j(b.size());
    double total = 0.0;
    for (const auto& c : cands) {
        if (used_i[c.i] || used_j[c.j]) continue;
        used_i[c.i] = used_j[c.j] = true;
        total += c.s;
    }
    return total / static_cast<double>(std::max(a.size(), b.size()));
}
```

Why this shape:
- **Missing words:** unmatched tokens contribute 0 to the numerator but
  still count in `max(|a|, |b|)`, so `the, shawshank, redemption` vs
  `shawshank, redemption` scores `2/3 ≈ 0.67` on tokens alone — still
  high enough to pass once the year matches.
- **Transpositions:** handled per-token by Damerau distance.
- **Order independence:** set-based, so `park.jurassic` == `jurassic.park`.

### 6.3 Pair score

```cpp
PairScore score_pair(const NormalizedName& na, const NormalizedName& nb,
                     std::size_t ia, std::size_t ib, const Config& cfg) {
    double year_score;
    if (na.year < 0 || nb.year < 0)      year_score = 0.5;
    else if (na.year == nb.year)         year_score = 1.0;
    else                                 year_score = 0.0;

    const double token_score = token_similarity(na.tokens, nb.tokens);
    double score = cfg.w_year * year_score + cfg.w_tokens * token_score;

    const bool cap = na.year >= 0 && nb.year >= 0 && na.year != nb.year;
    if (cap) score = std::min(score, cfg.year_cap);   // hard rule, SPEC FR-5

    PairScore p;
    p.a = ia; p.b = ib;
    p.score = score;
    p.token_score = token_score;
    p.year_score = year_score;
    p.year_cap_applied = cap;
    p.class_ = classify(score, cfg);
    return p;
}

MatchClass classify(double score, const Config& cfg) {
    if (score >= cfg.match_threshold) return MatchClass::Match;
    if (score >= cfg.close_threshold) return MatchClass::Close;
    return MatchClass::NoMatch;
}
```

### 6.4 Clustering (union-find)

```cpp
struct DSU {
    std::vector<int> p;
    explicit DSU(std::size_t n) : p(n) {
        for (std::size_t i = 0; i < n; ++i) p[i] = static_cast<int>(i);
    }
    int find(int x) { return p[x] == x ? x : p[x] = find(p[x]); }
    void unite(int a, int b) { a = find(a); b = find(b); if (a != b) p[b] = a; }
};

// In main pipeline:
DSU dsu(entries.size());
for (const auto& ps : pair_scores)
    if (ps.class_ == MatchClass::Match ||
        (cfg.merge_close && ps.class_ == MatchClass::Close))
        dsu.unite(ps.a, ps.b);
```

CLOSE pairs are emitted to `pairs.csv` but not unioned unless
`--merge-close`.

---

## 7. Blocking (avoiding O(n²))

```cpp
// Yield candidate index pairs without comparing everything.
void candidate_pairs(const std::vector<NormalizedName>& norms,
                     const Config& cfg,
                     std::function<void(std::size_t, std::size_t)> emit) {
    std::map<std::pair<int, std::string>, std::vector<std::size_t>> buckets;
    for (std::size_t i = 0; i < norms.size(); ++i) {
        const auto& n = norms[i];
        const std::string first = n.tokens.empty() ? "" : n.tokens[0];
        buckets[{n.year, first}].push_back(i);
    }

    // 1) Same bucket → all pairs.
    for (auto& [key, idxs] : buckets)
        for (std::size_t a = 0; a < idxs.size(); ++a)
            for (std::size_t b = a + 1; b < idxs.size(); ++b)
                emit(idxs[a], idxs[b]);

    // 2) Transposed first-token neighbors (same year, or either year -1).
    const auto keys = [&] {
        std::vector<std::pair<int, std::string>> v;
        for (auto& [k, _] : buckets) v.push_back(k);
        return v;
    }();
    for (const auto& k : keys)
        for (const auto& k2 : keys) {
            if (k == k2) continue;
            if (k.first != k2.first && !(k.first < 0 || k2.first < 0)) continue;
            if (damerau_levenshtein_cached(k.second, k2.second) > 1) continue;
            for (std::size_t a : buckets[k])
                for (std::size_t b : buckets[k2])
                    emit(a, b);
        }
}
```

Deduplicate with a `std::set<std::pair<std::size_t, std::size_t>>` of
`(min, max)` before scoring. This keeps the hot path near O(n) for
realistic corpora while still catching transposed first words (SPEC §7).

---

## 8. Scanner

`std::filesystem` does the walking; the scanner is strictly read-only
(NFR-5):

```cpp
std::vector<FileEntry> scan(const std::vector<std::filesystem::path>& dirs,
                            const Config& cfg) {
    std::vector<FileEntry> out;
    for (const auto& d : dirs) {
        std::error_code ec;
        auto add = [&](const std::filesystem::directory_entry& e) {
            std::error_code fec;
            if (!e.is_regular_file(fec)) return;
            if (cfg.skip_hidden && has_hidden_component(e.path())) return;
            const std::string fn = e.path().filename().string();
            if (!glob_match(cfg.include, fn)) return;
            if (!cfg.exclude.empty() && glob_match(cfg.exclude, fn)) return;
            out.push_back(FileEntry{e.path(), fn, e.file_size(fec)});
        };
        if (cfg.recursive) {
            for (auto it = std::filesystem::recursive_directory_iterator{
                     d, std::filesystem::directory_options::skip_permission_denied, ec};
                 it != std::filesystem::recursive_directory_iterator{};
                 it.increment(ec))
                add(*it);
        } else {
            for (auto it = std::filesystem::directory_iterator{d, ec};
                 it != std::filesystem::directory_iterator{};
                 it.increment(ec))
                add(*it);
        }
        if (ec) { warn(d, ec); ec.clear(); }   // keep going (SPEC §12)
    }
    return out;
}
```

`glob_match` is a small `fnmatch`-style matcher (or `fnmatch()` on POSIX,
`PathMatching`-style wildcards on Windows) supporting `*` and `?` — enough
for `--include "*.mp4"`.

---

## 9. Reporter

Write four artifacts (SPEC FR-8) into `./match-report` (or `--out`):

1. **`report.md`** — per cluster: anchor name, then each member with
   `path — score — year — junk tokens`. Sort clusters by size desc, then
   anchor name.
2. **`pairs.csv`** — all pairs with `score ≥ close_threshold`, columns:
   `path_a,path_b,score,class,year_a,year_b,token_score,year_score`.
   Escape per RFC 4180 (quote fields containing `,`, `"`, or newline).
3. **`clusters.json`** — via `nlohmann::json`:
   `[{ "anchor": ..., "members": [ {path, name, normalized, year, junk,
   score_vs_anchor} ] }]`.
4. **`unmatched.csv`** — entries in no cluster and with no CLOSE partner.

Determinism (NFR-2): sort all rows by `(path_a, path_b)` using
`std::filesystem::path::compare`; write with `\n` line endings; fixed
float format (`%.4f`).

---

## 10. Configuration

`config.example.txt` — flat `key = value` file, parsed by ~60 lines of
code (no YAML dependency):

```ini
# namematch config
year_lo = 1900
year_hi = 2099
w_year = 0.30
w_tokens = 0.70
match_threshold = 0.85
close_threshold = 0.60
year_cap = 0.50
junk = 1080p, 720p, 4k, uhd, hdr, x264, x265, hevc, avc, aac, ac3, eac3, \
       dts, atmos, mp3, flac, webrip, webdl, hdtv, bluray, brrip, dvdrip, \
       remux, remastered, remaster, extended, unrated, uncut, theatrical, \
       directors, cut, deluxe, anniversary, complete, definitive, proper, \
       repack, internal, sample, limited
skip_hidden = true
recursive = false
include = *
exclude =
```

Load order: built-in defaults → config file → CLI flags (CLI wins).

---

## 11. CLI

```
namematch DIR [DIR ...]
  --config FILE        # config file (see §10)
  --recursive          # descend into subdirectories
  --include GLOB       # e.g. "*.mp4"
  --exclude GLOB
  --out DIR            # report directory (default ./match-report)
  --merge-close        # include CLOSE pairs in clustering
  --verbose            # print normalization per file
  --explain A B        # score one pair and print the breakdown
```

Hand-rolled argument parsing in `main.cpp` (positional dirs + `--flag`
loop) keeps the dependency count at zero. `--explain` is the debugging
workhorse: it prints tokens, year, junk, per-token matches, and the final
weighted score for two names.

---

## 12. Testing Strategy

Framework: **doctest** (single header) + **CTest**.

### 12.1 Unit tests (pure functions, no filesystem)
- `fold_codepoint`: `é → e`, `É → e`, `ß → 0` (separator, out of scope),
  `İ → i` (casefold), combining `U+0301 → 0`, fullwidth `Ａ → 0` in v1
  (documented), `z → z`, `5 → 5`.
- `utf8_decode`: valid 1–4 byte sequences, invalid lead byte, truncated
  sequence → `U+FFFD`.
- `normalize`: every row of the SPEC §5 worked-examples table is a test
  case (table-driven via doctest's `TEST_CASE` + data).
- `extract_year`: `1999`, `(1999)`, `1984remastered`, `1999.1080p`,
  `1080` (rejected), `19991` (rejected), two years (first wins).
- `damerau_levenshtein`: `("godfather","godfater") == 1`,
  `("abc","abc") == 0`, `("abc","cab") == 2`, empty-string cases.
- `token_similarity`: missing-word case, transposition case, disjoint case.
- `score_pair`: each row of the SPEC §8 acceptance table (19 rows).

### 12.2 Integration tests (temp directories)
- Build a temp tree (`std::filesystem::temp_directory_path()` + unique
  suffix) with two directories containing the SPEC §8 name pairs, run the
  full pipeline, assert cluster membership and that all four report files
  exist with expected rows.

### 12.3 Property / regression
- Determinism: run the pipeline twice on the same fixture, assert
  byte-identical `pairs.csv` and `clusters.json`.
- Performance smoke test: generate 10,000 synthetic names, assert
  wall-clock < 60 s via `std::chrono::steady_clock` (NFR-1).
- ASan/UBSan pass in Debug (table-driven UTF-8 code is the risk area).

---

## 13. Build Order (suggested milestones)

1. **M1 — UTF-8 + normalize:** `utf8.{hpp,cpp}`, `normalize.{hpp,cpp}`,
   fold-table generator, unit tests from §12.1. *Exit criterion: all
   SPEC §5 examples pass on GCC, Clang, and MSVC.*
2. **M2 — Compare:** Damerau, token similarity, `score_pair`, DSU +
   SPEC §8 table as tests. *Exit criterion: all 19 acceptance rows
   classify as expected.*
3. **M3 — Scan + block:** `scanner.cpp`, blocking, clustering wiring.
4. **M4 — Report + CLI:** four artifacts, `--verbose`, `--explain`,
   config loading.
5. **M5 — Hardening:** determinism test, performance smoke test,
   sanitizer runs, edge-case sweep (empty names, invalid UTF-8, very long
   names, no tokens after junk removal → treat as unmatchable, report in
   `unmatched.csv`).

---

## 14. Edge Cases to Handle Explicitly

| Case | Behavior |
|---|---|
| Name becomes empty after normalization (e.g. `1080p.x264`) | Skip from matching; list in `unmatched.csv` with a note. |
| Diacritics / case | Required by SPEC FR-2a: fold via `fold_codepoint()` (v1: Latin table + combining-mark removal + ASCII casefold), so `café` == `cafe`, `Léon` == `leon`. Original name kept for display. |
| Invalid UTF-8 bytes | Decode to `U+FFFD` → separator; never crash, never assume ASCII. |
| Non-Latin scripts (CJK, Greek) | v1: code points become separators (both sides fold the same way, so identical names still match). Full NFKD via ICU backend is the documented extension. |
| Same name in two directories | Always a MATCH (score 1.0) — this is the primary use case. |
| Year-only name (`1999`) | Tokens empty → unmatchable, same as above. |
| Very long names (> 200 chars) | Truncate tokens list to first 32 tokens for scoring; keep full name in reports. |
| Symlinks / broken links | `is_regular_file()` follows symlinks; broken links are skipped silently. |
| Read-only / permission errors | `skip_permission_denied` + per-directory `error_code` check; warn and continue. |
| Windows paths | `std::filesystem::path` throughout; use `.generic_string()` only in report output for stable, comparable paths. |
