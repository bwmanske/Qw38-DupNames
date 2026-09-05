#include "dn/utf8.hpp"

#include "dn/fold_table.hpp"

namespace dn {

namespace {

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Decode one UTF-8 codepoint at s[i]; on success set *cp and return the number
// of bytes consumed, on invalid input return 0 (caller skips one byte).
std::size_t decode_utf8(const std::string& s, std::size_t i, std::uint32_t& cp) {
    const std::size_t n = s.size();
    const unsigned char c = static_cast<unsigned char>(s[i]);
    std::uint32_t value = 0;
    std::size_t len = 0;
    if (c < 0x80) {
        value = c;
        len = 1;
    } else if ((c & 0xE0) == 0xC0) {
        value = c & 0x1F;
        len = 2;
    } else if ((c & 0xF0) == 0xE0) {
        value = c & 0x0F;
        len = 3;
    } else if ((c & 0xF8) == 0xF0) {
        value = c & 0x07;
        len = 4;
    } else {
        return 0;  // invalid lead byte
    }
    if (i + len > n) return 0;
    for (std::size_t k = 1; k < len; ++k) {
        const unsigned char cc = static_cast<unsigned char>(s[i + k]);
        if ((cc & 0xC0) != 0x80) return 0;
        value = (value << 6) | (cc & 0x3F);
    }
    cp = value;
    return len;
}

}  // namespace

std::string fold_codepoint(std::uint32_t cp) {
    if (cp >= 0x0300 && cp <= 0x036F) return "";  // combining mark: drop
    if (const char* f = fold_lookup(cp)) return f;
    if (cp >= 'A' && cp <= 'Z') return std::string(1, static_cast<char>(cp - 'A' + 'a'));
    std::string out;
    append_utf8(out, cp);
    return out;
}

std::string fold_string(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    std::size_t i = 0;
    while (i < in.size()) {
        std::uint32_t cp = 0;
        const std::size_t len = decode_utf8(in, i, cp);
        if (len == 0) {
            ++i;  // skip invalid byte
            continue;
        }
        i += len;
        out += fold_codepoint(cp);
    }
    return out;
}

}  // namespace dn
