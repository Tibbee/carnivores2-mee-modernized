// ==========================================================================
// SoftMisc.cpp — Software renderer miscellaneous functions
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft

void AllocateRenderTables(void)
{
    // nothing to do yet for the software renderer
}

void RenderElements()
{
}

void RenderModelsList()
{
}

void ClearRendererLevelCache()
{
    // No-op: software renderer doesn't cache per-level model textures.
}

void ClearRendererTerrainCache()
{
    // No-op: software renderer doesn't cache terrain textures.
}

void ReleaseModelTexture(const TModel* /*mptr*/)
{
    // No-op: software renderer doesn't cache per-level model textures.
}

void Hardware_ZBuffer(BOOL /*bl*/)
{
}

void Render3DHardwarePosts()
{
}

void CopyHARDToDIB()
{
}

#endif // _soft
