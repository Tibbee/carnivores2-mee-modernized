// ==========================================================================
// GLRenderer.h — OpenGL 3.3 Core Profile renderer for Carnivores 2 ME
// ==========================================================================

#ifndef GLRENDERER_H
#define GLRENDERER_H

#pragma once

#include "IRenderer.h"
#include "glad/glad.h"
#include "GLPerf.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <vector>

class GLRenderer : public IRenderer {
public:
    GLRenderer();
    ~GLRenderer() override;

    bool Initialize() override;
    void Shutdown() override;

    bool CreateContext();
    void DestroyContext();

    void DrawScene() override;
    void DrawPostObjects() override;

    void RegisterTexture(TEXTURE* tptr) override;
    void RegisterPicture(TPicture* pptr) override;
    void ReleaseModelTextures(const TModel* mptr) override;
    void ResetTerrainTextureCache() override;
    void ClearLevelTextureCache() override;

    void ClearVideoBuf() override;
    void WaitRetrace() override;
    void PostProcess() override;

    void DrawTPlane(bool clip) override;
    void DrawTPlaneClip(bool clip) override;
    void DrawHMap() override;

    void RenderModel(TModel* mptr, float x0, float y0, float z0,
                     int light, int vt, float al, float bt) override;
    void RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                         int light, int vt, float al, float bt) override;
    void RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                              int light, int vt, float al, float bt) override;
    void RenderModelClipPhongMap(TModel* mptr, float x0, float y0, float z0,
                                 float al, float bt);
    void RenderModelClipEnvMap(TModel* mptr, float x0, float y0, float z0,
                               float al, float bt);
    void RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                         int light, int vt, float al, float bt) override;

    void RenderCharacter(TCharacter* cptr) override;
    void RenderExplosion(int index) override;
    void RenderShip() override;
    void RenderPlayer(int index) override;
    void RenderSkyPlane() override;
    void RenderWCircles() override;

    void DrawPicture(int x, int y, TPicture& pic) override;
    void DrawScaledPicture(int x, int y, int w, int h, TPicture& pic) override;
    void DrawTrophyText(int x, int y) override;
    void RenderHealthBar() override;
    void Render_Cross(int x, int y) override;
    void Render_LifeInfo(int index) override;

    void SetVideoMode(int w, int h) override;
    void SetFullScreen() override;
    bool IsSoftwareStyle() const override;

    void RenderGround();
    void RenderProjectedShadows();
    void RenderWater();
    void RenderObject(int x, int y);
    void RenderMappedObject(int x, int y);
    void RenderModelsList();
    void RenderBMPModel(TBMPModel* mptr, float x0, float y0, float z0, int light);
    void Render3DHardwarePosts();
    void RenderCircle(float cx, float cy, float z, float R, uint32_t RGBA, uint32_t RGBA2);
    void RenderElements();

private:
    // Packed terrain vertex (Phase 1.5). 32 bytes total (was 48).
    //   offset  0: vec3  aPos                    (12 bytes)  -- attribute 0, float
    //   offset 12: vec2  aTexCoord                ( 8 bytes)  -- attribute 1, float
    //   offset 20: float aLayer                   ( 4 bytes)  -- attribute 2, float
    //   offset 24: vec4  aLightFogAlpha           ( 4 bytes)  -- attribute 3, uint8 normalized
    //                 .x = light, .y = fog, .z = alpha, .w = pad
    //   offset 28: vec3  aFogColor                ( 3 bytes)  -- attribute 4, uint8 normalized
    //   offset 31: 1 byte explicit padding to round the vertex up to 32 bytes
    struct TerrainVertex {
        float    x, y, z;          // 12
        float    u, v;             //  8
        float    layer;            //  4
        uint8_t  light;            //  1 (vec4.x)
        uint8_t  fog;              //  1 (vec4.y)
        uint8_t  alpha;            //  1 (vec4.z)
        uint8_t  _pad1;            //  1 (vec4.w)
        uint8_t  fogR;             //  1 (vec3.x)
        uint8_t  fogG;             //  1 (vec3.y)
        uint8_t  fogB;             //  1 (vec3.z)
        uint8_t  _pad2;            //  1  (pad to 32)
    };
    static_assert(sizeof(TerrainVertex) == 32, "TerrainVertex must stay 32 bytes (Phase 1.5)");

    // Packed model vertex (Phase 1.4). 32 bytes total (was 48).
    //   offset  0: vec3  aPos                    (12 bytes)  -- attribute 0, float
    //   offset 12: vec2  aTexCoord                ( 8 bytes)  -- attribute 1, float
    //   offset 20: vec4  aLightFogAlphaCutout     ( 4 bytes)  -- attribute 2, uint8 normalized
    //                 .x = light, .y = fog, .z = alpha, .w = cutout
    //   offset 24: vec3  aFogColor                ( 3 bytes)  -- attribute 3, uint8 normalized
    //   offset 27: 5 bytes explicit padding to round the vertex up to 32 bytes
    struct ModelVertex {
        float    x, y, z;          // 12
        float    u, v;             //  8
        uint8_t  light;            //  1 (vec4.x)
        uint8_t  fog;              //  1 (vec4.y)
        uint8_t  alpha;            //  1 (vec4.z)
        uint8_t  cutout;           //  1 (vec4.w)
        uint8_t  fogR;             //  1 (vec3.x)
        uint8_t  fogG;             //  1 (vec3.y)
        uint8_t  fogB;             //  1 (vec3.z)
        uint8_t  _pad[5];          //  5  (pad to 32)
    };
    static_assert(sizeof(ModelVertex) == 32, "ModelVertex must stay 32 bytes (Phase 1.4)");

    // Phase 1.4 conversion helpers: float [0,1] / float [0,255] -> uint8.
    // Drivers normalize the uint8 attribute back to [0,1] in the vertex
    // shader, so the shader sees the same values as the old float layout.
    static inline uint8_t Float01ToByte(float v) {
        return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    }
    static inline uint8_t Light255ToByte(float v) {
        return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f) + 0.5f);
    }
    static inline uint8_t CutoutToByte(bool on) {
        return on ? 255 : 0;  // normalized to 1.0 or 0.0; shader checks vCutout > 0.5
    }

    struct ModelDrawItem {
        GLuint texture;
        float distance;
        bool additive;  // true => draw with GL_BLEND_FUNC(SRC_ALPHA, ONE), no depth write
        std::vector<ModelVertex> opaqueVertices;
        std::vector<ModelVertex> cutoutVertices;
        std::vector<ModelVertex> transparentVertices;
    };

    // Phase 2.1: per-instance data layout for geometry instancing.
    //   attribute 4: vec4 aWorldRow0    (offset  0)  -- instance world transform, row 0
    //   attribute 5: vec4 aWorldRow1    (offset 16)  -- instance world transform, row 1
    //   attribute 6: vec4 aWorldRow2    (offset 32)  -- instance world transform, row 2
    //   attribute 7: vec4 aWorldRow3    (offset 48)  -- instance world transform, row 3
    //   attribute 8: vec4 aInstanceLight (offset 64) -- .x = base; .yzw = reserved
    //   attribute 9: vec4 aInstanceFlags (offset 80) -- .x = bitfield; .yzw = reserved
    // Total: 96 bytes per instance, 16-byte aligned.
    //
    // The 4x4 world form (16 floats) is one Mat4 + one Vec4 + one Vec4
    // and reads in the vertex shader as:
    //   mat4 iWorld = mat4(aWorldRow0, aWorldRow1, aWorldRow2, aWorldRow3);
    //   gl_Position = uProjection * iWorld * vec4(aPos, 1.0);
    //
    // The compact 4x3 form (12 floats: worldPos + 3 basis vec4s) was an
    // option in the design doc but adds shader complexity (the world
    // transform is no longer a single mat4 construction) for a 16 B/instance
    // saving. At kInitialInstanceCapacity (4,096) instances per frame, the
    // saving is 64 KB — not worth the readability cost.
    //
    // This struct is the data that the GPU vertex shader (Phase 2.4) will
    // use to transform model-space vertices to world space without CPU
    // work. It is allocated in 2.1 but not yet consumed — the new VAO/VBO
    // are created empty; the per-frame upload and the instanced draw calls
    // land in Phase 2.3.
    struct ModelInstance {
        float worldRow0[4];     // 16
        float worldRow1[4];     // 16
        float worldRow2[4];     // 16
        float worldRow3[4];     // 16
        float instanceLight[4]; // 16 -- .x = base; .yzw = reserved for future
        float instanceFlags[4]; // 16 -- .x = bitfield (cutout, transparent, additive, shadow); .yzw = reserved
    };
    static_assert(sizeof(ModelInstance) == 96, "ModelInstance must stay 96 bytes (Phase 2.1)");

    bool InitGLState();
    void LoadGLExtensions();
    bool InitializeTerrainPipeline();
    void ShutdownTerrainPipeline();
    bool InitializeModelPipeline();
    void ShutdownModelPipeline();
    bool InitializeInstancingPipeline();
    void ShutdownInstancingPipeline();
    void UpdatePerFrameUBO();
    void UpdatePerFrameUBO(const std::array<float, 16>& projection);
    void EnsurePerFrameUBO();
    void BeginTerrainFrame();
    void BeginWaterFrame();
    void RenderTerrain();
    void RenderWaterSurface();
    void RenderWorldModels();
    void RenderProjectedCharacterShadow(const TCharacter& character, float alpha);
    void DrawVertexBatch(const std::vector<TerrainVertex>& vertices) const;
    GLuint UploadModelTexture(TModel* mptr);
    GLuint UploadBMPModelTexture(TBMPModel* mptr);
    GLuint UploadPictureTexture(const TPicture& pic);
    bool BuildModelEffectVertices(std::vector<ModelVertex>& outVertices,
                                  TModel* mptr,
                                  float x0,
                                  float y0,
                                  float z0,
                                  float al,
                                  float bt,
                                  int flagMask,
                                  const Vector3d& fogColor) const;
    bool BuildModelDrawItem(ModelDrawItem& outItem,
                            TModel* mptr,
                            float x0,
                            float y0,
                            float z0,
                            int light,
                            int vt,
                            float al,
                            float bt,
                            bool waterClipped,
                            bool disableFog,
                            bool clippedVariant,
                            bool additive) const;
    void DrawModelVertices(GLuint texture,
                           const std::vector<ModelVertex>& vertices,
                           const std::array<float, 16>& projection,
                           bool depthTest,
                           bool enableBlend,
                           bool additive,
                           bool tintByFogColor = false);
    bool NeedsNearestModelFiltering(const std::vector<ModelVertex>& vertices) const;
    void SetModelTextureFiltering(GLuint texture, bool nearest);
    void EnsureTerrainTextureArray();
    void UploadTerrainLayer(int layer, const TEXTURE& texture);
    void CollectTerrainTile(int x, int y, int r);
    void CollectTerrainTile2(int x, int y, int r);
    void CollectWaterTile(int x, int y, int r);
    void CollectWaterTile2(int x, int y, int r);
    void AppendTerrainTriangle(std::vector<TerrainVertex>& vertices,
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
                               float alpha0 = 1.0f,
                               float alpha1 = 1.0f,
                               float alpha2 = 1.0f);
    void AppendWaterTriangle(std::vector<TerrainVertex>& vertices,
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
                             float alpha2);
    static std::array<Vector2df, 3> GetTerrainUVs(bool reverse, bool second, int direction);
    static std::array<float, 16> BuildLegacyProjection();
    static Vector3d GetCurrentFogColor();
    static Vector3d GetFogColorForMapPoint(int mapX, int mapY);
    static Vector3d DecodeFogColor(int rgb);
    static bool IsWaterTriangleValid(const EPoint& v0, const EPoint& v1, const EPoint& v2, float backR);
    static float CalcWaterAlpha(const EPoint& vertex, float centerDistanceSq, float fadeStart, float fadeStartSq, float fadeEnd);
    static float Clamp01(float value);
    static unsigned int Expand1555to8888(unsigned short c);

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hrc = nullptr;
    bool m_Initialized = false;

    unsigned int m_terrainShader = 0;
    unsigned int m_terrainVAO = 0;
    unsigned int m_terrainVBO = 0;
    unsigned int m_terrainTextureArray = 0;

    unsigned int m_modelShader = 0;
    unsigned int m_modelVAO = 0;
    unsigned int m_modelVBO = 0;

    // Phase 2.1: per-instance data plumbing for geometry instancing.
    // These GL objects are allocated at InitializeInstancingPipeline() but
    // not yet used by the renderer; the per-frame upload and instanced
    // draw calls land in Phase 2.3. Kept as siblings of m_modelVAO /
    // m_modelVBO so the instance pipeline lifetime is paired with the
    // model pipeline lifetime.
    unsigned int m_instanceVBO = 0;
    unsigned int m_instanceVAO = 0;

    // Per-frame instance array. Filled in 2.3; declared here so the
    // vector's backing storage lives for the whole program (avoid
    // per-frame heap churn once 2.3 starts using it).
    std::vector<ModelInstance> m_instanceData;
    // Phase 1.11: last-bound model texture, used to skip redundant
    // glBindTexture calls when consecutive buckets share a texture
    // (which is the common case after Phase 1.7's bucket sort).
    GLuint m_lastBoundModelTexture = 0;
    unsigned int m_whiteTexture = 0;  // 1x1 white texture for flat-color rendering
    unsigned int m_phongTexture = 0;
    unsigned int m_envTexture = 0;

    // Cached uniform locations (Phase 1.6, 1.9): set once at Initialize();
    // eliminates per-draw glGetUniformLocation lookups.
    int m_locModelTexture = -1;        // model shader: uModelTexture sampler
    int m_locModelTint = -1;           // model shader: uTintByFogColor
    int m_locSkyTexture = -1;          // sky shader: uSkyTexture
    int m_locSkyViewport = -1;         // sky shader: uViewport
    int m_locSkyVideoCenter = -1;      // sky shader: uVideoCenter
    int m_locSkyQ = -1;                // sky shader: uQ
    int m_locSkyP = -1;                // sky shader: uP
    int m_locSkyR = -1;                // sky shader: uR
    int m_locSkyTime = -1;             // sky shader: uSkyTime
    int m_locSkyFogBase = -1;          // sky shader: uFogBase

    // PerFrame UBO (Phase 1.1): binding 0, shared by terrain and model shaders.
    // std140 layout: mat4 uProjection + vec2 uFogRange + vec3 uDistanceFogColor
    // + float uForceFog + vec3 uFogColor = 108 bytes, padded to 112.
    // The cached fields are repacked into the UBO on every UpdatePerFrameUBO()
    // call. The projection is recomputed each call because near-model draws
    // (wind indicator, compass, weapon viewmodels) change VideoCX/VideoCY/
    // CameraW/CameraH between the main scene and the HUD overlay, so a
    // per-frame cache would feed a stale matrix to the near-model path.
    unsigned int m_perFrameUBO = 0;
    bool m_perFrameUBOInitialized = false;
    std::array<float, 16> m_cachedProjection{};
    float m_cachedFogStart = 0.0f;
    float m_cachedFogDistance = 0.0f;
    float m_cachedForceFog = 0.0f;
    float m_cachedDistanceFogColor[3] = {0.0f, 0.0f, 0.0f};
    float m_cachedFogColor[3] = {0.0f, 0.0f, 0.0f};
    bool m_hasLastNearModelProjection = false;
    std::array<float, 16> m_lastNearModelProjection{};
    std::map<const TModel*, GLuint> m_modelTextureCache;
    std::map<const TBMPModel*, GLuint> m_bmpTextureCache;
    std::map<GLuint, bool> m_modelTextureFilterState;
    std::vector<ModelDrawItem> m_worldModelItems;
    std::vector<const ModelDrawItem*> m_transparentModelItems;
    std::vector<Vector2di> m_objectList;

    static const int kTerrainMipLevels = 4;
    static const int kMaxTerrainTextureLayers = 1024;
    // Phase 2.1: initial capacity for the per-frame instance array.
    // The dense custom map Phase 0 baseline measured ~2,400 visible model
    // objects per frame; reserve 4,096 to absorb the high end with
    // headroom. The vector grows automatically if a frame exceeds this.
    static const int kInitialInstanceCapacity = 4096;
    std::vector<TerrainVertex> m_terrainVertices;
    std::vector<TerrainVertex> m_waterVertices;
    // Non-owning cache: tracks which terrain textures are currently
    // uploaded to the GPU. Raw pointer (not unique_obj_ptr) because
    // ownership stays with the global Textures[] array. A previous
    // attempt to use unique_obj_ptr here caused release() to steal
    // ownership from Textures[layer], leaving it null on the next
    // frame and crashing the terrain renderer.
    std::array<TEXTURE*, kMaxTerrainTextureLayers> m_uploadedTerrainTextures{};

    // Sky pipeline
    unsigned int m_skyShader = 0;
    unsigned int m_skyVAO = 0;
    unsigned int m_skyTexture = 0;
    bool m_skyTextureDirty = true;

    // Smoothed sky fog color (temporal filter to absorb day/night sky
    // color changes without frame-to-frame popping). The target color is
    // the global distance-fog color, not the current fixed fog volume.
    Vector3d m_smoothedSkyFogColor = {0.0f, 0.0f, 0.0f};
    bool m_smoothedSkyFogColorInit = false;

    void InitializeSkyPipeline();
    void ShutdownSkyPipeline();
    void UploadSkyTexture();

    // Sun rendering
    float m_sunLight = 0.0f;
    float m_skyTraceK = 1.0f;
    float m_traceK = 1.0f;
    int m_sunScrX = 0;
    int m_sunScrY = 0;
    int m_lastSunVisibilityScrX = 0;
    int m_lastSunVisibilityScrY = 0;
    unsigned int m_lastSunVisibilityUpdate = 0;
    int m_lastSunTraceScrX = -1;
    int m_lastSunTraceScrY = -1;
    unsigned int m_lastSunTraceFrame = 0;
    float m_lastSunTraceK = 1.0f;
    std::vector<ModelVertex> m_sunModelVertices;

    void RenderSun(float x, float y, float z);
    void RenderModelSun(TModel* mptr, float x0, float y0, float z0, int alpha);
    float GetSkyK(int x, int y);
    float GetTraceK(int x, int y);
    void UpdateSunVisibility();

    // HUD/UI overlay pipeline — uploads lpVideoBuf as a fullscreen quad on top of the 3D scene
    GLuint m_uiShader = 0;
    GLuint m_uiVAO = 0;
    GLuint m_uiVBO = 0;
    GLuint m_uiTexture = 0;
    int m_uiTextureWidth = 0;
    int m_uiTextureHeight = 0;
    std::vector<uint32_t> m_uiPixels;
    void InitializeHudPipeline();
    void ShutdownHudPipeline();
    void EnsureUITexture();
    void UpdateUIPixels();

public:
    float GetSunLight() const { return m_sunLight; }
    void RenderFSRect(uint32_t color);
    void ApplySunDepthOcclusion();
    void DrawHUDOverlay();
};

extern GLRenderer* g_GLRenderer;

#endif // GLRENDERER_H
