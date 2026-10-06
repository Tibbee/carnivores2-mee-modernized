#include "TuningSession.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <system_error>

namespace workbench {
namespace {
// Source: the former MainWndProc F10 bounds/steps and ReInitGame defaults.
// These are runtime control keys, NOT recognized config.cfg entries.
const std::array<Descriptor, kParameterCount> registry{{
	{Parameter::FogDensity, Group::Fog, "Base density", "UWFog_BaseDensityMult", "BaseDensity", "multiplier",
	 "Scales the underwater distance-density demand; pocket fog is unaffected.",
	 .1f, 3.f, .01f, .23f, LegacyAdjustment::Multiply, 1.1f, .9f, 2},
	{Parameter::FogCameraDepth, Group::Fog, "Camera-depth gain", "UWFog_CameraDepthMult", "CamDepthMult", "multiplier",
	 "Adds camera-depth extinction to the underwater density demand.",
	 0.f, 2.f, .01f, .10f, LegacyAdjustment::Add, .05f, -.05f, 2},
	{Parameter::FogVerticalRange, Group::Fog, "Vertical gradient range", "UWFog_VertRange", "VertRange", "world units",
	 "Depth span over which the underwater vertical fog gradient builds.",
	 50.f, 4000.f, 1.f, 1488.6f, LegacyAdjustment::Multiply, 1.1f, .9f, 1},
	{Parameter::FogVerticalStrength, Group::Fog, "Vertical gradient strength", "UWFog_VertStrength", "VertStrength", "fog units",
	 "Adds underwater fog at the maximum vertical gradient depth.",
	 0.f, 300.f, 1.f, 85.f, LegacyAdjustment::Add, 5.f, -5.f, 1},
	{Parameter::FogCurve, Group::Fog, "Gradient exponent", "UWFog_CurveExp", "CurveExp", "exponent",
	 "Shapes the underwater gradient; larger values delay its initial rise.",
	 .5f, 10.f, .01f, 1.94f, LegacyAdjustment::Multiply, 1.05f, .95f, 2},
	{Parameter::FogCap, Group::Fog, "Fog cap addition", "UWFog_CapBase", "CapBase", "fog units",
	 "Adds to the underwater body's fog limit before the soft cap.",
	 0.f, 500.f, 1.f, 85.f, LegacyAdjustment::Add, 5.f, -5.f, 1},
	{Parameter::FogCameraCap, Group::Fog, "Camera-depth cap boost", "UWFog_CapCameraBoost", "CapCamBoost", "fog units",
	 "Raises the underwater soft cap as the camera descends.",
	 0.f, 200.f, 1.f, 100.f, LegacyAdjustment::Add, 5.f, -5.f, 1},
	{Parameter::WavePrimary, Group::Waves, "Primary swell", "WWave1Amp", "Wave1Amp", "world units",
	 "Amplitude of the primary water-wave term; does not change its equation.",
	 0.f, 120.f, .01f, 14.58f, LegacyAdjustment::Multiply, 1.1f, .9f, 2},
	{Parameter::WaveCross, Group::Waves, "Cross-wave", "WWave2Amp", "Wave2Amp", "world units",
	 "Amplitude of the secondary water-wave term.",
	 0.f, 60.f, .01f, 8.10f, LegacyAdjustment::Multiply, 1.1f, .9f, 2},
	{Parameter::WaveDetail, Group::Waves, "Fine wave detail", "WWave3Amp", "Wave3Amp", "world units",
	 "Amplitude of the fine-detail water-wave term.",
	 0.f, 30.f, .01f, 5.f, LegacyAdjustment::Multiply, 1.1f, .9f, 2},
	{Parameter::WaveSpeed, Group::Waves, "Wave speed", "WWaveSpeed", "WaveSpeed", "time multiplier",
	 "Scales water-wave time without changing the wave math.",
	 .1f, 4.f, .01f, .50f, LegacyAdjustment::Add, .05f, -.05f, 2},
	{Parameter::SunGlare, Group::Sun, "Sun glare", "SunGlare_Master", "GlareMaster", "multiplier",
	 "Scales the fullscreen daytime sun glare; zero disables that wash.",
	 0.f, 2.f, .01f, 1.f, LegacyAdjustment::Add, .1f, -.1f, 2},
	{Parameter::CelestialDisc, Group::Sun, "Sun / moon disc", "SunGlare_Disc", "GlareDisc", "multiplier",
	 "Scales the sky disc opacity, independently of moon RGB brightness and glow.",
	 0.f, 2.f, .01f, 1.f, LegacyAdjustment::Add, .1f, -.1f, 2}
}};

bool Finite(float value) {
	// Bit test remains correct under the game's /fp:fast release policy.
	static_assert(sizeof(float) == sizeof(std::uint32_t), "32-bit float required");
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	return (bits & 0x7f800000u) != 0x7f800000u;
}
bool Space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }
}

const std::array<Descriptor, kParameterCount>& Descriptors() { return registry; }
const Descriptor* FindDescriptor(Parameter id) {
	const auto index = static_cast<std::size_t>(id);
	return index < registry.size() ? &registry[index] : nullptr;
}
Parameter FindParameter(std::string_view key) {
	for (const auto& d : registry) if (key == d.key) return d.id;
	return Parameter::Count;
}
Parameter LegacyParameter(int tab, int selected) {
	if (tab < 0 || tab > 2 || selected < 0) return Parameter::Count;
	const Group group = tab == 0 ? Group::Fog : tab == 1 ? Group::Waves : Group::Sun;
	for (const auto& d : registry) if (d.group == group && selected-- == 0) return d.id;
	return Parameter::Count;
}
std::size_t GroupCount(Group group) {
	std::size_t count = 0;
	for (const auto& d : registry) if (d.group == group) ++count;
	return count;
}
Values CompiledDefaults() {
	Values values{};
	for (const auto& d : registry) values[static_cast<std::size_t>(d.id)] = d.compiledDefault;
	return values;
}
const char* ErrorMessage(Error error) {
	switch (error) {
	case Error::None: return "";
	case Error::UnknownParameter: return "Unknown tuning parameter.";
	case Error::NotLoaded: return "No loaded tuning baseline. Capture the current values first.";
	case Error::Blank: return "Enter a number; blank is not zero.";
	case Error::InvalidNumber: return "Enter a complete decimal number using a decimal point.";
	case Error::NonFinite: return "NaN and infinity cannot be applied.";
	case Error::OutOfRange: return "Value is outside this control's supported range.";
	case Error::Comparing: return "Return to edited values before editing.";
	case Error::RuntimeChanged: return "Runtime values changed outside this session. Capture a fresh baseline before editing.";
	}
	return "Invalid tuning operation.";
}
bool SameValue(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }
bool SameValues(const Values& a, const Values& b) { return std::memcmp(a.data(), b.data(), sizeof(float) * a.size()) == 0; }
Error Validate(Parameter id, float value) {
	const auto* d = FindDescriptor(id);
	if (!d) return Error::UnknownParameter;
	if (!Finite(value)) return Error::NonFinite;
	return value < d->minimum || value > d->maximum ? Error::OutOfRange : Error::None;
}
Error ParseValue(Parameter id, std::string_view text, float& value) {
	if (!FindDescriptor(id)) return Error::UnknownParameter;
	while (!text.empty() && Space(text.front())) text.remove_prefix(1);
	while (!text.empty() && Space(text.back())) text.remove_suffix(1);
	if (text.empty()) return Error::Blank;
	// from_chars deliberately ignores the process locale and leading '+'.
	if (text.front() == '+') {
		text.remove_prefix(1);
		if (text.empty() || text.front() == '+' || text.front() == '-') return Error::InvalidNumber;
	}
	float parsed = 0;
	const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (result.ec == std::errc::result_out_of_range) return Error::OutOfRange;
	if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return Error::InvalidNumber;
	const auto error = Validate(id, parsed);
	if (error == Error::None) value = parsed;
	return error;
}
Error LegacyAdjustedValue(Parameter id, float current, bool increase, float& value) {
	const auto* d = FindDescriptor(id);
	if (!d) return Error::UnknownParameter;
	if (!Finite(current)) return Error::NonFinite;
	const float step = increase ? d->legacyIncrease : d->legacyDecrease;
	// Preserve the exact old addition/multiplication and clamp order.
	const float adjusted = d->legacyAdjustment == LegacyAdjustment::Add ? current + step : current * step;
	if (!Finite(adjusted)) return Error::NonFinite;
	value = std::clamp(adjusted, d->minimum, d->maximum);
	return Error::None;
}

void Session::Invalidate() {
	++m_generation;
	m_loaded = m_comparing = m_gesture = false;
	m_baseline = {}; m_edited = {}; m_gestureBefore = {};
	m_undo.clear(); m_redo.clear();
}
Error Session::Capture(const Values& loaded) {
	Invalidate();
	for (const auto& d : registry) {
		const auto error = Validate(d.id, loaded[static_cast<std::size_t>(d.id)]);
		if (error != Error::None) return error;
	}
	m_baseline = m_edited = loaded;
	m_loaded = true;
	return Error::None;
}
bool Session::Changed(Parameter id) const
{
	const auto index = static_cast<std::size_t>(id);
	return m_loaded && index < kParameterCount && !SameValue(m_edited[index], m_baseline[index]);
}
std::size_t Session::ChangedCount() const
{
	std::size_t count = 0;
	for (const auto& d : registry) if (Changed(d.id)) ++count;
	return count;
}
void Session::Record(const Values& before, const Values& after) {
	if (SameValues(before, after)) return;
	if (m_undo.size() == kHistoryLimit) m_undo.erase(m_undo.begin());
	m_undo.push_back({before, after});
	m_redo.clear();
}
Error Session::Replace(const Values& values) {
	if (!m_loaded) return Error::NotLoaded;
	if (m_comparing) return Error::Comparing;
	for (const auto& d : registry) {
		const auto error = Validate(d.id, values[static_cast<std::size_t>(d.id)]);
		if (error != Error::None) return error;
	}
	if (!m_gesture) Record(m_edited, values);
	m_edited = values;
	return Error::None;
}
Error Session::Edit(Parameter id, float value) {
	const auto error = Validate(id, value);
	if (error != Error::None) return error;
	auto values = m_edited;
	values[static_cast<std::size_t>(id)] = value;
	return Replace(values);
}
Error Session::EditText(Parameter id, std::string_view text) {
	float value = 0;
	const auto error = ParseValue(id, text, value);
	return error == Error::None ? Edit(id, value) : error;
}
Error Session::Reset(Parameter id) {
	if (!FindDescriptor(id)) return Error::UnknownParameter;
	if (!m_loaded) return Error::NotLoaded;
	return Edit(id, m_baseline[static_cast<std::size_t>(id)]);
}
Error Session::ResetGroup(Group group) {
	auto values = m_edited;
	for (const auto& d : registry) if (d.group == group) values[static_cast<std::size_t>(d.id)] = m_baseline[static_cast<std::size_t>(d.id)];
	return Replace(values);
}
Error Session::ResetAll() { return Replace(m_baseline); }
Error Session::Compare(bool loaded) {
	if (!m_loaded) return Error::NotLoaded;
	EndGesture(); m_comparing = loaded;
	return Error::None;
}
Error Session::BeginGesture() {
	if (!m_loaded) return Error::NotLoaded;
	if (m_comparing) return Error::Comparing;
	if (!m_gesture) { m_gestureBefore = m_edited; m_gesture = true; }
	return Error::None;
}
void Session::EndGesture() {
	if (!m_gesture) return;
	m_gesture = false;
	Record(m_gestureBefore, m_edited);
}
bool Session::CanUndo() const { return !m_undo.empty() || (m_gesture && !SameValues(m_gestureBefore, m_edited)); }
bool Session::CanRedo() const { return !m_redo.empty() && (!m_gesture || SameValues(m_gestureBefore, m_edited)); }
bool Session::Undo() {
	EndGesture();
	if (m_undo.empty()) return false;
	const auto command = m_undo.back(); m_undo.pop_back();
	m_edited = command.before; m_redo.push_back(command);
	return true;
}
bool Session::Redo() {
	EndGesture();
	if (m_redo.empty()) return false;
	const auto command = m_redo.back(); m_redo.pop_back();
	m_edited = command.after; m_undo.push_back(command);
	return true;
}
} // namespace workbench
