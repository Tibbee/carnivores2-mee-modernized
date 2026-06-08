// ==========================================================================
// GLRenderer.h — OpenGL 3.3 Core Profile renderer for Carnivores 2 ME
//
// Ported from Carnivores 1's GLRenderer, adapted to C2 ME data structures.
// Requires: OpenGL 3.3+, GLAD loader, GLShader module.
//
// Note: This header must be included AFTER Hunt.h.
// ==========================================================================

#ifndef GLRENDERER_H
#define GLRENDERER_H

#pragma once

#include "IRenderer.h"
#include <windows.h>

class GLRenderer : public IRenderer {
public:
    GLRenderer();
    ~GLRenderer() override;

    // ── Lifecycle ──────────────────────────────────────────────────────
    bool Initialize() override;
    void Shutdown() override;

    // ── GL Context (called from Init3DHardware) ────────────────────────
    bool CreateContext();
    void DestroyContext();

    // ── Scene ──────────────────────────────────────────────────────────
    void DrawScene() override;
    void DrawPostObjects() override;

    // ── Resource Management ────────────────────────────────────────────
    void RegisterTexture(TEXTURE* tptr) override;
    void RegisterPicture(TPicture* pptr) override;
    void ReleaseModelTextures(const TModel* mptr) override;
    void ResetTerrainTextureCache() override;
    void ClearLevelTextureCache() override;

    // ── Frame Management ───────────────────────────────────────────────
    void ClearVideoBuf() override;
    void WaitRetrace() override;
    void PostProcess() override;

    // ── 3D Rendering — Terrain ─────────────────────────────────────────
    void DrawTPlane(bool clip) override;
    void DrawTPlaneClip(bool clip) override;
    void DrawHMap() override;

    // ── 3D Rendering — Models ──────────────────────────────────────────
    void RenderModel(TModel* mptr, float x0, float y0, float z0,
                     int light, float al, float bt) override;
    void RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                         int light, float al, float bt) override;
    void RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                              int light, float al, float bt) override;
    void RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                         int light, float al, float bt) override;

    // ── Character / Entity Renders ─────────────────────────────────────
    void RenderCharacter(TCharacter* cptr) override;
    void RenderExplosion(int index) override;
    void RenderShip() override;
    void RenderPlayer(int index) override;
    void RenderSkyPlane() override;

    // ── 2D Rendering ───────────────────────────────────────────────────
    void DrawPicture(int x, int y, TPicture& pic) override;
    void DrawScaledPicture(int x, int y, int w, int h, TPicture& pic) override;
    void DrawTrophyText(int x, int y) override;
    void RenderHealthBar() override;
    void Render_Cross(int x, int y) override;
    void Render_LifeInfo(int index) override;

    // ── System / State ─────────────────────────────────────────────────
    void SetVideoMode(int w, int h) override;
    void SetFullScreen() override;
    bool IsSoftwareStyle() const override;

    // ── Test functions (temporary - public for activation timing) ───────
    bool InitTestTexture();

private:
    // Internal helpers
    bool InitGLState();
    void LoadGLExtensions();
    bool InitTestTriangle();
    void RenderTestTriangle();
    void ShutdownTestTriangle();

    // GL context handles
    HWND m_hwnd = nullptr;
    HDC  m_hdc  = nullptr;
    HGLRC m_hrc = nullptr;
    bool m_Initialized = false;

    // Test triangle resources
    unsigned int m_TestShader = 0;
    unsigned int m_TestVAO = 0;
    unsigned int m_TestVBO = 0;
    bool m_TestTriangleReady = false;

    // Test texture resources
    unsigned int m_TestTexture = 0;
    bool m_TestTextureReady = false;

    // Texture management
    static const int kMaxGLTextures = 4096;
    unsigned int m_TextureCache[kMaxGLTextures] = {};
    bool m_TextureUsed[kMaxGLTextures] = {};

    // Helper: expand 16-bit X1R5G5B5 to 32-bit RGBA
    static unsigned int Expand1555to8888(unsigned short c);
};

// Global GL renderer instance (created in Init3DHardware, destroyed in ShutDown3DHardware)
extern GLRenderer* g_GLRenderer;

#endif // GLRENDERER_H
