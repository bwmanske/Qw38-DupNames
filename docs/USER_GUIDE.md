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

The main window (titled **DupNames**, 900 x 640) has three parts:

- **Directories** (top): a list of the folders to scan. Use **Add...** to pick
  a folder (a folder browser opens) and **Remove** to drop the selected one.
  The list is loaded from the INI `[PathList]` on startup and saved back on
  exit (see [Configuration and user data](#configuration-and-user-data)).
  Folders added with **Add...** are saved as common (deletable) paths.
- **Scan** (top-right): scans the listed directories, finds similar file
  names, and fills the queue below. The status line reports how many files
  were scanned and how many match groups were found.
- **Options...** (top-right): opens the options dialog to change the matching
  and scanning settings (thresholds, year range, weights, recursive/hidden,
  include/exclude globs, and the junk-token list). Changes are applied on **OK**
  and saved to the INI `[InitState]` immediately (see
  [Options dialog](#options-dialog)).
- **Matches** (bottom): a tree of match groups. Each top-level node is a group
  (the representative name plus a count); expand it to see every file in the
  group with its directory.

## Command line

```
DupNames.exe [--ini FILE] [--match VALUE] [--close VALUE]
```

| Option | Meaning |
|---|---|
| `--ini FILE` | Initialization INI file to load (see [Configuration and user data](#configuration-and-user-data)). If omitted, the default INI is used: `%APPDATA%\DupNames\DupNames.ini`. |
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

All persistent settings live in one INI file. Default location:

```
%APPDATA%\DupNames\DupNames.ini
```

(i.e. `C:\Users\<user>\AppData\Roaming\DupNames\DupNames.ini`). A different
file can be selected with `--ini FILE` on the command line. The file and its
sections are created on first save if they do not already exist.

### `[InitState]` — startup options

Options that control how the app matches and scans. All are editable in the
[Options dialog](#options-dialog); every key is written on save, so a fresh INI
contains the full set.

| Key | Default | Meaning |
|---|---|---|
| `MatchThreshold` | `0.85` | Score at or above which a pair is a MATCH (0–1). |
| `CloseThreshold` | `0.60` | Score at or above which (and below the match threshold) a pair is CLOSE (0–1). |
| `MergeClose` | `false` | When true, CLOSE pairs are merged into their MATCH group in the queue. |
| `YearLo` | `1900` | Lower bound of the recognized year range. |
| `YearHi` | `2099` | Upper bound of the recognized year range (must be > `YearLo`). |
| `WYear` | `0.3` | Weight of year agreement in the similarity score (0–1). |
| `WTokens` | `0.7` | Weight of token similarity in the similarity score (0–1). |
| `YearCap` | `0.5` | Maximum score a pair with a year mismatch can reach (0–1). |
| `Recursive` | `false` | When true, scan subdirectories as well as the listed folders. |
| `SkipHidden` | `true` | When true, skip hidden files and directories. |
| `Include` | `*` | Comma-separated glob(s) of file names to include. |
| `Exclude` | *(empty)* | Comma-separated glob(s) of file names to exclude. |
| `Junk` | `the,extended,1080p,x264,720p,bluray,directors` | Comma-separated junk tokens stripped before matching. |

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

## Options dialog

The **Options...** button opens a modal dialog with three groups:

- **Matching** — match/close thresholds, the "merge CLOSE" toggle, the year
  range (lo–hi), the year/token weights, and the year cap.
- **Scanning** — recursive and skip-hidden toggles, plus the include and
  exclude glob patterns.
- **Junk tokens** — a multi-line list (one token per line) of tokens to strip
  before matching; stored comma-separated in the INI `Junk` key.

Values are validated on **OK**: thresholds and weights must be numbers in
0–1, and the year lo must be less than the year hi. An invalid value shows a
warning and keeps the dialog open so you can correct it. **OK** applies the
changes and saves them to the INI immediately; **Cancel** (or closing the
dialog) discards them.

## What's implemented

| Area | State |
|---|---|
| Project build (CMake, MSVC) | Done |
| Name matching core (normalization, Unicode folding, year extraction, junk-token removal, fuzzy scoring, clustering) | Done, in the `dn_core` library, wired into the GUI |
| Test suite (85 test cases, including all 19 spec acceptance cases) | Done, all passing |
| Directory scanning | Done (recursive/flat, include/exclude globs, hidden skip) |
| GUI main window | Done — directory list, Scan button, match queue |
| Match queue in the GUI | Done — tree of MATCH groups, expandable to member files |
| Command line (`--ini` / `--match` / `--close`) | Done, with INI write-back |
| Path lists / INI persistence | Done — loaded on start, saved on exit; local + UNC paths |
| Options dialog | Done — matching/scanning/junk settings, validated, saved to INI on OK |
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
