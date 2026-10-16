#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Shared by the menu and the render EXE: both enable per-monitor-v2 DPI
// awareness before creating their windows, through the runtime-resolved
// SetProcessDpiAwarenessContext entry point. Old SDKs lack the context
// constant and pre-1607 Windows lacks the function, so both are guarded.

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

inline void EnablePerMonitorV2DpiAwareness()
{
	using SetProcessDpiAwarenessContextProc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
	HMODULE user32 = GetModuleHandleA("user32.dll");
	if (!user32) {
		return;
	}
	auto setProcessDpiAwarenessContext =
		reinterpret_cast<SetProcessDpiAwarenessContextProc>(
			GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

	if (setProcessDpiAwarenessContext) {
		setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}
}
