#pragma once

// Flashlight accessory policy: cone maths, clamps and defaults shared by the
// game state (key/config), the GL shaders and any future software pass.
//
// Pure header -- no windows.h, no GL, no engine globals -- so the curve stays
// unit-testable without a renderer (tests/test_flashlight.cpp). This follows
// TerrainMip.h and WaterWaveFade.h: the policy lives here, the renderers only
// consume it.
//
// The reference look is luluco250's MIT-licensed Flashlight.fx (and the GShade
// "Spotlight" variant of it). Those are screen-space post-processes over a
// copied back buffer, so their falloff is a radial screen gradient scaled by
// linearized depth. This engine has real view-space positions in its world
// shaders (vViewPos / vViewZ / vRadialDist), so it lights actual geometry with
// a true cone and a real distance falloff instead -- no framebuffer copy, and
// the beam does not bleed onto the sky or the HUD. Two ideas do carry over:
// the warm tint, and the "blend fix" floor that keeps the beam visible on
// pure black surfaces (a purely multiplicative boost cannot lift black).

#include <cmath>

// Death must suppress both the world beam and flashlight-driven HUD grading,
// even if a stale toggle remains set. Health is authoritative, not UI mode.
inline constexpr bool FlashlightShouldIlluminate(bool equipped, bool switchedOn,
    int intensityPct, int hunterHealth)
{
    return equipped && switchedOn && intensityPct > 0 && hunterHealth > 0;
}

// Toggle binding. Stored outside TKeyMap -- which is blobsaved with
// sizeof(TKeyMap) into trophy0N.sav -- exactly like NightVisionKey, so adding
// it cannot shift the profile layout.
inline constexpr int kFlashlightKeyDefault     = 0x46; // 'F'
inline constexpr int kFlashlightKeyMin         = 0;
inline constexpr int kFlashlightKeyMax         = 255;

// Cone defaults. Angles are half-angles measured from the view axis, in
// degrees. Inside the inner angle the beam is at full strength; it fades to
// nothing at the outer angle.
inline constexpr float kFlashlightInnerAngleDegDefault = 14.0f;
inline constexpr float kFlashlightOuterAngleDegDefault = 26.0f;
inline constexpr float kFlashlightAngleDegMin          = 1.0f;
inline constexpr float kFlashlightAngleDegMax          = 90.0f;

// Beam reach in world units. One terrain cell is 256 units, so the default is
// about a third of the ~18700-unit view distance OptViewR=128 produces -- far
// enough to pick out an animal, short enough that the beam reads as a torch
// rather than a searchlight.
inline constexpr float kFlashlightRangeDefault = 6000.0f;
inline constexpr float kFlashlightRangeMin     = 256.0f;
inline constexpr float kFlashlightRangeMax     = 60000.0f;

// Peak gain inside the inner cone at zero distance, as a percent. 500 means
// surface colours are multiplied by up to 1 + 5 there (times the headroom
// roll-off), which is what it takes to make genuinely dark night ground read
// as torch-lit rather than merely less black: Classic night multiplies world
// colours by 0.5 and desaturates them, so a small boost is invisible. Moonlit
// first restores pre-night color locally, then applies this unchanged boost.
// Measured on
// area1 at night, this roughly doubles near foreground terrain luminance
// while keeping local texture contrast intact.
inline constexpr int kFlashlightIntensityPctDefault = 500;
inline constexpr int kFlashlightIntensityPctMin     = 0;
inline constexpr int kFlashlightIntensityPctMax     = 1000;

// Beam tint: warm white, matching the reference shader's (255, 230, 200).
inline constexpr float kFlashlightColorR = 1.0f;
inline constexpr float kFlashlightColorG = 0.902f;
inline constexpr float kFlashlightColorB = 0.784f;

// Floor added to beam-lit pixels so genuinely black surfaces still show the
// light. Without it the multiplicative boost leaves black at black and the
// beam vanishes on unlit night ground -- the same reason the reference port
// its "blend fix" (`max(color, (result - 1.0) * 0.001)`). Kept small: with the
// default intensity this term is what flattens shadowed terrain detail when it
// is too large, which is precisely the wash-out it replaced.
inline constexpr float kFlashlightBlendFixFloor = 0.03f;

// Exponent of the headroom roll-off. A linear boost clips every lit channel at
// 1.0 and the surface's own shading collapses into a flat wash, so the beam is
// scaled by how much headroom each pixel has left before white. Cubed, the
// curve behaves the way a torch should:
//   - near-black night ground (luma ~0.06) keeps ~83% of the beam, and because
//     the boost is multiplicative its own texture detail is preserved;
//   - mid-tone texels (luma ~0.5) keep ~12%, so they lift without pinning;
//   - anything near white keeps almost none, so nothing blows out.
// Quadratic leaves too much beam on daylight mid-tones (a 0.5-luma surface
// still reached ~0.94, i.e. clipped).
inline constexpr float kFlashlightHeadroomExponent = 3.0f;

inline int ClampFlashlightKey(int key)
{
    if (key < kFlashlightKeyMin) return kFlashlightKeyMin;
    if (key > kFlashlightKeyMax) return kFlashlightKeyMax;
    return key;
}

inline int ClampFlashlightRange(int range)
{
    if (range < static_cast<int>(kFlashlightRangeMin)) return static_cast<int>(kFlashlightRangeMin);
    if (range > static_cast<int>(kFlashlightRangeMax)) return static_cast<int>(kFlashlightRangeMax);
    return range;
}

inline int ClampFlashlightIntensityPct(int percent)
{
    if (percent < kFlashlightIntensityPctMin) return kFlashlightIntensityPctMin;
    if (percent > kFlashlightIntensityPctMax) return kFlashlightIntensityPctMax;
    return percent;
}

// Half-angle -> cosine of that angle off the view axis. The shaders compare
// `vViewZ / vRadialDist` (which is exactly that cosine, with the camera at the
// origin in view space) against these thresholds, so no per-fragment normal
// or trig is needed.
inline float FlashlightCosFromAngleDeg(float degrees)
{
    return std::cos(degrees * 3.14159265358979323846f / 180.0f);
}

// Keeps the inner/outer pair ordered. deltas < the outer angle must never be
// inverted: GLSL smoothstep(edge0, edge1, x) needs edge0 < edge1, and the
// cosines invert the ordering (a wider angle is a smaller cosine), so an
// unordered pair would silently produce an undefined ladder.
inline void NormalizeFlashlightConeDeg(float& innerDeg, float& outerDeg)
{
    float inner = innerDeg;
    float outer = outerDeg;
    if (inner < kFlashlightAngleDegMin) inner = kFlashlightAngleDegMin;
    if (outer < kFlashlightAngleDegMin) outer = kFlashlightAngleDegMin;
    if (inner > kFlashlightAngleDegMax) inner = kFlashlightAngleDegMax;
    if (outer > kFlashlightAngleDegMax) outer = kFlashlightAngleDegMax;
    if (outer < inner + 1.0f) outer = inner + 1.0f;
    if (outer > kFlashlightAngleDegMax) {
        outer = kFlashlightAngleDegMax;
        inner = std::fmin(inner, outer - 1.0f);
    }
    innerDeg = inner;
    outerDeg = outer;
}

// Cone term for one fragment. Mirrors the GLSL `smoothstep` used by the world
// shaders, so a unit test here pins the same curve the GPU evaluates.
inline float FlashlightConeTerm(float cosTheta, float cosInner, float cosOuter)
{
    if (cosOuter >= cosInner) return 0.0f;         // unordered pair: no beam
    float t = (cosTheta - cosOuter) / (cosInner - cosOuter);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// Distance term: linear falloff squared, reaching zero exactly at `range`.
inline float FlashlightDistanceTerm(float distance, float range)
{
    if (range <= 0.0f) return 0.0f;
    if (distance <= 0.0f) return 1.0f;
    const float t = 1.0f - distance / range;
    if (t <= 0.0f) return 0.0f;
    return t * t;
}

// How much of the beam a surface of the given luminance still receives.
// This is the anti-wash-out curve: because the boost multiplies the texel, a
// pixel with no headroom left would pin every channel at white and lose all of
// its own detail, which is what made lit terrain read as flat paint. Feeding
// the remaining headroom through `kFlashlightHeadroomExponent` instead keeps
// texture contrast intact and blunts the beam exactly where it cannot help.
inline float FlashlightSurfaceGain(float luminance)
{
    const float clamped = luminance < 0.0f ? 0.0f : (luminance > 1.0f ? 1.0f : luminance);
    const float headroom = 1.0f - clamped;
    return std::pow(headroom, kFlashlightHeadroomExponent);
}
