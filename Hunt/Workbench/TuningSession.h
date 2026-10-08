#pragma once

// UI-independent W1 tuning model. No window, renderer, engine-global or file I/O.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace workbench {

enum class Parameter : std::size_t {
	FogDensity, FogCameraDepth, FogVerticalRange, FogVerticalStrength,
	FogCurve, FogCap, FogCameraCap,
	WavePrimary, WaveCross, WaveDetail, WaveSpeed,
	SunGlare, CelestialDisc,
	NightWorld, NightDesaturation, NightSky, MoonBrightness, MoonGlow,
	FlashlightIntensity, FlashlightRange, FlashlightInner, FlashlightOuter,
	FlashlightRed, FlashlightGreen, FlashlightBlue,
	Count
};
enum class Group { Fog, Waves, Sun, Night, Flashlight };
enum class ValueType { Float, Integer, ColorChannel };
enum class Scope { GlobalRuntime };
enum class ApplyPolicy { Live };
enum class PersistPolicy { RuntimeDumpOnly, RecognizedConfigKey, RuntimeOnly };
enum class LegacyAdjustment { Multiply, Add };
enum class Error {
	None, UnknownParameter, NotLoaded, Blank, InvalidNumber, NonFinite,
	OutOfRange, Comparing, RuntimeChanged, IntegerRequired, InvalidCone
};

constexpr std::size_t kParameterCount = static_cast<std::size_t>(Parameter::Count);
constexpr std::size_t kLegacyParameterCount = 13;
constexpr std::size_t kHistoryLimit = 128;
using Values = std::array<float, kParameterCount>;

struct Descriptor {
	Parameter id;
	Group group;
	const char* label;
	const char* key;
	const char* legacyLabel;
	const char* unit;
	const char* help;
	float minimum;
	float maximum;
	float sliderStep;
	float compiledDefault;
	LegacyAdjustment legacyAdjustment;
	float legacyIncrease;
	float legacyDecrease;
	int dumpDecimals;
	ValueType type = ValueType::Float;
	Scope scope = Scope::GlobalRuntime;
	ApplyPolicy apply = ApplyPolicy::Live;
	PersistPolicy persist = PersistPolicy::RuntimeDumpOnly;
};

const std::array<Descriptor, kParameterCount>& Descriptors();
const Descriptor* FindDescriptor(Parameter id);
Parameter FindParameter(std::string_view key);
Parameter LegacyParameter(int tab, int selected);
std::size_t GroupCount(Group group);
Values CompiledDefaults();
const char* ErrorMessage(Error error);
bool SameValue(float a, float b);
bool SameValues(const Values& a, const Values& b);
Error Validate(Parameter id, float value);
Error ValidateValues(const Values& values);
Error ParseValue(Parameter id, std::string_view text, float& value);
Error LegacyAdjustedValue(Parameter id, float current, bool increase, float& value);

class Session {
public:
	// Capture is all-or-nothing. A failed/new capture invalidates old history,
	// comparison and gestures, so a restart can never replay the previous hunt.
	Error Capture(const Values& loaded);
	void Invalidate();
	bool IsLoaded() const { return m_loaded; }
	std::uint64_t Generation() const { return m_generation; }
	Values Loaded() const { return m_baseline; }
	Values Edited() const { return m_edited; }
	Values Effective() const { return m_comparing ? m_baseline : m_edited; }
	bool IsComparing() const { return m_comparing; }
	bool Changed(Parameter id) const;
	std::size_t ChangedCount() const;

	Error Edit(Parameter id, float value);
	Error EditText(Parameter id, std::string_view text);
	Error EditFlashlightColor(const std::array<float, 3>& color);
	Error Reset(Parameter id);
	Error ResetGroup(Group group);
	Error ResetAll();
	Error Compare(bool loaded);
	Error BeginGesture();
	void EndGesture();
	bool CanUndo() const;
	bool CanRedo() const;
	bool Undo();
	bool Redo();

private:
	struct Command { Values before, after; };
	Error Replace(const Values& values);
	void Record(const Values& before, const Values& after);
	bool m_loaded = false;
	bool m_comparing = false;
	bool m_gesture = false;
	std::uint64_t m_generation = 0;
	Values m_baseline{}, m_edited{}, m_gestureBefore{};
	std::vector<Command> m_undo, m_redo;
};

} // namespace workbench
