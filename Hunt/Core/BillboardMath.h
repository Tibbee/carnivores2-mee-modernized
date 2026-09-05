// BillboardMath.h — dependency-free billboard geometry helpers
#pragma once

struct BillboardViewOffset
{
    float x;
    float y;
    float z;
};

inline BillboardViewOffset CalculateCylindricalBillboardViewOffset(
    float localX, float localY, float cameraPitchCos, float cameraPitchSin) noexcept
{
    return {
        localX,
        localY * cameraPitchCos,
        localY * cameraPitchSin
    };
}
