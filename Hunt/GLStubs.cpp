// ==========================================================================
// GLStubs.cpp — Global function stubs for OpenGL renderer
//
// These are free functions called by Hunt.cpp, Interface.cpp, etc.
// They will be implemented incrementally as the GL renderer is developed.
// For now, they are stubs that allow the GL build to link.
// ==========================================================================

#include "Hunt.h"

#ifdef _gl

#include <cstdio>

// ============================================================================
// Hardware lifecycle (called from WinMain, ProcessGame)
// ============================================================================

void Init3DHardware()
{
    PrintLog("GL: Init3DHardware() — stub\n");
}

void Activate3DHardware()
{
    PrintLog("GL: Activate3DHardware() — stub\n");
}

void ShutDown3DHardware()
{
    PrintLog("GL: ShutDown3DHardware() — stub\n");
}

// ============================================================================
// Frame management
// ============================================================================

void ShowVideo()
{
    // TODO: Swap buffers (wglSwapBuffers)
}

void Hardware_ZBuffer(BOOL enable)
{
    // TODO: glEnable/glDisable(GL_DEPTH_TEST)
    (void)enable;
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
    // TODO: Render sky using GL
}

void RenderGround()
{
    // TODO: Render terrain using GL
}

void RenderModelsList()
{
    // TODO: Render models using GL
}

void Render3DHardwarePosts()
{
    // TODO: Render hardware posts using GL
}

void RenderWater()
{
    // TODO: Render water using GL
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
    // TODO: Render near model using GL
    (void)mptr; (void)x0; (void)y0; (void)z0;
    (void)light; (void)al; (void)bt;
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
    // TODO: Draw 2D picture using GL
    (void)x; (void)y; (void)pic;
}

void DrawFlash(int x, int y, int w, int h, TPicture& pic)
{
    // TODO: Draw flash effect using GL
    (void)x; (void)y; (void)w; (void)h; (void)pic;
}

void DrawTrophyText(int x, int y)
{
    // TODO: Draw trophy text using GL
    (void)x; (void)y;
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
    // TODO: Draw crosshair using GL
    (void)x; (void)y;
}

void Render_LifeInfo(int index)
{
    // TODO: Draw life info using GL
    (void)index;
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
