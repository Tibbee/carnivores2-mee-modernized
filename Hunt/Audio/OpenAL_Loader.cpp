#include "OpenAL_Loader.h"

LPALCOPENDEVICE alcOpenDevice = nullptr;
LPALCCLOSEDEVICE alcCloseDevice = nullptr;
LPALCCREATECONTEXT alcCreateContext = nullptr;
LPALCMAKECONTEXTCURRENT alcMakeContextCurrent = nullptr;
LPALCDESTROYCONTEXT alcDestroyContext = nullptr;
LPALDISTANCEMODEL alDistanceModel = nullptr;
LPALGENBUFFERS alGenBuffers = nullptr;
LPALDELETEBUFFERS alDeleteBuffers = nullptr;
LPALBUFFERDATA alBufferData = nullptr;
LPALGENSOURCES alGenSources = nullptr;
LPALDELETESOURCES alDeleteSources = nullptr;
LPALSOURCEI alSourcei = nullptr;
LPALSOURCEF alSourcef = nullptr;
LPALSOURCE3F alSource3f = nullptr;
LPALSOURCEPLAY alSourcePlay = nullptr;
LPALSOURCESTOP alSourceStop = nullptr;
LPALGETSOURCEI alGetSourcei = nullptr;
LPALGETERROR alGetError = nullptr;
LPALLISTENERF alListenerf = nullptr;
LPALLISTENER3F alListener3f = nullptr;
LPALLISTENERFV alListenerfv = nullptr;
LPALGENEFFECTS alGenEffects = nullptr;
LPALDELETEEFFECTS alDeleteEffects = nullptr;
LPALEFFECTI alEffecti = nullptr;
LPALEFFECTF alEffectf = nullptr;
LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots = nullptr;
LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots = nullptr;
LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti = nullptr;
LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf = nullptr;
LPALISEXTENSIONPRESENT alIsExtensionPresent = nullptr;
LPALSOURCE3I alSource3i = nullptr;
LPALGETSTRING alGetString = nullptr;
LPALCGETSTRING alcGetString = nullptr;

static HMODULE hOpenAL = nullptr;

bool LoadOpenAL() {
    hOpenAL = LoadLibraryA("openal32.dll");
    if (!hOpenAL) return false;

    alcOpenDevice = (LPALCOPENDEVICE)GetProcAddress(hOpenAL, "alcOpenDevice");
    alcCloseDevice = (LPALCCLOSEDEVICE)GetProcAddress(hOpenAL, "alcCloseDevice");
    alcCreateContext = (LPALCCREATECONTEXT)GetProcAddress(hOpenAL, "alcCreateContext");
    alcMakeContextCurrent = (LPALCMAKECONTEXTCURRENT)GetProcAddress(hOpenAL, "alcMakeContextCurrent");
    alcDestroyContext = (LPALCDESTROYCONTEXT)GetProcAddress(hOpenAL, "alcDestroyContext");
    alDistanceModel = (LPALDISTANCEMODEL)GetProcAddress(hOpenAL, "alDistanceModel");
    alGenBuffers = (LPALGENBUFFERS)GetProcAddress(hOpenAL, "alGenBuffers");
    alDeleteBuffers = (LPALDELETEBUFFERS)GetProcAddress(hOpenAL, "alDeleteBuffers");
    alBufferData = (LPALBUFFERDATA)GetProcAddress(hOpenAL, "alBufferData");
    alGenSources = (LPALGENSOURCES)GetProcAddress(hOpenAL, "alGenSources");
    alDeleteSources = (LPALDELETESOURCES)GetProcAddress(hOpenAL, "alDeleteSources");
    alSourcei = (LPALSOURCEI)GetProcAddress(hOpenAL, "alSourcei");
    alSourcef = (LPALSOURCEF)GetProcAddress(hOpenAL, "alSourcef");
    alSource3f = (LPALSOURCE3F)GetProcAddress(hOpenAL, "alSource3f");
    alSourcePlay = (LPALSOURCEPLAY)GetProcAddress(hOpenAL, "alSourcePlay");
    alSourceStop = (LPALSOURCESTOP)GetProcAddress(hOpenAL, "alSourceStop");
    alGetSourcei = (LPALGETSOURCEI)GetProcAddress(hOpenAL, "alGetSourcei");
    alGetError = (LPALGETERROR)GetProcAddress(hOpenAL, "alGetError");
    alListenerf = (LPALLISTENERF)GetProcAddress(hOpenAL, "alListenerf");
    alListener3f = (LPALLISTENER3F)GetProcAddress(hOpenAL, "alListener3f");
    alListenerfv = (LPALLISTENERFV)GetProcAddress(hOpenAL, "alListenerfv");
    alGenEffects = (LPALGENEFFECTS)GetProcAddress(hOpenAL, "alGenEffects");
    alDeleteEffects = (LPALDELETEEFFECTS)GetProcAddress(hOpenAL, "alDeleteEffects");
    alEffecti = (LPALEFFECTI)GetProcAddress(hOpenAL, "alEffecti");
    alEffectf = (LPALEFFECTF)GetProcAddress(hOpenAL, "alEffectf");
    alGenAuxiliaryEffectSlots = (LPALGENAUXILIARYEFFECTSLOTS)GetProcAddress(hOpenAL, "alGenAuxiliaryEffectSlots");
    alDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)GetProcAddress(hOpenAL, "alDeleteAuxiliaryEffectSlots");
    alAuxiliaryEffectSloti = (LPALAUXILIARYEFFECTSLOTI)GetProcAddress(hOpenAL, "alAuxiliaryEffectSloti");
    alAuxiliaryEffectSlotf = (LPALAUXILIARYEFFECTSLOTF)GetProcAddress(hOpenAL, "alAuxiliaryEffectSlotf");
    alIsExtensionPresent = (LPALISEXTENSIONPRESENT)GetProcAddress(hOpenAL, "alIsExtensionPresent");
    alSource3i = (LPALSOURCE3I)GetProcAddress(hOpenAL, "alSource3i");
    alGetString = (LPALGETSTRING)GetProcAddress(hOpenAL, "alGetString");
    alcGetString = (LPALCGETSTRING)GetProcAddress(hOpenAL, "alcGetString");

    // Core functions required (EFX optional)
    if (!alcOpenDevice || !alcCloseDevice || !alcCreateContext ||
        !alcMakeContextCurrent || !alcDestroyContext || !alDistanceModel ||
        !alGenBuffers || !alDeleteBuffers || !alBufferData ||
        !alGenSources || !alDeleteSources || !alSourcei || !alSourcef ||
        !alSource3f || !alSourcePlay || !alSourceStop || !alGetSourcei ||
        !alGetError || !alListenerf || !alListener3f || !alListenerfv) {
        FreeLibrary(hOpenAL);
        hOpenAL = nullptr;
        return false;
    }

    return true;
}

void UnloadOpenAL() {
    if (hOpenAL) {
        FreeLibrary(hOpenAL);
        hOpenAL = nullptr;

        alcOpenDevice = nullptr;
        alcCloseDevice = nullptr;
        alcCreateContext = nullptr;
        alcMakeContextCurrent = nullptr;
        alcDestroyContext = nullptr;
        alDistanceModel = nullptr;
        alGenBuffers = nullptr;
        alDeleteBuffers = nullptr;
        alBufferData = nullptr;
        alGenSources = nullptr;
        alDeleteSources = nullptr;
        alSourcei = nullptr;
        alSourcef = nullptr;
        alSource3f = nullptr;
        alSourcePlay = nullptr;
        alSourceStop = nullptr;
        alGetSourcei = nullptr;
        alGetError = nullptr;
        alListenerf = nullptr;
        alListener3f = nullptr;
        alListenerfv = nullptr;
        alGenEffects = nullptr;
        alDeleteEffects = nullptr;
        alEffecti = nullptr;
        alEffectf = nullptr;
        alGenAuxiliaryEffectSlots = nullptr;
        alDeleteAuxiliaryEffectSlots = nullptr;
        alAuxiliaryEffectSloti = nullptr;
        alAuxiliaryEffectSlotf = nullptr;
        alIsExtensionPresent = nullptr;
        alSource3i = nullptr;
        alGetString = nullptr;
        alcGetString = nullptr;
    }
}
