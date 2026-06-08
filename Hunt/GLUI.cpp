// ==========================================================================
// GLUI.cpp — OpenGL 2D UI rendering for Carnivores 2 ME
//
// Minimal stub for compilation. Handles 2D overlays, text, HUD elements.
// Will be implemented when GL renderer is developed further.
// ==========================================================================

#include "Hunt.h"

#ifdef _gl

#include <cstdio>

// ============================================================================
// 2D Rendering (stubs)
// ============================================================================

void DrawPictureGL(int x, int y, TPicture& pic)
{
    // TODO: Render 2D picture using GL
    // Reference: C1 GLUI.cpp DrawPicture()
    (void)x; (void)y; (void)pic;
}

void DrawScaledPictureGL(int x, int y, int w, int h, TPicture& pic)
{
    // TODO: Render scaled 2D picture using GL
    // Reference: C1 GLUI.cpp DrawScaledPicture()
    (void)x; (void)y; (void)w; (void)h; (void)pic;
}

void DrawTrophyTextGL(int x, int y)
{
    // TODO: Render trophy text using GL
    // Reference: C1 GLUI.cpp DrawTrophyText()
    (void)x; (void)y;
}

void RenderHealthBarGL()
{
    // TODO: Render health bar using GL
    // Reference: C1 GLUI.cpp RenderHealthBar()
}

void RenderCrossGL(int x, int y)
{
    // TODO: Render crosshair using GL
    // Reference: C1 GLUI.cpp Render_Cross()
    (void)x; (void)y;
}

void RenderLifeInfoGL(int index)
{
    // TODO: Render life info using GL
    // Reference: C1 GLUI.cpp Render_LifeInfo()
    (void)index;
}

#endif // _gl
