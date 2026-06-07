// Resolutions.cpp
// Runtime resolution list for the standalone menu executable.
//
// The menu and the render exe are two separate binaries, so this is a
// copy of Hunt/Game.cpp's EnumerateResolutions(). Keep them in sync if
// the rules ever diverge (e.g., adding a `resmode=...` filter from
// _MENU.TXT). The duplication is intentional: the menu does not link
// against the render exe's code.

#include "Hunt.h"

TRes g_ResolutionList[128];
int  g_ResCount = 0;

void EnumerateResolutions()
{
	// Populate g_ResolutionList[] from the display's available modes.
	// Replaces the old hardcoded 8-entry table. 16-bit minimum (matches
	// the DIB depth the render exe uses). Modes wider/taller than the
	// current desktop are skipped: SetVideoMode() in the render exe
	// can't actually display them, and offering them in the menu just
	// leads to silent failure.
	g_ResCount = 0;
	DEVMODE dm;
	ZeroMemory(&dm, sizeof(dm));
	dm.dmSize = sizeof(dm);
	for (int i = 0; EnumDisplaySettings(NULL, i, &dm); i++) {
		if (dm.dmBitsPerPel < 16) continue;
		if (dm.dmPelsWidth  > GetSystemMetrics(SM_CXSCREEN) ||
			dm.dmPelsHeight > GetSystemMetrics(SM_CYSCREEN))
			continue;

		BOOL found = FALSE;
		for (int r = 0; r < g_ResCount; r++) {
			if (g_ResolutionList[r].w == dm.dmPelsWidth &&
				g_ResolutionList[r].h == dm.dmPelsHeight) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			g_ResolutionList[g_ResCount].w = dm.dmPelsWidth;
			g_ResolutionList[g_ResCount].h = dm.dmPelsHeight;
			g_ResCount++;
			if (g_ResCount >= 128) break;
		}
	}
	// Guarantee at least one entry: 800x600 (the historical default).
	if (g_ResCount == 0) {
		g_ResolutionList[0].w = 800;
		g_ResolutionList[0].h = 600;
		g_ResCount = 1;
	}
}
