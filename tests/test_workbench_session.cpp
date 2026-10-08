#include <gtest/gtest.h>
#include "Workbench/TuningSession.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>

namespace {
using namespace workbench;
float Value(const Values& values, Parameter id) { return values[static_cast<std::size_t>(id)]; }
Session LoadedSession() { Session s; EXPECT_EQ(s.Capture(CompiledDefaults()), Error::None); return s; }

// Independent copy of the pre-workbench MainWndProc arrow math and limits.
float LegacyReference(int index, float value, bool right) {
	if (index < 7) {
		if (index == 1 || index == 3 || index == 5 || index == 6) {
			float add = right ? 5.f : -5.f;
			if (index == 1) add = right ? .05f : -.05f;
			value += add;
		} else {
			float step = right ? 1.1f : .9f;
			if (index == 4) step = right ? 1.05f : .95f;
			value *= step;
		}
		const float lower[] = {.1f, 0.f, 50.f, 0.f, .5f, 0.f, 0.f};
		const float upper[] = {3.f, 2.f, 4000.f, 300.f, 10.f, 500.f, 200.f};
		return std::clamp(value, lower[index], upper[index]);
	}
	if (index < 11) {
		const int wave = index - 7;
		if (wave == 3) value += right ? .05f : -.05f;
		else value *= right ? 1.1f : .9f;
		const float lower[] = {0.f, 0.f, 0.f, .1f};
		const float upper[] = {120.f, 60.f, 30.f, 4.f};
		return std::clamp(value, lower[wave], upper[wave]);
	}
	value += right ? .1f : -.1f;
	return std::clamp(value, 0.f, 2.f);
}
}

TEST(WorkbenchRegistry, InventoryKeysTypesAndPoliciesAreExplicit) {
	std::set<std::string> keys;
	ASSERT_EQ(Descriptors().size(), 25u);
	EXPECT_EQ(GroupCount(Group::Night), 5u); EXPECT_EQ(GroupCount(Group::Flashlight), 7u);
	EXPECT_EQ(GroupCount(Group::Fog), 7u);
	EXPECT_EQ(GroupCount(Group::Waves), 4u);
	EXPECT_EQ(GroupCount(Group::Sun), 2u);
	for (std::size_t i = 0; i < Descriptors().size(); ++i) {
		const auto& d = Descriptors()[i];
		EXPECT_EQ(static_cast<std::size_t>(d.id), i);
		EXPECT_TRUE(keys.insert(d.key).second);
		EXPECT_EQ(FindParameter(d.key), d.id);
		EXPECT_EQ(FindDescriptor(d.id), &d);
		EXPECT_EQ(d.type, i < 13 ? ValueType::Float : i < 22 ? ValueType::Integer : ValueType::ColorChannel);
		EXPECT_EQ(d.scope, Scope::GlobalRuntime);
		EXPECT_EQ(d.apply, ApplyPolicy::Live);
		EXPECT_EQ(d.persist, i < 13 ? PersistPolicy::RuntimeDumpOnly : i < 22 ? PersistPolicy::RecognizedConfigKey : PersistPolicy::RuntimeOnly);
		EXPECT_GT(std::string(d.help).size(), 10u);
		EXPECT_GT(d.sliderStep, 0.f);
		EXPECT_EQ(Validate(d.id, d.compiledDefault), Error::None);
	}
}
TEST(WorkbenchRegistry, FactoryDefaultsMatchTheFormerReInitGameValuesExactly) {
	const Values expected{.23f, .10f, 1488.6f, 85.f, 1.94f, 85.f, 100.f, 14.58f, 8.10f, 5.f, .50f, 1.f, 1.f,
		22, 85, 60, 128, 100, 500, 6000, 14, 26, 1, .902f, .784f};
	EXPECT_TRUE(SameValues(expected, CompiledDefaults()));
}
TEST(WorkbenchRegistry, UnknownAndOutOfBoundsIdsAreRejected) {
	EXPECT_EQ(FindParameter("uwfog_basedensitymult"), Parameter::Count);
	EXPECT_EQ(FindParameter("not_a_key"), Parameter::Count);
	EXPECT_EQ(FindDescriptor(Parameter::Count), nullptr);
	EXPECT_EQ(Validate(static_cast<Parameter>(500), 1.f), Error::UnknownParameter);
	EXPECT_EQ(LegacyParameter(-1, 0), Parameter::Count);
	EXPECT_EQ(LegacyParameter(3, 0), Parameter::Count);
	EXPECT_EQ(LegacyParameter(0, -1), Parameter::Count);
	EXPECT_EQ(LegacyParameter(0, 7), Parameter::Count);
	EXPECT_EQ(LegacyParameter(1, 4), Parameter::Count);
	EXPECT_EQ(LegacyParameter(2, 2), Parameter::Count);
	EXPECT_EQ(LegacyParameter(0, 6), Parameter::FogCameraCap);
	EXPECT_EQ(LegacyParameter(1, 3), Parameter::WaveSpeed);
	EXPECT_EQ(LegacyParameter(2, 1), Parameter::CelestialDisc);
}
TEST(WorkbenchRegistry, EveryBoundaryAndNonFiniteValueIsValidated) {
	for (const auto& d : Descriptors()) {
		EXPECT_EQ(Validate(d.id, d.minimum), Error::None);
		EXPECT_EQ(Validate(d.id, d.maximum), Error::None);
		EXPECT_EQ(Validate(d.id, std::nextafter(d.minimum, -std::numeric_limits<float>::infinity())), Error::OutOfRange);
		EXPECT_EQ(Validate(d.id, std::nextafter(d.maximum, std::numeric_limits<float>::infinity())), Error::OutOfRange);
		EXPECT_EQ(Validate(d.id, std::numeric_limits<float>::quiet_NaN()), Error::NonFinite);
		EXPECT_EQ(Validate(d.id, std::numeric_limits<float>::infinity()), Error::NonFinite);
		EXPECT_EQ(Validate(d.id, -std::numeric_limits<float>::infinity()), Error::NonFinite);
	}
}
TEST(WorkbenchRegistry, DecimalParsingIsCompleteAndDoesNotOverwriteOnFailure) {
	float value = 42.f;
	for (const auto* text : {"", " ", "\t\n"}) {
		EXPECT_EQ(ParseValue(Parameter::FogDensity, text, value), Error::Blank);
		EXPECT_EQ(value, 42.f);
	}
	for (const auto* text : {"0.23garbage", "0,23", "0x1p0", "1 2", "++1", "+-1", "+", "1e", "true"}) {
		EXPECT_EQ(ParseValue(Parameter::FogDensity, text, value), Error::InvalidNumber) << text;
		EXPECT_EQ(value, 42.f);
	}
	for (const auto* text : {"nan", "inf", "-inf"}) {
		EXPECT_EQ(ParseValue(Parameter::FogDensity, text, value), Error::NonFinite) << text;
		EXPECT_EQ(value, 42.f);
	}
	EXPECT_EQ(ParseValue(Parameter::FogDensity, "1e10000", value), Error::OutOfRange);
	EXPECT_EQ(ParseValue(Parameter::FogDensity, "0", value), Error::OutOfRange);
	EXPECT_EQ(ParseValue(Parameter::FogDensity, " \t+.234567e0\r\n", value), Error::None);
	EXPECT_FLOAT_EQ(value, .234567f);
}
TEST(WorkbenchRegistry, LegacyArrowMathIsBitExactAcrossAllFieldsAndEdges) {
	for (const auto& d : Descriptors()) {
		if (static_cast<std::size_t>(d.id) >= kLegacyParameterCount) continue;
		for (float start : {d.minimum, d.maximum, d.compiledDefault, (d.minimum + d.maximum) * .5f}) {
			for (bool right : {false, true}) {
				float oldValue = start, newValue = start;
				for (int step = 0; step < 100; ++step) {
					oldValue = LegacyReference(static_cast<int>(d.id), oldValue, right);
					ASSERT_EQ(LegacyAdjustedValue(d.id, newValue, right, newValue), Error::None);
					EXPECT_TRUE(SameValue(oldValue, newValue)) << d.key << " step " << step;
				}
			}
		}
	}
}
TEST(WorkbenchSession, UnloadedStateDoesNotPermitMutationOrComparison) {
	Session s;
	EXPECT_FALSE(s.IsLoaded()); EXPECT_FALSE(s.CanUndo()); EXPECT_FALSE(s.CanRedo());
	EXPECT_EQ(s.Edit(Parameter::FogDensity, 1.f), Error::NotLoaded);
	EXPECT_EQ(s.ResetAll(), Error::NotLoaded);
	EXPECT_EQ(s.Compare(true), Error::NotLoaded);
	EXPECT_EQ(s.BeginGesture(), Error::NotLoaded);
}
TEST(WorkbenchSession, CaptureUsesLoadedValuesNotFactoryDefaults) {
	auto values = CompiledDefaults(); values[0] = 1.3f;
	Session s; ASSERT_EQ(s.Capture(values), Error::None);
	ASSERT_EQ(s.Edit(Parameter::FogDensity, 2.f), Error::None);
	ASSERT_EQ(s.Reset(Parameter::FogDensity), Error::None);
	EXPECT_FLOAT_EQ(Value(s.Edited(), Parameter::FogDensity), 1.3f);
	EXPECT_EQ(s.ChangedCount(), 0u);
}
TEST(WorkbenchSession, ExactEntryIsNotQuantizedToSliderStep) {
	auto s = LoadedSession();
	ASSERT_EQ(s.EditText(Parameter::FogDensity, "0.234567"), Error::None);
	EXPECT_FLOAT_EQ(Value(s.Edited(), Parameter::FogDensity), .234567f);
	EXPECT_EQ(s.ChangedCount(), 1u);
	EXPECT_TRUE(s.Changed(Parameter::FogDensity));
}
TEST(WorkbenchSession, RejectedEditsLeaveValuesHistoryAndComparisonIntact) {
	auto s = LoadedSession(); s.Edit(Parameter::FogDensity, 1.f); s.Undo();
	const auto before = s.Edited();
	EXPECT_EQ(s.Edit(Parameter::FogDensity, 4.f), Error::OutOfRange);
	EXPECT_EQ(s.EditText(Parameter::FogDensity, ""), Error::Blank);
	EXPECT_EQ(s.Edit(Parameter::Count, 1.f), Error::UnknownParameter);
	EXPECT_TRUE(SameValues(before, s.Edited())); EXPECT_TRUE(s.CanRedo()); EXPECT_FALSE(s.CanUndo());
}
TEST(WorkbenchSession, OneDragIsOneUndoActionAndCanReturnToBaseline) {
	auto s = LoadedSession(); ASSERT_EQ(s.BeginGesture(), Error::None);
	s.BeginGesture(); // Duplicate pointer-down must not overwrite drag origin.
	for (float value : {15.f, 20.f, 25.f}) s.Edit(Parameter::WavePrimary, value);
	EXPECT_TRUE(s.CanUndo()); s.EndGesture();
	ASSERT_TRUE(s.Undo()); EXPECT_TRUE(SameValues(s.Edited(), CompiledDefaults())); EXPECT_FALSE(s.CanUndo());
	ASSERT_TRUE(s.Redo()); EXPECT_FLOAT_EQ(Value(s.Edited(), Parameter::WavePrimary), 25.f);
}
TEST(WorkbenchSession, EmptyAndNetZeroDragsDoNotEraseRedo) {
	auto s = LoadedSession(); s.Edit(Parameter::FogDensity, 1.f); s.Undo();
	s.BeginGesture(); s.EndGesture(); EXPECT_TRUE(s.CanRedo());
	s.BeginGesture(); s.Edit(Parameter::FogDensity, 2.f); EXPECT_FALSE(s.CanRedo());
	s.Edit(Parameter::FogDensity, .23f); s.EndGesture(); EXPECT_TRUE(s.CanRedo()); EXPECT_FALSE(s.CanUndo());
}
TEST(WorkbenchSession, UndoCommitsAnUnfinishedGestureAndNewEditInvalidatesRedo) {
	auto s = LoadedSession(); s.BeginGesture(); s.Edit(Parameter::FogDensity, 1.f);
	ASSERT_TRUE(s.Undo()); EXPECT_FLOAT_EQ(Value(s.Edited(), Parameter::FogDensity), .23f);
	EXPECT_TRUE(s.CanRedo()); s.Edit(Parameter::FogDensity, 2.f); EXPECT_FALSE(s.CanRedo());
}
TEST(WorkbenchSession, ComparisonRetainsDiffAndDisallowsEditing) {
	auto s = LoadedSession(); s.BeginGesture(); s.Edit(Parameter::FogDensity, 1.f);
	ASSERT_EQ(s.Compare(true), Error::None);
	EXPECT_TRUE(SameValues(s.Effective(), s.Loaded())); EXPECT_EQ(s.ChangedCount(), 1u);
	EXPECT_EQ(s.Edit(Parameter::FogDensity, 2.f), Error::Comparing);
	EXPECT_EQ(s.ResetAll(), Error::Comparing); EXPECT_EQ(s.BeginGesture(), Error::Comparing);
	ASSERT_EQ(s.Compare(false), Error::None);
	EXPECT_FLOAT_EQ(Value(s.Effective(), Parameter::FogDensity), 1.f);
	EXPECT_TRUE(s.CanUndo());
}
TEST(WorkbenchSession, UndoDuringComparisonOnlyChangesTheRetainedEdit) {
	auto s = LoadedSession(); s.Edit(Parameter::FogDensity, 1.f); s.Edit(Parameter::FogDensity, 2.f); s.Compare(true);
	ASSERT_TRUE(s.Undo()); EXPECT_TRUE(SameValues(s.Effective(), s.Loaded()));
	s.Compare(false); EXPECT_FLOAT_EQ(Value(s.Effective(), Parameter::FogDensity), 1.f);
}
TEST(WorkbenchSession, FieldAndGroupResetsAreUndoableAndDoNotResetOtherGroups) {
	auto s = LoadedSession(); s.Edit(Parameter::FogDensity, 1.f); s.Edit(Parameter::WavePrimary, 20.f); s.Edit(Parameter::WaveCross, 12.f);
	ASSERT_EQ(s.ResetGroup(Group::Waves), Error::None);
	EXPECT_EQ(s.ChangedCount(), 1u); EXPECT_TRUE(s.Changed(Parameter::FogDensity));
	ASSERT_TRUE(s.Undo()); EXPECT_EQ(s.ChangedCount(), 3u);
	ASSERT_EQ(s.Reset(Parameter::FogDensity), Error::None); EXPECT_EQ(s.ChangedCount(), 2u);
	ASSERT_TRUE(s.Undo()); EXPECT_EQ(s.ChangedCount(), 3u);
}
TEST(WorkbenchSession, WholeSessionResetIsOneUndoAction) {
	auto s = LoadedSession(); s.Edit(Parameter::FogDensity, 1.f); s.Edit(Parameter::WavePrimary, 20.f);
	const auto edited = s.Edited(); ASSERT_EQ(s.ResetAll(), Error::None); EXPECT_EQ(s.ChangedCount(), 0u);
	ASSERT_TRUE(s.Undo()); EXPECT_TRUE(SameValues(s.Edited(), edited));
}
TEST(WorkbenchSession, NoOpEditPreservesRedoAndDoesNotCreateHistory) {
	auto s = LoadedSession(); s.Edit(Parameter::FogDensity, 1.f); s.Undo();
	s.Edit(Parameter::FogDensity, .23f); EXPECT_FALSE(s.CanUndo()); EXPECT_TRUE(s.CanRedo());
}
TEST(WorkbenchSession, SignedZeroAndSingleUlpChangesAreRetainedExactly) {
	auto s = LoadedSession(); s.Edit(Parameter::WaveDetail, 0.f); s.Capture(s.Edited());
	ASSERT_EQ(s.Edit(Parameter::WaveDetail, -0.f), Error::None);
	EXPECT_TRUE(s.Changed(Parameter::WaveDetail)); EXPECT_TRUE(std::signbit(Value(s.Edited(), Parameter::WaveDetail)));
	ASSERT_TRUE(s.Undo()); EXPECT_FALSE(std::signbit(Value(s.Edited(), Parameter::WaveDetail)));
	const float next = std::nextafter(.23f, 1.f); s.Edit(Parameter::FogDensity, next);
	EXPECT_TRUE(SameValue(Value(s.Edited(), Parameter::FogDensity), next));
}
TEST(WorkbenchSession, ReturnedSnapshotsCannotMutateSessionState) {
	auto s = LoadedSession(); auto loaded = s.Loaded(); auto edited = s.Edited(); auto effective = s.Effective();
	loaded[0] = edited[0] = effective[0] = 2.f;
	EXPECT_FLOAT_EQ(Value(s.Edited(), Parameter::FogDensity), .23f); EXPECT_EQ(s.ChangedCount(), 0u);
}
TEST(WorkbenchSession, HistoryIsBoundedAndRedoReplaysRetainedCommands) {
	auto s = LoadedSession();
	for (int i = 0; i < 200; ++i) s.Edit(Parameter::FogDensity, i % 2 ? 1.f : 2.f);
	int undos = 0, redos = 0;
	while (s.Undo()) ++undos;
	while (s.Redo()) ++redos;
	EXPECT_EQ(undos, static_cast<int>(kHistoryLimit)); EXPECT_EQ(redos, undos);
	EXPECT_FLOAT_EQ(Value(s.Edited(), Parameter::FogDensity), 1.f);
}
TEST(WorkbenchSession, RecaptureInvalidatesComparisonHistoryAndOpenGesture) {
	auto s = LoadedSession(); const auto generation = s.Generation();
	s.Edit(Parameter::FogDensity, 1.f); s.Compare(true);
	auto values = CompiledDefaults(); values[0] = .8f; ASSERT_EQ(s.Capture(values), Error::None);
	EXPECT_GT(s.Generation(), generation); EXPECT_FALSE(s.IsComparing()); EXPECT_FALSE(s.CanUndo()); EXPECT_FALSE(s.CanRedo());
	s.BeginGesture(); s.Edit(Parameter::FogDensity, 1.1f); s.Capture(values); s.EndGesture();
	EXPECT_FALSE(s.CanUndo()); EXPECT_TRUE(SameValues(s.Effective(), values));
}
TEST(WorkbenchAtmosphere, IntegerSettingsRejectFractionsWithoutRoundingOrPublishing) {
	for (const auto& d : Descriptors()) if (d.type == ValueType::Integer) {
		SCOPED_TRACE(d.key); auto s = LoadedSession();
		const auto before = s.Edited(); float parsed = -123;
		EXPECT_EQ(ParseValue(d.id, "22.00000001", parsed), Error::IntegerRequired);
		EXPECT_EQ(parsed, -123);
		EXPECT_EQ(s.Edit(d.id, d.compiledDefault + .5f), Error::IntegerRequired);
		EXPECT_EQ(s.EditText(d.id, "nan"), Error::NonFinite);
		EXPECT_EQ(s.Edited(), before); EXPECT_FALSE(s.CanUndo());
	}
	float parsed = 0;
	EXPECT_EQ(ParseValue(Parameter::NightWorld, " +42 ", parsed), Error::None); EXPECT_EQ(parsed, 42);
	EXPECT_EQ(Validate(Parameter::MoonBrightness, -0.f), Error::IntegerRequired);
}
TEST(WorkbenchAtmosphere, ConeEditsAndResetsValidateTheWholePairTransactionally) {
	auto s = LoadedSession(); const auto before = s.Edited();
	EXPECT_EQ(s.Edit(Parameter::FlashlightInner, 26), Error::InvalidCone);
	EXPECT_EQ(s.Edit(Parameter::FlashlightOuter, 14), Error::InvalidCone);
	EXPECT_EQ(s.Edited(), before); EXPECT_FALSE(s.CanUndo());
	ASSERT_EQ(s.Edit(Parameter::FlashlightOuter, 60), Error::None);
	ASSERT_EQ(s.Edit(Parameter::FlashlightInner, 50), Error::None);
	EXPECT_EQ(s.Reset(Parameter::FlashlightOuter), Error::InvalidCone);
	ASSERT_EQ(s.ResetGroup(Group::Flashlight), Error::None);
	EXPECT_EQ(s.Edited(), before);
	ASSERT_TRUE(s.Undo()); EXPECT_EQ(Value(s.Edited(), Parameter::FlashlightInner), 50);
	EXPECT_EQ(Value(s.Edited(), Parameter::FlashlightOuter), 60);
}
TEST(WorkbenchAtmosphere, ColorIsAnAtomicValidatedReversibleEdit) {
	auto s = LoadedSession(); const auto before = s.Edited();
	EXPECT_EQ(s.EditFlashlightColor({.1f, std::numeric_limits<float>::quiet_NaN(), .3f}), Error::NonFinite);
	EXPECT_EQ(s.EditFlashlightColor({.1f, .2f, 2}), Error::OutOfRange);
	EXPECT_EQ(s.Edited(), before); EXPECT_FALSE(s.CanUndo());
	ASSERT_EQ(s.EditFlashlightColor({.1f, .2f, .3f}), Error::None);
	EXPECT_EQ(s.ChangedCount(), 3u);
	ASSERT_TRUE(s.Undo()); EXPECT_EQ(s.Edited(), before); EXPECT_FALSE(s.CanUndo());
	ASSERT_TRUE(s.Redo()); s.Compare(true); EXPECT_EQ(s.Effective(), before);
	EXPECT_EQ(s.EditFlashlightColor({.4f, .5f, .6f}), Error::Comparing);
	s.Compare(false); EXPECT_FLOAT_EQ(Value(s.Effective(), Parameter::FlashlightBlue), .3f);
}
TEST(WorkbenchSession, InvalidBaselineCannotLeaveAnOldHuntSessionUsable) {
	auto s = LoadedSession(); s.Edit(Parameter::FogDensity, 1.f);
	auto invalid = CompiledDefaults(); invalid[0] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(s.Capture(invalid), Error::NonFinite);
	EXPECT_FALSE(s.IsLoaded()); EXPECT_FALSE(s.CanUndo()); EXPECT_EQ(s.ChangedCount(), 0u);
	EXPECT_EQ(s.Edit(Parameter::FogDensity, 1.f), Error::NotLoaded);
}
