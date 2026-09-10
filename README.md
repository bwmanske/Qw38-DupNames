# DupNames

Windows-only Win32 GUI app (C++17, CMake) that scans local and network
directories for similar file names (fuzzy matching), shows a queue of matched
groups, and deletes duplicates from "common" (non-protected) directories.

## Requirements

- Windows
- Visual Studio 2026 (MSVC 19.51)
- CMake 4.4.x

## Build

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64   # configure (once)
cmake --build build --config Release               # build
```

See [docs/BUILDING.md](docs/BUILDING.md) for prerequisites, build options, and
troubleshooting.

Artifacts land in `build\Release\`:

- `DupNames.exe` — the GUI app
- `dn_tests.exe` — test runner
- `dn_core.lib` — core logic library

## Tests

```powershell
ctest --test-dir build -C Release
```

Run a single doctest case:

```powershell
build\Release\dn_tests.exe -tc="<test case name>"
```

See [docs/TESTING.md](docs/TESTING.md) for running subsets, reporters, and
writing new tests.

## Project layout

```
CMakeLists.txt        top-level build
include/dn/           public headers (namespace dn)
src/                  core logic (dn_core) + GUI (DupNames)
tests/                doctest test cases
third_party/doctest.h vendored doctest (header-only)
specs/SPEC.md         matching-algorithm specification
docs/                 BUILDING.md, TESTING.md, USER_GUIDE.md, and the
                      original CLI design docs (partially obsolete)
tools/                one-off codegen utilities
```

## How it works

File names are normalized (Unicode folding, casefolding, tokenization, year
extraction, junk-token removal) and compared with a weighted similarity score
combining token similarity (Damerau–Levenshtein based) and year agreement.
Pairs above the match threshold are clustered (union-find) and shown in the
GUI queue. See `specs/SPEC.md` for details.

## User data

Configuration and path lists are stored in
`%AppData%\Roaming\DupNames\` as an INI file.
