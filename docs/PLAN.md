# PLAN.md — Implementation Plan

**Project:** `namematch` — cross-directory file name similarity matcher
**Spec:** [SPEC.md](SPEC.md) · **Build guide:** [IMPLEMENTATION.md](IMPLEMENTATION.md)
**Stack:** C++17, CMake ≥ 3.20, zero runtime dependencies.
**Language policy:** *everything* is implemented in C++ — including the
fold-table generator (originally sketched as a Python script, now the
`gen_fold_table` C++ tool). No Python, no other scripting language,
anywhere in the project.

---

## 0. Ground Rules

- Every phase ends with its tests green on **at least one** of GCC / Clang /
  MSVC (MSVC on this Windows machine is the primary target).
- No phase merges on a red build; `nm_core` is the shared unit of work.
- All public behavior is traceable to a SPEC requirement (FR/NFR) — each
  task below cites its requirement.
- Generated artifacts (`fold_table.hpp`) are checked in; the build itself
  never runs codegen.

---

## Phase 1 — Project Scaffolding

**Goal:** an empty but building CMake project with the test harness wired.

| # | Task | Files |
|---|---|---|
| 1.1 | Root `CMakeLists.txt`: C++17, warnings (`/W4 /permissive-` MSVC, `-Wall -Wextra -Werror` GCC/Clang), `nm_core` static lib, `namematch` exe, `gen_fold_table` tool, doctest + CTest via `FetchContent` | `CMakeLists.txt` |
| 1.2 | Directory skeleton per IMPLEMENTATION.md §1 | `include/nm/`, `src/`, `tests/`, `tools/gen_fold_table/` |
| 1.3 | `types.hpp` with `FileEntry`, `NormalizedName`, `PairScore`, `MatchClass`, `Config` (defaults = SPEC §8 config values) | `include/nm/types.hpp` |
| 1.4 | Smoke test: one doctest `TEST_CASE` that compiles and runs under CTest | `tests/test_smoke.cpp` |

**Exit criteria:** `cmake -B build && cmake --build build && ctest --test-dir build`
succeeds; `namematch` runs and prints usage.

---

## Phase 2 — UTF-8 + Folding (SPEC FR-2a)

**Goal:** the Unicode layer, the riskiest C++-specific piece, done first.

| # | Task | Files |
|---|---|---|
| 2.1 | `utf8_decode()`: 1–4 byte sequences, invalid → `U+FFFD` | `src/utf8.cpp`, `include/nm/utf8.hpp` |
| 2.2 | `gen_fold_table` C++ tool: reads Unicode NFKD data, emits `include/nm/fold_table.hpp` with `kFoldTable[0x190]` for U+00C0–U+024F (ASCII decomposition or 0) | `tools/gen_fold_table/main.cpp`, `include/nm/fold_table.hpp` (generated, checked in) |
| 2.3 | `fold_codepoint()`: ASCII casefold, combining marks (U+0300–U+036F) → 0, Latin table lookup, else 0 | `src/utf8.cpp` |
| 2.4 | Unit tests: `é→e`, `É→e`, `İ→i`, `ß→0`, `U+0301→0`, `Ａ→0` (v1 documented), invalid UTF-8 → `U+FFFD` | `tests/test_utf8.cpp` |

**Exit criteria:** all §12.1 UTF-8 tests pass; ASan/UBSan clean in Debug.

---

## Phase 3 — Normalizer (SPEC FR-2, FR-3, FR-4)

**Goal:** raw name → `NormalizedName`, fully deterministic.

| # | Task | Files |
|---|---|---|
| 3.1 | `strip_extension()` (1–8 ASCII alnum final dot-segment) | `src/normalize.cpp` |
| 3.2 | `normalize()` pipeline: fold → separator pass → tokenize (order per IMPLEMENTATION.md §5.1) | `src/normalize.cpp` |
| 3.3 | `extract_year()`: standalone pass + glued pass, digit-run guards, first-wins, range check | `src/normalize.cpp` |
| 3.4 | `remove_junk()` + default junk list in `config.example.txt` | `src/normalize.cpp`, `config.example.txt` |
| 3.5 | `load_config()`: `key = value` parser, defaults → file → CLI precedence | `src/config.cpp`, `include/nm/config.hpp` |
| 3.6 | Unit tests: every SPEC §5 worked-example row (table-driven), all `extract_year` cases from §12.1, config precedence cases | `tests/test_normalize.cpp` |

**Exit criteria:** all SPEC §5 examples and §12.1 normalize/year tests pass.

---

## Phase 4 — Comparator (SPEC FR-5, FR-6, FR-7)

**Goal:** scoring, classification, clustering.

| # | Task | Files |
|---|---|---|
| 4.1 | `damerau_levenshtein()` (OSA variant) + `unordered_map` memo cache | `src/compare.cpp` |
| 4.2 | `token_similarity()`: pairwise sims, greedy best-first assignment, `max(|a|,|b|)` denominator | `src/compare.cpp` |
| 4.3 | `score_pair()` with year component + hard year-cap rule; `classify()` | `src/compare.cpp` |
| 4.4 | `DSU` (union-find) + cluster grouping | `src/compare.cpp` |
| 4.5 | `candidate_pairs()` blocking: (year, first-token) buckets + transposition-neighbor buckets, pair dedup | `src/compare.cpp` |
| 4.6 | Unit tests: Damerau cases, token-similarity cases, **all 19 SPEC §8 acceptance rows** as `score_pair` tests | `tests/test_compare.cpp` |

**Exit criteria:** all 19 acceptance rows classify exactly as SPEC §8
expects; blocking produces the same pair set as a brute-force O(n²)
reference on a 500-name fixture (differential test).

---

## Phase 5 — Scanner (SPEC FR-1, NFR-5)

**Goal:** directory walking, read-only.

| # | Task | Files |
|---|---|---|
| 5.1 | `scan()`: `directory_iterator` / `recursive_directory_iterator`, `skip_permission_denied`, per-directory `error_code` warn-and-continue | `src/scanner.cpp` |
| 5.2 | `has_hidden_component()`, `glob_match()` (`*`, `?`) for include/exclude | `src/scanner.cpp` |
| 5.3 | Unit/integration tests: temp-dir fixtures, hidden files, permission-denied dir (where testable), include/exclude globs | `tests/test_scanner.cpp` |

**Exit criteria:** scanner tests pass; static review confirms no write
calls anywhere in `scanner.cpp` (NFR-5).

---

## Phase 6 — Reporter + CLI (SPEC FR-8, FR-9)

**Goal:** end-to-end runnable tool.

| # | Task | Files |
|---|---|---|
| 6.1 | `report.md` writer (clusters by size desc, then anchor) | `src/report.cpp` |
| 6.2 | `pairs.csv` writer (RFC 4180 escaping, `%.4f`, sorted rows) | `src/report.cpp` |
| 6.3 | `clusters.json` via `nlohmann::json` | `src/report.cpp` |
| 6.4 | `unmatched.csv` writer | `src/report.cpp` |
| 6.5 | `main.cpp`: arg parsing (positional dirs + flags per IMPLEMENTATION.md §11), config load, pipeline wiring, `--verbose`, `--explain` | `src/main.cpp` |
| 6.6 | Integration test: temp tree with SPEC §8 name pairs → assert clusters + all four artifacts exist with expected rows | `tests/test_pipeline.cpp` |

**Exit criteria:** `namematch dirA dirB` on a fixture tree produces all
four reports with correct cluster membership; `--explain` prints the full
breakdown for a pair.

---

## Phase 7 — Hardening & Verification (NFR-1, NFR-2, NFR-4)

**Goal:** prove the spec's non-functional requirements.

| # | Task |
|---|---|
| 7.1 | Determinism test: run pipeline twice on the same fixture, byte-compare `pairs.csv` + `clusters.json` (NFR-2) |
| 7.2 | Performance smoke test: 10,000 synthetic names < 60 s wall clock (NFR-1) |
| 7.3 | Sanitizer pass (ASan/UBSan Debug) across the full suite |
| 7.4 | Cross-compiler check: GCC and Clang builds green (NFR-4) |
| 7.5 | Edge-case sweep per IMPLEMENTATION.md §14 (empty-after-normalization, invalid UTF-8, year-only names, >200-char names, symlinks, permission errors) |
| 7.6 | Final spec audit: walk SPEC FR-1…FR-9 + NFR-1…NFR-5 and confirm each has a passing test or a documented manual check |

**Exit criteria:** full suite green on all three compilers; spec audit
table complete with no open items.

---

## Dependency Graph

```
Phase 1 (scaffold)
   └── Phase 2 (utf8/fold)
         └── Phase 3 (normalize)
               └── Phase 4 (compare)
                     ├── Phase 5 (scanner)      ← can start after Phase 1
                     └── Phase 6 (report+CLI)   ← needs 3, 4, 5
                           └── Phase 7 (hardening)
```

Phase 5 is independent of 2–4 and can be interleaved if parallel work is
wanted; everything else is strictly sequential.

## Definition of Done (project-level)

1. `namematch DIR [DIR ...]` runs end-to-end and emits all four report
   artifacts.
2. All 19 SPEC §8 acceptance cases pass as automated tests.
3. Full test suite green on MSVC (primary), GCC, and Clang.
4. Determinism and performance NFRs verified by test.
5. Zero runtime dependencies; no Python anywhere in the tree.
6. SPEC, IMPLEMENTATION, and PLAN docs consistent with the shipped code.
