#pragma once

#include "OpenAL_Loader.h"

// ==========================================================================
// OpenALDevice.h -- Shared default-device and context lifecycle
// ==========================================================================
// The standalone menu and the engine each open the default playback device
// with the same sequence and tear it down in the same order; this header is
// that sequence's one owner. The loader stays with the call sites (they call
// UnloadOpenAL on failure and teardown) and so does failure logging, so each
// EXE keeps its own messages and log sink.
// ==========================================================================

// Distinct failure steps of OpenOpenALDevice so callers can keep their own
// messages for "no device" and "no context".
enum class OpenALDeviceResult { Ok, NoDevice, NoContext };

// Opens the default device and a context and makes the context current. On
// any failure the partially opened pair is released and both handles are
// left null. The caller owns UnloadOpenAL() and failure logging.
inline OpenALDeviceResult OpenOpenALDevice(ALCdevice*& device, ALCcontext*& context)
{
    device = alcOpenDevice(nullptr);
    if (!device) return OpenALDeviceResult::NoDevice;

    context = alcCreateContext(device, nullptr);
    if (!context || !alcMakeContextCurrent(context)) {
        if (context) alcDestroyContext(context);
        context = nullptr;
        alcCloseDevice(device);
        device = nullptr;
        return OpenALDeviceResult::NoContext;
    }
    return OpenALDeviceResult::Ok;
}

// Releases the context and the device; safe on an already-closed pair.
inline void CloseOpenALDevice(ALCdevice*& device, ALCcontext*& context)
{
    if (context) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        context = nullptr;
    }
    if (device) {
        alcCloseDevice(device);
        device = nullptr;
    }
}
