// ==========================================================================
// UIText.cpp - see UIText.h for why the box text needs measuring, not
// just multiplying by the HUD scale.
// ==========================================================================

#include "Hunt.h"
#include "Renderer/UIText.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uitxt {

namespace {

// Below this the text stops being legible, so it is better to overflow the art
// slightly than to keep shrinking.
constexpr int kMinFontPx = 6;

// The stock font was created as CreateFont(16, 7, ...) - see EngineInit.cpp.
constexpr int kBaseFontPx  = 16;
constexpr int kBaseFontWidth = 7;

// One cached font at a time: the HUD draws a handful of boxes per frame and
// they all settle on the same size, so a single-entry cache avoids both the
// CreateFont churn and a font table.
HFONT FontFor(int px)
{
    static HFONT cached = nullptr;
    static int   cachedPx = -1;

    if (cached && cachedPx == px) return cached;

    if (cached) {
        DeleteObject(cached);
        cached = nullptr;
    }

    const int width = static_cast<int>(
        std::lround(static_cast<double>(px) * kBaseFontWidth / kBaseFontPx));

    cached = CreateFont(
        px, width, 0, 0,
        100, 0, 0, 0,
#ifdef __rus
        RUSSIAN_CHARSET,
#else
        ANSI_CHARSET,
#endif
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, nullptr);
    cachedPx = px;

    return cached;
}

int TextWidth(HDC hdc, const char* s)
{
    if (!s) return 0;
    SIZE sz;
    if (!GetTextExtentPoint32(hdc, s, static_cast<int>(strlen(s)), &sz)) return 0;
    return sz.cx;
}

int RowWidth(HDC hdc, const Row& row, int gap)
{
    int total = 0;
    for (int i = 0; i < row.count; i++) {
        total += TextWidth(hdc, row.seg[i].text);
        if (i + 1 < row.count) total += gap;
    }
    return total;
}

int WidestRow(HDC hdc, const Row* rows, int rowCount, int gap)
{
    int widest = 0;
    for (int i = 0; i < rowCount; i++) {
        widest = (std::max)(widest, RowWidth(hdc, rows[i], gap));
    }
    return widest;
}

} // namespace

float Scale()
{
    return static_cast<float>(WinH) / 600.0f * UIScale;
}

int Px(int artPixels)
{
    return static_cast<int>(std::lround(static_cast<double>(artPixels) * Scale()));
}

int DrawBox(HDC hdc, int x, int y,
            int padX, int padY, int step,
            int maxW, int maxH,
            const Row* rows, int rowCount)
{
    if (!hdc || !rows || rowCount <= 0) return 0;

    const float s = Scale();
    if (s <= 0.0f) return 0;

    const int padXpx = Px(padX);
    const int padYpx = Px(padY);
    const int stepPx = Px(step);
    const int availW = Px(maxW);
    const int availH = Px(maxH);
    if (availW <= 0 || availH <= 0) return 0;

    const int nominal = (std::max)(kMinFontPx, Px(kBaseFontPx));

    // Shrink from the resolution-scaled size until the widest row fits the
    // panel width and all rows fit its height. Width is re-measured each pass
    // because it does not scale linearly with the font height.
    int px = nominal;
    for (int iter = 0; iter < 6; iter++) {
        const int gap = (std::max)(1, px / 2);

        HGDIOBJ oldFont = SelectObject(hdc, FontFor(px));
        const int widest = WidestRow(hdc, rows, rowCount, gap);
        SelectObject(hdc, oldFont);

        const int byWidth = (widest > 0) ? MulDiv(px, availW, widest) : px;

        // Block height ~= (rowCount-1) * step*(px/nominal) + px, so the tallest
        // font that still fits solves to availH * nominal / ((rows-1)*step + nominal).
        const int denom = (rowCount - 1) * stepPx + nominal;
        const int byHeight = (denom > 0) ? MulDiv(availH, nominal, denom) : px;

        int target = (std::min)(px, (std::min)(byWidth, byHeight));
        if (target < kMinFontPx) target = kMinFontPx;
        if (target >= px) break;

        px = target;
    }

    const int gap = (std::max)(1, px / 2);
    const int lineStep = (std::max)(px, MulDiv(stepPx, px, nominal));

    HGDIOBJ oldFont = SelectObject(hdc, FontFor(px));
    const int oldBk = SetBkMode(hdc, TRANSPARENT);

    for (int r = 0; r < rowCount; r++) {
        const Row& row = rows[r];
        int cx = x + padXpx;
        const int cy = y + padYpx + r * lineStep;

        for (int i = 0; i < row.count; i++) {
            const Seg& seg = row.seg[i];
            if (seg.text && seg.text[0]) {
                const int len = static_cast<int>(strlen(seg.text));
                SetTextColor(hdc, 0x00101010);
                TextOut(hdc, cx + 1, cy + 1, seg.text, len);
                SetTextColor(hdc, seg.color);
                TextOut(hdc, cx, cy, seg.text, len);
                cx += TextWidth(hdc, seg.text);
            }
            if (i + 1 < row.count) cx += gap;
        }
    }

    SetBkMode(hdc, oldBk);
    SelectObject(hdc, oldFont);

    return px;
}

} // namespace uitxt
