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

static void AddResolution(int w, int h)
{
	// Append (w, h) to g_ResolutionList[] if not already present.
	for (int r = 0; r < g_ResCount; r++) {
		if (g_ResolutionList[r].w == w && g_ResolutionList[r].h == h)
			return;
	}
	if (g_ResCount >= 128) return;
	g_ResolutionList[g_ResCount].w = w;
	g_ResolutionList[g_ResCount].h = h;
	g_ResCount++;
}

void EnumerateResolutions()
{
	// Populate g_ResolutionList[] from the display's available modes.
	// Replaces the old hardcoded 8-entry table. 16-bit minimum (matches
	// the DIB depth the render exe uses).
	//
	// Top cap: the current desktop mode (ENUM_CURRENT_SETTINGS). This is
	// the monitor's active resolution. We always include it explicitly
	// even if the driver doesn't report it through the enumeration loop,
	// so a 2560x1440 native panel always has its native mode selectable.
	// We never offer modes wider/taller than the desktop because
	// SetVideoMode() in the render exe can't actually display them.
	g_ResCount = 0;

	int desktopW = GetSystemMetrics(SM_CXSCREEN);
	int desktopH = GetSystemMetrics(SM_CYSCREEN);

	DEVMODE current;
	ZeroMemory(&current, sizeof(current));
	current.dmSize = sizeof(current);
	if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &current)) {
		// Prefer the DEVMODE values — they can be slightly different
		// from GetSystemMetrics in multi-monitor / DPI-scaled setups.
		desktopW = current.dmPelsWidth;
		desktopH = current.dmPelsHeight;
	}

	DEVMODE dm;
	ZeroMemory(&dm, sizeof(dm));
	dm.dmSize = sizeof(dm);
	for (int i = 0; EnumDisplaySettings(nullptr, i, &dm); i++) {
		if (dm.dmBitsPerPel < 16) continue;
		if (dm.dmPelsWidth  > desktopW ||
			dm.dmPelsHeight > desktopH)
			continue;
		AddResolution(dm.dmPelsWidth, dm.dmPelsHeight);
	}

	// Always include the current desktop resolution itself. Some drivers
	// don't enumerate the native panel mode, so without this the menu
	// would silently cap below the monitor's actual capability.
	AddResolution(desktopW, desktopH);

	// Guarantee at least one entry: 800x600 (the historical default).
	if (g_ResCount == 0) {
		g_ResolutionList[0].w = 800;
		g_ResolutionList[0].h = 600;
		g_ResCount = 1;
	}
}
