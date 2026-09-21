#pragma once
#include <string>
#include <vector>

#include "dn/cli.hpp"
#include "dn/types.hpp"

namespace dn {

// ---------------------------------------------------------------------------
// Low-level INI I/O, backed by the Win32 GetPrivateProfileStringW /
// WritePrivateProfileStringW / GetPrivateProfileSectionW APIs. All paths and
// strings are wide. A missing file/section/key reads as the supplied default
// (never an error); writes create the file and its parent directory if absent.
// ---------------------------------------------------------------------------

// Read a string key; returns `def` if the file/section/key is missing.
std::wstring ini_get_string(const std::wstring& file, const std::wstring& section,
                            const std::wstring& key, const std::wstring& def = L"");

// Write a string key (creates the file and parent directory if missing).
void ini_set_string(const std::wstring& file, const std::wstring& section,
                    const std::wstring& key, const std::wstring& value);

// Read a double key; returns `def` if missing or not parseable as a number.
double ini_get_double(const std::wstring& file, const std::wstring& section,
                      const std::wstring& key, double def);

// Write a double key (shortest round-tripping decimal form).
void ini_set_double(const std::wstring& file, const std::wstring& section,
                    const std::wstring& key, double value);

// Read a boolean key; accepts "true"/"false"/"1"/"0" (case-insensitive).
// Returns `def` if the key is missing or not one of those values.
bool ini_get_bool(const std::wstring& file, const std::wstring& section,
                  const std::wstring& key, bool def);

// Write a boolean key as "true" or "false".
void ini_set_bool(const std::wstring& file, const std::wstring& section,
                  const std::wstring& key, bool value);

// Read an integer key; returns `def` if missing or not parseable as an int.
int ini_get_int(const std::wstring& file, const std::wstring& section,
                const std::wstring& key, int def);

// Write an integer key.
void ini_set_int(const std::wstring& file, const std::wstring& section,
                 const std::wstring& key, int value);

// Enumerate the key names present in a section, in file order. Empty if the
// section or file is missing. Returns exactly the keys that exist, so
// non-contiguous numbering (e.g. ProtectedPath1, ProtectedPath3) is preserved.
std::vector<std::wstring> ini_section_keys(const std::wstring& file,
                                           const std::wstring& section);

// True if `key` exists in `section`, even if its value is empty. Unlike
// ini_get_string (which reads an empty value and a missing key identically),
// this distinguishes "present but empty" from "absent".
bool ini_has_key(const std::wstring& file, const std::wstring& section,
                 const std::wstring& key);

// Default INI location: %APPDATA%\DupNames\DupNames.ini
// (i.e. C:\Users\<user>\AppData\Roaming\DupNames\DupNames.ini). Pure: computes
// the path without creating anything.
std::wstring default_ini_path();

// ---------------------------------------------------------------------------
// High-level load/save for the two INI sections the app uses.
// ---------------------------------------------------------------------------

// Load [InitState] into cfg. Every option key (thresholds, scan options,
// advanced matching params, junk list) is read; only keys actually present in
// the INI override cfg, so absent keys leave cfg untouched (built-in defaults
// survive).
void ini_load_state(const std::wstring& file, Config& cfg);

// Save [InitState] from cfg: every option key (MatchThreshold, CloseThreshold,
// MergeClose, YearLo, YearHi, WYear, WTokens, YearCap, Recursive, SkipHidden,
// Include, Exclude, Junk).
void ini_save_state(const std::wstring& file, const Config& cfg);

// Resolve the effective config by precedence: built-in default <- INI
// [InitState] <- command line. Any CLI-provided threshold is written back to
// the INI in use (creating the file/section if missing); INI-only values are
// left as-is. Returns the effective config.
Config resolve_config(const std::wstring& ini_path, const CliArgs& cli);

// Load [PathList] into a vector of DirEntry. ProtectedPathN -> protected_ =
// true, CommonPathN -> protected_ = false. Protected entries come first, then
// common, each ordered by their numeric suffix. Local and UNC paths are both
// accepted.
std::vector<DirEntry> ini_load_paths(const std::wstring& file);

// Save [PathList] from a vector of DirEntry. Renumbered 1..N per category
// (ProtectedPath1..N, CommonPath1..N) and any stale keys in the section are
// removed first.
void ini_save_paths(const std::wstring& file, const std::vector<DirEntry>& dirs);

}  // namespace dn
