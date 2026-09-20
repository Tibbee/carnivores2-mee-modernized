#pragma once

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

// _RES.TXT values include the line ending because the parser passes the text
// after '=' directly from fgets(). The legacy engine read these fields with
// atoi()/atof(), which accept a numeric prefix and ignore everything after
// it; mods rely on that for decimal literals on integer-backed fields,
// C-style suffixes (1.0f, 7L), and stray trailing tokens. The helpers below
// preserve that prefix behavior while still rejecting values that do not
// start with a number, overflow the destination, or are not finite.

inline bool ParseScriptInt(const char* text, int& out)
{
    if (!text)
        return false;

    char* end = nullptr;
    errno = 0;
    const long value = strtol(text, &end, 10);
    if (end == text || errno == ERANGE)
        return false;
    if (value < static_cast<long>((std::numeric_limits<int>::min)()) ||
        value > static_cast<long>((std::numeric_limits<int>::max)()))
        return false;

    out = static_cast<int>(value);
    return true;
}

inline bool ParseScriptFloat(const char* text, float& out)
{
    if (!text)
        return false;

    char* end = nullptr;
    errno = 0;
    const float value = strtof(text, &end);
    if (end == text || errno == ERANGE || !std::isfinite(value))
        return false;

    out = value;
    return true;
}

// Integer-backed gameplay fields were authored with decimal literals or a
// C-style long suffix. The old atoi/atof paths truncated those values; parse
// through strtod so `health = 13.5` keeps meaning 13.
inline bool ParseScriptLegacyInt(const char* text, int& out)
{
    if (!text)
        return false;

    char* end = nullptr;
    errno = 0;
    const double value = strtod(text, &end);
    if (end == text || errno == ERANGE || !std::isfinite(value))
        return false;
    if (value < static_cast<double>((std::numeric_limits<int>::min)()) ||
        value > static_cast<double>((std::numeric_limits<int>::max)()))
        return false;

    out = static_cast<int>(value);
    return true;
}
