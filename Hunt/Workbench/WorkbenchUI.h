#pragma once
#include <windows.h>
#include "InputBoundary.h"

namespace workbench {
// OpenGL-only shell. CPU/session state survives renderer recreation; GPU and
// Win32 backend objects are detached before the owning GL context is deleted.
bool InitializeUI(HWND window, void (*inputChanged)(bool ownsInput), void (*dumpSink)(char*));
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
