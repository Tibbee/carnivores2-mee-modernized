#pragma once

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

inline bool ScriptNumericTailIsValid(const char* text)
{
    if (!text)
        return false;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        ++text;
    return *text == '\0' || (text[0] == '/' && text[1] == '/');
}

// _RES.TXT values include the line ending because the parser passes the text
// after '=' directly from fgets(). Reject partial conversions such as "4oops"
// while still accepting documented trailing whitespace/comments.
inline bool ParseScriptInt(const char* text, int& out)
{
    if (!text)
        return false;

    char* end = nullptr;
    errno = 0;
    const long value = strtol(text, &end, 10);
    if (end == text || errno == ERANGE || !ScriptNumericTailIsValid(end))
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
    if (end == text || errno == ERANGE || !ScriptNumericTailIsValid(end) ||
        !std::isfinite(value))
        return false;

    out = value;
    return true;
}
