#include <gtest/gtest.h>
#include "Hunt.h"
#include "wav_fixture.h"
#include <filesystem>
#include <fstream>
bool LoadWave(SoundFX&,const std::string&);

class MenuWaveTest : public ::testing::Test {
protected:
    std::filesystem::path path;
    void SetUp() override {
        path=std::filesystem::temp_directory_path()/
            ("c2-menu-wave-"+std::to_string(GetCurrentProcessId())+".wav");
    }
    void TearDown() override {std::error_code e;std::filesystem::remove(path,e);}
    void Write(const std::string& data) {std::ofstream f(path,std::ios::binary);f.write(data.data(),data.size());}
};
TEST_F(MenuWaveTest, ValidStockSoundIsExact) {
    Write(wav_fixture::Pcm16({123,-456}));SoundFX s;
    ASSERT_TRUE(LoadWave(s,path.string()));ASSERT_EQ(s.m_Length,4u);
    EXPECT_EQ(s.m_Data[0],123);EXPECT_EQ(s.m_Data[1],-456);EXPECT_EQ(s.m_Frequency,22050u);
}
TEST_F(MenuWaveTest, MissingDataAndEveryTruncationReturnWithoutMutatingExistingSound) {
    SoundFX s;s.m_Data=new int16_t[1]{42};s.m_Length=2;auto* original=s.m_Data;
    const auto valid=wav_fixture::Pcm16({123,-456});
    for(size_t n=0;n<valid.size();++n) {
        Write(valid.substr(0,n));EXPECT_FALSE(LoadWave(s,path.string()))<<n;
        EXPECT_EQ(s.m_Data,original);EXPECT_EQ(s.m_Length,2u);EXPECT_EQ(s.m_Data[0],42);
    }
    Write(wav_fixture::Riff(wav_fixture::Format()));EXPECT_FALSE(LoadWave(s,path.string()));
}
TEST_F(MenuWaveTest, Stereo44100IsNormalizedRatherThanPlayedAtTheWrongSpeed) {
    Write(wav_fixture::Pcm16({100,300,200,400,300,500,400,600},2,44100));SoundFX s;
    ASSERT_TRUE(LoadWave(s,path.string()));ASSERT_EQ(s.m_Length,4u);
    EXPECT_EQ(s.m_Data[0],200);EXPECT_EQ(s.m_Data[1],400);EXPECT_EQ(s.m_Frequency,22050u);
}
TEST_F(MenuWaveTest, Odd16BitLengthIsRejectedButPadded8BitIsDecodedSafely) {
    Write(wav_fixture::Riff(wav_fixture::Format()+wav_fixture::Chunk("data","x")));
    SoundFX s;EXPECT_FALSE(LoadWave(s,path.string()));EXPECT_EQ(s.m_Data,nullptr);
    Write(wav_fixture::Riff(wav_fixture::Format(1,8)+wav_fixture::Chunk("data",std::string("\x80",1))));
    ASSERT_TRUE(LoadWave(s,path.string()));ASSERT_EQ(s.m_Length,2u);EXPECT_EQ(s.m_Data[0],0);
}
TEST_F(MenuWaveTest, EmptySoundCopyIsInitializedAndValidCopyIsIndependent) {
    SoundFX empty;SoundFX copy(empty);EXPECT_EQ(copy.m_Data,nullptr);EXPECT_EQ(copy.m_Length,0u);
    Write(wav_fixture::Pcm16({123,-456}));ASSERT_TRUE(LoadWave(empty,path.string()));
    SoundFX populated(empty);EXPECT_NE(populated.m_Data,empty.m_Data);
    EXPECT_EQ(populated.m_Length,empty.m_Length);EXPECT_EQ(populated.m_Data[1],-456);
}
