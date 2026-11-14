#include <gtest/gtest.h>
#include "GameMode.h"
#include "Core/WeaponSelection.h"
#include "../Menu/LaunchArgs.h"
#include <cstring>

GameMode g_GameMode = GameMode::Normal;
char ProjectName[128] = {};

TEST(TrophyRoom, SessionSurvivesMovementAndOverlayModes)
{
    std::strcpy(ProjectName, "huntdat/areas/trophy");
    for (GameMode mode : {GameMode::Normal, GameMode::Swimming,
                         GameMode::Binocular, GameMode::MapMode,
                         GameMode::Paused, GameMode::ExitCountdown}) {
        g_GameMode = mode;
        EXPECT_TRUE(InTrophyRoom());
        EXPECT_FALSE(CanUseMap());
    }
    g_GameMode = GameMode::Normal;
    ProjectName[0] = 0;
}

TEST(TrophyRoom, NormalHuntsDoNotEnableRoomInteractions)
{
    std::strcpy(ProjectName, "huntdat/areas/area1");
    g_GameMode = GameMode::Normal;
    EXPECT_FALSE(InTrophyRoom());
    EXPECT_TRUE(CanUseMap());
    g_GameMode = GameMode::TrophyMode;
    EXPECT_TRUE(InTrophyRoom()); // retain explicit legacy mode support
    EXPECT_FALSE(CanUseMap());
    g_GameMode = GameMode::Normal;
    ProjectName[0] = 0;
}

TEST(TrophyRoom, MenuLaunchWithoutWeaponHasSafeHiddenWeaponIndex)
{
    HuntLaunchRequest request;
    request.projectName = "trophy";
    request.timeOfDay = 1;
    std::string arguments;
    ASSERT_TRUE(BuildHuntLaunchArguments(request, arguments));
    EXPECT_NE(arguments.find("wep=0"), std::string::npos);
    EXPECT_EQ(request.weaponFlags, 0);

    std::strcpy(ProjectName, "huntdat/areas/trophy");
    // ReInitGame and movement can clear TrophyMode; room identity survives.
    for (GameMode mode : {GameMode::TrophyMode, GameMode::Normal,
                          GameMode::Swimming, GameMode::Binocular}) {
        g_GameMode = mode;
        EXPECT_EQ(ResolveInitialWeaponIndex(-1, InTrophyRoom(), false), 0);
    }
    g_GameMode = GameMode::Normal;
    ProjectName[0] = 0;
}

TEST(TrophyRoom, EmptySelectionPolicyDistinguishesObserverHuntAndSurvival)
{
    std::strcpy(ProjectName, "huntdat/areas/area1");
    g_GameMode = GameMode::Normal;
    EXPECT_EQ(ResolveInitialWeaponIndex(-1, InTrophyRoom(), true), 0);
    EXPECT_EQ(ResolveInitialWeaponIndex(-2, true, true), -2);
    EXPECT_EQ(ResolveInitialWeaponIndex(-1, InTrophyRoom(), false), -1);
    g_GameMode = GameMode::SurvivalMode;
    EXPECT_EQ(ResolveInitialWeaponIndex(-1, InTrophyRoom(), false), -1);
    for (int selected = 0; selected < 10; ++selected) {
        EXPECT_EQ(ResolveInitialWeaponIndex(selected, false, false), selected);
        EXPECT_EQ(ResolveInitialWeaponIndex(selected, true, false), selected);
        EXPECT_EQ(ResolveInitialWeaponIndex(selected, false, true), selected);
    }
    g_GameMode = GameMode::Normal;
    ProjectName[0] = 0;
}
