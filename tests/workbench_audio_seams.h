#pragma once
#include "Audio/AudioInspection.h"
namespace workbench_test {
extern c2audio::Environments audio;
extern bool audioAvailable;
extern int audioRequested,audioApplied;
extern c2audio::Application audioApplication;
extern int audioSamples, audioDetailSamples;
void ResetAudio();
}
