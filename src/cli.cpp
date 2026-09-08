#include "dn/cli.hpp"

#include <cmath>
#include <cstddef>

namespace dn {

namespace {

// Strict whole-string decimal parse. Rejects empty input, trailing junk, and
// non-finite values (inf/nan).
bool parse_double(const std::wstring& s, double& out) {
    if (s.empty()) return false;
    try {
        std::size_t pos = 0;
        out = std::stod(s, &pos);
        if (pos != s.size()) return false;
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

}  // namespace

CliArgs parse_cli(const std::vector<std::wstring>& args) {
    CliArgs out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::wstring& a = args[i];
        if (a == L"--ini") {
            if (i + 1 >= args.size()) {
                out.error = true;
                out.error_msg = L"--ini requires a FILE argument";
                return out;
            }
            out.ini_path = args[++i];
        } else if (a == L"--match") {
            if (i + 1 >= args.size()) {
                out.error = true;
                out.error_msg = L"--match requires a VALUE argument";
                return out;
            }
            if (!parse_double(args[++i], out.match)) {
                out.error = true;
                out.error_msg = L"--match VALUE must be a number";
                return out;
            }
            out.has_match = true;
        } else if (a == L"--close") {
            if (i + 1 >= args.size()) {
                out.error = true;
                out.error_msg = L"--close requires a VALUE argument";
                return out;
            }
            if (!parse_double(args[++i], out.close)) {
                out.error = true;
                out.error_msg = L"--close VALUE must be a number";
                return out;
            }
            out.has_close = true;
        } else {
            out.error = true;
            out.error_msg = L"Unknown argument: " + a;
            return out;
        }
    }
    return out;
}

}  // namespace dn
