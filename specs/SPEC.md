# File Name Similarity Matching — Specification

**Status:** Draft v0.1
**Date:** 2026-08-19

## 1. Purpose

Given a list of directories, scan the files in each directory and compare file
names across directories to identify names that are **matching** or **close**
to each other, even when the names have been "curated" (e.g. `The Matrix (1999)`)
or left uncurated (e.g. `the.matrix.1999.extended.1080p.x264`).

The matcher must tolerate:

- Different separators (spaces, dashes, periods, underscores, mixed).
- A year that may or may not be in parentheses, and may be followed by
  extraneous words or characters (`1999 extended`, `1999.1080p`, `(1999) remastered`).
- Missing words (e.g. `The Shawshank Redemption (1994)` vs `shawshank.redemption.1994`).
- Transposed or mistyped characters (e.g. `godfather` vs `godfater`).
- Extraneous tokens (quality tags, codecs, release groups, edition words).

## 2. Scope

### In scope
- Scanning a configurable list of directories (non-recursive by default, recursive optional).
- Normalizing file names into a comparable token form.
- Extracting and comparing a year component.
- Scoring name pairs with a weighted similarity score.
- Grouping names into match clusters and producing a report.

### Out of scope (v1)
- Comparing file *contents* (hashing) — names only.
- Cross-language / translation matching.
- GUI. CLI + report files only.

## 3. Inputs

| Input | Description |
|---|---|
| `directories` | One or more directory paths (CLI args or a `dirs.txt` file, one path per line). |
| `--recursive` | Optional flag to descend into subdirectories. |
| `--config` | Optional YAML/JSON config overriding defaults (thresholds, junk-token list, weights). |
| `--include` / `--exclude` | Optional glob filters for file extensions (default: all files). |

## 4. Functional Requirements

### FR-1: Directory scanning
- Enumerate files in each directory (respecting `--recursive`, `--include`, `--exclude`).
- Record for each file: full path, file name, size, last-modified time.
- Skip hidden/system files by default; make this configurable.

### FR-2: Name normalization
Every file name must pass through a deterministic normalization pipeline
(see §5) producing:

- `tokens`: ordered list of significant words (folded, lowercase, alphanumeric).
- `year`: a 4-digit year (1900–2099) if one was detected, else `null`.
- `junk`: tokens removed as extraneous (kept for diagnostics).

**FR-2a: Unicode / diacritic folding**
- Names are compared in a *folded* form: Unicode folding (NFKD-equivalent),
  removal of combining marks, then casefolding.
- Consequence: diacritical marks are ignored in matching — `café` == `cafe`,
  `Léon` == `leon`, `naïve` == `naive`, `Zürich` == `zurich`.
- Case is likewise ignored (`THE` == `the`).
- The original, un-folded name is always preserved for display in reports.
- **v1 scope:** folding covers the Latin ranges (U+00C0–U+024F, Latin-1
  Supplement + Latin Extended-A/B) via a built-in table, plus removal of
  combining marks (U+0300–U+036F) and ASCII casefolding. This covers
  French, German, Spanish, Portuguese, Polish, Turkish, and most other
  Latin-script titles.
- **Extension:** full Unicode NFKD (fullwidth forms, CJK compatibility,
  Greek/Cyrillic) is an optional backend behind the same
  `fold_codepoint()` interface (e.g. ICU); not required for v1.
- Out of scope: compatibility mappings beyond NFKD (e.g. `ß` → `ss`).

### FR-3: Year extraction
- A year is a 4-digit number in range **1900–2099**.
- It may appear as a standalone token (`1999`), inside parentheses
  (`(1999)`), glued to other text (`1999extended`, `1984remastered`), or
  separated by periods/dashes (`1999.1080p`).
- If multiple candidate years exist, the **first** one wins; the rest are
  treated as ordinary tokens.
- A year is **not** a year if it is part of a longer digit run
  (`19991` is not a year) or is outside the range (`1080` is not a year).

### FR-4: Junk-token removal
Tokens matching a configurable junk list are removed from the significant
token list before scoring. Default junk categories:

- **Quality/resolution:** `1080p`, `720p`, `4k`, `uhd`, `hdr`, `sdr`, `2160p`
- **Codecs/containers:** `x264`, `x265`, `hevc`, `avc`, `h264`, `h265`, `aac`, `ac3`, `eac3`, `dts`, `atmos`, `mp3`, `flac`
- **Source:** `webrip`, `web-dl`, `hdtv`, `bluray`, `blu-ray`, `brrip`, `dvdrip`, `remux`
- **Edition words:** `remastered`, `remaster`, `extended`, `unrated`, `uncut`, `theatrical`, `directors.cut`, `deluxe`, `anniversary`, `complete`, `definitive`, `proper`, `repack`, `internal`, `sample`, `limited`
- **Release-group patterns:** trailing all-caps tokens of length ≥ 3 that
  appear after the year (heuristic, configurable, off by default).

Junk removal is **configurable and reversible** (tokens are kept in `junk`
for diagnostics and for a "strict" scoring mode).

### FR-5: Similarity scoring
Each pair of normalized names is scored in `[0.0, 1.0]` as:

```
score = W_year * year_score + W_tokens * token_score
```

with defaults `W_year = 0.30`, `W_tokens = 0.70`.

**year_score**

| Condition | Value |
|---|---|
| Both have a year and they are equal | `1.0` |
| Exactly one has a year | `0.5` (neutral) |
| Neither has a year | `0.5` (neutral) |
| Both have years and they differ | `0.0` |

**token_score** — multiset-aware, transposition-tolerant:

1. For each token `a` in name A, compute the best similarity against all
   tokens `b` in name B: `sim(a,b) = 1 - DamerauLevenshtein(a,b) / max(len(a), len(b))`.
2. Greedily assign each token in A to its best unused token in B (and vice
   versa for tokens in B not yet matched).
3. `token_score = (sum of matched similarities + 0 for unmatched tokens) / max(|A|, |B|)`.

Properties:
- **Missing words** lower the score proportionally (unmatched tokens count 0).
- **Transposed characters** are handled by Damerau–Levenshtein (a single
  transposition costs 1, not 2).
- **Word order** does not matter (set-based matching).

**Hard rule:** if both names have years and the years differ, the final
score is capped at `0.50` regardless of token similarity (different years
almost always mean different editions).

### FR-6: Classification thresholds
| Score | Class |
|---|---|
| `score ≥ 0.85` | **MATCH** |
| `0.60 ≤ score < 0.85` | **CLOSE** (candidate, needs review) |
| `score < 0.60` | **NO MATCH** |

Thresholds and weights are configurable.

### FR-7: Clustering
- All pairs classified **MATCH** are merged into clusters (union-find).
- **CLOSE** pairs are reported separately and are *not* merged into
  clusters by default (`--merge-close` flag to include them).
- A cluster is a set of file paths whose names are considered the same
  work/edition.

### FR-8: Output
Produce, in an output directory (default `./match-report`):

1. `report.md` — human-readable: one section per cluster, each entry showing
   path, normalized form, year, score vs. cluster anchor, and junk tokens.
2. `pairs.csv` — every compared pair above the NO-MATCH floor:
   `path_a, path_b, score, class, year_a, year_b, token_score, year_score`.
3. `clusters.json` — machine-readable clusters with full metadata.
4. `unmatched.csv` — files that had no MATCH or CLOSE partner.

### FR-9: Diagnostics
- `--verbose` prints the normalization of each name (tokens, year, junk).
- `--explain <nameA> <nameB>` prints the full scoring breakdown for one pair.

## 5. Normalization Pipeline (deterministic)

```
raw name
  → strip extension (last dot-segment)
  → fold: Unicode fold (v1: Latin table + combining-mark removal), casefold   (FR-2a)
  → replace every non-alphanumeric run with a single space
  → tokenize
  → extract year (first 4-digit token in 1900–2099; also split years
     glued to letters, e.g. "1984remastered" → year 1984 + token "remastered")
  → remove junk tokens (configurable list)
  → drop empty tokens
  → result: { tokens: [...], year: int|null, junk: [...] }
```

**Worked examples**

| Raw name | tokens | year | junk |
|---|---|---|---|
| `The Matrix (1999)` | `the, matrix` | `1999` | — |
| `the.matrix.1999.extended.1080p.x264` | `the, matrix` | `1999` | `extended, 1080p, x264` |
| `Inception (2010)` | `inception` | `2010` | — |
| `inception.2010.1080p.x264` | `inception` | `2010` | `1080p, x264` |
| `The Shawshank Redemption (1994)` | `the, shawshank, redemption` | `1994` | — |
| `shawshank.redemption.1994` | `shawshank, redemption` | `1994` | — |
| `The Godfather (1972)` | `the, godfather` | `1972` | — |
| `thegodfater.1972` | `the, godfater` | `1972` | — |
| `Jurassic Park (1993)` | `jurassic, park` | `1993` | — |
| `jurassic.park.1993` | `jurassic, park` | `1993` | — |

## 6. Non-Functional Requirements

- **NFR-1 Performance:** blocking (see §7) must keep pairwise comparisons
  below ~O(n log n) for typical inputs; 10,000 files across 10 directories
  should complete in under 60 s on a laptop.
- **NFR-2 Determinism:** same inputs → same outputs, byte-for-byte.
- **NFR-3 Configurability:** weights, thresholds, junk list, year range,
  and hard rules must be overridable via config file or CLI.
- **NFR-4 Portability:** C++17, CMake ≥ 3.20 build; zero *runtime*
  dependencies (standard library only). Header-only third-party libraries
  are permitted for JSON output and tests (fetched at build time).
  Builds on GCC, Clang, and MSVC.
- **NFR-5 Safety:** read-only with respect to scanned directories; never
  modify, rename, or delete scanned files.

## 7. Candidate Blocking (performance)

To avoid comparing every pair:

1. Bucket files by `(year, first significant token)`.
2. Compare every pair within a bucket.
3. Additionally compare each file against files in the same directory's
   bucket neighbors when the first token differs by Damerau distance ≤ 1
   (catches transposed first words).
4. Files with no year are compared against all files in the same
   first-token bucket regardless of year.

## 8. Acceptance Criteria (test cases)

| # | Name A | Name B | Expected |
|---|---|---|---|
| 1 | `The Matrix (1999)` | `the.matrix.1999.extended.1080p.x264` | MATCH |
| 2 | `Inception (2010)` | `inception.2010.1080p.x264` | MATCH |
| 3 | `The Shawshank Redemption (1994)` | `shawshank.redemption.1994` | MATCH (missing word tolerated) |
| 4 | `The Godfather (1972)` | `thegodfater.1972` | MATCH (transposition tolerated) |
| 5 | `The Matrix (1999)` | `the.matrix.reloaded.2003` | NO MATCH (year cap) |
| 6 | `Jurassic Park (1993)` | `jurassic.park.1993` | MATCH |
| 7 | `Blade Runner (1982)` | `blade.runner.1982.directors.cut` | MATCH |
| 8 | `Blade Runner (1982)` | `blade.runner.2017` | NO MATCH (year cap) |
| 9 | `Interstellar (2014)` | `interstellar.2014` | MATCH |
| 10 | `Interstellar (2014)` | `interstellar.2014.1080p` | MATCH |
| 11 | `The Dark Knight (2008)` | `dark.knight.rises.2012` | NO MATCH or CLOSE (year cap) |
| 12 | `Pulp Fiction (1994)` | `pulp.fiction.1994` | MATCH |
| 13 | `Pulp Fiction (1994)` | `pulp.fiction.1994.720p.bluray` | MATCH |
| 14 | `Fight Club (1999)` | `fight.club.1999` | MATCH |
| 15 | `Fight Club (1999)` | `fight.club.1999.1080p.x264` | MATCH |
| 16 | `The Godfather (1972)` | `the.godfather.part.ii.1974` | NO MATCH (year cap) |
| 17 | `Amélie (2001)` | `amelie.2001` | MATCH (diacritics folded) |
| 18 | `Léon: The Professional (1994)` | `leon.the.professional.1994` | MATCH (diacritics folded) |
| 19 | `naïve.error.1993` | `Naive Error (1993)` | MATCH (diacritics + case folded) |

## 9. Open Questions

1. Should episode/season identifiers (`s01e02`, `ep02`) be treated as
   significant tokens (default: yes) or junk (configurable)?
2. Should the release-group heuristic (FR-4) be on by default?
3. Do we need to deduplicate identical names *within* a single directory,
   or only across directories?
4. Is the v1 Latin-only folding table acceptable, or is full NFKD
   (ICU backend) required from the start?
