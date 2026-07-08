// ==========================================================================
// WaterColor.h — Water-colour-aware submersion depth modulation
// ==========================================================================
// Used by the underwater effects (§3.1 overlay, §3.2 fog colour, and the
// §3.4 terrain wavelength attenuation) so that the depth tint respects the
// actual colour of the body of water the camera is in.
//
// The old implementation used a FIXED, blue-biased absorption curve (red
// absorbed fastest, blue slowest). That is correct only for clear, blue
// ocean water. For brown tannin / swamp water it wrongly drove the colour
// toward dark navy / teal at depth.
//
// This version is water-colour aware: the dominant channel(s) of the
// surface colour are transmitted (survive with depth) while the weaker
// channels are absorbed. Blue ocean therefore deepens to dark blue, while
// brown swamp water deepens to dark brown — i.e. a darker shade of the
// water's OWN hue.
//
// The dominant-channel floor (kFloor) keeps the whole colour from going
// pure black at full depth (the dominant channel fades to ~exp(-kFloor) ≈
// 67%), while kScale adds extra absorption on the non-dominant channels so
// off-hue tints wash out as you descend.
#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

// Modulate a colour in place. Channels are integers in 0..255.
// depth: 0 at the surface, 1 at the reference depth (CameraWaterDepthFactor).
inline void ModulateWaterColorByDepth(int& r, int& g, int& b, float depth)
{
    if (depth <= 0.0f) {
        return;
    }

    const int maxC = (std::max)((std::max)(r, g), b);
    if (maxC <= 0) {
        return;  // black water stays black
    }

    constexpr float kFloor = 0.40f;   // baseline absorption of the dominant channel
    constexpr float kScale = 2.20f;   // extra absorption for non-dominant channels

    const float d  = (std::max)(0.0f, (std::min)(1.0f, depth));
    const float kR = kFloor + kScale * (1.0f - static_cast<float>(r) / maxC);
    const float kG = kFloor + kScale * (1.0f - static_cast<float>(g) / maxC);
    const float kB = kFloor + kScale * (1.0f - static_cast<float>(b) / maxC);

    const float fR = std::exp(-d * kR);
    const float fG = std::exp(-d * kG);
    const float fB = std::exp(-d * kB);

    // Tiny floor avoids pure-black channels (looks broken) without forcing
    // any particular hue — the result remains a dark shade of the water's
    // own colour.
    r = (std::max)(2, (std::min)(255, static_cast<int>(r * fR + 0.5f)));
    g = (std::max)(2, (std::min)(255, static_cast<int>(g * fG + 0.5f)));
    b = (std::max)(2, (std::min)(255, static_cast<int>(b * fB + 0.5f)));
}

// Convenience for the packed BGR fog colour used by WaterList[w].fogRGB and
// FogsList[127].fogRGB: bits 0-7 = Blue, 8-15 = Green, 16-23 = Red.
inline int ModulateWaterColorByDepthBGR(int packed, float depth)
{
    if (depth <= 0.0f) {
        return packed;
    }
    int b =  packed        & 0xFF;
    int g = (packed >>  8) & 0xFF;
    int r = (packed >> 16) & 0xFF;
    ModulateWaterColorByDepth(r, g, b, depth);
    return (r << 16) | (g << 8) | b;
}
