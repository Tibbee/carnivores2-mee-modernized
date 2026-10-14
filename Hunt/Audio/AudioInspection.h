#pragma once
#include <array>
#include <cstdint>

namespace c2audio {
using Environments = std::array<std::array<float, 4>, 9>;
// The one owner of the compiled preset table: {decay, decayHF, diffusion,
// reverb} for presets 0-8 (7 is the intentional dry no-op). The engine seeds
// its runtime copy from this and the workbench registry reads it for the
// compiled defaults, so the values cannot drift between the two.
inline constexpr Environments Defaults() {
	return {{{1.49f,.3f,.4f,-620}, {1.2f,.2f,.2f,-600}, {1.49f,.18f,.3f,-700},
		{2.6f,.2f,.326f,-500}, {1.1f,.6f,.275f,-700}, {3.2f,.6f,.9f,-200},
		{1.8f,.4f,.4f,-400}, {0,0,0,0}, {1.5f,.1f,.1f,-200}}};
}
inline const char* EnvironmentLabel(int id) {
	static const char* const labels[] = {"0 Generic", "1 Plate", "2 Forest", "3 Mountain", "4 Canyon",
		"5 Cave", "6 Special 2", "7 Dry", "8 Underwater"};
	return id >= 0 && id < 9 ? labels[id] : "Unknown / not confirmed";
}
enum class Backend { Disabled, OpenAL, Legacy };
enum class Application { Pending, Reverb, Dry, DryFallback, Failed, Disabled, LegacyUnverified };
enum class WriteResult { Ok, Invalid, Changed, Unavailable };
struct VoiceSnapshot {
	bool known = false, playing = false, relative = false;
	float x = 0, y = 0, z = 0, configuredGain = 0, distance = -1;
	int fade = -1;
};
struct Snapshot {
	Backend backend = Backend::Disabled;
	Application application = Application::Disabled;
	bool reverbAvailable = false, details = false;
	int requested = -1, applied = -1;
	std::array<float, 4> appliedValues{};
	std::array<VoiceSnapshot, 19> voices{}; // 16 voices, two beds, moving ambient.
};
// Main-thread facades. The UI receives independent copies, never AL/PCM handles.
// Detailed sampling is explicit; disabled/legacy/status-only reads never query AL.
Environments ReadEnvironments();
Snapshot ReadSnapshot(bool details = false);
// Compare-and-publish the full validated table. Failed AL application is reported
// in Snapshot, not confused with rejection of the owned runtime preview values.
WriteResult WriteEnvironments(const Environments& expected, const Environments& values,
	bool requireCapability = true);
const char* ApplicationName(Application application);
}
