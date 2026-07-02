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
    if (m_terrainShader) {
        glDeleteProgram(m_terrainShader);
        m_terrainShader = 0;
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

// The 3-arg version is the backward-compatible entry point.
// It delegates to the 6-arg overload (which has the actual implementation).
void GLRenderer::CollectTerrainTile(int x, int y, int r)
{
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);
    CollectTerrainTile(x, y, r, fadeStart, fadeStartSq, fadeEnd);
}

// §5.2: Chunked pair collection — processes two horizontally adjacent
// tiles (x1,y) and (x2,y) where x2 = x1 + 1. The two tiles share
// 3 VMap corners, 3 fog lookups, and 3 alpha computations instead
// of 8 each (25% reduction in per-tile work).
//
// Corner layout:
//   v00(x1,y) --- v10(x1+1,y) --- v20(x1+2,y)
//      |               |               |
//   v01(x1,y+1) --- v11(x1+1,y+1) --- v21(x1+2,y+1)
//
// Tile 1 uses corners: v00, v10, v01, v11
// Tile 2 uses corners: v10, v20, v11, v21
// The 4-arg version delegates to the 7-arg overload (which has the
// actual implementation with hoisted constants and de-dup cull).
void GLRenderer::CollectTerrainTilePair(int x1, int x2, int y, int r)
{
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);
    CollectTerrainTilePair(x1, x2, y, r, fadeStart, fadeStartSq, fadeEnd);
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
    glUseProgram(m_terrainShader);

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
    // (see the original CollectTerrainTile for the full rationale).
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (cz < 0.0f && std::fabs(cx * FOVK) > -cz + backR * 2.0f + 2048.0f) {
            return;
        }
    }

    // Fetch vertices
    EPoint v00 = VMap[localY][localX];
    if (v00.v.z > backR) {
        return;
    }

    EPoint v10 = VMap[localY][localX + 1];
    EPoint v01 = VMap[localY + 1][localX];
    EPoint v11 = VMap[localY + 1][localX + 1];

    // Precise frustum cull.  Use the horizontal-plane depth (cz1, computed
    // from tile-center world coords) for the frustum half-width, NOT the
    // camera-space z (zz).  When looking up, zz = cz1*cb + wy*sb can be
    // very shallow for edge tiles (terrain at camera height) while cz1
    // correctly reflects the horizontal distance.  Using -zz narrows the
    // frustum artificially and culls tiles that are well within the
    // horizontal view distance.  cz1 is the same formula as the coarse
    // test's forward depth but at the tile center (x+1, y+1).
    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;
    // Horizontal-plane depth (before pitch rotation):
    const float wx_center = static_cast<float>((x + 1) * 256) - CameraX;
    const float wz_center = static_cast<float>((y + 1) * 256) - CameraZ;
    const float cz1_center = wz_center * ca - wx_center * sa;
    // Use the deeper of cz1 and -zz so the frustum never collapses when
    // looking up.  Edge terrain tiles at camera height have cz1 >> -zz.
    const float depthForFrustum = (std::max)(cz1_center, -zz);
    if (std::fabs(xx * FOVK) > depthForFrustum + backR) {
        return;
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

// ---------------------------------------------------------------------------
// Phase 5: Overloaded CollectTerrainTilePair with hoisted fade constants.
// Original 4-arg version delegates here.
// Also fixes §4.8: tile-1 cull de-duplication (the frustum/distance test
// is evaluated once, and the result is reused for both triangle emission
// and RenderObject decision).
// ---------------------------------------------------------------------------

void GLRenderer::CollectTerrainTilePair(int x1, int x2, int y, int r,
                                        float fadeStart, float fadeStartSq, float fadeEnd)
{
    (void)r;

    // Boundary checks
    if (x1 < 0 || x2 < 0 || y < 0 ||
        x1 >= ctMapSize - 1 || x2 >= ctMapSize - 1 || y >= ctMapSize - 1) {
        if (x1 >= 0 && x1 < ctMapSize - 1 && y >= 0 && y < ctMapSize - 1)
            CollectTerrainTile(x1, y, r, fadeStart, fadeStartSq, fadeEnd);
        if (x2 >= 0 && x2 < ctMapSize - 1 && y >= 0 && y < ctMapSize - 1)
            CollectTerrainTile(x2, y, r, fadeStart, fadeStartSq, fadeEnd);
        return;
    }

    // Local coordinate checks
    const int localX1 = x1 - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX1 < 0 || localY < 0 || localX1 + 2 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        if (localX1 >= 0 && localX1 + 1 < kViewGridSize)
            CollectTerrainTile(x1, y, r, fadeStart, fadeStartSq, fadeEnd);
        if (localX1 + 1 >= 0 && localX1 + 2 < kViewGridSize)
            CollectTerrainTile(x2, y, r, fadeStart, fadeStartSq, fadeEnd);
        return;
    }

    float backR1 = BackViewR;
    if (OMap[y][x1] != 255) backR1 += MObjects[OMap[y][x1]].info.BoundR;
    float backR2 = BackViewR;
    if (OMap[y][x2] != 255) backR2 += MObjects[OMap[y][x2]].info.BoundR;
    const float backR = (std::max)(backR1, backR2);

    // Coarse frustum pre-test
    {
        const float wx = static_cast<float>((x1 + 1) * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x1]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (cz < 0.0f && std::fabs(cx * FOVK) > -cz + backR * 2.0f + 2048.0f) {
            return;
        }
    }

    // Fetch 6 VMap corners
    EPoint v00 = VMap[localY][localX1];
    EPoint v10 = VMap[localY][localX1 + 1];
    EPoint v20 = VMap[localY][localX1 + 2];
    EPoint v01 = VMap[localY + 1][localX1];
    EPoint v11 = VMap[localY + 1][localX1 + 1];
    EPoint v21 = VMap[localY + 1][localX1 + 2];

    if (v00.v.z > backR && v10.v.z > backR && v20.v.z > backR &&
        v01.v.z > backR && v11.v.z > backR && v21.v.z > backR) {
        return;
    }

    // Precompute fog indices, colors, alphas
    const int fogIdx_x1_y   = GetFogIndexForMapPoint(x1, y);
    const int fogIdx_x2_y   = GetFogIndexForMapPoint(x2, y);
    const int fogIdx_x3_y   = GetFogIndexForMapPoint(x2 + 1, y);
    const int fogIdx_x1_y1  = GetFogIndexForMapPoint(x1, y + 1);
    const int fogIdx_x2_y1  = GetFogIndexForMapPoint(x2, y + 1);
    const int fogIdx_x3_y1  = GetFogIndexForMapPoint(x2 + 1, y + 1);

    const Vector3d fogColor_x1_y  = GetFogColorForMapPoint(fogIdx_x1_y);
    const Vector3d fogColor_x2_y  = GetFogColorForMapPoint(fogIdx_x2_y);
    const Vector3d fogColor_x3_y  = GetFogColorForMapPoint(fogIdx_x3_y);
    const Vector3d fogColor_x1_y1 = GetFogColorForMapPoint(fogIdx_x1_y1);
    const Vector3d fogColor_x2_y1 = GetFogColorForMapPoint(fogIdx_x2_y1);
    const Vector3d fogColor_x3_y1 = GetFogColorForMapPoint(fogIdx_x3_y1);

    // Use hoisted fade constants
    const float alpha00 = CalcTerrainAlpha(VertexDistanceSq(v00.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha10 = CalcTerrainAlpha(VertexDistanceSq(v10.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha20 = CalcTerrainAlpha(VertexDistanceSq(v20.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha01 = CalcTerrainAlpha(VertexDistanceSq(v01.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha11 = CalcTerrainAlpha(VertexDistanceSq(v11.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);
    const float alpha21 = CalcTerrainAlpha(VertexDistanceSq(v21.v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);

    // Precompute fog amounts
    v00.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x1_y, v00.Fog, m_isUnderwater));
    v10.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x2_y, v10.Fog, m_isUnderwater));
    v20.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x3_y, v20.Fog, m_isUnderwater));
    v01.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x1_y1, v01.Fog, m_isUnderwater));
    v11.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x2_y1, v11.Fog, m_isUnderwater));
    v21.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x3_y1, v21.Fog, m_isUnderwater));

    // Tile 1: (x1, y) — de-duplicated cull
    {
        const float xx = (v00.v.x + v11.v.x) * 0.5f;
        const float yy = (v00.v.y + v11.v.y) * 0.5f;
        const float zz = (v00.v.z + v11.v.z) * 0.5f;
        // Use horizontal-plane depth (cz1) so the frustum doesn't
        // collapse when looking up (see CollectTerrainTile for rationale).
        const float wx_t1 = static_cast<float>((x1 + 1) * 256) - CameraX;
        const float wz_t1 = static_cast<float>((y + 1) * 256) - CameraZ;
        const float cz1_t1 = wz_t1 * ca - wx_t1 * sa;
        const float depthFrT1 = (std::max)(cz1_t1, -zz);

        const bool tile1PassedFrustum = (std::fabs(xx * FOVK) <= depthFrT1 + backR1);
        bool tile1InView = false;
        if (tile1PassedFrustum) {
            const float viewDistance = static_cast<float>(ctViewR * 256);
            const float viewDistanceSq = viewDistance * viewDistance;
            const float distanceSq = xx * xx + yy * yy + zz * zz;
            tile1InView = (distanceSq <= viewDistanceSq);
        }

        if (tile1InView) {
            constexpr float kAlphaCullThreshold = 0.02f;
            if (!(alpha00 < kAlphaCullThreshold && alpha10 < kAlphaCullThreshold &&
                  alpha01 < kAlphaCullThreshold && alpha11 < kAlphaCullThreshold)) {
                const bool reverse = (FMap[y][x1] & fmReverse) != 0;
                const int direction = FMap[y][x1] & 3;
                const int textureLayer = TMap1[y][x1];
                if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
                    if (reverse) {
                        AppendTerrainTriangle(v00, v10, v01, fogColor_x1_y, fogColor_x2_y, fogColor_x1_y1, textureLayer, reverse, false, direction, alpha00, alpha10, alpha01);
                        AppendTerrainTriangle(v01, v10, v11, fogColor_x1_y1, fogColor_x2_y, fogColor_x2_y1, textureLayer, reverse, true, direction, alpha01, alpha10, alpha11);
                    } else {
                        AppendTerrainTriangle(v00, v10, v11, fogColor_x1_y, fogColor_x2_y, fogColor_x2_y1, textureLayer, reverse, false, direction, alpha00, alpha10, alpha11);
                        AppendTerrainTriangle(v00, v11, v01, fogColor_x1_y, fogColor_x2_y1, fogColor_x1_y1, textureLayer, reverse, true, direction, alpha00, alpha11, alpha01);
                    }
                }
            }
            RenderObject(x1, y);
        }
    }

    // Tile 2: (x2, y)
    {
        const float xx = (v10.v.x + v21.v.x) * 0.5f;
        const float yy = (v10.v.y + v21.v.y) * 0.5f;
        const float zz = (v10.v.z + v21.v.z) * 0.5f;
        // Horizontal-plane depth for tile 2 (center at x2+1 = x1+2, y+1).
        const float wx_t2 = static_cast<float>((x2 + 1) * 256) - CameraX;
        const float wz_t2 = static_cast<float>((y + 1) * 256) - CameraZ;
        const float cz1_t2 = wz_t2 * ca - wx_t2 * sa;
        const float depthFrT2 = (std::max)(cz1_t2, -zz);

        if (std::fabs(xx * FOVK) <= depthFrT2 + backR2) {
            const float viewDistance = static_cast<float>(ctViewR * 256);
            const float viewDistanceSq = viewDistance * viewDistance;
            const float distanceSq = xx * xx + yy * yy + zz * zz;
            if (distanceSq <= viewDistanceSq) {
                constexpr float kAlphaCullThreshold = 0.02f;
                if (!(alpha10 < kAlphaCullThreshold && alpha20 < kAlphaCullThreshold &&
                      alpha11 < kAlphaCullThreshold && alpha21 < kAlphaCullThreshold)) {
                    const bool reverse = (FMap[y][x2] & fmReverse) != 0;
                    const int direction = FMap[y][x2] & 3;
                    const int textureLayer = TMap1[y][x2];
                    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
                        if (reverse) {
                            AppendTerrainTriangle(v10, v20, v11, fogColor_x2_y, fogColor_x3_y, fogColor_x2_y1, textureLayer, reverse, false, direction, alpha10, alpha20, alpha11);
                            AppendTerrainTriangle(v11, v20, v21, fogColor_x2_y1, fogColor_x3_y, fogColor_x3_y1, textureLayer, reverse, true, direction, alpha11, alpha20, alpha21);
                        } else {
                            AppendTerrainTriangle(v10, v20, v21, fogColor_x2_y, fogColor_x3_y, fogColor_x3_y1, textureLayer, reverse, false, direction, alpha10, alpha20, alpha21);
                            AppendTerrainTriangle(v10, v21, v11, fogColor_x2_y, fogColor_x3_y1, fogColor_x2_y1, textureLayer, reverse, true, direction, alpha10, alpha21, alpha11);
                        }
                    }
                }
                RenderObject(x2, y);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 2: 2x2 chunked collection — processes four tiles in a 2x2 block,
// reading a 3x3 grid of VMap cells (9 reads for 4 tiles vs 16 in 1x1 mode,
// a 44% reduction in VMap cache traffic). Used from the frustum sweep.
//
// Corner layout:
//   v00 -- v10 -- v20
//    |  T1  |  T2  |
//   v01 -- v11 -- v21
//    |  T3  |  T4  |
//   v02 -- v12 -- v22
// ---------------------------------------------------------------------------

void GLRenderer::CollectTerrainChunk2x2(int x, int y, int r,
                                        float fadeStart, float fadeStartSq, float fadeEnd)
{
    (void)r;

    // Boundary check — needs (x..x+2, y..y+2) within map and view grid
    if (x < 0 || y < 0 || x + 2 >= ctMapSize || y + 2 >= ctMapSize) {
        // Fall back to individual tiles (not pairs — the outer sweep
        // handles the odd-column/row leftovers, so this is defensive).
        CollectTerrainTile(x,     y,     0, fadeStart, fadeStartSq, fadeEnd);
        CollectTerrainTile(x + 1, y,     0, fadeStart, fadeStartSq, fadeEnd);
        CollectTerrainTile(x,     y + 1, 0, fadeStart, fadeStartSq, fadeEnd);
        CollectTerrainTile(x + 1, y + 1, 0, fadeStart, fadeStartSq, fadeEnd);
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 2 >= kViewGridSize || localY + 2 >= kViewGridSize) {
        CollectTerrainTile(x,     y,     0, fadeStart, fadeStartSq, fadeEnd);
        CollectTerrainTile(x + 1, y,     0, fadeStart, fadeStartSq, fadeEnd);
        CollectTerrainTile(x,     y + 1, 0, fadeStart, fadeStartSq, fadeEnd);
        CollectTerrainTile(x + 1, y + 1, 0, fadeStart, fadeStartSq, fadeEnd);
        return;
    }

    // Compute backR for all 4 tiles (max of individual backR values)
    float backR = BackViewR;
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            if (OMap[y + dy][x + dx] != 255) {
                float r2 = BackViewR + MObjects[OMap[y + dy][x + dx]].info.BoundR;
                if (r2 > backR) backR = r2;
            }
        }
    }

    // Coarse frustum pre-test for the chunk center
    {
        const float wx = static_cast<float>((x + 1) * 256 + 128) - CameraX;
        const float wz = static_cast<float>((y + 1) * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (cz < 0.0f && std::fabs(cx * FOVK) > -cz + backR * 2.0f + 2048.0f) {
            return;
        }
    }

    // Read 3x3 VMap corners (9 reads for 4 tiles, vs 16 in 1x1 mode)
    EPoint v00 = VMap[localY][localX];
    EPoint v10 = VMap[localY][localX + 1];
    EPoint v20 = VMap[localY][localX + 2];
    EPoint v01 = VMap[localY + 1][localX];
    EPoint v11 = VMap[localY + 1][localX + 1];
    EPoint v21 = VMap[localY + 1][localX + 2];
    EPoint v02 = VMap[localY + 2][localX];
    EPoint v12 = VMap[localY + 2][localX + 1];
    EPoint v22 = VMap[localY + 2][localX + 2];

    // Early z-check — if all 9 corners are behind BackViewR, skip
    if (v00.v.z > backR && v10.v.z > backR && v20.v.z > backR &&
        v01.v.z > backR && v11.v.z > backR && v21.v.z > backR &&
        v02.v.z > backR && v12.v.z > backR && v22.v.z > backR) {
        return;
    }

    // Per-corner fog lookups (9 fog indices / colors / amounts)
    // We compute fog at the 3x3 grid, shared by all 4 tiles.
    const int fogIdx[3][3] = {
        { GetFogIndexForMapPoint(x,     y    ), GetFogIndexForMapPoint(x + 1, y    ), GetFogIndexForMapPoint(x + 2, y    ) },
        { GetFogIndexForMapPoint(x,     y + 1), GetFogIndexForMapPoint(x + 1, y + 1), GetFogIndexForMapPoint(x + 2, y + 1) },
        { GetFogIndexForMapPoint(x,     y + 2), GetFogIndexForMapPoint(x + 1, y + 2), GetFogIndexForMapPoint(x + 2, y + 2) }
    };

    Vector3d fogColor[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            fogColor[i][j] = GetFogColorForMapPoint(fogIdx[i][j]);

    // Precompute fog amounts
    EPoint* corners[3][3] = {{&v00, &v10, &v20}, {&v01, &v11, &v21}, {&v02, &v12, &v22}};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            corners[i][j]->Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx[i][j], corners[i][j]->Fog, m_isUnderwater));

    // Precompute alphas for all 9 vertices
    float alpha[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            alpha[i][j] = CalcTerrainAlpha(VertexDistanceSq(corners[i][j]->v), fadeStart, fadeStartSq, fadeEnd, m_isUnderwater);

    constexpr float kAlphaCullThreshold = 0.02f;
    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;

    // Helper lambda to process one tile in the chunk
    auto processTile = [&](int tileX, int tileY, int ci, int cj) {
        // Tile uses corners (ci,cj), (ci+1,cj), (ci,cj+1), (ci+1,cj+1)
        const EPoint& tv00 = *corners[ci][cj];
        const EPoint& tv10 = *corners[ci][cj + 1];
        const EPoint& tv01 = *corners[ci + 1][cj];
        const EPoint& tv11 = *corners[ci + 1][cj + 1];

        const float xx = (tv00.v.x + tv11.v.x) * 0.5f;
        const float yy = (tv00.v.y + tv11.v.y) * 0.5f;
        const float zz = (tv00.v.z + tv11.v.z) * 0.5f;

        float tileBackR = BackViewR;
        if (OMap[tileY][tileX] != 255) tileBackR += MObjects[OMap[tileY][tileX]].info.BoundR;

        // Horizontal-plane depth for the frustum (see CollectTerrainTile).
        const float wx_tile2 = static_cast<float>((tileX + 1) * 256) - CameraX;
        const float wz_tile2 = static_cast<float>((tileY + 1) * 256) - CameraZ;
        const float cz1_tile2 = wz_tile2 * ca - wx_tile2 * sa;
        const float depthFrTile = (std::max)(cz1_tile2, -zz);
        if (std::fabs(xx * FOVK) > depthFrTile + tileBackR) return;

        const float distanceSq = xx * xx + yy * yy + zz * zz;
        if (distanceSq > viewDistanceSq) return;

        const float a00 = alpha[ci][cj];
        const float a10 = alpha[ci][cj + 1];
        const float a01 = alpha[ci + 1][cj];
        const float a11 = alpha[ci + 1][cj + 1];

        if (a00 < kAlphaCullThreshold && a10 < kAlphaCullThreshold &&
            a01 < kAlphaCullThreshold && a11 < kAlphaCullThreshold) {
            RenderObject(tileX, tileY);
            return;
        }

        const bool reverse = (FMap[tileY][tileX] & fmReverse) != 0;
        const int direction = FMap[tileY][tileX] & 3;
        const int textureLayer = TMap1[tileY][tileX];

        if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
            if (reverse) {
                AppendTerrainTriangle(tv00, tv10, tv01, fogColor[ci][cj], fogColor[ci][cj + 1], fogColor[ci + 1][cj], textureLayer, reverse, false, direction, a00, a10, a01);
                AppendTerrainTriangle(tv01, tv10, tv11, fogColor[ci + 1][cj], fogColor[ci][cj + 1], fogColor[ci + 1][cj + 1], textureLayer, reverse, true, direction, a01, a10, a11);
            } else {
                AppendTerrainTriangle(tv00, tv10, tv11, fogColor[ci][cj], fogColor[ci][cj + 1], fogColor[ci + 1][cj + 1], textureLayer, reverse, false, direction, a00, a10, a11);
                AppendTerrainTriangle(tv00, tv11, tv01, fogColor[ci][cj], fogColor[ci + 1][cj + 1], fogColor[ci + 1][cj], textureLayer, reverse, true, direction, a00, a11, a01);
            }
        }

        RenderObject(tileX, tileY);
    };

    // Process the 4 tiles
    processTile(x,     y,     0, 0);  // T1: v00,v10,v01,v11
    processTile(x + 1, y,     0, 1);  // T2: v10,v20,v11,v21
    processTile(x,     y + 1, 1, 0);  // T3: v01,v11,v02,v12
    processTile(x + 1, y + 1, 1, 1);  // T4: v11,v21,v12,v22
}

// ---------------------------------------------------------------------------

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
    // CollectTerrainTile / CollectTerrainTilePair don't recompute them
    // per call.
    const float tFadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float tFadeStartSq = tFadeStart * tFadeStart;
    const float tFadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    // Phase 1: Row-major tile sweep (replaces the outer-to-inner ring walk).
    // Iterates every cell in the view disk once, row by row, which is
    // simpler and more cache-friendly than the 4-sided perimeter walk.
    // Per-tile culling is unchanged.
    {
#ifdef GL_PERF_HOOKS
        GLPerfScope scope_walk("RenderGround_Walk");
#endif
        const int yLo = (std::max)(0, CCY - ctViewR);
        const int yHi = (std::min)(ctMapSize - 1, CCY + ctViewR);
        for (int y = yLo; y <= yHi; ++y) {
            int xLeft  = (std::max)(CCX - ctViewR, 0);
            int xRight = (std::min)(CCX + ctViewR, ctMapSize - 1);
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
