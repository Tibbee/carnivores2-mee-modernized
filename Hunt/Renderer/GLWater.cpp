// ==========================================================================
// GLWater.cpp � Water surface rendering
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"
#include "Renderer/GLUtils.h"

#ifdef _gl

#include "glad/glad.h"
#include <cmath>

void GLRenderer::SetWaterAlphaFade(float enabled, float fadeStart, float fadeEnd, float fadeStep)
{
    if (!m_perFrameUBO) {
        return;
    }

    const float data[4] = {
        fadeStart,
        fadeEnd,
        enabled,
        fadeStep
    };

    glBindBuffer(GL_UNIFORM_BUFFER, m_perFrameUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 176, sizeof(data), data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLRenderer::BeginWaterFrame()
{
    m_waterVertexCount = 0;
    // §5.4: Ensure worst-case capacity for the current view distance.
    const size_t maxTiles = static_cast<size_t>(2 * ctViewR + 1) * static_cast<size_t>(2 * ctViewR + 1);
    EnsureWaterVertexCapacity(maxTiles * 6);
    m_waterUsedLayers.fill(false);
}

void GLRenderer::RenderWaterSurface()
{
    if (m_waterVertexCount == 0) {
        return;
    }

    EnsureTerrainTextureArray();

    // m_waterUsedLayers is maintained incrementally by AppendWaterTriangle
    // (called from CollectWaterTileFast / CollectWaterTile / CollectWaterTile2),
    // so we no longer need to scan m_waterVertices to discover which layers
    // are in use. The Textures[layer] check mirrors the previous behavior:
    // a vertex referencing a null texture pointer should be skipped.
    for (int layer = 0; layer < kMaxTerrainTextureLayers; ++layer) {
        if (!m_waterUsedLayers[layer] || !Textures[layer]) {
            continue;
        }
        if (m_uploadedTerrainTextures[layer] != Textures[layer].get()) {
            UploadTerrainLayer(layer, *Textures[layer]);
            m_uploadedTerrainTextures[layer] = Textures[layer].get();
        }
    }

    const auto projection = BuildLegacyProjection();
    // Bake water alpha fade into the UBO update — saves a separate
    // glBufferSubData call vs. UpdatePerFrameUBO() + SetWaterAlphaFade().
    UpdatePerFrameUBO(projection, 1.0f,
                      static_cast<float>((ctViewR - 8) << 8),
                      256.0f * static_cast<float>(ctViewR - 4),
                      765.0f);
    m_terrainShader.Use();
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // Per-pixel distance fog: uFogRange is the (fadeStart, distance) pair,
    // now sourced from the PerFrame UBO. The shader interpolates between
    // the per-vertex volumetric fog color and the global horizon color
    // (uDistanceFogColor, also in the UBO), keeping local volumes from
    // tinting the far horizon.

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);
    glBindVertexArray(m_terrainVAO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    DrawVertexBatch(m_waterVertices.get(), m_waterVertexCount);
    SetWaterAlphaFade(0.0f, static_cast<float>((ctViewR - 8) << 8), 256.0f * static_cast<float>(ctViewR - 4), 765.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}

bool GLRenderer::IsWaterTriangleValid(const EPoint& v0, const EPoint& v1, const EPoint& v2, float backR)
{
    // Only cull if ALL vertices are beyond the far plane.
    // OpenGL's hardware clipper handles near-plane and screen-edge
    // clipping, so we must not CPU-cull based on DFlags (near plane
    // or screen bounds) or individual-vertex far-plane checks.
    if (v0.v.z > backR && v1.v.z > backR && v2.v.z > backR) {
        return false;
    }

    return true;
}

float GLRenderer::CalcWaterAlpha(const EPoint& vertex, float centerDistanceSq, float fadeStart, float fadeStartSq, float fadeEnd)
{
    float alpha = Clamp01(vertex.ALPHA / 255.0f);

    if (!IsUnderwater() && centerDistanceSq > fadeStartSq) {
        const float distanceSq = VertexDistanceSq(vertex.v);
        if (distanceSq > fadeStartSq) {
            const float zz = std::sqrt(distanceSq) - fadeEnd;
            if (zz > 0.0f) {
                alpha = Clamp01((255.0f - zz / 3.0f) / 255.0f);
            }
        }
    }

    return alpha;
}

void GLRenderer::AppendWaterTriangle(const EPoint& v0,
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
                                     float alpha2,
                                     float fadeEnabled)
{
    const auto uv = GetTerrainUVs(reverse, second, direction);
    const float layer = static_cast<float>(textureLayer);

    // Mark the water texture layer as used this frame so RenderWaterSurface
    // can skip its O(m_waterVertices) layer scan. All three Collect paths
    // (Fast, Tile, Tile2) go through this function, so the mark is
    // centralized here. The bounds check is defensive — callers already
    // validate, but a stray out-of-range layer would corrupt the array.
    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers) {
        m_waterUsedLayers[textureLayer] = true;
    }

    // Phase 1.5: same uint8 packing as AppendTerrainTriangle. v0.Fog
    // is in 0..200 from CalcFogLevel (clamped to FLimit, typically
    // 200) and the old shader divided it by 255, so the uint8 packing
    // matches byte-for-byte.
    // §5.4: write directly to the flat array instead of push_back.
    TerrainVertex* dst = m_waterVertices.get() + m_waterVertexCount;
    dst[0] = {v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer,
              Light255ToByte(static_cast<float>(v0.Light)),
              Light255ToByte(v0.Fog),
              Float01ToByte(alpha0),
              Float01ToByte(fadeEnabled),
              Float01ToByte(fogColor0.x),
              Float01ToByte(fogColor0.y),
              Float01ToByte(fogColor0.z),
              0};
    dst[1] = {v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer,
              Light255ToByte(static_cast<float>(v1.Light)),
              Light255ToByte(v1.Fog),
              Float01ToByte(alpha1),
              Float01ToByte(fadeEnabled),
              Float01ToByte(fogColor1.x),
              Float01ToByte(fogColor1.y),
              Float01ToByte(fogColor1.z),
              0};
    dst[2] = {v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer,
              Light255ToByte(static_cast<float>(v2.Light)),
              Light255ToByte(v2.Fog),
              Float01ToByte(alpha2),
              Float01ToByte(fadeEnabled),
              Float01ToByte(fogColor2.x),
              Float01ToByte(fogColor2.y),
              Float01ToByte(fogColor2.z),
              0};
    m_waterVertexCount += 3;
}

void GLRenderer::CollectWaterTileFast(int x, int y, int r,
                                      float viewDistanceSq,
                                      float fadeStart, float fadeStartSq,
                                      float fadeEnd, float fadeEndSq)
{
    (void)r;
    (void)fadeStart;
    (void)fadeEnd;
    (void)fadeEndSq;

    if (x >= ctMapSize - 1 || y >= ctMapSize - 1 || x < 0 || y < 0) {
        return;
    }

    if (!((FMap[y][x] & fmWaterA) && (FMap[y][x + 1] & fmWaterA) &&
          (FMap[y + 1][x] & fmWaterA) && (FMap[y + 1][x + 1] & fmWaterA))) {
        return;
    }

    const int textureLayer = WaterList[WMap[y][x]].tindex;
    if (textureLayer < 0 || textureLayer >= kMaxTerrainTextureLayers || !Textures[textureLayer]) {
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 1 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        return;
    }

    // Coarse frustum pre-test — same as CollectTerrainTile, but using
    // HMapO as a rough height proxy for the water surface (water level
    // is typically close to terrain height).  Skips 4 VMap2 reads for
    // tiles outside the horizontal frustum.
    // Uses a generous margin to avoid false rejects.
    // SAFEGUARD: only reject when cz < 0 (see terrain for rationale).
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (cz < 0.0f && std::fabs(cx * FOVK) > -cz + BackViewR * 2.0f + 2048.0f) {
            return;
        }
    }

    EPoint v00 = VMap2[localY][localX];
    EPoint v10 = VMap2[localY][localX + 1];
    EPoint v01 = VMap2[localY + 1][localX];
    EPoint v11 = VMap2[localY + 1][localX + 1];

    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float centerDistanceSq = xx * xx + yy * yy + zz * zz;
    if (centerDistanceSq > viewDistanceSq) {
        return;
    }

    const float fadeEnabled = (!m_isUnderwater && centerDistanceSq > fadeStartSq) ? 1.0f : 0.0f;

    float a00 = Clamp01(v00.ALPHA / 255.0f);
    float a10 = Clamp01(v10.ALPHA / 255.0f);
    float a01 = Clamp01(v01.ALPHA / 255.0f);
    float a11 = Clamp01(v11.ALPHA / 255.0f);

    // §5.1: Early-out if all water vertex alphas are zero — skip the
    // fog lookup and triangle validation for fully transparent water.
    if (a00 <= 0.0f && a10 <= 0.0f && a01 <= 0.0f && a11 <= 0.0f) {
        return;
    }

    // Fresnel water-surface alpha from below: when underwater, the water
    // surface is more transparent directly overhead (where you can see
    // the sky) and more opaque near the horizon (where light is
    // internally reflected).  Uses Schlick's Fresnel approximation.
    // For a flat water surface, the Fresnel effect depends on the camera's
    // pitch angle (how much you're looking up/down), not per-tile position.
    if (m_isUnderwater) {
        // Calculate the camera's view direction relative to water surface normal
        // Water surface normal is (0, 1, 0) in world space
        // Camera forward direction in world space:
        //   x = sin(CameraAlpha) * cos(CameraBeta)
        //   y = -sin(CameraBeta)  (negative because positive beta = looking down)
        //   z = cos(CameraAlpha) * cos(CameraBeta)
        //
        // ndotv = dot(viewDir, surfaceNormal) = -sin(CameraBeta)
        // When looking straight up (CameraBeta = -PI/2): ndotv = 1.0 (transparent)
        // When looking horizontal (CameraBeta = 0): ndotv = 0.0 (opaque)
        // When looking down (CameraBeta = PI/2): ndotv = -1.0 (behind surface)

        // Clamp ndotv to [0, 1]:
        // - When looking up: ndotv > 0, Fresnel applies normally
        // - When looking horizontal: ndotv = 0, Fresnel = 1.0 (opaque)
        // - When looking down: ndotv < 0, clamps to 0, stays opaque
        // This creates a smooth transition with no discontinuity
        float ndotv = (std::max)(0.0f, -std::sin(CameraBeta));

        // Schlick Fresnel: ndotv=1 (zenith) -> fresnel≈0 (transparent)
        //                  ndotv=0 (horizon/down) -> fresnel≈1 (opaque)
        float fresnel = 0.03f + 0.97f * std::pow(1.0f - ndotv, 4.0f);

        // Blend base alpha toward 1.0 based on Fresnel
        a00 = Clamp01(a00 + (1.0f - a00) * fresnel);
        a10 = Clamp01(a10 + (1.0f - a10) * fresnel);
        a01 = Clamp01(a01 + (1.0f - a01) * fresnel);
        a11 = Clamp01(a11 + (1.0f - a11) * fresnel);
    }

    // Single FogsMap lookup for the tile center (water is flat; per-corner
    // fog precision is invisible — saves 3 FogsMap lookups per tile).
    const Vector3d fogTile = GetFogColorForMapPoint(x, y);

    if (a00 > 0.0f || a10 > 0.0f || a11 > 0.0f) {
        if (IsWaterTriangleValid(v00, v10, v11, BackViewR)) {
            AppendWaterTriangle(v00, v10, v11,
                               fogTile, fogTile, fogTile,
                               textureLayer, false, false, 0, a00, a10, a11, fadeEnabled);
        }
    }

    if (a00 > 0.0f || a11 > 0.0f || a01 > 0.0f) {
        if (IsWaterTriangleValid(v00, v11, v01, BackViewR)) {
            AppendWaterTriangle(v00, v11, v01,
                               fogTile, fogTile, fogTile,
                               textureLayer, false, true, 0, a00, a11, a01, fadeEnabled);
        }
    }
}

void GLRenderer::CollectWaterTile(int x, int y, int r)
{
    (void)r;

    if (x >= ctMapSize - 1 || y >= ctMapSize - 1 || x < 0 || y < 0) {
        return;
    }

    if (!((FMap[y][x] & fmWaterA) && (FMap[y][x + 1] & fmWaterA) &&
          (FMap[y + 1][x] & fmWaterA) && (FMap[y + 1][x + 1] & fmWaterA))) {
        return;
    }

    const int textureLayer = WaterList[WMap[y][x]].tindex;
    if (textureLayer < 0 || textureLayer >= kMaxTerrainTextureLayers || !Textures[textureLayer]) {
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 1 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        return;
    }

    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    EPoint v00 = VMap2[localY][localX];
    EPoint v10 = VMap2[localY][localX + 1];
    EPoint v01 = VMap2[localY + 1][localX];
    EPoint v11 = VMap2[localY + 1][localX + 1];

    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float centerDistanceSq = xx * xx + yy * yy + zz * zz;
    if (centerDistanceSq > viewDistanceSq) {
        return;
    }

    const float fadeEnabled = (!m_isUnderwater && centerDistanceSq > fadeStartSq) ? 1.0f : 0.0f;

    const float a00 = Clamp01(v00.ALPHA / 255.0f);
    const float a10 = Clamp01(v10.ALPHA / 255.0f);
    const float a01 = Clamp01(v01.ALPHA / 255.0f);
    const float a11 = Clamp01(v11.ALPHA / 255.0f);

    // Per-corner map-based fog color (mirrors the terrain path in
    // CollectTerrainTile). GetFogColorForMapPoint looks up the active
    // fog volume for each map cell, so water straddling a fog boundary
    // gets a per-vertex fog color that smoothly interpolates across the
    // surface. The previous version used GetCurrentFogColor() for all
    // three vertices, which lost the per-volume color and produced
    // a uniform tint across the whole water body.
    const Vector3d fog00 = GetFogColorForMapPoint(x, y);
    const Vector3d fog10 = GetFogColorForMapPoint(x + 1, y);
    const Vector3d fog01 = GetFogColorForMapPoint(x, y + 1);
    const Vector3d fog11 = GetFogColorForMapPoint(x + 1, y + 1);

    if (a00 > 0.0f || a10 > 0.0f || a11 > 0.0f) {
        if (IsWaterTriangleValid(v00, v10, v11, BackViewR)) {
            AppendWaterTriangle(v00, v10, v11, fog00, fog10, fog11, textureLayer, false, false, 0, a00, a10, a11, fadeEnabled);
        }
    }

    if (a00 > 0.0f || a11 > 0.0f || a01 > 0.0f) {
        if (IsWaterTriangleValid(v00, v11, v01, BackViewR)) {
            AppendWaterTriangle(v00, v11, v01, fog00, fog11, fog01, textureLayer, false, true, 0, a00, a11, a01, fadeEnabled);
        }
    }
}

void GLRenderer::CollectWaterTile2(int x, int y, int r)
{
    (void)r;

    if (x >= ctMapSize - 2 || y >= ctMapSize - 2 || x < 0 || y < 0) {
        return;
    }

    if (!((FMap[y][x] & fmWaterA) && (FMap[y][x + 2] & fmWaterA) &&
          (FMap[y + 2][x] & fmWaterA) && (FMap[y + 2][x + 2] & fmWaterA))) {
        return;
    }

    const int textureLayer = WaterList[WMap[y][x]].tindex;
    if (textureLayer < 0 || textureLayer >= kMaxTerrainTextureLayers || !Textures[textureLayer]) {
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
        const float wy = WaterList[WMap[y][x]].wlevel * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (std::fabs(cx * FOVK) > -cz + BackViewR * 2.0f + 2048.0f) {
            return;
        }
    }

    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    EPoint v00 = VMap2[localY][localX];
    EPoint v20 = VMap2[localY][localX + 2];
    EPoint v02 = VMap2[localY + 2][localX];
    EPoint v22 = VMap2[localY + 2][localX + 2];

    const float xx = (v00.v.x + v22.v.x) * 0.5f;
    const float yy = (v00.v.y + v22.v.y) * 0.5f;
    const float zz = (v00.v.z + v22.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float centerDistanceSq = xx * xx + yy * yy + zz * zz;
    if (centerDistanceSq > viewDistanceSq) {
        return;
    }

    const float fadeEnabled = (!m_isUnderwater && centerDistanceSq > fadeStartSq) ? 1.0f : 0.0f;

    const float a00 = Clamp01(v00.ALPHA / 255.0f);
    const float a20 = Clamp01(v20.ALPHA / 255.0f);
    const float a02 = Clamp01(v02.ALPHA / 255.0f);
    const float a22 = Clamp01(v22.ALPHA / 255.0f);

    // Per-corner map-based fog color (far-detail water path; mirrors
    // the near-detail CollectWaterTile and the terrain path in
    // CollectTerrainTile).
    const Vector3d fog00 = GetFogColorForMapPoint(x, y);
    const Vector3d fog20 = GetFogColorForMapPoint(x + 2, y);
    const Vector3d fog02 = GetFogColorForMapPoint(x, y + 2);
    const Vector3d fog22 = GetFogColorForMapPoint(x + 2, y + 2);

    if (a00 > 0.0f || a20 > 0.0f || a22 > 0.0f) {
        if (IsWaterTriangleValid(v00, v20, v22, BackViewR)) {
            AppendWaterTriangle(v00, v20, v22, fog00, fog20, fog22, textureLayer, false, false, 0, a00, a20, a22, fadeEnabled);
        }
    }

    if (a00 > 0.0f || a22 > 0.0f || a02 > 0.0f) {
        if (IsWaterTriangleValid(v00, v22, v02, BackViewR)) {
            AppendWaterTriangle(v00, v22, v02, fog00, fog22, fog02, textureLayer, false, true, 0, a00, a22, a02, fadeEnabled);
        }
    }
}

void GLRenderer::RenderWater()
{
    if (!NeedWater) {
        return;
    }

    // Water vertices were already collected during the unified ring walk
    // in RenderGround().  Only the GL draw call remains here — it must
    // stay after the model/shadow passes for correct alpha blending order.
    RenderWaterSurface();
}

void GLRenderer::RenderWCircles()
{
    // Water circles are wave ripples spawned by wading dinosaurs, the player,
    // and projectile impacts. They are full 3D morphed models (WCircleModel),
    // not 2D circles — so we use the model pipeline with CreateMorphedModel.
    // The D3D/3DFX legacy renderers call RenderWCircles() from inside their
    // own RenderWater() and use additive blending. We follow the same pattern
    // by routing through the IRenderer hook and drawing with additive=true.
    // See Hunt/RendererD3D.cpp:3288 and Hunt/Render3DFX.cpp:2184 for the
    // reference implementations.

    if (WCCount <= 0) {
        return;
    }

    Vector3d rpos;
    for (int c = 0; c < WCCount; c++) {
        TWCircle* wptr = &WCircles[c];
        rpos.x = wptr->pos.x - CameraX;
        rpos.y = wptr->pos.y - CameraY;
        rpos.z = wptr->pos.z - CameraZ;

        // Distance cull against ctViewR (matches D3D/3DFX ring-based cull).
        const float r = static_cast<float>(MAX(fabs(rpos.x), fabs(rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.4f);
        if (ri < 0) ri = 0;
        if (ri > ctViewR) continue;

        rpos = RotateVector(rpos);

        // Frustum cull against BackViewR (matches D3D/3DFX cone test).
        if (rpos.z > BackViewR) continue;
        if (fabs(rpos.x) > -rpos.z + BackViewR) continue;
        if (fabs(rpos.y) > -rpos.z + BackViewR) continue;

        // Alpha fades from ~52 down to 0 as FTime advances from 0 to 2000.
        // D3D: GlassL = 255 - (2000 - FTime) / 38   => baseAlpha = (255 - GlassL) / 255
        // i.e. the alpha is exactly the same expression the shader reads from GlassL.
        GlassL = 255 - (2000 - wptr->FTime) / 38;

        CreateMorphedModel(WCircleModel.mptr.get(), &WCircleModel.Animation[0],
                           static_cast<int>(wptr->FTime), wptr->scale);

        // Build the draw item directly with additive=true. We can't go through
        // RenderModelClip / RenderModelClipWater because the public IRenderer
        // overrides don't expose the additive flag (other renderers don't need it).
        ModelDrawItem item;
        const bool closeEnough = fabs(rpos.z) + fabs(rpos.x) < 1000.0f;
        if (!BuildModelDrawItem(item, WCircleModel.mptr.get(),
                                rpos.x, rpos.y, rpos.z, 250, 0, 0, CameraBeta,
                                false, false, closeEnough, /*additive=*/true)) {
            continue;
        }
        item.texture = UploadModelTexture(WCircleModel.mptr.get());
        if (!item.texture) {
            continue;
        }
        m_worldModelItems.push_back(std::move(item));
    }

    GlassL = 0;  // reset for subsequent pass

    // Drain the water-circle items we just queued with additive blending.
    // m_worldModelItems should only contain water circles at this point:
    // RenderGround() clears it at the start of the frame, and RenderModelsList()
    // (which calls RenderWorldModels()) already drained it before we got here.
    RenderWorldModels();
}
#endif // _gl
