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

// Enumerate the key names present in a section, in file order. Empty if the
// section or file is missing. Returns exactly the keys that exist, so
// non-contiguous numbering (e.g. ProtectedPath1, ProtectedPath3) is preserved.
std::vector<std::wstring> ini_section_keys(const std::wstring& file,
                                           const std::wstring& section);

// Default INI location: %APPDATA%\DupNames\DupNames.ini
// (i.e. C:\Users\<user>\AppData\Roaming\DupNames\DupNames.ini). Pure: computes
// the path without creating anything.
std::wstring default_ini_path();

// ---------------------------------------------------------------------------
// High-level load/save for the two INI sections the app uses.
// ---------------------------------------------------------------------------

// Load [InitState] into cfg. Only keys actually present in the INI override
// cfg; absent keys leave cfg untouched (so built-in defaults survive).
void ini_load_state(const std::wstring& file, Config& cfg);

// Save [InitState] from cfg (MatchThreshold, CloseThreshold).
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
