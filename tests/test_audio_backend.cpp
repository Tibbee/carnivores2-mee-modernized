// Production backend + strict AL seams; no DLL/device/hardware is required.
#include <gtest/gtest.h>
#include "../Hunt/Audio/Audio_DLL.cpp"
#include "fake_openal.h"

int AudioFCount=0;
AudioQuad data[8192]{};
void UploadGeometry() {++fakeal::geometryCalls;}
void PrintLog(LPSTR message) {fakeal::logs.emplace_back(message);}

class AudioBackendTest : public ::testing::Test {
protected:
    short a[4]{1,2,3,4}, b[4]{5,6,7,8}, c[4]{9,10,11,12};
    void SetUp() override {
        fakeal::Install(); g_AudioBackend=AudioBackend::OpenALSoft;
        alDevice=reinterpret_cast<ALCdevice*>(1);alContext=reinterpret_cast<ALCcontext*>(1);
        InitializeCriticalSection(&AudioCS);iSoundActive=1;g_AudioShutdown=false;
        g_CurrentEnv=-1;g_EnvRuntimeInit=false;
        ASSERT_TRUE(CreateAudioSources()); InitReverb();fakeal::logs.clear();
    }
    void TearDown() override {
        Audio_Shutdown();
        EXPECT_TRUE(fakeal::sources.empty()); EXPECT_TRUE(fakeal::buffers.empty());
        EXPECT_TRUE(fakeal::effects.empty()); EXPECT_TRUE(fakeal::slots.empty());
        EXPECT_TRUE(bufferCache.empty());
    }
    void FullFade() {for(int i=0;i<16;++i)UpdateAmbientFades();}
};

TEST_F(AudioBackendTest, TransitionPreservesOutgoingPlaybackAndReachesExactEndpoint) {
    SetAmbient(sizeof(a),a,256);FullFade();
    const ALuint original=ambient.source;fakeal::sources[original].offset=1881;
    SetAmbient(sizeof(b),b,128);
    EXPECT_EQ(ambient2.source,original);
    EXPECT_EQ(fakeal::sources[original].offset,1881);
    EXPECT_EQ(fakeal::sources[original].starts,1);
    FullFade();
    EXPECT_EQ(ambient.volume,256);EXPECT_FLOAT_EQ(fakeal::sources[ambient.source].gain,0.5f);
    EXPECT_EQ(ambient2.lpData,nullptr);EXPECT_FALSE(fakeal::sources[ambient2.source].playing);
    EXPECT_EQ(fakeal::sources[ambient2.source].buffer,0u);
}
TEST_F(AudioBackendTest, ReversingAnActiveTransitionDoesNotRestartEitherSound) {
    SetAmbient(sizeof(a),a,256);FullFade();const auto sourceA=ambient.source;
    SetAmbient(sizeof(b),b,256);for(int i=0;i<5;++i)UpdateAmbientFades();
    const auto sourceB=ambient.source;const int va=ambient2.volume,vb=ambient.volume;
    fakeal::sources[sourceA].offset=100;fakeal::sources[sourceB].offset=200;
    SetAmbient(sizeof(a),a,256);
    EXPECT_EQ(ambient.source,sourceA);EXPECT_EQ(ambient.volume,va);EXPECT_EQ(ambient2.volume,vb);
    EXPECT_EQ(fakeal::sources[sourceA].offset,100);EXPECT_EQ(fakeal::sources[sourceB].offset,200);
    EXPECT_EQ(fakeal::sources[sourceA].starts,1);EXPECT_EQ(fakeal::sources[sourceB].starts,1);
}
TEST_F(AudioBackendTest, ThirdZoneRetainsTheLouderLiveBedRatherThanRestartingIt) {
    SetAmbient(sizeof(a),a,256);FullFade();const auto sourceA=ambient.source;
    SetAmbient(sizeof(b),b,256);for(int i=0;i<5;++i)UpdateAmbientFades();
    fakeal::sources[sourceA].offset=321;SetAmbient(sizeof(c),c,256);
    EXPECT_EQ(ambient2.lpData,a);EXPECT_EQ(ambient.lpData,c);
    EXPECT_EQ(fakeal::sources[sourceA].offset,321);EXPECT_EQ(fakeal::sources[sourceA].starts,1);
}
TEST_F(AudioBackendTest, FadeEndpointRetriesAfterGainSetterFailure) {
    SetAmbient(sizeof(a),a,256);for(int i=0;i<15;++i)UpdateAmbientFades();
    fakeal::failure="sourceF";UpdateAmbientFades();EXPECT_EQ(ambient.volume,240);
    UpdateAmbientFades();EXPECT_EQ(ambient.volume,256);
    EXPECT_FLOAT_EQ(fakeal::sources[ambient.source].gain,1);
}
TEST_F(AudioBackendTest, SameLoopVolumeEditKeepsPlaybackAndUsesFloatGainArithmetic) {
    SetAmbient(sizeof(a),a,128);FullFade();const auto source=ambient.source;
    fakeal::sources[source].offset=123;
    SetAmbient(sizeof(a),a,256);EXPECT_EQ(fakeal::sources[source].offset,123);
    EXPECT_EQ(fakeal::sources[source].starts,1);EXPECT_FLOAT_EQ(fakeal::sources[source].gain,1);
    SetAmbient(sizeof(a),a,2147483647);EXPECT_GT(fakeal::sources[source].gain,1);
}
TEST_F(AudioBackendTest, FailedVolumeEditRetriesInsteadOfCachingAnUnappliedGain) {
    SetAmbient(sizeof(a),a,256);FullFade();
    fakeal::failure="sourceF";SetAmbient(sizeof(a),a,128);EXPECT_EQ(ambient.avolume,256);
    SetAmbient(sizeof(a),a,128);EXPECT_EQ(ambient.avolume,128);
    EXPECT_FLOAT_EQ(fakeal::sources[ambient.source].gain,0.5f);
}
TEST_F(AudioBackendTest, FailedMovingSourceStopDoesNotLieAboutItsOwner) {
    SetAmbient3d(sizeof(a),a,1,2,3);
    fakeal::failure="stop";SetAmbient3d(sizeof(b),b,1,2,3);
    EXPECT_TRUE(IsAmbient3dOwner(a));EXPECT_FALSE(IsAmbient3dOwner(b));
    SetAmbient3d(sizeof(b),b,1,2,3);EXPECT_TRUE(IsAmbient3dOwner(b));
}
TEST_F(AudioBackendTest, SilenceFadesAndInvalidReplacementKeepsGoodAmbience) {
    SetAmbient(sizeof(a),a,256);FullFade();const auto source=ambient.source;
    SetAmbient(1,b,256);EXPECT_EQ(ambient.source,source);EXPECT_EQ(ambient.lpData,a);
    SetAmbient(0,nullptr,0);EXPECT_EQ(ambient2.lpData,a);FullFade();
    EXPECT_EQ(ambient2.lpData,nullptr);EXPECT_FALSE(fakeal::sources[source].playing);
}
TEST_F(AudioBackendTest, FailedNewAmbientPlaybackRollsBackToPreviousLoop) {
    SetAmbient(sizeof(a),a,256);FullFade();const auto source=ambient.source;
    fakeal::failure="play";SetAmbient(sizeof(b),b,256);
    EXPECT_EQ(ambient.lpData,a);EXPECT_EQ(ambient.source,source);
    EXPECT_TRUE(fakeal::sources[source].playing);EXPECT_EQ(fakeal::sources[source].starts,1);
}
TEST_F(AudioBackendTest, UploadFailureDoesNotPoisonCacheAndValidRetrySucceeds) {
    fakeal::failure="bufferData";EXPECT_EQ(GetBuffer(a,sizeof(a)),0u);
    EXPECT_EQ(bufferCache.count(a),0u);EXPECT_TRUE(fakeal::buffers.empty());
    const auto id=GetBuffer(a,sizeof(a));ASSERT_NE(id,0u);
    EXPECT_EQ(fakeal::buffers[id],sizeof(a));EXPECT_EQ(GetBuffer(a,sizeof(a)),id);
    EXPECT_EQ(GetBuffer(b,1),0u);EXPECT_EQ(GetBuffer(b,0),0u);EXPECT_EQ(GetBuffer(b,-2),0u);
    EXPECT_EQ(bufferCache.count(b),0u);
}
TEST_F(AudioBackendTest, InvalidVoiceRequestsCannotPlayWithStaleGain) {
    fakeal::sources[channel[0].source].gain=0.75f;
    AddVoicev(sizeof(a),a,-11);EXPECT_FALSE(fakeal::sources[channel[0].source].playing);
    EXPECT_TRUE(bufferCache.empty());EXPECT_TRUE(fakeal::logs.empty());
    AddVoicev(sizeof(a),a,FootstepVolume(-0.7f,false));
    EXPECT_TRUE(fakeal::sources[channel[0].source].playing);
    EXPECT_FLOAT_EQ(fakeal::sources[channel[0].source].gain,59/256.0f);
}
TEST_F(AudioBackendTest, FailedVoiceSetupDoesNotClaimOrPlayTheChannel) {
    fakeal::failure="sourceF";AddVoice(sizeof(a),a);
    EXPECT_EQ(channel[0].lpData,nullptr);EXPECT_FALSE(fakeal::sources[channel[0].source].playing);
    AddVoice(sizeof(a),a);EXPECT_EQ(channel[0].lpData,a);
}
TEST_F(AudioBackendTest, FailedStopDoesNotEraseStillPlayingVoiceStateAndCanRetry) {
    AddVoice(sizeof(a),a);
    fakeal::failure="stop";AudioStop();
    EXPECT_EQ(channel[0].lpData,a);EXPECT_TRUE(fakeal::sources[channel[0].source].playing);
    AudioStop();EXPECT_EQ(channel[0].lpData,nullptr);
    EXPECT_FALSE(fakeal::sources[channel[0].source].playing);
}
TEST_F(AudioBackendTest, FailedVoiceStateQueryIsNotAnUninitializedFreeSlot) {
    fakeal::failure="getSourceI";AddVoice(sizeof(a),a);
    EXPECT_EQ(channel[0].lpData,nullptr);EXPECT_EQ(channel[1].lpData,a);
}
TEST_F(AudioBackendTest, MovingLoopReplacementStopsBeforeBindingAndOwnershipIsAccurate) {
    SetAmbient3d(sizeof(a),a,100,200,300);const auto source=mambient.source;
    SetAmbient3d(sizeof(b),b,101,201,301);
    EXPECT_TRUE(IsAmbient3dOwner(b));EXPECT_FALSE(IsAmbient3dOwner(a));
    EXPECT_EQ(fakeal::sources[source].buffer,mambient.buffer);
    EXPECT_EQ(fakeal::sources[source].starts,2);EXPECT_TRUE(fakeal::logs.empty());
    SetAmbient3d(1,c,0,0,0);EXPECT_TRUE(IsAmbient3dOwner(b));
    fakeal::failure="play";SetAmbient3d(sizeof(c),c,0,0,0);
    EXPECT_TRUE(IsAmbient3dFree());EXPECT_FALSE(IsAmbient3dOwner(c));
}
TEST_F(AudioBackendTest, EnvironmentSevenDetachesAllWetSendsAndReturningWetReconnects) {
    Audio_SetEnvironment(8,0);ASSERT_EQ(g_CurrentEnv,8);EXPECT_EQ(fakeal::slots[g_slot],g_effect);
    Audio_SetEnvironment(7,0);EXPECT_EQ(g_CurrentEnv,7);EXPECT_EQ(fakeal::slots[g_slot],0u);
    for(const auto& source:fakeal::sources)EXPECT_EQ(source.second.send,0u);
    Audio_SetEnvironment(2,0);EXPECT_EQ(g_CurrentEnv,2);
    for(const auto& source:fakeal::sources)EXPECT_EQ(source.second.send,g_slot);
}
TEST_F(AudioBackendTest, FailedEnvironmentAndLiveTuningAreRetryable) {
    fakeal::failure="effectF";Audio_SetEnvironment(8,0);EXPECT_EQ(g_CurrentEnv,-1);
    Audio_SetEnvironment(8,0);EXPECT_EQ(g_CurrentEnv,8);
    Audio_SetEnvironment(99,0);EXPECT_EQ(g_CurrentEnv,8);
    EXPECT_TRUE(Audio_SetEnvParam(8,0,1.7f));EXPECT_EQ(g_CurrentEnv,-1);
    Audio_SetEnvironment(8,0);EXPECT_EQ(g_CurrentEnv,8);
}
TEST_F(AudioBackendTest, ListenerPitchMatchesAimAndUnusedGeometryIsNotBuilt) {
    AudioSetCameraPos(1,2,3,0,0.5f);EXPECT_FLOAT_EQ(fakeal::orientation[1],-std::sin(0.5f));
    Audio_UploadGeometry();EXPECT_EQ(fakeal::geometryCalls,0);
}
TEST_F(AudioBackendTest, BackgroundWorkerJoinsBeforeStateTeardown) {
    hAudioThread=CreateThread(nullptr,0,ProcessAudioThread,nullptr,0,&AudioTId);
    ASSERT_NE(hAudioThread,nullptr);SetAmbient(sizeof(a),a,128);
    bool advanced=false;
    for(int attempt=0;attempt<200 && !advanced;++attempt) {
        Sleep(10);
        AudioLock lock;
        advanced=ambient.volume>0;
    }
    EXPECT_TRUE(advanced); // Exercise a real worker tick, not just thread creation.
    Audio_Shutdown();EXPECT_EQ(hAudioThread,nullptr);EXPECT_EQ(iSoundActive.load(),0);
    EXPECT_EQ(alContext,nullptr);EXPECT_EQ(alDevice,nullptr);
}

TEST(AudioBackendSetup, PartialSourceCreationCanBeCompletelyUnwound) {
    fakeal::Install();fakeal::failSourceAfter=3;g_AudioBackend=AudioBackend::OpenALSoft;
    alDevice=reinterpret_cast<ALCdevice*>(1);alContext=reinterpret_cast<ALCcontext*>(1);
    InitializeCriticalSection(&AudioCS);iSoundActive=1;
    EXPECT_FALSE(CreateAudioSources());Audio_Shutdown();
    EXPECT_TRUE(fakeal::sources.empty());EXPECT_EQ(iSoundActive.load(),0);
}
TEST(AudioBackendSetup, EfxCapabilityAndCompleteEntryPointsAreRequired) {
    fakeal::Install();auto* device=reinterpret_cast<ALCdevice*>(1);
    fakeal::efx=false;EXPECT_FALSE(LoadOpenALEFX(device));
    fakeal::efx=true;fakeal::missingProc="alEffectf";EXPECT_FALSE(LoadOpenALEFX(device));
    fakeal::missingProc.clear();EXPECT_TRUE(LoadOpenALEFX(device));UnloadOpenAL();
    EXPECT_EQ(alGetError,nullptr);EXPECT_EQ(alEffectf,nullptr);
}
TEST(AudioBackendSetup, FailedEffectSlotCreationRollsBackTheEffectWithoutDoubleCheckingErrors) {
    fakeal::Install();alDevice=reinterpret_cast<ALCdevice*>(1);fakeal::failure="genSlots";
    InitReverb();EXPECT_EQ(g_effect,0u);EXPECT_EQ(g_slot,0u);
    EXPECT_TRUE(fakeal::effects.empty());EXPECT_TRUE(fakeal::slots.empty());
    InitReverb();EXPECT_NE(g_effect,0u);EXPECT_NE(g_slot,0u);
    fakeal::DeleteSlots(1,&g_slot);fakeal::DeleteEffects(1,&g_effect);
    g_effect=g_slot=0;alDevice=nullptr;UnloadOpenAL();
}
