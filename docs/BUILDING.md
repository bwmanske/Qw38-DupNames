# BUILDING.md — Building DupNames

How to configure and build DupNames from source on Windows. For running and
writing the test suite, see [TESTING.md](TESTING.md). For the matching
algorithm, see `specs/SPEC.md`.

## Prerequisites

- **Windows** — the app is Win32-only and builds with the MSVC toolchain.
- **Visual Studio 2026 Community** with the MSVC C++ toolchain (MSVC 19.51)
  and the Windows SDK.
- **CMake ≥ 3.20** (the project is developed against 4.4.x).
- **Git** — to clone the repository.

> **Generator note:** Ninja is not part of the required toolchain. Always
> configure with the **`Visual Studio 18 2026`** generator. Do not use Ninja or
> an older Visual Studio generator.

## Quick start

From the repository root, in PowerShell:

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64   # configure (once)
cmake --build build --config Release               # build
```

The first command configures the project into the `build/` directory — run it
once, or again after changing `CMakeLists.txt`. The second builds the `Release`
configuration.

Artifacts land in `build\Release\`:

| Artifact | What it is |
|---|---|
| `DupNames.exe` | The GUI application (Win32, `WIN32` subsystem). |
| `dn_tests.exe` | The test runner (doctest). |
| `dn_core.lib` | The core logic static library (matching, scanning, persistence). |

## Build options

| CMake option | Default | Effect |
|---|---|---|
| `DN_TESTS` | `ON` | Build the `dn_tests` runner and register it with CTest. Pass `-DDN_TESTS=OFF` to skip the tests. |

Configure without tests:

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64 -DDN_TESTS=OFF
```

## Rebuilding and cleaning

- **Incremental rebuild** — re-run `cmake --build build --config Release`.
- **Clean** — `cmake --build build --target clean` removes build outputs but
  keeps the configured `build/` tree.
- **Full reset** — delete the `build/` directory and re-run the configure
  command. `build/` is git-ignored.

## Targets

| Target | Type | Links |
|---|---|---|
| `dn_core` | static library | — (all core logic: `src/*.cpp`, headers in `include/dn/`, namespace `dn`) |
| `doctest` | INTERFACE | — (vendored header `third_party/doctest.h`) |
| `DupNames` | executable (`WIN32`) | `dn_core comctl32 gdiplus shell32 ole32` |
| `dn_tests` | executable | `dn_core doctest` |

`dn_core` is shared by the app and the tests so both exercise exactly the same
logic. New logic belongs in `dn_core`, not in the GUI.

## Compiler settings

- **C++17** (`CMAKE_CXX_STANDARD 17`, extensions off).
- **MSVC flags:** `/W4 /permissive- /utf-8`. Source files are UTF-8.
- **`UNICODE` / `_UNICODE`** are defined globally for every target — the code
  uses the wide Win32 APIs only (`wWinMain`, `CreateWindowExW`, …). Never use
  the ANSI (`A`) variants.
- Non-MSVC compilers get `-Wall -Wextra -Werror`. The project is developed and
  supported on MSVC; other toolchains are best-effort.

## Troubleshooting

- **Generator / Ninja error** — you used the wrong generator. Use
  `-G "Visual Studio 18 2026"`.
- **Wrong configuration produced** — the VS generator is multi-config; always
  pass `--config Release` to `cmake --build` (and `-C Release` to `ctest`).
- **`LNK2019` / unresolved `main` in `dn_tests`** — doctest needs exactly one
  implementation translation unit: `tests/doctest_impl.cpp` (defines
  `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`). Do not add a second one.
- **`C5285` warning from `doctest.h`** — expected and intentionally suppressed
  for the `dn_tests` target only (`/wd5285`); it does not affect `dn_core` or
  `DupNames`.

## See also

- [README.md](../README.md) — project overview.
- [TESTING.md](TESTING.md) — running and writing tests.
- `specs/SPEC.md` — the matching-algorithm specification.
- `docs/IMPLEMENTATION.md` — the original CLI design (partially obsolete).
