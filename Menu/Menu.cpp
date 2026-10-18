/***************************************************
* AtmosFear 2.1
* Menu.cpp
*
* Main Menu Code
*
*/

#include "Hunt.h"
#include "MenuInternal.h"
#include "LaunchArgs.h"
#include "ListMath.h"
#include "SliderMath.h"
#include "ShopSafety.h"
#include "../Hunt/Core/KeyBindings.h"
#include "../Hunt/Core/ScoreMod.h"
#include "../Hunt/Core/ShadowText.h"
#include <cassert>
#include <cmath>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

std::pair<unsigned, unsigned> g_HuntInfo;
MenuSet MenuOptions[3];
MenuSet MenuRegistry;
MenuSet MenuHunt[4];

void* lpVideoBuf;
HDC hdcCMain;
HBITMAP bmpMain;
HBITMAP hbmpOld;
HFONT hfntOld;

POINT g_CursorPos;
int g_WaitKey = -1;
bool g_KeyboardUsed = false;
int g_LastHoverId = -1;

// Registry name-list double-click tracking (double-click a name to enter).
static DWORD g_LastListClickTime = 0;
static int g_LastListClickIndex = -1;

// Accessory indices. MenuHunt[3].Item and g_UtilInfo stay parallel and fully
// populated (see UtilInfo note in Hunt.h), so launch/smod code can index
// both arrays with these. NOTE: _iceage inserts "Supply drop" at slot 4
// and shifts NV/tranq -- these constants are C2-only.
constexpr int kAccCamo = 0;
constexpr int kAccRadar = 1;
constexpr int kAccScent = 2;
constexpr int kAccDouble = 3;
constexpr int kAccNV = 4;
constexpr int kAccTranq = 5;
constexpr int kAccFlashlight = 6;
constexpr int kAccCount = 7;

static void NormalizeRendererOption()
{
	const int32_t normalized = NormalizeMenuRenderAPI(g_Options.RenderAPI);
	if (normalized != g_Options.RenderAPI) {
		g_Options.RenderAPI = normalized;
		SaveConfig();
	}
}

// Append the display-mode launch flag matching the menu's Display Mode
// option (0=windowed, 1=exclusive fullscreen, 2=borderless fullscreen).
// The same key is written to config.cfg (display_mode), so the engine
// applies it even when launched without these flags; the flag here just
// makes the intent explicit and covers any config-less launch.
// Borderless is the GL default because DWM-composed fullscreen keeps
// Windows 11 color management active on wide-gamut displays. The
// software renderer has no borderless presentation, so its "borderless"
// pick maps to exclusive fullscreen (its classic behaviour).
static void AppendDisplayModeLaunchFlag(std::stringstream& params)
{
	switch (g_Options.DisplayMode) {
	case 0: params << " -windowed"; break;
	case 1: params << " -fullscreen"; break;
	case 2:
	default:
		if (NormalizeMenuRenderAPI(g_Options.RenderAPI) == kRenderAPI_OpenGL)
			params << " -borderless";
		else
			params << " -fullscreen";
		break;
	}
}

// ======================================================================= //
// Global Layout Columns (initialized in InitInterface)
// ======================================================================= //
Layout::Column g_GameLabels, g_GameValues;
Layout::Column g_ControlsLabels, g_ControlsValues;
Layout::Column g_VideoLabels, g_VideoValues;

bool IsPointInRect(POINT& p, RECT& rc)
{
	return (p.x > rc.left && p.y > rc.top && p.x < rc.right&& p.y < rc.bottom);
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
				*((uint32_t*)(&g_Options.KeyMap) + g_WaitKey) = 0;
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
						const int keyFieldCount = static_cast<int>(sizeof(g_Options.KeyMap) / sizeof(int32_t));
						for (int t = 0; t < keyFieldCount; t++)
							if (*((uint32_t*)(&g_Options.KeyMap) + t) == k)
								*((uint32_t*)(&g_Options.KeyMap) + t) = 0;

						*(reinterpret_cast<int*>((&g_Options.KeyMap)) + g_WaitKey) = k;
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

void ChangeMenuState(int32_t ms)
{
	// NOTE: the ambient bed keeps playing across submenus (hunt, options,
	// briefing...). It is silenced only around external processes -- hunts
	// and the trophy room -- by LaunchProcess, which also resumes it.
	g_PrevMenuState = g_MenuState;
	g_MenuState = ms;
	g_LastHoverId = -1;
	LoadGameMenu(g_MenuState);
}

// Centralized menu-exit path. Saves the current profile (and options,
// since TrophySave writes the full 1660-byte trophy block) before
// requesting shutdown, so unsaved settings in the menu do not get lost
// on quit. Callers must use this instead of calling PostQuitMessage
// directly, otherwise the save is skipped.
void RequestMenuExit(int exitCode)
{
	TrophySave(g_UserProfile);
	PostQuitMessage(exitCode);
}

int GetTextW(HDC hdc, const std::string& s)
{
	SIZE sz;
	GetTextExtentPoint(hdc, s.c_str(), (int)s.length(), &sz);
	return sz.cx;
}

int GetTextH(HDC hdc, const std::string& s)
{
	SIZE sz;
	GetTextExtentPoint(hdc, s.c_str(), (int)s.length(), &sz);
	return sz.cy;
}

void MenuSet::AddItem(const std::string& txt)
{
	this->Item.push_back(std::make_pair(txt, false));
	this->Count = this->Item.size();
}

/*
Determine the amount of points that the current license selection
in the hunt screen will cost.
*/
HuntBudget CalculateDebit()
{
	HuntBudget debit = 0;

	if (MenuHunt[0].Selected >= 0 && MenuHunt[0].Selected < (int)g_AreaInfo.size())
	{
		debit += g_AreaInfo[MenuHunt[0].Selected].m_Price;
	}

	for (unsigned i = 0; i < MenuHunt[1].Item.size() && i < g_DinoList.size(); i++)
	{
		if (MenuHunt[1].Item[i].second && g_DinoList[i] < g_DinoInfo.size())
		{
			debit += g_DinoInfo[g_DinoList[i]].m_Price;
		}
	}

	debit += SelectedRowCost(MenuHunt[2].Item, g_WeapInfo);

	// Prices reserve the current setup's budget, not permanently spend score.
	// Accessory scoring is a separate hunt-end calculation (smod= multipliers).
	debit += SelectedRowCost(MenuHunt[3].Item, g_UtilInfo);

	return debit;
}

/*
Currently unused
*/
void CALLBACK WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{

}

/*
Currently unused
*/
void AudioSoftThread()
{
	HWAVEOUT hwo;
	WAVEOUTCAPS woc;
	WAVEFORMATEX wfx;
	unsigned device = WAVE_MAPPER;

	wfx.cbSize = 0;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = 22050;
	wfx.nBlockAlign = 2;
	wfx.wBitsPerSample = 16;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	std::cout << "Audio: Checking " << waveInGetNumDevs() << " devices for wave out support..." << std::endl;
	for (unsigned i = 0; i < waveInGetNumDevs(); i++)
		if (waveOutOpen(&hwo, i, &wfx, 0, 0, CALLBACK_EVENT | WAVE_FORMAT_QUERY) == MMSYSERR_NOERROR)
		{
			device = i;

			std::cout << "WaveOut Device ID: " << device << std::endl;
			std::cout << std::setw(20) << std::setfill('=') << "=" << std::endl;

			waveOutGetDevCaps(device, &woc, sizeof(WAVEOUTCAPS));
			std::cout << "Manufacturer ID: " << woc.wMid << std::endl;
			std::cout << "Product ID: " << woc.wPid << std::endl;
			std::cout << "Driver Version: " << (HIWORD(woc.vDriverVersion)) << "." << (LOWORD(woc.vDriverVersion)) << std::endl;
			std::cout << "Product Name: " << woc.szPname << std::endl;
			std::cout << "Channels: " << woc.wChannels << std::endl;

			std::cout << "Supported formats:" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_1M08)
				std::cout << "\t22.05 kHz Mono 8-bit" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_2M16)
				std::cout << "\t22.05 kHz Mono 16-bit" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_2S08)
				std::cout << "\t22.05 kHz Stereo 8-bit" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_2S16)
				std::cout << "\t22.05 kHz Stereo 16-bit" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_4M08)
				std::cout << "\t44.1 kHz Mono 8-bit" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_4M16)
				std::cout << "\t44.1 kHz Mono 16-bit" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_4S08)
				std::cout << "\t44.1 kHz Stereo 8-bit" << std::endl;
			if (woc.dwFormats & WAVE_FORMAT_4S16)
				std::cout << "\t44.1 kHz Stereo 16-bit" << std::endl;

			std::cout << "Supported functionality:" << std::endl;
			if (woc.dwSupport & WAVECAPS_LRVOLUME)
				std::cout << "\tSeparate left and right volume control." << std::endl;
			if (woc.dwSupport & WAVECAPS_PITCH)
				std::cout << "\tPitch control" << std::endl;
			if (woc.dwSupport & WAVECAPS_PLAYBACKRATE)
				std::cout << "\tPlayback rate control" << std::endl;
			if (woc.dwSupport & WAVECAPS_SYNC)
				std::cout << "\tDriver is synchronous" << std::endl;
			if (woc.dwSupport & WAVECAPS_VOLUME)
				std::cout << "\tVolume control" << std::endl;
			if (woc.dwSupport & WAVECAPS_SAMPLEACCURATE)
				std::cout << "\tSample-accurate position" << std::endl;

			break;
		}

	if (waveOutOpen(&hwo, device, &wfx, 0, reinterpret_cast<DWORD_PTR>(&WaveOutProc), CALLBACK_FUNCTION | WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
	{
		std::cout << "Audio: Failed to open WaveOut device!" << std::endl;
		return;
	}

	waveOutClose(hwo);
}

/*
Initialise all the required settings and interface elements
*/
void InitInterface()
{
	std::cout << "Interface: Creating GDI Font handles..." << std::endl;
	fnt_Big = CreateFont(
		23, 0, 0, 0,
		600, 0, 0, 0,
		ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);

	g_FontOptions = CreateFont(
		21, 0, 0, 0,
		500, 0, 0, 0,
		ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);

	fnt_Small = CreateFont(
		14, 0, 0, 0,
		100, 0, 0, 0,
		ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);

	fnt_Midd = CreateFont(
		16, 0, 0, 0,
		550, 0, 0, 0,
		ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);

	if (!fnt_Small)
		std::cout << "Interface : Failed to create Small Font" << std::endl;
	if (!fnt_Midd)
		std::cout << "Interface : Failed to create Medium Font" << std::endl;
	if (!g_FontOptions)
		std::cout << "Interface : Failed to create Options Font" << std::endl;
	if (!fnt_Big)
		std::cout << "Interface : Failed to create Large Font" << std::endl;

	hdcCMain = CreateCompatibleDC(hdcMain);

	if (hdcCMain == nullptr)
	{
		throw std::runtime_error("Interface: Failed to create CompatibleDC!");
		return;
	}

	BITMAPINFOHEADER bmih;
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = 800;
	bmih.biHeight = -600;
	bmih.biPlanes = 1;
	bmih.biBitCount = 16;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;
	bmih.biXPelsPerMeter = 400;
	bmih.biYPelsPerMeter = 400;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;

	BITMAPINFO binfo;
	binfo.bmiHeader = bmih;
	bmpMain = CreateDIBSection(hdcMain, &binfo, DIB_RGB_COLORS, &lpVideoBuf, nullptr, 0);

	if (!bmpMain) {
		throw std::runtime_error("Interface: Failed to create DIB Section!");
		return;
	}

	g_PrevMenuState = -1;
	g_MenuState = 0;
	g_TypingBuffer = "";
	LoadGameMenu(0);

	g_ProfileIndex = 0;
	g_HuntSelectPic = nullptr;

	/************************************************************
	* Register/Profile lists
	*/
	MenuRegistry.x0 = 360;
	MenuRegistry.y0 = 363;
	//MenuEventStart(MENU_REGISTER) contains the AddMenuItem() calls
	MenuRegistry.Rect = { 360, 363, 499, 491 }; // Enough room for 9.14 items with fnt_Small

	/************************************************************
	* Options lists
	*/
	int m = OPT_GAME;
	MenuOptions[m].x0 = OptionsLayout::PANEL_GAME.x;
	MenuOptions[m].y0 = OptionsLayout::PANEL_GAME.y;
	MenuOptions[m].Padding = 15;
	MenuOptions[m].Count = 0;
	MenuOptions[m].AddItem("Agressivity");
	MenuOptions[m].AddItem("Density");
	MenuOptions[m].AddItem("Sensitivity");
	MenuOptions[m].AddItem("View range");
	MenuOptions[m].AddItem("Object detail");
	MenuOptions[m].AddItem("Measurement");
	MenuOptions[m].AddItem("Sound API");
	MenuOptions[m].AddItem("FPS limit");
	MenuOptions[m].Rect = { OptionsLayout::PANEL_GAME.x, OptionsLayout::PANEL_GAME.y, OptionsLayout::PANEL_GAME.GetRight(), OptionsLayout::PANEL_GAME.GetBottom() };

	m = OPT_KEYBINDINGS;
	MenuOptions[m].x0 = OptionsLayout::PANEL_CONTROLS.x;
	MenuOptions[m].y0 = OptionsLayout::PANEL_CONTROLS.y;
	MenuOptions[m].Padding = 15;
	MenuOptions[m].Count = 0;
	MenuOptions[m].AddItem("Forward");
	MenuOptions[m].AddItem("Backward");
	MenuOptions[m].AddItem("Reload");
	MenuOptions[m].AddItem("Resupply");
	MenuOptions[m].AddItem("Hold Breath");
	MenuOptions[m].AddItem("Firing Mode");
	MenuOptions[m].AddItem("Fire");
	MenuOptions[m].AddItem("Get weapon");
	MenuOptions[m].AddItem("Step Left");
	MenuOptions[m].AddItem("Step Right");
	MenuOptions[m].AddItem("Rack");
	MenuOptions[m].AddItem("Jump");
	MenuOptions[m].AddItem("Run");
	MenuOptions[m].AddItem("Crouch");
	MenuOptions[m].AddItem("Lure Call");
	MenuOptions[m].AddItem("Change Call");
	MenuOptions[m].AddItem("Binoculars");
	MenuOptions[m].AddItem("Night vision");
	MenuOptions[m].AddItem("Flashlight");
#ifdef _iceage
	MenuOptions[m].AddItem("Call Resupply");
#endif //_iceage
	MenuOptions[m].AddItem("Reverse mouse");
	MenuOptions[m].AddItem("Mouse sensitivity");
	MenuOptions[m].Rect = { OptionsLayout::PANEL_CONTROLS.x, OptionsLayout::PANEL_CONTROLS.y, OptionsLayout::PANEL_CONTROLS.GetRight(), OptionsLayout::PANEL_CONTROLS.GetBottom() };
	// Rows below are addressed positionally into Options::KeyMap (see
	// ControlsRowIndex), so the two lists must stay the same length.
	assert(FirstControlsRowIndex() == static_cast<int>(sizeof(TKeyMap) / sizeof(int32_t)));

	m = OPT_VIDEO;
	MenuOptions[m].x0 = OptionsLayout::PANEL_VIDEO.x;
	MenuOptions[m].y0 = OptionsLayout::PANEL_VIDEO.y;
	MenuOptions[m].Padding = 15;
	MenuOptions[m].Count = 0;
	MenuOptions[m].AddItem("Video Driver");
	MenuOptions[m].AddItem("Resolution");
	MenuOptions[m].AddItem("Display Mode");
	MenuOptions[m].AddItem("3D Shadows");
	MenuOptions[m].AddItem("Fog");
	MenuOptions[m].AddItem("Textures");
	MenuOptions[m].AddItem("Alpha Source");
	MenuOptions[m].AddItem("Brightness");
	MenuOptions[m].AddItem("Field of View");
	MenuOptions[m].Rect = { OptionsLayout::PANEL_VIDEO.x, OptionsLayout::PANEL_VIDEO.y, OptionsLayout::PANEL_VIDEO.GetRight(), OptionsLayout::PANEL_VIDEO.GetBottom() };

	/************************************************************
	* Initialize Layout Columns
	* Fixed StartLegacy-style rows keep labels/values inside the artwork panels.
	*/
	g_GameLabels = OptionsLayout::MakeGameLabels(static_cast<int>(MenuOptions[OPT_GAME].Item.size()));
	g_GameValues = OptionsLayout::MakeGameValues(static_cast<int>(MenuOptions[OPT_GAME].Item.size()));

	g_ControlsLabels = OptionsLayout::MakeControlsLabels(static_cast<int>(MenuOptions[OPT_KEYBINDINGS].Item.size()));
	g_ControlsValues = OptionsLayout::MakeControlsValues(static_cast<int>(MenuOptions[OPT_KEYBINDINGS].Item.size()));

	g_VideoLabels = OptionsLayout::MakeVideoLabels(static_cast<int>(MenuOptions[OPT_VIDEO].Item.size()));
	g_VideoValues = OptionsLayout::MakeVideoValues(static_cast<int>(MenuOptions[OPT_VIDEO].Item.size()));

	/************************************************************
	* Hunt lists
	*/
	m = 0; // Areas
	MenuHunt[m].x0 = 10;
	MenuHunt[m].y0 = 382;
	MenuHunt[m].Padding = 4;
	MenuHunt[m].Count = 0;
	MenuHunt[m].Offset = 0;
	// ...
	MenuHunt[m].Rect = { 10, 382, 190, 542 };

	m = 1; // Dinosaurs
	MenuHunt[m].x0 = 10;
	MenuHunt[m].y0 = 382;
	MenuHunt[m].Padding = 4;
	MenuHunt[m].Count = 0;
	MenuHunt[m].Offset = 0;
	// ...
	MenuHunt[m].Rect = { 210, 382, 390, 542 };

	m = 2; // Weapons
	MenuHunt[m].x0 = 210;
	MenuHunt[m].y0 = 382;
	MenuHunt[m].Padding = 4;
	MenuHunt[m].Count = 0;
	MenuHunt[m].Offset = 0;
	// ...
	MenuHunt[m].Rect = { 410, 382, 590, 542 };

	m = 3; // Accessories / Utilities
	MenuHunt[m].x0 = 10;
	MenuHunt[m].y0 = 382;
	MenuHunt[m].Padding = 4;
	MenuHunt[m].Count = 0;
	MenuHunt[m].Offset = 0;
	MenuHunt[m].AddItem("Camouflage");
	MenuHunt[m].AddItem("Radar");
	MenuHunt[m].AddItem("Cover scent");
	MenuHunt[m].AddItem("Double ammo");
#ifdef _iceage
	MenuHunt[m].AddItem("Supply drop");
#endif //_iceage
	MenuHunt[m].AddItem("Night vision");
	MenuHunt[m].AddItem("Tranquilizers");
	MenuHunt[m].AddItem("Flashlight");
#ifndef _iceage
	// Keep parallel with g_UtilInfo (camo/radar/scent/double/NV/tranq/flashlight).
	assert(MenuHunt[m].Item.size() == (size_t)kAccCount);
#endif
	MenuHunt[m].Rect = { 610, 382, 790, 542 };

	std::cout << "Interface: Initialisation Ok!" << std::endl;

#ifdef _DEBUG
	std::thread audio_thread(AudioSoftThread);
	audio_thread.join();
#endif // _DEBUG
}

void ShutdownInterface()
{
	if (bmpMain)
		DeleteObject(reinterpret_cast<HBITMAP>(bmpMain));
	if (hdcCMain)
		DeleteDC(hdcCMain);

	if (fnt_Small)
		DeleteObject(fnt_Small);
	if (fnt_Midd)
		DeleteObject(fnt_Midd);
	if (fnt_Big)
		DeleteObject(fnt_Big);
	if (g_FontOptions)
		DeleteObject(g_FontOptions);

	std::cout << "Interface: Shutdown Ok!" << std::endl;
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
	g_SavedHuntArea.clear();
	if (MenuHunt[0].Selected >= 0 && MenuHunt[0].Selected < (int)g_AreaInfo.size())
		g_SavedHuntArea = g_AreaInfo[MenuHunt[0].Selected].m_ProjectName;
	auto mask = [](const MenuSet& set) {
		unsigned long long m = 0;
		for (size_t i = 0; i < set.Item.size() && i < 64; i++)
			if (set.Item[i].second) m |= 1ULL << i;
		return m;
	};
	g_SavedHuntDinos = mask(MenuHunt[1]);
	g_SavedHuntWeapons = mask(MenuHunt[2]);
	g_SavedHuntUtils = mask(MenuHunt[3]);
	g_SavedHuntTime = g_TimeOfDay;
	g_HasSavedHunt = true;
}
// Reapply the persisted setup after the entry defaults. Everything is
// validated against the current data, so mod list changes can only fall
// back to defaults, never select out of range. Saved at launch, so a
// restored setup always corresponds to a launchable one.
static void RestoreHuntSelections()
{
	if (!g_HasSavedHunt) return;
	if (!g_SavedHuntArea.empty()) {
		for (size_t i = 0; i < g_AreaInfo.size(); i++)
			if (g_AreaInfo[i].m_ProjectName == g_SavedHuntArea) { MenuHunt[0].Selected = (int)i; break; }
	}
	auto apply = [](MenuSet& set, unsigned long long m) {
		for (size_t i = 0; i < set.Item.size() && i < 64; i++)
			set.Item[i].second = ((m >> i) & 1ULL) != 0;
	};
	apply(MenuHunt[1], g_SavedHuntDinos);
	apply(MenuHunt[2], g_SavedHuntWeapons);
	apply(MenuHunt[3], g_SavedHuntUtils);
	if (g_SavedHuntTime >= HUNT_DAWN && g_SavedHuntTime <= HUNT_NIGHT && g_SavedHuntTime != g_TimeOfDay) {
		g_MenuItem.SetIsElementSet(g_TimeOfDay + 1, false);
		g_TimeOfDay = g_SavedHuntTime;
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
void MenuEventStart(int32_t menu_state)
{
	g_MenuItem.ResetElementSet();

	switch (menu_state) {
	case MENU_REGISTER: {
		MenuRegistry.Offset = 0;
		MenuRegistry.Count = 0;
		MenuRegistry.AddItem("");
		g_LastListClickTime = 0;
		g_LastListClickIndex = -1;
		LoadProfileSummaries();
	} break;
	case MENU_OPTIONS: {
		g_WaitKey = -1;

		for (auto m = 0U; m < OPT_MAX; m++) {
			MenuOptions[m].Hilite = -1;
			MenuOptions[m].Selected = -1;
		}
	} break;
	case MENU_MAIN: {
		MenuAudioStartAmbient();
	} break;
		/****************************************************
		Hunt License Menu */
	case MENU_HUNT:
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
	} break;
	}
}

void LoadGameMenu(int32_t menu)
{
	std::string	mf_off = "";
	std::string	mf_on = "";
	std::string	mf_map = "";

	switch (menu) {
	case MENU_REGISTER:
	{
		mf_off = "HUNTDAT/MENU/MENUR.TGA";
		mf_on = "HUNTDAT/MENU/MENUR_ON.TGA";
		mf_map = "HUNTDAT/MENU/MR_MAP.RAW";
	} break;
	case MENU_REGISTRY_DELETE:
	{
		mf_off = "HUNTDAT/MENU/MENUD.TGA";
		mf_on = "HUNTDAT/MENU/MENUD_ON.TGA";
		mf_map = "HUNTDAT/MENU/MD_MAP.RAW";
	} break;
	case MENU_REGISTRY_WAIVER:
	{
		mf_off = "HUNTDAT/MENU/MENUL.TGA";
		mf_on = "HUNTDAT/MENU/MENUL_ON.TGA";
		mf_map = "HUNTDAT/MENU/ML_MAP.RAW";
	} break;
	case MENU_MAIN:
	{
		mf_off = "HUNTDAT/MENU/MENUM.TGA";
		mf_on = "HUNTDAT/MENU/MENUM_ON.TGA";
		mf_map = "HUNTDAT/MENU/MAIN_MAP.RAW";
	} break;
	case MENU_HUNT:
	{
		mf_off = "HUNTDAT/MENU/MENU2.TGA";
		mf_on = "HUNTDAT/MENU/MENU2_ON.TGA";
		mf_map = "HUNTDAT/MENU/M2_MAP.RAW";
	} break;
	case MENU_OPTIONS:
	{
		mf_off = "HUNTDAT/MENU/OPT_OFF.TGA";
		mf_on = "HUNTDAT//MENU/OPT_ON.TGA";
		mf_map = "HUNTDAT/MENU/OPT_MAP.RAW";
	} break;
	case MENU_CREDITS:
	{
		mf_off = "HUNTDAT/MENU/CREDITS.TGA";
		mf_on = "";
		mf_map = "";
	} break;
	case MENU_STATISTICS:
	{
		mf_off = "HUNTDAT/MENU/MENUS.TGA";
		mf_on = "";
		mf_map = "";
	} break;
	case MENU_QUIT:
	{
		mf_off = "HUNTDAT/MENU/MENUQ.TGA";
		mf_on = "HUNTDAT/MENU/MENUQ_ON.TGA";
		mf_map = "HUNTDAT/MENU/MQ_MAP.RAW";
	} break;
	default:
	{
		throw std::invalid_argument("Invalid argument: unsigned int menu.\nExpected a member of MenuStateEnum!");
	} break;
	}

	TargaImage tga;

	if (LoadMenuBackground(mf_off, tga)) {
		memcpy(g_MenuItem.m_Image, tga.m_Data, (800 * 2) * 600);
	}
	else {
		throw std::runtime_error("Failed to open the file...");
		return;
	}

	if (!mf_on.empty()) {
		if (LoadMenuBackground(mf_on, tga)) {
			memcpy(g_MenuItem.m_Image_On, tga.m_Data, (800 * 2) * 600);
		}
		else {
			throw std::runtime_error("Failed to open the file...");
			return;
		}
	}

	if (!mf_map.empty()) {
		std::ifstream fs(mf_map, std::ios::binary);
		if (fs.is_open())
			fs.read(reinterpret_cast<char*>(g_MenuItem.m_Image_Map), 400 * 300);
	}
	else {
		memset(g_MenuItem.m_Image_Map, 0, 400 * 300);
	}
}

/*
The registry name-list rows share one hit-test rectangle between the
draw-side hover highlight and the input-side double-click test.
*/
bool RegistryRowContains(const POINT& p, unsigned row)
{
	return p.x >= 308 && p.y >= (368 + (16 * row)) && p.x <= 408 && p.y <= (368 + (16 * row) + 16);
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
void MenuEventInput(int32_t menu)
{
	if (!g_KeyboardUsed) return;

	uint8_t id = g_MenuItem.GetID(g_CursorPos.x / 2, g_CursorPos.y / 2);

	if (g_KeyboardState[VK_LBUTTON] & 128) {
	}

	if (menu == MENU_CREDITS)
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
	else if (menu == MENU_REGISTER)
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
	/*
	*/
	else if (menu == MENU_REGISTRY_DELETE)
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
	/*
	*/
	else if (menu == MENU_REGISTRY_WAIVER)
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
	else if (menu == MENU_OPTIONS) {
		if (g_WaitKey >= 0)
		{
			AcceptNewKey();
		}
		else if (id == 4)
		{
			if (g_KeyboardState[VK_LBUTTON] & 128)
			{
				WaitForMouseRelease();
				MenuAudioPlayClick();
				TrophySave(g_UserProfile); // Save all the settings
				ChangeMenuState(MENU_MAIN);
			}
		}
		else
		{
			for (int m = 0; m < OPT_MAX; m++)
			{
				MenuSet& mo = MenuOptions[m];
				if (IsPointInRect(g_CursorPos, mo.Rect))
				{
					g_MenuItem.SetIsElementSet(m + 1, true);

					// Use layout system for hit detection
					Layout::Column* col = nullptr;
					if (m == OPT_GAME) col = &g_GameLabels;
					else if (m == OPT_KEYBINDINGS) col = &g_ControlsLabels;
					else if (m == OPT_VIDEO) col = &g_VideoLabels;

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

					if (g_KeyboardState[VK_LBUTTON] & 128) // Left Click
					{
						if (m == OPT_GAME)
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
						else if (m == OPT_KEYBINDINGS) { // Left Click
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
						else if (m == OPT_VIDEO) { // Left Click
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
					}
				}
				else
				{
					g_MenuItem.SetIsElementSet(m + 1, false);
					mo.Hilite = -1;
				}
			}
		}
	}
	else if (menu == MENU_HUNT) {
		if (g_KeyboardState[VK_ESCAPE] & 128)
		{
			ChangeMenuState(MENU_QUIT);
		}

		// Mouse hover
		if (IsPointInRect(g_CursorPos, MenuHunt[0].Rect))
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
				if (g_AreaInfo[index].m_Price >= 1000 && g_UserProfile.Score < 1000)
					g_HuntSelectPic = &g_AreaInfo[index].m_ThumbnailHidden;
				else
					g_HuntSelectPic = &g_AreaInfo[index].m_Thumbnail;
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
		else if (IsPointInRect(g_CursorPos, MenuHunt[1].Rect))
		{
			HuntBudget score = RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit);
			int yd = g_CursorPos.y - MenuHunt[1].Rect.top;

			unsigned index = yd / 16;
			unsigned dataIndex = HuntListDataIndex(index, MenuHunt[1].Offset);

			if (dataIndex < g_DinoList.size())
			{
				if (g_DinoInfo[g_DinoList[dataIndex]].m_Price >= 1000 && g_UserProfile.Score < 1000)
					g_HuntSelectPic = &g_DinoInfo[g_DinoList[dataIndex]].m_ThumbnailHidden;
				else
					g_HuntSelectPic = &g_DinoInfo[g_DinoList[dataIndex]].m_Thumbnail;
				g_HuntInfo.first = 1; // Dinos
				g_HuntInfo.second = dataIndex;

				if ((g_KeyboardState[VK_LBUTTON] & 128))
				{
					WaitForMouseRelease();
					MenuAudioPlayClick();

					if (score >= g_DinoInfo[g_DinoList[dataIndex]].m_Price && !MenuHunt[1].Item[dataIndex].second)
					{
						MenuHunt[1].Item[dataIndex].second = true;
					}
					else
					{
						MenuHunt[1].Item[dataIndex].second = false;
					}

					g_ScoreDebit = CalculateDebit();
				}
			}
		}
		else if (IsPointInRect(g_CursorPos, MenuHunt[2].Rect))
		{
			HuntBudget score = RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit);
			int yd = g_CursorPos.y - MenuHunt[2].Rect.top;

			unsigned index = HuntListDataIndex(yd / 16, MenuHunt[2].Offset);

			if (index < MenuHunt[2].Item.size())
			{
				if (g_WeapInfo[index].m_Price >= 1000 && g_UserProfile.Score < 1000)
					g_HuntSelectPic = &g_WeapInfo[index].m_ThumbnailHidden;
				else
					g_HuntSelectPic = &g_WeapInfo[index].m_Thumbnail;
				g_HuntInfo.first = 2; // Weapons
				g_HuntInfo.second = index;

				if ((g_KeyboardState[VK_LBUTTON] & 128))
				{
					WaitForMouseRelease();
					MenuAudioPlayClick();

					if (score >= g_WeapInfo[index].m_Price && !MenuHunt[2].Item[index].second)
					{
						MenuHunt[2].Item[index].second = true;
					}
					else
					{
						MenuHunt[2].Item[index].second = false;
					}

					g_ScoreDebit = CalculateDebit();
				}
			}
		}
		else if (IsPointInRect(g_CursorPos, MenuHunt[3].Rect))
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

					// Same affordability gate as dinos/weapons: selecting
					// requires covering the price, deselecting is always
					// free -- so the account can never be driven negative.
					if (score >= g_UtilInfo[accIndex].m_Price && !MenuHunt[3].Item[accIndex].second)
					{
						MenuHunt[3].Item[accIndex].second = true;
					}
					else
					{
						MenuHunt[3].Item[accIndex].second = false;
					}

					g_ScoreDebit = CalculateDebit();
				}
			}
		}
		else if (id == 6) {
			// Observer info panel
			g_HuntSelectPic = &g_ObserverInfo.m_Thumbnail;
			g_HuntInfo.first = 3; // Accessories
			g_HuntInfo.second = 9999;
		}

		// Left Mouse Click
		if (g_KeyboardState[VK_LBUTTON] & 128)
		{
			if (id >= 1 && id <= 6)
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
			else if (id == 7) // Back
			{
				WaitForMouseRelease();
				MenuAudioPlayClick();
				ChangeMenuState(MENU_MAIN);
			}
			else if (id == 8) // Hunt/Next
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

				int din = 0;
				int wep = 0;

				// Calculate the dinosaur flags
				for (unsigned i = 0; i < g_DinoList.size(); i++)
				{
					if (MenuHunt[1].Item[i].second)
						din |= 1 << i;
				}

				// Calculate the weapon flags
				for (unsigned i = 0; i < g_WeapInfo.size(); i++)
				{
					if (MenuHunt[2].Item[i].second)
						wep |= 1 << i;
				}

				// Initialise the command line parameters through the tested helper. For
				// slot six, this uses the basename whose files actually exist.
				if (MenuHunt[0].Selected < 0 ||
					MenuHunt[0].Selected >= static_cast<int>(g_AreaInfo.size()))
				{
					ShowErrorMessage("The selected hunt is invalid.");
					return;
				}
				const AreaInfo& launchArea = g_AreaInfo[MenuHunt[0].Selected];
				HuntLaunchRequest launchRequest;
				launchRequest.projectName = launchArea.m_ProjectName;
				launchRequest.mapFile = launchArea.m_MapFile;
				launchRequest.registration = g_UserProfile.RegNumber;
				launchRequest.dinoFlags = din;
				launchRequest.weaponFlags = wep;
				launchRequest.timeOfDay = g_TimeOfDay;

				// The engine opens <basename>.map and <basename>.rsc from this one
				// name, so refuse here with the missing path named rather than
				// launching into "Error opening resource file".
				{
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
						return;
					}
				}
				std::string launchArguments;
				if (!BuildHuntLaunchArguments(launchRequest, launchArguments))
				{
					ShowErrorMessage("The selected hunt has invalid launch data.");
					return;
				}
				// The base arguments must be the first write into an empty stream;
				// MakeLaunchParamStream owns that contract.
				std::stringstream params = MakeLaunchParamStream(launchArguments);

#ifdef _iceage
				// Ice Age resupply
				if (MenuHunt[3].Item[4].second)
					params << " " << g_UtilInfo[4].m_Command;
#endif //_iceage

				if (MenuHunt[3].Item[kAccDouble].second)
					params << " " << g_UtilInfo[kAccDouble].m_Command;

				// Night vision goggles
				{
					if (kAccNV < static_cast<int>(MenuHunt[3].Item.size()) && MenuHunt[3].Item[kAccNV].second)
						params << " " << g_UtilInfo[kAccNV].m_Command;
				}

				// These three persist as owned regardless of selection; only
				// the launch flags depend on what is ticked.
				g_Options.CamoMode = true;
				g_Options.RadarMode = true;
				g_Options.ScentMode = true;
				if (MenuHunt[3].Item[kAccCamo].second) params << " -camo";
				if (MenuHunt[3].Item[kAccRadar].second) params << " -radar";
				if (MenuHunt[3].Item[kAccScent].second) params << " -scent";

				// Tranquilizers
				{
					if (kAccTranq < static_cast<int>(MenuHunt[3].Item.size()) && MenuHunt[3].Item[kAccTranq].second)
						params << " " << g_UtilInfo[kAccTranq].m_Command;
				}

				// Flashlight: like night vision this only unlocks the in-game
				// toggle; the accessory itself carries no quieter cheat behind it.
				{
					if (static_cast<int>(MenuHunt[3].Item.size()) > kAccFlashlight && MenuHunt[3].Item[kAccFlashlight].second)
						params << " " << g_UtilInfo[kAccFlashlight].m_Command;
				}

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

				std::stringstream renderer("");
				renderer << g_RendererFile[g_Options.RenderAPI] << ".ren";

				// Observer mode is an exploration session and deliberately permits an
				// empty creature/weapon loadout, matching the original launcher.
				if (g_ObserverMode || (wep && din))
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
		}
	}
	else if (menu == MENU_MAIN)
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
				else if (id == 4) { ChangeMenuState(MENU_CREDITS); }
				else if (id == 5) { ChangeMenuState(MENU_QUIT); }
				else if (id == 6) { ChangeMenuState(MENU_STATISTICS); }
			}
		}
	}
	else if (menu == MENU_STATISTICS)
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
	else if (menu == MENU_QUIT)
	{
		if (g_KeyboardState[VK_LBUTTON] & 128) {
			WaitForMouseRelease();
			MenuAudioPlayClick();

			if (id == 1)      RequestMenuExit(0);
			else if (id == 2) ChangeMenuState(MENU_MAIN);
		}
	}
}

// TODO: Move these to Hunt.h
#define FPS_TARGET 60L
#define FRAME_TIME_DELTA (1000L / FPS_TARGET) // The time in milliseconds a frame takes to process to achieve FPS_TARGET

int64_t g_PrevFrameTime = 0;
#ifdef _DEBUG
int32_t g_Frames = 0;
int32_t g_FramesPerSecond = 0;
int64_t g_PrevFrameCountTime = 0;
#endif

/*
Perform per-frame/tick update of the menus
*/
void ProcessMenu()
{
	NormalizeRendererOption();

	GetCursorPos(&g_CursorPos);
	ScreenToClient(hwndMain, &g_CursorPos);

	// Scale mouse coordinates from scaled window to internal 800x600 space
	if (g_ScaleX > 0.0f)
		g_CursorPos.x = static_cast<int>(g_CursorPos.x / g_ScaleX);
	if (g_ScaleY > 0.0f)
		g_CursorPos.y = static_cast<int>(g_CursorPos.y / g_ScaleY);

	// Restrict the virtual cursor to the client area
	if (g_CursorPos.x < 0) g_CursorPos.x = 0;
	if (g_CursorPos.y < 0) g_CursorPos.y = 0;
	if (g_CursorPos.x >= MENU_BASE_WIDTH) g_CursorPos.x = MENU_BASE_WIDTH - 1;
	if (g_CursorPos.y >= MENU_BASE_HEIGHT) g_CursorPos.y = MENU_BASE_HEIGHT - 1;

	// Get the keyboard state
	if (GetActiveWindow() == hwndMain) {
		g_KeyboardUsed = GetKeyboardState(g_KeyboardState);
	}
	else {
		memset(g_KeyboardState, 0, 256);
		g_KeyboardUsed = false;
	}

	// Trigger the Start() event of the menu if applicable
	if (g_PrevMenuState != g_MenuState) {
		MenuEventStart(g_MenuState);
		g_PrevMenuState = g_MenuState;
	}

	// Handle input from mouse/keyboard
	MenuEventInput(g_MenuState);

	// Clear the video buffer and then draw the menu background
	InterfaceClear(HiColor(0, 0, 10));
	DrawMenuBg(g_MenuItem);

	// Draw menus
	switch (g_MenuState)
	{
	case MENU_REGISTRY_DELETE: DrawMenuRegistry(); break;
	case MENU_REGISTER: DrawMenuRegistry(); break;
	case MENU_MAIN: DrawMenuProfile(); break;
	case MENU_STATISTICS: DrawMenuStatistics(); break;
	case MENU_QUIT: DrawMenuProfile(); break;
	case MENU_OPTIONS: DrawMenuOptions(); break;
	case MENU_CREDITS: DrawMenuCredits(); break;
	case MENU_HUNT: DrawMenuHunt(); break;
	}

#ifdef _DEBUG
	// Perform some framerate metric stuff, only for [Debug] builds though
	g_Frames++;

	InterfaceSetFont(fnt_Small);

	int64_t t = Timer::GetTime();
	int64_t t_diff = t - g_PrevFrameTime;

	std::stringstream ss;
	ss << "FPS: " << g_FramesPerSecond;
	DrawTextShadow(2, 2, ss.str(), RGB(255, 60, 60));
	ss.str(""); ss.clear();

	ss << "FT:  " << t_diff << "ms";
	DrawTextShadow(2, 2 + 14, ss.str(), RGB(255, 60, 60));
	ss.str(""); ss.clear();

	ss << "XY:  " << g_CursorPos.x << "x" << g_CursorPos.y;
	DrawTextShadow(2, 2 + 28, ss.str(), RGB(255, 60, 60));

	if ((t - g_PrevFrameCountTime) >= 1000) {
		g_FramesPerSecond = g_Frames;
		g_Frames = 0;
		g_PrevFrameCountTime = t;
	}

	g_PrevFrameTime = t;
#endif

	// Draw the GDI buffer to the window
	InterfaceBlt();
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

