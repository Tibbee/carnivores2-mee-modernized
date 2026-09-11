// WaterObjectVisibility.h -- dependency-free water/object visibility helpers
#pragma once

inline bool ObjectIntersectsWaterSurface(float objectBaseY,
                                         float objectTopY,
                                         float waterSurfaceY) noexcept
{
    return objectBaseY < waterSurfaceY && objectTopY >= waterSurfaceY;
}
