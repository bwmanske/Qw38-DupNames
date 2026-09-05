#pragma once
#include <cstdint>
#include <string>

namespace dn {

// Fold a single Unicode codepoint to its comparable form (v1 scope, FR-2a):
//   - combining marks (U+0300..U+036F) are dropped (returns "")
//   - Latin codepoints U+00C0..U+024F map to their base ASCII letter(s)
//   - ASCII A-Z casefold to a-z
//   - anything else is returned unchanged (re-encoded to UTF-8)
std::string fold_codepoint(std::uint32_t cp);

// Fold an entire UTF-8 string: decode each codepoint, fold it, re-encode.
// Invalid UTF-8 bytes are skipped.
std::string fold_string(const std::string& utf8);

}  // namespace dn
