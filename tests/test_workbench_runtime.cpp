#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include "Workbench/TuningRuntime.h"
#include "Core/GameState.h"
#include "Core/NightLighting.h"

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
	return {UWFog_BaseDensityMult, UWFog_CameraDepthMult, UWFog_VertRange,
			UWFog_VertStrength, UWFog_CurveExp, UWFog_CapBase, UWFog_CapCameraBoost,
			WWave1Amp, WWave2Amp, WWave3Amp, WWaveSpeed, SunGlare_Master, SunGlare_Disc,
			static_cast<float>(NightLightingOptions().worldBrightnessPct), static_cast<float>(NightLightingOptions().desaturationPct),
			static_cast<float>(NightLightingOptions().skyBrightnessPct), static_cast<float>(NightLightingOptions().moonBrightnessPct),
			static_cast<float>(NightLightingOptions().moonGlowStrengthPct), static_cast<float>(OptFlashlightIntensityPct),
			static_cast<float>(OptFlashlightRange), static_cast<float>(OptFlashlightInnerAngleDeg), static_cast<float>(OptFlashlightOuterAngleDeg),
			FlashlightTint()[0], FlashlightTint()[1], FlashlightTint()[2]};
}
std::string Source(const char* path) {
	std::ifstream stream(std::string(C2_SOURCE_ROOT) + "/" + path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
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
TEST_F(WorkbenchRuntime, DumpRetainsTheExistingKeysGroupsPrecisionAndDepthTelemetry) {
	CameraWaterDepthFactor = .099f;
	const std::string expected =
		"=== UNDERWATER DEBUG VALUES ===\n--- Fog ---\n"
		"UWFog_BaseDensityMult = 0.23\nUWFog_CameraDepthMult = 0.10\n"
		"UWFog_VertRange = 1488.6\nUWFog_VertStrength = 85.0\nUWFog_CurveExp = 1.94\n"
		"UWFog_CapBase = 85.0\nUWFog_CapCameraBoost = 100.0\nCameraWaterDepthFactor = 0.099\n"
		"--- Waves ---\nWWave1Amp = 14.58\nWWave2Amp = 8.10\nWWave3Amp = 5.00\nWWaveSpeed = 0.50\n"
		"--- Sun glare ---\nSunGlare_Master = 1.00\nSunGlare_Disc = 1.00\n=== END DEBUG VALUES ===\n";
	EXPECT_EQ(FormatRuntimeDump(), expected);
	EditRuntime(Parameter::FogDensity, 1.12f);
	EXPECT_NE(FormatRuntimeDump().find("UWFog_BaseDensityMult = 1.12\n"), std::string::npos);
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
	EXPECT_EQ(emittedLines, 19);
	EmitRuntimeDump(nullptr); // Optional/unavailable sink must be safe.
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
	const auto flashlight = Source("Hunt/Renderer/GLFlashlight.cpp");
	EXPECT_NE(flashlight.find("tint = FlashlightTint()"), std::string::npos);
	EXPECT_NE(flashlight.find("colorValue.shouldUpload(tint)"), std::string::npos);
	ASSERT_EQ(EditRuntime(Parameter::NightWorld, 42), Error::None);
	EXPECT_EQ(NightLightingParameters(2, false, NightLightingOptions())[0], .42f);
	EXPECT_EQ(NightLightingParameters(1, false, NightLightingOptions())[0], 1);
	EXPECT_EQ(NightLightingParameters(2, true, NightLightingOptions())[0], 1);
}
TEST_F(WorkbenchRuntime, HuntResetAndKeyboardDispatchUseTheProductionAdapter) {
	const auto loader = Source("Hunt/Loaders/CharacterLoader.cpp");
	const auto reset = loader.find("workbench::ResetRuntimeForHunt()");
	ASSERT_NE(reset, std::string::npos);
	EXPECT_LT(reset, loader.find("  PlaceHunter();", reset));
	EXPECT_EQ(loader.find("UWFog_BaseDensityMult ="), std::string::npos);
	const auto hunt = Source("Hunt/Game/Hunt.cpp");
	EXPECT_NE(hunt.find("workbench::AdjustLegacyRuntime(parameter, right)"), std::string::npos);
	EXPECT_NE(hunt.find("workbench::EmitRuntimeDump(PrintLog)"), std::string::npos);
	EXPECT_EQ(hunt.find("float* params[]"), std::string::npos);
}
