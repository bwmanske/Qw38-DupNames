#pragma once
#include <string>
#include <vector>

namespace dn {

// Parsed command-line options. `args` is the command line split into tokens
// with the program name removed (see CommandLineToArgvW in the app).
//
//   DupNames.exe [--ini FILE] [--match VALUE] [--close VALUE]
//
// Precedence is applied by the caller: command line > INI > built-in default.
struct CliArgs {
    std::wstring ini_path;  // empty = use the default INI location
    bool has_match = false;
    double match = 0.0;
    bool has_close = false;
    double close = 0.0;
    bool error = false;      // set if any argument was unknown or malformed
    std::wstring error_msg;  // human-readable reason when error is true
};

// Parse `args` into CliArgs. Unknown options, a missing value, or a non-numeric
// threshold set error = true (with error_msg) and stop. Repeated options use
// the last value. An empty `args` yields a clean, all-default result.
CliArgs parse_cli(const std::vector<std::wstring>& args);

}  // namespace dn
