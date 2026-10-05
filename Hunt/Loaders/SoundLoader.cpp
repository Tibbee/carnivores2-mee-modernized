// SoundLoader.cpp -- shared checked RIFF decoding for gameplay WAV assets.
#include "Hunt.h"
#include "../Audio/WaveReader.h"
#include <fstream>

void LoadWav(char* FName, TSFX& sfx)
{
    std::ifstream file(FName, std::ios::binary);
    std::vector<short> samples;
    const char* error = nullptr;
    if (!file || !c2audio::ReadWave(file, samples, error)) {
        // The stream is RAII-owned: the test halt seam can throw without leaking
        // the OS file handle, and a failed replacement leaves the sound intact.
        char message[512];
        sprintf_s(message, sizeof(message), "Sound loading error: %s\n%s.",
            error ? error : "cannot open WAV file", FName);
        DoHalt(message);
        return;
    }
    sfx.length = static_cast<int>(samples.size() * sizeof(short));
    sfx.lpData.swap(samples);
}
