#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Shared by the menu and the render EXE: both publish the same runtime
// video-mode list, but the two binaries do not link each other's code, so
// each keeps its own storage (g_ResolutionList[] / ResolutionList[]) and
// builds it with the rules below. Historically the menu held a copy of the
// engine's enumerator; this header is that copy promoted to one owner.

struct TRes
{
	int w, h;
};

namespace resolutions {

// Capacity of g_ResolutionList[] / ResolutionList[] (RES_MAX in Menu/Hunt.h
// documents the same number for the legacy enum).
inline constexpr int kMaxResolutions = 128;

// Append (w, h) to the list if it is not already present.
inline void Add(TRes* list, int& count, int w, int h)
{
	for (int r = 0; r < count; r++) {
		if (list[r].w == w && list[r].h == h)
			return;
	}
	if (count >= kMaxResolutions) return;
	list[count].w = w;
	list[count].h = h;
	count++;
}

// Populate the list from the display's available modes and set `count`.
// Replaces the old hardcoded 8-entry table. 16-bit minimum (matches the
// DIB depth in CreateVideoDIB).
//
// Top cap: the current desktop mode (ENUM_CURRENT_SETTINGS). This is the
// monitor's active resolution. We always include it explicitly even if the
// driver doesn't report it through the enumeration loop, so a 2560x1440
// native panel always has its native mode selectable. We never offer modes
// wider/taller than the desktop because SetVideoMode() can't actually
// display them.
inline void Enumerate(TRes* list, int& count)
{
	count = 0;

	int desktopW = GetSystemMetrics(SM_CXSCREEN);
	int desktopH = GetSystemMetrics(SM_CYSCREEN);

	DEVMODE current;
	ZeroMemory(&current, sizeof(current));
	current.dmSize = sizeof(current);
	if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &current)) {
		// Prefer the DEVMODE values -- they can be slightly different from
		// GetSystemMetrics in multi-monitor / DPI-scaled setups.
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
		Add(list, count, dm.dmPelsWidth, dm.dmPelsHeight);
	}

	// Always include the current desktop resolution itself. Some drivers
	// don't enumerate the native panel mode, so without this the list
	// would silently cap below the monitor's actual capability.
	Add(list, count, desktopW, desktopH);

	// Guarantee at least one entry: 800x600 (the historical default).
	if (count == 0) {
		list[0].w = 800;
		list[0].h = 600;
		count = 1;
	}
}

} // namespace resolutions
