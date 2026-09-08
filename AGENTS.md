# AGENTS.md — DupNames

Windows-only Win32 GUI app (C++17, CMake). Scans local + network directories for
similar file names (fuzzy matching), shows a queue of matched groups, and deletes
duplicates from "common" (non-protected) directories.

## Build & test

`cmake`/`ctest` (4.4.x) and `git` (2.55.x) are on PATH. Build with the VS generator:

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64   # configure (once)
cmake --build build --config Release               # build
ctest  --test-dir build -C Release                 # run all tests
```

- Generator **must** be `Visual Studio 18 2026` (VS 2026 Community, MSVC 19.51).
  Ninja is not installed; do not use an older VS generator.
- The VS generator is multi-config: `--config Release` is **required** on build/test.
- Artifacts land in `build\Release\`: `DupNames.exe` (GUI), `dn_tests.exe` (tests), `dn_core.lib`.
- Run one doctest case: `build\Release\dn_tests.exe -tc="<test case name>"` (wildcards ok); `-ts="<suite>"` for a suite.

## Layout / boundaries

- **`dn_core`** (static lib) = ALL logic: matching, scanning, persistence. `src/*.cpp` + `include/dn/*.hpp`, namespace `dn`. Shared by the app and the tests — put new logic here, not in the GUI.
- **`DupNames`** (exe, `WIN32` GUI subsystem) = `src/main.cpp` + `src/gui/*`. Links `dn_core comctl32 gdiplus shell32 ole32`.
- **`dn_tests`** (exe) = `tests/*.cpp`. Links `dn_core` + doctest.
- `third_party/doctest.h` = vendored doctest (header-only).
- `tools/gen_fold_table/` = one-off codegen for the Latin fold table (emits `include/dn/fold_table.hpp`); not yet implemented.

## Conventions / gotchas

- **Wide Win32 APIs only** (`wWinMain`, `CreateWindowExW`, `LoadCursorW`, …). `UNICODE`/`_UNICODE` are defined globally in CMake. Never use the ANSI (`A`) variants.
- MSVC flags: `/W4 /permissive- /utf-8`. Source files are UTF-8.
- `protected` is a C++ keyword → the member is `protected_` (see `include/dn/types.hpp`).
- **doctest needs exactly one impl TU**: `tests/doctest_impl.cpp` defines `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` (provides `main`). To add a test, create `tests/test_foo.cpp` and add it to the `dn_tests` target in `CMakeLists.txt` — do **not** add another impl TU. The `C5285` warning from doctest.h is intentionally suppressed (`/wd5285`).

## Docs

- `specs/SPEC.md` — the matching-algorithm spec (folding, year/junk extraction, Damerau scoring, clustering, 19 acceptance cases). Source of truth for the matching core.
- `docs/PLAN.md` + `docs/IMPLEMENTATION.md` — describe the ORIGINAL **CLI** design. The CLI-arg / report-file / "read-only, never delete" parts are **OBSOLETE** (the project pivoted to a Win32 GUI with deletion). The matching-algorithm sections are still valid.

## User data

- `%APPDATA%\DupNames\DupNames.ini` (i.e. `C:\Users\<user>\AppData\Roaming\DupNames\DupNames.ini`; default, overridable with `--ini FILE`). Sections: `[InitState]` (startup options; first two: `MatchThreshold`, `CloseThreshold`) and `[PathList]` (`ProtectedPathN=<path>` never delete, `CommonPathN=<path>` deletable; local and UNC paths). CLI `--match`/`--close` override the INI and are written back to it (the `--ini` file if given, else the default INI). The path list is loaded on startup and saved on exit. See `docs/USER_GUIDE.md` and PLAN.md Phase 8.

## Status

- P0 scaffold done: configures, builds clean, smoke test passes, GUI launches.
- P1 matching core done: normalization, folding, scoring, clustering; 37 test cases green.
- P2 scanner done: `dn::scan()` (recursive/flat, include/exclude globs, hidden skip, progress callback) in `src/scanner.cpp`; 52 test cases green.
- P3 GUI queue (M1) done: `src/gui/app.cpp` — in-memory dir list (add/remove via folder dialog), Scan button, tree-view queue of MATCH clusters; `dn::match()` pipeline (normalize→block→score→cluster) in `src/match.cpp`; 57 test cases green.
- P4 lists/INI done: `dn::ini` (Win32 INI I/O, default path, load/save `[InitState]`+`[PathList]`, `resolve_config` precedence) in `src/ini.cpp`; `dn::parse_cli` (`--ini`/`--match`/`--close`) in `src/cli.cpp`; GUI loads the path list on start, saves on exit, applies thresholds; 79 test cases green.
- Pending: P5 options → P6 deletion → P7 hardening.
