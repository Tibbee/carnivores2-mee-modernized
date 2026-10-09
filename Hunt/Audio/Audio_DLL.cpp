// Audio_DLL.cpp -- OpenAL Soft backend with optional legacy DirectSound DLL support
// Keeps the same public API signatures as the original DLL-based system
// so the call sites in the engine need zero changes.

#include "hunt.h"
#include "Audio.h"
#include "AudioMath.h"
#include "AudioInspection.h"
#include <mutex>
#include <cstring>
#include <unordered_map>
#include <cmath>
#include <atomic>
#include <utility>

// ---------------------------------------------------------------------------
// OpenAL state
// ---------------------------------------------------------------------------
static ALCdevice*  alDevice = nullptr;
static ALCcontext* alContext = nullptr;
static std::unordered_map<short*, ALuint> bufferCache;
static HANDLE      hAudioThread = nullptr;
static DWORD       AudioTId;
static CRITICAL_SECTION AudioCS;
static std::atomic<bool> g_AudioShutdown{false};

static std::atomic<int> iSoundActive{0};
static CHANNEL channel[MAX_CHANNEL]{};
static AMBIENT ambient{};
static AMBIENT ambient2{};
static MAMBIENT mambient{};
static std::array<float, 3> g_ListenerPosition{};
static bool g_ListenerKnown = false;

enum class AudioBackend {
    OpenALSoft,
    LegacyDLL
};

static AudioBackend g_AudioBackend = AudioBackend::OpenALSoft;
static HMODULE g_LegacyAudioDLL = nullptr;

using LegacyAudioInitFn = void (WINAPI *)(HWND, HANDLE);
using LegacyAudioVoidFn = void (WINAPI *)(void);
using LegacyAudioCameraFn = void (WINAPI *)(float, float, float, float, float);
using LegacyAudioSoundFn = void (WINAPI *)(int, short int*, int);
using LegacyAudioSound3DFn = void (WINAPI *)(int, short int*, float, float, float);
using LegacyAudioVoiceFn = void (WINAPI *)(int, short int*, float, float, float, int);
using LegacyAudioVersionFn = int (WINAPI *)(void);
using LegacyAudioEnvFn = void (WINAPI *)(int, float);
using LegacyAudioGeomFn = void (WINAPI *)(int, void*);

static LegacyAudioInitFn g_LegacyInitAudioSystem = nullptr;
static LegacyAudioVoidFn g_LegacyAudioRestore = nullptr;
static LegacyAudioVoidFn g_LegacyAudioStop = nullptr;
static LegacyAudioVoidFn g_LegacyAudioShutdown = nullptr;
static LegacyAudioCameraFn g_LegacyAudioSetCameraPos = nullptr;
static LegacyAudioSoundFn g_LegacySetAmbient = nullptr;
static LegacyAudioSound3DFn g_LegacySetAmbient3d = nullptr;
static LegacyAudioVoiceFn g_LegacyAddVoice3dv = nullptr;
static LegacyAudioVersionFn g_LegacyAudioGetVersion = nullptr;
static LegacyAudioEnvFn g_LegacyAudioSetEnvironment = nullptr;
static LegacyAudioGeomFn g_LegacyAudioUploadGeometry = nullptr;

// For EAX → EFX reverb
static ALuint g_effect = 0;
static ALuint g_slot   = 0;
static int    g_CurrentEnv = -1;
// Presets also exist before AudioCS/device initialization (LoadConfig).
static std::recursive_mutex g_EnvironmentMutex;
static int g_RequestedEnv = -1, g_AppliedEnv = -1;
static c2audio::Application g_Application = c2audio::Application::Pending;
static std::array<float, 4> g_AppliedValues{};

// ---------------------------------------------------------------------------
// EAX 2.0 environment presets  (values match original EAX2Audio/Audio3d.cpp)
// ---------------------------------------------------------------------------
struct EAX2ENV {
    int   envID;      // EAX preset enum (unused in EFX path)
    int   room;       // mB  (-10000 … 0)
    float decay;      // seconds
    float decayHF;    // ratio
    float diffusion;  // 0.0 … 1.0
    int   reverb;     // mB  (-10000 … 0)
};

static const EAX2ENV g_EnvPresets[9] = {
    { 0, -400, 1.49f,  0.3f,   0.4f,   -620 },  // 0  Generic
    { 1, -400, 1.2f,  0.2f,   0.2f,   -600 },  // 1  Plate
    { 2, -400, 1.49f,  0.18f,  0.3f,   -700 },  // 2  Forest
    { 3, -200, 2.6f,  0.2f,   0.326f, -500 },  // 3  Mountain
    { 4, -400, 1.1f,  0.6f,   0.275f, -700 },  // 4  Canyon
    { 5, -300, 3.2f,  0.6f,   0.9f,   -200 },  // 5  Cave
    { 6, -400, 1.8f,  0.4f,   0.4f,   -400 },  // 6  Special 2
    { 7,    0, 0.0f,  0.0f,   0.0f,      0 },  // 7  Special 3 (dry)
    { 8, -400, 1.5f,  0.1f,   0.1f,   -200 },  // 8  Underwater
};

// Convert EAX mB to linear gain (OpenAL EFX uses 0.0-1.0)
static float mBToGain(int mB) {
    return std::pow(10.0f, mB / 2000.0f);
}

// Runtime-tunable copy of the preset table (config.cfg envN_* keys).
// Untouched entries read exactly the compiled defaults above.
static EAX2ENV g_EnvRuntime[9];
static bool g_EnvRuntimeInit = false;
static void EnsureEnvRuntime() {
    if (!g_EnvRuntimeInit) {
        memcpy(g_EnvRuntime, g_EnvPresets, sizeof(g_EnvPresets));
        g_EnvRuntimeInit = true;
    }
}
// field: 0=decay (s, 0.1-20), 1=decayHF (ratio, 0.1-2), 2=diffusion (0-1),
// 3=reverb (mB, -10000-0). room/envID are intentionally not settable -- the
// EFX path does not consume them, so a knob would be wired to nothing.
bool Audio_SetEnvParam(int env, int field, float v)
{
    // NaN bypasses ordered range comparisons; reject it (and infinities)
    // before storing floats or converting the reverb level to an integer.
    std::uint32_t bits; std::memcpy(&bits, &v, sizeof(bits));
    if (env < 0 || env > 8 || (bits & 0x7f800000u) == 0x7f800000u) return false;
    std::lock_guard<std::recursive_mutex> presetLock(g_EnvironmentMutex);
    EnsureEnvRuntime();
    switch (field) {
    case 0: if (v < 0.1f || v > 20.0f) return false; g_EnvRuntime[env].decay = v; break;
    case 1: if (v < 0.1f || v > 2.0f) return false; g_EnvRuntime[env].decayHF = v; break;
    case 2: if (v < 0.0f || v > 1.0f) return false; g_EnvRuntime[env].diffusion = v; break;
    case 3: if (v < -10000.0f || v > 0.0f) return false; g_EnvRuntime[env].reverb = static_cast<int>(v); break;
    default: return false;
    }
    if (env == g_CurrentEnv) {
        g_CurrentEnv = g_AppliedEnv = -1; // force re-push if live
        g_Application = c2audio::Application::Pending;
    }
    return true;
}

static void UnloadLegacyAudioBackend()
{
    if (g_LegacyAudioDLL) {
        FreeLibrary(g_LegacyAudioDLL);
        g_LegacyAudioDLL = nullptr;
    }

    g_LegacyInitAudioSystem = nullptr;
    g_LegacyAudioRestore = nullptr;
    g_LegacyAudioStop = nullptr;
    g_LegacyAudioShutdown = nullptr;
    g_LegacyAudioSetCameraPos = nullptr;
    g_LegacySetAmbient = nullptr;
    g_LegacySetAmbient3d = nullptr;
    g_LegacyAddVoice3dv = nullptr;
    g_LegacyAudioGetVersion = nullptr;
    g_LegacyAudioSetEnvironment = nullptr;
    g_LegacyAudioUploadGeometry = nullptr;
    g_AudioBackend = AudioBackend::OpenALSoft;
}

static bool LoadLegacyAudioBackend(const char* dllName)
{
    UnloadLegacyAudioBackend();

    g_LegacyAudioDLL = LoadLibraryA(dllName);
    if (!g_LegacyAudioDLL)
        return false;

#define LOAD_LEGACY_PROC(member, type, name) \
    g_##member = reinterpret_cast<type>(GetProcAddress(g_LegacyAudioDLL, name)); \
    if (!g_##member) { UnloadLegacyAudioBackend(); return false; }

    LOAD_LEGACY_PROC(LegacyInitAudioSystem, LegacyAudioInitFn, "InitAudioSystem");
    LOAD_LEGACY_PROC(LegacyAudioRestore, LegacyAudioVoidFn, "Audio_Restore");
    LOAD_LEGACY_PROC(LegacyAudioStop, LegacyAudioVoidFn, "AudioStop");
    LOAD_LEGACY_PROC(LegacyAudioShutdown, LegacyAudioVoidFn, "Audio_Shutdown");
    LOAD_LEGACY_PROC(LegacyAudioSetCameraPos, LegacyAudioCameraFn, "AudioSetCameraPos");
    LOAD_LEGACY_PROC(LegacySetAmbient, LegacyAudioSoundFn, "SetAmbient");
    LOAD_LEGACY_PROC(LegacySetAmbient3d, LegacyAudioSound3DFn, "SetAmbient3d");
    LOAD_LEGACY_PROC(LegacyAddVoice3dv, LegacyAudioVoiceFn, "AddVoice3dv");
    LOAD_LEGACY_PROC(LegacyAudioSetEnvironment, LegacyAudioEnvFn, "Audio_SetEnvironment");

    g_LegacyAudioGetVersion = reinterpret_cast<LegacyAudioVersionFn>(GetProcAddress(g_LegacyAudioDLL, "Audio_GetVersion"));
    g_LegacyAudioUploadGeometry = reinterpret_cast<LegacyAudioGeomFn>(GetProcAddress(g_LegacyAudioDLL, "Audio_UploadGeometry"));

#undef LOAD_LEGACY_PROC

    if (g_LegacyAudioGetVersion) {
        int v = g_LegacyAudioGetVersion();
        char buf[128];
        sprintf_s(buf, sizeof(buf), "Legacy audio driver version: %d.%d\n", v >> 16, v & 0xFFFF);
        PrintLog(buf);
    }

    g_AudioBackend = AudioBackend::LegacyDLL;
    return true;
}

// Checked operations return their status; never consume the same error twice.
static bool CheckALError(const char* operation)
{
    const ALenum error = alGetError();
    if (error == AL_NO_ERROR) return true;
    char message[256];
    sprintf_s(message, sizeof(message), "ALerr 0x%04X at %s\n", error, operation);
    PrintLog(message);
    return false;
}
#define AL_CHECK(call) ([&]() { call; return CheckALError(#call); }())

class AudioLock {
public:
    AudioLock() { EnterCriticalSection(&AudioCS); }
    ~AudioLock() { LeaveCriticalSection(&AudioCS); }
    AudioLock(const AudioLock&) = delete;
    AudioLock& operator=(const AudioLock&) = delete;
};

static bool StopAndDetach(ALuint source)
{
    if (!source) return true;
    return AL_CHECK(alSourceStop(source)) && AL_CHECK(alSourcei(source, AL_BUFFER, 0));
}

static bool ClearAmbient(AMBIENT& sound)
{
    if (!StopAndDetach(sound.source)) return false;
    const ALuint source = sound.source;
    sound = {};
    sound.source = source;
    return true;
}

static float AmbientGain(const AMBIENT& sound)
{
    return AudioGain(sound.avolume) * (sound.volume / 256.0f);
}

static void SetAmbientVolume(AMBIENT& sound, int volume)
{
    if (sound.avolume == volume) return;
    AMBIENT next = sound;
    next.avolume = volume;
    if (AL_CHECK(alSourcef(next.source, AL_GAIN, AmbientGain(next))))
        sound.avolume = volume;
}

// Cache only successful, nonempty mono16 uploads. A failed request can retry.
static ALuint GetBuffer(short int* lpData, int length)
{
    if (!lpData || length <= 0 || length > (16 << 20) || length % sizeof(short)) return 0;
    const auto found = bufferCache.find(lpData);
    if (found != bufferCache.end()) return found->second;
    ALuint buffer = 0;
    if (!AL_CHECK(alGenBuffers(1, &buffer)) || !buffer) {
        if (buffer) AL_CHECK(alDeleteBuffers(1, &buffer));
        return 0;
    }
    if (!AL_CHECK(alBufferData(buffer, AL_FORMAT_MONO16, lpData, length, 22050))) {
        AL_CHECK(alDeleteBuffers(1, &buffer));
        return 0;
    }
    bufferCache.emplace(lpData, buffer);
    return buffer;
}

// One deterministic 70ms tick, also driven directly by regression tests.
static void UpdateAmbientFades()
{
    if (ambient.lpData && ambient.volume < 256) {
        AMBIENT next = ambient;
        next.volume = (std::min)(256, next.volume + 16);
        if (AL_CHECK(alSourcef(next.source, AL_GAIN, AmbientGain(next))))
            ambient.volume = next.volume;
    }
    if (ambient2.lpData) {
        AMBIENT next = ambient2;
        next.volume = (std::max)(0, next.volume - 16);
        if (next.volume == 0) ClearAmbient(ambient2);
        else if (AL_CHECK(alSourcef(next.source, AL_GAIN, AmbientGain(next))))
            ambient2.volume = next.volume;
    }
}

DWORD WINAPI ProcessAudioThread(LPVOID)
{
    while (!g_AudioShutdown.load()) {
        if (iSoundActive.load()) {
            AudioLock lock;
            if (!g_AudioShutdown.load()) UpdateAmbientFades();
        }
        Sleep(70);
    }
    return 0;
}

static bool CreateAudioSources()
{
    g_ListenerKnown = false;
    if (!AL_CHECK(alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED))) return false;
    for (auto& voice : channel) {
        if (!AL_CHECK(alGenSources(1, &voice.source)) || !voice.source ||
            !AL_CHECK(alSourcef(voice.source, AL_ROLLOFF_FACTOR, 0.418f)) ||
            !AL_CHECK(alSourcef(voice.source, AL_REFERENCE_DISTANCE, static_cast<float>(MIN_RADIUS))) ||
            !AL_CHECK(alSourcef(voice.source, AL_MAX_DISTANCE, 10000.0f))) return false;
    }
    for (AMBIENT* sound : {&ambient, &ambient2}) {
        if (!AL_CHECK(alGenSources(1, &sound->source)) || !sound->source ||
            !AL_CHECK(alSourcei(sound->source, AL_LOOPING, AL_TRUE)) ||
            !AL_CHECK(alSourcei(sound->source, AL_SOURCE_RELATIVE, AL_TRUE)) ||
            !AL_CHECK(alSource3f(sound->source, AL_POSITION, 0, 0, 0))) return false;
    }
    return AL_CHECK(alGenSources(1, &mambient.source)) && mambient.source &&
        AL_CHECK(alSourcei(mambient.source, AL_LOOPING, AL_TRUE)) &&
        AL_CHECK(alSourcef(mambient.source, AL_ROLLOFF_FACTOR, 0.418f)) &&
        AL_CHECK(alSourcef(mambient.source, AL_REFERENCE_DISTANCE, static_cast<float>(MIN_RADIUS))) &&
        AL_CHECK(alSourcef(mambient.source, AL_MAX_DISTANCE, 10000.0f));
}

static void InitReverb()
{
    // EFX is ALC_EXT_EFX; AL_SOFT_effect_target is a separate extension.
    if (LoadOpenALEFX(alDevice) &&
        AL_CHECK(alGenEffects(1, &g_effect)) && g_effect &&
        AL_CHECK(alEffecti(g_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB)) &&
        AL_CHECK(alGenAuxiliaryEffectSlots(1, &g_slot)) && g_slot &&
        AL_CHECK(alAuxiliaryEffectSlotf(g_slot, AL_EFFECTSLOT_GAIN, 1.0f))) {
        PrintLog("OpenAL: EFX reverb available\n");
        return;
    }
    if (g_slot) AL_CHECK(alDeleteAuxiliaryEffectSlots(1, &g_slot));
    if (g_effect) AL_CHECK(alDeleteEffects(1, &g_effect));
    g_slot = g_effect = 0;
    PrintLog("OpenAL: EFX unavailable; using dry audio\n");
}

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------
void InitAudioSystem(HWND hw, HANDLE hlog, int driver)
{
    if (iSoundActive) Audio_Shutdown();
    if (iSoundActive) return; // A failed thread join must not be reinitialized.
    { std::lock_guard<std::recursive_mutex> lock(g_EnvironmentMutex);
      g_RequestedEnv = g_AppliedEnv = -1; g_Application = c2audio::Application::Pending; }
    driver = NormalizeAudioBackend(driver);
    if (driver == AUDIO_DIRECTSOUND) {
        if (LoadLegacyAudioBackend("a_ds3d.dll")) {
            PrintLog("DirectSound: legacy audio DLL loaded\n");
            g_LegacyInitAudioSystem(hw, hlog);
            g_AudioShutdown = false;
            iSoundActive = 1;
            return;
        }
        PrintLog("DirectSound: legacy audio DLL not found — falling back to OpenAL Soft\n");
    }
    g_AudioBackend = AudioBackend::OpenALSoft;
    if (!LoadOpenAL()) {
        PrintLog("OpenAL: loader unavailable — audio disabled\n");
        return;
    }
    alDevice = alcOpenDevice(nullptr);
    if (!alDevice) {
        PrintLog("OpenAL: no default audio device — audio disabled\n");
        UnloadOpenAL();
        return;
    }
    alContext = alcCreateContext(alDevice, nullptr);
    if (!alContext || !alcMakeContextCurrent(alContext)) {
        PrintLog("OpenAL: failed to create context — audio disabled\n");
        if (alContext) alcDestroyContext(alContext);
        alcCloseDevice(alDevice);
        alDevice = nullptr;
        alContext = nullptr;
        UnloadOpenAL();
        return;
    }
    InitializeCriticalSection(&AudioCS);
    g_CurrentEnv = -1;
    iSoundActive = 1; // Common cleanup also handles partial source/thread setup.
    if (!CreateAudioSources()) {
        PrintLog("OpenAL: source setup failed — audio disabled\n");
        Audio_Shutdown();
        return;
    }
    InitReverb();
    g_AudioShutdown = false;
    hAudioThread = CreateThread(nullptr, 0, ProcessAudioThread, nullptr, 0, &AudioTId);
    if (!hAudioThread) {
        PrintLog("OpenAL: ambient worker creation failed — audio disabled\n");
        Audio_Shutdown();
        return;
    }
    SetThreadPriority(hAudioThread, THREAD_PRIORITY_HIGHEST);
    char message[256];
    const char* name = alcGetString ? alcGetString(alDevice, ALC_DEVICE_SPECIFIER) : nullptr;
    sprintf_s(message, sizeof(message), "OpenAL: device=\"%s\"\n", name ? name : "(unknown)");
    PrintLog(message);
    PrintLog("OpenAL: Audio System Initialized\n");
}

void Audio_Shutdown()
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacyAudioShutdown)
            g_LegacyAudioShutdown();
        UnloadLegacyAudioBackend();
        iSoundActive = 0;
        g_AudioShutdown = true;
        return;
    }

    if (!iSoundActive) return;

    g_AudioShutdown = true;

    if (hAudioThread) {
        DWORD wait = WaitForSingleObject(hAudioThread, 5000);
        if (wait == WAIT_TIMEOUT) {
            PrintLog("OpenAL: waiting for ambient worker before teardown\n");
            wait = WaitForSingleObject(hAudioThread, INFINITE);
        }
        if (wait != WAIT_OBJECT_0) {
            PrintLog("OpenAL: worker join failed; refusing unsafe teardown\n");
            return;
        }
        CloseHandle(hAudioThread);
        hAudioThread = nullptr;
    }

    EnterCriticalSection(&AudioCS);
    AudioStop();
    iSoundActive = 0;
    g_CurrentEnv = -1;

    for (auto& voice : channel) {
        if (voice.source) AL_CHECK(alDeleteSources(1, &voice.source));
        voice = {};
    }
    if (ambient.source) AL_CHECK(alDeleteSources(1, &ambient.source));
    if (ambient2.source) AL_CHECK(alDeleteSources(1, &ambient2.source));
    if (mambient.source) AL_CHECK(alDeleteSources(1, &mambient.source));
    ambient = {}; ambient2 = {}; mambient = {};
    if (g_slot) AL_CHECK(alDeleteAuxiliaryEffectSlots(1, &g_slot));
    if (g_effect) AL_CHECK(alDeleteEffects(1, &g_effect));
    g_slot = g_effect = 0;

    for (auto& kv : bufferCache)
        AL_CHECK(alDeleteBuffers(1, &kv.second));
    bufferCache.clear();

    LeaveCriticalSection(&AudioCS);

    if (alContext) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(alContext);
        alContext = nullptr;
    }
    if (alDevice) {
        alcCloseDevice(alDevice);
        alDevice = nullptr;
    }

    DeleteCriticalSection(&AudioCS);
    UnloadOpenAL();

    PrintLog("OpenAL Audio System Shut Down\n");
}

void AudioStop()
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacyAudioStop)
            g_LegacyAudioStop();
        return;
    }

    if (!alContext) return;

    EnterCriticalSection(&AudioCS);

    for (auto& voice : channel) {
        if (!StopAndDetach(voice.source)) continue;
        const ALuint source = voice.source;
        voice = {};
        voice.source = source;
    }
    ClearAmbient(ambient);
    ClearAmbient(ambient2);
    if (StopAndDetach(mambient.source)) {
        const ALuint movingSource = mambient.source;
        mambient = {};
        mambient.source = movingSource;
    }

    LeaveCriticalSection(&AudioCS);
}

// ---------------------------------------------------------------------------
// Restore (legacy only; OpenAL endpoint recovery is not implemented here)
// ---------------------------------------------------------------------------
void Audio_Restore()
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacyAudioRestore)
            g_LegacyAudioRestore();
        return;
    }

    // Nothing to do.
}

// ---------------------------------------------------------------------------
// Camera / listener
// ---------------------------------------------------------------------------
void AudioSetCameraPos(float cx, float cy, float cz, float ca, float cb)
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacyAudioSetCameraPos)
            g_LegacyAudioSetCameraPos(cx, cy, cz, ca, cb);
        return;
    }

    if (!iSoundActive) return;

    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz) ||
        !std::isfinite(ca) || !std::isfinite(cb)) return;
    AudioLock lock;
    ALfloat pos[] = { cx, cy, cz };
    ALfloat orient[6];
    AudioListenerOrientation(ca, cb, orient);
    g_ListenerKnown = AL_CHECK(alListenerfv(AL_POSITION, pos));
    if (g_ListenerKnown) g_ListenerPosition = {cx, cy, cz};
    AL_CHECK(alListenerfv(AL_ORIENTATION, orient));
}

// ---------------------------------------------------------------------------
// Ambient (non-positional, e.g. jungle loop)
// ---------------------------------------------------------------------------
void SetAmbient(int length, short int* lpdata, int av)
{
    av = (std::max)(0, av);
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacySetAmbient) g_LegacySetAmbient(length, lpdata, av);
        return;
    }
    if (!iSoundActive) return;
    AudioLock lock;
    if (ambient.lpData == lpdata && ambient.iLength == length) {
        SetAmbientVolume(ambient, av);
        return;
    }
    const ALuint buffer = lpdata ? GetBuffer(lpdata, length) : 0;
    if (lpdata && !buffer) return;
    if (lpdata && ambient2.lpData == lpdata && ambient2.iLength == length) {
        // Reverse A -> B without restarting either loop.
        std::swap(ambient, ambient2);
        SetAmbientVolume(ambient, av);
        return;
    }
    // Two-source bound: keep the louder bed on a third-zone/silence request.
    // Swap the LIVE retained source into the outgoing role; never copy/replay it.
    if (AmbientGain(ambient2) > AmbientGain(ambient)) {
        if (!ClearAmbient(ambient)) return;
    } else {
        if (!ClearAmbient(ambient2)) return;
        std::swap(ambient, ambient2);
    }
    if (!buffer) return; // Silence fades out the previous bed.
    if (!AL_CHECK(alSourcei(ambient.source, AL_BUFFER, buffer)) ||
        !AL_CHECK(alSourcef(ambient.source, AL_GAIN, 0.0f)) ||
        !AL_CHECK(alSourcePlay(ambient.source))) {
        ClearAmbient(ambient);
        std::swap(ambient, ambient2);
        return;
    }
    ambient.lpData = lpdata; ambient.iLength = length;
    ambient.buffer = buffer; ambient.avolume = av; ambient.volume = 0;
}

// ---------------------------------------------------------------------------
// Ambient with 3D position (e.g. ship engine)
// ---------------------------------------------------------------------------
void SetAmbient3d(int length, short int* lpdata, float cx, float cy, float cz)
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacySetAmbient3d) g_LegacySetAmbient3d(length, lpdata, cx, cy, cz);
        return;
    }
    if (!iSoundActive) return;
    AudioLock lock;
    if (!lpdata) {
        if (!StopAndDetach(mambient.source)) return;
        const ALuint source = mambient.source;
        mambient = {};
        mambient.source = source;
        return;
    }
    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz)) return;
    if (mambient.lpData != lpdata || mambient.iLength != length) {
        const ALuint buffer = GetBuffer(lpdata, length);
        if (!buffer) return;
        // AL_BUFFER cannot be changed on a playing/paused source.
        if (!StopAndDetach(mambient.source)) return;
        mambient.lpData = nullptr; mambient.buffer = 0; mambient.iLength = 0;
        if (!AL_CHECK(alSourcei(mambient.source, AL_BUFFER, buffer)) ||
            !AL_CHECK(alSource3f(mambient.source, AL_POSITION, cx, cy, cz)) ||
            !AL_CHECK(alSourcePlay(mambient.source))) {
            StopAndDetach(mambient.source);
            return;
        }
        mambient.lpData = lpdata; // Commit ownership only after playback succeeds.
        mambient.buffer = buffer; mambient.iLength = length;
    } else if (!AL_CHECK(alSource3f(mambient.source, AL_POSITION, cx, cy, cz))) return;
    mambient.x = cx; mambient.y = cy; mambient.z = cz;
}

// ---------------------------------------------------------------------------
// Moving-ambient channel queries. The trophy ship and the resupply ship
// SHARE the single looping mambient channel, so callers must coordinate:
// take it only when free or already yours, and stop it on dismissal only
// when you still own it. Otherwise the idle ship's per-frame stop makes
// the active ship replay from frame 0 at 60 Hz (the "engine buzz").
// ---------------------------------------------------------------------------
bool IsAmbient3dOwner(short int* lpdata)
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) return true; // unknowable: keep legacy behaviour
    if (!iSoundActive) return false;
    EnterCriticalSection(&AudioCS);
    bool owned = (lpdata != nullptr && mambient.lpData == lpdata);
    LeaveCriticalSection(&AudioCS);
    return owned;
}

bool IsAmbient3dFree()
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) return true; // unknowable: keep legacy behaviour
    if (!iSoundActive) return false;
    EnterCriticalSection(&AudioCS);
    bool idle = (mambient.lpData == nullptr);
    LeaveCriticalSection(&AudioCS);
    return idle;
}

// ---------------------------------------------------------------------------
// 3D voice (one-shot sounds)
// ---------------------------------------------------------------------------
void AddVoice3dv(int length, short int* lpdata, float cx, float cy, float cz, int vol)
{
    if (!lpdata || vol <= 0 || !std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz)) return;
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacyAddVoice3dv) g_LegacyAddVoice3dv(length, lpdata, cx, cy, cz, vol);
        return;
    }
    if (!iSoundActive) return;
    AudioLock lock;
    int idx = -1;
    for (int i = 0; i < MAX_CHANNEL; ++i) {
        ALint state = AL_PLAYING;
        if (AL_CHECK(alGetSourcei(channel[i].source, AL_SOURCE_STATE, &state)) &&
            state != AL_PLAYING) { idx = i; break; }
    }
    if (idx < 0) return; // Voice stealing remains a separate mix policy.
    const ALuint buffer = GetBuffer(lpdata, length);
    if (!buffer) return;
    CHANNEL& voice = channel[idx];
    if (!StopAndDetach(voice.source)) return;
    voice.lpData = nullptr; voice.buffer = 0;
    const bool relative = cx == 0.0f && cy == 0.0f && cz == 0.0f;
    if (!AL_CHECK(alSourcei(voice.source, AL_BUFFER, buffer)) ||
        !AL_CHECK(alSourcef(voice.source, AL_GAIN, AudioGain(vol))) ||
        !AL_CHECK(alSourcei(voice.source, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE)) ||
        !AL_CHECK(alSource3f(voice.source, AL_POSITION, cx, cy, cz)) ||
        !AL_CHECK(alSourcePlay(voice.source))) {
        StopAndDetach(voice.source);
        return;
    }
    voice.lpData = lpdata; voice.buffer = buffer;
    voice.x = cx; voice.y = cy; voice.z = cz; voice.volume = vol;
}

void AddVoice3d(int length, short int* lpdata, float cx, float cy, float cz)
{
    AddVoice3dv(length, lpdata, cx, cy, cz, 256);
}

void AddVoicev(int length, short int* lpdata, int v)
{
    AddVoice3dv(length, lpdata, 0, 0, 0, v);
}

void AddVoice(int length, short int* lpdata)
{
    AddVoice3dv(length, lpdata, 0, 0, 0, 256);
}

// ---------------------------------------------------------------------------
// Environment reverb  (EAX → OpenAL EFX)
// ---------------------------------------------------------------------------
static bool SetReverbSends(ALuint slot)
{
    bool ok = true;
    for (const auto& voice : channel)
        ok = AL_CHECK(alSource3i(voice.source, AL_AUXILIARY_SEND_FILTER, slot, 0, AL_FILTER_NULL)) && ok;
    for (ALuint source : {ambient.source, ambient2.source, mambient.source})
        ok = AL_CHECK(alSource3i(source, AL_AUXILIARY_SEND_FILTER, slot, 0, AL_FILTER_NULL)) && ok;
    return ok;
}

void Audio_SetEnvironment(int e, float f)
{
    if (e < 0 || e > 8) return;
    std::lock_guard<std::recursive_mutex> presetLock(g_EnvironmentMutex);
    g_RequestedEnv = e;
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        if (g_LegacyAudioSetEnvironment) g_LegacyAudioSetEnvironment(e, f);
        return;
    }
    if (!iSoundActive || !alContext || e < 0 || e > 8) return;
    AudioLock lock;
    if (e == g_CurrentEnv) return;
    EnsureEnvRuntime();
    if (!g_effect || !g_slot) {
        g_CurrentEnv = e; g_AppliedEnv = -1;
        g_Application = c2audio::Application::DryFallback; return;
    }
    g_CurrentEnv = g_AppliedEnv = -1; // Failed/partial applications must be retryable.
    g_Application = c2audio::Application::Failed;
    if (e == 7) {
        // Special 3 has no authored reverb: match its cold-start dry behavior.
        const bool detached = SetReverbSends(0);
        if (AL_CHECK(alAuxiliaryEffectSloti(g_slot, AL_EFFECTSLOT_EFFECT, AL_NONE)) && detached) {
            g_CurrentEnv = g_AppliedEnv = e;
            g_Application = c2audio::Application::Dry; g_AppliedValues = {};
        }
        return;
    }
    const EAX2ENV& env = g_EnvRuntime[e];
    // Preserve existing tuning/mix; do not introduce a room-gain balance change.
    if (!AL_CHECK(alEffecti(g_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB)) ||
        !AL_CHECK(alEffectf(g_effect, AL_EAXREVERB_GAIN, 1.0f)) ||
        !AL_CHECK(alEffectf(g_effect, AL_EAXREVERB_DECAY_TIME, env.decay)) ||
        !AL_CHECK(alEffectf(g_effect, AL_EAXREVERB_DECAY_HFRATIO, env.decayHF)) ||
        !AL_CHECK(alEffectf(g_effect, AL_EAXREVERB_DIFFUSION, env.diffusion)) ||
        !AL_CHECK(alEffectf(g_effect, AL_EAXREVERB_LATE_REVERB_GAIN, mBToGain(env.reverb))) ||
        !AL_CHECK(alAuxiliaryEffectSloti(g_slot, AL_EFFECTSLOT_EFFECT, g_effect)) ||
        !SetReverbSends(g_slot)) return;
    g_CurrentEnv = g_AppliedEnv = e;
    g_Application = c2audio::Application::Reverb;
    g_AppliedValues = {env.decay, env.decayHF, env.diffusion, static_cast<float>(env.reverb)};
    char message[160];
    sprintf_s(message, sizeof(message),
        "Audio_SetEnvironment: env=%d decay=%.3f decayHF=%.3f diffusion=%.3f reverb=%d\n",
        e, env.decay, env.decayHF, env.diffusion, env.reverb);
    PrintLog(message);
}

// ---------------------------------------------------------------------------
// Terrain geometry upload  (no-op for OpenAL)
// Only legacy drivers consume terrain geometry; do not build discarded quads.
// ---------------------------------------------------------------------------
void Audio_UploadGeometry()
{
    if (g_AudioBackend == AudioBackend::LegacyDLL) {
        UploadGeometry();
        if (g_LegacyAudioUploadGeometry)
            g_LegacyAudioUploadGeometry(AudioFCount, data);
        return;
    }

    // OpenAL Soft has no terrain-occlusion equivalent -- discard.
}

namespace c2audio {
Environments ReadEnvironments() {
	std::lock_guard<std::recursive_mutex> lock(g_EnvironmentMutex);
	EnsureEnvRuntime();
	Environments values{};
	for (int e = 0; e < 9; ++e) {
		const auto& p = g_EnvRuntime[e];
		values[e] = {p.decay, p.decayHF, p.diffusion, static_cast<float>(p.reverb)};
	}
	return values;
}
Snapshot ReadSnapshot(bool details) {
	std::lock_guard<std::recursive_mutex> presetLock(g_EnvironmentMutex);
	Snapshot result; result.requested = g_RequestedEnv;
	if (!iSoundActive || g_AudioShutdown) return result;
	if (g_AudioBackend == AudioBackend::LegacyDLL) {
		result.backend = Backend::Legacy; result.application = Application::LegacyUnverified;
		return result;
	}
	if (!alContext) return result;
	AudioLock lock;
	result.backend = Backend::OpenAL;
	result.reverbAvailable = g_effect && g_slot;
	result.application = result.reverbAvailable ? g_Application : Application::DryFallback;
	result.applied = result.reverbAvailable ? g_AppliedEnv : -1;
	result.appliedValues = g_AppliedValues;
	if (!details) return result;
	result.details = true;
	const auto sample = [](ALuint source, VoiceSnapshot& value) {
		if (!source) return;
		ALint state = AL_STOPPED;
		value.known = AL_CHECK(alGetSourcei(source, AL_SOURCE_STATE, &state));
		value.playing = value.known && state == AL_PLAYING;
	};
	for (int i = 0; i < MAX_CHANNEL; ++i) {
		auto& v = result.voices[i]; const auto& source = channel[i];
		v.x = source.x; v.y = source.y; v.z = source.z;
		v.relative = source.x == 0 && source.y == 0 && source.z == 0;
		v.configuredGain = AudioGain(source.volume);
		sample(source.source, v);
	}
	for (int i = 0; i < 2; ++i) {
		const auto& source = i ? ambient2 : ambient;
		auto& v = result.voices[16 + i]; v.relative = true;
		v.configuredGain = AmbientGain(source); v.fade = source.volume;
		sample(source.source, v);
	}
	auto& moving = result.voices[18];
	moving.x = mambient.x; moving.y = mambient.y; moving.z = mambient.z;
	moving.configuredGain = 1; sample(mambient.source, moving);
	if (g_ListenerKnown) for (auto& v : result.voices) if (!v.relative) {
		const float x = v.x - g_ListenerPosition[0], y = v.y - g_ListenerPosition[1], z = v.z - g_ListenerPosition[2];
		v.distance = std::sqrt(x*x + y*y + z*z);
	}
	return result;
}
WriteResult WriteEnvironments(const Environments& expected, const Environments& values, bool requireCapability) {
	std::lock_guard<std::recursive_mutex> lock(g_EnvironmentMutex);
	const auto current = ReadEnvironments();
	if (std::memcmp(expected.data(), current.data(), sizeof(current))) return WriteResult::Changed;
	if (std::memcmp(values[7].data(), current[7].data(), sizeof(values[7]))) return WriteResult::Invalid;
	for (int e = 0; e < 9; ++e) if (e != 7) {
		const float minimum[] = {.1f,.1f,0,-10000}, maximum[] = {20,2,1,0};
		for (int f = 0; f < 4; ++f) {
			const float v = values[e][f]; std::uint32_t bits; std::memcpy(&bits, &v, sizeof(bits));
			if ((bits & 0x7f800000u) == 0x7f800000u || v < minimum[f] || v > maximum[f]) return WriteResult::Invalid;
			if (f == 3 && v != static_cast<float>(static_cast<int>(v))) return WriteResult::Invalid;
		}
	}
	if (!std::memcmp(values.data(), current.data(), sizeof(current))) return WriteResult::Ok;
	if (requireCapability && (!iSoundActive || g_AudioShutdown || !alContext ||
		g_AudioBackend != AudioBackend::OpenALSoft || !g_effect || !g_slot)) return WriteResult::Unavailable;
	bool refresh = false;
	for (int e = 0; e < 9; ++e) if (e != 7) for (int f = 0; f < 4; ++f) {
		if (std::memcmp(&current[e][f], &values[e][f], sizeof(float))) {
			refresh |= e == g_RequestedEnv;
			Audio_SetEnvParam(e, f, values[e][f]);
		}
	}
	if (refresh && iSoundActive && !g_AudioShutdown && alContext && g_AudioBackend == AudioBackend::OpenALSoft)
		Audio_SetEnvironment(g_RequestedEnv, 0);
	return WriteResult::Ok;
}
const char* ApplicationName(Application application) {
	switch (application) {
	case Application::Pending: return "Pending environment application";
	case Application::Reverb: return "Applied reverb";
	case Application::Dry: return "Applied dry environment 7";
	case Application::DryFallback: return "Dry fallback / EFX unavailable";
	case Application::Failed: return "Application failed / retry pending";
	case Application::LegacyUnverified: return "Legacy delegated / unverified";
	default: return "Audio disabled";
	}
}
} // namespace c2audio
