#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Shared by the menu EXE and both renderers: every on-screen text pass draws
// the same 1px drop shadow, but each copy spelled the SetTextColor / TextOut
// pair out itself. This is the one rule -- transparent background, shadow
// first at (+1, +1), then the main pass at the exact position.
//
// The shadow colour is a parameter because two spellings exist in the tree:
// the menu, the GL life-info HUD and the soft HUD use pure black; the GL
// control overlay and the box text in UIText.cpp use the near-black
// 0x00101010.

namespace shadowtext {

inline void Draw(HDC hdc, int x, int y, const char* text, int len,
                 COLORREF color, COLORREF shadow = RGB(0, 0, 0))
{
	if (!hdc || !text || len <= 0) return;
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, shadow);
	TextOutA(hdc, x + 1, y + 1, text, len);
	SetTextColor(hdc, color);
	TextOutA(hdc, x, y, text, len);
}

} // namespace shadowtext
