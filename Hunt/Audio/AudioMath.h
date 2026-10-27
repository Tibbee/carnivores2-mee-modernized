#ifndef C2_AUDIO_MATH_H
#define C2_AUDIO_MATH_H

#include <algorithm>
#include <cmath>
#include <cstdint>

inline float AudioGain(int volume)
{
    return static_cast<float>((std::max)(0, volume)) / 256.0f;
}

// RealTime holds DWORD uptime bits in a signed int. Convert BEFORE modulo:
// after ~24.9 days, signed remainders indexed before the three step sounds.
inline unsigned FootstepIndex(int realTime)
{
    return static_cast<std::uint32_t>(realTime) % 3u;
}

// Preserve forward-walking loudness, but not its accidental signed-speed bias.
// Eight is the existing cheat-flight speed; bound invalid inputs before int casts.
inline int FootstepVolume(float forwardSpeed, bool water)
{
    const float speed = std::isfinite(forwardSpeed)
        ? (std::min)(std::fabs(forwardSpeed), 8.0f) : 0.0f;
    return (water ? 64 : 24) + static_cast<int>(speed * (water ? 30.0f : 50.0f));
}

inline void AudioListenerOrientation(float yaw, float pitch, float (&out)[6])
{
    const float yawSin = std::sin(yaw), yawCos = std::cos(yaw);
    const float pitchSin = std::sin(pitch), pitchCos = std::cos(pitch);
    // Same forward direction as RotateVector / shooting: positive pitch looks down.
    out[0] = yawSin * pitchCos; out[1] = -pitchSin; out[2] = -yawCos * pitchCos;
    out[3] = yawSin * pitchSin; out[4] = pitchCos;  out[5] = -yawCos * pitchSin;
}

#endif
