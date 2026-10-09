#pragma once
#include <array>
#include <cstdint>

namespace c2audio {
using Environments = std::array<std::array<float, 4>, 9>;
inline Environments Defaults() {
	return {{{1.49f,.3f,.4f,-620}, {1.2f,.2f,.2f,-600}, {1.49f,.18f,.3f,-700},
		{2.6f,.2f,.326f,-500}, {1.1f,.6f,.275f,-700}, {3.2f,.6f,.9f,-200},
		{1.8f,.4f,.4f,-400}, {0,0,0,0}, {1.5f,.1f,.1f,-200}}};
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
