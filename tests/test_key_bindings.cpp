#include <gtest/gtest.h>
#include "KeyBindings.h"

TEST(KeyBindings, SavedLeftShiftDefaultMatchesWindowsKeyMessage)
{
    EXPECT_TRUE(KeyDownMatches(VK_LSHIFT, VK_SHIFT, 0x002A0001));
    EXPECT_FALSE(KeyDownMatches(VK_LSHIFT, VK_SHIFT, 0x00360001));
    EXPECT_TRUE(KeyDownMatches(VK_RSHIFT, VK_SHIFT, 0x00360001));
    EXPECT_FALSE(KeyDownMatches(VK_RSHIFT, VK_SHIFT, 0x002A0001));
}

TEST(KeyBindings, ReboundGenericModifiersStillMatchEitherSide)
{
    EXPECT_TRUE(KeyDownMatches(VK_SHIFT, VK_SHIFT, 0x002A0001));
    EXPECT_TRUE(KeyDownMatches(VK_SHIFT, VK_SHIFT, 0x00360001));
    EXPECT_TRUE(KeyDownMatches(VK_CONTROL, VK_CONTROL, 0x011D0001));
    EXPECT_TRUE(KeyDownMatches(VK_MENU, VK_MENU, 0x01380001));
}

TEST(KeyBindings, ControlAndAltRespectExtendedSide)
{
    EXPECT_TRUE(KeyDownMatches(VK_LCONTROL, VK_CONTROL, 0x001D0001));
    EXPECT_FALSE(KeyDownMatches(VK_RCONTROL, VK_CONTROL, 0x001D0001));
    EXPECT_TRUE(KeyDownMatches(VK_RCONTROL, VK_CONTROL, 0x011D0001));
    EXPECT_FALSE(KeyDownMatches(VK_LCONTROL, VK_CONTROL, 0x011D0001));
    EXPECT_TRUE(KeyDownMatches(VK_LMENU, VK_MENU, 0x00380001));
    EXPECT_FALSE(KeyDownMatches(VK_RMENU, VK_MENU, 0x00380001));
    EXPECT_TRUE(KeyDownMatches(VK_RMENU, VK_MENU, 0x01380001));
    EXPECT_FALSE(KeyDownMatches(VK_LMENU, VK_MENU, 0x01380001));
}

TEST(KeyBindings, OrdinaryAndUnassignedBindings)
{
    EXPECT_TRUE(KeyDownMatches('R', 'R', 1));
    EXPECT_FALSE(KeyDownMatches('R', 'W', 1));
    EXPECT_FALSE(KeyDownMatches(0, 0, 1));
    EXPECT_FALSE(KeyDownMatches(-1, VK_SHIFT, 0x002A0001));
    EXPECT_FALSE(KeyDownMatches(256, 'R', 1));
}

TEST(KeyBindings, HoldingToggleDoesNotRetrigger)
{
    EXPECT_TRUE(IsInitialKeyDown(0x002A0001));
    EXPECT_FALSE(IsInitialKeyDown(0x402A0001));
    EXPECT_TRUE(IsInitialKeyDown(0x21380001)); // Alt context bit, first press
    EXPECT_FALSE(IsInitialKeyDown(0x61380001));
}
