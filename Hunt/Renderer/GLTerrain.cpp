// ==========================================================================
// GLTerrain.cpp � Terrain rendering pipeline
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"
#include "Renderer/GLUtils.h"

#ifdef _gl

#include "glad/glad.h"
#include <cmath>

bool GLRenderer::InitializeTerrainPipeline()
{
    glGenVertexArrays(1, &m_terrainVAO);
    glGenBuffers(1, &m_terrainVBO);

    glBindVertexArray(m_terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Phase 1.5: packed TerrainVertex layout (32 bytes).
    //   attribute 0: vec3  aPos                (12 bytes, float)
    //   attribute 1: vec2  aTexCoord            ( 8 bytes, float)
    //   attribute 2: float aLayer               ( 4 bytes, float)
    //   attribute 3: vec4  light/fog/alpha/pad  ( 4 bytes, uint8 normalized)
    //   attribute 4: vec3  fogR/fogG/fogB       ( 3 bytes, uint8 normalized)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT,         GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT,         GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, layer)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, light)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, fogR)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void GLRenderer::ShutdownTerrainPipeline()
{
    if (m_terrainTextureArray) {
        glDeleteTextures(1, &m_terrainTextureArray);
        m_terrainTextureArray = 0;
    }
    if (m_terrainVBO) {
        glDeleteBuffers(1, &m_terrainVBO);
        m_terrainVBO = 0;
    }
    if (m_terrainVAO) {
        glDeleteVertexArrays(1, &m_terrainVAO);
        m_terrainVAO = 0;
    }


    m_terrainVertices.reset();
    m_terrainVertexCapacity = 0;
    m_terrainVertexCount = 0;
    m_waterVertices.reset();
    m_waterVertexCapacity = 0;
    m_waterVertexCount = 0;
    m_uploadedTerrainTextures.fill(nullptr);
}

void GLRenderer::EnsureTerrainVertexCapacity(size_t needed)
{
    if (needed <= m_terrainVertexCapacity) return;
    // Grow to the needed size (no shrinking — ctViewR rarely decreases).
    auto newBuf = std::make_unique<TerrainVertex[]>(needed);
    m_terrainVertices = std::move(newBuf);
    m_terrainVertexCapacity = needed;
}

void GLRenderer::EnsureWaterVertexCapacity(size_t needed)
{
    if (needed <= m_waterVertexCapacity) return;
    auto newBuf = std::make_unique<TerrainVertex[]>(needed);
    m_waterVertices = std::move(newBuf);
    m_waterVertexCapacity = needed;
}

void GLRenderer::BeginTerrainFrame()
{
    m_terrainVertexCount = 0;
    // §5.4: Ensure worst-case capacity for the current view distance.
    // Worst case: every cell in the visible disk produces 2 triangles = 6 vertices.
    // The 2x safety margin absorbs per-frame variance without reallocation.
    const size_t maxTiles = static_cast<size_t>(2 * ctViewR + 1) * static_cast<size_t>(2 * ctViewR + 1);
    EnsureTerrainVertexCapacity(maxTiles * 6);
    // m_waterVertices is cleared in BeginWaterFrame (called by RenderWater).
    // Clearing here too was redundant — RenderGround never touches water.
}

void GLRenderer::EnsureTerrainTextureArray()
{
    if (m_terrainTextureArray) {
        return;
    }

    glGenTextures(1, &m_terrainTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    // The original D3D/3DFX terrain/water renderers only ever sampled
    // DataA (128x128) and DataB (64x64).  DataC/DataD were generated
    // for the software renderer's very-far fallback, but using them in
    // the GL path made distant LOD tiles look oddly blurry / differently
    // textured.  Cap the array at mip level 1 to match the hardware
    // renderers.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);

    int size = 128;
    for (int level = 0; level < kTerrainMipLevels; ++level) {
        glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA8, size, size, kMaxTerrainTextureLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        const int nextSize = size / 2;
        size = nextSize < 1 ? 1 : nextSize;
    }
}

void GLRenderer::UploadTerrainLayer(int layer, const TEXTURE& texture)
{
    if (layer < 0 || layer >= kMaxTerrainTextureLayers) {
        return;
    }

    EnsureTerrainTextureArray();
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);

    const WORD* mipSources[kTerrainMipLevels] = {
        texture.DataA,
        texture.DataB,
        texture.DataC,
        texture.DataD
    };

    int mipSize = 128;
    for (int level = 0; level < kTerrainMipLevels; ++level) {
        std::vector<unsigned int> expanded(mipSize * mipSize);
        for (int i = 0; i < mipSize * mipSize; ++i) {
            expanded[i] = Expand1555to8888(mipSources[level][i]);
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, layer, mipSize, mipSize, 1, GL_RGBA, GL_UNSIGNED_BYTE, expanded.data());
        const int nextMipSize = mipSize / 2;
        mipSize = nextMipSize < 1 ? 1 : nextMipSize;
    }
}

void GLRenderer::AppendTerrainTriangle(const EPoint& v0,
                                       const EPoint& v1,
                                       const EPoint& v2,
                                       const Vector3d& fogColor0,
                                       const Vector3d& fogColor1,
                                       const Vector3d& fogColor2,
                                       int textureLayer,
                                       bool reverse,
                                       bool second,
                                       int direction,
                                       float alpha0,
                                       float alpha1,
                                       float alpha2)
{
    const auto uv = GetTerrainUVs(reverse, second, direction);
    const float layer = static_cast<float>(textureLayer);

    // Phase 1.5: pack light/fog (0..200, treated as 0..255 in the
    // shader) as uint8, alpha as uint8, and per-vertex fog color as a
    // vec3 of uint8. The driver normalizes the uint8 back to [0,1] in
    // the vertex shader, matching the old float layout.
    // §5.4: write directly to the flat array instead of push_back.
    TerrainVertex* dst = m_terrainVertices.get() + m_terrainVertexCount;
    dst[0] = {v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer,
              Light255ToByte(static_cast<float>(v0.Light)),
              Light255ToByte(v0.Fog),
              Float01ToByte(alpha0),
              0,  // pad1
              Float01ToByte(fogColor0.x),
              Float01ToByte(fogColor0.y),
              Float01ToByte(fogColor0.z),
              0};  // pad2
    dst[1] = {v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer,
              Light255ToByte(static_cast<float>(v1.Light)),
              Light255ToByte(v1.Fog),
              Float01ToByte(alpha1),
              0,  // pad1
              Float01ToByte(fogColor1.x),
              Float01ToByte(fogColor1.y),
              Float01ToByte(fogColor1.z),
              0};
    dst[2] = {v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer,
              Light255ToByte(static_cast<float>(v2.Light)),
              Light255ToByte(v2.Fog),
              Float01ToByte(alpha2),
              0,  // pad1
              Float01ToByte(fogColor2.x),
              Float01ToByte(fogColor2.y),
              Float01ToByte(fogColor2.z),
              0};
    m_terrainVertexCount += 3;
}



void GLRenderer::RenderTerrain()
{
    if (m_terrainVertexCount == 0) {
        return;
    }

    EnsureTerrainTextureArray();

    for (int layer = 0; layer < kMaxTerrainTextureLayers; ++layer) {
        if (!Textures[layer]) {
            continue;
        }
        if (m_uploadedTerrainTextures[layer] != Textures[layer].get()) {
            UploadTerrainLayer(layer, *Textures[layer]);
            m_uploadedTerrainTextures[layer] = Textures[layer].get();
        }
    }

    UpdatePerFrameUBO();
    SetWaterAlphaFade(0.0f, 0.0f, 0.0f, 765.0f);
    m_terrainShader.Use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_terrainTextureArray);
#endif
    glBindVertexArray(m_terrainVAO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    DrawVertexBatch(m_terrainVertices.get(), m_terrainVertexCount);

    glBindVertexArray(0);
}

void GLRenderer::ResetTerrainTextureCache()
{
    m_uploadedTerrainTextures.fill(nullptr);
    // Phase 3: invalidate the water bitmask on level transition.
    // It will be rebuilt on the first frame that needs it, AFTER the
    // new level's FMap has been loaded (the old code rebuilt here, but
    // that ran before LoadResources() populated the new FMap).
    m_waterBlockBitsValid = false;
}

void GLRenderer::ClearLevelTextureCache()
{
    // Clear per-level model texture and mesh caches between levels.
    // Per-level TModels are heap-allocated (unique addresses across
    // levels), so address recycling is not a concern, but the cache
    // entries and GPU resources from the previous level are dead
    // weight. Clearing them here prevents unbounded VRAM growth.
    // Global models (ChInfo, SunModel, etc.) survive this clear;
    // their textures and meshes are re-uploaded on first use next
    // level (a one-time cost per level transition).

    for (const auto& item : m_modelTextureCache) {
        if (item.second) {
            glDeleteTextures(1, &item.second);
        }
    }
    m_modelTextureCache.clear();

    for (const auto& item : m_bmpTextureCache) {
        if (item.second) {
            glDeleteTextures(1, &item.second);
        }
    }
    m_bmpTextureCache.clear();

    // Static mesh cache entries hold VBO/IBO offsets that are no longer
    // valid for the new level's models. Clear the map so UploadStaticMesh
    // re-uploads fresh geometry. The VBO/IBO data is orphaned but will be
    // reused when EnsureStaticMeshCapacity regrows the buffers.
    m_staticMeshCache.clear();

    m_skyTextureDirty = true;
}

// ---------------------------------------------------------------------------
// Phase 3: Water bitmask — coarse "has water" bitmap for 8x8 cell blocks.
// Built once at level load; skips CollectWaterTileFast for dry blocks.
// ---------------------------------------------------------------------------

bool GLRenderer::BlockHasWater(int x, int y) const
{
    if (!m_waterBlockBitsValid) return true;  // safe fallback: assume water
    const int bx = x >> kWaterBlockShift;
    const int by = y >> kWaterBlockShift;
    if (bx < 0 || by < 0 || bx >= kWaterBlockDim || by >= kWaterBlockDim) return false;
    const size_t idx = static_cast<size_t>(by) * kWaterBlockDim + static_cast<size_t>(bx);
    return (m_waterBlockBits[idx >> 6] >> (idx & 63)) & 1ULL;
}

void GLRenderer::RebuildWaterBlockBits()
{
    std::fill(m_waterBlockBits.begin(), m_waterBlockBits.end(), 0ULL);
    for (int by = 0; by < kWaterBlockDim; ++by) {
        for (int bx = 0; bx < kWaterBlockDim; ++bx) {
            bool any = false;
            for (int dy = 0; dy < 8 && !any; ++dy) {
                for (int dx = 0; dx < 8 && !any; ++dx) {
                    const int cx = (bx << kWaterBlockShift) + dx;
                    const int cy = (by << kWaterBlockShift) + dy;
                    if (cx < ctMapSize && cy < ctMapSize && (FMap[cy][cx] & fmWaterA)) {
                        any = true;
                    }
                }
            }
            if (any) {
                const size_t idx = static_cast<size_t>(by) * kWaterBlockDim + static_cast<size_t>(bx);
                m_waterBlockBits[idx >> 6] |= (1ULL << (idx & 63));
            }
        }
    }
    m_waterBlockBitsValid = true;
}

// ---------------------------------------------------------------------------
// Phase 5: Overloaded CollectTerrainTile with hoisted fade constants.
// These are called from the frustum-bounded sweep in RenderGround.
// The original 3-arg version delegates to these for backward compat.
// ---------------------------------------------------------------------------

void GLRenderer::CollectTerrainTile(int x, int y, int r,
                                    float fadeStart, float fadeStartSq, float fadeEnd)
{
    (void)r;

    if (x >= ctMapSize - 1 || y >= ctMapSize - 1 || x < 0 || y < 0) {
        return;
    }

    float backR = BackViewR;
    if (OMap[y][x] != 255) {
        backR += MObjects[OMap[y][x]].info.BoundR;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 1 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        return;
    }

    // Coarse frustum pre-test — SAFEGUARD: only reject when cz < 0
    // NOTE: no FOVK here — matches the software renderer's ProcessMap
    // formula.  The precise 4-corner check below still uses FOVK.
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (cz < 0.0f && std::fabs(cx) > -cz + backR * 2.0f + 2048.0f) {
            return;
        }
    }

    // Fetch vertices
    EPoint v00 = VMap[localY][localX];
    EPoint v10 = VMap[localY][localX + 1];
    EPoint v01 = VMap[localY + 1][localX];
    EPoint v11 = VMap[localY + 1][localX + 1];

    // Only reject if ALL corners are behind the back plane.
    if (v00.v.z > backR && v10.v.z > backR && v01.v.z > backR && v11.v.z > backR) {
        return;
    }

    // Precise frustum cull — conservative 4-corner check.
    // When looking up a slope, the tile center can be at shallower depth
    // than the elevated corners, causing the single-center test to cull
    // tiles that are still partially visible.  Only reject if ALL 4
    // corners are outside the same frustum side.
    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;
    {
        bool v00OutR = ( v00.v.x * FOVK > -v00.v.z + backR);
        bool v10OutR = ( v10.v.x * FOVK > -v10.v.z + backR);
        bool v01OutR = ( v01.v.x * FOVK > -v01.v.z + backR);
        bool v11OutR = ( v11.v.x * FOVK > -v11.v.z + backR);
        bool allOutR = v00OutR && v10OutR && v01OutR && v11OutR;

        bool v00OutL = (-v00.v.x * FOVK > -v00.v.z + backR);
        bool v10OutL = (-v10.v.x * FOVK > -v10.v.z + backR);
        bool v01OutL = (-v01.v.x * FOVK > -v01.v.z + backR);
        bool v11OutL = (-v11.v.x * FOVK > -v11.v.z + backR);
        bool allOutL = v00OutL && v10OutL && v01OutL && v11OutL;

        if (allOutR || allOutL) {
            return;
        }
    }

    // Distance cull
    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float distanceSq = xx * xx + yy * yy + zz * zz;
    if (distanceSq > viewDistanceSq) {
        return;
    }

    // Tile survived culling — compute fog + alpha using hoisted constants
    const int fogIdx00 = GetFogIndexForMapPoint(x, y);
    const int fogIdx10 = GetFogIndexForMapPoint(x + 1, y);
    const int fogIdx01 = GetFogIndexForMapPoint(x, y + 1);
    const int fogIdx11 = GetFogIndexForMapPoint(x + 1, y + 1);

    v00.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx00, v00.Fog, m_isUnderwater));
    v10.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx10, v10.Fog, m_isUnderwater));
    v01.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx01, v01.Fog, m_isUnderwater));
    v11.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx11, v11.Fog, m_isUnderwater));

    const Vector3d fog00 = GetFogColorForMapPoint(fogIdx00);
    const Vector3d fog10 = GetFogColorForMapPoint(fogIdx10);
    const Vector3d fog01 = GetFogColorForMapPoint(fogIdx01);
    const Vector3d fog11 = GetFogColorForMapPoint(fogIdx11);

    // Phase 5: use cached m_isUnderwater to skip the global load
    const float alpha00 = CalcTerrainAlpha(VertexDistanceSq(v00.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha10 = CalcTerrainAlpha(VertexDistanceSq(v10.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha01 = CalcTerrainAlpha(VertexDistanceSq(v01.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha11 = CalcTerrainAlpha(VertexDistanceSq(v11.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);

    // Alpha cull — skip tiles whose 4 vertex alphas are all below threshold
    constexpr float kAlphaCullThreshold = 0.02f;
    if (alpha00 < kAlphaCullThreshold && alpha10 < kAlphaCullThreshold &&
        alpha01 < kAlphaCullThreshold && alpha11 < kAlphaCullThreshold) {
        RenderObject(x, y);
        return;
    }

    const bool reverse = (FMap[y][x] & fmReverse) != 0;
    const int direction = FMap[y][x] & 3;

    const int textureLayer = TMap1[y][x];
    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
        if (reverse) {
            AppendTerrainTriangle(v00, v10, v01, fog00, fog10, fog01, textureLayer, reverse, false, direction, alpha00, alpha10, alpha01);
            AppendTerrainTriangle(v01, v10, v11, fog01, fog10, fog11, textureLayer, reverse, true, direction, alpha01, alpha10, alpha11);
        } else {
            AppendTerrainTriangle(v00, v10, v11, fog00, fog10, fog11, textureLayer, reverse, false, direction, alpha00, alpha10, alpha11);
            AppendTerrainTriangle(v00, v11, v01, fog00, fog11, fog01, textureLayer, reverse, true, direction, alpha00, alpha11, alpha01);
        }
    }

    RenderObject(x, y);
}

void GLRenderer::RenderGround()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderGround");
#endif
    BeginTerrainFrame();
    m_worldModelItems.clear();
    m_transparentModelItems.clear();
    m_objectList.clear();

    // Cache IsUnderwater() once per frame (Phase 5).
    m_isUnderwater = IsUnderwater();

    // Phase 3: lazily rebuild the water bitmask if it was invalidated
    // by a level transition (the bitmap must be built AFTER the new
    // level's FMap has been populated by LoadResources).
    if (NeedWater && !m_waterBlockBitsValid) {
        RebuildWaterBlockBits();
    }

    // If water is needed this frame, begin water collection here so the
    // water constants are ready for the ring walk.
    if (NeedWater) {
        BeginWaterFrame();
    }

    // Precompute water distance/fade constants once for the ring walk.
    float wViewDistSq = 0.0f, wFadeStart = 0.0f, wFadeStartSq = 0.0f;
    float wFadeEnd = 0.0f, wFadeEndSq = 0.0f;
    if (NeedWater) {
        const float vd = static_cast<float>(ctViewR * 256);
        wViewDistSq = vd * vd;
        wFadeStart = static_cast<float>((ctViewR - 8) << 8);
        wFadeStartSq = wFadeStart * wFadeStart;
        wFadeEnd = 256.0f * static_cast<float>(ctViewR - 4);
        wFadeEndSq = wFadeEnd * wFadeEnd;
    }

    // Hoist terrain fade constants once per frame (Phase 5) so that
    // CollectTerrainTile doesn't recompute them
    // per call.
    const float tFadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float tFadeStartSq = tFadeStart * tFadeStart;
    const float tFadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    // Phase 1: Frustum-bounded row sweep (replaces the full-disk ring walk).
    //
    // Camera space (per RotateVector in Vector.cpp; this engine's sign
    // convention is FORWARD = cz < 0, BEHIND = cz > 0, confirmed by the
    // coarse test `cz<0 && |cx|>-cz+...` and the projection `v.x/v.z`):
    //   cx  = wx*ca + wz*sa           (lateral)
    //   cz1 = wz*ca - wx*sa           (forward, pre-pitch)
    //   cz  = cz1*cb + wy*sb          (forward, post-pitch)
    // For a cell at offset (dx,dy) from (CCX,CCY), ignoring the sub-cell
    // camera residual (< 1 cell, absorbed by kMargin below):
    //   cx  = 256*(dx*ca + dy*sa)
    //   cz1 = 256*(dy*ca - dx*sa)
    // The precise per-corner frustum test in CollectTerrainTile keeps a
    // corner when |cx|*FOVK <= -cz + backR.  Substituting cz = cz1*cb + wy*sb
    // and using the most permissive terrain height wyEff (a safe superset
    // over the global height range, so the bound never misses a visible
    // tile regardless of pitch) yields two half-planes in (dx,dy):
    //   right:  Ar*dx + Br*dy <= Cr
    //   left:   Al*dx + Bl*dy >= Cl
    //   Ar = ca*FOVK - sa*cb,  Br = sa*FOVK + ca*cb
    //   Al = ca*FOVK + sa*cb,  Bl = sa*FOVK - ca*cb
    //   Cr = (backR - wyEff*sb)/256,  Cl = -Cr
    // For each row dy the sign of Ar/Al decides whether the edge gives a
    // lower or upper dx bound; a zero coefficient means the edge is
    // parallel to the row and the whole row is either in that half-plane
    // or outside it (skip).  The dx range is intersected with the
    // view-distance disk dx^2+dy^2 <= ctViewR^2 (a superset of the per-tile
    // 3D distance cull, since 3D distance >= 2D distance), expanded by
    // kMargin for the tile-corner span + camera residual + rounding, then
    // clamped to the map.  The unchanged per-tile coarse/precise/distance
    // culls inside CollectTerrainTile still run and remove any slack, so
    // the emitted geometry is identical to the full-disk walk.
    {
#ifdef GL_PERF_HOOKS
        GLPerfScope scope_walk("RenderGround_Walk");
#endif
        // Per-frame frustum coefficients.
        const float fovk = FOVK;
        const float ca_  = ca, sa_ = sa, cb_ = cb, sb_ = sb;
        // Safe terrain-height bounds relative to the camera (global map
        // range 0..255; conservative superset so pitch never misses a tile).
        const float wyMin = 0.0f - CameraY;
        const float wyMax = 255.0f * static_cast<float>(ctHScale) - CameraY;
        // wyEff maximizes (-wy*sb): lowest terrain when looking down,
        // highest terrain when looking up, so the bound is a superset of
        // the true pitched frustum for every possible corner height.
        const float wyEff = (sb_ > 0.0f) ? wyMin : (sb_ < 0.0f ? wyMax : 0.0f);
        const float P  = BackViewR - wyEff * sb_;   // effective near offset (world units)
        const float Cr = P / 256.0f;                // in cells
        const float Cl = -Cr;
        const float Ar = ca_ * fovk - sa_ * cb_;
        const float Br = sa_ * fovk + ca_ * cb_;
        const float Al = ca_ * fovk + sa_ * cb_;
        const float Bl = sa_ * fovk - ca_ * cb_;

        const float ctViewRf = static_cast<float>(ctViewR);
        const float ctViewR2 = ctViewRf * ctViewRf;
        // Margin: tile corners span +/-0.5 cell from the center, the
        // sub-cell camera residual is < 1 cell, and floor/ceil rounding
        // can eat ~1 cell.  3 cells comfortably covers all of it.
        constexpr float kMargin = 3.0f;

        const int yLo = (std::max)(0, CCY - ctViewR);
        const int yHi = (std::min)(ctMapSize - 1, CCY + ctViewR);
        for (int y = yLo; y <= yHi; ++y) {
            const float dy = static_cast<float>(y - CCY);
            // View-distance disk dx-extent for this row.  This is a safe
            // superset of the per-tile 3D distance cull because adding the
            // height term only increases the distance.
            const float d2 = ctViewR2 - dy * dy;
            if (d2 <= 0.0f) continue;
            const float D = std::sqrt(d2);
            float lo = -D, hi = D;
            bool skip = false;

            // Right half-plane: Ar*dx + Br*dy <= Cr
            const float rhsR = Cr - Br * dy;
            if      (Ar >  1e-12f) hi = (std::min)(hi, rhsR / Ar);
            else if (Ar < -1e-12f) lo = (std::max)(lo, rhsR / Ar);
            else if (Br * dy > Cr)  skip = true;   // edge parallel to row: row outside

            // Left half-plane: Al*dx + Bl*dy >= Cl
            if (!skip) {
                const float rhsL = Cl - Bl * dy;   // Al*dx >= rhsL
                if      (Al >  1e-12f) lo = (std::max)(lo, rhsL / Al);
                else if (Al < -1e-12f) hi = (std::min)(hi, rhsL / Al);
                else if (Bl * dy < Cl)  skip = true;
            }
            if (skip) continue;

            lo -= kMargin;
            hi += kMargin;
            if (lo < -ctViewRf) lo = -ctViewRf;
            if (hi >  ctViewRf) hi =  ctViewRf;
            if (lo > hi) continue;

            int xLeft  = (std::max)(0,             CCX + static_cast<int>(std::floor(lo)));
            int xRight = (std::min)(ctMapSize - 1, CCX + static_cast<int>(std::ceil(hi)));
            if (xLeft > xRight) continue;

            for (int x = xLeft; x <= xRight; ++x) {
                CollectTerrainTile(x, y, 0, tFadeStart, tFadeStartSq, tFadeEnd);
                if (NeedWater && BlockHasWater(x, y)) {
                    CollectWaterTileFast(x, y, 0, wViewDistSq, wFadeStart, wFadeStartSq, wFadeEnd, wFadeEndSq);
                }
            }
        }
    }

    RenderTerrain();
}
#endif // _gl
