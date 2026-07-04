// ==========================================================================
// SoftRenderer.h — Software renderer implementing IRenderer interface
//
// Thin wrapper around the existing free-function software renderer in
// RenderSoft.cpp. Each IRenderer method delegates to the corresponding
// free function, allowing the game loop to dispatch through a common
// interface (the same pattern used by the OpenGL renderer).
// ==========================================================================

#ifndef SOFTRENDERER_H
#define SOFTRENDERER_H

#pragma once

#include "Renderer/IRenderer.h"

class SoftRenderer : public IRenderer {
public:
    SoftRenderer() = default;
    ~SoftRenderer() override;

    // ── Lifecycle ──────────────────────────────────────────────────────
    bool Initialize() override;
    void Shutdown() override;

    // ── Scene ──────────────────────────────────────────────────────────
    void DrawFrame(const RenderFrameContext& ctx) override;
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
                     int light, int vt, float al, float bt) override;
    void RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                         int light, int vt, float al, float bt) override;
    void RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                              int light, int vt, float al, float bt) override;
    void RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                         int light, int vt, float al, float bt) override;

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

    bool IsSoftwareStyle() const override { return true; }
};

extern SoftRenderer* g_SoftRenderer;

#endif // SOFTRENDERER_H
