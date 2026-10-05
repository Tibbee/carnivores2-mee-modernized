#ifndef C2_TEST_FAKE_OPENAL_H
#define C2_TEST_FAKE_OPENAL_H
// Strict, silent AL seam. Playing-buffer replacements and bad gains fail as on
// OpenAL Soft; selected operations can fail once for rollback/retry contracts.
#include "Audio/OpenAL_Loader.h"
#include <map>
#include <string>
#include <vector>
namespace fakeal {
struct Source { ALuint buffer=0, send=0; float gain=1; bool playing=false; int starts=0, offset=0; };
inline std::map<ALuint, Source> sources;
inline std::map<ALuint, int> buffers;
inline std::map<ALuint, ALuint> slots;
inline std::map<ALuint, bool> effects;
inline ALuint next=1;
inline ALenum error=0;
inline std::string failure, missingProc;
inline int failSourceAfter=-1, sourceGenerations=0, geometryCalls=0;
inline bool efx=true;
inline std::vector<std::string> logs;
inline float orientation[6]{};
inline bool Fail(const char* name) {
    if (failure != name) return false;
    failure.clear(); error=0xA003; return true;
}
inline ALenum GetError() { const auto e=error; error=0; return e; }
inline void GenBuffers(ALsizei n, ALuint* ids) {
    if (Fail("genBuffers")) { for(int i=0;i<n;++i) ids[i]=0; return; }
    for(int i=0;i<n;++i) { ids[i]=next++; buffers[ids[i]]=0; }
}
inline void DeleteBuffers(ALsizei n,const ALuint* ids) {
    for(int i=0;i<n;++i) {
        for(const auto& s:sources) if(s.second.buffer==ids[i]) { error=0xA004; return; }
        buffers.erase(ids[i]);
    }
}
inline void BufferData(ALuint id,ALenum,const ALvoid*,ALsizei bytes,ALsizei) {
    if(Fail("bufferData")) return;
    if(!buffers.count(id)||bytes<=0||bytes%2) { error=0xA003;return; }
    buffers[id]=bytes;
}
inline void GenSources(ALsizei n,ALuint* ids) {
    for(int i=0;i<n;++i) {
        if(Fail("genSources") || (failSourceAfter>=0 && sourceGenerations++>=failSourceAfter)) {
            error=0xA005; ids[i]=0; return;
        }
        ids[i]=next++; sources[ids[i]]={};
    }
}
inline void DeleteSources(ALsizei n,const ALuint* ids) {
    for(int i=0;i<n;++i) sources.erase(ids[i]);
}
inline void SourceI(ALuint id, ALenum parameter,ALint value) {
    if(Fail("sourceI")) return;
    if(!sources.count(id)) {error=0xA001;return;}
    auto& s=sources[id];
    if(parameter==AL_BUFFER) {
        if(s.playing) {error=0xA004;return;}
        if(value && !buffers.count(value)) {error=0xA001;return;}
        s.buffer=value; s.offset=0;
    }
}
inline void SourceF(ALuint id,ALenum parameter,ALfloat value) {
    if(Fail("sourceF")) return;
    if(!sources.count(id)) {error=0xA001;return;}
    if(parameter==AL_GAIN) {
        if(!(value>=0)) {error=0xA003;return;}
        sources[id].gain=value;
    }
}
inline void Source3F(ALuint,ALenum,ALfloat,ALfloat,ALfloat) { Fail("source3F"); }
inline void Source3I(ALuint id,ALenum,ALint slot,ALint,ALint) {
    if(Fail("source3I")) return;
    if(!sources.count(id)|| (slot && !slots.count(slot))) {error=0xA001;return;}
    sources[id].send=slot;
}
inline void Play(ALuint id) {
    if(Fail("play")) return;
    if(!sources.count(id)||!sources[id].buffer) {error=0xA004;return;}
    auto& s=sources[id]; s.playing=true; ++s.starts; s.offset=0;
}
inline void Stop(ALuint id) {
    if(Fail("stop")) return;
    if(!sources.count(id)) {error=0xA001;return;}
    sources[id].playing=false;
}
inline void GetSourceI(ALuint id,ALenum parameter,ALint* value) {
    if(Fail("getSourceI")) return;
    if(!sources.count(id)) {error=0xA001;return;}
    const auto& s=sources[id];
    *value= parameter==AL_BUFFER ? s.buffer : (s.playing?AL_PLAYING:AL_STOPPED);
}
inline void ListenerFV(ALenum parameter,const ALfloat* values) {
    if(parameter==AL_ORIENTATION) for(int i=0;i<6;++i) orientation[i]=values[i];
}
inline void ListenerF(ALenum,ALfloat) {}
inline void Listener3F(ALenum,ALfloat,ALfloat,ALfloat) {}
inline void DistanceModel(ALenum) { Fail("distanceModel"); }
inline ALCdevice* OpenDevice(const ALCchar*) {return reinterpret_cast<ALCdevice*>(1);}
inline ALCboolean CloseDevice(ALCdevice*) {return AL_TRUE;}
inline ALCcontext* CreateContext(ALCdevice*,const ALCint*) {return reinterpret_cast<ALCcontext*>(1);}
inline ALCboolean MakeCurrent(ALCcontext*) {return AL_TRUE;}
inline void DestroyContext(ALCcontext*) {}
inline ALCboolean Extension(ALCdevice*,const ALCchar* name) {return efx && std::string(name)=="ALC_EXT_EFX";}
inline void GenEffects(ALsizei n,ALuint* ids) {
    if(Fail("genEffects")) {for(int i=0;i<n;++i) ids[i]=0;return;}
    for(int i=0;i<n;++i) {ids[i]=next++;effects[ids[i]]=true;}
}
inline void DeleteEffects(ALsizei n,const ALuint* ids) {for(int i=0;i<n;++i) effects.erase(ids[i]);}
inline void EffectI(ALuint,ALenum,ALint) {Fail("effectI");}
inline void EffectF(ALuint,ALenum,ALfloat) {Fail("effectF");}
inline void GenSlots(ALsizei n,ALuint* ids) {
    if(Fail("genSlots")) {for(int i=0;i<n;++i) ids[i]=0;return;}
    for(int i=0;i<n;++i) {ids[i]=next++;slots[ids[i]]=0;}
}
inline void DeleteSlots(ALsizei n,const ALuint* ids) {for(int i=0;i<n;++i) slots.erase(ids[i]);}
inline void SlotI(ALuint id,ALenum,ALint effect) {if(!Fail("slotI")) slots[id]=effect;}
inline void SlotF(ALuint,ALenum,ALfloat) {Fail("slotF");}
inline void* GetProc(const ALchar* name) {
    if(missingProc==name) return nullptr;
#define FAKE_PROC(api, impl) if(std::string(name)==#api) return reinterpret_cast<void*>(&impl)
    FAKE_PROC(alGenEffects,GenEffects); FAKE_PROC(alDeleteEffects,DeleteEffects);
    FAKE_PROC(alEffecti,EffectI); FAKE_PROC(alEffectf,EffectF);
    FAKE_PROC(alGenAuxiliaryEffectSlots,GenSlots); FAKE_PROC(alDeleteAuxiliaryEffectSlots,DeleteSlots);
    FAKE_PROC(alAuxiliaryEffectSloti,SlotI); FAKE_PROC(alAuxiliaryEffectSlotf,SlotF);
    FAKE_PROC(alSource3i,Source3I);
#undef FAKE_PROC
    return nullptr;
}
inline void Install() {
    sources.clear();buffers.clear();slots.clear();effects.clear();logs.clear();
    next=1;error=0;failure.clear();missingProc.clear();efx=true;
    failSourceAfter=-1;sourceGenerations=geometryCalls=0;
    alcOpenDevice=OpenDevice;alcCloseDevice=CloseDevice;alcCreateContext=CreateContext;
    alcMakeContextCurrent=MakeCurrent;alcDestroyContext=DestroyContext;
    alGenBuffers=GenBuffers;alDeleteBuffers=DeleteBuffers;alBufferData=BufferData;
    alGenSources=GenSources;alDeleteSources=DeleteSources;
    alSourcei=SourceI;alSourcef=SourceF;alSource3f=Source3F;alSource3i=Source3I;
    alSourcePlay=Play;alSourceStop=Stop;alGetSourcei=GetSourceI;alGetError=GetError;
    alListenerfv=ListenerFV;alListenerf=ListenerF;alListener3f=Listener3F;
    alDistanceModel=DistanceModel;alcIsExtensionPresent=Extension;alGetProcAddress=GetProc;
    alGenEffects=GenEffects;alDeleteEffects=DeleteEffects;alEffecti=EffectI;alEffectf=EffectF;
    alGenAuxiliaryEffectSlots=GenSlots;alDeleteAuxiliaryEffectSlots=DeleteSlots;
    alAuxiliaryEffectSloti=SlotI;alAuxiliaryEffectSlotf=SlotF;
}
} // namespace fakeal
#endif
