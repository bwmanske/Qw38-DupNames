#pragma once
#include <cstddef>
#include <cstdint>

namespace dn {

// Latin fold table (v1 scope, FR-2a): maps codepoints U+00C0..U+024F
// (Latin-1 Supplement + Latin Extended-A/B) to their folded ASCII form.
//
// The table is a dense array indexed by (cp - kFoldLo). Each entry is a
// NUL-terminated ASCII string of up to 2 characters:
//   - "a".."z"  : single base letter (the common case)
//   - "ae","oe","ij","th","dz","lj","nj","dj","ts" : ligature expansions
//   - ""        : no mapping (keep the codepoint as-is; e.g. x, /, B)
//
// Folding is defined as: NFKD-decompose, drop combining marks (U+0300..U+036F),
// then casefold. For these ranges that reduces to "base letter, lowercased".
// Compatibility mappings beyond NFKD (e.g. B -> ss) are intentionally out of scope.
//
// This header is hand-maintained (the tools/gen_fold_table codegen is not
// implemented); keep entries sorted by codepoint (they are, by index).

constexpr std::uint32_t kFoldLo = 0x00C0;
constexpr std::uint32_t kFoldHi = 0x024F;
constexpr std::size_t kFoldSize = kFoldHi - kFoldLo + 1;  // 400

// folded[0] == '\0'  =>  no mapping.
extern const char kLatinFold[kFoldSize][3];

// Returns the folded ASCII form for a single codepoint, or nullptr if the
// codepoint has no entry in the Latin table (caller keeps it unchanged).
inline const char* fold_lookup(std::uint32_t cp) {
    if (cp < kFoldLo || cp > kFoldHi) return nullptr;
    const char* f = kLatinFold[cp - kFoldLo];
    return f[0] ? f : nullptr;
}

}  // namespace dn
