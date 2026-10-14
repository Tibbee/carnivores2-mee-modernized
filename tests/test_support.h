#pragma once

// ==========================================================================
// test_support.h -- Shared fixtures for the loader contract tests
// ==========================================================================
// LoaderHalt is the seam the loader tests throw in place of the production
// DoHalt process boundary (each test TU still defines its own DoHalt).
// TempScript owns the tmpfile() stream the script readers parse.
// ==========================================================================

#include <cstdio>
#include <stdexcept>

namespace test_support {

class LoaderHalt final : public std::runtime_error
{
public:
    explicit LoaderHalt(const char* message)
        : std::runtime_error(message ? message : "loader halt")
    {}
};

// RAII tmpfile() stream; optionally prefilled with text and rewound so the
// readers start from the top.
class TempScript final
{
public:
    TempScript()
        : stream(tmpfile())
    {}

    explicit TempScript(const char* text)
        : stream(tmpfile())
    {
        if (stream) {
            fputs(text, stream);
            rewind(stream);
        }
    }

    ~TempScript()
    {
        if (stream)
            fclose(stream);
    }

    TempScript(const TempScript&) = delete;
    TempScript& operator=(const TempScript&) = delete;

    FILE* stream;
};

} // namespace test_support
