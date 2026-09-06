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
