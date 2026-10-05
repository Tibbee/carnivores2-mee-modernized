#include <gtest/gtest.h>
#include "../Menu/Audio.cpp"
#include "fake_openal.h"

class MenuAudioBackendTest : public ::testing::Test {
protected:
    void SetUp() override {fakeal::Install();}
    void TearDown() override {
        MenuAudioShutdown();
        EXPECT_TRUE(fakeal::sources.empty());EXPECT_TRUE(fakeal::buffers.empty());
    }
    void Sound(SoundFX& s) {s.m_Length=8;s.m_Data=new int16_t[4]{1,2,3,4};}
};
TEST_F(MenuAudioBackendTest, FailedUploadIsNotReportedAsSuccessOrLeaked) {
    SoundFX s;Sound(s);ALuint buffer=0;
    fakeal::failure="bufferData";EXPECT_FALSE(UploadSound(buffer,s));
    EXPECT_EQ(buffer,0u);EXPECT_TRUE(fakeal::buffers.empty());
    EXPECT_TRUE(UploadSound(g_MenuAudio.clickBuffer,s));
}
TEST_F(MenuAudioBackendTest, SourceSetupFailureDeletesPartialSourceAndCanRetry) {
    SoundFX s;Sound(s);ASSERT_TRUE(UploadSound(g_MenuAudio.clickBuffer,s));
    fakeal::failure="sourceF";
    EXPECT_FALSE(CreateSource(g_MenuAudio.clickSource,g_MenuAudio.clickBuffer,false,1));
    EXPECT_EQ(g_MenuAudio.clickSource,0u);EXPECT_TRUE(fakeal::sources.empty());
    EXPECT_TRUE(CreateSource(g_MenuAudio.clickSource,g_MenuAudio.clickBuffer,false,1));
    PlaySource(g_MenuAudio.clickSource);EXPECT_TRUE(fakeal::sources[g_MenuAudio.clickSource].playing);
}
TEST_F(MenuAudioBackendTest, InvalidLengthsAndFrequencyNeverUpload) {
    SoundFX s;Sound(s);ALuint buffer=0;
    s.m_Length=1;EXPECT_FALSE(UploadSound(buffer,s));
    s.m_Length=8;s.m_Frequency=0;EXPECT_FALSE(UploadSound(buffer,s));
    EXPECT_TRUE(fakeal::buffers.empty());
}
TEST_F(MenuAudioBackendTest, StartingAnAlreadyPlayingBedDoesNotRestartIt) {
    SoundFX s;Sound(s);ASSERT_TRUE(UploadSound(g_MenuAudio.ambientBuffer,s));
    ASSERT_TRUE(CreateSource(g_MenuAudio.ambientSource,g_MenuAudio.ambientBuffer,true,0.85f));
    g_MenuAudio.active=true;
    MenuAudioStartAmbient();fakeal::sources[g_MenuAudio.ambientSource].offset=123;
    MenuAudioStartAmbient();
    EXPECT_EQ(fakeal::sources[g_MenuAudio.ambientSource].starts,1);
    EXPECT_EQ(fakeal::sources[g_MenuAudio.ambientSource].offset,123);
}
