// ProfileIO.cpp -- binary trophy/profile save IO (split from Resources.cpp)
// ==========================================================================
// The legacy fixed-size profile blob (1660 bytes on C2, 1664 on Ice Age)
// that the menu writes and the engine's trophy loader reads.
// ==========================================================================

#include "Hunt.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "Core/Flashlight.h"

void TrophyLoad(Profile& profile, int pr)
{
	//PlayerProfile.RegNumber
	memset(&profile, 0, sizeof(Profile));
	profile.RegNumber = pr;

	std::stringstream fname;
	fname << "trophy" << std::setfill('0') << std::setw(2) << profile.RegNumber << ".sav";

	std::ifstream fs(fname.str(), std::ios::binary);

	if (!fs.is_open()) {
		std::stringstream ss;
		ss << "Unable to find the save file: " << fname.str();
		throw std::runtime_error(ss.str());
		return;
	}

	// Check version
	fs.seekg(0, std::ios::end);
	auto file_size = fs.tellg();

#ifdef _iceage
	if (file_size != 1664)
	{
		ShowErrorMessage("Not a compatible Carnivores: Ice Age save file!");
		fs.close();
		return;
	}
#else
	if (file_size != 1660)
	{
		ShowErrorMessage("Not a compatible Carnivores 2 save file!");
		fs.close();
		return;
	}
#endif

	fs.seekg(0, std::ios::beg);

	fs.read(reinterpret_cast<char*>(&profile), sizeof(Profile));

	fs.read(reinterpret_cast<char*>(&g_Options.Aggression), 4);
	fs.read(reinterpret_cast<char*>(&g_Options.Density), 4);
	fs.read(reinterpret_cast<char*>(&g_Options.Sensitivity), 4);

	fs.read(reinterpret_cast<char*>(&g_Options.Resolution), 4);
	// The old menu (StartLegacy.exe) used a hardcoded 8-entry resolution
	// table (320x240..1600x1200). Our dynamic list may differ. Convert
	// the old index to an actual resolution, then find the matching index
	// in the current list.
	{
		// Old hardcoded table (indices 0-7)
		static const struct { int w, h; } kOldRes[] = {
			{320,240}, {400,300}, {512,384}, {640,480},
			{800,600}, {1024,768}, {1280,1024}, {1600,1200}
		};
		int oldIdx = g_Options.Resolution;
		if (oldIdx >= 0 && oldIdx < 8) {
			int w = kOldRes[oldIdx].w;
			int h = kOldRes[oldIdx].h;
			// Find the matching entry in the dynamic list
			bool found = false;
			for (int r = 0; r < g_ResCount; r++) {
				if (g_ResolutionList[r].w == w && g_ResolutionList[r].h == h) {
					g_Options.Resolution = r;
					found = true;
					break;
				}
			}
			// If the old resolution isn't available (e.g. 320x240, 512x384),
			// fall back to 800x600 or the first available resolution.
			if (!found) {
				g_Options.Resolution = 0;
				for (int r = 0; r < g_ResCount; r++) {
					if (g_ResolutionList[r].w == 800 && g_ResolutionList[r].h == 600) {
						g_Options.Resolution = r;
						break;
					}
				}
			}
		}
	}
	// Bool fields are 1 byte in the struct but 4 bytes on disk.
	// Read into temporary int32_t to avoid adjacent-field overflow.
	{ int32_t tmp; fs.read(reinterpret_cast<char*>(&tmp), 4); g_Options.Fog = (bool)tmp; }
	fs.read(reinterpret_cast<char*>(&g_Options.Textures), 4);
	fs.read(reinterpret_cast<char*>(&g_Options.ViewRange), 4);
	g_Options.ViewRange = ClampViewOpt(g_Options.ViewRange);
	{ int32_t tmp; fs.read(reinterpret_cast<char*>(&tmp), 4); g_Options.Shadows = (bool)tmp; }
	fs.read(reinterpret_cast<char*>(&g_Options.MouseSensitivity), 4);
	fs.read(reinterpret_cast<char*>(&g_Options.Brightness), 4);

	fs.read(reinterpret_cast<char*>(&g_Options.KeyMap), sizeof(TKeyMap));
	{ int32_t tmp; fs.read(reinterpret_cast<char*>(&tmp), 4); g_Options.MouseInvert = (bool)tmp; }

	{ int32_t tmp; fs.read(reinterpret_cast<char*>(&tmp), 4); g_Options.ScentMode = (bool)tmp; }
	{ int32_t tmp; fs.read(reinterpret_cast<char*>(&tmp), 4); g_Options.CamoMode = (bool)tmp; }
	{ int32_t tmp; fs.read(reinterpret_cast<char*>(&tmp), 4); g_Options.RadarMode = (bool)tmp; }
	{ int32_t tmp; fs.read(reinterpret_cast<char*>(&tmp), 4); g_Options.TranqMode = (bool)tmp; }
	fs.read(reinterpret_cast<char*>(&g_Options.AlphaColorKey), 4);

	fs.read(reinterpret_cast<char*>(&g_Options.OptSys), 4);
	fs.read(reinterpret_cast<char*>(&g_Options.SoundAPI), 4);
	fs.read(reinterpret_cast<char*>(&g_Options.RenderAPI), 4);
	g_Options.RenderAPI = NormalizeMenuRenderAPI(g_Options.RenderAPI);
	g_Options.SoundAPI = NormalizeAudioBackend(g_Options.SoundAPI);

	// FOV and other extended settings are now in config.cfg, not here.
	// Set defaults; LoadConfig() will override if the config file exists.
	g_Options.FOV = kFovDefault;
	g_Options.ViewRange = ClampViewOpt(g_Options.ViewRange);
	g_Options.ObjectDetail = kObjectDetailDefault;
	g_Options.NightVisionKey = 0x4E; // Default: 'N' key
	g_Options.FlashlightKey = kFlashlightKeyDefault; // Default: 'F' key

	//Temporary:
	int r = profile.Rank;
	profile.Rank = RANK_BEGINNER;
	if (profile.Score >= 100) profile.Rank = RANK_ADVANCED;
	if (profile.Score >= 300) profile.Rank = RANK_MASTER;
	if (profile.Score >= 10000) profile.Rank = 1000;

	std::cout << "Profile Loaded." << std::endl;
}


void TrophySave(Profile& profile)
{
	std::stringstream fname;
	fname << "trophy" << std::setfill('0') << std::setw(2) << profile.RegNumber << ".sav";

	int r = profile.Rank;
	profile.Rank = RANK_BEGINNER;
	if (profile.Score >= 100) profile.Rank = RANK_ADVANCED;
	if (profile.Score >= 300) profile.Rank = RANK_MASTER;
	if (profile.Score >= 10000) profile.Rank = 1000;

	/*
	// Taken from Carnivores 1
	if (r != TrophyRoom.Rank) {
		if (profile.Rank == RANK_ADVANCED) MenuState = 112;
		if (profile.Rank == RANK_MASTER) MenuState = 113;
	}*/

	std::ofstream fs(fname.str(), std::ios::binary | std::ios::trunc);

	if (!fs.is_open()) {
		std::cout << "Profile: Error saving trophy!" << std::endl;
		return;
	}

	fs.write(reinterpret_cast<char*>(&profile), sizeof(Profile));

	fs.write(reinterpret_cast<char*>(&g_Options.Aggression), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.Density), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.Sensitivity), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.Resolution), 4);
	// Bool fields are 1 byte in the struct but 4 bytes on disk.
	// Write via temporary int32_t to avoid adjacent-field overflow.
	{ int32_t tmp = g_Options.Fog ? 1 : 0; fs.write(reinterpret_cast<char*>(&tmp), 4); }
	fs.write(reinterpret_cast<char*>(&g_Options.Textures), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.ViewRange), 4);
	{ int32_t tmp = g_Options.Shadows ? 1 : 0; fs.write(reinterpret_cast<char*>(&tmp), 4); }
	fs.write(reinterpret_cast<char*>(&g_Options.MouseSensitivity), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.Brightness), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.KeyMap), sizeof(TKeyMap));
	{ int32_t tmp = g_Options.MouseInvert ? 1 : 0; fs.write(reinterpret_cast<char*>(&tmp), 4); }
	{ int32_t tmp = g_Options.ScentMode ? 1 : 0; fs.write(reinterpret_cast<char*>(&tmp), 4); }
	{ int32_t tmp = g_Options.CamoMode ? 1 : 0; fs.write(reinterpret_cast<char*>(&tmp), 4); }
	{ int32_t tmp = g_Options.RadarMode ? 1 : 0; fs.write(reinterpret_cast<char*>(&tmp), 4); }
	{ int32_t tmp = g_Options.TranqMode ? 1 : 0; fs.write(reinterpret_cast<char*>(&tmp), 4); }
	fs.write(reinterpret_cast<char*>(&g_Options.AlphaColorKey), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.OptSys), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.SoundAPI), 4);
	fs.write(reinterpret_cast<char*>(&g_Options.RenderAPI), 4);

	// FOV and other extended settings live in config.cfg, not here.

	std::cout << "Profile Saved." << std::endl;
}

void TrophyDelete(uint32_t trophy_index)
{
	std::stringstream fname;
	fname << "trophy" << std::setfill('0') << std::setw(2) << trophy_index << ".sav";
	std::filesystem::path fp = std::filesystem::current_path();
	fp /= std::filesystem::path(fname.str());
	std::cout << "Deleting trophy: \'" << fp << "\'" << std::endl;
	std::filesystem::remove(fp);
}


void Profile::New(const std::string& name, int32_t index)
{
	if (index == -1)
		index = g_ProfileIndex;

	memset(Name, 0, 128);
	memcpy(Name, name.data(), name.length());
	RegNumber = index;
	Score = g_StartCredits;
	Rank = RANK_BEGINNER;
	memset(&Last, 0, sizeof(ProfileStats));
	memset(&Total, 0, sizeof(ProfileStats));
	memset(Body, 0, sizeof(TrophyItem) * 24);
}

// Fill g_Profiles[0..7] with the registry summaries the menu lists. This is
// the same fixed prefix TrophySave writes (name[128] + RegNumber/Score/Rank);
// missing files keep the beginner defaults. The stream stays in text mode, as
// it always was, so existing profiles read identically.
void LoadProfileSummaries()
{
	char tname[128];
	for (auto i = 0U; i < 8U; i++) {
		g_Profiles[i].m_Name = "";
		g_Profiles[i].m_RegNumber = i;
		g_Profiles[i].m_Rank = RANK_BEGINNER;
		g_Profiles[i].m_Score = 0U;

		std::stringstream sn;
		sn << "trophy" << std::setfill('0') << std::setw(2) << i << ".sav";

		std::ifstream fs(sn.str());
		if (!fs.is_open()) { continue; }

		fs.read(tname, 128);
		fs.read(reinterpret_cast<char*>(&g_Profiles[i].m_RegNumber), 4);
		fs.read(reinterpret_cast<char*>(&g_Profiles[i].m_Score), 4);
		fs.read(reinterpret_cast<char*>(&g_Profiles[i].m_Rank), 4);

		g_Profiles[i].m_Name = tname;
	}
}
