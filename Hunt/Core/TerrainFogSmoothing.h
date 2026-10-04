#pragma once

#include "MathTypes.h"
#include <array>

// Blend once per tile, never in-place on the shared 3x3 chunk grid.
inline void SmoothFogColors(Vector3d& fog00, Vector3d& fog10,
                            Vector3d& fog01, Vector3d& fog11)
{
    constexpr float kBlend = 0.3f;
    constexpr float kInvBlend = 1.0f - kBlend;
    const float avgX = (fog00.x + fog10.x + fog01.x + fog11.x) * 0.25f;
    const float avgY = (fog00.y + fog10.y + fog01.y + fog11.y) * 0.25f;
    const float avgZ = (fog00.z + fog10.z + fog01.z + fog11.z) * 0.25f;
    for (Vector3d* fog : {&fog00, &fog10, &fog01, &fog11}) {
        fog->x = fog->x * kInvBlend + avgX * kBlend;
        fog->y = fog->y * kInvBlend + avgY * kBlend;
        fog->z = fog->z * kInvBlend + avgZ * kBlend;
    }
}

inline std::array<Vector3d, 4> SmoothedTileFogColors(
    const Vector3d& fog00, const Vector3d& fog10,
    const Vector3d& fog01, const Vector3d& fog11)
{
    std::array<Vector3d, 4> result{fog00, fog10, fog01, fog11};
    SmoothFogColors(result[0], result[1], result[2], result[3]);
    return result;
}
