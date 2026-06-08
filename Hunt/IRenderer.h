// ==========================================================================
// IRenderer.h — Abstract renderer interface for Carnivores 2 ME
//
// All renderers (Software, D3D, 3DFX, OpenGL) implement this interface.
// The game loop calls only IRenderer methods; the concrete implementation
// is selected at startup via RENDERER define or command-line override.
//
// Note: This header must be included AFTER Hunt.h which defines the
// forward-declared types (TModel, TEXTURE, TPicture, TCharacter, etc.).
// ==========================================================================

#ifndef IRENDERER_H
#define IRENDERER_H

#pragma once

// Types are defined in Hunt.h — include that first.

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // ── Scene ──────────────────────────────────────────────────────────
    virtual void DrawScene() = 0;
    virtual void DrawPostObjects() = 0;

    // ── Resource Management ────────────────────────────────────────────
    virtual void RegisterTexture(TEXTURE* tptr) = 0;
    virtual void RegisterPicture(TPicture* pptr) = 0;
    virtual void ReleaseModelTextures(const TModel* mptr) = 0;
    virtual void ResetTerrainTextureCache() = 0;
    virtual void ClearLevelTextureCache() = 0;

    // ── Frame Management ───────────────────────────────────────────────
    virtual void ClearVideoBuf() = 0;
    virtual void WaitRetrace() = 0;
    virtual void PostProcess() = 0;

    // ── 3D Rendering — Terrain ─────────────────────────────────────────
    virtual void DrawTPlane(bool clip) = 0;
    virtual void DrawTPlaneClip(bool clip) = 0;
    virtual void DrawHMap() = 0;

    // ── 3D Rendering — Models ──────────────────────────────────────────
    virtual void RenderModel(TModel* mptr, float x0, float y0, float z0,
                             int light, float al, float bt) = 0;
    virtual void RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                                 int light, float al, float bt) = 0;
    virtual void RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                                      int light, float al, float bt) = 0;
    virtual void RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                                 int light, float al, float bt) = 0;

    // ── Character / Entity Renders ─────────────────────────────────────
    virtual void RenderCharacter(TCharacter* cptr) = 0;
    virtual void RenderExplosion(int index) = 0;
    virtual void RenderShip() = 0;
    virtual void RenderPlayer(int index) = 0;
    virtual void RenderSkyPlane() = 0;

    // ── 2D Rendering ───────────────────────────────────────────────────
    virtual void DrawPicture(int x, int y, TPicture& pic) = 0;
    virtual void DrawScaledPicture(int x, int y, int w, int h, TPicture& pic) = 0;
    virtual void DrawTrophyText(int x, int y) = 0;
    virtual void RenderHealthBar() = 0;
    virtual void Render_Cross(int x, int y) = 0;
    virtual void Render_LifeInfo(int index) = 0;

    // ── System / State ─────────────────────────────────────────────────
    virtual void SetVideoMode(int w, int h) = 0;
    virtual void SetFullScreen() = 0;

    // Returns true if the renderer uses software-style list building
    virtual bool IsSoftwareStyle() const = 0;
};

#endif // IRENDERER_H
