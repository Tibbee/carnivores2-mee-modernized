// test_sound_loader_entry.cpp -- production WAV-loader failure contract

#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>
#include <string>

#include "Hunt.h"

namespace {

class LoaderHalt final : public std::runtime_error
{
public:
    explicit LoaderHalt(const char* message)
        : std::runtime_error(message ? message : "loader halt")
    {}
};

}  // namespace

// DoHalt terminates the game after showing the production error. The test
// target replaces that process boundary with an exception so the real loader
// entry point can be exercised in-process.
void DoHalt(LPSTR message)
{
    throw LoaderHalt(message);
}

TEST(SoundLoaderEntry, MissingWavUsesProductionFailurePath)
{
    char missingPath[] = "__c2_loader_contract_missing__.wav";
    TSFX sound{};

    EXPECT_THROW(LoadWav(missingPath, sound), LoaderHalt);
}
