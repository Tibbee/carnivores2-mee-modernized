#pragma once

// Unarmed sessions still pass through shared HUD/underwater metadata reads.
// Slot zero is an in-bounds metadata placeholder, not an equipped weapon:
// WeaponPres stays empty and ReInitGame keeps Weapon.state hidden (zero).
// Preserve -1 for an ordinary hunt so its missing-weapon guard still fires.
inline int ResolveInitialWeaponIndex(int selectedWeapon, bool trophyRoom,
                                     bool observerMode)
{
    if (selectedWeapon == -1 && (trophyRoom || observerMode))
        return 0;
    return selectedWeapon;
}
