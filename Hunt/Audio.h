#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "OpenAL_Loader.h"

// Matches the original audio DLL channel layout
constexpr int MAX_CHANNEL = 16;
constexpr int MIN_RADIUS  = 512;

struct CHANNEL {
    ALuint source;
    ALuint buffer;
    short int* lpData;
    float x, y, z;
    int volume;
};

struct AMBIENT {
    ALuint source;
    ALuint buffer;
    short int* lpData;
    int iLength;
    int volume, avolume;   // crossfade state
};

struct MAMBIENT {
    ALuint source;
    ALuint buffer;
    short int* lpData;
    int iLength;
    float x, y, z;
};

// Internal OpenAL state — not used by the game engine directly
extern int iSoundActive;
extern CHANNEL channel[MAX_CHANNEL];
extern AMBIENT ambient;
extern AMBIENT ambient2;
extern MAMBIENT mambient;

extern int xCamera, yCamera, zCamera;
extern float alphaCamera, betaCamera;

#endif
