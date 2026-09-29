#pragma once

// Sky cloud-texture placement policy, shared by both renderers.
//
// Pure header -- no windows.h, no GL, no engine globals -- so the selection
// rules and the projection maths stay unit-testable without a renderer
// (tests/test_sky_mode_mapping.cpp).
//
// The OpenGL path evaluates the mode-1 and mode-2 mapping in shaders/sky.frag
// and the mode-0 mapping through the legacy q/p/r projection coefficients; the
// software path evaluates the same maths on the CPU in SoftSky.cpp. The
// duplication between a shader and a scanline loop is unavoidable, so the
// shared, testable part is what must not drift: the legacy pitch, the camera
// basis, and the world-ray -> level-plane mapping. Both renderers call these.
//
// The software rasteriser interpolates the texture coordinate linearly across
// a scanline, because the per-pixel work lives in x86 assembly that is not
// being modernised. That is an approximation the projected-plane mode also
// already makes, but mode 1 amplifies it near the horizon: see
// architecture/terrain-rendering.md.

#include "Core/MathTypes.h"

#include <cmath>

namespace skyl {

// Mirrors SkyMappingMode in Core/GameState.h, which asserts agreement with
// these at compile time. Duplicated as a namespace so this header stays free
// of engine globals.
inline constexpr int kModeLegacy = 0;  // camera-coupled pitch offset
inline constexpr int kModeLevel  = 1;  // world-level projected plane
inline constexpr int kModeDome   = 2;  // OpenGL only
inline constexpr int kModeCount  = 3;

// Legacy plane texture scale in texels: 0.004 texels per world unit at the
// inherited 4*512*16 plane height (131.072 texels per unit cot). Identical
// to kPlaneTexelScale in GLSky.cpp :: RenderSkyPlane.
inline constexpr float kPlaneTexelScale = 0.004f * (4.0f * 512.0f * 16.0f);

// World-origin texture phase for the level plane, in texels.
inline constexpr float kPlaneAnchorScale = 0.004f;

// Texture coordinates are 8.8 fixed point, not raw texel counts. The
// software scanline routines compute the texel index as (coord << 8) >> 24,
// i.e. coord >> 16, and advance by (step << 8) >> 24 per pixel. So only the
// top eight bits select a texel and the low sixteen are sub-texel precision.
// The legacy projected plane happens to produce coordinates in the millions,
// which is why it has always had ample precision; a mapping that produces
// raw texel counts of order 10..130 would truncate to texel 0 everywhere and
// paint the sky from a single texel.
inline constexpr float kTexCoordScale = 65536.0f;  // 1 << 16

// The software rasteriser masks the coordinate to 24 bits, so it wraps every
// 2^24 units -- 256 texels, matching the 256x256 sky texture and the shader's
// REPEAT wrap.
inline constexpr int kTexCoordMask = (256 << 16) - 1;

// The software renderer implements the legacy and level-plane modes only.
// Asking for the dome falls back to the legacy plane rather than silently
// rendering something else, and the caller warns so config.cfg never claims
// an effect the renderer does not have.
inline int ResolveSoftwareSkyMode(int requested)
{
    return (requested == kModeLevel) ? kModeLevel : kModeLegacy;
}

// Sky-plane pitch for the legacy camera-coupled mode, OpenGL path.
//
// The offset is applied after yaw, so the plane's tilt direction followed the
// camera and the cloud rows leaned by up to the full offset while turning. It
// shrinks from 0.20 rad at ground level to 0.10 rad at altitude, so a high
// camera is not looking at the plane nearly edge-on.
//
// This is also the canonical fog basis in every mode: the inherited sky fog is
// a scanline-width proxy that must not change when sky_mode changes. The
// software renderer keeps the frozen offset below instead -- see
// ComputeSoftwareLegacySkyPitch for why the two differ.
inline float ComputeLegacySkyPitch(float cameraBeta, float cameraY, float heightScale)
{
    const float heightAboveTerrain = (cameraY < 0.0f) ? -cameraY : 0.0f;
    float altitudeFactor = heightAboveTerrain / (200.0f * heightScale);
    if (altitudeFactor < 0.0f) altitudeFactor = 0.0f;
    if (altitudeFactor > 1.0f) altitudeFactor = 1.0f;
    return cameraBeta - (0.20f - altitudeFactor * 0.10f);
}

// Sky-plane pitch for the legacy camera-coupled mode, software path.
//
// The 1998 rasteriser hardcoded a flat 0.15 rad. The 0.20..0.10 altitude
// ramp above is a later C2 refinement, and adopting it in the software
// renderer is a visible REGRESSION: it tilts the plane closer to edge-on at
// ground level, and the software sky has no mip chain, so the extra texture
// compression resolves into hard horizontal cloud banding instead of the soft
// gradient OpenGL's trilinear mip filtering produces from the same geometry.
// Measured at the 1024x768 capture scenario, 0.20 rad bands badly and the
// original 0.15 does not.
//
// So the software renderer keeps its own value. This is a deliberate,
// documented divergence rather than drift: the two renderers agree on what
// each MODE does (modes 0 and 1 are implemented the same way, and mode 1's
// mapping is shared verbatim), and differ only in a legacy constant that the
// software's sampling model cannot absorb. Mode 0 in software is therefore
// exactly what it was before this feature, with no visual change.
inline float ComputeSoftwareLegacySkyPitch(float cameraBeta)
{
    return cameraBeta - 0.15f;
}

// Camera axes in world space: the inverse of the engine's world->view rotation
// R = R_x(beta) * R_y(alpha), applied to (1,0,0), (0,1,0) and (0,0,-1).
//
// Note this is the INVERSE. RotateVector computes R*v (world->view), which is
// the wrong direction for anything that needs a view ray in world space.
// right.y is 0 because with no camera roll the right axis is always
// horizontal.
struct CameraBasis {
    Vector3d right;
    Vector3d up;
    Vector3d forward;
};

inline CameraBasis ComputeCameraBasis(float cameraAlpha, float cameraBeta)
{
    const float ca = std::cos(cameraAlpha);
    const float sa = std::sin(cameraAlpha);
    const float cb = std::cos(cameraBeta);
    const float sb = std::sin(cameraBeta);
    CameraBasis basis;
    basis.right   = { ca,       0.0f,  sa };
    basis.up      = { sb * sa,  cb,   -sb * ca };
    basis.forward = { cb * sa, -sb,   -cb * ca };
    return basis;
}

// World-space view ray for a screen offset, normalised.
//
// (sx, sy) use the engine's screen convention: sx is measured from the video
// centre, positive right; sy is measured from the bottom of the client up to
// the video centre, i.e. sy = VideoCY - scanline, matching both the software
// scanline loop and sky.frag's uVideoCenter-relative coordinates.
//
// The result is R^-1 * (sx/CameraW, sy/CameraH, -1), which is the same ray
// sky.frag builds from its NDC position and the tanX/tanY-scaled axes.
inline Vector3d ComputeWorldRay(float sx, float sy, float cameraW, float cameraH,
                                const CameraBasis& basis)
{
    const float ax = (cameraW > 1e-3f) ? (sx / cameraW) : 0.0f;
    const float ay = (cameraH > 1e-3f) ? (sy / cameraH) : 0.0f;
    Vector3d ray = {
        ax * basis.right.x   + ay * basis.up.x   + basis.forward.x,
        ax * basis.right.y   + ay * basis.up.y   + basis.forward.y,
        ax * basis.right.z   + ay * basis.up.z   + basis.forward.z,
    };
    const float length = std::sqrt(ray.x * ray.x + ray.y * ray.y + ray.z * ray.z);
    if (length > 1e-6f) {
        ray.x /= length;
        ray.y /= length;
        ray.z /= length;
    }
    return ray;
}

// World-level plane -> texture coordinate, in the software rasteriser's 8.8
// fixed point. Mirrors the uSkyMode == 1 branch of shaders/sky.frag:
//
//   denom   = wdir.y + dropSin
//   planeST = vec2(wdir.x, -wdir.z) * (planeScale / max(|denom|, 0.001))
//   planeST += anchor
//
// dropSin is sin(sky_horizon_drop) in degrees-to-sine form. It lowers the
// plane's compression singularity to a fixed world elevation below the true
// horizon, uniformly in azimuth, so the canopy never leans while turning.
//
// The result is pre-multiplied by kTexCoordScale: the maths is in texels,
// but the caller feeds fixed-point coordinates to the scanline routines.
// Omitting that scale is a silent failure -- the whole sky collapses to one
// texel -- so it lives here rather than at the call site.
inline void ComputeLevelPlaneTexel(const Vector3d& ray, float planeScale, float dropSin,
                                   float anchorX, float anchorY,
                                   float* outU, float* outV)
{
    const float denom = ray.y + dropSin;
    const float magnitude = std::fabs(denom);
    const float safe = (magnitude < 0.001f) ? 0.001f : magnitude;
    const float scale = planeScale / safe;
    *outU = (ray.x * scale + anchorX) * kTexCoordScale;
    *outV = (-ray.z * scale + anchorY) * kTexCoordScale;
}

// Horizon drop in degrees -> the sine form the mapping consumes.
inline float HorizonDropSine(float degrees)
{
    return std::sin(degrees * (3.14159265358979323846f / 180.0f));
}

} // namespace skyl
