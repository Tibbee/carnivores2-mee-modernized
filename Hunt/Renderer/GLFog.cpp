// World-model fog sampling. Kept separate from GLUtils.cpp so the production
// sampler can be regression-tested without an OpenGL context.
#include "Hunt.h"
#include "Renderer/GLUtils.h"

#include <algorithm>
#include <cmath>

FogSample SampleFogAtPoint(const Vector3d& point, bool disableFog, bool distanceFallback)
{
    if (disableFog) {
        return {0.0f, GetDistanceFogColor()};
    }

    // Preserve the existing underwater model treatment. Pocket fog below
    // delegates to CalcFogLevel instead of maintaining a second formula.
    if (IsUnderwater()) {
        float d = VectorLength(point);
        const TFogEntity& fog = FogsList[127];
        float fla = -(point.y + CameraY - fog.YBegin * ctHScale) / ctHScale;
        float flb = -(CameraY - fog.YBegin * ctHScale) / ctHScale;
        float fl = 0.0f;
        if (!(fla < 0.0f && flb < 0.0f)) {
            if (fla < 0.0f) { d *= flb / (flb - fla); fla = 0.0f; }
            if (flb < 0.0f) { d *= fla / (fla - flb); flb = 0.0f; }
            fl = std::clamp((fla + flb) * (d + fog.Transp * 0.5f) / fog.Transp,
                            0.0f, fog.FLimit);
        }

        if (fl <= 0.0f) {
            fl = (d + fog.Transp * 0.5f) / fog.Transp;
        }

        float extinction = 1.0f - std::exp(-CameraWaterDepthFactor * 3.5f);
        fl *= 1.0f + extinction * 0.5f;

        float vertDepth = (std::max)(0.0f, fog.YBegin * ctHScale - (point.y + CameraY));
        float vertFactor = std::clamp(vertDepth / 512.0f, 0.0f, 1.0f);
        fl *= 1.0f + vertFactor * 2.5f;

        float capBoost = (1.0f - std::exp(-CameraWaterDepthFactor * 2.0f)) * 50.0f;
        fl = (std::min)(fl, fog.FLimit + capBoost);

        const float amount = std::clamp(fl / 255.0f, 0.0f, (fog.FLimit + capBoost) / 255.0f);
        return {amount, DecodeFogColorBGR(fog.fogRGB)};
    }

    if (FOGON) {
        const int worldX = static_cast<int>(point.x + CameraX);
        const int worldZ = static_cast<int>(point.z + CameraZ);
        const int mapFogIndex = FogsMap[(worldZ >> 9) & 511][(worldX >> 9) & 511];
        // Pass the ORIGINAL cell index, including zero: CalcFogLevel needs
        // zero to mark the destination as outside the camera pocket and
        // compute only the traversed fog segment. Resolving it here would
        // incorrectly treat a clear destination as inside the volume.
        const float amount = std::clamp(CalcFogLevel(point, mapFogIndex) / 255.0f, 0.0f, 1.0f);
        if (amount > 0.0f) {
            // Capture the same sun-shifted colour used by terrain/trees.
            return {amount, DecodeFogColor(CurFogColor)};
        }
    }

    if (!distanceFallback) {
        return {0.0f, GetDistanceFogColor()};
    }

    // Preserve the legacy model horizon fallback when no pocket contributes.
    const float d = VectorLength(point);
    const float fogDistance = static_cast<float>(ctViewR) * 256.0f;
    const float fogFadeStart = static_cast<float>(ctViewR) * 192.0f;
    const float fadeRange = fogDistance - fogFadeStart;
    const float distanceFog = std::clamp(
        (d - fogFadeStart) / (fadeRange > 1.0f ? fadeRange : 1.0f), 0.0f, 1.0f);
    return {distanceFog, GetDistanceFogColor()};
}
