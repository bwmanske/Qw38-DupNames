#pragma once
#include <string>

#include "dn/types.hpp"

namespace dn {

// Run the deterministic normalization pipeline (SPEC §5) on a raw file name:
//   strip extension -> fold -> tokenize -> extract year -> remove junk -> drop empty.
// The original name is preserved in NormalizedName::raw for display.
NormalizedName normalize(const std::string& raw_name, const Config& cfg);

}  // namespace dn
