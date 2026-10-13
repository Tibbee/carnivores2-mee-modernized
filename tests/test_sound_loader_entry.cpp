// test_sound_loader_entry.cpp -- production WAV-loader failure contract

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#include "Hunt.h"
#include "wav_fixture.h"

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

TEST(SoundLoaderEntry, ValidAndNormalizedWavsLoadThroughTheProductionEntryPoint)
{
    {
        const char* path = "__c2_loader_contract_valid__.wav";
        const auto wav = wav_fixture::Pcm16({123, -456});
        const DWORD length = 4;
        const short samples[2] = {123, -456};
        FILE* file = nullptr;
        ASSERT_EQ(fopen_s(&file, path, "wb"), 0);
        ASSERT_EQ(fwrite(wav.data(), 1, wav.size(), file), wav.size());
        fclose(file);

        char mutablePath[] = "__c2_loader_contract_valid__.wav";
        TSFX sound{};
        EXPECT_NO_THROW(LoadWav(mutablePath, sound));
        EXPECT_EQ(sound.length, length);
        ASSERT_EQ(sound.lpData.size(), 2u);
        EXPECT_EQ(sound.lpData[0], samples[0]);
        EXPECT_EQ(sound.lpData[1], samples[1]);

        remove(path);
    }
    // Stereo 44100 goes through the shared normalization contract.
    {
        char path[] = "__c2_loader_contract_stereo__.wav";
        const auto wav = wav_fixture::Pcm16({100,300,200,400,300,500,400,600},2,44100);
        FILE* file = nullptr;
        ASSERT_EQ(fopen_s(&file,path,"wb"),0);
        ASSERT_EQ(fwrite(wav.data(),1,wav.size(),file),wav.size());
        fclose(file);
        TSFX sound{};
        EXPECT_NO_THROW(LoadWav(path,sound));
        EXPECT_EQ(sound.length,4);
        EXPECT_EQ(sound.lpData,(std::vector<short>{200,400}));
        EXPECT_EQ(remove(path),0);
    }
}

TEST(SoundLoaderEntry, TruncatedWavsTakeTheProductionFailurePath)
{
    // A header with no data chunk at all.
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
    // A valid header whose payload is one byte short.
    {
        const char* path = "__c2_loader_contract_short_data__.wav";
        auto wav = wav_fixture::Pcm16({123});
        wav.pop_back();
        FILE* file = nullptr;
        ASSERT_EQ(fopen_s(&file, path, "wb"), 0);
        ASSERT_EQ(fwrite(wav.data(), 1, wav.size(), file), wav.size());
        fclose(file);

        char mutablePath[] = "__c2_loader_contract_short_data__.wav";
        TSFX sound{};
        EXPECT_THROW(LoadWav(mutablePath, sound), LoaderHalt);

        remove(path);
    }
}

TEST(SoundLoaderEntry, InvalidReplacementPreservesSoundAndClosesItsFile)
{
    char path[] = "__c2_loader_contract_bad_frame__.wav";
    const auto wav = wav_fixture::Riff(wav_fixture::Format()+wav_fixture::Chunk("data","x"));
    FILE* file = nullptr;
    ASSERT_EQ(fopen_s(&file,path,"wb"),0);
    ASSERT_EQ(fwrite(wav.data(),1,wav.size(),file),wav.size());
    fclose(file);
    TSFX sound{};sound.length=2;sound.lpData={42};
    EXPECT_THROW(LoadWav(path,sound),LoaderHalt);
    EXPECT_EQ(sound.length,2);EXPECT_EQ(sound.lpData,(std::vector<short>{42}));
    EXPECT_EQ(remove(path),0); // Win32 refuses deletion of a leaked read-only-shared handle.
}
