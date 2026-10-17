/***************************************************
* AtmosFear 2.1
* Characters.cpp
*
* Resources, Files and Memory Management
*
*/

#include "Hunt.h"
#include "Loaders/TgaFormat.h"
#include "../Hunt/Audio/WaveReader.h"

#include <iostream>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <fstream>
#include <memory>
#include <limits>
#include <string>
#include <type_traits>
#include <cstdlib>
#include <cctype>
#include <vector>
#include "../Hunt/Core/Flashlight.h"
#include "Core/ConfigMerge.h"
#include "Core/ConfigPath.h"
#include "Core/ConfigScan.h"
#include "Core/ConfigText.h"
#include "ListMath.h"
#include "Loaders/LoadDiagnostics.h"
#include "Loaders/LoadValidate.h"
#include "Loaders/ScriptValueParse.h"

// An unreadable but existing redirect should not suppress a stock .nfo;
// for newer accessories the built-in blurb remains the last fallback.
static void LoadRowDescription(std::vector<std::string>& text,
                               const std::string& scoreKey, const char* name, const char* cmd,
                               const std::string& defaultPath, const std::vector<std::string>* builtin)
{
	const std::string path = LookupAccessoryText(scoreKey, name, cmd, defaultPath);
	if (path != defaultPath) {
		if (LoadText(text, path)) return;
		LoadDiagnostics::Instance().Report("accessory text", path.c_str(),
			"override file could not be read (using the default description file)", nullptr);
		text.clear();
	}
	LoadAccessoryDescription(text, defaultPath, builtin);
}

void LoadResources()
{
	//TODO use this and ignore old _RES stuff

	size_t accIdx = 0;
	auto addUtil = [&](const char* name, const char* descFile, const char* cmd, const char* scoreKey, float scoreDefault, int32_t defaultPrice, const char* pic, const std::vector<std::string>* builtinDesc = nullptr) {
		// Fresh picture per row: a missing file must not inherit the prior row's art.
		UtilInfo ui;
		ui.m_Name = name;
		// A mod-supplied .nfo always wins; the built-in text only covers installs
		// whose HUNTDAT predates the accessory, where the panel would otherwise be
		// blank and LoadText() would print a failure. An 'accessory_descriptions
		// { }' entry redirects the row to another text file first.
		LoadRowDescription(ui.m_Description, scoreKey, name, cmd, descFile, builtinDesc);
		ui.m_Command = cmd;
		ui.m_ScoreMod = LookupAccessoryScoreMod(scoreKey, scoreDefault);
		const int32_t positional = (accIdx < g_AccessoryPrices.size()) ? g_AccessoryPrices[accIdx] : defaultPrice;
		ui.m_Price = LookupAccessoryPrice(scoreKey, name, cmd, positional);
		accIdx++;
		// Icon override hook: mods can point any accessory at their own art via
		// the 'accessory_icons { }' block. Both blocks address the row by its
		// score key ('scent', 'tranq'), its display name or its legacy
		// command-derived key -- see the accessory key comment above.
		// The shop icon is optional: some accessories (e.g. tranquilizers in
		// both stock C2 and Triassic) have no dedicated image, and the
		// original game shows them with an empty icon box. Never guess a
		// different EQUIPn image - a wrong icon is worse than none.
		LoadPicture(ui.m_Thumbnail, LookupAccessoryIcon(scoreKey, name, cmd, pic));
		g_UtilInfo.push_back(ui);
	};

	addUtil("Camouflage",  "huntdat/menu/txt/camoflag.nfo", "-camo",  "camo", 0.85f, 30,  "huntdat/menu/pics/equip1.tga");
	addUtil("Radar",       "huntdat/menu/txt/radar.nfo",    "-radar", "radar", 0.70f, 40,  "huntdat/menu/pics/equip2.tga");
	addUtil("Cover scent", "huntdat/menu/txt/scent.nfo",    "",       "scent", 0.80f, 20,  "huntdat/menu/pics/equip3.tga");
	addUtil("Double ammo", "huntdat/menu/txt/double.nfo",   "-double","double",1.0f,  50,  "huntdat/menu/pics/equip4.tga");

	// Night vision is a modernization feature and is always available, even
	// when a mod ships no nightvis.nfo or equip_nv.tga (the icon is optional;
	// the row shows with an empty icon box like other icon-less accessories).
	// A built-in description covers mods without the .nfo.
	UtilInfo ui;
	ui.m_Name = "Night vision";
	const std::vector<std::string> nightVisionDescription = {
		"Accessory : Night Vision Goggles",
		"",
		"Toggle a green-tinted night vision view during",
		"night hunts. Press the configured key (default: N)",
		"to switch it on and off. Night vision does not",
		"affect your score."
	};
	LoadRowDescription(ui.m_Description, "nightvision", "Night vision", "-nightvision",
		"huntdat/menu/txt/nightvis.nfo", &nightVisionDescription);
	ui.m_Command = "-nightvision";
	ui.m_ScoreMod = LookupAccessoryScoreMod("nightvision", 1.0f); // neutral
	// Price rule: consume a positional acces= line ONLY when the mod priced 6
	// accessories (stock C2: camo/radar/scent/double/NV/tranq). Mods with
	// the original 5 lines (camo/radar/scent/double/tranq) keep all of them
	// for those accessories and NV falls back to its fixed default - otherwise
	// NV would steal the tranquilizer's price and tranq would lose its custom
	// price (it would fall back to 60 instead of prices[4]).
	if (g_AccessoryPrices.size() > 5) {
		ui.m_Price = g_AccessoryPrices[4]; // 5th line = NV price on 6-line mods
		accIdx++; // NV consumed a slot, so tranq reads prices[5]
	} else {
		ui.m_Price = 50; // fixed default (original feature commit)
		// Do NOT advance accIdx: tranq still reads prices[4] on 5-line mods.
	}
	ui.m_Price = LookupAccessoryPrice("nightvision", "Night vision", "-nightvision", ui.m_Price);
	// Addressable by both override blocks as 'nightvision' / 'Night vision'.
	LoadPicture(ui.m_Thumbnail, LookupAccessoryIcon("nightvision", "Night vision", "-nightvision", "huntdat/menu/pics/equip_nv.tga"));
	g_UtilInfo.push_back(ui);

	addUtil("Tranquilizers","huntdat/menu/txt/tranq.nfo",  "-tranq -tranquilizer", "tranq", 1.25f, 60, "huntdat/menu/pics/equip6.tga");

	// Flashlight: a modernization accessory, priced like the other entries but
	// listed last on purpose. Stock _MENU.TXT ships six 'acces' price lines
	// (camo/radar/scent/double/NV/tranq), so accIdx is already 6 here and there
	// is no seventh slot to consume -- it must fall back to its own default
	// rather than steal a line meant for a future accessory. The .nfo and icon
	// are both optional (no stock data ships either), so the row always shows,
	// with a built-in description and an empty icon box as the worst case.
	// Lines stay under ~48 characters: the info panel draws them unwrapped at
	// x=424 (Menu.cpp), so longer lines run off the 800-wide menu buffer.
	addUtil("Flashlight", "huntdat/menu/txt/flashlight.nfo", "-flashlight", "flashlight", 1.0f, 30, "huntdat/menu/pics/equip_fl.tga", &kFlashlightDescription);

	// Observer info (used for info panel display, not in equipment list)
	g_ObserverInfo.m_Name = "Observer";
	LoadRowDescription(g_ObserverInfo.m_Description, "observe", "Observer", "-observe -observer",
		"huntdat/menu/txt/observe.nfo", nullptr);
	g_ObserverInfo.m_Command = "-observe -observer";
	g_ObserverInfo.m_ScoreMod = LookupAccessoryScoreMod("observe", 1.0f); // neutral


	LoadWave(g_MenuSound_Go, "huntdat/soundfx/menugo.wav");
	LoadWave(g_MenuSound_Ambient, "huntdat/soundfx/menuamb.wav");
	LoadWave(g_MenuSound_Move, "huntdat/soundfx/menumov.wav");
	LoadWave(g_MenuSound_Type, "huntdat/soundfx/type.wav");
	LoadWave(g_MenuSound_TypeGo, "huntdat/soundfx/typego.wav");

	// Icon and description overrides (and malformed TGA art) are resolved in
	// this function, after LoadResourcesScript() has printed its own summary.
	LoadDiagnostics& diagnostics = LoadDiagnostics::Instance();
	if (diagnostics.Count() > 0) {
		std::cout << diagnostics.Summary();
		diagnostics.Clear();
	}
}


void ReleaseResources()
{
	g_WeapInfo.clear();
	g_DinoInfo.clear();
	g_AreaInfo.clear();
}
void Options::Default()
{
	this->Aggression = 128;
	this->Density = 128;
	this->Sensitivity = 128;
	this->Resolution = 5;
	this->DisplayMode = 2; // Borderless fullscreen (legacy menu launch default)
	this->Fog = true;
	this->Textures = 1;
	this->ViewRange = kViewOptDefault;
	this->ObjectDetail = kObjectDetailDefault;
	this->Brightness = 128;
	this->Shadows = true;
	this->MouseSensitivity = 128;
	this->FOV = kFovDefault;
	// -- Set default controls
	this->KeyMap.fkForward = 'W';
	this->KeyMap.fkBackward = 'S';
	this->KeyMap.fkSLeft = 'A';
	this->KeyMap.fkSRight = 'D';
	this->KeyMap.fkFire = VK_LBUTTON;
	this->KeyMap.fkShow = VK_RBUTTON;
	this->KeyMap.fkJump = VK_SPACE;
	this->KeyMap.fkCall = VK_MENU;
	this->KeyMap.fkBinoc = 'B';
	this->KeyMap.fkCrouch = 'C';
	this->KeyMap.fkRun = VK_LSHIFT;
	this->KeyMap.fkReload = 'R';
	this->KeyMap.fkResupply = 'T';
	this->KeyMap.fkHoldBreath = VK_LCONTROL;
	this->KeyMap.fkFiringMode = 'V';
	this->KeyMap.fkStrafe = 'G'; // Rack / Pump (also used for strafe movement by the MEE engine)
#ifdef _iceage
	this->KeyMap.fkSupply = 'O';
#endif //_iceage
	this->MouseInvert = false;
	this->ScentMode = false;
	this->CamoMode = false;
	this->RadarMode = false;
	this->TranqMode = false;
	this->AlphaColorKey = 1;
	this->OptSys = 1;
	this->SoundAPI = AUDIO_OPENALSOFT; // Default to OpenAL Soft
	this->RenderAPI = 1; // Default to OpenGL
	this->OptFpsLimit = kFpsLimitDefault;
	this->VerboseLogging = false;
	this->NightVisionKey = 0x4E; // Default: 'N' key
	this->FlashlightKey = kFlashlightKeyDefault; // Default: 'F' key
}
