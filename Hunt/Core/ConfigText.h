#pragma once
#include <istream>
#include <string>
#include <string_view>

// Shared by menu and engine: do not let embedded NUL padding terminate the
// engine's C-string parser while the menu still sees settings after it.
// Read the whole file (not just the first 4095 bytes of a commented config).

// The size ceiling every config.cfg reader and writer agrees on; the
// workbench's save path refuses anything over it instead of recovering.
inline constexpr std::size_t kMaxConfigBytes = 1024 * 1024;

// UTF-16 is not this format; don't mistake its zero bytes for padding. The
// writer refuses these files, the reader below also refuses them.
inline bool IsUtf16ConfigText(std::string_view text)
{
    if (text.size() < 2) return false;
    const unsigned char first = static_cast<unsigned char>(text[0]);
    const unsigned char second = static_cast<unsigned char>(text[1]);
    return (first == 0xFF && second == 0xFE) || (first == 0xFE && second == 0xFF);
}

inline bool ReadConfigText(std::istream& input, std::string& text, size_t& nulBytes)
{
    text.clear();
    nulBytes = 0;
    char chunk[4096];
    while (input.read(chunk, sizeof(chunk)) || input.gcount() > 0) {
        const size_t count = static_cast<size_t>(input.gcount());
        if (text.size() + count > kMaxConfigBytes) { text.clear(); return false; }
        text.append(chunk, count);
    }
    if (input.bad() || !input.eof()) { text.clear(); return false; }
    if (IsUtf16ConfigText(text)) {
        text.clear();
        return false;
    }
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    for (char& c : text) {
        if (c == '\0') { c = '\n'; ++nulBytes; }
    }
    return true;
}
