#include "workbench_audio_seams.h"
#include <cstring>
namespace workbench_test {
c2audio::Environments audio = c2audio::Defaults();
bool audioAvailable = true;
int audioSamples = 0, audioDetailSamples = 0;
void ResetAudio() { audio = c2audio::Defaults(); audioAvailable = true; audioSamples = audioDetailSamples = 0; }
}
namespace c2audio {
Environments ReadEnvironments() { return workbench_test::audio; }
Snapshot ReadSnapshot(bool details) {
	++workbench_test::audioSamples;
	if (details) ++workbench_test::audioDetailSamples;
	Snapshot s; s.backend = workbench_test::audioAvailable ? Backend::OpenAL : Backend::Disabled;
	s.reverbAvailable = workbench_test::audioAvailable;
	s.application = workbench_test::audioAvailable ? Application::Reverb : Application::Disabled;
	s.requested = s.applied = workbench_test::audioAvailable ? 2 : -1;
	s.appliedValues = workbench_test::audio[2];
	s.details = details && s.reverbAvailable; return s;
}
WriteResult WriteEnvironments(const Environments& expected, const Environments& values, bool requireCapability) {
	if (std::memcmp(expected.data(), workbench_test::audio.data(), sizeof(expected))) return WriteResult::Changed;
	if (std::memcmp(expected.data(), values.data(), sizeof(expected)) && requireCapability && !workbench_test::audioAvailable) return WriteResult::Unavailable;
	workbench_test::audio = values; return WriteResult::Ok;
}
const char* ApplicationName(Application a) { return a == Application::Disabled ? "Audio disabled" : "Applied reverb (test seam)"; }
}
