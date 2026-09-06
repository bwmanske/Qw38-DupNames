# DupNames — User Guide

**Version:** 0.1.0
**Platform:** Windows only

DupNames is a Win32 desktop app that scans local and network directories for
similar file names (fuzzy matching), shows a queue of matched groups, and can
delete duplicates from "common" (non-protected) directories.

> **Status: work in progress.** This guide describes the app as it exists
> today. Most of the product's features are not yet implemented; see
> [What's implemented](#whats-implemented) for the current state.

---

## Requirements

- Windows 10/11
- To build from source: Visual Studio 2026 (MSVC 19.51) and CMake 4.4.x

## Building from source

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64   # configure (once)
cmake --build build --config Release               # build
```

Artifacts in `build\Release\`:

| File | Purpose |
|---|---|
| `DupNames.exe` | the GUI app |
| `dn_tests.exe` | test runner (development) |
| `dn_core.lib` | core logic library (development) |

## Running the app

Run `build\Release\DupNames.exe` (double-click or from a terminal).

The app opens its main window, titled **DupNames** (900 x 640). That is all
it does today — the window is an empty placeholder. There are no menus,
buttons, or settings yet.

## Command line

Command-line options are **planned** (not yet implemented — see
[What's implemented](#whats-implemented)). Planned syntax:

```
DupNames.exe [--ini FILE] [--match VALUE] [--close VALUE]
```

| Option | Meaning |
|---|---|
| `--ini FILE` | Initialization INI file to load (see [Configuration and user data](#configuration-and-user-data)). If omitted, the default INI is used: `%AppData%\Roaming\DupNames\DupNames.ini`. |
| `--match VALUE` | Match threshold, a number between 0 and 1 (default `0.85`). Pairs scoring at or above this are classified MATCH. |
| `--close VALUE` | Close threshold, a number between 0 and 1 (default `0.60`). Pairs scoring at or above this, but below the match threshold, are classified CLOSE. |

**Write-back rule:** `--match` and `--close` are not only used for the
current run — they are saved into the INI file under `[InitState]`:

- If `--ini FILE` was given on the command line, the values are written to
  `FILE`.
- If no `--ini` was given, the values are written to the default INI file.

The INI file (and its sections) is created if it does not already exist.

Precedence when the same option is available in more than one place:
**command line > INI file > built-in default.**

(The project's original command-line design — `--recursive`, `--config`,
`--include`, report files — is obsolete. See `docs/IMPLEMENTATION.md` for
that design and `AGENTS.md` for the note on the pivot to the GUI.)

## Configuration and user data

**Planned** (not yet implemented). All persistent settings live in one INI
file. Default location:

```
%AppData%\Roaming\DupNames\DupNames.ini
```

A different file can be selected with `--ini FILE` on the command line.

### `[InitState]` — startup options

Options that control how the app starts up and matches. The first two:

| Key | Default | Meaning |
|---|---|---|
| `MatchThreshold` | `0.85` | Score at or above which a pair is a MATCH. |
| `CloseThreshold` | `0.60` | Score at or above which (and below the match threshold) a pair is CLOSE. |

`[InitState]` is the general home for startup options; further options may be
added to this section over time.

### `[PathList]` — directories to scan

Numbered keys, `1` to `n`:

| Key pattern | Meaning |
|---|---|
| `ProtectedPath1` … `ProtectedPathN` | Directories that are **never** deleted from. |
| `CommonPath1` … `CommonPathN` | Directories from which duplicates **may** be deleted. |

Both protected and common paths may reference local drives
(`D:\Media\Movies`) or network / UNC shares (`\\fileserver\share\inbox`).

### Example INI file

```ini
[InitState]
MatchThreshold = 0.85
CloseThreshold = 0.60

[PathList]
ProtectedPath1 = D:\Media\Movies
ProtectedPath2 = \\fileserver\archive\protected
CommonPath1 = E:\Downloads
CommonPath2 = \\fileserver\share\inbox
```

## What's implemented

| Area | State |
|---|---|
| Project build (CMake, MSVC) | Done |
| Name matching core (normalization, Unicode folding, year extraction, junk-token removal, fuzzy scoring, clustering) | Done, in the `dn_core` library — **not yet visible in the GUI** |
| Test suite (37 test cases, including all 19 spec acceptance cases) | Done, all passing |
| GUI main window | Empty placeholder |
| Directory scanning | Not started |
| Match queue in the GUI | Not started |
| Path lists / INI persistence | Not started |
| Options dialog | Not started |
| Duplicate deletion | Not started |

## How matching will work (preview)

File names are normalized (diacritics and case folded, tokens split, a year
extracted, junk tokens like `1080p`/`x264`/`extended` removed) and compared
with a weighted similarity score combining token similarity (transposition
tolerant) and year agreement. Examples that will match:

- `The Matrix (1999)` ↔ `the.matrix.1999.extended.1080p.x264`
- `The Godfather (1972)` ↔ `thegodfater.1972` (typo tolerated)
- `Amélie (2001)` ↔ `amelie.2001` (diacritics ignored)

Names with different years (e.g. `1982` vs `2017`) are capped as non-matches.
Full details: `specs/SPEC.md`.

## More documentation

- `specs/SPEC.md` — the matching-algorithm specification (source of truth).
- `AGENTS.md` — project layout, build notes, and status.
- `docs/PLAN.md`, `docs/IMPLEMENTATION.md` — original CLI design (partially
  obsolete; the matching sections are still valid).
