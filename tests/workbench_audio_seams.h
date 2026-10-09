#pragma once
#include "Audio/AudioInspection.h"
namespace workbench_test {
extern c2audio::Environments audio;
extern bool audioAvailable;
extern int audioSamples, audioDetailSamples;
void ResetAudio();
}
