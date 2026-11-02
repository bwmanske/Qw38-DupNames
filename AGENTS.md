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

- `%APPDATA%\DupNames\DupNames.ini` (i.e. `C:\Users\<user>\AppData\Roaming\DupNames\DupNames.ini`; default, overridable with `--ini FILE`). Sections: `[InitState]` (13 startup/matching/scanning options: `MatchThreshold`, `CloseThreshold`, `MergeClose`, `YearLo`, `YearHi`, `WYear`, `WTokens`, `YearCap`, `Recursive`, `SkipHidden`, `Include`, `Exclude`, `Junk`), `[PathList]` (`ProtectedPathN=<path>` never delete, `CommonPathN=<path>` deletable; local and UNC paths), and `[Window]` (window `X`,`Y`,`W`,`H` + `Maximized`, column widths `DirCol0-1`, `QueueCol0-6`; saved on exit and on add/remove/type change, restored on startup with an off-screen guard). CLI `--match`/`--close` override the INI and are written back to it (the `--ini` file if given, else the default INI). The path list is loaded on startup and saved on exit; `[InitState]` is loaded on startup and saved on exit and on Options-OK. See `docs/USER_GUIDE.md` and PLAN.md Phase 8.

## Status

- P0 scaffold done: configures, builds clean, smoke test passes, GUI launches.
- P1 matching core done: normalization, folding, scoring, clustering; 37 test cases green.
- P2 scanner done: `dn::scan()` (recursive/flat, include/exclude globs, hidden skip, progress callback) in `src/scanner.cpp`; 52 test cases green.
- P3 GUI queue (M1) done: `src/gui/app.cpp` — in-memory dir list (add/remove via folder dialog), Scan button, tree-view queue of MATCH clusters; `dn::match()` pipeline (normalize→block→score→cluster) in `src/match.cpp`; 57 test cases green.
- P4 lists/INI done: `dn::ini` (Win32 INI I/O, default path, load/save `[InitState]`+`[PathList]`, `resolve_config` precedence) in `src/ini.cpp`; `dn::parse_cli` (`--ini`/`--match`/`--close`) in `src/cli.cpp`; GUI loads the path list on start, saves on exit, applies thresholds; 79 test cases green.
- P5 options done: `src/gui/options.cpp` — modal Options dialog (Matching/Scanning/Junk groups, validated, saved to INI on OK); `dn::ini` extended with `ini_get/set_bool`/`ini_get/set_int`/`ini_has_key` and full 13-key `[InitState]` load/save (presence-detected, so an empty `Junk=` overrides to no tokens); 85 test cases green.
  - **Known issue (tabled, P7):** changing a value in the dialog and clicking OK reports "Options saved." and calls `ini_save_state`, but the INI can retain the prior/default value (repro: set `MatchThreshold` to 0.99, OK → INI still 0.85). The core flow is verified correct by a throwaway repro (resolve→edit→`ini_save_state`→raw-file read-back all persist the new value), so the defect is in the live dialog interaction, which is not headlessly testable. Root cause not pinned down; investigate before relying on Options persistence.
- P6 deletion done: `plan_deletion` (keep-one rule, protected skip) in `src/deletion.cpp`; GUI: TVS_CHECKBOXES tree, Delete button, confirmation dialog, Recycle Bin (local) / `fs::remove` (UNC); 93 test cases green.
- P7 hardening done: 106 test cases / 342 assertions green (Release + ASan Debug).
  - **7.1 Determinism** (`tests/test_determinism.cpp`): pipeline run 6×, canonical serialization byte-compared (NFR-2).
  - **7.2 Performance** (`tests/test_perf.cpp`): 10,000 synthetic names in ~2 s Release (NFR-1); test is sanitizer-aware (skips the 60 s bound under ASan).
  - **7.3 Sanitizer**: MSVC ASan Debug build (`/fsanitize=address /EHsc`, needs `clang_rt.asan_dbg_dynamic-x86_64.dll` on PATH) — full suite clean, no memory errors. UBSan is not available on MSVC.
  - **7.4 Cross-compiler**: all three verified — MSVC (`cl.exe`), Clang (`clang-cl` 22.1.3, Ninja; CMake auto-detects MSVC-mode), GCC (MinGW-w64 `g++` 16.2.0, Ninja). Full solution + all 106 tests green on each. Only cross-compiler fix: `-municode` on the `DupNames` link line for non-MSVC (selects the `wWinMain` wide entry point). See `docs/SPEC_AUDIT.md`.
  - **7.5 Edge-case sweep** (`tests/test_edge.cpp`): empty-after-normalization, invalid UTF-8, year-only, >200-char (32-token cap), symlinks/broken links, permission tolerance. **Found + fixed 2 bugs:** names that normalize to zero tokens (all-junk, year-only) were treated as identical to each other (`token_score` returned 1.0 for two empty token lists) and falsely clustered; now skipped from matching per SPEC §14 (`score_pair` returns NoMatch, `candidate_pairs` doesn't bucket them). Also implemented the SPEC §14 32-token scoring cap in `normalize()`.
  - **7.6 Spec audit** (`docs/SPEC_AUDIT.md`): every FR-1…FR-9 + NFR-1…NFR-5 mapped to a passing test, code inspection, obsolete-by-GUI-pivot (FR-8/FR-9), or documented manual check (NFR-4 GCC/Clang). No open items.
- P8 GUI polish done: 107 test cases / 346 assertions green (Release).
  - **8.1 Flat list-view queue**: the Matches queue is now a flat `SysListView32` (one row per cluster member; checkbox, File Name, Folder, Drive, Group, Size, File modified columns) instead of the P3/P6 tree view; alternating per-group shading — even group numbers get a pale blue `RGB(216,233,244)` band via `NM_CUSTOMDRAW` (`clrTextBk`, not `clrBackground`, which is not a member of `NMLVCUSTOMDRAW`).
  - **8.2 Type column**: the directory list has Path + Type columns; clicking the Type cell toggles a folder Common↔Protected (persisted immediately).
  - **8.3 Date fix**: `kWindowsToUnixTicks` in `src/scanner.cpp` was `11824452000000000LL` (a ~2007 offset, so file dates showed ~37 years in the future, 2340–2360); corrected to `116444736000000000LL` (11644473600 s × 10⁷). Regression test in `tests/test_scanner.cpp` (FILETIME→epoch via `SetFileTime`).
  - **8.4 Window/column persistence**: INI `[Window]` section (`X`,`Y`,`W`,`H`,`Maximized`,`DirCol0-1`,`QueueCol0-5`) via the low-level `ini_get/set_int`/`ini_get/set_bool`/`ini_has_key` (no `dn::ini` core changes). Restored on startup with an off-screen guard (≥100×100 overlap with the virtual screen, min 320×200, else 900×640/`CW_USEDEFAULT`); saved on `WM_CLOSE`, **not** `WM_DESTROY` (Windows destroys child windows before `WM_DESTROY`, so the `ListView_GetColumnWidth` reads must run while children are alive).
- P9 matches context menu + protected-row checkboxes done: 107 test cases / 346 assertions green (Release).
  - **9.1 No checkbox on protected rows**: `PopulateQueue` tracks `row_protected_` per row (from `FileEntry::protected_`, set by the scanner from `dir.protected_`); `NM_CUSTOMDRAW` at `CDDS_ITEMPOSTPAINT` fills the checkbox cell with the row's band color (even group `RGB(216,233,244)`, odd white), cell width `MulDiv(16, dpi, 96)` with `dpi = GetDeviceCaps(cd->nmcd.hdc, LOGPIXELSX)` (the HDC is on the `NMCUSTOMDRAW` base, not on `NMLVCUSTOMDRAW`). `plan_deletion` still skips protected entries as a backstop. *(The POSTPAINT stage was never actually delivered — see P10.1 for the fix.)*
  - **9.2 Matches context menu**: `NM_RCLICK` → `ListView_SubItemHitTest` (cursor screen→client) → `CreatePopupMenu` + **Open File** / **Open Folder in Explorer** / **Rename** (IDs 2001–2003, routed in `WM_COMMAND` before the `BN_CLICKED` check) → `TrackPopupMenu(TPM_RIGHTBUTTON, …)`. Open File = `ShellExecuteW(L"open", path)` (HINSTANCE ≤ 32 → status error); Open Folder = `explorer.exe /select,"<fullpath>"` (folder opens with the file selected).
  - **9.3 Rename dialog**: `src/gui/rename.{hpp,cpp}` — modal dialog (Options-dialog pattern), edit pre-filled with the current name, base name without extension pre-selected via `EM_SETSEL`. Validation: non-empty, no `\/:*?"<>|`, target must not exist; then `std::filesystem::rename` (the `error_code` overload returns **void** — check `ec`), update `entries_[idx].path`/`.name` + the row's File Name cell. Match results are stale for the renamed file until the next scan (accepted). GUI-only, not headlessly testable.
- P10 protected-checkbox fix + Score column done: 110 test cases / 358 assertions green (Release).
  - **10.1 Protected-checkbox fix**: the P9 cover-paint never ran because `CDDS_ITEMPOSTPAINT` is **only delivered if `CDRF_NOTIFYSUBITEMDRAW` (0x1000) was returned from `CDDS_ITEMPREPAINT`** — the P9 code returned `CDRF_NEWFONT`/`CDRF_DODEFAULT` there. Now `CDDS_ITEMPREPAINT` returns `CDRF_NOTIFYSUBITEMDRAW | CDRF_NEWFONT` (banded rows) / `CDRF_NOTIFYSUBITEMDRAW | CDRF_DODEFAULT` (plain rows). POSTPAINT is then sent **once per subitem**, so the cover-paint is guarded on `cd->iSubItem == 0` (the checkbox lives in subitem 0). Cover rect: row left edge → `LVIR_LABEL` left edge (the first column's text starts just past the checkbox), falling back to the DPI-scaled 16 px cell width if the label rect is unavailable.
  - **10.2 Score column**: new `dn::member_scores(entries, cluster, cfg)` in `src/match.cpp` — similarity of each member to the cluster's anchor (`Cluster::anchor` = `members.front()`), via `normalize` + `score_pair(...).score`; the anchor's own score is 1.0 (special-cased, since `score_pair` returns NoMatch for zero-token names). GUI: "Score" appended as Matches column 6 (integer percentage, e.g. `92%`, via `format_score_pct` = `std::lround(score*100)`); `queue_col_widths_` is now 7 entries (default 60 px) and `[Window]` persists `QueueCol6` (existing `QueueCol0-5` keys stay valid). 3 new test cases in `tests/test_match.cpp`.
