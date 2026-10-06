#include <cstdio>
#include "TuningRuntime.h"
#include "Core/GameState.h"

namespace workbench {
namespace {
Session session;
// Parameter enum order is covered against these production globals by native
// adapter tests. Storage is process-lifetime scalar state, not level resources.
const std::array<float*, kParameterCount> bindings{{
	&UWFog_BaseDensityMult, &UWFog_CameraDepthMult, &UWFog_VertRange,
	&UWFog_VertStrength, &UWFog_CurveExp, &UWFog_CapBase, &UWFog_CapCameraBoost,
	&WWave1Amp, &WWave2Amp, &WWave3Amp, &WWaveSpeed,
	&SunGlare_Master, &SunGlare_Disc
}};
void WriteRuntimeValues(const Values& values) {
	for (std::size_t i = 0; i < values.size(); ++i) *bindings[i] = values[i];
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
template <typename Operation> Error Mutate(Operation operation) {
	const auto current = CheckCurrent();
	if (current != Error::None) return current;
	const auto error = operation();
	if (error == Error::None) WriteRuntimeValues(session.Effective());
	return error;
}
}
const Session& RuntimeSession() { return session; }
Values ReadRuntimeValues() {
	Values values{};
	for (std::size_t i = 0; i < values.size(); ++i) values[i] = *bindings[i];
	return values;
}
Error CaptureRuntimeBaseline() { return session.Capture(ReadRuntimeValues()); }
Error ResetRuntimeForHunt() {
	WriteRuntimeValues(CompiledDefaults());
	return CaptureRuntimeBaseline();
}
Error EditRuntime(Parameter id, float value) { return Mutate([&] { return session.Edit(id, value); }); }
Error EditRuntimeText(Parameter id, std::string_view text) { return Mutate([&] { return session.EditText(id, text); }); }
Error AdjustLegacyRuntime(Parameter id, bool increase) {
	if (!FindDescriptor(id)) return Error::UnknownParameter;
	return Mutate([&] {
		float value = 0;
		const auto error = LegacyAdjustedValue(id, session.Effective()[static_cast<std::size_t>(id)], increase, value);
		return error == Error::None ? session.Edit(id, value) : error;
	});
}
Error ResetRuntime(Parameter id) { return Mutate([&] { return session.Reset(id); }); }
Error ResetRuntimeGroup(Group group) { return Mutate([&] { return session.ResetGroup(group); }); }
Error ResetRuntimeAll() { return Mutate([] { return session.ResetAll(); }); }
Error CompareRuntime(bool loaded) { return Mutate([&] { return session.Compare(loaded); }); }
Error BeginRuntimeGesture() { return Mutate([] { return session.BeginGesture(); }); }
Error EndRuntimeGesture() { return Mutate([] { session.EndGesture(); return Error::None; }); }
Error UndoRuntime() { return Mutate([] { session.Undo(); return Error::None; }); }
Error RedoRuntime() { return Mutate([] { session.Redo(); return Error::None; }); }

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
