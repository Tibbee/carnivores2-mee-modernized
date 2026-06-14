// ==========================================================================
// GLRenderer.h — OpenGL 3.3 Core Profile renderer for Carnivores 2 ME
// ==========================================================================

#ifndef GLRENDERER_H
#define GLRENDERER_H

#pragma once

#include "IRenderer.h"
#include "glad/glad.h"
#include <windows.h>
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
    struct TerrainVertex {
        float x, y, z;
        float u, v;
        float layer;
        float light;
        float fog;
        float fogR, fogG, fogB;
        float alpha;
    };

    struct ModelVertex {
        float x, y, z;
        float u, v;
        float light;
        float fog;
        float fogR, fogG, fogB;
        float alpha;
        float cutout;
    };

    struct ModelDrawItem {
        GLuint texture;
        float distance;
        bool additive;  // true => draw with GL_BLEND_FUNC(SRC_ALPHA, ONE), no depth write
        std::vector<ModelVertex> opaqueVertices;
        std::vector<ModelVertex> cutoutVertices;
        std::vector<ModelVertex> transparentVertices;
    };

    bool InitGLState();
    void LoadGLExtensions();
    bool InitializeTerrainPipeline();
    void ShutdownTerrainPipeline();
    bool InitializeModelPipeline();
    void ShutdownModelPipeline();
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
    unsigned int m_whiteTexture = 0;  // 1x1 white texture for flat-color rendering
    unsigned int m_phongTexture = 0;
    unsigned int m_envTexture = 0;
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
    std::vector<TerrainVertex> m_terrainVertices;
    std::vector<TerrainVertex> m_waterVertices;
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
