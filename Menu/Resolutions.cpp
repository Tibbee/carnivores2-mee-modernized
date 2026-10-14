// Resolutions.cpp
// Runtime resolution list storage for the standalone menu executable.
//
// The enumeration rules (16-bit minimum, desktop-mode cap, dedupe, 800x600
// fallback) are shared with the render exe in Core/ResolutionList.h; each
// binary keeps its own list because they do not link each other's code.

#include "Hunt.h"
#include "Core/ResolutionList.h"

TRes g_ResolutionList[resolutions::kMaxResolutions];
int  g_ResCount = 0;

void EnumerateResolutions()
{
	resolutions::Enumerate(g_ResolutionList, g_ResCount);
}
