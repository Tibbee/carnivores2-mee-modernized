// ProjectileMath.h — dependency-free projectile range helpers
#pragma once

inline float ProjectileViewRangeSquared(int viewRadiusCells) noexcept
{
    const float viewDistance = 256.0f * static_cast<float>(viewRadiusCells);
    return viewDistance * viewDistance;
}
