#include <gtest/gtest.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include "Workbench/TuningRuntime.h"
#include "Core/GameState.h"
#include "Core/NightLighting.h"
#include "workbench_audio_seams.h"

// StateDefs links empty asset holders. No game allocations are permitted in
// this asset-free executable; satisfy their deleter without linking the engine.
BOOL _HeapFree(HANDLE, DWORD, LPVOID memory) {
	if (memory) std::abort();
	return TRUE;
}

namespace {
using namespace workbench;
class WorkbenchRuntime : public ::testing::Test {
protected:
	void SetUp() override {
		workbench_test::ResetAudio();
		NightLightingOptions() = {};
		OptFlashlightIntensityPct = 500; OptFlashlightRange = 6000;
		OptFlashlightInnerAngleDeg = 14; OptFlashlightOuterAngleDeg = 26;
		FlashlightTint() = {1, .902f, .784f};
		UWFog_BaseDensityMult = 0; // Simulate fresh engine initialization.
		ASSERT_EQ(ResetRuntimeForHunt(), Error::None);
	}
};
// Deliberately independent of production bindings: wrong/missing/swapped
// bindings must fail, not validate through the same lookup as the adapter.
Values Globals() {
	Values v{UWFog_BaseDensityMult, UWFog_CameraDepthMult, UWFog_VertRange,
			UWFog_VertStrength, UWFog_CurveExp, UWFog_CapBase, UWFog_CapCameraBoost,
			WWave1Amp, WWave2Amp, WWave3Amp, WWaveSpeed, SunGlare_Master, SunGlare_Disc,
			static_cast<float>(NightLightingOptions().worldBrightnessPct), static_cast<float>(NightLightingOptions().desaturationPct),
			static_cast<float>(NightLightingOptions().skyBrightnessPct), static_cast<float>(NightLightingOptions().moonBrightnessPct),
			static_cast<float>(NightLightingOptions().moonGlowStrengthPct), static_cast<float>(OptFlashlightIntensityPct),
			static_cast<float>(OptFlashlightRange), static_cast<float>(OptFlashlightInnerAngleDeg), static_cast<float>(OptFlashlightOuterAngleDeg),
			FlashlightTint()[0], FlashlightTint()[1], FlashlightTint()[2]};
	// Independent bank ordering; do not derive it from the production ID helper.
	int i = 25;
	for (int bank : {0,1,2,3,4,5,6,8}) for (int f = 0; f < 4; ++f) v[i++] = workbench_test::audio[bank][f];
	return v;
}
}
TEST_F(WorkbenchRuntime, CompiledInitializationCapturesTheRealScalarGlobals) {
	EXPECT_TRUE(SameValues(Globals(), CompiledDefaults()));
	EXPECT_TRUE(SameValues(ReadRuntimeValues(), Globals()));
	EXPECT_TRUE(SameValues(RuntimeSession().Loaded(), Globals()));
}
TEST_F(WorkbenchRuntime, EachSetterChangesOnlyItsOwnProductionGlobal) {
	for (const auto& d : Descriptors()) {
		ASSERT_EQ(ResetRuntimeForHunt(), Error::None);
		auto expected = Globals(); const auto before = expected;
		expected[static_cast<std::size_t>(d.id)] = d.maximum == d.compiledDefault ? d.minimum : d.maximum;
		if (d.id == Parameter::FlashlightInner) expected[static_cast<std::size_t>(d.id)] = 25;
		ASSERT_EQ(EditRuntime(d.id, expected[static_cast<std::size_t>(d.id)]), Error::None);
		EXPECT_TRUE(SameValues(Globals(), expected)) << d.key;
		EXPECT_TRUE(RuntimeSession().Changed(d.id));
		EXPECT_EQ(RuntimeSession().ChangedCount(), 1u);
		ASSERT_EQ(UndoRuntime(), Error::None); EXPECT_TRUE(SameValues(Globals(), before));
		ASSERT_EQ(RedoRuntime(), Error::None); EXPECT_TRUE(SameValues(Globals(), expected));
	}
}
TEST_F(WorkbenchRuntime, SynchronizationOnlyObservesAndInvalidatesExternalWrites) {
	const auto baseline = Globals();
	EXPECT_EQ(SynchronizeRuntimeSession(), Error::None); EXPECT_EQ(Globals(), baseline);
	WWave1Amp = .5f;
	EXPECT_EQ(SynchronizeRuntimeSession(), Error::RuntimeChanged);
	EXPECT_FLOAT_EQ(WWave1Amp, .5f); EXPECT_FALSE(RuntimeSession().IsLoaded());
	EXPECT_EQ(SynchronizeRuntimeSession(), Error::NotLoaded);
	EXPECT_FLOAT_EQ(WWave1Amp, .5f);
}
TEST_F(WorkbenchRuntime, InvalidTextAndNumericEditsNeverPublish) {
	const auto before = Globals();
	EXPECT_EQ(EditRuntimeText(Parameter::FogDensity, ""), Error::Blank);
	EXPECT_EQ(EditRuntimeText(Parameter::FogDensity, "3oops"), Error::InvalidNumber);
	EXPECT_EQ(EditRuntime(Parameter::FogDensity, 4.f), Error::OutOfRange);
	EXPECT_EQ(EditRuntime(Parameter::WavePrimary, std::numeric_limits<float>::quiet_NaN()), Error::NonFinite);
	EXPECT_EQ(EditRuntime(Parameter::Count, 1.f), Error::UnknownParameter);
	EXPECT_TRUE(SameValues(Globals(), before)); EXPECT_FALSE(RuntimeSession().CanUndo());
}
TEST_F(WorkbenchRuntime, BaselineRecaptureRetainsAnExistingOverrideAndResetsToIt) {
	UWFog_BaseDensityMult = .8f;
	ASSERT_EQ(CaptureRuntimeBaseline(), Error::None);
	ASSERT_EQ(EditRuntimeText(Parameter::FogDensity, "1.23456"), Error::None);
	EXPECT_FLOAT_EQ(UWFog_BaseDensityMult, 1.23456f);
	ASSERT_EQ(ResetRuntime(Parameter::FogDensity), Error::None);
	EXPECT_FLOAT_EQ(UWFog_BaseDensityMult, .8f);
}
TEST_F(WorkbenchRuntime, CompareSwapsRuntimeValuesWithoutErasingEdits) {
	ASSERT_EQ(EditRuntime(Parameter::FogDensity, 1.f), Error::None);
	ASSERT_EQ(EditRuntime(Parameter::WavePrimary, 20.f), Error::None);
	const auto edited = Globals();
	ASSERT_EQ(CompareRuntime(true), Error::None);
	EXPECT_TRUE(SameValues(Globals(), CompiledDefaults())); EXPECT_EQ(RuntimeSession().ChangedCount(), 2u);
	EXPECT_EQ(EditRuntime(Parameter::FogDensity, 2.f), Error::Comparing);
	ASSERT_EQ(UndoRuntime(), Error::None); EXPECT_TRUE(SameValues(Globals(), CompiledDefaults()));
	ASSERT_EQ(RedoRuntime(), Error::None); EXPECT_TRUE(SameValues(Globals(), CompiledDefaults()));
	ASSERT_EQ(CompareRuntime(false), Error::None); EXPECT_TRUE(SameValues(Globals(), edited));
}
TEST_F(WorkbenchRuntime, GestureResetAndUndoOperateOnRealValues) {
	ASSERT_EQ(BeginRuntimeGesture(), Error::None);
	for (float waveValue : {15.f, 20.f, 25.f}) ASSERT_EQ(EditRuntime(Parameter::WavePrimary, waveValue), Error::None);
	ASSERT_EQ(EndRuntimeGesture(), Error::None);
	ASSERT_EQ(UndoRuntime(), Error::None); EXPECT_FLOAT_EQ(WWave1Amp, 14.58f); EXPECT_FALSE(RuntimeSession().CanUndo());
	ASSERT_EQ(RedoRuntime(), Error::None); EXPECT_FLOAT_EQ(WWave1Amp, 25.f);
	ASSERT_EQ(EditRuntime(Parameter::FogDensity, 1.f), Error::None);
	ASSERT_EQ(ResetRuntimeGroup(Group::Waves), Error::None);
	EXPECT_FLOAT_EQ(WWave1Amp, 14.58f); EXPECT_FLOAT_EQ(UWFog_BaseDensityMult, 1.f);
	ASSERT_EQ(ResetRuntimeAll(), Error::None); EXPECT_TRUE(SameValues(Globals(), CompiledDefaults()));
	ASSERT_EQ(UndoRuntime(), Error::None); EXPECT_FLOAT_EQ(UWFog_BaseDensityMult, 1.f);
}
TEST_F(WorkbenchRuntime, ExternalChangeInvalidatesInsteadOfOverwritingNewState) {
	ASSERT_EQ(EditRuntime(Parameter::FogDensity, 1.f), Error::None);
	WWave1Amp = 23.f;
	const auto externallyEdited = Globals();
	EXPECT_EQ(UndoRuntime(), Error::RuntimeChanged);
	EXPECT_FALSE(RuntimeSession().IsLoaded()); EXPECT_FALSE(RuntimeSession().CanUndo());
	EXPECT_TRUE(SameValues(Globals(), externallyEdited));
	EXPECT_EQ(EditRuntime(Parameter::FogDensity, 2.f), Error::NotLoaded);
	ASSERT_EQ(CaptureRuntimeBaseline(), Error::None);
	ASSERT_EQ(EditRuntime(Parameter::FogDensity, 2.f), Error::None); EXPECT_FLOAT_EQ(WWave1Amp, 23.f);
}
TEST_F(WorkbenchRuntime, ExternalMutationDuringComparisonCannotBeOverwrittenByReturn) {
	EditRuntime(Parameter::FogDensity, 1.f); CompareRuntime(true);
	SunGlare_Master = .5f;
	EXPECT_EQ(CompareRuntime(false), Error::RuntimeChanged);
	EXPECT_FLOAT_EQ(SunGlare_Master, .5f); EXPECT_FLOAT_EQ(UWFog_BaseDensityMult, .23f);
}
TEST_F(WorkbenchRuntime, RestartEndsComparisonAndCannotReplayOldHuntEdits) {
	EditRuntime(Parameter::FogDensity, 1.f); CompareRuntime(true);
	const auto generation = RuntimeSession().Generation();
	ASSERT_EQ(ResetRuntimeForHunt(), Error::None);
	EXPECT_GT(RuntimeSession().Generation(), generation); EXPECT_FALSE(RuntimeSession().IsComparing());
	EXPECT_FALSE(RuntimeSession().CanUndo()); EXPECT_FALSE(RuntimeSession().CanRedo());
	EXPECT_TRUE(SameValues(Globals(), CompiledDefaults()));
}
TEST_F(WorkbenchRuntime, LegacyKeysUpdateTheCorrespondingGlobalAndRecordHistory) {
	for (int tab = 0; tab < 3; ++tab) {
		const int count = tab == 0 ? 7 : tab == 1 ? 4 : 2;
		for (int selected = 0; selected < count; ++selected) {
			ResetRuntimeForHunt(); const auto id = LegacyParameter(tab, selected);
			float expected = 0;
			ASSERT_EQ(LegacyAdjustedValue(id, CompiledDefaults()[static_cast<std::size_t>(id)], true, expected), Error::None);
			ASSERT_EQ(AdjustLegacyRuntime(id, true), Error::None);
			EXPECT_TRUE(SameValue(Globals()[static_cast<std::size_t>(id)], expected));
			EXPECT_TRUE(RuntimeSession().CanUndo());
		}
	}
}
TEST_F(WorkbenchRuntime, DumpRetainsTheLegacyBlockByteExactAndAppendsTheFullInventory) {
	CameraWaterDepthFactor = .099f;
	const std::string legacy =
		"=== UNDERWATER DEBUG VALUES ===\n--- Fog ---\n"
		"UWFog_BaseDensityMult = 0.23\nUWFog_CameraDepthMult = 0.10\n"
		"UWFog_VertRange = 1488.6\nUWFog_VertStrength = 85.0\nUWFog_CurveExp = 1.94\n"
		"UWFog_CapBase = 85.0\nUWFog_CapCameraBoost = 100.0\nCameraWaterDepthFactor = 0.099\n"
		"--- Waves ---\nWWave1Amp = 14.58\nWWave2Amp = 8.10\nWWave3Amp = 5.00\nWWaveSpeed = 0.50\n"
		"--- Sun glare ---\nSunGlare_Master = 1.00\nSunGlare_Disc = 1.00\n=== END DEBUG VALUES ===\n";
	const auto dump = FormatRuntimeDump();
	EXPECT_EQ(dump.compare(0, legacy.size(), legacy), 0) << "the legacy D-dump is a byte-exact log contract";
	EXPECT_GT(dump.size(), legacy.size());
	EditRuntime(Parameter::FogDensity, 1.12f);
	EXPECT_NE(FormatRuntimeDump().find("UWFog_BaseDensityMult = 1.12\n"), std::string::npos);
}
TEST_F(WorkbenchRuntime, DumpInventoryCoversEveryDescriptorWithItsPersistencePolicy) {
	const auto dump = FormatRuntimeDump();
	std::size_t tagged = 0;
	for (const auto& d : Descriptors()) {
		const std::string key = std::string(d.key) + " = ";
		std::size_t at = 0;
		while ((at = dump.find(key, at)) != std::string::npos) {
			const auto end = dump.find("\n", at);
			ASSERT_NE(end, std::string::npos) << d.key;
			const auto line = dump.substr(at, end - at);
			if (line.find("[cfg]") != std::string::npos ||
				line.find("[runtime]") != std::string::npos ||
				line.find("[dump]") != std::string::npos) ++tagged;
			at = end;
		}
	}
	// The legacy block repeats the untagged thirteen; the appended inventory tags
	// every descriptor exactly once.
	EXPECT_EQ(tagged, kParameterCount);
	EXPECT_NE(dump.find("--- Night ---\n"), std::string::npos);
	EXPECT_NE(dump.find("--- Beam ---\n"), std::string::npos);
	EXPECT_NE(dump.find("--- Audio ---\n"), std::string::npos);
	EXPECT_NE(dump.find("night_world_brightness = 22  [cfg]\n"), std::string::npos);
	EXPECT_NE(dump.find("UWFog_BaseDensityMult = 0.23  [dump]\n"), std::string::npos);
	EXPECT_NE(dump.find("runtime.flashlight_tint_r = 1.000  [runtime]\n"), std::string::npos);
	EXPECT_NE(dump.find("env0_decay = 1.490  [cfg]\n"), std::string::npos);
	EXPECT_NE(dump.find("env0_reverb = -620  [cfg]\n"), std::string::npos);
}
namespace {
std::string emittedDump;
int emittedLines = 0;
void PrintLogSeam(char* line) {
	// Match Resources.cpp::PrintLog: only the final LF becomes CRLF.
	const std::string text(line);
	EXPECT_EQ(text.find('\n'), text.size() - 1);
	emittedDump += text.substr(0, text.size() - 1) + "\r\n";
	++emittedLines;
}
}
TEST_F(WorkbenchRuntime, DumpEmitterPreservesLegacyLogLineEndings) {
	const auto text = FormatRuntimeDump();
	std::string expected;
	for (char c : text) { if (c == '\n') expected += '\r'; expected += c; }
	emittedDump.clear(); emittedLines = 0;
	EmitRuntimeDump(PrintLogSeam);
	EXPECT_EQ(emittedDump, expected);
	// Derived from the registry rather than hard-coded: the legacy dump was
	// nineteen lines, and a new descriptor changes this only by existing.
	EXPECT_EQ(emittedLines, std::count(text.begin(), text.end(), '\n'));
	EXPECT_NE(emittedLines, 19);
	EmitRuntimeDump(nullptr); // Optional/unavailable sink must be safe.
}
TEST_F(WorkbenchRuntime, DumpEndsWithAPasteableConfigBlockOfRecognizedKeysOnly) {
	NightLightingOptions().worldBrightnessPct = 15;
	ASSERT_EQ(CaptureRuntimeBaseline(), Error::None);
	const auto dump = FormatRuntimeDump();
	const auto begin = dump.find("=== CONFIG.CFG KEYS FOR TUNABLES ===");
	ASSERT_NE(begin, std::string::npos);
	const auto end = dump.find("=== END CONFIG.CFG KEYS ===");
	ASSERT_NE(end, std::string::npos);
	EXPECT_GT(end, begin);
	const auto block = dump.substr(begin, end - begin);
	std::size_t listed = 0;
	for (const auto& d : Descriptors()) {
		if (d.persist != PersistPolicy::RecognizedConfigKey) continue;
		++listed;
		// Every pasted token must be one complete parseable number, otherwise the
		// engine reader logs a rejected value and the block silently does nothing.
		const std::string prefix = std::string(d.key) + " ";
		const auto at = block.find(prefix);
		ASSERT_NE(at, std::string::npos) << d.key;
		const auto stop = block.find("\n", at + prefix.size());
		ASSERT_NE(stop, std::string::npos);
		const std::string token = block.substr(at + prefix.size(), stop - at - prefix.size());
		EXPECT_FALSE(token.empty()) << d.key;
		char* tail = nullptr;
		errno = 0;
		const float parsed = std::strtof(token.c_str(), &tail);
		EXPECT_EQ(errno, 0) << d.key << " -> " << token;
		EXPECT_NE(tail, token.c_str()) << d.key << " -> " << token;
		EXPECT_EQ(*tail, '\0') << d.key << " -> " << token;
		EXPECT_FLOAT_EQ(parsed, ReadRuntimeValues()[static_cast<std::size_t>(d.id)]);
	}
	EXPECT_EQ(listed, 41u); // Nine night/beam keys plus the eight live banks x four reverb fields.
	EXPECT_EQ(block.find("UWFog_BaseDensityMult"), std::string::npos); // dump-only: no loader key
	EXPECT_EQ(block.find("runtime.flashlight_tint_r"), std::string::npos); // runtime-only
	EXPECT_NE(dump.find("night_world_brightness 15\n"), std::string::npos); // edited, not compiled default
}
TEST_F(WorkbenchRuntime, HuntResetRetainsConfiguredBaselinesRatherThanFactoryLighting) {
	NightLightingOptions().worldBrightnessPct = 35;
	OptFlashlightRange = 11000; OptFlashlightIntensityPct = 200;
	OptFlashlightInnerAngleDeg = 10; OptFlashlightOuterAngleDeg = 22;
	ASSERT_EQ(CaptureRuntimeBaseline(), Error::None); const auto configured = Globals();
	ASSERT_EQ(EditRuntime(Parameter::NightWorld, 55), Error::None);
	ASSERT_EQ(EditRuntime(Parameter::FlashlightRange, 15000), Error::None);
	ASSERT_EQ(EditRuntimeFlashlightColor({.1f, .2f, .3f}), Error::None);
	ASSERT_EQ(CompareRuntime(true), Error::None);
	ASSERT_EQ(ResetRuntimeForHunt(), Error::None);
	EXPECT_EQ(Globals(), configured); EXPECT_FALSE(RuntimeSession().CanUndo());
	EXPECT_EQ(NightLightingParameters(2, false, NightLightingOptions())[0], .35f);
}
TEST_F(WorkbenchRuntime, NewExternalConfigurationAtRestartWinsOverTheOldPreview) {
	ASSERT_EQ(EditRuntime(Parameter::NightWorld, 55), Error::None);
	OptFlashlightRange = 12345; // New external/config value, not the tool's snapshot.
	ASSERT_EQ(ResetRuntimeForHunt(), Error::None);
	EXPECT_EQ(OptFlashlightRange, 12345); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 55);
	EXPECT_EQ(RuntimeSession().Loaded(), Globals()); EXPECT_FALSE(RuntimeSession().CanUndo());
}
TEST_F(WorkbenchRuntime, InvalidAtmosphereAndConeEditsPublishNothing) {
	const auto before = Globals();
	EXPECT_EQ(EditRuntimeText(Parameter::NightWorld, "55.5"), Error::IntegerRequired);
	EXPECT_EQ(EditRuntime(Parameter::FlashlightInner, 26), Error::InvalidCone);
	EXPECT_EQ(EditRuntimeFlashlightColor({.1f, 2, .3f}), Error::OutOfRange);
	EXPECT_EQ(Globals(), before); EXPECT_FALSE(RuntimeSession().CanUndo());
	ASSERT_EQ(EditRuntimeFlashlightColor({.1f, .2f, .3f}), Error::None);
	EXPECT_EQ(FlashlightTint(), (std::array<float, 3>{.1f, .2f, .3f}));
	ASSERT_EQ(UndoRuntime(), Error::None); EXPECT_EQ(Globals(), before);
}
TEST_F(WorkbenchRuntime, ContextCopiesBoundedScalarsWithoutGrantingEquipmentOrMutatingState) {
	const auto originalProject = std::string(ProjectName);
	const int day = OptDayNight, underwater = UNDERWATER, fog = CameraFogI, pocket = CAMERAINFOG, env = Env, health = MyHealth;
	const BOOL equipped = FlashlightMode, on = FlashlightOn, nv = NightVisionOn;
	std::fill(ProjectName, ProjectName + sizeof(ProjectName), 'x');
	OptDayNight = 2; UNDERWATER = 1; CameraFogI = 127; CAMERAINFOG = 1; Env = 7;
	FlashlightMode = FALSE; FlashlightOn = TRUE; NightVisionOn = TRUE; MyHealth = 100;
	const auto before = Globals(); const auto state = ReadEnvironmentContext();
	EXPECT_EQ(state.project.size(), sizeof(ProjectName)); EXPECT_EQ(state.timeOfDay, 2);
	EXPECT_TRUE(state.underwater); EXPECT_FALSE(state.inPocketFog); EXPECT_EQ(state.requestedEnvironment, 8);
	EXPECT_FALSE(state.flashlightEquipped); EXPECT_TRUE(state.flashlightSwitchedOn);
	EXPECT_FALSE(state.flashlightIlluminating); EXPECT_TRUE(state.nightVision);
	EXPECT_EQ(Globals(), before); EXPECT_FALSE(FlashlightMode);
	strcpy_s(ProjectName, originalProject.c_str()); OptDayNight = day; UNDERWATER = underwater;
	CameraFogI = fog; CAMERAINFOG = pocket; Env = env; MyHealth = health;
	FlashlightMode = equipped; FlashlightOn = on; NightVisionOn = nv;
}
TEST_F(WorkbenchRuntime, ExistingRendererConsumesLiveLightingAndTintWithoutShaderChanges) {
	ASSERT_EQ(EditRuntime(Parameter::NightWorld, 42), Error::None);
	EXPECT_EQ(NightLightingParameters(2, false, NightLightingOptions())[0], .42f);
	EXPECT_EQ(NightLightingParameters(1, false, NightLightingOptions())[0], 1);
	EXPECT_EQ(NightLightingParameters(2, true, NightLightingOptions())[0], 1);
}
TEST_F(WorkbenchRuntime, LegacyRestartResetsSurviveUnsupportedLoadedAudioValues) {
	UWFog_BaseDensityMult=.77f;WWave1Amp=.31f;
	workbench_test::audio[5][0]=40; // Fault-injected backend snapshot, outside validated range.
	EXPECT_EQ(workbench::ResetRuntimeForHunt(),Error::OutOfRange);
	EXPECT_FLOAT_EQ(UWFog_BaseDensityMult,CompiledDefaults()[0]);
	EXPECT_FLOAT_EQ(WWave1Amp,CompiledDefaults()[7]);
	EXPECT_FLOAT_EQ(workbench_test::audio[5][0],40); // Do not silently repair an outside backend mutation.
	EXPECT_FALSE(RuntimeSession().IsLoaded());
}
TEST_F(WorkbenchRuntime, AudioUnavailableRejectsAtomicallyWithoutHistoryOrOtherGroupsChanging) {
	const auto before=Globals(); workbench_test::audioAvailable=false;
	EXPECT_EQ(EditRuntime(AudioParameter(0,0),3),Error::Unavailable);
	EXPECT_EQ(Globals(),before);EXPECT_FALSE(RuntimeSession().CanUndo());
	EXPECT_EQ(EditRuntime(Parameter::WavePrimary,20),Error::None);
	EXPECT_EQ(UndoRuntime(),Error::None);
}
TEST_F(WorkbenchRuntime, AudioBankResetAndMixedHistoryRemainGroupedAndConfigured) {
	workbench_test::audio[8][0]=3.25f;ASSERT_EQ(CaptureRuntimeBaseline(),Error::None);
	ASSERT_EQ(BeginRuntimeGesture(),Error::None);
	ASSERT_EQ(EditRuntime(AudioParameter(8,0),4),Error::None);
	ASSERT_EQ(EditRuntime(AudioParameter(8,1),1),Error::None);ASSERT_EQ(EndRuntimeGesture(),Error::None);
	ASSERT_EQ(EditRuntime(AudioParameter(2,0),5),Error::None);
	ASSERT_EQ(ResetRuntimeAudioBank(8),Error::None);
	EXPECT_FLOAT_EQ(workbench_test::audio[8][0],3.25f);EXPECT_FLOAT_EQ(workbench_test::audio[2][0],5);
	ASSERT_EQ(UndoRuntime(),Error::None);EXPECT_FLOAT_EQ(workbench_test::audio[8][0],4);
	ASSERT_EQ(ResetRuntimeForHunt(),Error::None);EXPECT_FLOAT_EQ(workbench_test::audio[8][0],3.25f);
}
TEST_F(WorkbenchRuntime, AudioExternalMutationInvalidatesAndRestartPreservesOutsideWrites) {
	ASSERT_EQ(EditRuntime(AudioParameter(2,0),4),Error::None);
	workbench_test::audio[8][2]=.55f;
	EXPECT_EQ(SynchronizeRuntimeSession(),Error::RuntimeChanged);
	ASSERT_EQ(ResetRuntimeForHunt(),Error::None);
	EXPECT_FLOAT_EQ(workbench_test::audio[8][2],.55f);EXPECT_FLOAT_EQ(workbench_test::audio[2][0],4);
}
TEST_F(WorkbenchRuntime, CapabilityLossCannotHalfApplyMixedUndoAndComparisonCanSafelyWithdraw) {
	ASSERT_EQ(BeginRuntimeGesture(),Error::None);
	ASSERT_EQ(EditRuntime(Parameter::WavePrimary,20),Error::None);
	ASSERT_EQ(EditRuntime(AudioParameter(2,0),4),Error::None);ASSERT_EQ(EndRuntimeGesture(),Error::None);
	const auto edited=Globals();workbench_test::audioAvailable=false;
	EXPECT_EQ(UndoRuntime(),Error::Unavailable);EXPECT_EQ(Globals(),edited);EXPECT_TRUE(RuntimeSession().CanUndo());
	workbench_test::audioAvailable=true;ASSERT_EQ(CompareRuntime(true),Error::None);
	workbench_test::audioAvailable=false;ASSERT_EQ(CompareRuntime(false),Error::None);
	EXPECT_EQ(Globals(),edited);EXPECT_FALSE(RuntimeSession().IsComparing());
}
