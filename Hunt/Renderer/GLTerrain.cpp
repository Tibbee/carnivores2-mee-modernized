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

    m_terrainVertices.clear();
    m_waterVertices.clear();
    m_uploadedTerrainTextures.fill(nullptr);
}

void GLRenderer::BeginTerrainFrame()
{
    m_terrainVertices.clear();
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
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, kTerrainMipLevels - 1);

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

void GLRenderer::AppendTerrainTriangle(std::vector<TerrainVertex>& vertices,
                                       const EPoint& v0,
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
    vertices.push_back({v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer,
                        Light255ToByte(static_cast<float>(v0.Light)),
                        Light255ToByte(v0.Fog),
                        Float01ToByte(alpha0),
                        0,  // pad1
                        Float01ToByte(fogColor0.x),
                        Float01ToByte(fogColor0.y),
                        Float01ToByte(fogColor0.z),
                        0});  // pad2
    vertices.push_back({v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer,
                        Light255ToByte(static_cast<float>(v1.Light)),
                        Light255ToByte(v1.Fog),
                        Float01ToByte(alpha1),
                        0,  // pad1
                        Float01ToByte(fogColor1.x),
                        Float01ToByte(fogColor1.y),
                        Float01ToByte(fogColor1.z),
                        0});
    vertices.push_back({v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer,
                        Light255ToByte(static_cast<float>(v2.Light)),
                        Light255ToByte(v2.Fog),
                        Float01ToByte(alpha2),
                        0,  // pad1
                        Float01ToByte(fogColor2.x),
                        Float01ToByte(fogColor2.y),
                        Float01ToByte(fogColor2.z),
                        0});
}

void GLRenderer::CollectTerrainTile(int x, int y, int r)
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

    // Coarse frustum pre-test using map coordinates + HMapO height
    // estimate.  Avoids reading 4 VMap EPoints (64 bytes) for tiles
    // that are clearly outside the horizontal frustum.
    // Uses a generous margin (backR*2 + 2048) so we never false-reject
    // tiles that the precise VMap-based test would accept.
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (std::fabs(cx * FOVK) > -cz + backR * 2.0f + 2048.0f) {
            return;
        }
    }

    // Fetch vertices — needed for precise frustum + distance culling
    EPoint v00 = VMap[localY][localX];
    if (v00.v.z > backR) {
        return;
    }

    EPoint v10 = VMap[localY][localX + 1];
    EPoint v01 = VMap[localY + 1][localX];
    EPoint v11 = VMap[localY + 1][localX + 1];

    // Precise frustum cull using VMap vertices
    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + backR) {
        return;
    }

    // Distance cull
    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float distanceSq = xx * xx + yy * yy + zz * zz;
    if (distanceSq > viewDistanceSq) {
        return;
    }

    // Tile survived culling — now compute fog + alpha
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    const int fogIdx00 = GetFogIndexForMapPoint(x, y);
    const int fogIdx10 = GetFogIndexForMapPoint(x + 1, y);
    const int fogIdx01 = GetFogIndexForMapPoint(x, y + 1);
    const int fogIdx11 = GetFogIndexForMapPoint(x + 1, y + 1);

    v00.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx00, v00.Fog));
    v10.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx10, v10.Fog));
    v01.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx01, v01.Fog));
    v11.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx11, v11.Fog));

    const Vector3d fog00 = GetFogColorForMapPoint(fogIdx00);
    const Vector3d fog10 = GetFogColorForMapPoint(fogIdx10);
    const Vector3d fog01 = GetFogColorForMapPoint(fogIdx01);
    const Vector3d fog11 = GetFogColorForMapPoint(fogIdx11);

    const float alpha00 = CalcTerrainAlpha(VertexDistanceSq(v00.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha10 = CalcTerrainAlpha(VertexDistanceSq(v10.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha01 = CalcTerrainAlpha(VertexDistanceSq(v01.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha11 = CalcTerrainAlpha(VertexDistanceSq(v11.v), fadeStart, fadeStartSq, fadeEnd);

    const bool reverse = (FMap[y][x] & fmReverse) != 0;
    const int direction = FMap[y][x] & 3;

    const int textureLayer = TMap1[y][x];
    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
        if (reverse) {
            AppendTerrainTriangle(m_terrainVertices, v00, v10, v01, fog00, fog10, fog01, textureLayer, reverse, false, direction, alpha00, alpha10, alpha01);
            AppendTerrainTriangle(m_terrainVertices, v01, v10, v11, fog01, fog10, fog11, textureLayer, reverse, true, direction, alpha01, alpha10, alpha11);
        } else {
            AppendTerrainTriangle(m_terrainVertices, v00, v10, v11, fog00, fog10, fog11, textureLayer, reverse, false, direction, alpha00, alpha10, alpha11);
            AppendTerrainTriangle(m_terrainVertices, v00, v11, v01, fog00, fog11, fog01, textureLayer, reverse, true, direction, alpha00, alpha11, alpha01);
        }
    }

    RenderObject(x, y);
}

void GLRenderer::CollectTerrainTile2(int x, int y, int r)
{
    (void)r;

    if (x >= ctMapSize - 2 || y >= ctMapSize - 2 || x < 0 || y < 0) {
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 2 >= kViewGridSize || localY + 2 >= kViewGridSize) {
        return;
    }

    // Coarse frustum pre-test (same pattern as CollectTerrainTile)
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (std::fabs(cx * FOVK) > -cz + BackViewR * 2.0f + 2048.0f) {
            return;
        }
    }

    EPoint v00 = VMap[localY][localX];
    if (v00.v.z > BackViewR) {
        return;
    }

    const int textureLayer = TMap2[y][x];

    EPoint v20 = VMap[localY][localX + 2];
    EPoint v02 = VMap[localY + 2][localX];
    EPoint v22 = VMap[localY + 2][localX + 2];

    // Frustum + distance culls before fog computation
    const float xx = (v00.v.x + v22.v.x) * 0.5f;
    const float yy = (v00.v.y + v22.v.y) * 0.5f;
    const float zz = (v00.v.z + v22.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float distanceSq = xx * xx + yy * yy + zz * zz;
    if (distanceSq > viewDistanceSq) {
        return;
    }

    // Tile survived — now compute fog + alpha
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    const int fogIdx00 = GetFogIndexForMapPoint(x, y);
    const int fogIdx20 = GetFogIndexForMapPoint(x + 2, y);
    const int fogIdx02 = GetFogIndexForMapPoint(x, y + 2);
    const int fogIdx22 = GetFogIndexForMapPoint(x + 2, y + 2);

    v00.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx00, v00.Fog));
    v20.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx20, v20.Fog));
    v02.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx02, v02.Fog));
    v22.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx22, v22.Fog));

    const Vector3d fog00 = GetFogColorForMapPoint(fogIdx00);
    const Vector3d fog20 = GetFogColorForMapPoint(fogIdx20);
    const Vector3d fog02 = GetFogColorForMapPoint(fogIdx02);
    const Vector3d fog22 = GetFogColorForMapPoint(fogIdx22);

    const float alpha00 = CalcTerrainAlpha(VertexDistanceSq(v00.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha20 = CalcTerrainAlpha(VertexDistanceSq(v20.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha02 = CalcTerrainAlpha(VertexDistanceSq(v02.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha22 = CalcTerrainAlpha(VertexDistanceSq(v22.v), fadeStart, fadeStartSq, fadeEnd);

    const int direction = (FMap[y][x] >> 8) & 3;

    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
        AppendTerrainTriangle(m_terrainVertices, v00, v20, v22, fog00, fog20, fog22, textureLayer, false, false, direction, alpha00, alpha20, alpha22);
        AppendTerrainTriangle(m_terrainVertices, v00, v22, v02, fog00, fog22, fog02, textureLayer, false, true, direction, alpha00, alpha22, alpha02);
    }

    // Primary cell only — neighbor cells are covered by adjacent
    // 2×2 tiles (same ring) or by the inner 1×1 loop (inner rings).
    // Calling all four would duplicate entries in m_objectList.
    RenderObject(x, y);
}

void GLRenderer::RenderTerrain()
{
    if (m_terrainVertices.empty()) {
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
    DrawVertexBatch(m_terrainVertices);

    glBindVertexArray(0);
}

void GLRenderer::ResetTerrainTextureCache()
{
    m_uploadedTerrainTextures.fill(nullptr);
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

void GLRenderer::RenderGround()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderGround");
#endif
    BeginTerrainFrame();
    m_worldModelItems.clear();
    m_transparentModelItems.clear();
    m_objectList.clear();

    // If water is needed this frame, begin water collection here so we
    // can collect terrain + water vertices in a single ring walk instead
    // of two separate passes over the same ~1,800 tiles.
    if (NeedWater) {
        BeginWaterFrame();
    }

    // Precompute water distance/fade constants once for the unified walk.
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

    // Terrain and water share the same LOD boundary (ctViewR1). Water is
    // flat, so 2x2 tiles are geometrically identical to 1x1 — this is
    // always a pure win. Terrain 2x2 trades a slight blur for fewer
    // vertices, controlled by the TerrainLOD percentage slider.
    for (int r = ctViewR; r > ctViewR1; --r) {
        for (int x = -r; x <= r; ++x) {
            if (ctViewR1 < ctViewR) {
                CollectTerrainTile2(CCX + x, CCY + r, r);
                CollectTerrainTile2(CCX + x, CCY - r, r);
            }
            if (NeedWater) {
                CollectWaterTile2(CCX + x, CCY + r, r);
                CollectWaterTile2(CCX + x, CCY - r, r);
            }
        }
        for (int y = -r + 1; y < r; ++y) {
            if (ctViewR1 < ctViewR) {
                CollectTerrainTile2(CCX + r, CCY + y, r);
                CollectTerrainTile2(CCX - r, CCY + y, r);
            }
            if (NeedWater) {
                CollectWaterTile2(CCX + r, CCY + y, r);
                CollectWaterTile2(CCX - r, CCY + y, r);
            }
        }
    }

    for (int r = ctViewR1; r > 0; --r) {
        for (int x = -r; x <= r; ++x) {
            CollectTerrainTile(CCX + x, CCY + r, r);
            CollectTerrainTile(CCX + x, CCY - r, r);
            if (NeedWater) {
                CollectWaterTileFast(CCX + x, CCY + r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
                CollectWaterTileFast(CCX + x, CCY - r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }
        }
        for (int y = -r + 1; y < r; ++y) {
            CollectTerrainTile(CCX + r, CCY + y, r);
            CollectTerrainTile(CCX - r, CCY + y, r);
            if (NeedWater) {
                CollectWaterTileFast(CCX + r, CCY + y, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
                CollectWaterTileFast(CCX - r, CCY + y, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }
        }
    }

    CollectTerrainTile(CCX, CCY, 0);
    if (NeedWater) {
        CollectWaterTileFast(CCX, CCY, 0, wViewDistSq,
                             wFadeStart, wFadeStartSq,
                             wFadeEnd, wFadeEndSq);
    }

    RenderTerrain();
}
#endif // _gl
