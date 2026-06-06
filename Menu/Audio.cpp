#include "Hunt.h"
#include "OpenAL_Loader.h"

#include <iostream>

namespace
{
struct MenuAudioState
{
	ALCdevice* device = nullptr;
	ALCcontext* context = nullptr;

	ALuint ambientBuffer = 0;
	ALuint ambientSource = 0;

	ALuint hoverBuffer = 0;
	ALuint hoverSource = 0;

	ALuint clickBuffer = 0;
	ALuint clickSource = 0;

	ALuint typeBuffer = 0;
	ALuint typeSource = 0;

	ALuint typeGoBuffer = 0;
	ALuint typeGoSource = 0;

	bool active = false;
};

MenuAudioState g_MenuAudio;

bool UploadSound(ALuint& buffer, const SoundFX& sfx)
{
	if (!sfx.m_Data || !sfx.m_Length || !sfx.m_Frequency)
		return false;

	alGenBuffers(1, &buffer);
	if (!buffer)
		return false;

	alBufferData(buffer, AL_FORMAT_MONO16, sfx.m_Data, (ALsizei)sfx.m_Length, (ALsizei)sfx.m_Frequency);
	return true;
}

bool CreateSource(ALuint& source, ALuint buffer, bool loop, float gain)
{
	if (!buffer)
		return false;

	alGenSources(1, &source);
	if (!source)
		return false;

	alSourcei(source, AL_BUFFER, buffer);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSourcef(source, AL_GAIN, gain);
	alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	return true;
}

void DestroySource(ALuint& source)
{
	if (source) {
		alSourceStop(source);
		alDeleteSources(1, &source);
		source = 0;
	}
}

void DestroyBuffer(ALuint& buffer)
{
	if (buffer) {
		alDeleteBuffers(1, &buffer);
		buffer = 0;
	}
}

void PlaySource(ALuint source)
{
	if (!source)
		return;

	ALint state = AL_STOPPED;
	alGetSourcei(source, AL_SOURCE_STATE, &state);
	if (state == AL_PLAYING)
		alSourceStop(source);

	alSourcePlay(source);
}
}

bool MenuAudioInit()
{
	if (g_MenuAudio.active)
		return true;

	if (!LoadOpenAL()) {
		std::cout << "MenuAudio: OpenAL loader unavailable, audio disabled" << std::endl;
		return false;
	}

	g_MenuAudio.device = alcOpenDevice(nullptr);
	if (!g_MenuAudio.device) {
		std::cout << "MenuAudio: no default OpenAL device, audio disabled" << std::endl;
		UnloadOpenAL();
		return false;
	}

	g_MenuAudio.context = alcCreateContext(g_MenuAudio.device, nullptr);
	if (!g_MenuAudio.context || !alcMakeContextCurrent(g_MenuAudio.context)) {
		std::cout << "MenuAudio: failed to create OpenAL context, audio disabled" << std::endl;
		if (g_MenuAudio.context)
			alcDestroyContext(g_MenuAudio.context);
		alcCloseDevice(g_MenuAudio.device);
		g_MenuAudio.device = nullptr;
		g_MenuAudio.context = nullptr;
		UnloadOpenAL();
		return false;
	}

	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

	// Core sounds (required). If any of these fail, the menu is silent.
	bool coreOk = true;
	coreOk &= UploadSound(g_MenuAudio.ambientBuffer, g_MenuSound_Ambient);
	coreOk &= UploadSound(g_MenuAudio.hoverBuffer, g_MenuSound_Move);
	coreOk &= UploadSound(g_MenuAudio.clickBuffer, g_MenuSound_Go);

	coreOk &= CreateSource(g_MenuAudio.ambientSource, g_MenuAudio.ambientBuffer, true, 0.85f);
	coreOk &= CreateSource(g_MenuAudio.hoverSource, g_MenuAudio.hoverBuffer, false, 0.95f);
	coreOk &= CreateSource(g_MenuAudio.clickSource, g_MenuAudio.clickBuffer, false, 1.0f);

	if (!coreOk) {
		std::cout << "MenuAudio: core sound setup failed (ambient/hover/click), audio disabled" << std::endl;
		MenuAudioShutdown();
		return false;
	}

	// Typing sounds (optional). MEE-only assets (type.wav, typego.wav) are not
	// shipped with stock C2, so a missing file is fine - just no typing feedback.
	if (UploadSound(g_MenuAudio.typeBuffer, g_MenuSound_Type) &&
	    CreateSource(g_MenuAudio.typeSource, g_MenuAudio.typeBuffer, false, 0.9f)) {
		std::cout << "MenuAudio: typing sound (type.wav) loaded" << std::endl;
	} else {
		std::cout << "MenuAudio: typing sound unavailable (no type.wav) - typing feedback disabled" << std::endl;
		DestroyBuffer(g_MenuAudio.typeBuffer);
		g_MenuAudio.typeBuffer = 0;
	}

	if (UploadSound(g_MenuAudio.typeGoBuffer, g_MenuSound_TypeGo) &&
	    CreateSource(g_MenuAudio.typeGoSource, g_MenuAudio.typeGoBuffer, false, 1.0f)) {
		std::cout << "MenuAudio: typing-go sound (typego.wav) loaded" << std::endl;
	} else {
		std::cout << "MenuAudio: typing-go sound unavailable (no typego.wav) - Enter feedback disabled" << std::endl;
		DestroyBuffer(g_MenuAudio.typeGoBuffer);
		g_MenuAudio.typeGoBuffer = 0;
	}

	g_MenuAudio.active = true;
	std::cout << "MenuAudio: OpenAL audio initialized" << std::endl;
	return true;
}

void MenuAudioShutdown()
{
	if (g_MenuAudio.ambientSource)
		DestroySource(g_MenuAudio.ambientSource);
	if (g_MenuAudio.hoverSource)
		DestroySource(g_MenuAudio.hoverSource);
	if (g_MenuAudio.clickSource)
		DestroySource(g_MenuAudio.clickSource);
	if (g_MenuAudio.typeSource)
		DestroySource(g_MenuAudio.typeSource);
	if (g_MenuAudio.typeGoSource)
		DestroySource(g_MenuAudio.typeGoSource);

	if (g_MenuAudio.ambientBuffer)
		DestroyBuffer(g_MenuAudio.ambientBuffer);
	if (g_MenuAudio.hoverBuffer)
		DestroyBuffer(g_MenuAudio.hoverBuffer);
	if (g_MenuAudio.clickBuffer)
		DestroyBuffer(g_MenuAudio.clickBuffer);
	if (g_MenuAudio.typeBuffer)
		DestroyBuffer(g_MenuAudio.typeBuffer);
	if (g_MenuAudio.typeGoBuffer)
		DestroyBuffer(g_MenuAudio.typeGoBuffer);

	if (g_MenuAudio.context) {
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(g_MenuAudio.context);
		g_MenuAudio.context = nullptr;
	}

	if (g_MenuAudio.device) {
		alcCloseDevice(g_MenuAudio.device);
		g_MenuAudio.device = nullptr;
	}

	UnloadOpenAL();
	g_MenuAudio.active = false;
}

void MenuAudioStartAmbient()
{
	if (!g_MenuAudio.active || !g_MenuAudio.ambientSource)
		return;

	PlaySource(g_MenuAudio.ambientSource);
}

void MenuAudioStopAmbient()
{
	if (!g_MenuAudio.active || !g_MenuAudio.ambientSource)
		return;

	alSourceStop(g_MenuAudio.ambientSource);
}

void MenuAudioPlayHover()
{
	if (!g_MenuAudio.active || !g_MenuAudio.hoverSource)
		return;

	PlaySource(g_MenuAudio.hoverSource);
}

void MenuAudioPlayClick()
{
	if (!g_MenuAudio.active || !g_MenuAudio.clickSource)
		return;

	PlaySource(g_MenuAudio.clickSource);
}

void MenuAudioPlayType()
{
	if (!g_MenuAudio.active || !g_MenuAudio.typeSource)
		return;

	PlaySource(g_MenuAudio.typeSource);
}

void MenuAudioPlayTypeGo()
{
	if (!g_MenuAudio.active || !g_MenuAudio.typeGoSource)
		return;

	PlaySource(g_MenuAudio.typeGoSource);
}
