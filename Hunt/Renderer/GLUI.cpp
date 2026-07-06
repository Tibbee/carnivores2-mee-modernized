// ==========================================================================
// GLUI.cpp — OpenGL 2D UI rendering and renderer dispatch free functions
//
// Provides the free functions declared in Hunt.h for the OpenGL renderer
// (_gl). The 2D UI elements (DrawPicture, DrawFlash, DrawTrophyText,
// DrawScoreText, DrawSurvivalText, DrawHMap, RenderHealthBar, etc.) write
// into lpVideoBuf via GDI or direct pixel writes, and ShowControlElements
// ends with g_GLRenderer->DrawHUDOverlay() to upload the buffer as a
// texture and composite it on top of the 3D scene. The 3D free functions
// delegate to g_GLRenderer. Init3DHardware creates the GLRenderer instance.
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"

#ifdef _gl

#include "glad/glad.h"
#include "Renderer/GLPerf.h"

#include <cmath>
#include <cstdio>
#include <vector>

// ============================================================================
// Hardware lifecycle (called from WinMain, ProcessGame)
// ============================================================================

#ifdef GL_PERF_HOOKS
// F11 key handler — triggers a 1-second per-frame GL perf CSV capture.
void PerfTriggerCapture()
{
    glperf_trigger_capture();
}

// Frame boundary hooks — called from Hunt.cpp::DrawScene (begin) and
// Hunt.cpp::DrawPostObjects (end). Bracket the per-frame GL work so
// glperf.log reports a clean "frame total" alongside the per-pass
// scopes. No-op when GL_PERF_HOOKS is not defined.
void PerfFrameBegin()
{
    glperf_frame_begin();
}

void PerfFrameEnd()
{
    glperf_frame_end();
}
#endif



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

    DirectActive = true;
    HARD3D = true;
    PrintLog("==Init3DHardware (OpenGL) Complete==\n");
    PrintLog("\n");
}

void Activate3DHardware()
{
    PrintLog("GL: Activate3DHardware()\n");

    // Set video mode (this sets window size and position)
    SetVideoMode(WinW, WinH);

    // If the renderer was shut down (e.g., during RestartMode), create a
    // new one. ShutDown3DHardware() deletes g_GLRenderer and nulls the
    // pointer; the restart flow then sets NeedRVM which routes through
    // here. Without this, g_GLRenderer stays null, every GL draw call
    // becomes a no-op, and the screen stays black after Escape+R.
    // Init3DHardware() handles the "already exists" case by shutting
    // down first, so it's safe to call unconditionally.
    if (!g_GLRenderer) {
        Init3DHardware();
    }

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

    DirectActive = false;
}

// ============================================================================
// Frame management
// ============================================================================

void ClearRendererLevelCache()
{
    // Phase 5E follow-up: delete GL textures from the per-level model
    // caches before LoadResources loads new models. This is called from
    // ReleaseResources() (in Resources.cpp), which runs at the START of
    // LoadResources, before any new textures are uploaded. The arena
    // Reset() has already freed the TModel* pointers that key these
    // caches; clearing them here prevents the GL renderer from serving
    // stale textures when the same arena addresses are reused.
    if (g_GLRenderer) {
        g_GLRenderer->ClearLevelTextureCache();
    }
}

void ClearRendererTerrainCache()
{
    // Reset the terrain texture upload cache between levels so the next
    // level's terrain tiles trigger fresh glTexSubImage3D uploads.
    if (g_GLRenderer) {
        g_GLRenderer->ResetTerrainTextureCache();
    }
}

void ReleaseModelTexture(const TModel* mptr)
{
    // Remove the GL texture cache entry for a single model before the
    // model is freed (either via arena reset or explicit _HeapFree).
    // This prevents stale cache hits when a new model reuses the same
    // arena address. Global models (heap-allocated) also pass through
    // here during ReleaseGlobalResources — the cache entry is removed
    // once and the GL texture is deleted, which is correct since the
    // global model is being destroyed permanently.
    if (g_GLRenderer) {
        g_GLRenderer->ReleaseModelTextures(mptr);
    }
}

void ClearVideoBuf()
{
    if (g_GLRenderer) g_GLRenderer->ClearVideoBuf();
}

void ShowVideo()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_FRAME_END();
#endif
    // Apply depth-based sun occlusion after the full scene is rendered
    if (g_GLRenderer) {
        g_GLRenderer->ApplySunDepthOcclusion();
    }

    // Apply sun glare/blinding effect (matching D3D/3DFX ShowVideo)
    if (g_GLRenderer) {
        float sunLight = g_GLRenderer->GetSunLight();
        // Midway boost between C1 (1.5x + double skyTraceK) and C2 (1.0x):
        // use 1.25x with linear skyTraceK, giving peak alpha ~175/255 ≈ 0.69.
        const float kGlareBoost = 1.25f;
        sunLight = (std::min)(255.0f, sunLight * kGlareBoost);
        if (!IsUnderwater() && sunLight > 1.0f) {
            uint32_t glareColor = 0xFFFFC0 | (static_cast<uint32_t>(sunLight) << 24);
            g_GLRenderer->RenderFSRect(glareColor);
        }
    }

    // Apply night darkness overlay (desaturation was done before HUD in ShowControlElements)
    if (OptDayNight == 2 && !NightVisionOn) {
        g_GLRenderer->RenderNightDarkness();
    }

    // Apply night vision green overlay (toggleable via equipment + keybind)
    if (NightVisionOn) {
        g_GLRenderer->RenderFSRect(0x6000FF00);
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
    if (!g_GLRenderer || !lpVideoBuf || WinW <= 0 || WinH <= 0 || VideoPitch <= 0) return;

    std::vector<GLubyte> pixels(static_cast<size_t>(WinW) * WinH * 4);
    glReadPixels(0, 0, WinW, WinH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    WORD* dst = static_cast<WORD*>(lpVideoBuf);
    for (int y = 0; y < WinH; y++)
    {
        const GLubyte* srcRow = pixels.data() + (WinH - 1 - y) * WinW * 4;
        WORD* dstRow = dst + y * VideoPitch;
        for (int x = 0; x < WinW; x++)
        {
            const int sx = x * 4;
            const WORD r = static_cast<WORD>((srcRow[sx + 0] >> 3) & 0x1F);
            const WORD g = static_cast<WORD>((srcRow[sx + 1] >> 3) & 0x1F);
            const WORD b = static_cast<WORD>((srcRow[sx + 2] >> 3) & 0x1F);
            dstRow[x] = static_cast<WORD>((r << 10) | (g << 5) | b);
        }
    }

    // The full DIB was overwritten with 3D scene pixels.  The next
    // frame must do a full clear + full upload so the HUD overlay
    // texture doesn't show stale scene data.
    g_GLRenderer->InvalidateHUDOverlay();
}

// ============================================================================
// Rendering pipeline (called from DrawScene)
// ============================================================================

void RenderSkyPlane()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_FRAME_BEGIN();
#endif
    if (g_GLRenderer) {
        g_GLRenderer->ClearVideoBuf();
        // Clear only previous frame's dirty HUD regions (not the whole buffer).
        // The rest of lpVideoBuf retains its state from the last upload — no
        // need to zero it out because the GPU texture already matches.
        g_GLRenderer->ClearStaleHUDRegions();
        g_GLRenderer->RenderSkyPlane();
    }
}

void RenderWCircles()
{
    if (g_GLRenderer) g_GLRenderer->RenderWCircles();
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

void RenderProjectedShadows()
{
    if (g_GLRenderer) g_GLRenderer->RenderProjectedShadows();
}

void RenderWater()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderWater");
#endif
    if (!g_GLRenderer) return;
    g_GLRenderer->RenderWater();
    // D3D/3DFX call RenderWCircles() from inside their own RenderWater();
    // the GL flow has RenderWater() on the renderer return, so we route the
    // call through the IRenderer hook here. RenderWCircles() pushes the
    // morphed ripple models with additive blending and drains them.
    g_GLRenderer->RenderWCircles();
}

void RenderElements()
{
    if (g_GLRenderer) g_GLRenderer->RenderElements();
}

static int CircleCXBuf = 0;
static int CircleCYBuf = 0;

static void PutPixelBuf(int x, int y, WORD color)
{
    if (!lpVideoBuf || x < 0 || x >= WinW || y < 0 || y >= WinH) return;
    (static_cast<WORD*>(lpVideoBuf))[y * VideoPitch + x] = color;
}

static void Put8PixelBuf(int x, int y, WORD color)
{
    PutPixelBuf(CircleCXBuf + x, CircleCYBuf + y, color);
    PutPixelBuf(CircleCXBuf + x, CircleCYBuf - y, color);
    PutPixelBuf(CircleCXBuf - x, CircleCYBuf + y, color);
    PutPixelBuf(CircleCXBuf - x, CircleCYBuf - y, color);
    PutPixelBuf(CircleCXBuf + y, CircleCYBuf + x, color);
    PutPixelBuf(CircleCXBuf + y, CircleCYBuf - x, color);
    PutPixelBuf(CircleCXBuf - y, CircleCYBuf + x, color);
    PutPixelBuf(CircleCXBuf - y, CircleCYBuf - x, color);
}

static void DrawCircleBuf(int cx, int cy, int radius, WORD color)
{
    int d = 3 - (2 * radius);
    int x = 0;
    int y = radius;
    CircleCXBuf = cx;
    CircleCYBuf = cy;

    do
    {
        Put8PixelBuf(x, y, color);
        x++;
        if (d < 0)
        {
            d = d + (x << 2) + 6;
        }
        else
        {
            d = d + (x - y) * 4 + 10;
            y--;
        }
    } while (x < y);

    Put8PixelBuf(x, y, color);
}

static void DrawBoxBuf(int x, int y, int size, WORD color)
{
    for (int dy = 0; dy < size; dy++)
        for (int dx = 0; dx < size; dx++)
            PutPixelBuf(x + dx, y + dy, color);
}

static void DrawBoxMysteryBuf(int x, int y, WORD color)
{
    PutPixelBuf(x + 1, y, color);
    PutPixelBuf(x + 2, y, color);
    PutPixelBuf(x + 1, y + 1, color);
    PutPixelBuf(x + 1, y + 3, color);
    PutPixelBuf(x, y + 4, color);
    PutPixelBuf(x + 3, y + 4, color);
    PutPixelBuf(x + 1, y + 5, color);
    PutPixelBuf(x + 2, y + 5, color);
}

void DrawHMap()
{
    if (g_GameMode == GameMode::SurvivalMode) return;
    if (!lpVideoBuf || !MapPic.lpImage) return;

    // Draw map background
    DrawPicture(VideoCX - MapPic.W / 2, VideoCY - MapPic.H / 2 - 6, MapPic);

    int xx = VideoCX - 128 + (CCX >> 2);
    int yy = VideoCY - 128 + (CCY >> 2);
    const int playerX = xx;
    const int playerY = yy;

    if (yy < 0 || yy >= WinH || xx < 0 || xx >= WinW) return;

    DrawBoxBuf(xx + 1, yy + 1, 2, static_cast<WORD>(8 << 10));    // dark red shadow
    DrawBoxBuf(xx, yy, 2, static_cast<WORD>(30 << 10));            // bright red

    float previousSonarPos = 0.0f;
    if (g_GameMode == GameMode::SonarMode)
    {
        previousSonarPos = sonarPos;
        sonarPos += TimeDt * 0.02f * static_cast<float>(std::cos((pi / 2.0f) * (sonarPos / 41.0f)));
        if (sonarPos > 38.0f) sonarPos = 1.0f;
        DrawCircleBuf(xx, yy, static_cast<int>(sonarPos), static_cast<WORD>(18 << 5));
    }

    DrawCircleBuf(xx + 1, yy + 1, ctViewR / 4, static_cast<WORD>(4 << 5));
    DrawCircleBuf(xx, yy, ctViewR / 4, static_cast<WORD>(18 << 5));

    for (int b = 0; b < bulletCh; b++)
    {
        if (!bullet[b].RTime) continue;

        xx = VideoCX - 128 + static_cast<int>(bullet[b].a.x) / 1024;
        yy = VideoCY - 128 + static_cast<int>(bullet[b].a.z) / 1024;
        if (yy > 0 && yy < WinH && xx > 0 && xx < WinW)
        {
            DrawBoxBuf(xx, yy, 2, WeapInfo[bullet[b].parent].radarColour555);
        }
    }

    for (int c = 0; c < ChCount; c++)
    {
        if (!DinoInfo[Characters[c].CType].onRadar && !Characters[c].RTime) continue;
        if (!Characters[c].Health && !Characters[c].RTime) continue;

        xx = VideoCX - 128 + static_cast<int>(Characters[c].pos.x) / 1024;
        yy = VideoCY - 128 + static_cast<int>(Characters[c].pos.z) / 1024;
        if (yy <= 0 || yy >= WinH || xx <= 0 || xx >= WinW) continue;

        if (Characters[c].Clone == AI_HUNTDOG)
        {
            DrawBoxBuf(xx, yy, 2, DinoInfo[Characters[c].CType].radarColour555);
            continue;
        }

        if (RadarMode || Characters[c].RTime)
        {
            WORD colour = DinoInfo[Characters[c].CType].radarColour555;
            if (Characters[c].tracker >= 0) colour = WeapInfo[Characters[c].tracker].radarColour555;

            if (DinoInfo[Characters[c].CType].Mystery)
                DrawBoxMysteryBuf(xx, yy, colour);
            else
                DrawBoxBuf(xx, yy, 2, colour);
        }

        if (g_GameMode == GameMode::SonarMode)
        {
            const int dx = playerX - xx;
            const int dz = playerY - yy;
            const int distance = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dz * dz)));

            if (distance < 38)
            {
                if (distance >= static_cast<int>(previousSonarPos) && distance <= static_cast<int>(sonarPos))
                {
                    Characters[c].showSonar = true;
                    Characters[c].sonar.x = xx;
                    Characters[c].sonar.y = yy;
                    AddVoicev(fxBlip.length, fxBlip.lpData.data(), 256);
                }
                else
                {
                    Characters[c].showSonar = false;
                }
            }
            else
            {
                Characters[c].showSonar = false;
            }

            if (Characters[c].showSonar && !Characters[c].RTime)
            {
                if (DinoInfo[Characters[c].CType].Mystery)
                    DrawBoxMysteryBuf(Characters[c].sonar.x, Characters[c].sonar.y, DinoInfo[Characters[c].CType].radarColour555);
                else
                    DrawBoxBuf(Characters[c].sonar.x, Characters[c].sonar.y, 2, DinoInfo[Characters[c].CType].radarColour555);
            }
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
    if (g_GLRenderer) {
        g_GLRenderer->RenderModelClipPhongMap(mptr, x0, y0, z0, al, bt);
    }
}

void RenderModelClipEnvMap(TModel* mptr, float x0, float y0, float z0,
                           float al, float bt)
{
    if (g_GLRenderer) {
        g_GLRenderer->RenderModelClipEnvMap(mptr, x0, y0, z0, al, bt);
    }
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
    WORD* dst = static_cast<WORD*>(lpVideoBuf);
    for (int yy = 0; yy < pic.H; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int copyW = pic.W;
        int srcX = 0;
        int dstX = x;
        if (dstX < 0) { srcX = -dstX; copyW += dstX; dstX = 0; }
        if (dstX + copyW > WinW) copyW = WinW - dstX;
        if (copyW <= 0) continue;
        const WORD* src = pic.lpImage.get() + yy * pic.W + srcX;
        WORD* d = dst + dstY * VideoPitch + dstX;
        for (int i = 0; i < copyW; i++) {
            d[i] = Conv565to555(src[i]);
        }
    }

    if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(x, y, pic.W, pic.H);
}

void DrawScaledPicture(int x, int y, int w, int h, TPicture& pic)
{
    if (g_GLRenderer) g_GLRenderer->DrawScaledPicture(x, y, w, h, pic);
}

void DrawFlash(int x, int y, int w, int h, TPicture& pic)
{
    // Flash effect: copy raw pixels to lpVideoBuf (no transparency).
    // When w/h differ from the source picture, nearest-neighbor scale the
    // destination region so scaled ammo muzzle flashes do not read past the
    // source bitmap.
    if (!pic.lpImage || pic.W <= 0 || pic.H <= 0 || w <= 0 || h <= 0 || !lpVideoBuf) return;

    WORD* dst = static_cast<WORD*>(lpVideoBuf);
    if (w == pic.W && h == pic.H) {
        for (int yy = 0; yy < h; yy++) {
            int dstY = yy + y;
            if (dstY < 0 || dstY >= WinH) continue;
            int copyW = w;
            int srcX = 0;
            int dstX = x;
            if (dstX < 0) { srcX = -dstX; copyW += dstX; dstX = 0; }
            if (dstX + copyW > WinW) copyW = WinW - dstX;
            if (copyW <= 0) continue;
            const WORD* src = pic.lpImage.get() + yy * pic.W + srcX;
            WORD* d = dst + dstY * VideoPitch + dstX;
            memcpy(d, src, copyW * sizeof(WORD));
        }
        if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(x, y, w, h);
        return;
    }

    for (int yy = 0; yy < h; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int sy = yy * pic.H / h;
        for (int xx = 0; xx < w; xx++) {
            int dstX = xx + x;
            if (dstX < 0 || dstX >= WinW) continue;
            int sx = xx * pic.W / w;
            dst[dstY * VideoPitch + dstX] = pic.lpImage[sy * pic.W + sx];
        }
    }

    if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(x, y, w, h);
}

void DrawTrophyText(int x, int y)
{
    if (g_GLRenderer) g_GLRenderer->DrawTrophyText(x, y);
}

void DrawScoreText(int x, int y)
{
    // Draw score text onto lpVideoBuf via GDI
    if (!hdcMain || !hbmpVideoBuf || !lpVideoBuf) return;

    HBITMAP hbmpOld = reinterpret_cast<HBITMAP>(SelectObject(hdcCMain, hbmpVideoBuf));
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = nullptr;
    if (fnt_Small) oldFont = reinterpret_cast<HFONT>(SelectObject(hdcCMain, fnt_Small));

    char t[32];
    int tx = x + 14;
    int ty = y + 18;

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00101010);
        TextOut(hdcCMain, px + 1, py + 1, str, static_cast<int>(strlen(str)));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, static_cast<int>(strlen(str)));
    };

    textOut(tx, ty, "Unclaimed Kill - Score Added: ", 0x00BFBFBF);
    SIZE sz;
    GetTextExtentPoint32(hdcCMain, "Unclaimed Kill - Score Added: ", 31, &sz);
    tx += sz.cx;
    sprintf_s(t, sizeof(t), "%d", ScoreDisp);
    textOut(tx, ty, t, 0x0000BFBF);

    // Mark dirty: 1 line at (x+14, y+18) + shadow
    if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(x + 13, y + 17, 260, 18);

    if (oldFont) SelectObject(hdcCMain, oldFont);
    SelectObject(hdcCMain, hbmpOld);
}

void DrawSurvivalText(int x, int y)
{
    // Draw survival text onto lpVideoBuf via GDI
    if (!hdcMain || !hbmpVideoBuf || !lpVideoBuf) return;

    HBITMAP hbmpOld = reinterpret_cast<HBITMAP>(SelectObject(hdcCMain, hbmpVideoBuf));
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = nullptr;
    if (fnt_Small) oldFont = reinterpret_cast<HFONT>(SelectObject(hdcCMain, fnt_Small));

    char t[32];

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00101010);
        TextOut(hdcCMain, px + 1, py + 1, str, static_cast<int>(strlen(str)));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, static_cast<int>(strlen(str)));
    };

    int tx = x + 40;
    int ty = y + 98;
    textOut(tx, ty, "Waves Survived: ", 0x00BFBFBF);
    SIZE sz;
    GetTextExtentPoint32(hdcCMain, "Waves Survived: ", 16, &sz);
    tx += sz.cx;
    sprintf_s(t, sizeof(t), "%i", SurvivalWave - 1);
    textOut(tx, ty, t, 0x0000BFBF);

    tx = x + 40;
    ty = y + 124;
    textOut(tx, ty, "High Score: ", 0x00BFBFBF);
    GetTextExtentPoint32(hdcCMain, "High Score: ", 12, &sz);
    tx += sz.cx;
    sprintf_s(t, sizeof(t), "%i", TrophyRoom2.survivalHighScore);
    textOut(tx, ty, t, 0x0000BFBF);

    // Mark dirty: 2 lines at (x+40, y+98) and (x+40, y+124) + shadow
    if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(x + 39, y + 97, 180, 45);

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
    // 5-bit values (0..31) for 555 format: G at bits 5-9, R at bits 10-14
    int G = (MyHealth * 31 / 100000);              // green, 0..31
    int R = ((100000 - MyHealth) * 31 / 100000);   // red,   0..31
    int HCOLOR = (G << 5) | (R << 10);             // 555 packed

    int L0 = (L * MyHealth) / 100000;
    int H  = WinH / 200;
    if (H < 1) H = 1;

    if (x0 < 1 || x0 + L >= WinW || y0 < 1 || y0 + H + 1 >= WinH) return;

    const WORD BORDER = 0x0001; // non-zero so the overlay treats it as opaque

    // Top and bottom border rows (full width of bar + corners)
    FillMemory(static_cast<WORD*>(lpVideoBuf) + ((y0 - 1) * VideoPitch) + x0 - 1, (L + 2) * 2, BORDER);
    FillMemory(static_cast<WORD*>(lpVideoBuf) + ((y0 + H + 1) * VideoPitch) + x0 - 1, (L + 2) * 2, BORDER);

    // Bar body
    for (int y = 0; y <= H; y++) {
        WORD* row = static_cast<WORD*>(lpVideoBuf) + ((y0 + y) * VideoPitch);
        row[x0 - 1] = BORDER;
        row[x0 + L] = BORDER;
        for (int x = 0; x < L0; x++)
            row[x0 + x] = static_cast<WORD>(HCOLOR);
    }

    // Mark dirty: health bar rect including 1px border on all sides
    if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(x0 - 1, y0 - 1, L + 2, H + 3);
}

void ShowControlElements()
{
    if (!hdcMain || !hbmpVideoBuf || !lpVideoBuf) return;

    char buf[128];

    // Draw text elements onto lpVideoBuf via GDI
    HBITMAP hbmpOld = reinterpret_cast<HBITMAP>(SelectObject(hdcCMain, hbmpVideoBuf));
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = nullptr;
    if (fnt_Small) oldFont = reinterpret_cast<HFONT>(SelectObject(hdcCMain, fnt_Small));

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00101010);
        TextOut(hdcCMain, px + 1, py + 1, str, static_cast<int>(strlen(str)));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, static_cast<int>(strlen(str)));
    };

    if (TIMER)
    {
        sprintf_s(buf, sizeof(buf), "msc: %d", TimeDt);
        textOut(WinEX - 81, 11, buf, 0x0020A0A0);
        sprintf_s(buf, sizeof(buf), "polys: %d", dFacesCount);
        textOut(WinEX - 90, 24, buf, 0x0020A0A0);
        // 2 lines of timer text near top-left of extended area
        if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(WinEX - 91, 9, 100, 32);
    }

    if (MessageList.timeleft)
    {
        if (RealTime > MessageList.timeleft) MessageList.timeleft = 0;
        textOut(10, 10, MessageList.mtext, 0x0020A0A0);
        // Variable-length message, generous estimate
        if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(8, 8, 500, 20);
    }

    // ── Underwater fog debug menu ─────────────────────────────────
    if (UnderwaterDebugMenu && IsUnderwater())
    {
        int dx = 10;
        int dy = 40;
        int lineH = 16;
        int selected = UnderwaterDebugSelected;

        // Background hint
        textOut(dx, dy, "=== UNDERWATER FOG DEBUG (F10=close, D=dump) ===", 0x00FFFFFF);
        dy += lineH + 4;

        // Parameter list
        const char* names[] = { "BaseDensity", "CamDepthMult", "VertRange", "VertStrength", "CurveExp", "CapBase", "CapCamBoost" };
        float values[] = { UWFog_BaseDensityMult, UWFog_CameraDepthMult, UWFog_VertRange, UWFog_VertStrength, UWFog_CurveExp, UWFog_CapBase, UWFog_CapCameraBoost };
        const char* descs[] = { "(1.0=normal)", "(Beer-Lambert)", "(units)", "(fog units)", "(exponent)", "(added to FLimit)", "(camera boost)" };

        for (int i = 0; i < 7; i++)
        {
            char line[128];
            sprintf_s(line, sizeof(line), "%s = %.2f  %s", names[i], values[i], descs[i]);
            int color = (i == selected) ? 0x0000FFFF : 0x00C0C0C0;  // yellow if selected
            if (i == selected) {
                // Draw selection indicator
                char selLine[132];
                sprintf_s(selLine, sizeof(selLine), "> %s", line);
                textOut(dx, dy, selLine, color);
            } else {
                textOut(dx + 10, dy, line, color);
            }
            dy += lineH;
        }

        dy += 4;
        textOut(dx, dy, "Arrows: select +/-: adjust", 0x00808080);

        if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(dx - 2, 38, 450, 200);
    }

    if (ExitTime)
    {
        int yline = WinH / 3;
        sprintf_s(buf, sizeof(buf), "Preparing for evacuation...");
        textOut(VideoCX - GetTextW(hdcMain, buf) / 2, yline, buf, 0x0060C0D0);
        sprintf_s(buf, sizeof(buf), "%d seconds left.", 1 + ExitTime / 1000);
        textOut(VideoCX - GetTextW(hdcMain, buf) / 2, yline + 18, buf, 0x0060C0D0);
        // 2 lines centered, ~300px wide
        if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(VideoCX - 150, yline - 1, 300, 36);
    }

    if (WaveNoteTime)
    {
        int yline = WinH / 3;
        sprintf_s(buf, sizeof(buf), "Waves Survived: %i", SurvivalWave - 1);
        textOut(VideoCX - GetTextW(hdcMain, buf) / 2, yline, buf, 0x0060C0D0);
        // 1 line centered
        if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(VideoCX - 120, yline - 1, 240, 18);
    }

    if (oldFont) SelectObject(hdcCMain, oldFont);
    SelectObject(hdcCMain, hbmpOld);

    // Health bar is drawn into lpVideoBuf after the text elements so it
    // sits on top in the overlay upload.
    RenderHealthBar();

    // Apply desaturation to the 3D scene BEFORE the HUD overlay,
    // so the HUD (health bar, text) stays at full color.
    if (OptDayNight == 2 && !NightVisionOn && g_GLRenderer) {
        g_GLRenderer->RenderSceneDesaturated();
    }

    // Upload lpVideoBuf overlay to GL (composited on top of the desaturated scene)
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
