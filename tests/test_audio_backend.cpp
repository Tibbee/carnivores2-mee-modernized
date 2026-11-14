// Production backend + strict AL seams; no DLL/device/hardware is required.
#include <gtest/gtest.h>
#include "../Hunt/Audio/Audio_DLL.cpp"
#include "Audio/OpenALDevice.h"
#include "fake_openal.h"
#include "Workbench/TuningRuntime.h"
#include "Core/Constants.h"
#include "Core/NightLighting.h"
#include <cstdlib>
#include <limits>
BOOL _HeapFree(HANDLE, DWORD, LPVOID pointer) { if (pointer) std::abort(); return TRUE; }
void UploadGeometry() {++fakeal::geometryCalls;}
void PrintLog(LPSTR message) {fakeal::logs.emplace_back(message);}

class AudioBackendTest : public ::testing::Test {
protected:
    short a[4]{1,2,3,4}, b[4]{5,6,7,8}, c[4]{9,10,11,12};
    void SetUp() override {
        fakeal::Install(); g_AudioBackend=AudioBackend::OpenALSoft;
        alDevice=reinterpret_cast<ALCdevice*>(1);alContext=reinterpret_cast<ALCcontext*>(1);
        InitializeCriticalSection(&AudioCS);iSoundActive=1;g_AudioShutdown=false;
        g_CurrentEnv=g_RequestedEnv=g_AppliedEnv=-1;g_EnvRuntimeInit=false;
        g_Application=c2audio::Application::Pending;
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
    EXPECT_EQ(bufferCache.count(BufferKey{a,static_cast<int>(sizeof(a)),g_BufferGeneration}),0u);
    EXPECT_TRUE(fakeal::buffers.empty());
    const auto id=GetBuffer(a,sizeof(a));ASSERT_NE(id,0u);
    EXPECT_EQ(fakeal::buffers[id],sizeof(a));EXPECT_EQ(GetBuffer(a,sizeof(a)),id);
    EXPECT_EQ(GetBuffer(b,1),0u);EXPECT_EQ(GetBuffer(b,0),0u);EXPECT_EQ(GetBuffer(b,-2),0u);
    EXPECT_EQ(bufferCache.count(BufferKey{b,1,g_BufferGeneration}),0u);
}
TEST_F(AudioBackendTest, GenerationChangeUploadsFreshBufferWhenPointerAndLengthMatch) {
    const auto oldBuffer=GetBuffer(a,sizeof(a));ASSERT_NE(oldBuffer,0u);
    AddVoice(sizeof(a),a);ASSERT_EQ(channel[0].buffer,oldBuffer);
    Audio_ForgetLevelSamples();
    const auto newBuffer=GetBuffer(a,sizeof(a));ASSERT_NE(newBuffer,0u);
    EXPECT_NE(newBuffer,oldBuffer);
    EXPECT_EQ(fakeal::sources[channel[0].source].buffer,oldBuffer);
    EXPECT_EQ(fakeal::buffers.count(oldBuffer),1u);
}
TEST_F(AudioBackendTest, GenerationSweepDeletesUnattachedRetiredBuffers) {
    const auto oldBuffer=GetBuffer(a,sizeof(a));ASSERT_NE(oldBuffer,0u);
    Audio_ForgetLevelSamples();
    EXPECT_EQ(fakeal::buffers.count(oldBuffer),0u);
}
TEST_F(AudioBackendTest, AmbientAddressReuseAcrossGenerationsUsesNewSample) {
    SetAmbient(sizeof(a),a,256);FullFade();
    const auto oldBuffer=ambient.buffer;
    const auto oldSource=ambient.source;
    fakeal::sources[oldSource].offset=123;
    Audio_ForgetLevelSamples();
    a[0]=99; // A released level's address and length can be reused for new PCM.
    SetAmbient(sizeof(a),a,256);
    EXPECT_NE(ambient.buffer,oldBuffer);
    EXPECT_EQ(ambient2.buffer,oldBuffer);
    EXPECT_EQ(fakeal::sources[oldSource].offset,123);
    EXPECT_TRUE(fakeal::sources[oldSource].playing); // Retained outgoing bed fades normally.
    FullFade();
    EXPECT_EQ(ambient2.buffer,0u);
}
TEST_F(AudioBackendTest, MovingAmbientAddressReuseAcrossGenerationsRebinds) {
    SetAmbient3d(sizeof(a),a,10,20,30);
    const auto oldBuffer=mambient.buffer;
    const auto source=mambient.source;
    Audio_ForgetLevelSamples();
    a[0]=99;
    SetAmbient3d(sizeof(a),a,11,21,31);
    EXPECT_NE(mambient.buffer,oldBuffer);
    EXPECT_EQ(fakeal::sources[source].buffer,mambient.buffer);
    EXPECT_EQ(fakeal::sources[source].starts,2);
    EXPECT_TRUE(fakeal::logs.empty()); // Replacement stops/detaches before binding.
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

TEST(AudioBackendSetup, PresetReadAndConfigBeforeInitializationNeverNeedALOrAudioCS) {
	EXPECT_EQ(iSoundActive.load(),0);
	g_EnvRuntimeInit=false;
	EXPECT_TRUE(Audio_SetEnvParam(0,0,3.75f));
	EXPECT_EQ(c2audio::ReadSnapshot(true).backend,c2audio::Backend::Disabled);
	EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[0][0],3.75f);
	EXPECT_TRUE(Audio_SetEnvParam(0,3,-777.75f)); // Legacy loader keeps truncation.
	EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[0][3],-777);
	EXPECT_FALSE(Audio_SetEnvParam(0,0,std::numeric_limits<float>::infinity()));
}
TEST_F(AudioBackendTest, PresetDefaultsAndSnapshotCopiesMatchEffectiveBackendTable) {
	EXPECT_EQ(c2audio::ReadEnvironments(),c2audio::Defaults());
	auto snapshot=c2audio::ReadEnvironments();snapshot[0][0]=19;
	EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[0][0],1.49f);
}
TEST_F(AudioBackendTest, StatusSeparatesRequestSuccessDryFallbackAndPartialFailure) {
	EXPECT_EQ(c2audio::ReadSnapshot().application,c2audio::Application::Pending);
	Audio_SetEnvironment(8,0);
	auto s=c2audio::ReadSnapshot();EXPECT_EQ(s.requested,8);EXPECT_EQ(s.applied,8);
	EXPECT_EQ(s.application,c2audio::Application::Reverb);EXPECT_EQ(s.appliedValues,c2audio::Defaults()[8]);
	fakeal::failure="source3I";Audio_SetEnvironment(2,0);
	s=c2audio::ReadSnapshot();EXPECT_EQ(s.requested,2);EXPECT_EQ(s.applied,-1);
	EXPECT_EQ(s.application,c2audio::Application::Failed);
	Audio_SetEnvironment(2,0);EXPECT_EQ(c2audio::ReadSnapshot().applied,2);
	Audio_SetEnvironment(7,0);EXPECT_EQ(c2audio::ReadSnapshot().application,c2audio::Application::Dry);
	fakeal::DeleteSlots(1,&g_slot);fakeal::DeleteEffects(1,&g_effect);g_slot=g_effect=0;
	Audio_SetEnvironment(8,0);s=c2audio::ReadSnapshot();EXPECT_EQ(g_CurrentEnv,8);
	EXPECT_FALSE(s.reverbAvailable);EXPECT_EQ(s.applied,-1);EXPECT_EQ(s.application,c2audio::Application::DryFallback);
}
TEST_F(AudioBackendTest, StatusOnlyNeverSamplesVoicesAndFailedQueriesRemainUnknown) {
	AddVoicev(sizeof(a),a,128);AudioSetCameraPos(0,0,0,0,0);
	fakeal::failure="getSourceI";
	auto status=c2audio::ReadSnapshot();EXPECT_FALSE(status.details);EXPECT_EQ(fakeal::failure,"getSourceI");
	auto s=c2audio::ReadSnapshot(true);EXPECT_FALSE(s.voices[0].known);
	EXPECT_TRUE(fakeal::sources[channel[0].source].playing);
	s=c2audio::ReadSnapshot(true);EXPECT_TRUE(s.voices[0].playing);EXPECT_TRUE(s.voices[0].relative);
	EXPECT_FLOAT_EQ(s.voices[0].configuredGain,.5f);s.voices[0].configuredGain=0;
	EXPECT_FLOAT_EQ(fakeal::sources[channel[0].source].gain,.5f);
}
TEST_F(AudioBackendTest, SpatialAndAmbientDiagnosticsHaveBoundedCopiedObservations) {
	AudioSetCameraPos(10,20,30,0,0);AddVoice3dv(sizeof(a),a,13,24,30,256);
	SetAmbient(sizeof(b),b,128);FullFade();SetAmbient3d(sizeof(c),c,10,20,35);
	const auto s=c2audio::ReadSnapshot(true);EXPECT_EQ(s.voices.size(),19u);
	EXPECT_TRUE(s.voices[0].playing);EXPECT_FALSE(s.voices[0].relative);EXPECT_FLOAT_EQ(s.voices[0].distance,5);
	EXPECT_EQ(s.voices[16].fade,256);EXPECT_FLOAT_EQ(s.voices[16].configuredGain,.5f);
	EXPECT_TRUE(s.voices[18].playing);EXPECT_FLOAT_EQ(s.voices[18].distance,5);
}
TEST_F(AudioBackendTest, LegacyAndShutdownSnapshotsDoNotUseOpenALOrDeletedLocks) {
	g_AudioBackend=AudioBackend::LegacyDLL;fakeal::failure="getSourceI";
	EXPECT_EQ(c2audio::ReadSnapshot(true).application,c2audio::Application::LegacyUnverified);
	EXPECT_EQ(fakeal::failure,"getSourceI");g_AudioBackend=AudioBackend::OpenALSoft;
	Audio_Shutdown();EXPECT_EQ(c2audio::ReadSnapshot(true).application,c2audio::Application::Disabled);
	EXPECT_EQ(fakeal::failure,"getSourceI");
}
TEST_F(AudioBackendTest, FullTableWritesRejectFractionsNonfiniteStaleAndDryBankAtomically) {
	const auto before=c2audio::ReadEnvironments();auto v=before;
	v[0][0]=2;v[8][3]=-100.5f;
	EXPECT_EQ(c2audio::WriteEnvironments(before,v),c2audio::WriteResult::Invalid);EXPECT_EQ(c2audio::ReadEnvironments(),before);
	v=before;v[0][0]=std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(c2audio::WriteEnvironments(before,v),c2audio::WriteResult::Invalid);
	v=before;v[7][0]=1;EXPECT_EQ(c2audio::WriteEnvironments(before,v),c2audio::WriteResult::Invalid);
	v=before;v[0][0]=2;EXPECT_TRUE(Audio_SetEnvParam(5,0,4));
	EXPECT_EQ(c2audio::WriteEnvironments(before,v),c2audio::WriteResult::Changed);EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[0][0],before[0][0]);
}
TEST_F(AudioBackendTest, ActiveBankRefreshDoesNotRestartAndInactiveEditDoesNotSwitchEnvironment) {
	SetAmbient(sizeof(a),a,256);FullFade();Audio_SetEnvironment(2,0);
	const auto source=ambient.source;fakeal::sources[source].offset=812;
	auto before=c2audio::ReadEnvironments(), v=before;v[5][0]=4;
	ASSERT_EQ(c2audio::WriteEnvironments(before,v),c2audio::WriteResult::Ok);
	EXPECT_EQ(c2audio::ReadSnapshot().applied,2);EXPECT_EQ(c2audio::ReadSnapshot().requested,2);
	before=v;v[2][0]=3.5f;ASSERT_EQ(c2audio::WriteEnvironments(before,v),c2audio::WriteResult::Ok);
	EXPECT_FLOAT_EQ(c2audio::ReadSnapshot().appliedValues[0],3.5f);
	EXPECT_EQ(fakeal::sources[source].offset,812);EXPECT_EQ(fakeal::sources[source].starts,1);
	fakeal::failure="effectF";before=v;v[2][0]=3.6f;
	ASSERT_EQ(c2audio::WriteEnvironments(before,v),c2audio::WriteResult::Ok);
	EXPECT_EQ(c2audio::ReadSnapshot().application,c2audio::Application::Failed);
	Audio_SetEnvironment(2,0);EXPECT_EQ(c2audio::ReadSnapshot().applied,2);
}
TEST_F(AudioBackendTest, InvalidDiagnosticTableDoesNotBlockOrdinaryLegacyRestart) {
	ASSERT_FALSE(Audio_SetEnvParam(5,0,40)); // Existing loader range rejection remains.
	g_EnvRuntime[5][0]=40; // Fault injection, not a reachable config-loader value.
	UWFog_BaseDensityMult=.77f;WWave1Amp=.31f;
	EXPECT_EQ(workbench::ResetRuntimeForHunt(),workbench::Error::OutOfRange);
	EXPECT_FLOAT_EQ(UWFog_BaseDensityMult,workbench::CompiledDefaults()[0]);
	EXPECT_FLOAT_EQ(WWave1Amp,workbench::CompiledDefaults()[7]);
	EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[5][0],40);
	Audio_SetEnvParam(5,0,c2audio::Defaults()[5][0]);
}
TEST_F(AudioBackendTest, RuntimeSessionUsesRealConfiguredBanksHistoryCompareAndRestart) {
	using namespace workbench;
	NightLightingOptions()={};OptFlashlightIntensityPct=500;OptFlashlightRange=6000;
	OptFlashlightInnerAngleDeg=14;OptFlashlightOuterAngleDeg=26;FlashlightTint()={1,.902f,.784f};
	UWFog_BaseDensityMult=0;ASSERT_TRUE(Audio_SetEnvParam(8,0,3.25f));
	ASSERT_EQ(ResetRuntimeForHunt(),Error::None);Audio_SetEnvironment(8,0);
	const auto id=AudioParameter(8,0);ASSERT_EQ(EditRuntimeText(id,"4.75"),Error::None);
	EXPECT_FLOAT_EQ(c2audio::ReadSnapshot().appliedValues[0],4.75f);
	ASSERT_EQ(UndoRuntime(),Error::None);EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[8][0],3.25f);
	ASSERT_EQ(RedoRuntime(),Error::None);ASSERT_EQ(CompareRuntime(true),Error::None);
	EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[8][0],3.25f);ASSERT_EQ(CompareRuntime(false),Error::None);
	EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[8][0],4.75f);ASSERT_EQ(ResetRuntimeForHunt(),Error::None);
	EXPECT_FLOAT_EQ(c2audio::ReadEnvironments()[8][0],3.25f);
}
TEST(AudioBackendNormalizeTest, LegacySavedDriverValuesKeepThePreExtractionMapping) {
	// 0..3 = software / DirectSound / A3D / EAX -> DirectSound
	EXPECT_EQ(AUDIO_DIRECTSOUND, NormalizeAudioBackend(0));
	EXPECT_EQ(AUDIO_DIRECTSOUND, NormalizeAudioBackend(2));
	EXPECT_EQ(AUDIO_DIRECTSOUND, NormalizeAudioBackend(3));
	// 1 = the stored OpenAL Soft index; the historical rule maps everything
	// >= 4 (legacy OpenAL / XAudio2 and unknown values) to OpenAL Soft.
	EXPECT_EQ(AUDIO_OPENALSOFT, NormalizeAudioBackend(AUDIO_OPENALSOFT));
	EXPECT_EQ(AUDIO_OPENALSOFT, NormalizeAudioBackend(4));
	EXPECT_EQ(AUDIO_OPENALSOFT, NormalizeAudioBackend(5));
	EXPECT_EQ(AUDIO_OPENALSOFT, NormalizeAudioBackend(6));
	// Negative values are not a stored backend and fall back to DirectSound.
	EXPECT_EQ(AUDIO_DIRECTSOUND, NormalizeAudioBackend(-1));
}
TEST(OpenALDeviceLifecycleTest, EveryOpenFailureLeavesNoPartialPair) {
	fakeal::Install();
	ALCdevice* device=nullptr;ALCcontext* context=nullptr;
	fakeal::failure="openDevice";
	EXPECT_EQ(OpenALDeviceResult::NoDevice,OpenOpenALDevice(device,context));
	EXPECT_EQ(nullptr,device);EXPECT_EQ(nullptr,context);
	fakeal::failure="createContext";
	EXPECT_EQ(OpenALDeviceResult::NoContext,OpenOpenALDevice(device,context));
	EXPECT_EQ(nullptr,device);EXPECT_EQ(nullptr,context);EXPECT_EQ(1,fakeal::deviceCloses);
	fakeal::failure="makeCurrent";
	EXPECT_EQ(OpenALDeviceResult::NoContext,OpenOpenALDevice(device,context));
	EXPECT_EQ(nullptr,device);EXPECT_EQ(nullptr,context);
	EXPECT_EQ(2,fakeal::deviceCloses);EXPECT_EQ(1,fakeal::contextDestroys);
}
TEST(OpenALDeviceLifecycleTest, CloseReleasesBothHandlesAndIsIdempotent) {
	fakeal::Install();
	ALCdevice* device=nullptr;ALCcontext* context=nullptr;
	EXPECT_EQ(OpenALDeviceResult::Ok,OpenOpenALDevice(device,context));
	EXPECT_NE(nullptr,device);EXPECT_NE(nullptr,context);
	CloseOpenALDevice(device,context);
	EXPECT_EQ(nullptr,device);EXPECT_EQ(nullptr,context);
	EXPECT_EQ(1,fakeal::deviceCloses);EXPECT_EQ(1,fakeal::contextDestroys);
	CloseOpenALDevice(device,context);
	EXPECT_EQ(1,fakeal::deviceCloses);EXPECT_EQ(1,fakeal::contextDestroys);
}
