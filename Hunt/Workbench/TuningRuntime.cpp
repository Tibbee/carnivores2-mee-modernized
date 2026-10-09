#include <cstdio>
#include <algorithm>
#include <utility>
#include "Core/NightLighting.h"
#include "Audio/AudioInspection.h"
#include "TuningRuntime.h"
#include "Core/GameState.h"

namespace workbench {
namespace {
Session session;
// Parameter enum order is covered against these production globals by native
// adapter tests. Storage is process-lifetime scalar state, not level resources.
const std::array<float*, kLegacyParameterCount> bindings{{
	&UWFog_BaseDensityMult, &UWFog_CameraDepthMult, &UWFog_VertRange,
	&UWFog_VertStrength, &UWFog_CurveExp, &UWFog_CapBase, &UWFog_CapCameraBoost,
	&WWave1Amp, &WWave2Amp, &WWave3Amp, &WWaveSpeed,
	&SunGlare_Master, &SunGlare_Disc
}};
const std::array<int*, 9> integerBindings{{
	&NightLightingOptions().worldBrightnessPct, &NightLightingOptions().desaturationPct,
	&NightLightingOptions().skyBrightnessPct, &NightLightingOptions().moonBrightnessPct,
	&NightLightingOptions().moonGlowStrengthPct, &OptFlashlightIntensityPct,
	&OptFlashlightRange, &OptFlashlightInnerAngleDeg, &OptFlashlightOuterAngleDeg
}};
Error WriteRuntimeValues(const Values& values, bool requireCapability = true) {
	const auto expected = c2audio::ReadEnvironments();
	auto audio = expected;
	for (std::size_t i = kAudioParameterBegin; i < kParameterCount; ++i)
		audio[AudioEnvironment(static_cast<Parameter>(i))][(i - kAudioParameterBegin) % 4] = values[i];
	const auto result = c2audio::WriteEnvironments(expected, audio, requireCapability);
	if (result != c2audio::WriteResult::Ok) return result == c2audio::WriteResult::Unavailable ? Error::Unavailable : Error::RuntimeChanged;
	for (std::size_t i = 0; i < bindings.size(); ++i) *bindings[i] = values[i];
	for (std::size_t i = 0; i < integerBindings.size(); ++i) *integerBindings[i] = static_cast<int>(values[kLegacyParameterCount + i]);
	for (std::size_t i = 0; i < 3; ++i) FlashlightTint()[i] = values[static_cast<std::size_t>(Parameter::FlashlightRed) + i];
	return Error::None;
}
Error CheckCurrent() {
	if (!session.IsLoaded()) return Error::NotLoaded;
	if (!SameValues(ReadRuntimeValues(), session.Effective())) {
		// Do not overwrite external edits or replay commands from an obsolete
		// baseline. A future GUI must offer explicit recapture on this error.
		session.Invalidate();
		return Error::RuntimeChanged;
	}
	return Error::None;
}
template <typename Operation> Error Mutate(Operation operation, bool requireCapability = true) {
	const auto current = CheckCurrent();
	if (current != Error::None) return current;
	// Publish before committing history: unavailable audio must not half-apply a
	// cross-group reset/compare/undo or create a fictitious accepted command.
	auto candidate = session;
	const auto error = operation(candidate);
	if (error != Error::None) return error;
	if (!SameValues(candidate.Effective(), session.Effective())) {
		const auto published = WriteRuntimeValues(candidate.Effective(), requireCapability);
		if (published != Error::None) return published;
	}
	session = std::move(candidate);
	return Error::None;
}
}
const Session& RuntimeSession() { return session; }
Error SynchronizeRuntimeSession() { return CheckCurrent(); }
Values ReadRuntimeValues() {
	Values values{};
	for (std::size_t i = 0; i < bindings.size(); ++i) values[i] = *bindings[i];
	for (std::size_t i = 0; i < integerBindings.size(); ++i) values[kLegacyParameterCount + i] = static_cast<float>(*integerBindings[i]);
	for (std::size_t i = 0; i < 3; ++i) values[static_cast<std::size_t>(Parameter::FlashlightRed) + i] = FlashlightTint()[i];
	const auto audio = c2audio::ReadEnvironments();
	for (std::size_t i = kAudioParameterBegin; i < kParameterCount; ++i)
		values[i] = audio[AudioEnvironment(static_cast<Parameter>(i))][(i - kAudioParameterBegin) % 4];
	return values;
}
Error CaptureRuntimeBaseline() { return session.Capture(ReadRuntimeValues()); }
Error ResetRuntimeForHunt() {
	auto values = ReadRuntimeValues();
	// Preserve freshly loaded/external configuration. Only undo this session's
	// atmosphere preview when it still owns the effective runtime snapshot.
	if (session.IsLoaded() && SameValues(values, session.Effective())) values = session.Loaded();
	const auto defaults = CompiledDefaults();
	for (std::size_t i = 0; i < kLegacyParameterCount; ++i) {
		values[i] = defaults[i]; *bindings[i] = defaults[i];
	}
	// Hunt lifecycle owns these legacy resets even when invalid diagnostic
	// state cannot form a strictly validated workbench baseline. Do not
	// let diagnostic/tuning availability change ordinary restart behavior.
	if (const auto error = ValidateValues(values); error != Error::None) { session.Invalidate(); return error; }
	if (const auto error = WriteRuntimeValues(values, false); error != Error::None) { session.Invalidate(); return error; }
	return CaptureRuntimeBaseline();
}
Error EditRuntime(Parameter id, float value) { return Mutate([&](Session& s) { return s.Edit(id, value); }); }
Error EditRuntimeText(Parameter id, std::string_view text) { return Mutate([&](Session& s) { return s.EditText(id, text); }); }
Error EditRuntimeFlashlightColor(const std::array<float, 3>& color) {
	return Mutate([&](Session& s) { return s.EditFlashlightColor(color); });
}
EnvironmentContext ReadEnvironmentContext() {
	EnvironmentContext state;
	state.project.assign(ProjectName, std::find(ProjectName, ProjectName + sizeof(ProjectName), '\0'));
	state.timeOfDay = OptDayNight; state.fogIndex = CameraFogI;
	state.underwater = IsUnderwater(); state.inPocketFog = CAMERAINFOG != 0 && !state.underwater;
	state.requestedEnvironment = state.underwater ? 8 : Env;
	state.nightVision = NightVisionOn != 0;
	state.flashlightEquipped = FlashlightMode != 0; state.flashlightSwitchedOn = FlashlightOn != 0;
	state.flashlightIlluminating = FlashlightShouldIlluminate(FlashlightMode, FlashlightOn, OptFlashlightIntensityPct, MyHealth);
	return state;
}
Error AdjustLegacyRuntime(Parameter id, bool increase) {
	if (!FindDescriptor(id)) return Error::UnknownParameter;
	return Mutate([&](Session& s) {
		float value = 0;
		const auto error = LegacyAdjustedValue(id, s.Effective()[static_cast<std::size_t>(id)], increase, value);
		return error == Error::None ? s.Edit(id, value) : error;
	});
}
Error ResetRuntime(Parameter id) { return Mutate([&](Session& s) { return s.Reset(id); }); }
Error ResetRuntimeGroup(Group group) { return Mutate([&](Session& s) { return s.ResetGroup(group); }); }
Error ResetRuntimeAudioBank(int bank) { return Mutate([&](Session& s) { return s.ResetAudioBank(bank); }); }
Error ResetRuntimeAll() { return Mutate([](Session& s) { return s.ResetAll(); }); }
Error CompareRuntime(bool loaded) { return Mutate([&](Session& s) { return s.Compare(loaded); }, loaded); }
Error BeginRuntimeGesture() { return Mutate([](Session& s) { return s.BeginGesture(); }); }
Error EndRuntimeGesture() { return Mutate([](Session& s) { s.EndGesture(); return Error::None; }); }
Error UndoRuntime() { return Mutate([](Session& s) { s.Undo(); return Error::None; }); }
Error RedoRuntime() { return Mutate([](Session& s) { s.Redo(); return Error::None; }); }

std::string FormatRuntimeDump() {
	const auto values = ReadRuntimeValues();
	std::string dump = "=== UNDERWATER DEBUG VALUES ===\n";
	char line[128];
	for (const Group group : {Group::Fog, Group::Waves, Group::Sun}) {
		dump += group == Group::Fog ? "--- Fog ---\n" : group == Group::Waves ? "--- Waves ---\n" : "--- Sun glare ---\n";
		for (const auto& d : Descriptors()) if (d.group == group) {
			std::snprintf(line, sizeof(line), "%s = %.*f\n", d.key, d.dumpDecimals, values[static_cast<std::size_t>(d.id)]);
			dump += line;
		}
		if (group == Group::Fog) {
			std::snprintf(line, sizeof(line), "CameraWaterDepthFactor = %.3f\n", CameraWaterDepthFactor);
			dump += line;
		}
	}
	dump += "=== END DEBUG VALUES ===\n";
	return dump;
}
void EmitRuntimeDump(void (*sink)(char*)) {
	if (!sink) return;
	const auto dump = FormatRuntimeDump();
	std::size_t start = 0;
	for (auto end = dump.find('\n'); end != std::string::npos; end = dump.find('\n', start)) {
		// PrintLog converts only its final LF to CRLF, not interior newlines.
		// Passing the whole dump would silently change the existing log bytes.
		auto line = dump.substr(start, end - start + 1);
		sink(line.data());
		start = end + 1;
	}
}
} // namespace workbench
