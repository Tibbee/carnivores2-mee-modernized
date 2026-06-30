// ==========================================================================
// GLSky.cpp � Sky plane, sun, and atmospheric rendering
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"
#include "Renderer/GLUtils.h"

#ifdef _gl

#include "glad/glad.h"
#include <cmath>

void GLRenderer::RenderSkyPlane()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderSkyPlane");
#endif
    if (!m_skyVAO || !m_skyTexture || !m_skyShader) {
        return;
    }

    UploadSkyTexture();

    const float localCa = std::cos(CameraAlpha);
    const float localSa = std::sin(CameraAlpha);
    const float pitchCos = std::cos(CameraBeta);
    const float pitchSin = std::sin(CameraBeta);

    SKYDTime = RealTime & ((1 << 16) - 1);

    const float skyPitchCos = std::cos(CameraBeta - 0.15f);
    const float skyPitchSin = std::sin(CameraBeta - 0.15f);

    Vector3d tx = {0.004f, 0.0f, 0.0f};
    Vector3d ty = {0.0f, 0.0f, 0.004f};
    Vector3d nv = {0.0f, -1.0f, 0.0f};

    auto rotateSky = [&](Vector3d& v) {
        // First rotate around Y axis (CameraAlpha)
        float x = v.x * localCa - v.z * localSa;
        float z = v.z * localCa + v.x * localSa;
        // Then rotate around X axis (CameraBeta - 0.15)
        float y = v.y * skyPitchCos + z * skyPitchSin;
        float zz = z * skyPitchCos - v.y * skyPitchSin;

        v.x = x;
        v.y = y;
        v.z = zz;
    };

    rotateSky(tx);
    rotateSky(ty);
    rotateSky(nv);

    Vector3d vbase = {-CameraX, 4.0f * 512.0f * 16.0f, CameraZ};
    rotateSky(vbase);

    const float p = nv.x * vbase.x + nv.y * vbase.y + nv.z * vbase.z;
    const float ddx = vbase.x * tx.x + vbase.y * tx.y + vbase.z * tx.z;
    const float ddy = vbase.x * ty.x + vbase.y * ty.y + vbase.z * ty.z;

    const float qx = CameraH * nv.x;
    const float qy = CameraW * nv.y;
    const float qz = CameraW * CameraH * nv.z;

    float px = p * CameraH * tx.x;
    float py = p * CameraW * tx.y;
    float pz = p * CameraW * CameraH * tx.z;
    float rx = p * CameraH * ty.x;
    float ry = p * CameraW * ty.y;
    float rz = p * CameraW * CameraH * ty.z;

    px -= ddx * qx;
    py -= ddx * qy;
    pz -= ddx * qz;
    rx -= ddy * qx;
    ry -= ddy * qy;
    rz -= ddy * qz;

    // The sky's distance-fog color is global, not the color of the
    // fixed fog volume the camera is currently inside. Local volumes are
    // still applied to terrain/models by their per-vertex fog color.
    const Vector3d targetSkyFogColor = GetDistanceFogColor();

    // Temporal low-pass filter on the sky color so day/night sky changes
    // settle smoothly instead of popping between frames.
    if (!m_smoothedSkyFogColorInit) {
        m_smoothedSkyFogColor = targetSkyFogColor;
        m_smoothedSkyFogColorInit = true;
    } else {
        constexpr float k = 0.15f;
        m_smoothedSkyFogColor.x += (targetSkyFogColor.x - m_smoothedSkyFogColor.x) * k;
        m_smoothedSkyFogColor.y += (targetSkyFogColor.y - m_smoothedSkyFogColor.y) * k;
        m_smoothedSkyFogColor.z += (targetSkyFogColor.z - m_smoothedSkyFogColor.z) * k;
    }

    UpdatePerFrameUBO();
    glUseProgram(m_skyShader);
    glUniform1i(m_locSkyTexture, 0);
    glUniform2f(m_locSkyViewport, static_cast<float>(WinW), static_cast<float>(WinH));
    glUniform2f(m_locSkyVideoCenter, static_cast<float>(VideoCX), static_cast<float>(VideoCY));
    // uFogColor now sourced from PerFrame UBO (Phase 1.1). uForceFog is
    // still bound in the UBO (kept for layout compatibility) but no longer
    // used by this shader; the sky's underwater look comes from the
    // 3dfx fog formula plus the uUnderwaterDepth uniform, plus scissor
    // clipping below the water surface horizon.
    glUniform3f(m_locSkyQ, qx, qy, qz);
    glUniform3f(m_locSkyP, px, py, pz);
    glUniform3f(m_locSkyR, rx, ry, rz);
    glUniform1f(m_locSkyTime, static_cast<float>(SKYDTime) / 256.0f);

    // Sample CalcFogLevel directly above the camera (X=0, Z=0 in
    // camera-relative space) at sky height to get the base fog amount
    // for the per-pixel sky gradient. Using (0, ...) instead of
    // (512, ...) keeps the probe in the same map cell as the camera,
    // so the resulting fog amount matches the volume the camera is in
    // (when CAMERAINFOG) and doesn't jump as the camera crosses cell
    // boundaries along the X axis.
    const Vector3d fogProbe = {0.0f, 4.0f * 512.0f * 16.0f, 0.0f};
    const float fogBase = CalcFogLevel(fogProbe);
    glUniform1f(m_locSkyFogBase, fogBase);

    // uUnderwaterDepth: 0 above water, ramps to 1 at ~1024 world units
    // below the surface.  The shader uses this to add up to 30% extra
    // fog on top of the 3dfx sky formula, giving a depth-based dimming
    // effect that matches the per-vertex fog on terrain and models.
    float underwaterDepth = 0.0f;
    if (IsUnderwater()) {
        const float waterLevel = GetLandUpH(CameraX, CameraZ);
        // (std::max) parenthesised to defeat the Windows max macro.
        const float depth = (std::max)(0.0f, waterLevel - CameraY);
        underwaterDepth = std::clamp(depth / 1024.0f, 0.0f, 1.0f);
    }
    glUniform1f(m_locSkyUnderwaterDepth, underwaterDepth);

    // uWaterLineY: screen Y (from top) of the water surface horizon.
    // Used by the shader to fade the sky to full fog near the water
    // line for a seamless blend with the distance-fog colour below.
    // Set to WinH (no fade) when not underwater or looking up.
    float waterLineY = static_cast<float>(WinH);

    // When the camera is underwater, clip the sky to only render above
    // the water-surface horizon in screen space.  Below that line the
    // distance-fog colour (already set as glClearColor) fills the
    // background and terrain renders on top.  Matches the 3DFX renderer
    // which only draws the sky plane down to scry (Render3DFX.cpp:4841).
    //
    // Three cases for the screen-space water horizon (scry, Y from top):
    //   scry >= WinH  → water line below screen → looking UP through
    //                   water surface → full sky (dimmed by shader)
    //   0 < scry < WinH → water line on screen → scissor to top scry px
    //   scry <= 0    → water line above screen → looking DOWN into
    //                   water → no sky at all
    bool underwaterFullSky = false;  // scry >= WinH: full sky, no scissor
    bool scissorEnabled = false;
    if (IsUnderwater()) {
        const float waterLevel = GetLandUpH(CameraX, CameraZ);
        // Camera-relative height of the water surface in world units
        // (positive when the camera is below the surface).
        const float sh = waterLevel - CameraY;
        const float locCb = std::cos(CameraBeta);
        const float locSb = std::sin(CameraBeta);
        // A point on the water surface at a representative distance
        // directly in front of the camera.
        const float vz = static_cast<float>(ctViewR * 4 / 5) * 256.0f;
        const float vy = sh;
        // Rotate by CameraBeta (pitch) to get view-space position.
        float viewY = vy * locCb + vz * locSb;
        float viewZ = vz * locCb - vy * locSb;
        if (viewZ < 128.0f) viewZ = 128.0f;
        // Project to screen space (Y from top, matching 3DFX scry).
        int scry = VideoCY - static_cast<int>((viewY / viewZ) * CameraH);

        if (scry >= WinH) {
            // Water horizon is below the screen: the entire view is above
            // the water surface (looking up through water at the sky).
            // Render the full sky with the shader's depth-based dimming.
            underwaterFullSky = true;
            waterLineY = static_cast<float>(WinH);  // no water line on screen
        } else if (scry > 0) {
            // Water horizon is on screen.  Clip the sky to the top
            // scry pixels so it only appears above the water line.
            // OpenGL window coords: Y=0 at bottom, so the region is
            // [WinH - scry, WinH].
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, WinH - scry, WinW, scry);
            scissorEnabled = true;
            waterLineY = static_cast<float>(scry);
        }
        // else scry <= 0: entire view is below water surface.
        // Sky is completely hidden — scissorEnabled stays false,
        // underwaterFullSky stays false, and the draw is skipped.
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_skyTexture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_skyTexture);
#endif

    const bool shouldDrawSky = !IsUnderwater() || underwaterFullSky || scissorEnabled;
    if (shouldDrawSky) {
        glUniform1f(m_locSkyWaterLineY, waterLineY);
        glBindVertexArray(m_skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
#ifdef GL_PERF_HOOKS
        GL_PERF_DRAW(1);
#endif
        glBindVertexArray(0);
    }

    glDepthMask(GL_TRUE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    // Render sun on top of sky (matching D3D/3DFX: sky plane renders sun).
    // The sun is only drawn when the sky is drawn.  The scissor (if any)
    // is still active, so the sun is also clipped to above the water line.
    if (SunModel && shouldDrawSky) {
        m_sunLight = 0.0f;
        Vector3d sunDir = {-2048.0f, 4048.0f, -2048.0f};
        sunDir = RotateVector(sunDir);
        if (sunDir.z < -2024.0f) {
            RenderSun(sunDir.x, sunDir.y, sunDir.z);
            // GetSkyK is called inside RenderSun for cloud occlusion.
            // GetTraceK (depth-based) is deferred to ShowVideo() after the
            // full scene is rendered, so the depth buffer has terrain/models.
        }
    }

    if (scissorEnabled) {
        glDisable(GL_SCISSOR_TEST);
    }
}

void GLRenderer::RenderFSRect(uint32_t color, bool additive)
{
    float a = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
    float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>(color & 0xFF) / 255.0f;

    // Reuse the persistent 1x1 white texture created for flat-color rendering.
    if (!m_whiteTexture) return;

    // Build a fullscreen quad using the packed ModelVertex layout
    // (Phase 1.4: 32 bytes, color attributes are uint8 normalized).
    // The pre-Phase-1.4 local FSVertex struct used float fields for
    // light/fog/fogR/G/B/alpha/cutout, which the GL driver reads as
    // raw bytes -- producing garbage colors. Must use the same packed
    // layout as ModelVertex for the VBO's attribute pointers to interpret
    // the data correctly.
    const uint8_t lightByte  = 255;
    const uint8_t fogByte    = 255;                 // 1.0 normalized
    const uint8_t alphaByte  = static_cast<uint8_t>(a * 255.0f + 0.5f);
    const uint8_t cutoutByte = 0;
    const uint8_t fogRByte   = static_cast<uint8_t>(r * 255.0f + 0.5f);
    const uint8_t fogGByte   = static_cast<uint8_t>(g * 255.0f + 0.5f);
    const uint8_t fogBByte   = static_cast<uint8_t>(b * 255.0f + 0.5f);
    const ModelVertex quad[6] = {
        {-1.0f, -1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        { 1.0f, -1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        { 1.0f,  1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        {-1.0f, -1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        { 1.0f,  1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        {-1.0f,  1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
    };

    // The fullscreen quad is already in NDC, so the UBO must carry the
    // identity projection. With the world projection here, the NDC
    // vertices get re-projected off-screen and the glare is invisible.
    const std::array<float, 16> identity = {
        1.0f,0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,0.0f, 0.0f,0.0f,1.0f,0.0f, 0.0f,0.0f,0.0f,1.0f
    };

    glDisable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDepthMask(GL_FALSE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // Use additive blending (glare) or standard alpha blending (dark overlay)
    if (additive)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    UpdatePerFrameUBO(identity);
    glUseProgram(m_modelShader);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // uProjection in PerFrame UBO (Phase 1.1)

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_whiteTexture);
#endif
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(2);
#endif
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void GLRenderer::ApplySunDepthOcclusion()
{
    // Called from ShowVideo() after the full scene is rendered.
    // Samples the depth buffer at the sun's screen position to check
    // if terrain/models are occluding the sun.
    // Depth-based occlusion sample is cached for the current frame so
    // ApplySunDepthOcclusion() can reuse the value computed by UpdateSunVisibility().
    float traceK = 0.0f;
    if (m_sunScrX == m_lastSunTraceScrX &&
        m_sunScrY == m_lastSunTraceScrY &&
        RealTime == m_lastSunTraceFrame) {
        traceK = m_lastSunTraceK;
    } else {
        traceK = GetTraceK(m_sunScrX, m_sunScrY);
        m_lastSunTraceK = traceK;
        m_lastSunTraceScrX = m_sunScrX;
        m_lastSunTraceScrY = m_sunScrY;
        m_lastSunTraceFrame = RealTime;
    }
    m_sunLight *= traceK;
}

void GLRenderer::UpdateSunVisibility()
{
    if (m_sunScrX < 10 || m_sunScrY < 10 || m_sunScrX > WinW - 10 || m_sunScrY > WinH - 10) {
        m_skyTraceK = 0.5f;
        return;
    }

    // Rate-limit to ~15 Hz (66ms) to avoid GPU stalls from glReadPixels
    if (m_sunScrX == m_lastSunVisibilityScrX &&
        m_sunScrY == m_lastSunVisibilityScrY &&
        RealTime - m_lastSunVisibilityUpdate < 66) {
        return;
    }

    m_lastSunVisibilityUpdate = RealTime;
    m_lastSunVisibilityScrX = m_sunScrX;
    m_lastSunVisibilityScrY = m_sunScrY;

    // Depth-based occlusion (GetTraceK): is terrain/models blocking the sun?
    float traceK = GetTraceK(m_sunScrX, m_sunScrY);
    m_lastSunTraceK = traceK;
    m_lastSunTraceScrX = m_sunScrX;
    m_lastSunTraceScrY = m_sunScrY;
    m_lastSunTraceFrame = RealTime;

    // Color-based cloud occlusion (GetSkyK): are clouds dimming the sky?
    float skyK = GetSkyK(m_sunScrX, m_sunScrY);

    // Final visibility is the product
    float visibility = traceK * skyK;

    // Smooth transition
    float delta = (0.07f + std::fabs(visibility - m_skyTraceK)) * (static_cast<float>(TimeDt) / 512.0f);
    if (visibility > m_skyTraceK) {
        m_skyTraceK = (std::min)(visibility, m_skyTraceK + delta);
    } else {
        m_skyTraceK = (std::max)(visibility, m_skyTraceK - delta);
    }
}

void GLRenderer::RenderModelSun(TModel* mptr, float x0, float y0, float z0, int alpha)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    if (!mptr || !mptr->lpTexture || !mptr->gVertex || !mptr->gFace) return;

    const GLuint texture = UploadModelTexture(mptr);
    if (!texture) return;

    m_sunModelVertices.clear();
    const size_t reserveCount = static_cast<size_t>(mptr->FCount) * 3;
    m_sunModelVertices.reserve(reserveCount);
    const float alphaVal = static_cast<float>(alpha) / 255.0f;

    for (int f = 0; f < mptr->FCount; ++f) {
        const TFace& face = mptr->gFace[f];
        const int texHeight = (mptr->TextureHeight > 1) ? mptr->TextureHeight : 1;

        auto makeVertex = [&](int vIdx, int tx, int ty) -> ModelVertex {
            const Vector2df uv = DecodeLegacyFaceUV(static_cast<float>(tx), static_cast<float>(ty), texHeight);
            return {
                mptr->gVertex[vIdx].x + x0,
                mptr->gVertex[vIdx].y + y0,
                mptr->gVertex[vIdx].z + z0,
                uv.x, uv.y,
                Light255ToByte(255.0f),  // full brightness
                Float01ToByte(0.0f),     // no fog
                Float01ToByte(alphaVal),  // per-frame alpha
                CutoutToByte(false),      // no cutout
                0, 0, 0,                  // fog color (unused)
                {0, 0, 0, 0, 0}
            };
        };

        m_sunModelVertices.push_back(makeVertex(face.v1, face.tax, face.tay));
        m_sunModelVertices.push_back(makeVertex(face.v2, face.tbx, face.tby));
        m_sunModelVertices.push_back(makeVertex(face.v3, face.tcx, face.tcy));
    }

    if (m_sunModelVertices.empty()) return;

    const auto projection = BuildLegacyProjection();
    UpdatePerFrameUBO();
    glUseProgram(m_modelShader);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // uProjection in PerFrame UBO (Phase 1.1)

    // The weapon's phong/env-map draw passes set uTintByFogColor=1.0 on this
    // shader. With vFogColor=(0,0,0) on the sun, that multiplies litColor to
    // black and the sun goes invisible under additive blending. Reset to 0
    // here so the sun renders normally.
    glUniform1f(m_locModelTint, 0.0f);

    glEnable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive blending for sun
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);  // don't write depth for sun
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(texture);
#endif
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(m_sunModelVertices.size() * sizeof(ModelVertex));
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, m_sunModelVertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_sunModelVertices.size()));
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(static_cast<uint32_t>(m_sunModelVertices.size()) / 3);
#endif

    glDepthMask(GL_TRUE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDisable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glBindVertexArray(0);
}

void GLRenderer::RenderSun(float x, float y, float z)
{
    m_sunScrX = VideoCX + static_cast<int>(x / (-z) * CameraW);
    m_sunScrY = VideoCY - static_cast<int>(y / (-z) * CameraH);
    GetSkyK(m_sunScrX, m_sunScrY);

    float d = std::sqrt(x * x + y * y);
    if (d < 2048.0f) {
        m_sunLight = 220.0f - d * 220.0f / 2048.0f;
        if (m_sunLight > 140.0f) m_sunLight = 140.0f;
        m_sunLight *= m_skyTraceK;
    }

    if (d > 812.0f) d = 812.0f;
    d = (2048.0f + d) / 3048.0f;
    d += (1.0f - m_skyTraceK) / 2.0f;
    if (OptDayNight == 2) d = 1.5f;

    // Underwater depth fade: the sun's corona should dim the deeper the
    // camera is below the water surface, matching the per-vertex fog
    // behaviour on terrain and models.  waterLevel is the height of the
    // water surface at the camera's XZ position; depthFactor is 0 at the
    // surface and ramps to 1 around 1024 world units below it.  The sun
    // keeps ~30% brightness at maximum fade so it remains a faint glow
    // when very deep, rather than vanishing entirely.
    float depthAtten = 1.0f;
    if (IsUnderwater()) {
        const float waterLevel = GetLandUpH(CameraX, CameraZ);
        // (std::max) parenthesised to defeat the Windows max macro.
        const float depth = (std::max)(0.0f, waterLevel - CameraY);
        const float depthFactor = std::clamp(depth / 1024.0f, 0.0f, 1.0f);
        depthAtten = 1.0f - depthFactor * 0.7f;
    }

    const int sunAlpha = static_cast<int>(200.0f * m_skyTraceK * depthAtten);
    RenderModelSun(SunModel.get(), x * d, y * d, z * d, sunAlpha);
}

float GLRenderer::GetSkyK(int x, int y)
{
    if (x < 10 || y < 10 || x > WinW - 10 || y > WinH - 10) return 0.5f;

    float skySumR = 0.0f, skySumG = 0.0f, skySumB = 0.0f;

    // Sample 9 points around the sun position on the color buffer
    const int offsets[][2] = {
        {0, 0}, {6, 0}, {-6, 0}, {0, 6}, {0, -6},
        {4, 4}, {4, -4}, {-4, 4}, {-4, -4}
    };
    for (const auto& off : offsets) {
        unsigned char pixel[4];
        glReadPixels(x + off[0], WinH - (y + off[1]), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        // GL returns BGR in byte order for glReadPixels
        skySumR += pixel[0];
        skySumG += pixel[1];
        skySumB += pixel[2];
    }

    // Subtract the expected sky color (target)
    skySumR -= SkyTR * 9.0f;
    skySumG -= SkyTG * 9.0f;
    skySumB -= SkyTB * 9.0f;

    float k = std::sqrt(skySumR * skySumR + skySumG * skySumG + skySumB * skySumB) / 9.0f;
    if (k > 80.0f) k = 80.0f;
    if (k < 0.0f) k = 0.0f;
    k = 1.0f - k / 80.0f;
    if (k < 0.2f) k = 0.2f;
    if (OptDayNight == 2) k = 0.12f + k / 5.0f;

    DeltaFunc(m_skyTraceK, k, (0.07f + std::fabs(k - m_skyTraceK)) * (TimeDt / 512.0f));
    return m_skyTraceK;
}

float GLRenderer::GetTraceK(int x, int y)
{
    if (x < 8 || y < 8 || x > WinW - 8 || y > WinH - 8) return 0.0f;

    float k = 0.0f;
    // Sample 9 points around the sun position on the depth buffer
    const int offsets[][2] = {
        {0, 0}, {10, 0}, {-10, 0}, {0, 10}, {0, -10},
        {8, 8}, {8, -8}, {-8, 8}, {-8, -8}
    };
    for (const auto& off : offsets) {
        float depth = 1.0f;
        glReadPixels(x + off[0], WinH - (y + off[1]), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        // Depth near 1.0 means sky (nothing occluding)
        if (depth > 0.9999f) k += 1.0f;
    }
    k /= 9.0f;

    DeltaFunc(m_traceK, k, TimeDt / 1024.0f);
    return m_traceK;
}

void GLRenderer::UploadSkyTexture()
{
    if (!m_skyTextureDirty || !m_skyTexture) {
        return;
    }
    m_skyTextureDirty = false;

    std::vector<uint32_t> expanded(256 * 256);
    for (int i = 0; i < 256 * 256; ++i) {
        expanded[i] = Expand1555to8888(SkyPic[i]);
    }

    glBindTexture(GL_TEXTURE_2D, m_skyTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, expanded.data());
}

void GLRenderer::ShutdownSkyPipeline()
{
    if (m_skyTexture) {
        glDeleteTextures(1, &m_skyTexture);
        m_skyTexture = 0;
    }
    if (m_skyVAO) {
        glDeleteVertexArrays(1, &m_skyVAO);
        m_skyVAO = 0;
    }
    if (m_skyShader) {
        glDeleteProgram(m_skyShader);
        m_skyShader = 0;
    }
}

void GLRenderer::InitializeSkyPipeline()
{
    const char* vsSource =
        "#version 330 core\n"
        "out vec2 vNdc;\n"
        "const vec2 kPositions[3] = vec2[3](\n"
        "   vec2(-1.0, -1.0),\n"
        "   vec2( 3.0, -1.0),\n"
        "   vec2(-1.0,  3.0)\n"
        ");\n"
        "void main() {\n"
        "   vec2 pos = kPositions[gl_VertexID];\n"
        "   vNdc = pos;\n"
        "   gl_Position = vec4(pos, 0.0, 1.0);\n"
        "}";

    const char* fsSource =
        "#version 330 core\n"
        "in vec2 vNdc;\n"
        "out vec4 FragColor;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "};\n"
        "uniform sampler2D uSkyTexture;\n"
        "uniform vec2 uViewport;\n"
        "uniform vec2 uVideoCenter;\n"
        "uniform vec3 uQ;\n"
        "uniform vec3 uP;\n"
        "uniform vec3 uR;\n"
        "uniform float uSkyTime;\n"
        "uniform float uFogBase;\n"
        "uniform float uUnderwaterDepth;\n"
        "uniform float uWaterLineY;\n"  // screen Y (from top) of water surface, WinH if no clip
        "void main() {\n"
        "   vec2 pixel = vec2((vNdc.x * 0.5 + 0.5) * uViewport.x,\n"
        "                     (1.0 - (vNdc.y * 0.5 + 0.5)) * uViewport.y);\n"
        "   float sx = pixel.x - uVideoCenter.x;\n"
        "   float sy = uVideoCenter.y - pixel.y;\n"
        "   float sxQ = uQ.x * sx + uQ.y * sy + uQ.z;\n"
        "   float q = sign(sxQ) * max(abs(sxQ), 0.001);\n"
        "   float skyU = (uP.x * sx + uP.y * sy + uP.z) / q;\n"
        "   float skyV = (uR.x * sx + uR.y * sy + uR.z) / q;\n"
        "   float leftQ = uQ.x * (-uVideoCenter.x) + uQ.y * sy + uQ.z;\n"
        "   float rightQ = uQ.x * uVideoCenter.x + uQ.y * sy + uQ.z;\n"
        "   float leftU = (uP.x * (-uVideoCenter.x) + uP.y * sy + uP.z) / max(abs(leftQ), 0.001);\n"
        "   float leftV = (uR.x * (-uVideoCenter.x) + uR.y * sy + uR.z) / max(abs(leftQ), 0.001);\n"
        "   float rightU = (uP.x * uVideoCenter.x + uP.y * sy + uP.z) / max(abs(rightQ), 0.001);\n"
        "   float rightV = (uR.x * uVideoCenter.x + uR.y * sy + uR.z) / max(abs(rightQ), 0.001);\n"
        "   float dx = rightU - leftU;\n"
        "   float dy = rightV - leftV;\n"
        "   float dt = sqrt(dx*dx + dy*dy) / 96.0 - 6.0;\n"
        "   dt = clamp(dt, 0.0, 10.0);\n"
        // 3dfx sky formula: identical above and below water.  The
        // underwater effect comes from uFogBase being high (capped at
        // FLimit) and from the uUnderwaterDepth uniform adding up to
        // 30% extra fog at maximum depth, so the sky fades out like
        // the per-vertex fog on terrain and models.  Matches
        // Render3DFX.cpp:2621.  Previously the shader did
        //   fogFactor = mix(fogFactor, 1.0, uForceFog)
        // which fully replaced the sky with the fog colour when
        // underwater, completely hiding the sky and sun.
        "   float fogFactor = clamp(max(dt * 225.0 / 10.0, uFogBase) / 255.0, 0.0, 1.0);\n"
        // Depth-based fade: the deeper the camera is below the water
        // surface, the more the sky is blended toward the fog colour.
        // uUnderwaterDepth is 0 at the surface and ramps to 1 at
        // ~1024 world units below; the 0.55 multiplier makes the sky
        // dim significantly faster than the per-vertex fog on terrain,
        // so the sky/sun disappear quickly as you dive.
        "   fogFactor = clamp(fogFactor + uUnderwaterDepth * 0.55, 0.0, 1.0);\n"
        // Fade to full fog near the water-surface horizon so the sky
        // blends seamlessly into the underwater distance-fog colour.
        // pixel.y is the screen-space Y from the top (see above).
        // uWaterLineY is WinH when there is no water line on screen.
        "   float distToWaterLine = uWaterLineY - pixel.y;\n"
        "   float fadeWidth = 32.0;\n"
        "   if (distToWaterLine < fadeWidth && uWaterLineY < uViewport.y) {\n"
        "       fogFactor = mix(1.0, fogFactor, clamp(distToWaterLine / fadeWidth, 0.0, 1.0));\n"
        "   }\n"
        "   vec2 uv = vec2((skyU + uSkyTime) / 256.0, (skyV - uSkyTime) / 256.0);\n"
        "   vec3 skyColor = texture(uSkyTexture, uv).rgb;\n"
        "   FragColor = vec4(mix(skyColor, uFogColor, fogFactor), 1.0);\n"
        "}";

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    m_skyShader = LinkProgram(vertexShader, fragmentShader);
    if (!m_skyShader) {
        PrintLog("GLRenderer: Sky shader compilation... FAILED!\n");
        return;
    }
    PrintLog("GLRenderer: Sky shader compilation... OK\n");

    glGenVertexArrays(1, &m_skyVAO);
    glGenTextures(1, &m_skyTexture);

    glBindTexture(GL_TEXTURE_2D, m_skyTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    m_skyTextureDirty = true;
}

#endif // _gl
