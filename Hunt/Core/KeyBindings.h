#pragma once
#include <windows.h>

// WM_KEYDOWN uses generic modifier VKs; saved defaults may name a side.
// Keep generic bindings working for either side without rewriting user saves.
inline bool KeyDownMatches(int binding, unsigned int key, unsigned int keyData)
{
    if (binding <= 0 || binding > 255) return false;
    if (binding == static_cast<int>(key)) return true;

    unsigned int sidedKey = key;
    if (key == VK_SHIFT)
        sidedKey = MapVirtualKey((keyData >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX);
    else if (key == VK_CONTROL)
        sidedKey = (keyData & (1u << 24)) ? VK_RCONTROL : VK_LCONTROL;
    else if (key == VK_MENU)
        sidedKey = (keyData & (1u << 24)) ? VK_RMENU : VK_LMENU;
    return binding == static_cast<int>(sidedKey);
}

inline bool IsInitialKeyDown(unsigned int keyData)
{
    return (keyData & (1u << 30)) == 0;
}

// Map a virtual-key code to the scan code used by the key-name tables.
// The mouse buttons have no MapVirtualKey code, so they keep the legacy
// 124..129 range that the menu's key display expects.
inline int MapVKKey(int k)
{
    if (k == VK_LBUTTON) return 124;
    if (k == VK_RBUTTON) return 125;
    if (k == VK_MBUTTON) return 126;
    if (k == VK_XBUTTON1) return 128;
    if (k == VK_XBUTTON2) return 129;
    return MapVirtualKey(k, MAPVK_VK_TO_VSC);
}
