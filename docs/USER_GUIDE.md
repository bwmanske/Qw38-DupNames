# DupNames — User Guide

**Version:** 0.1.0
**Platform:** Windows only

DupNames is a Win32 desktop app that scans local and network directories for
similar file names (fuzzy matching), shows a queue of matched groups, and can
delete duplicates from "common" (non-protected) directories.

> **Status:** the core product is implemented — scanning, matching, the
> match queue, duplicate deletion, options, and INI persistence all work.
> See [What's implemented](#whats-implemented) for the current state.

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

The main window (titled **DupNames**, 900 x 640 by default) has three parts.
Its size, position, and column widths are remembered between runs (see
[Configuration and user data](#configuration-and-user-data)).

- **Directories** (top): a list of the folders to scan, with a **Path** column
  and a **Type** column (`Common` = deletable, `Protected` = never delete).
  Use **Add...** to pick a folder (a folder browser opens) and **Remove** to
  drop the selected one. **Clicking the Type cell of a row toggles it**
  between Common and Protected. The list is loaded from the INI `[PathList]`
  on startup and saved back on exit — and immediately on add, remove, or type
  change (see [Configuration and user data](#configuration-and-user-data)).
  Folders added with **Add...** are saved as common (deletable) paths.
- **Scan** (top-right): scans the listed directories, finds similar file
  names, and fills the queue below. The status line reports how many files
  were scanned and how many match groups were found.
- **Options...** (top-right): opens the options dialog to change the matching
  and scanning settings (thresholds, year range, weights, recursive/hidden,
  include/exclude globs, and the junk-token list). Changes are applied on **OK**
  and saved to the INI `[InitState]` immediately (see
  [Options dialog](#options-dialog)).
- **Delete** (top-right): deletes the checked files from the match queue
  (see [Deleting duplicates](#deleting-duplicates)).
- **Matches** (bottom): a flat list of match groups — one row per file, with
  a checkbox, **File Name**, **Folder**, **Drive** (`H:` or
  `\\server\share`), **Group** (the group number), **Size** (e.g. `1.37 GB`),
  **File modified** (e.g. `9/9/2026 10:03:26 PM`), and **Score** (e.g.
  `92%`). Rows of even group numbers are shaded pale blue so groups are easy
  to tell apart. **Score** shows how similar the file is to the group's
  anchor (the first file in the group, shown as `100%`); it is the same
  0–1 similarity the matcher uses, as a percentage. Check the files you want
  to delete. Files in **protected** directories have **no checkbox at all** —
  they can never be selected for deletion. **Right-clicking a row** opens a
  context menu that depends on the selection: one selected row gets
  **Open File**, **Open Folder in Explorer**, and **Rename**; two or more
  selected rows get **Copy Files to Clipboard** and **Copy CSV data to
  clipboard** (see [Context menu](#context-menu)).

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

### `[Window]` — window and layout

The main window's size, position, and column widths, written on exit (and on
add/remove/type changes) and read on startup. You normally don't edit these by
hand; they are listed here for completeness.

| Key | Meaning |
|---|---|
| `X`, `Y` | Top-left of the window's normal (restored) position. |
| `W`, `H` | Normal size in pixels. |
| `Maximized` | `true`/`false` — reopen maximized if the window was closed while maximized. |
| `DirCol0`, `DirCol1` | Directory-list column widths (Path, Type). |
| `QueueCol0` … `QueueCol6` | Matches-list column widths (File Name, Folder, Drive, Group, Size, File modified, Score). |

The window rect is restored only if it is sane (at least 320×200 and at least
partly on a connected monitor); otherwise the default 900×640 is used. Column
widths below 20 px are ignored in favor of the built-in defaults.

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

[Window]
X = 100
Y = 100
W = 900
H = 640
Maximized = false
DirCol0 = 600
DirCol1 = 100
QueueCol0 = 280
QueueCol1 = 290
QueueCol2 = 55
QueueCol3 = 50
QueueCol4 = 70
QueueCol5 = 130
QueueCol6 = 60
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

## Deleting duplicates

After a scan, the match queue shows each group with its member files. Each
member row has a checkbox. To delete duplicates:

1. **Scan** the directories.
2. In the **Matches** list, **check** the files you want deleted (typically
   the copies in common / non-protected directories).
3. Click **Delete**. A confirmation dialog lists every file that will be
   deleted (and how many protected files were skipped). Click **Yes** to
   proceed.
4. Deleted files are removed:
   - **Local paths** → sent to the Recycle Bin (recoverable).
   - **UNC / network paths** → permanently removed (no Recycle Bin over the
     network).
5. The queue updates: deleted rows are removed from the list. Groups that
   lose all but one member disappear; the remaining (protected) entries stay.
   Group numbers stay stable until the next scan.

**Safety rules** (enforced by `plan_deletion` in `dn_core`):

- Files in **protected** directories are never deleted, even if checked.
- At least one file per group is always kept (the "keep-one" rule): if a
  group has a protected member, all common copies may be deleted; if it has
  no protected member, one common copy (the anchor) is kept.
- Only files that are actually checked in the list are candidates for
  deletion.

## Context menu

Right-click a row in the **Matches** list to open a context menu. The menu
depends on the current selection:

- A right-click on a row that is **not** selected first selects just that
  row (like Windows Explorer), so it gets the single-file menu.
- **Exactly one row selected** — the single-file menu:
  - **Open File** — opens the file with its default application (e.g. a video
    player for a `.mkv`). If the file cannot be opened, the status line says
    so.
  - **Open Folder in Explorer** — opens the row's folder (the **Folder**
    column) in Windows Explorer with the file itself selected, so you can
    examine it alongside its neighbors.
  - **Rename** — opens a small dialog with the file's current name; the base
    name (without the extension) is pre-selected so you can just type a new
    name. The new name must not be empty, must not contain the characters
    `\ / : * ? " < > |`, and must not already exist in the same folder. On
    success the file is renamed in place and the row's **File Name** updates.
    The match results for the renamed file are stale until you scan again.
- **Two or more rows selected** — the copy menu:
  - **Copy Files to Clipboard** — copies the selected files' full paths to
    the clipboard, one per line. Paste them into Explorer's address bar, a
    file manager, or any text field.
  - **Copy CSV data to clipboard** — copies a header row plus one record per
    selected row with the seven visible columns (File Name, Folder, Drive,
    Group, Size, File modified, Score). Fields containing commas, quotes, or
    newlines are quoted per RFC 4180, so the result pastes cleanly into
    Excel, a spreadsheet, or a CSV file.

## What's implemented

| Area | State |
|---|---|
| Project build (CMake, MSVC) | Done |
| Name matching core (normalization, Unicode folding, year extraction, junk-token removal, fuzzy scoring, clustering) | Done, in the `dn_core` library, wired into the GUI |
| Test suite (110 test cases / 358 assertions, including all 19 spec acceptance cases) | Done, all passing |
| Directory scanning | Done (recursive/flat, include/exclude globs, hidden skip, real-world file dates) |
| GUI main window | Done — directory list (Path + Type columns), Scan button, match queue |
| Match queue in the GUI | Done — flat list of MATCH groups (one row per member) with checkboxes (no checkbox at all on protected rows), Drive/Group/Size/File-modified/Score columns, and alternating per-group shading |
| Matches context menu | Done — selection-dependent: one row → Open File (default app), Open Folder in Explorer (file selected), Rename; multiple rows → Copy Files to Clipboard (full paths), Copy CSV data to clipboard (header + 7 columns, RFC 4180) |
| Rename dialog | Done — modal dialog, base name pre-selected, validated (non-empty, legal characters, no existing target), renames in place |
| Directory type toggle | Done — click the Type cell to switch a folder between Common and Protected |
| Command line (`--ini` / `--match` / `--close`) | Done, with INI write-back |
| Path lists / INI persistence | Done — loaded on start, saved on exit (and on add/remove/type change); local + UNC paths |
| Window / column persistence | Done — window size, position, maximized state, and all column widths saved in the INI `[Window]` section |
| Options dialog | Done — matching/scanning/junk settings, validated, saved to INI on OK |
| Duplicate deletion | Done — check members in the queue, click Delete; confirmation dialog lists the files; Recycle Bin (local) or permanent remove (UNC) |

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
