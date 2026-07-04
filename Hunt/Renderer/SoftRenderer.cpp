// ==========================================================================
// SoftRenderer.cpp — IRenderer wrapper for the software renderer
//
// Each method delegates to the existing free function in RenderSoft.cpp
// (or Interface.cpp for system functions). This lets the game loop
// dispatch through IRenderer uniformly, matching the GL renderer pattern.
// ==========================================================================

#include "Hunt.h"
#include "Renderer/SoftRenderer.h"

#ifdef _soft

SoftRenderer* g_SoftRenderer = nullptr;

// ============================================================================
// Helpers — free functions that exist outside RenderSoft.cpp
// ============================================================================

// Defined in Interface.cpp
extern void WaitRetrace();
extern void SetFullScreen();

// ============================================================================
// SoftRenderer
// ============================================================================

SoftRenderer::~SoftRenderer()
{
    Shutdown();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

bool SoftRenderer::Initialize()
{
    // The software renderer is initialized by InitEngine() and friends
    // (CreateVideoDIB, etc.) — nothing extra needed here.
    return true;
}

void SoftRenderer::Shutdown()
{
    // Nothing to tear down beyond what ShutDownEngine() already does.
}

// ── Scene ──────────────────────────────────────────────────────────────────

void SoftRenderer::DrawFrame(const RenderFrameContext& ctx)
{
    (void)ctx;
    ::DrawScene();
}

void SoftRenderer::DrawScene()
{
    ::DrawScene();
}

void SoftRenderer::DrawPostObjects()
{
    ::DrawPostObjects();
}

// ── Resource Management ────────────────────────────────────────────────────

void SoftRenderer::RegisterTexture(TEXTURE* /*tptr*/)
{
    // The soft renderer reads Textures[] directly — no registration needed.
}

void SoftRenderer::RegisterPicture(TPicture* /*pptr*/)
{
    // The soft renderer writes to lpVideoBuf directly — no registration needed.
}

void SoftRenderer::ReleaseModelTextures(const TModel* mptr)
{
    ::ReleaseModelTexture(mptr);
}

void SoftRenderer::ResetTerrainTextureCache()
{
    // No terrain texture cache in the soft renderer.
}

void SoftRenderer::ClearLevelTextureCache()
{
    // No level texture cache in the soft renderer.
}

// ── Frame Management ───────────────────────────────────────────────────────

void SoftRenderer::ClearVideoBuf()
{
    ::ClearVideoBuf();
}

void SoftRenderer::WaitRetrace()
{
    ::WaitRetrace();
}

void SoftRenderer::PostProcess()
{
    // The soft renderer presents the frame in ShowVideo() via BitBlt,
    // so no separate post-process step is needed here.
}

// ── 3D Rendering — Terrain ─────────────────────────────────────────────────

void SoftRenderer::DrawTPlane(bool clip)
{
    ::DrawTPlane(static_cast<BOOL>(clip));
}

void SoftRenderer::DrawTPlaneClip(bool clip)
{
    ::DrawTPlaneClip(static_cast<BOOL>(clip));
}

void SoftRenderer::DrawHMap()
{
    ::DrawHMap();
}

// ── 3D Rendering — Models ──────────────────────────────────────────────────

void SoftRenderer::RenderModel(TModel* mptr, float x0, float y0, float z0,
                                int light, int vt, float al, float bt)
{
    ::RenderModel(mptr, x0, y0, z0, light, vt, al, bt);
}

void SoftRenderer::RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                                    int light, int vt, float al, float bt)
{
    ::RenderModelClip(mptr, x0, y0, z0, light, vt, al, bt);
}

void SoftRenderer::RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                                         int light, int vt, float al, float bt)
{
    ::RenderModelClipWater(mptr, x0, y0, z0, light, vt, al, bt);
}

void SoftRenderer::RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                                    int light, int vt, float al, float bt)
{
    ::RenderNearModel(mptr, x0, y0, z0, light, al, bt);
}

// ── Character / Entity Renders ─────────────────────────────────────────────

void SoftRenderer::RenderCharacter(TCharacter* cptr)
{
    ::RenderCharacter(cptr);
}

void SoftRenderer::RenderExplosion(int /*index*/)
{
    // Not implemented in the soft renderer.
}

void SoftRenderer::RenderShip()
{
    ::RenderShip();
}

void SoftRenderer::RenderPlayer(int /*index*/)
{
    // Not implemented in the soft renderer.
}

void SoftRenderer::RenderSkyPlane()
{
    ::RenderSkyPlane();
}

// ── 2D Rendering ───────────────────────────────────────────────────────────

void SoftRenderer::DrawPicture(int x, int y, TPicture& pic)
{
    ::DrawPicture(x, y, pic);
}

void SoftRenderer::DrawScaledPicture(int x, int y, int w, int h, TPicture& pic)
{
    ::DrawScaledPicture(x, y, w, h, pic);
}

void SoftRenderer::DrawTrophyText(int x, int y)
{
    ::DrawTrophyText(x, y);
}

void SoftRenderer::RenderHealthBar()
{
    ::RenderHealthBar();
}

void SoftRenderer::Render_Cross(int x, int y)
{
    ::Render_Cross(x, y);
}

void SoftRenderer::Render_LifeInfo(int index)
{
    ::Render_LifeInfo(index);
}

// ── System / State ─────────────────────────────────────────────────────────

void SoftRenderer::SetVideoMode(int w, int h)
{
    ::SetVideoMode(w, h);
}

void SoftRenderer::SetFullScreen()
{
    ::SetFullScreen();
}

#endif // _soft
