// menu_resource_seams.cpp -- narrow test doubles for the production menu
// resource readers.
//
// The menu resource translation units reference exactly these globals and
// ShowErrorMessage();
// defining them here lets the real translation unit link into a test without
// creating a menu window, a profile, or any Win32 state. Keep this list to the
// symbols the readers actually touch: growing it means the test is drifting
// away from a narrow seam.

#include "Hunt.h"

std::vector<DinoInfo> g_DinoInfo;
std::vector<WeapInfo> g_WeapInfo;
std::vector<AreaInfo> g_AreaInfo;
std::vector<UtilInfo> g_UtilInfo;
std::map<std::string, float> g_AccessoryScoreMods;
std::map<std::string, std::string> g_AccessoryIcons;
std::map<std::string, std::string> g_AccessoryDescriptions;
std::map<std::string, int32_t> g_AccessoryPriceOverrides;
std::vector<int32_t> g_AccessoryPrices;
UtilInfo g_ObserverInfo;
Options g_Options;
SavedHuntSetup g_SavedHunt;
uint32_t g_ProfileIndex = 0;
ProfileShort g_Profiles[10];
uint32_t g_StartCredits = 0;
int g_ResCount = 0;
TRes g_ResolutionList[resolutions::kMaxResolutions];
SoundFX g_MenuSound_Go;
SoundFX g_MenuSound_Ambient;
SoundFX g_MenuSound_Move;
SoundFX g_MenuSound_Type;
SoundFX g_MenuSound_TypeGo;

void ShowErrorMessage(const std::string&)
{
}
