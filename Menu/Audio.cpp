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

	bool ok = true;
	ok &= UploadSound(g_MenuAudio.ambientBuffer, g_MenuSound_Ambient);
	ok &= UploadSound(g_MenuAudio.hoverBuffer, g_MenuSound_Move);
	ok &= UploadSound(g_MenuAudio.clickBuffer, g_MenuSound_Go);

	ok &= CreateSource(g_MenuAudio.ambientSource, g_MenuAudio.ambientBuffer, true, 0.85f);
	ok &= CreateSource(g_MenuAudio.hoverSource, g_MenuAudio.hoverBuffer, false, 0.95f);
	ok &= CreateSource(g_MenuAudio.clickSource, g_MenuAudio.clickBuffer, false, 1.0f);

	if (!ok) {
		std::cout << "MenuAudio: sound setup failed, audio disabled" << std::endl;
		MenuAudioShutdown();
		return false;
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

	if (g_MenuAudio.ambientBuffer)
		DestroyBuffer(g_MenuAudio.ambientBuffer);
	if (g_MenuAudio.hoverBuffer)
		DestroyBuffer(g_MenuAudio.hoverBuffer);
	if (g_MenuAudio.clickBuffer)
		DestroyBuffer(g_MenuAudio.clickBuffer);

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
