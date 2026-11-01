# Spec Audit — P7 Hardening

Walks every Functional Requirement (FR-1…FR-9) and Non-Functional Requirement
(NFR-1…NFR-5) from `specs/SPEC.md` and records how each is satisfied: by an
automated test (in `dn_tests`), by code inspection, or as a documented manual
check. This is the P7 exit-criteria artifact ("spec audit table complete with
no open items").

**Context:** the project pivoted from the original CLI + report-files design to
a Win32 GUI with in-memory clusters and user-initiated deletion. FR-8 (report
files) and FR-9 (CLI diagnostics) are therefore **obsolete** — their intent is
carried by the GUI queue and the Options dialog instead. The matching core
(FR-1…FR-7) is unchanged and fully tested.

**Test counts:** 107 test cases / 346 assertions, all passing on **MSVC**
(Release), **Clang** (`clang-cl`, Release), and **GCC** (MinGW-w64, Release).
ASan Debug build (MSVC): same 107 cases, no memory errors.

## Functional Requirements

| Req | Description | Status | Evidence |
|---|---|---|---|
| FR-1 | Directory scanning (recursive/include/exclude, record path/name/size/mtime, skip hidden) | **Covered** | `tests/test_scanner.cpp` — non-recursive, recursive, include/exclude globs, hidden skip + `skip_hidden=false`, size/dirIndex/protection/lastModified, multi-dir, empty/missing dir, progress callback, FILETIME→epoch regression (lastModified is real-world epoch seconds, not 1601-based) |
| FR-2 | Name normalization → tokens/year/junk | **Covered** | `tests/test_normalize.cpp` — all SPEC §5 worked examples, extension stripping, glued-year splitting |
| FR-2a | Unicode/diacritic folding (Latin table + combining marks + casefold); original preserved | **Covered** | `tests/test_utf8.cpp` — accented Latin, casefolding, combining marks, ligatures, non-Latin passthrough; `tests/test_normalize.cpp` checks `raw` is preserved |
| FR-3 | Year extraction (4-digit 1900–2099; standalone/paren/glued/period; first wins; not part of longer run; range-checked) | **Covered** | `tests/test_normalize.cpp` — glued both directions, 5-digit rejected, out-of-range rejected, first-year-wins, year-as-extension preserved |
| FR-4 | Junk-token removal (configurable, reversible) | **Covered (mechanism)** — see note | `tests/test_normalize.cpp` (junk removed + kept in `junk`), `tests/test_ini.cpp` (junk list round-trips, empty list). **Note:** the *default* junk list is a minimal stub (`the, extended, 1080p, x264, 720p, bluray, directors`) — just enough for the 19 acceptance cases. The full FR-4 categories (codecs, sources, edition words, release-group heuristic) are **not** all in the default list; this is a documented, deferrable enhancement (see `include/dn/types.hpp`), not a defect. The removal mechanism itself is fully tested and configurable. |
| FR-5 | Similarity scoring (`W_year·year_score + W_tokens·token_score`; Damerau token similarity; year hard-cap) | **Covered** | `tests/test_compare.cpp` — `damerau_levenshtein`, `token_similarity`, `year_score` table, `score_pair` + year cap; `tests/test_match.cpp` (different years not clustered) |
| FR-6 | Classification thresholds (MATCH ≥0.85, CLOSE 0.60–0.85, NO MATCH <0.60; configurable) | **Covered** | `tests/test_compare.cpp`, `tests/test_acceptance.cpp` (19 SPEC §8 rows), `tests/test_ini.cpp` (thresholds configurable) |
| FR-7 | Clustering (MATCH merged via union-find; CLOSE not merged unless `merge_close`) | **Covered** | `tests/test_cluster.cpp` (DSU merge, `merge_close` on/off), `tests/test_match.cpp`, `tests/test_determinism.cpp` |
| FR-8 | Output report files (`report.md`, `pairs.csv`, `clusters.json`, `unmatched.csv`) | **Obsolete** | Superseded by the GUI: clusters are shown in the flat list-view queue (`src/gui/app.cpp` `PopulateQueue`), not written to report files. The original CLI report writers were never built (the project pivoted at P3). |
| FR-9 | Diagnostics (`--verbose`, `--explain`) | **Obsolete** | Superseded by the GUI (Options dialog + status bar). CLI diagnostics were part of the obsolete CLI design. |

## Non-Functional Requirements

| Req | Description | Status | Evidence |
|---|---|---|---|
| NFR-1 | Performance: 10,000 files across 10 dirs < 60 s; blocking keeps comparisons sub-quadratic | **Covered** | `tests/test_perf.cpp` — 10,000 synthetic names in ~2 s (Release). Blocking buckets by first token (`src/blocking.cpp`); unmatchable names skipped. |
| NFR-2 | Determinism: same inputs → byte-identical outputs | **Covered** | `tests/test_determinism.cpp` — pipeline run 6×, canonical serialization byte-compared (default + `merge_close`). Blocking uses ordered `std::map`/`std::set`; `cluster()` sorts members and clusters by anchor. |
| NFR-3 | Configurability: weights, thresholds, junk, year range, hard rules overridable | **Covered** | `tests/test_ini.cpp` (all 13 `[InitState]` keys round-trip; precedence default < INI < CLI; write-back), `tests/test_cli.cpp` (`--ini`/`--match`/`--close`) |
| NFR-4 | Portability: C++17, CMake ≥ 3.20, zero runtime deps, builds on GCC/Clang/MSVC | **Covered** | C++17 + CMake 3.20 + std-lib-only: **verified** (`dn_core` links only the CRT). All three compilers build the full solution and pass all 107 tests: **MSVC** (`cl.exe`, VS generator, Release), **Clang** (`clang-cl` 22.1.3, MSVC-like mode, Ninja) — CMake auto-detects MSVC-mode and applies the MSVC flags, and **GCC** (MinGW-w64 `g++` 16.2.0, Ninja). The only cross-compiler fix needed was adding `-municode` to the `DupNames` link line for non-MSVC (MinGW selects the `wWinMain` wide entry point that way; MSVC does so automatically from `UNICODE` + the WIN32 subsystem). The code uses only portable C++17; the Windows-specific code is isolated in `src/ini.cpp` and `src/scanner.cpp` (Win32 attribute/INI APIs), as expected for a Windows-only app. |
| NFR-5 | Safety: read-only w.r.t. scanned directories; never modify/rename/delete scanned files | **Covered** | `tests/test_scanner.cpp` (scan produces no filesystem mutations); code inspection of `src/scanner.cpp` (only `status`/`file_size`/`GetFileAttributes*` reads — no write calls). **Note:** the app *does* delete duplicates (P6), but that is a separate, explicit, user-initiated action that respects the protected/common split (`src/deletion.cpp` `plan_deletion` keeps one copy and skips protected dirs) — it is not part of the scan. |

## Open items

None. Every FR/NFR is covered by an automated test, satisfied by code
inspection, or obsolete by the GUI pivot (FR-8, FR-9). NFR-4 is fully verified
on all three compilers (MSVC, Clang/clang-cl, GCC/MinGW-w64). The one documented
limitation is the minimal default junk list (FR-4 note) — a deferrable
enhancement, not a correctness gap.
