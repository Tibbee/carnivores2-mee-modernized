#pragma once
#include <istream>
#include <string>

// Shared by menu and engine: do not let embedded NUL padding terminate the
// engine's C-string parser while the menu still sees settings after it.
// Read the whole file (not just the first 4095 bytes of a commented config).
inline bool ReadConfigText(std::istream& input, std::string& text, size_t& nulBytes)
{
    constexpr size_t maxBytes = 1024 * 1024;
    text.clear();
    nulBytes = 0;
    char chunk[4096];
    while (input.read(chunk, sizeof(chunk)) || input.gcount() > 0) {
        const size_t count = static_cast<size_t>(input.gcount());
        if (text.size() + count > maxBytes) { text.clear(); return false; }
        text.append(chunk, count);
    }
    if (input.bad() || !input.eof()) { text.clear(); return false; }
    // UTF-16 is not this format; don't mistake its zero bytes for padding.
    if (text.compare(0, 2, "\xFF\xFE") == 0 || text.compare(0, 2, "\xFE\xFF") == 0) {
        text.clear();
        return false;
    }
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    for (char& c : text) {
        if (c == '\0') { c = '\n'; ++nulBytes; }
    }
    return true;
}
