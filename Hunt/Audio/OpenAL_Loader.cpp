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
LPALGETPROCADDRESS alGetProcAddress = nullptr;
LPALCISEXTENSIONPRESENT alcIsExtensionPresent = nullptr;

static HMODULE hOpenAL = nullptr;

bool LoadOpenAL() {
    if (hOpenAL) return true;
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
    alGetProcAddress = (LPALGETPROCADDRESS)GetProcAddress(hOpenAL, "alGetProcAddress");
    alcIsExtensionPresent = (LPALCISEXTENSIONPRESENT)GetProcAddress(hOpenAL, "alcIsExtensionPresent");

    // Core functions required (EFX optional)
    if (!alcOpenDevice || !alcCloseDevice || !alcCreateContext ||
        !alcMakeContextCurrent || !alcDestroyContext || !alDistanceModel ||
        !alGenBuffers || !alDeleteBuffers || !alBufferData ||
        !alGenSources || !alDeleteSources || !alSourcei || !alSourcef ||
        !alSource3f || !alSourcePlay || !alSourceStop || !alGetSourcei ||
        !alGetError || !alListenerf || !alListener3f || !alListenerfv ||
        !alGetProcAddress || !alcIsExtensionPresent) {
        UnloadOpenAL();
        return false;
    }

    return true;
}

// Device-scoped EFX is optional. Never assume the DLL exports extension symbols.
bool LoadOpenALEFX(ALCdevice* device)
{
    if (!device || !alcIsExtensionPresent || !alGetProcAddress ||
        !alcIsExtensionPresent(device, "ALC_EXT_EFX")) return false;
#define LOAD_EFX(name, type) name = reinterpret_cast<type>(alGetProcAddress(#name))
    LOAD_EFX(alGenEffects, LPALGENEFFECTS);
    LOAD_EFX(alDeleteEffects, LPALDELETEEFFECTS);
    LOAD_EFX(alEffecti, LPALEFFECTI);
    LOAD_EFX(alEffectf, LPALEFFECTF);
    LOAD_EFX(alGenAuxiliaryEffectSlots, LPALGENAUXILIARYEFFECTSLOTS);
    LOAD_EFX(alDeleteAuxiliaryEffectSlots, LPALDELETEAUXILIARYEFFECTSLOTS);
    LOAD_EFX(alAuxiliaryEffectSloti, LPALAUXILIARYEFFECTSLOTI);
    LOAD_EFX(alAuxiliaryEffectSlotf, LPALAUXILIARYEFFECTSLOTF);
    LOAD_EFX(alSource3i, LPALSOURCE3I);
#undef LOAD_EFX
    return alGenEffects && alDeleteEffects && alEffecti && alEffectf &&
        alGenAuxiliaryEffectSlots && alDeleteAuxiliaryEffectSlots &&
        alAuxiliaryEffectSloti && alAuxiliaryEffectSlotf && alSource3i;
}

void UnloadOpenAL()
{
    if (hOpenAL) FreeLibrary(hOpenAL);
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
    alGetProcAddress = nullptr;
    alcIsExtensionPresent = nullptr;
}
