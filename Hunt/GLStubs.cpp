// ==========================================================================
// GLStubs.cpp — Global function stubs for OpenGL renderer
//
// These are free functions called by Hunt.cpp, Interface.cpp, etc.
// Init3DHardware creates the GLRenderer instance; other functions delegate
// to g_GLRenderer where appropriate.
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"

#ifdef _gl

#include "glad/glad.h"

#include <cstdio>

// ============================================================================
// Hardware lifecycle (called from WinMain, ProcessGame)
// ============================================================================

void Init3DHardware()
{
    PrintLog("\n");
    PrintLog("==Init3DHardware (OpenGL)==\n");

    if (g_GLRenderer) {
        PrintLog("GL: WARNING - GLRenderer already exists, shutting down first.\n");
        g_GLRenderer->Shutdown();
        delete g_GLRenderer;
        g_GLRenderer = nullptr;
    }

    g_GLRenderer = new GLRenderer();
    if (!g_GLRenderer->Initialize()) {
        PrintLog("GL: ERROR - GLRenderer::Initialize() failed!\n");
        delete g_GLRenderer;
        g_GLRenderer = nullptr;
        DoHalt("OpenGL initialization failed. Check render.log for details.");
    }

    DirectActive = TRUE;
    HARD3D = TRUE;
    PrintLog("==Init3DHardware (OpenGL) Complete==\n");
    PrintLog("\n");
}

void Activate3DHardware()
{
    PrintLog("GL: Activate3DHardware()\n");

    // Set video mode (this sets window size and position)
    SetVideoMode(WinW, WinH);

    if (g_GLRenderer) {
        g_GLRenderer->SetVideoMode(WinW, WinH);
    }

    // Ensure window is in foreground
    if (hwndMain) {
        SetForegroundWindow(hwndMain);
        SetFocus(hwndMain);
    }
}

void ShutDown3DHardware()
{
    PrintLog("GL: ShutDown3DHardware()\n");

    if (g_GLRenderer) {
        g_GLRenderer->Shutdown();
        delete g_GLRenderer;
        g_GLRenderer = nullptr;
    }

    DirectActive = FALSE;
}

// ============================================================================
// Frame management
// ============================================================================

void ClearVideoBuf()
{
    if (g_GLRenderer) g_GLRenderer->ClearVideoBuf();
}

void ShowVideo()
{
    // Apply depth-based sun occlusion after the full scene is rendered
    if (g_GLRenderer) {
        g_GLRenderer->ApplySunDepthOcclusion();
    }

    // Apply sun glare/blinding effect (matching D3D/3DFX ShowVideo)
    if (g_GLRenderer) {
        float sunLight = g_GLRenderer->GetSunLight();
        if (!UNDERWATER && sunLight > 1.0f) {
            uint32_t glareColor = 0xFFFFC0 | (static_cast<uint32_t>(sunLight) << 24);
            g_GLRenderer->RenderFSRect(glareColor);
        }
    }

    // Swap buffers
    if (g_GLRenderer && hwndMain) {
        HDC hdc = GetDC(hwndMain);
        if (hdc) {
            SwapBuffers(hdc);
            ReleaseDC(hwndMain, hdc);
        }
    }
}

void Hardware_ZBuffer(BOOL enable)
{
    if (enable) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void CopyHARDToDIB()
{
    // TODO: Copy GL framebuffer to DIB for screenshots
}

// ============================================================================
// Rendering pipeline (called from DrawScene)
// ============================================================================

void RenderSkyPlane()
{
    if (g_GLRenderer) {
        g_GLRenderer->ClearVideoBuf();
        g_GLRenderer->RenderSkyPlane();
    }
}

void RenderGround()
{
    if (g_GLRenderer) g_GLRenderer->RenderGround();
}

void RenderModelsList()
{
    if (g_GLRenderer) g_GLRenderer->RenderModelsList();
}

void Render3DHardwarePosts()
{
    if (g_GLRenderer) g_GLRenderer->Render3DHardwarePosts();
}

void RenderWater()
{
    if (g_GLRenderer) g_GLRenderer->RenderWater();
}

void RenderElements()
{
    // TODO: Render elements using GL
}

void DrawHMap()
{
    // TODO: Render minimap using GL
}

// ============================================================================
// Model rendering (called from DrawPostObjects, etc.)
// ============================================================================

void RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                     int light, float al, float bt)
{
    if (g_GLRenderer) g_GLRenderer->RenderNearModel(mptr, x0, y0, z0, light, 0, al, bt);
}

void RenderModel(TModel* mptr, float x0, float y0, float z0, int light, int vt, float al, float bt)
{
    if (g_GLRenderer) g_GLRenderer->RenderModel(mptr, x0, y0, z0, light, vt, al, bt);
}

void RenderModelClip(TModel* mptr, float x0, float y0, float z0, int light, int vt, float al, float bt)
{
    if (g_GLRenderer) g_GLRenderer->RenderModelClip(mptr, x0, y0, z0, light, vt, al, bt);
}

void RenderModelClipWater(TModel* mptr, float x0, float y0, float z0, int light, int vt, float al, float bt)
{
    if (g_GLRenderer) g_GLRenderer->RenderModelClipWater(mptr, x0, y0, z0, light, vt, al, bt);
}

void RenderBMPModel(TBMPModel* mptr, float x0, float y0, float z0, int light)
{
    if (g_GLRenderer) g_GLRenderer->RenderBMPModel(mptr, x0, y0, z0, light);
}

void RenderModelClipPhongMap(TModel* mptr, float x0, float y0, float z0,
                             float al, float bt)
{
    // TODO: Render model with phong mapping using GL
    (void)mptr; (void)x0; (void)y0; (void)z0;
    (void)al; (void)bt;
}

void RenderModelClipEnvMap(TModel* mptr, float x0, float y0, float z0,
                           float al, float bt)
{
    // TODO: Render model with environment mapping using GL
    (void)mptr; (void)x0; (void)y0; (void)z0;
    (void)al; (void)bt;
}

// ============================================================================
// 2D rendering (called from DrawPostObjects, Interface.cpp)
// ============================================================================

void DrawPicture(int x, int y, TPicture& pic)
{
    if (g_GLRenderer) g_GLRenderer->DrawPicture(x, y, pic);
}

void DrawFlash(int x, int y, int w, int h, TPicture& pic)
{
    // TODO: Draw flash effect using GL
    (void)x; (void)y; (void)w; (void)h; (void)pic;
}

void DrawTrophyText(int x, int y)
{
    if (g_GLRenderer) g_GLRenderer->DrawTrophyText(x, y);
}

void DrawScoreText(int x, int y)
{
    // TODO: Draw score text using GL
    (void)x; (void)y;
}

void DrawSurvivalText(int x, int y)
{
    // TODO: Draw survival text using GL
    (void)x; (void)y;
}

void Render_Cross(int x, int y)
{
    if (g_GLRenderer) g_GLRenderer->Render_Cross(x, y);
}

void Render_LifeInfo(int index)
{
    if (g_GLRenderer) g_GLRenderer->Render_LifeInfo(index);
}

void ShowControlElements()
{
    // TODO: Show control elements using GL
}

// ============================================================================
// Render table allocation (called from ReInitGame)
// ============================================================================

void AllocateRenderTables()
{
    // TODO: Allocate render tables for GL
    // In software renderer, this allocates rVertex, ChRenderList, etc.
    // GL renderer may not need these, but the function must exist.
}

#endif // _gl
