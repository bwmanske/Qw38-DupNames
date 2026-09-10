# TESTING.md — Testing DupNames

How to build, run, and extend the DupNames test suite. For building the
project, see [BUILDING.md](BUILDING.md). The matching behavior under test is
specified in `specs/SPEC.md`.

## Overview

- **Framework:** [doctest](https://github.com/doctest/doctest) **2.4.11**,
  vendored as a single header at `third_party/doctest.h` (no install step).
- **Runner:** the `dn_tests` executable, built from `tests/*.cpp`.
- **Coverage:** 79 test cases (231 assertions), including all 19 acceptance
  cases from `specs/SPEC.md`.
- **Single implementation TU:** `tests/doctest_impl.cpp` defines
  `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`, which provides `main()`. Every other
  test file only `#include <doctest.h>` and declares `TEST_CASE`s.

## Build the tests

The tests build as part of the normal build (see [BUILDING.md](BUILDING.md)):

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

This produces `build\Release\dn_tests.exe`. To skip the tests entirely,
configure with `-DDN_TESTS=OFF`.

## Run all tests

Either through CTest (the registered test) or by running the runner directly:

```powershell
ctest --test-dir build -C Release
```

```powershell
.\build\Release\dn_tests.exe
```

A passing run ends with:

```
[doctest] test cases:  79 |  79 passed | 0 failed | 0 skipped
[doctest] assertions: 231 | 231 passed | 0 failed |
[doctest] Status: SUCCESS!
```

The process exit code is non-zero if any assertion fails, so both commands are
CI-friendly.

## Run a subset

Test cases are filtered by name with `-tc` (a.k.a. `--test-case`). Filters use
wildcards (`*` and `?`) and are comma-separated. This project has no
`TEST_SUITE`s; instead each test case name is prefixed with its area
(`cli:`, `ini:`, `scan:`, …), so the prefix is the practical way to group them:

```powershell
.\build\Release\dn_tests.exe -tc="cli:*"        # all 10 CLI tests
.\build\Release\dn_tests.exe -tc="ini:*"        # all 12 INI tests
.\build\Release\dn_tests.exe -tc="acceptance:*" # the 19 spec acceptance cases
.\build\Release\dn_tests.exe -tc="scan: recursive*"  # wildcard within a name
```

To exclude instead of include, use `-tce` (`--test-case-exclude`):

```powershell
.\build\Release\dn_tests.exe -tce="smoke:*"     # everything except the smoke test
```

> Every flag, option, and filter is also available with a `dt-` prefix
> (e.g. `-dt-tc="cli:*"`, `-dt-ltc`). The unprefixed forms shown here are the
> default.

## List tests and reporters

```powershell
.\build\Release\dn_tests.exe -ltc        # list all test cases by name
.\build\Release\dn_tests.exe -lts        # list test suites
.\build\Release\dn_tests.exe -lr         # list registered reporters
.\build\Release\dn_tests.exe -c          # print the number of matching tests
.\build\Release\dn_tests.exe -h          # full help (all flags/options/filters)
```

## Reporters and output

The default reporter is `console`. This doctest build also registers `xml` and
`junit`. Select one or more (comma-separated) with `-r` (`--reporters`) and
write to a file with `-o` (`--out`):

```powershell
.\build\Release\dn_tests.exe -r=junit -o=build\test-results.xml
.\build\Release\dn_tests.exe -r=xml   -o=build\test-results.xml
```

## Useful flags

| Flag | Long form | Effect |
|---|---|---|
| `-tc=<f>` | `--test-case=<f>` | Run only test cases matching `<f>`. |
| `-tce=<f>` | `--test-case-exclude=<f>` | Exclude test cases matching `<f>`. |
| `-sf=<f>` | `--source-file=<f>` | Filter by source file. |
| `-r=<f>` | `--reporters=<f>` | Reporters to use (default `console`). |
| `-o=<file>` | `--out=<file>` | Write reporter output to a file. |
| `-d` | `--duration` | Print the time taken by each test case. |
| `-s` | `--success` | Also report successful assertions. |
| `-aa=<n>` | `--abort-after=<n>` | Stop after `<n>` failed assertions. |
| `-m` | `--minimal` | Minimal console output (failures only). |
| `-q` | `--quiet` | No console output. |
| `-nc` | `--no-colors` | Disable colored output. |
| `-cs` | `--case-sensitive` | Treat filters as case-sensitive. |
| `-c` | `--count` | Print the number of matching tests and exit. |

## Test layout

| File | Covers |
|---|---|
| `tests/doctest_impl.cpp` | doctest implementation + `main()` (the only impl TU). |
| `tests/test_smoke.cpp` | Build sanity (version string is non-empty). |
| `tests/test_utf8.cpp` | UTF-8 decoding and codepoint folding. |
| `tests/test_normalize.cpp` | Name normalization (folding, tokenization, year extraction, junk removal). |
| `tests/test_compare.cpp` | Damerau–Levenshtein distance, token/year scoring, classification. |
| `tests/test_cluster.cpp` | Union-find clustering of MATCH/CLOSE pairs. |
| `tests/test_blocking.cpp` | Candidate-pair blocking by first token. |
| `tests/test_acceptance.cpp` | The 19 acceptance cases from `specs/SPEC.md` (section 8). |
| `tests/test_scanner.cpp` | `dn::scan()`, glob matching, hidden-file skip, progress callback. |
| `tests/test_match.cpp` | The full `dn::match()` pipeline (normalize → block → score → cluster). |
| `tests/test_ini.cpp` | INI I/O, default path, path-list load/save, `resolve_config` precedence + write-back. |
| `tests/test_cli.cpp` | `dn::parse_cli()` (`--ini` / `--match` / `--close`). |

## Writing a new test

1. Create `tests/test_<area>.cpp` (or add to an existing file). `#include
   <doctest.h>` and the relevant `dn` header, then declare `TEST_CASE`s:

   ```cpp
   #include <doctest.h>
   #include "dn/yourmodule.hpp"

   using namespace dn;

   TEST_CASE("yourarea: describe the behavior") {
       CHECK(your_function(...) == expected);
   }
   ```

2. Add the new file to the `dn_tests` target in `CMakeLists.txt` (the
   `add_executable(dn_tests ...)` list).
3. **Do not** add another implementation TU — `tests/doctest_impl.cpp` already
   provides `main()`. A second `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` breaks the
   link.
4. Rebuild and run:

   ```powershell
   cmake --build build --config Release
   .\build\Release\dn_tests.exe -tc="yourarea:*"
   ```

**Naming convention:** prefix each test case name with its area and a colon
(`"area: description"`), e.g. `"ini: UNC paths round-trip"`. This keeps the
`-tc="area:*"` filters meaningful.

## Troubleshooting

- **`dn_tests.exe` not found** — the tests were disabled. Reconfigure with
  `-DDN_TESTS=ON` (the default) and rebuild.
- **`LNK2019` / unresolved `main`** — a second doctest implementation TU was
  added. Keep only `tests/doctest_impl.cpp`.
- **`C5285` warning from `doctest.h`** — expected; suppressed for `dn_tests`
  only (`/wd5285`).
- **A filter matches nothing** — check the exact test case name with `-ltc`;
  filters are case-insensitive by default and use `*`/`?` wildcards.

## See also

- [BUILDING.md](BUILDING.md) — building the project.
- [README.md](../README.md) — project overview.
- `specs/SPEC.md` — the matching-algorithm specification (source of truth for
  the matching core).
