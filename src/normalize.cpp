#include "dn/normalize.hpp"

#include <cctype>
#include <string>
#include <vector>

#include "dn/utf8.hpp"

namespace dn {

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alpha_lower(char c) { return c >= 'a' && c <= 'z'; }
// A significant byte after folding: ASCII lowercase letter or digit.
bool is_significant(char c) { return is_alpha_lower(c) || is_digit(c); }

bool in_year_range(int v, int lo, int hi) { return v >= lo && v <= hi; }

// True if the whole string is exactly 4 digits forming a year in [lo, hi].
bool is_year_token(const std::string& s, int lo, int hi) {
    if (s.size() != 4) return false;
    for (char c : s)
        if (!is_digit(c)) return false;
    int v = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    return in_year_range(v, lo, hi);
}

// Strip the file extension (last dot-segment), unless that segment is itself a
// 4-digit year in range (in which case it is the year, not an extension).
std::string strip_extension(const std::string& name, int lo, int hi) {
    const std::size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return name;
    const std::string ext = name.substr(dot + 1);
    if (is_year_token(ext, lo, hi)) return name;  // protect the year
    return name.substr(0, dot);
}

// Split the folded stem into tokens on runs of non-significant bytes.
std::vector<std::string> tokenize(const std::string& folded) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : folded) {
        if (is_significant(c)) {
            cur += c;
        } else if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// Split common glued prefixes (e.g. "the" in "thegodfater") so that
// uncurated names without separators still tokenize correctly (SPEC §5).
void split_glued_prefixes(std::vector<std::string>& tokens) {
    static const char* const prefixes[] = {"the"};
    std::vector<std::string> out;
    out.reserve(tokens.size() * 2);
    for (auto& tok : tokens) {
        bool split = false;
        for (const char* p : prefixes) {
            const std::size_t plen = std::char_traits<char>::length(p);
            if (tok.size() >= plen + 2 && tok.compare(0, plen, p) == 0) {
                out.push_back(p);
                out.push_back(tok.substr(plen));
                split = true;
                break;
            }
        }
        if (!split) out.push_back(std::move(tok));
    }
    tokens = std::move(out);
}

// Find the first maximal digit run in `token` that is exactly 4 digits and in
// [lo, hi]. On success set prefix/suffix (the text before/after the run) and
// year, and return true.
bool find_year_run(const std::string& token, int lo, int hi, std::string& prefix,
                   int& year, std::string& suffix) {
    std::size_t i = 0;
    const std::size_t n = token.size();
    while (i < n) {
        if (!is_digit(token[i])) {
            ++i;
            continue;
        }
        const std::size_t start = i;
        while (i < n && is_digit(token[i])) ++i;
        const std::size_t len = i - start;
        if (len == 4) {
            const int v = (token[start] - '0') * 1000 + (token[start + 1] - '0') * 100 +
                          (token[start + 2] - '0') * 10 + (token[start + 3] - '0');
            if (in_year_range(v, lo, hi)) {
                prefix = token.substr(0, start);
                suffix = token.substr(start + 4);
                year = v;
                return true;
            }
        }
    }
    return false;
}

bool is_junk(const std::string& token, const std::vector<std::string>& junk) {
    for (const std::string& j : junk)
        if (j == token) return true;
    return false;
}

}  // namespace

NormalizedName normalize(const std::string& raw_name, const Config& cfg) {
    NormalizedName result;
    result.raw = raw_name;

    const std::string stem = strip_extension(raw_name, cfg.year_lo, cfg.year_hi);
    const std::string folded = fold_string(stem);
    std::vector<std::string> tokens = tokenize(folded);
    split_glued_prefixes(tokens);

    // Extract the first year (may split a glued token like "1984remastered").
    int year = -1;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        std::string prefix, suffix;
        int y = -1;
        if (find_year_run(tokens[i], cfg.year_lo, cfg.year_hi, prefix, y, suffix)) {
            year = y;
            std::vector<std::string> replaced;
            if (!prefix.empty()) replaced.push_back(prefix);
            if (!suffix.empty()) replaced.push_back(suffix);
            tokens.erase(tokens.begin() + static_cast<std::ptrdiff_t>(i));
            for (std::size_t k = 0; k < replaced.size(); ++k)
                tokens.insert(tokens.begin() + static_cast<std::ptrdiff_t>(i) +
                                static_cast<std::ptrdiff_t>(k),
                              replaced[k]);
            break;  // only the first year is extracted
        }
    }
    result.year = year;

    // Remove junk tokens (kept in result.junk for diagnostics).
    std::vector<std::string> kept;
    kept.reserve(tokens.size());
    for (const std::string& t : tokens) {
        if (is_junk(t, cfg.junk)) {
            result.junk.push_back(t);
        } else {
            kept.push_back(t);
        }
    }
    result.tokens = std::move(kept);
    return result;
}

}  // namespace dn
