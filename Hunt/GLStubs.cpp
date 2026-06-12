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
        // Clear lpVideoBuf at the start of each frame for HUD overlay
        if (lpVideoBuf && VideoPitchB > 0 && WinH > 0)
            memset(lpVideoBuf, 0, (size_t)VideoPitchB * WinH);
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
    if (g_GLRenderer) g_GLRenderer->RenderElements();
}

static void PutPixelBuf(int x, int y, WORD color)
{
    if (!lpVideoBuf || x < 0 || x >= WinW || y < 0 || y >= WinH) return;
    ((WORD*)lpVideoBuf)[y * VideoPitch + x] = color;
}

static void DrawBoxBuf(int x, int y, int size, WORD color)
{
    for (int dy = 0; dy < size; dy++)
        for (int dx = 0; dx < size; dx++)
            PutPixelBuf(x + dx, y + dy, color);
}

void DrawHMap()
{
    if (SurvivalMode) return;
    if (!lpVideoBuf || !MapPic.lpImage) return;

    // Draw map background
    DrawPicture(VideoCX - MapPic.W / 2, VideoCY - MapPic.H / 2 - 6, MapPic);

    // Player marker
    int xx = VideoCX - 128 + (CCX >> 2);
    int yy = VideoCY - 128 + (CCY >> 2);
    if (yy >= 0 && yy < WinH && xx >= 0 && xx < WinW) {
        DrawBoxBuf(xx + 1, yy + 1, 2, (WORD)(8 << 11));   // dark red shadow
        DrawBoxBuf(xx, yy, 2, (WORD)(30 << 11));           // bright red
    }

    // Dinosaur markers (if radar mode)
    if (RadarMode) {
        for (int c = 0; c < ChCount; c++) {
            if (!DinoInfo[Characters[c].CType].onRadar && !Characters[c].RTime) continue;
            if (!Characters[c].Health && !Characters[c].RTime) continue;

            int dx = VideoCX - 128 + (int)Characters[c].pos.x / 1024;
            int dy = VideoCY - 128 + (int)Characters[c].pos.z / 1024;
            if (dy <= 0 || dy >= WinH || dx <= 0 || dx >= WinW) continue;
            DrawBoxBuf(dx, dy, 2, DinoInfo[Characters[c].CType].radarColour565);
        }
    }
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

// Convert 565 to 555 format for lpVideoBuf (16-bit BI_RGB DIB)
static inline WORD Conv565to555(WORD c) {
    // 565: RRRRRGGGGGGBBBBB
    // 555: XRRRRRGGGGGBBBBB
    // Split 565 into components
    int r = (c >> 11) & 0x1F;
    int g = (c >> 5) & 0x3F;
    int b = c & 0x1F;
    // Pack as 555 (drop lowest G bit)
    return (r << 10) | ((g >> 1) << 5) | b;
}

void DrawPicture(int x, int y, TPicture& pic)
{
    if (!pic.lpImage || pic.W <= 0 || pic.H <= 0 || !lpVideoBuf) return;

    // Pictures are in 565 format (after conv_pic). Copy to lpVideoBuf (555 DIB)
    // with 565→555 conversion.
    WORD* dst = (WORD*)lpVideoBuf;
    for (int yy = 0; yy < pic.H; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int copyW = pic.W;
        int srcX = 0;
        int dstX = x;
        if (dstX < 0) { srcX = -dstX; copyW += dstX; dstX = 0; }
        if (dstX + copyW > WinW) copyW = WinW - dstX;
        if (copyW <= 0) continue;
        const WORD* src = pic.lpImage + yy * pic.W + srcX;
        WORD* d = dst + dstY * VideoPitch + dstX;
        for (int i = 0; i < copyW; i++) {
            d[i] = Conv565to555(src[i]);
        }
    }
}

void DrawFlash(int x, int y, int w, int h, TPicture& pic)
{
    // Flash effect: copy raw pixels to lpVideoBuf (no transparency)
    if (!pic.lpImage || w <= 0 || h <= 0 || !lpVideoBuf) return;
    WORD* dst = (WORD*)lpVideoBuf;
    for (int yy = 0; yy < h; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int copyW = w;
        int srcX = 0;
        int dstX = x;
        if (dstX < 0) { srcX = -dstX; copyW += dstX; dstX = 0; }
        if (dstX + copyW > WinW) copyW = WinW - dstX;
        if (copyW <= 0) continue;
        const WORD* src = pic.lpImage + yy * pic.W + srcX;
        WORD* d = dst + dstY * VideoPitch + dstX;
        for (int i = 0; i < copyW; i++) {
            d[i] = Conv565to555(src[i]);
        }
    }
}

void DrawTrophyText(int x, int y)
{
    if (g_GLRenderer) g_GLRenderer->DrawTrophyText(x, y);
}

void DrawScoreText(int x, int y)
{
    // Draw score text onto lpVideoBuf via GDI
    if (!hdcMain || !hbmpVideoBuf || !lpVideoBuf) return;

    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcCMain, hbmpVideoBuf);
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = NULL;
    if (fnt_Small) oldFont = (HFONT)SelectObject(hdcCMain, fnt_Small);

    char t[32];
    int tx = x + 14;
    int ty = y + 18;

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00101010);
        TextOut(hdcCMain, px + 1, py + 1, str, (int)strlen(str));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, (int)strlen(str));
    };

    textOut(tx, ty, "Unclaimed Kill - Score Added: ", 0x00BFBFBF);
    SIZE sz;
    GetTextExtentPoint32(hdcCMain, "Unclaimed Kill - Score Added: ", 31, &sz);
    tx += sz.cx;
    wsprintf(t, "%d", ScoreDisp);
    textOut(tx, ty, t, 0x0000BFBF);

    if (oldFont) SelectObject(hdcCMain, oldFont);
    SelectObject(hdcCMain, hbmpOld);
}

void DrawSurvivalText(int x, int y)
{
    // Draw survival text onto lpVideoBuf via GDI
    if (!hdcMain || !hbmpVideoBuf || !lpVideoBuf) return;

    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcCMain, hbmpVideoBuf);
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = NULL;
    if (fnt_Small) oldFont = (HFONT)SelectObject(hdcCMain, fnt_Small);

    char t[32];

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00101010);
        TextOut(hdcCMain, px + 1, py + 1, str, (int)strlen(str));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, (int)strlen(str));
    };

    int tx = x + 40;
    int ty = y + 98;
    textOut(tx, ty, "Waves Survived: ", 0x00BFBFBF);
    SIZE sz;
    GetTextExtentPoint32(hdcCMain, "Waves Survived: ", 16, &sz);
    tx += sz.cx;
    wsprintf(t, "%i", SurvivalWave - 1);
    textOut(tx, ty, t, 0x0000BFBF);

    tx = x + 40;
    ty = y + 124;
    textOut(tx, ty, "High Score: ", 0x00BFBFBF);
    GetTextExtentPoint32(hdcCMain, "High Score: ", 12, &sz);
    tx += sz.cx;
    wsprintf(t, "%i", TrophyRoom2.survivalHighScore);
    textOut(tx, ty, t, 0x0000BFBF);

    if (oldFont) SelectObject(hdcCMain, oldFont);
    SelectObject(hdcCMain, hbmpOld);
}

void Render_Cross(int x, int y)
{
    if (g_GLRenderer) g_GLRenderer->Render_Cross(x, y);
}

void Render_LifeInfo(int index)
{
    if (g_GLRenderer) g_GLRenderer->Render_LifeInfo(index);
}

// Top-right health bar. Matches the layout used by Soft/D3D/3DFX
// (L = WinW/4, top-right with WinW/20 margin, WinH/40 from top).
// The bar is drawn into lpVideoBuf; DrawHUDOverlay uploads it.
//
// Note: in the GDI-to-lpVideoBuf HUD pipeline, a value of 0 is treated as
// transparent in the overlay upload (see GLRenderer::UpdateUIPixels), so
// the bar's black borders must use a non-zero value. 0x0001 expands to a
// near-black opaque pixel in the overlay.
void RenderHealthBar()
{
    if (MyHealth >= 100000) return;
    if (MyHealth == 0) return;
    if (!lpVideoBuf) return;

    int L  = WinW / 4;
    int x0 = WinW - (WinW / 20) - L;
    int y0 = WinH / 40;
    int G  = (MyHealth * 30 / 100000);            if (G > 20) G = 20;
    int R  = ((100000 - MyHealth) * 30 / 100000); if (R > 20) R = 20;
    int HCOLOR = (G << 5) | (R << 10);            // 555: G at bits 5-9, R at 10-14

    int L0 = (L * MyHealth) / 100000;
    int H  = WinH / 200;
    if (H < 1) H = 1;

    if (x0 < 1 || x0 + L >= WinW || y0 < 1 || y0 + H + 1 >= WinH) return;

    const WORD BORDER = 0x0001; // non-zero so the overlay treats it as opaque

    // Top and bottom border rows (full width of bar + corners)
    FillMemory((WORD*)lpVideoBuf + ((y0 - 1) * VideoPitch) + x0 - 1, (L + 2) * 2, BORDER);
    FillMemory((WORD*)lpVideoBuf + ((y0 + H + 1) * VideoPitch) + x0 - 1, (L + 2) * 2, BORDER);

    // Bar body
    for (int y = 0; y <= H; y++) {
        WORD* row = (WORD*)lpVideoBuf + ((y0 + y) * VideoPitch);
        row[x0 - 1] = BORDER;
        row[x0 + L] = BORDER;
        for (int x = 0; x < L0; x++)
            row[x0 + x] = (WORD)HCOLOR;
    }
}

void ShowControlElements()
{
    if (!hdcMain || !hbmpVideoBuf || !lpVideoBuf) return;

    char buf[128];

    // Draw text elements onto lpVideoBuf via GDI
    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcCMain, hbmpVideoBuf);
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = NULL;
    if (fnt_Small) oldFont = (HFONT)SelectObject(hdcCMain, fnt_Small);

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00101010);
        TextOut(hdcCMain, px + 1, py + 1, str, (int)strlen(str));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, (int)strlen(str));
    };

    if (TIMER)
    {
        wsprintf(buf, "msc: %d", TimeDt);
        textOut(WinEX - 81, 11, buf, 0x0020A0A0);
        wsprintf(buf, "polys: %d", dFacesCount);
        textOut(WinEX - 90, 24, buf, 0x0020A0A0);
    }

    if (MessageList.timeleft)
    {
        if (RealTime > MessageList.timeleft) MessageList.timeleft = 0;
        textOut(10, 10, MessageList.mtext, 0x0020A0A0);
    }

    if (ExitTime)
    {
        int yline = WinH / 3;
        wsprintf(buf, "Preparing for evacuation...");
        textOut(VideoCX - GetTextW(hdcMain, buf) / 2, yline, buf, 0x0060C0D0);
        wsprintf(buf, "%d seconds left.", 1 + ExitTime / 1000);
        textOut(VideoCX - GetTextW(hdcMain, buf) / 2, yline + 18, buf, 0x0060C0D0);
    }

    if (WaveNoteTime)
    {
        int yline = WinH / 3;
        wsprintf(buf, "Waves Survived: %i", SurvivalWave - 1);
        textOut(VideoCX - GetTextW(hdcMain, buf) / 2, yline, buf, 0x0060C0D0);
    }

    if (oldFont) SelectObject(hdcCMain, oldFont);
    SelectObject(hdcCMain, hbmpOld);

    // Health bar is drawn into lpVideoBuf after the text elements so it
    // sits on top in the overlay upload.
    RenderHealthBar();

    // Upload lpVideoBuf overlay to GL
    if (g_GLRenderer) g_GLRenderer->DrawHUDOverlay();
}

// ============================================================================
// Render table allocation (called from ReInitGame)
// ============================================================================

void AllocateRenderTables()
{
    // GL renderer doesn't need software render tables.
}

#endif // _gl
