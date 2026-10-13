#pragma once
#include <windows.h>
#include "InputBoundary.h"

namespace workbench {
// OpenGL-only shell. CPU/session state survives renderer recreation; GPU and
// Win32 backend objects are detached before the owning GL context is deleted.
bool InitializeUI(HWND window, void (*inputChanged)(bool ownsInput), void (*dumpSink)(char*));
// Where config.cfg lives, resolved by the engine so a save lands on the file the
// loader reads. Empty means persistence is unavailable and the panel says so.
void SetConfigPath(const char* path);
void DetachUI();
void ShutdownUI();
void RenderUI();
bool HandleUIMessage(HWND window, UINT message, WPARAM key, LPARAM data, bool huntReady);
void NotifyUIFocus(bool focused);
bool UIAvailable();
bool UIOpen();
bool UIOwnsInput();
bool UIBlocksLook();
void FilterGameplayKeys(unsigned char* keys);
void FinishGameplayInputFrame();
#ifdef C2_WORKBENCH_UI_PROBE
// Test-only semantic hit rectangles; no OS input injection or game-side hooks.
void SetUIProbe(void (*observer)(const char*, int, float, float, float, float));
#endif
} // namespace workbench
