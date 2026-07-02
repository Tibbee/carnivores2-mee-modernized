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
    //
    // SAFEGUARD: only reject when cz < 0 (representative point in front
    // of the camera).  When looking up a steep hill the height term
    // wy*sb can cancel the forward term cz1*cb and drive cz >= 0 (at or
    // behind the camera origin).  In that case -cz + margin goes
    // non-positive and |cx*FOVK| > (non-positive) is ALWAYS true, which
    // would unconditionally cull the tile — the root cause of the
    // steep-hill side-clipping bug (the C1 GL renderer has no such
    // pre-test and never exhibited it).  Skipping the coarse reject
    // when cz >= 0 lets the precise 4-corner test below decide, while
    // preserving the optimization for the common in-front-of-camera case.
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

    // §5.1: Early-fade-out cull — skip fog + triangle emission for
    // tiles whose 4 vertex alphas are all below the visibility threshold.
    // These tiles are in the outer fade ring (ctViewR-4..ctViewR) and
    // are effectively invisible.  RenderObject is still called so that
    // objects on faded tiles are queued (they have their own distance fade).
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
void GLRenderer::CollectTerrainTilePair(int x1, int x2, int y, int r)
{
    (void)r;

    // Boundary checks — fall back to 1×1 for out-of-bounds tiles
    if (x1 < 0 || x2 < 0 || y < 0 ||
        x1 >= ctMapSize - 1 || x2 >= ctMapSize - 1 || y >= ctMapSize - 1) {
        if (x1 >= 0 && x1 < ctMapSize - 1 && y >= 0 && y < ctMapSize - 1)
            CollectTerrainTile(x1, y, r);
        if (x2 >= 0 && x2 < ctMapSize - 1 && y >= 0 && y < ctMapSize - 1)
            CollectTerrainTile(x2, y, r);
        return;
    }

    // Local coordinate checks
    const int localX1 = x1 - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX1 < 0 || localY < 0 || localX1 + 2 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        if (localX1 >= 0 && localX1 + 1 < kViewGridSize)
            CollectTerrainTile(x1, y, r);
        if (localX1 + 1 >= 0 && localX1 + 2 < kViewGridSize)
            CollectTerrainTile(x2, y, r);
        return;
    }

    // BackViewR with object bounds for both tiles
    float backR1 = BackViewR;
    if (OMap[y][x1] != 255) backR1 += MObjects[OMap[y][x1]].info.BoundR;
    float backR2 = BackViewR;
    if (OMap[y][x2] != 255) backR2 += MObjects[OMap[y][x2]].info.BoundR;
    const float backR = (std::max)(backR1, backR2);

    // Coarse frustum pre-test for the pair's center.
    // SAFEGUARD: only reject when cz < 0 (see CollectTerrainTile for the
    // full rationale).  Without the guard, ascending a steep hill while
    // looking up drives the representative cz >= 0 and makes the test
    // unconditionally true, false-culling the pair.  The precise
    // per-tile 4-corner test below handles the cz >= 0 case correctly.
    {
        const float wx = static_cast<float>((x1 + 1) * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x1]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (cz < 0.0f && std::fabs(cx * FOVK) > -cz + backR * 2.0f + 2048.0f) {
            return;  // Pair outside frustum — skip entirely (matches CollectTerrainTile)
        }
    }

    // Fetch 6 VMap corners (shared between 2 tiles)
    EPoint v00 = VMap[localY][localX1];
    EPoint v10 = VMap[localY][localX1 + 1];  // shared
    EPoint v20 = VMap[localY][localX1 + 2];
    EPoint v01 = VMap[localY + 1][localX1];
    EPoint v11 = VMap[localY + 1][localX1 + 1];  // shared
    EPoint v21 = VMap[localY + 1][localX1 + 2];

    // Early z-check for the pair — must check ALL 6 corners (not just top row)
    // to avoid false-culling when looking uphill/downhill.
    if (v00.v.z > backR && v10.v.z > backR && v20.v.z > backR &&
        v01.v.z > backR && v11.v.z > backR && v21.v.z > backR) {
        return;  // All corners behind back plane — skip
    }

    // Precompute all 6 fog indices (shared between tiles)
    const int fogIdx_x1_y   = GetFogIndexForMapPoint(x1, y);
    const int fogIdx_x2_y   = GetFogIndexForMapPoint(x2, y);      // = x1+1, shared
    const int fogIdx_x3_y   = GetFogIndexForMapPoint(x2 + 1, y);  // = x1+2
    const int fogIdx_x1_y1  = GetFogIndexForMapPoint(x1, y + 1);
    const int fogIdx_x2_y1  = GetFogIndexForMapPoint(x2, y + 1);  // = x1+1, shared
    const int fogIdx_x3_y1  = GetFogIndexForMapPoint(x2 + 1, y + 1);  // = x1+2

    // Precompute all 6 fog colors
    const Vector3d fogColor_x1_y  = GetFogColorForMapPoint(fogIdx_x1_y);
    const Vector3d fogColor_x2_y  = GetFogColorForMapPoint(fogIdx_x2_y);
    const Vector3d fogColor_x3_y  = GetFogColorForMapPoint(fogIdx_x3_y);
    const Vector3d fogColor_x1_y1 = GetFogColorForMapPoint(fogIdx_x1_y1);
    const Vector3d fogColor_x2_y1 = GetFogColorForMapPoint(fogIdx_x2_y1);
    const Vector3d fogColor_x3_y1 = GetFogColorForMapPoint(fogIdx_x3_y1);

    // Precompute all 6 alpha values
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    const float alpha00 = CalcTerrainAlpha(VertexDistanceSq(v00.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha10 = CalcTerrainAlpha(VertexDistanceSq(v10.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha20 = CalcTerrainAlpha(VertexDistanceSq(v20.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha01 = CalcTerrainAlpha(VertexDistanceSq(v01.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha11 = CalcTerrainAlpha(VertexDistanceSq(v11.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha21 = CalcTerrainAlpha(VertexDistanceSq(v21.v), fadeStart, fadeStartSq, fadeEnd);

    // Precompute fog amounts for ALL 6 corners unconditionally.
    // This ensures shared corners (v10, v11) have correct fog even if
    // Tile 1 is alpha-culled (reviewer fix: fog-on-shared-corners).
    v00.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x1_y, v00.Fog));
    v10.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x2_y, v10.Fog));
    v20.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x3_y, v20.Fog));
    v01.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x1_y1, v01.Fog));
    v11.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x2_y1, v11.Fog));
    v21.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx_x3_y1, v21.Fog));

    // Process tile 1: (x1, y)
    {
        const float xx = (v00.v.x + v11.v.x) * 0.5f;
        const float yy = (v00.v.y + v11.v.y) * 0.5f;
        const float zz = (v00.v.z + v11.v.z) * 0.5f;

        bool tile1Emitted = false;
        if (std::fabs(xx * FOVK) <= -zz + backR1) {
            const float viewDistance = static_cast<float>(ctViewR * 256);
            const float viewDistanceSq = viewDistance * viewDistance;
            const float distanceSq = xx * xx + yy * yy + zz * zz;
            if (distanceSq <= viewDistanceSq) {
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
                        tile1Emitted = true;
                    }
                }
            }
        }
        // Only call RenderObject if tile passed distance cull (matches CollectTerrainTile behavior)
        if (std::fabs(xx * FOVK) <= -zz + backR1) {
            const float viewDistance = static_cast<float>(ctViewR * 256);
            const float viewDistanceSq = viewDistance * viewDistance;
            const float distanceSq = xx * xx + yy * yy + zz * zz;
            if (distanceSq <= viewDistanceSq) {
                RenderObject(x1, y);
            }
        }
    }

    // Process tile 2: (x2, y) — reuses v10, v11 and their fog data
    {
        const float xx = (v10.v.x + v21.v.x) * 0.5f;
        const float yy = (v10.v.y + v21.v.y) * 0.5f;
        const float zz = (v10.v.z + v21.v.z) * 0.5f;

        if (std::fabs(xx * FOVK) <= -zz + backR2) {
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

    // Single full ring walk with 1x1 tiles. The earlier 2x2 far-LOD
    // system had two structural problems: a stride-1 walk caused
    // adjacent 2x2 tiles to overlap (z-fighting, texture twitching);
    // and the 2x2 tile's inner edge has only 2 vertices over a 2-cell
    // span while the 1x1 ring on the inside has 3, leaving a T-junction
    // that the GL rasterizer renders as a thin gap.  Dropping the 2x2
    // system eliminates both, at the cost of ~4x vertices in the far
    // ring — most of which are alpha-faded to invisible anyway by
    // CalcTerrainAlpha at ctViewR-4..ctViewR-8.  This matches the
    // post-b4a0f2b state of the three reference renderers
    // (Render3DFX/RenderSoft/RendererD3D).
    // §5.2: Chunked ring walk — process horizontal pairs on top/bottom
    // edges to share 3 VMap corners, 3 fog lookups, and 3 alpha
    // computations between 2 tiles (25% reduction in per-tile work).
    //
    // The ring walk iterates from outermost to innermost. For each ring,
    // the top/bottom edges have (2r+1) tiles and the left/right edges
    // have (2r-1) tiles. We process horizontal pairs where possible,
    // with individual tile fallback for the last tile on each edge.
    if (NeedWater) {
        for (int r = ctViewR; r > 0; --r) {
            // Top edge (y = CCY + r): process horizontal pairs
            const int topStartX = CCX - r;
            const int topEndX = CCX + r;
            for (int x = topStartX; x < topEndX; x += 2) {
                CollectTerrainTilePair(x, x + 1, CCY + r, r);
                CollectWaterTileFast(x, CCY + r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
                CollectWaterTileFast(x + 1, CCY + r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }
            // Last tile on top edge (always odd count)
            CollectTerrainTile(topEndX, CCY + r, r);
            CollectWaterTileFast(topEndX, CCY + r, r, wViewDistSq,
                                 wFadeStart, wFadeStartSq,
                                 wFadeEnd, wFadeEndSq);

            // Bottom edge (y = CCY - r): process horizontal pairs
            for (int x = topStartX; x < topEndX; x += 2) {
                CollectTerrainTilePair(x, x + 1, CCY - r, r);
                CollectWaterTileFast(x, CCY - r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
                CollectWaterTileFast(x + 1, CCY - r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }
            CollectTerrainTile(topEndX, CCY - r, r);
            CollectWaterTileFast(topEndX, CCY - r, r, wViewDistSq,
                                 wFadeStart, wFadeStartSq,
                                 wFadeEnd, wFadeEndSq);

            // Left edge (x = CCX - r): individual tiles (vertical pairs
            // would require a separate function; skip for now)
            for (int y = -r + 1; y < r; ++y) {
                CollectTerrainTile(CCX - r, CCY + y, r);
                CollectWaterTileFast(CCX - r, CCY + y, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }

            // Right edge (x = CCX + r): individual tiles
            for (int y = -r + 1; y < r; ++y) {
                CollectTerrainTile(CCX + r, CCY + y, r);
                CollectWaterTileFast(CCX + r, CCY + y, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }
        }
        CollectTerrainTile(CCX, CCY, 0);
        CollectWaterTileFast(CCX, CCY, 0, wViewDistSq,
                             wFadeStart, wFadeStartSq,
                             wFadeEnd, wFadeEndSq);
    } else {
        for (int r = ctViewR; r > 0; --r) {
            // Top edge (y = CCY + r): process horizontal pairs
            const int topStartX = CCX - r;
            const int topEndX = CCX + r;
            for (int x = topStartX; x < topEndX; x += 2) {
                CollectTerrainTilePair(x, x + 1, CCY + r, r);
            }
            CollectTerrainTile(topEndX, CCY + r, r);

            // Bottom edge (y = CCY - r): process horizontal pairs
            for (int x = topStartX; x < topEndX; x += 2) {
                CollectTerrainTilePair(x, x + 1, CCY - r, r);
            }
            CollectTerrainTile(topEndX, CCY - r, r);

            // Left edge (x = CCX - r): individual tiles
            for (int y = -r + 1; y < r; ++y) {
                CollectTerrainTile(CCX - r, CCY + y, r);
            }

            // Right edge (x = CCX + r): individual tiles
            for (int y = -r + 1; y < r; ++y) {
                CollectTerrainTile(CCX + r, CCY + y, r);
            }
        }
        CollectTerrainTile(CCX, CCY, 0);
    }

    RenderTerrain();
}
#endif // _gl
