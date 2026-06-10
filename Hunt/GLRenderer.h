// ==========================================================================
// GLRenderer.h — OpenGL 3.3 Core Profile renderer for Carnivores 2 ME
// ==========================================================================

#ifndef GLRENDERER_H
#define GLRENDERER_H

#pragma once

#include "IRenderer.h"
#include <windows.h>
#include <array>
#include <cstdint>
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
                     int light, float al, float bt) override;
    void RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                         int light, float al, float bt) override;
    void RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                              int light, float al, float bt) override;
    void RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                         int light, float al, float bt) override;

    void RenderCharacter(TCharacter* cptr) override;
    void RenderExplosion(int index) override;
    void RenderShip() override;
    void RenderPlayer(int index) override;
    void RenderSkyPlane() override;

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
    void RenderWater();

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

    bool InitGLState();
    void LoadGLExtensions();
    bool InitializeTerrainPipeline();
    void ShutdownTerrainPipeline();
    void BeginTerrainFrame();
    void RenderTerrain();
    void DrawVertexBatch(const std::vector<TerrainVertex>& vertices) const;
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
                             int textureLayer,
                             float alpha0,
                             float alpha1,
                             float alpha2);
    static std::array<Vector2df, 3> GetTerrainUVs(bool reverse, bool second, int direction);
    static std::array<float, 16> BuildLegacyProjection();
    static Vector3d GetCurrentFogColor();
    static Vector3d GetFogColorForMapPoint(int mapX, int mapY);
    static Vector3d DecodeFogColor(int rgb);
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

    static const int kTerrainMipLevels = 4;
    static const int kMaxTerrainTextureLayers = 1024;
    std::vector<TerrainVertex> m_terrainVertices;
    std::vector<TerrainVertex> m_waterVertices;
    std::array<TEXTURE*, kMaxTerrainTextureLayers> m_uploadedTerrainTextures{};
};

extern GLRenderer* g_GLRenderer;

#endif // GLRENDERER_H
