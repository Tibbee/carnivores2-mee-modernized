// test_sound_loader_entry.cpp -- production WAV-loader failure contract

#include <gtest/gtest.h>

#include <cstdio>
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

TEST(SoundLoaderEntry, TruncatedWavWithoutDataChunkUsesProductionFailurePath)
{
    const char* path = "__c2_loader_contract_no_data__.wav";
    const unsigned char header[36] = {};
    FILE* file = nullptr;
    ASSERT_EQ(fopen_s(&file, path, "wb"), 0);
    ASSERT_EQ(fwrite(header, 1, sizeof(header), file), sizeof(header));
    fclose(file);

    char mutablePath[] = "__c2_loader_contract_no_data__.wav";
    TSFX sound{};
    EXPECT_THROW(LoadWav(mutablePath, sound), LoaderHalt);

    remove(path);
}

TEST(SoundLoaderEntry, TruncatedWavPayloadUsesProductionFailurePath)
{
    const char* path = "__c2_loader_contract_short_data__.wav";
    unsigned char wav[44] = {};
    memcpy(wav + 36, "data", 4);
    const DWORD length = 2;
    memcpy(wav + 40, &length, sizeof(length));

    FILE* file = nullptr;
    ASSERT_EQ(fopen_s(&file, path, "wb"), 0);
    ASSERT_EQ(fwrite(wav, 1, sizeof(wav), file), sizeof(wav));
    fclose(file);

    char mutablePath[] = "__c2_loader_contract_short_data__.wav";
    TSFX sound{};
    EXPECT_THROW(LoadWav(mutablePath, sound), LoaderHalt);

    remove(path);
}
