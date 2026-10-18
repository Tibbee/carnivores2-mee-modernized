// MenuInput.cpp -- per-screen menu input: events, key capture, launch paths
// (split from Menu.cpp)
// ==========================================================================
// Owns the per-frame input handling: menu event start/input, keyboard
// capture, mouse scroll, the registry/hunt confirm paths and the renderer
// launch-argument assembly.
// ==========================================================================

#include "Hunt.h"
#include "MenuInternal.h"
#include "LaunchArgs.h"
#include "ListMath.h"
#include "SliderMath.h"
#include "ShopSafety.h"
#include "../Hunt/Core/KeyBindings.h"
#include "../Hunt/Core/ScoreMod.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

// Registry name-list double-click tracking (double-click a name to enter).
static DWORD g_LastListClickTime = 0;
static int g_LastListClickIndex = -1;

// TKeyMap is a packed block of int32_t fields persisted verbatim in trophy
// profiles. Address fields through this table instead of casting the object
// address; the table must stay in TKeyMap declaration order.
using KeyMapMember = int32_t TKeyMap::*;
static constexpr KeyMapMember kKeyMapMembers[] = {
	&TKeyMap::fkForward,
	&TKeyMap::fkBackward,
	&TKeyMap::fkReload,
	&TKeyMap::fkResupply,
	&TKeyMap::fkHoldBreath,
	&TKeyMap::fkFiringMode,
	&TKeyMap::fkFire,
	&TKeyMap::fkShow,
	&TKeyMap::fkSLeft,
	&TKeyMap::fkSRight,
	&TKeyMap::fkStrafe,
	&TKeyMap::fkJump,
	&TKeyMap::fkRun,
	&TKeyMap::fkCrouch,
	&TKeyMap::fkCall,
	&TKeyMap::fkCCall,
	&TKeyMap::fkBinoc,
#ifdef _iceage
	&TKeyMap::fkSupply,
#endif
};
static constexpr int kKeyMapFieldCount = sizeof(kKeyMapMembers) / sizeof(kKeyMapMembers[0]);
static_assert(sizeof(TKeyMap) == kKeyMapFieldCount * sizeof(int32_t),
	"TKeyMap must remain a packed int32_t block for profile compatibility");

static int32_t& KeyMapField(int index)
{
	return g_Options.KeyMap.*kKeyMapMembers[index];
}

static void ClearConflictingBinding(int key)
{
	for (int i = 0; i < kKeyMapFieldCount; i++)
		if (KeyMapField(i) == key)
			KeyMapField(i) = 0;
}

// Append the flag matching the menu's Display Mode. Borderless is the GL
// default because DWM-composed fullscreen keeps Windows 11 color management
// active on wide-gamut displays; the software renderer maps it to exclusive
// fullscreen (its classic behavior).
static void AppendDisplayModeLaunchFlag(std::stringstream& params)
{
	params << DisplayModeLaunchFlag(g_Options.DisplayMode,
		NormalizeMenuRenderAPI(g_Options.RenderAPI) == kRenderAPI_OpenGL);
}

void WaitForMouseRelease()
{
	while (GetAsyncKeyState(VK_RBUTTON) & 0x80000000);
	while (GetAsyncKeyState(VK_MBUTTON) & 0x80000000);
	while (GetAsyncKeyState(VK_LBUTTON) & 0x80000000);
}

void AcceptNewKey()
{
	uint8_t keystate[256];

	if (GetKeyboardState(keystate)) {

		if (keystate[VK_ESCAPE] & 128)
		{
			// Clearing a binding: KeyMap rows zero their own field, the two
			// equipment toggle rows clear their dedicated Options members.
			if (g_WaitKey == ControlsRowIndex(ControlsRow::NightVision)) {
				g_Options.NightVisionKey = 0;
			} else if (g_WaitKey == ControlsRowIndex(ControlsRow::Flashlight)) {
				g_Options.FlashlightKey = 0;
			} else {
				assert(g_WaitKey >= 0 && g_WaitKey < kKeyMapFieldCount);
				KeyMapField(g_WaitKey) = 0;
			}
			g_WaitKey = -2;
			return;
		}
		else
		{
			for (int k = 0; k < 255; k++)
			{
				if (keystate[k] & 128)
				{
					// Equipment toggles live outside Options::KeyMap, so they are
					// independent of the movement bindings and only need to avoid
					// colliding with their own slot.
					if (g_WaitKey == ControlsRowIndex(ControlsRow::NightVision)) {
						if (g_Options.NightVisionKey == k) return;
						g_Options.NightVisionKey = k;
					} else if (g_WaitKey == ControlsRowIndex(ControlsRow::Flashlight)) {
						if (g_Options.FlashlightKey == k) return;
						g_Options.FlashlightKey = k;
					} else {
						// Unassign any existing binding that uses the same key
						assert(g_WaitKey >= 0 && g_WaitKey < kKeyMapFieldCount);
						ClearConflictingBinding(k);
						KeyMapField(g_WaitKey) = k;
					}

#ifdef _DEBUG
					std::cout << "AcceptNewKey() : " << (static_cast<int>(k)) << " MapVK: " << (MapVKKey(k)) << std::endl;
#endif //_DEBUG

					WaitForMouseRelease();
					g_WaitKey = -1;
					return;
				}
			}
		}
	}
}

int MenuViewOptFromSlider(float v)
{
	int opt = kViewOptMin + static_cast<int>((v * static_cast<float>(kViewOptMax - kViewOptMin)));
	if (opt < kViewOptMin) opt = kViewOptMin;
	if (opt > kViewOptMax) opt = kViewOptMax;
	return opt;
}

int MenuObjectDetailFromSlider(float v)
{
	return DiscreteSliderValue(v, kObjectDetailMin, kObjectDetailMax, kObjectDetailStep);
}

/*
Menu screen initialization event
NOTE: This only gets called on the first tick that the menu exists, it can be used to
initialize variables and reset various things.
*/
// Capture the current hunt setup for config.cfg so the next visit to the
// hunt screen reselects it instead of resetting to defaults. Guarded: the
// MenuHunt lists only exist after the first MENU_HUNT entry.
static void SaveHuntSelections()
{
	if (MenuHunt[0].Item.empty()) return;
	g_SavedHunt.area.clear();
	if (MenuHunt[0].Selected >= 0 && MenuHunt[0].Selected < (int)g_AreaInfo.size())
		g_SavedHunt.area = g_AreaInfo[MenuHunt[0].Selected].m_ProjectName;
	auto mask = [](const MenuSet& set) {
		unsigned long long m = 0;
		for (size_t i = 0; i < set.Item.size() && i < 64; i++)
			if (set.Item[i].second) m |= 1ULL << i;
		return m;
	};
	g_SavedHunt.dinos = mask(MenuHunt[1]);
	g_SavedHunt.weapons = mask(MenuHunt[2]);
	g_SavedHunt.utils = mask(MenuHunt[3]);
	g_SavedHunt.time = g_TimeOfDay;
	g_SavedHunt.has = true;
}
// Reapply the persisted setup after the entry defaults. Everything is
// validated against the current data, so mod list changes can only fall
// back to defaults, never select out of range. Saved at launch, so a
// restored setup always corresponds to a launchable one.
static void RestoreHuntSelections()
{
	if (!g_SavedHunt.has) return;
	if (!g_SavedHunt.area.empty()) {
		for (size_t i = 0; i < g_AreaInfo.size(); i++)
			if (g_AreaInfo[i].m_ProjectName == g_SavedHunt.area) { MenuHunt[0].Selected = (int)i; break; }
	}
	auto apply = [](MenuSet& set, unsigned long long m) {
		for (size_t i = 0; i < set.Item.size() && i < 64; i++)
			set.Item[i].second = ((m >> i) & 1ULL) != 0;
	};
	apply(MenuHunt[1], g_SavedHunt.dinos);
	apply(MenuHunt[2], g_SavedHunt.weapons);
	apply(MenuHunt[3], g_SavedHunt.utils);
	if (g_SavedHunt.time >= HUNT_DAWN && g_SavedHunt.time <= HUNT_NIGHT && g_SavedHunt.time != g_TimeOfDay) {
		g_MenuItem.SetIsElementSet(g_TimeOfDay + 1, false);
		g_TimeOfDay = g_SavedHunt.time;
		g_MenuItem.SetIsElementSet(g_TimeOfDay + 1, true);
	}
	if (CalculateDebit() > static_cast<HuntBudget>(g_UserProfile.Score)) {
		// Saved setup is unaffordable here (e.g. restored onto a poorer
		// profile, or hand-edited masks) -- fall back to the entry defaults
		// above instead of displaying a negative account. Time-of-day is
		// free, so it stays as restored.
		MenuHunt[0].Selected = g_AreaInfo.empty() ? -1 : 0;
		for (auto m = 1U; m <= 3U; m++) {
			for (auto& it : MenuHunt[m].Item) it.second = false;
			if (m < 3U && !MenuHunt[m].Item.empty()) MenuHunt[m].Item[0].second = true;
		}
		// Modded prices or a low-credit profile can make even the defaults
		// unaffordable. Clear the loadout and keep only an affordable area.
		// Existing launch guards require an area and, outside observer mode,
		// at least one creature and weapon before starting a hunt.
		if (CalculateDebit() > g_UserProfile.Score) {
			for (auto m = 1U; m <= 3U; m++)
				for (auto& it : MenuHunt[m].Item) it.second = false;
			if (CalculateDebit() > g_UserProfile.Score)
				MenuHunt[0].Selected = -1;
		}
	}
}
static void ResetRegistryScreen()
{
	MenuRegistry.Offset = 0;
	MenuRegistry.Count = 0;
	MenuRegistry.AddItem("");
	g_LastListClickTime = 0;
	g_LastListClickIndex = -1;
	LoadProfileSummaries();
}

static void ResetOptionsScreen()
{
	g_WaitKey = -1;

	for (auto m = 0U; m < OPT_MAX; m++) {
		MenuOptions[m].Hilite = -1;
		MenuOptions[m].Selected = -1;
	}
}

static void BuildHuntLists()
{
	MenuHunt[0].Item.clear();
	for (auto i = 0U; i < g_AreaInfo.size(); i++)
	{
		MenuHunt[0].Item.push_back(std::make_pair(g_AreaInfo[i].m_Name, false));
	}
	MenuHunt[0].Count = g_AreaInfo.size();

	g_DinoList.clear();
	MenuHunt[1].Item.clear();
	for (auto i = 0U; i < g_DinoInfo.size(); i++)
	{
		DinoInfo& dino = g_DinoInfo.at(i);

		if (dino.m_AI >= 10)//&& dino.m_Rank <= g_UserProfile.Rank)
		{
			// Add to a list. Presentation assets follow the list position
			// (see ListMath.h); compute the slot before the push so the text
			// number matches the thumbnail loaded in ReadCharacters().
			const std::size_t menuSlot = HuntableMenuSlot(g_DinoList.size());
			g_DinoList.push_back(i);
			MenuHunt[1].Item.push_back(std::make_pair(dino.m_Name, false));

			std::stringstream spp;

			// Load appropriate text
			spp << "huntdat/menu/txt/dino" << menuSlot;
			if (g_Options.OptSys)
				spp << ".txu";
			else
				spp << ".txm";
			g_DinoInfo[i].m_Description.clear();
			LoadText(g_DinoInfo[i].m_Description, spp.str());
		}
	}
	MenuHunt[1].Count = g_DinoList.size();

	MenuHunt[2].Item.clear();
	for (auto i = 0U; i < g_WeapInfo.size(); i++)
	{
		MenuHunt[2].Item.push_back(std::make_pair(g_WeapInfo[i].m_Name, false));
		// Load appropriate text
		std::stringstream spp;
		spp << "huntdat/menu/txt/weapon" << (i + 1) << ".txt";
		g_WeapInfo[i].m_Description.clear();
		LoadText(g_WeapInfo[i].m_Description, spp.str());
	}
	MenuHunt[2].Count = g_WeapInfo.size();

	/* TODO: In future versions of the menu, support new types of accessories/utilities
	In the _RES they can have a string that defines the command line toggle to send to
	the .REN executable, such as '-camo' for camoflauge, or whatever custom accessory
	the modded .REN allows for. */
}

static void ResetHuntSelections()
{
	// Reset the states
	for (auto m = 0U; m < 4U; m++)
	{
		MenuHunt[m].Selected = -1;
		for (auto i = MenuHunt[m].Item.begin(); i != MenuHunt[m].Item.end(); i++)
		{
			i->second = false;
		}
	}

	if (!g_AreaInfo.empty())
	{
		MenuHunt[0].Selected = 0;
	}

	for (auto i = 0U; i < 3U; i++)
	{
		if (!MenuHunt[i].Item.empty())
		{
			MenuHunt[i].Item[0].second = true;
		}
	}

	RestoreHuntSelections();
	g_ScoreDebit = CalculateDebit();
}

static void ResetHuntScreen()
{
	if (g_TimeOfDay < HUNT_DAWN || g_TimeOfDay > HUNT_NIGHT)
		g_TimeOfDay = HUNT_DAY;

	g_HuntInfo.first = -1;
	g_HuntInfo.second = 0;

	for (auto i = 1U; i <= 3U; i++)
		g_MenuItem.SetIsElementSet(i, false);

	g_MenuItem.SetIsElementSet(g_TimeOfDay + 1, true);

	g_MenuItem.SetIsElementSet(4, false);
	g_MenuItem.SetIsElementSet(5, g_Options.TranqMode);
	g_MenuItem.SetIsElementSet(6, g_ObserverMode);

	for (auto i = 0U; i < 4U; i++)
	{
		MenuHunt[i].Offset = 0;
	}

	BuildHuntLists();
	ResetHuntSelections();
}

void MenuEventStart(int32_t menu_state)
{
	g_MenuItem.ResetElementSet();

	switch (menu_state) {
	case MENU_REGISTER: ResetRegistryScreen(); break;
	case MENU_OPTIONS: ResetOptionsScreen(); break;
	case MENU_MAIN: MenuAudioStartAmbient(); break;
		/****************************************************
		Hunt License Menu */
	case MENU_HUNT: ResetHuntScreen(); break;
	}
}

/*
Commit the current registry selection: load an existing profile or create
a new one from the typing buffer. Shared by the GO button, the Enter key
and the original double-click-a-name shortcut.
*/
static void ConfirmRegistrySelection()
{
	if (g_Profiles[g_ProfileIndex].m_Name.empty()) {
		if (!g_TypingBuffer.empty())
		{
			g_UserProfile.New(g_TypingBuffer);
			g_Options.Default();
			TrophySave(g_UserProfile);
			SaveConfig();
			ChangeMenuState(MENU_REGISTRY_WAIVER);
		}
		else
		{
			MessageBox(hwndMain, "You need to enter a name!", "Try again!", MB_OK | MB_ICONINFORMATION);
		}
	}
	else {
		TrophyLoad(g_UserProfile, g_ProfileIndex);
		LoadConfig();
		ChangeMenuState(MENU_MAIN);
	}
}

/*
Keyboard and Mouse handling for menus
NOTE: Could move these to individual functions if a state machine is confusing
*/
// Per-screen input handlers. Each branch used to live inline in
// MenuEventInput; they are separate functions now so the dispatcher stays
// readable and each screen's behavior can be reviewed in isolation.
static void HandleCreditsInput()
{
	if (g_KeyboardState[VK_RETURN] & 128) {
		g_KeyboardState[VK_LBUTTON] |= 128;
	}

	if (g_KeyboardState[VK_SPACE] & 128) {
		g_KeyboardState[VK_LBUTTON] |= 128;
	}

	if (g_KeyboardState[VK_LBUTTON] & 128) {
		WaitForMouseRelease();
		MenuAudioPlayClick();
		RECT rc = { 550, 42, 600 + GetTextW(hdcCMain, g_GitHubURL), 56 };

		if (IsPointInRect(g_CursorPos, rc))
		{
			ShellExecute(0, 0, TEXT(g_GitHubURL), 0, 0, SW_SHOW);
		}
		else
		{
			ChangeMenuState(MENU_MAIN);
		}
	}
}

static void HandleRegisterInput(uint8_t id)
{
	if (g_KeyboardState[VK_RETURN] & 128) {
		g_KeyboardState[VK_LBUTTON] |= 128;
		id = 1;
		MenuAudioPlayTypeGo();
	}

	if (g_KeyboardState[VK_DELETE] & 128) {
		g_KeyboardState[VK_LBUTTON] |= 128;
		id = 2;
	}

	if (g_KeyboardState[VK_LBUTTON] & 128) {
		if (id == 1) {
			MenuAudioPlayClick();
			WaitForMouseRelease();
			ConfirmRegistrySelection();
		}
		else if (id == 2) {
			// Delete the selected 'save'
			WaitForMouseRelease();
			MenuAudioPlayClick();
			ChangeMenuState(MENU_REGISTRY_DELETE);
		}
		else {
			WaitForMouseRelease();

			// Original behaviour: double-clicking a name in the list enters
			// the menu directly (same as clicking GO); a single click only
			// selects the profile.
			DWORD now = timeGetTime();
			bool inList = false;
			for (auto i = 0U; i < 7U; i++) {
				if (RegistryRowContains(g_CursorPos, i)) {
					inList = true;
					break;
				}
			}

			bool doubleClick = inList &&
				g_LastListClickIndex == g_HiliteProfileIndex &&
				(now - g_LastListClickTime) < (DWORD)GetDoubleClickTime();
			g_LastListClickTime = now;
			g_LastListClickIndex = g_HiliteProfileIndex;

			if (doubleClick) {
				g_ProfileIndex = g_HiliteProfileIndex;
				MenuAudioPlayClick();
				ConfirmRegistrySelection();
			}
			else {
				MenuAudioPlayHover();
				g_ProfileIndex = g_HiliteProfileIndex;
				g_TypingBuffer = g_Profiles[g_ProfileIndex].m_Name;
			}
		}
	}
}

static void HandleRegistryDeleteInput(uint8_t id)
{
	if (g_KeyboardState[VK_LBUTTON] & 128) {
		if (id == 1)
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();
			TrophyDelete(g_ProfileIndex); // Delete the last clicked profile
			ChangeMenuState(MENU_REGISTER);
		}
		else if (id == 2)
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();
			ChangeMenuState(MENU_REGISTER);
		}
	}
}

static void HandleRegistryWaiverInput(uint8_t id)
{
	if (g_KeyboardState[VK_LBUTTON] & 128) {
		if (id == 1)
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();
			ChangeMenuState(MENU_MAIN);
		}
		else if (id == 2)
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();
			TrophyDelete(g_ProfileIndex);
			ChangeMenuState(MENU_REGISTER);
		}
	}
}

/**************************************
* Options Menu
*/
static void UpdateOptionsHover(MenuSet& mo, Layout::Column* col)
{
	if (col && col->itemCount > 0) {
		int spacing = col->GetSpacing();
		int yd = g_CursorPos.y - col->yStart;
		if (yd >= 0 && yd < (col->yEnd - col->yStart)) {
			mo.Hilite = yd / spacing;
			if (mo.Hilite >= (int)col->itemCount)
				mo.Hilite = col->itemCount - 1;
		}
		else mo.Hilite = -1;
	}
	else mo.Hilite = -1;
}

static void ApplyGameOptionClick(MenuSet& mo)
{
	int sliderX = OptionsLayout::OPTION_SLIDER_X;
	int tbw = OptionsLayout::OPTION_SLIDER_W;
	float v = static_cast<float>((g_CursorPos.x - sliderX)) / static_cast<float>(tbw);
	if (v < 0.0f) v = 0.0f;
	if (v > 1.0f) v = 1.0f;

	mo.Selected = mo.Hilite;

	if (mo.Hilite == 0)
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw)
			g_Options.Aggression = static_cast<int>((v * 255.f));
	}
	else if (mo.Hilite == 1)
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw)
			g_Options.Density = static_cast<int>((v * 255.f));
	}
	else if (mo.Hilite == 2)
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw)
			g_Options.Sensitivity = static_cast<int>((v * 255.f));
	}
	else if (mo.Hilite == 3)
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw)
			g_Options.ViewRange = MenuViewOptFromSlider(v);
	}
	else if (mo.Hilite == 4) // Object detail
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw) {
			g_Options.ObjectDetail = MenuObjectDetailFromSlider(v);
			SaveConfig();
		}
	}
	else if (mo.Hilite == 5) // Metric or Imperial(US)
	{
		WaitForMouseRelease();
		g_Options.OptSys = !g_Options.OptSys;
	}
	else if (mo.Hilite == 6)
	{
		WaitForMouseRelease();
		g_Options.SoundAPI = (g_Options.SoundAPI + 1) % AUDIO_BACKEND_COUNT;
	}
	else if (mo.Hilite == 7)
	{
		WaitForMouseRelease();
		g_Options.OptFpsLimit = (g_Options.OptFpsLimit + 1) % kFpsLimitCount;
		SaveConfig();
	}
}

static void ApplyControlsOptionClick(MenuSet& mo)
{
	int sliderX = OptionsLayout::CONTROLS_SLIDER_X;
	int tbw = OptionsLayout::CONTROLS_SLIDER_W;
	float v = static_cast<float>((g_CursorPos.x - sliderX)) / static_cast<float>(tbw);
	if (v < 0.0f) v = 0.0f;
	if (v > 1.0f) v = 1.0f;

	mo.Selected = mo.Hilite;

	if (mo.Hilite >= 0 && mo.Hilite < FirstControlsRowIndex())
	{
		WaitForMouseRelease();
		g_WaitKey = mo.Hilite;
	}
	else if (mo.Hilite == ControlsRowIndex(ControlsRow::NightVision)) // Night vision toggle key
	{
		WaitForMouseRelease();
		g_WaitKey = mo.Hilite;
	}
	else if (mo.Hilite == ControlsRowIndex(ControlsRow::Flashlight)) // Flashlight toggle key
	{
		WaitForMouseRelease();
		g_WaitKey = mo.Hilite;
	}
	else if (mo.Hilite == ControlsRowIndex(ControlsRow::MouseInvert)) // Mouse Y-Axis Inverted
	{
		WaitForMouseRelease();
		g_Options.MouseInvert = !g_Options.MouseInvert;
	}
	else if (mo.Hilite == ControlsRowIndex(ControlsRow::MouseSensitivity)) // Mouse Sensitivty Slider
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw)
			g_Options.MouseSensitivity = static_cast<int>((v * 255.f));
	}
}

static void ApplyVideoOptionClick(MenuSet& mo)
{
	int sliderX = OptionsLayout::OPTION_SLIDER_X;
	int tbw = OptionsLayout::OPTION_SLIDER_W;
	float v = static_cast<float>((g_CursorPos.x - sliderX)) / static_cast<float>(tbw);
	if (v < 0.0f) v = 0.0f;
	if (v > 1.0f) v = 1.0f;

	mo.Selected = mo.Hilite;
	if (mo.Hilite == 0)
	{
		WaitForMouseRelease();
		g_Options.RenderAPI++;
		if (g_Options.RenderAPI >= kRenderAPI_Count)
			g_Options.RenderAPI = kRenderAPI_Software;
		g_Options.RenderAPI = NormalizeMenuRenderAPI(g_Options.RenderAPI);
		SaveConfig();
	}
	if (mo.Hilite == 1) // Resolution
	{
		WaitForMouseRelease();
		// Cycle through the dynamic list. g_ResCount is
		// rebuilt by EnumerateResolutions() at menu startup.
		// Clamp at g_ResCount (g_ResolutionList[] length) instead
		// of the old fixed RES_MAX=8.
		g_Options.Resolution++;
		if (g_Options.Resolution >= g_ResCount)
			g_Options.Resolution = 0;
		// Persist immediately: config.cfg's WxH line
		// overrides the profile index at engine boot,
		// so without this the pick never reaches the hunt.
		SaveConfig();
	}
	else if (mo.Hilite == 2) // Display Mode
	{
		WaitForMouseRelease();
		g_Options.DisplayMode++;
		if (g_Options.DisplayMode >= kDisplayModeCount)
			g_Options.DisplayMode = 0;
		SaveConfig();
	}
	else if (mo.Hilite == 3) // Shadows
	{
		WaitForMouseRelease();
		g_Options.Shadows = !g_Options.Shadows;
	}
	else if (mo.Hilite == 4) // Fog
	{
		WaitForMouseRelease();
		g_Options.Fog = !g_Options.Fog;
	}
	else if (mo.Hilite == 5) // Textures
	{
		WaitForMouseRelease();
		g_Options.Textures++;
		if (g_Options.Textures == 3)
			g_Options.Textures = 0;
	}
	else if (mo.Hilite == 6) // Colorkey
	{
		WaitForMouseRelease();
		g_Options.AlphaColorKey = !g_Options.AlphaColorKey;
	}
	else if (mo.Hilite == 7) // Brightness
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw)
			g_Options.Brightness = static_cast<int>((v * 255.f));
	}
	else if (mo.Hilite == 8) // Field of View
	{
		if (g_CursorPos.x >= sliderX && g_CursorPos.x <= sliderX + tbw)
		{
			int fov = kFovMin + static_cast<int>((v * static_cast<float>((kFovMax - kFovMin))));
			fov = kFovMin + ((fov - kFovMin) / kFovStep) * kFovStep;
			if (fov < kFovMin) fov = kFovMin;
			if (fov > kFovMax) fov = kFovMax;
			g_Options.FOV = fov;
			SaveConfig();
		}
	}
}

static void HandleOptionsInput(uint8_t id)
{
	if (g_WaitKey >= 0)
	{
		AcceptNewKey();
		return;
	}

	if (id == 4)
	{
		if (g_KeyboardState[VK_LBUTTON] & 128)
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();
			TrophySave(g_UserProfile); // Save all the settings
			ChangeMenuState(MENU_MAIN);
		}
		return;
	}

	for (int m = 0; m < OPT_MAX; m++)
	{
		MenuSet& mo = MenuOptions[m];
		if (!IsPointInRect(g_CursorPos, mo.Rect))
		{
			g_MenuItem.SetIsElementSet(m + 1, false);
			mo.Hilite = -1;
			continue;
		}

		g_MenuItem.SetIsElementSet(m + 1, true);

		// Use layout system for hit detection
		Layout::Column* col = nullptr;
		if (m == OPT_GAME) col = &g_GameLabels;
		else if (m == OPT_KEYBINDINGS) col = &g_ControlsLabels;
		else if (m == OPT_VIDEO) col = &g_VideoLabels;

		UpdateOptionsHover(mo, col);

		if (g_KeyboardState[VK_LBUTTON] & 128) // Left Click
		{
			if (m == OPT_GAME)
				ApplyGameOptionClick(mo);
			else if (m == OPT_KEYBINDINGS)
				ApplyControlsOptionClick(mo);
			else if (m == OPT_VIDEO)
				ApplyVideoOptionClick(mo);
		}
	}
}

static Picture* HuntThumbnailFor(HuntBudget price, Picture& visible, Picture& hidden)
{
	// Areas, dinos and weapons show the hidden thumbnail when the item costs
	// at least 1000 and the account is below 1000. Accessories have no hidden
	// variant and use m_Thumbnail directly.
	if (price >= 1000 && g_UserProfile.Score < 1000)
		return &hidden;
	return &visible;
}

static void ToggleBudgetedHuntItem(HuntBudget score, HuntBudget price, bool& selected)
{
	// Selecting requires covering the price; deselecting is always free, so
	// the account can never be driven negative.
	selected = score >= price && !selected;
	g_ScoreDebit = CalculateDebit();
}

static void UpdateHuntAreaHover()
{
	WaitForMouseRelease();
	HuntBudget scorea = 0;

	if (MenuHunt[0].Selected != -1)
	{
		scorea = g_AreaInfo[MenuHunt[0].Selected].m_Price;
	}

	HuntBudget score = RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit) + scorea;
	int yd = g_CursorPos.y - MenuHunt[0].Rect.top;

	unsigned index = HuntListDataIndex(yd / 16, MenuHunt[0].Offset);

	if (index < g_AreaInfo.size())
	{
		g_HuntSelectPic = HuntThumbnailFor(g_AreaInfo[index].m_Price,
			g_AreaInfo[index].m_Thumbnail, g_AreaInfo[index].m_ThumbnailHidden);
		g_HuntInfo.first = 0; // Areas
		g_HuntInfo.second = index;

		if ((g_KeyboardState[VK_LBUTTON] & 128) && score >= g_AreaInfo[index].m_Price)
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();

			// Reset the states
			for (auto i = MenuHunt[0].Item.begin(); i != MenuHunt[0].Item.end(); i++)
			{
				i->second = false;
			}

			MenuHunt[0].Item[index].second = true;
			MenuHunt[0].Selected = index;

			g_ScoreDebit = CalculateDebit();
		}
	}
}

static void UpdateHuntDinoHover()
{
	HuntBudget score = RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit);
	int yd = g_CursorPos.y - MenuHunt[1].Rect.top;

	unsigned index = yd / 16;
	unsigned dataIndex = HuntListDataIndex(index, MenuHunt[1].Offset);

	if (dataIndex < g_DinoList.size())
	{
		DinoInfo& dino = g_DinoInfo[g_DinoList[dataIndex]];
		g_HuntSelectPic = HuntThumbnailFor(dino.m_Price, dino.m_Thumbnail, dino.m_ThumbnailHidden);
		g_HuntInfo.first = 1; // Dinos
		g_HuntInfo.second = dataIndex;

		if ((g_KeyboardState[VK_LBUTTON] & 128))
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();

			ToggleBudgetedHuntItem(score, dino.m_Price, MenuHunt[1].Item[dataIndex].second);
		}
	}
}

static void UpdateHuntWeaponHover()
{
	HuntBudget score = RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit);
	int yd = g_CursorPos.y - MenuHunt[2].Rect.top;

	unsigned index = HuntListDataIndex(yd / 16, MenuHunt[2].Offset);

	if (index < MenuHunt[2].Item.size())
	{
		WeapInfo& weapon = g_WeapInfo[index];
		g_HuntSelectPic = HuntThumbnailFor(weapon.m_Price, weapon.m_Thumbnail, weapon.m_ThumbnailHidden);
		g_HuntInfo.first = 2; // Weapons
		g_HuntInfo.second = index;

		if ((g_KeyboardState[VK_LBUTTON] & 128))
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();

			ToggleBudgetedHuntItem(score, weapon.m_Price, MenuHunt[2].Item[index].second);
		}
	}
}

static void UpdateHuntAccessoryHover()
{
	HuntBudget score = RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit);
	int yd = g_CursorPos.y - MenuHunt[3].Rect.top;
	unsigned accIndex = (unsigned)(yd / 16) + MenuHunt[3].Offset;

	// MenuHunt[3].Item and g_UtilInfo are parallel and fully listed,
	// so the cursor row maps directly to the accessory index.
	if (accIndex < MenuHunt[3].Item.size() && accIndex < g_UtilInfo.size())
	{
		g_HuntSelectPic = &g_UtilInfo[accIndex].m_Thumbnail;
		g_HuntInfo.first = 3; // Accessories
		g_HuntInfo.second = accIndex;

		if ((g_KeyboardState[VK_LBUTTON] & 128))
		{
			WaitForMouseRelease();
			MenuAudioPlayClick();

			ToggleBudgetedHuntItem(score, g_UtilInfo[accIndex].m_Price,
				MenuHunt[3].Item[accIndex].second);
		}
	}
}

static void UpdateHuntObserverHover()
{
	// Observer info panel
	g_HuntSelectPic = &g_ObserverInfo.m_Thumbnail;
	g_HuntInfo.first = 3; // Accessories
	g_HuntInfo.second = 9999;
}

static void UpdateHuntHover(uint8_t id)
{
	// Mouse hover
	if (IsPointInRect(g_CursorPos, MenuHunt[0].Rect))
		UpdateHuntAreaHover();
	else if (IsPointInRect(g_CursorPos, MenuHunt[1].Rect))
		UpdateHuntDinoHover();
	else if (IsPointInRect(g_CursorPos, MenuHunt[2].Rect))
		UpdateHuntWeaponHover();
	else if (IsPointInRect(g_CursorPos, MenuHunt[3].Rect))
		UpdateHuntAccessoryHover();
	else if (id == 6)
		UpdateHuntObserverHover();
}

static void ToggleHuntSelection(uint8_t id)
{
	WaitForMouseRelease();
	MenuAudioPlayClick();
	bool b = g_MenuItem.ToggleIsElementSet(id);

	if (id >= 1 && id <= 3)
	{
		g_TimeOfDay = id - 1;

		for (int i = 1; i <= 3; i++)
		{
			if (i != id)
				g_MenuItem.SetIsElementSet(i, false);
		}
	}

	if (id == 4)
		b = false; // Unused
	if (id == 6) {
		g_ObserverMode = b;
	}
}

static int BuildHuntDinoFlags()
{
	int din = 0;

	// Calculate the dinosaur flags
	for (unsigned i = 0; i < g_DinoList.size(); i++)
	{
		if (MenuHunt[1].Item[i].second)
			din |= 1 << i;
	}

	return din;
}

static int BuildHuntWeaponFlags()
{
	int wep = 0;

	// Calculate the weapon flags
	for (unsigned i = 0; i < g_WeapInfo.size(); i++)
	{
		if (MenuHunt[2].Item[i].second)
			wep |= 1 << i;
	}

	return wep;
}

static bool PrepareHuntLaunchRequest(HuntLaunchRequest& launchRequest, int din, int wep)
{
	// Initialise the command line parameters through the tested helper. For
	// slot six, this uses the basename whose files actually exist.
	if (MenuHunt[0].Selected < 0 ||
		MenuHunt[0].Selected >= static_cast<int>(g_AreaInfo.size()))
	{
		ShowErrorMessage("The selected hunt is invalid.");
		return false;
	}
	const AreaInfo& launchArea = g_AreaInfo[MenuHunt[0].Selected];
	launchRequest.projectName = launchArea.m_ProjectName;
	launchRequest.mapFile = launchArea.m_MapFile;
	launchRequest.registration = g_UserProfile.RegNumber;
	launchRequest.dinoFlags = din;
	launchRequest.weaponFlags = wep;
	launchRequest.timeOfDay = g_TimeOfDay;

	// The engine opens <basename>.map and <basename>.rsc from this one
	// name, so refuse here with the missing path named rather than
	// launching into "Error opening resource file".
	const std::string launchBase = launchRequest.mapFile.empty()
		? launchRequest.projectName
		: launchRequest.mapFile;
	const std::string missingFile = MissingAreaFile(launchBase);
	if (!missingFile.empty())
	{
		ShowErrorMessage("This area cannot be played: " + missingFile +
			" is missing.\r\n\r\nAn area loads from a pair with the same "
			"basename: huntdat/areas/" + launchBase + ".map and .rsc.\r\n"
			"Restore the missing file from your game data, or remove the area.");
		return false;
	}

	return true;
}

static void AppendHuntAccessoryArguments(std::stringstream& params)
{
#ifdef _iceage
	// Ice Age resupply
	if (MenuHunt[3].Item[4].second)
		params << " " << g_UtilInfo[4].m_Command;
#endif //_iceage

	if (MenuHunt[3].Item[kAccDouble].second)
		params << " " << g_UtilInfo[kAccDouble].m_Command;

	// Night vision goggles
	if (kAccNV < static_cast<int>(MenuHunt[3].Item.size()) && MenuHunt[3].Item[kAccNV].second)
		params << " " << g_UtilInfo[kAccNV].m_Command;

	// These three persist as owned regardless of selection; only
	// the launch flags depend on what is ticked.
	g_Options.CamoMode = true;
	g_Options.RadarMode = true;
	g_Options.ScentMode = true;
	if (MenuHunt[3].Item[kAccCamo].second) params << " -camo";
	if (MenuHunt[3].Item[kAccRadar].second) params << " -radar";
	if (MenuHunt[3].Item[kAccScent].second) params << " -scent";

	// Tranquilizers
	if (kAccTranq < static_cast<int>(MenuHunt[3].Item.size()) && MenuHunt[3].Item[kAccTranq].second)
		params << " " << g_UtilInfo[kAccTranq].m_Command;

	// Flashlight: like night vision this only unlocks the in-game
	// toggle; the accessory itself carries no quieter cheat behind it.
	if (static_cast<int>(MenuHunt[3].Item.size()) > kAccFlashlight && MenuHunt[3].Item[kAccFlashlight].second)
		params << " " << g_UtilInfo[kAccFlashlight].m_Command;
}

static void AppendHuntModeArguments(std::stringstream& params)
{
	if (g_ObserverMode)
		params << " -observ";

	// Pass accessory score multipliers to the engine. The order lives
	// in Hunt/Core/ScoreMod.h and is shared with the parser in
	// Hunt/Game/CommandLine.cpp, so the two cannot drift apart.
	// Values come from UtilInfo.m_ScoreMod (populated from _RES.TXT's
	// 'accessories {}' block, falling back to legacy defaults).
	float scoreMods[kScoreModSlotCount] = {};
	scoreMods[static_cast<int>(ScoreModSlot::Camo)]     = g_UtilInfo[kAccCamo].m_ScoreMod;
	scoreMods[static_cast<int>(ScoreModSlot::Radar)]    = g_UtilInfo[kAccRadar].m_ScoreMod;
	scoreMods[static_cast<int>(ScoreModSlot::Scent)]    = g_UtilInfo[kAccScent].m_ScoreMod;
	scoreMods[static_cast<int>(ScoreModSlot::Double)]   = g_UtilInfo[kAccDouble].m_ScoreMod;
	scoreMods[static_cast<int>(ScoreModSlot::Tranq)]    = g_UtilInfo[kAccTranq].m_ScoreMod;
	scoreMods[static_cast<int>(ScoreModSlot::Observer)] = g_ObserverInfo.m_ScoreMod;
	char scoreModPayload[kScoreModSlotCount * 24];
	if (BuildScoreModPayload(scoreMods, scoreModPayload, sizeof scoreModPayload) > 0)
		params << " smod=" << scoreModPayload;

#ifdef _DEBUG
	params << " -debug";
#endif //_DEBUG
}

static void LaunchPreparedHunt(std::stringstream& params, bool hasLoadout)
{
	std::stringstream renderer("");
	renderer << g_RendererFile[g_Options.RenderAPI] << ".ren";

	// Observer mode is an exploration session and deliberately permits an
	// empty creature/weapon loadout, matching the original launcher.
	if (g_ObserverMode || hasLoadout)
	{
		TrophySave(g_UserProfile); // Save all the settings
		AppendDisplayModeLaunchFlag(params);
		std::cout << "Launching...  `> " << renderer.str() << " " << params.str() << "`" << std::endl;
		SaveHuntSelections();
		SaveConfig();
		LaunchProcess(renderer.str(), params.str());
		TrophyLoad(g_UserProfile, g_UserProfile.RegNumber); // Load the changes
		LoadConfig();
	}
	else
	{
		ShowErrorMessage("You need to select at least:\r\n 1x Creature\r\n 1x Weapon");
	}
}

static void LaunchSelectedHunt()
{
	// Launch the game
	WaitForMouseRelease();
	MenuAudioPlayClick();

	if (MenuHunt[0].Selected == -1)
	{
		// -- Don't launch
		return;
	}

	// Recheck the whole setup before saving/launching, including restored
	// selections whose authored prices may have changed since last use.
	g_ScoreDebit = CalculateDebit();
	if (g_ScoreDebit > static_cast<HuntBudget>(g_UserProfile.Score)) {
		ShowErrorMessage("The selected hunt exceeds your setup budget.");
		return;
	}

	const int din = BuildHuntDinoFlags();
	const int wep = BuildHuntWeaponFlags();

	HuntLaunchRequest launchRequest;
	if (!PrepareHuntLaunchRequest(launchRequest, din, wep))
		return;

	std::string launchArguments;
	if (!BuildHuntLaunchArguments(launchRequest, launchArguments))
	{
		ShowErrorMessage("The selected hunt has invalid launch data.");
		return;
	}
	// The base arguments must be the first write into an empty stream;
	// MakeLaunchParamStream owns that contract.
	std::stringstream params = MakeLaunchParamStream(launchArguments);

	AppendHuntAccessoryArguments(params);
	AppendHuntModeArguments(params);
	LaunchPreparedHunt(params, wep && din);
}

static void HandleHuntInput(uint8_t id)
{
	if (g_KeyboardState[VK_ESCAPE] & 128)
	{
		ChangeMenuState(MENU_QUIT);
	}

	UpdateHuntHover(id);

	// Left Mouse Click
	if (!(g_KeyboardState[VK_LBUTTON] & 128)) return;

	if (id >= 1 && id <= 6)
	{
		ToggleHuntSelection(id);
	}
	else if (id == 7) // Back
	{
		WaitForMouseRelease();
		MenuAudioPlayClick();
		ChangeMenuState(MENU_MAIN);
	}
	else if (id == 8) // Hunt/Next
	{
		LaunchSelectedHunt();
	}
}

static void LaunchTrophyRoom()
{
	// The room is a project like any other: it opens
	// huntdat/areas/trophy.map/.rsc, so a missing half is reported
	// here instead of halting the game on the path.
	const std::string trophyMissing = MissingAreaFile("trophy");
	if (!trophyMissing.empty())
	{
		ShowErrorMessage("The trophy room cannot be opened: " + trophyMissing +
			" is missing.\r\n\r\nThe room needs huntdat/areas/trophy.map "
			"and trophy.rsc in the game folder.");
		return;
	}
	HuntLaunchRequest launchRequest;
	launchRequest.projectName = "trophy";
	launchRequest.registration = g_UserProfile.RegNumber;
	launchRequest.timeOfDay = 1;
	std::string launchArguments;
	if (!BuildHuntLaunchArguments(launchRequest, launchArguments))
	{
		ShowErrorMessage("The trophy room has invalid launch data.");
		return;
	}
	std::stringstream params = MakeLaunchParamStream(launchArguments);
#ifdef _DEBUG
	params << " -debug";
#endif //_DEBUG
	std::stringstream renderer("");
	renderer << g_RendererFile[g_Options.RenderAPI] << ".ren";

	AppendDisplayModeLaunchFlag(params);
	std::cout << "Execute: [" << renderer.str() << " " << params.str() << "]" << std::endl;
	TrophySave(g_UserProfile); // Save the changes
	LaunchProcess(renderer.str(), params.str());
	TrophyLoad(g_UserProfile, g_UserProfile.RegNumber); // Load the changes
	LoadConfig();
}

static void HandleMainInput(uint8_t id)
{
	if (g_KeyboardState[VK_ESCAPE] & 128) {
		ChangeMenuState(MENU_QUIT);
	}
	else if (g_KeyboardState[VK_LBUTTON] & 128) {
		WaitForMouseRelease();
		if (id >= 1 && id <= 6) {
			WaitForMouseRelease();
			MenuAudioPlayClick();
			if (id == 1) { ChangeMenuState(MENU_HUNT); }
			else if (id == 2) { ChangeMenuState(MENU_OPTIONS); }
			else if (id == 3) {
				LaunchTrophyRoom();
			}
			else if (id == 4) { ChangeMenuState(MENU_CREDITS); }
			else if (id == 5) { ChangeMenuState(MENU_QUIT); }
			else if (id == 6) { ChangeMenuState(MENU_STATISTICS); }
		}
	}
}

static void HandleStatisticsInput()
{
	if (g_KeyboardState[VK_ESCAPE] & 128) {
		ChangeMenuState(MENU_QUIT);
	}

	if (g_KeyboardState[VK_LBUTTON] & 128) {
		WaitForMouseRelease();
		MenuAudioPlayClick();
		ChangeMenuState(MENU_MAIN);
	}
}

static void HandleQuitInput(uint8_t id)
{
	if (g_KeyboardState[VK_LBUTTON] & 128) {
		WaitForMouseRelease();
		MenuAudioPlayClick();

		if (id == 1)      RequestMenuExit(0);
		else if (id == 2) ChangeMenuState(MENU_MAIN);
	}
}

void MenuEventInput(int32_t menu)
{
	if (!g_KeyboardUsed) return;

	const uint8_t id = g_MenuItem.GetID(g_CursorPos.x / 2, g_CursorPos.y / 2);

	switch (menu) {
	case MENU_CREDITS: HandleCreditsInput(); break;
	case MENU_REGISTER: HandleRegisterInput(id); break;
	case MENU_REGISTRY_DELETE: HandleRegistryDeleteInput(id); break;
	case MENU_REGISTRY_WAIVER: HandleRegistryWaiverInput(id); break;
	case MENU_OPTIONS: HandleOptionsInput(id); break;
	case MENU_HUNT: HandleHuntInput(id); break;
	case MENU_MAIN: HandleMainInput(id); break;
	case MENU_STATISTICS: HandleStatisticsInput(); break;
	case MENU_QUIT: HandleQuitInput(id); break;
	default: break;
	}
}

void MenuKeyCharEvent(uint16_t wParam)
{
	if (g_MenuState == MENU_REGISTER) {
		if (wParam == 8) {
			if (!g_TypingBuffer.empty()) {
				g_TypingBuffer.pop_back();
				MenuAudioPlayType();
			}
		}
		else {
			if (g_TypingBuffer.size() < 19) {
				if (wParam >= 32 && wParam <= 128) {
					g_TypingBuffer.push_back(static_cast<char>(wParam));
					MenuAudioPlayType();
				}
			}
		}
	}
}

void MenuMouseScrollEvent(int32_t menu, int32_t scroll)
{
	// TODO: Enable scrolling of text lists
	if (menu == MENU_REGISTER)
	{

	}
	else if (menu == MENU_HUNT)
	{
		for (int i = 0; i < 4; i++)
		{
			if (IsPointInRect(g_CursorPos, MenuHunt[i].Rect) && MenuHunt[i].Item.size() > 10)
			{
				MenuHunt[i].Offset = static_cast<uint32_t>(ScrolledHuntListOffset(
					MenuHunt[i].Offset, MenuHunt[i].Item.size(), scroll));
			}
		}
	}
}

