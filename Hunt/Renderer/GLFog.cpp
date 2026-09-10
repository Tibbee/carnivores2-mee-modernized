// World-model fog sampling. The shared inline implementation keeps one
// formula for public callers, exact models, legacy models, and tests.
#include "Hunt.h"
#include "Renderer/GLUtils.h"

FogSample SampleFogAtPoint(const Vector3d& point, bool disableFog)
{
    return SampleFogAtPointInline<true>(point, disableFog);
}

FogSample SamplePocketFogAtPoint(const Vector3d& point, bool disableFog)
{
    return SampleFogAtPointInline<false>(point, disableFog);
}
