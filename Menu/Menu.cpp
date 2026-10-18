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

static void NormalizeRendererOption()
{
	const int32_t normalized = NormalizeMenuRenderAPI(g_Options.RenderAPI);
	if (normalized != g_Options.RenderAPI) {
		g_Options.RenderAPI = normalized;
		SaveConfig();
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
