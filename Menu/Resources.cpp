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


class script_error : public std::exception
{
private:
	std::string m_What;
	uint32_t	m_Line;
	std::string m_Where;

public:

	script_error(const std::string& _what, const std::string& _where = "", uint32_t _line = 0) :
		m_What(_what),
		m_Line(_line),
		m_Where(_where)
	{}

	std::string what()
	{
		std::stringstream ss;
		ss << "Where: " << m_Where << "\r\n";
		ss << "At Line: " << m_Line << "\r\n";
		ss << m_What << "\r\n";
		return ss.str();
	}
};


uint32_t g_ScriptLine = 0;

static int ReadScriptIntValue(const char* value, const char* where)
{
	const ScriptIntResult result = ParseScriptIntStatus(value, 0);
	if (result.status == ScriptScalarStatus::Ok)
		return result.value;

	const char* reason = ScriptScalarStatusReason(result.status);
	LoadDiagnostics::Instance().Report("MenuResource", where, reason, nullptr);
	if (LoadDiagnostics::Instance().Strict())
		throw script_error("Expected a valid integer value.", where, g_ScriptLine);
	return result.value;
}

static float ReadScriptFloatValue(const char* value, const char* where)
{
	const ScriptFloatResult result = ParseScriptFloatStatus(value, 0.0f);
	if (result.status == ScriptScalarStatus::Ok)
		return result.value;

	const char* reason = ScriptScalarStatusReason(result.status);
	LoadDiagnostics::Instance().Report("MenuResource", where, reason, nullptr);
	if (LoadDiagnostics::Instance().Strict())
		throw script_error("Expected a valid finite float value.", where, g_ScriptLine);
	return result.value;
}

// Legacy integer-backed fields (health) were authored with decimal literals in
// stock data (health = 13.5); the old atoi path truncated them, so keep that
// behaviour here instead of rejecting the line outright.
static int ReadScriptLegacyIntValue(const char* value, const char* where)
{
	const ScriptIntResult result = ParseScriptLegacyIntStatus(value, 0);
	if (result.status == ScriptScalarStatus::Ok)
		return result.value;

	const char* reason = ScriptScalarStatusReason(result.status);
	LoadDiagnostics::Instance().Report("MenuResource", where, reason, nullptr);
	if (LoadDiagnostics::Instance().Strict())
		throw script_error("Expected a valid integer value.", where, g_ScriptLine);
	return result.value;
}

// The menu reads both _MENU.TXT and the legacy _RES.TXT, and their key
// spellings are not consistent between the two files ("smell" vs "smellK",
// "hear" vs "hearK"). The historical reader matched keys by substring, which
// is why a line like `file = 'models/main_hunt/para.car'` was read as the AI
// field; matching goes through the shared whole-key predicate in
// LoadValidate.h, relaxing only letter case (ScriptKeyIsIgnoreCase).
//
// One field can appear under more than one historical spelling (see above).
static bool MenuScriptKeyIsAny(const char* line, const char* keyA, const char* keyB)
{
	return ScriptKeyIsIgnoreCase(line, keyA) || ScriptKeyIsIgnoreCase(line, keyB);
}

static std::string ReadAssignedText(const char* value, const char* where)
{
	const char* text = nullptr;
	size_t length = 0;
	if (!FindQuotedValue(value, &text, &length))
		throw script_error("Expected a quoted text value.", where, g_ScriptLine);
	return std::string(text, length);
}


void ReadWeapons(FILE*);
void ReadCharacters(FILE*);
void ReadAccessories(FILE*);
void ReadAccessoryIcons(FILE*);
void ReadAccessoryDescriptions(FILE*);
void ReadAccessoryPrices(FILE*);
void LoadC2Maps();


/*
 * Path of the missing half of an area's <basename>.map/.rsc pair, or an empty
 * string when the pair is complete. The engine opens both files from the single
 * "prj=" token the menu passes (LaunchArgs.h -> Hunt/Loaders/Resources.cpp), so
 * a .map without its .rsc is a guaranteed halt with "Error opening resource
 * file". Checking the pair here is therefore the same test the launch runs, not
 * a stricter rule. The .map is reported first so callers can tell "no area at
 * all" from "half an area".
 */
std::string MissingAreaFile(const std::string& base)
{
	if (base.empty())
		return std::string();

	std::ifstream f;
	const std::string stem = "huntdat/areas/" + base;

	f.open(stem + ".map", std::ios::binary);
	if (!f.is_open())
		return stem + ".map";
	f.close();

	f.open(stem + ".rsc", std::ios::binary);
	if (!f.is_open())
		return stem + ".rsc";
	f.close();

	return std::string();
}


/*
! DEPRECATED !
This function is deprecated and set for removal in future versions
! WARNING !
This is a kludge meant to support the original games method of loading resources via the _RES file.

Construct an AreaInfo object and return it with the correct information stored for use by the menu,
this is a backwards compatability function to support the original vanilla _RES and file structure.
*/
AreaInfo MakeOldAreaInfo(int index, int price)
{
	std::stringstream ss;
	AreaInfo a;

	std::cout << "MakeOldAreaInfo(index=" << index << ", price=" << price << ")" << std::endl;

	// Set the cost (required score) rank and the dinosaurs available
	a.m_Price = price;
	a.m_Rank = RANK_BEGINNER; // Not used in vanilla
	a.m_DinosAvail.clear(); // Not used currently but empty would mean [all]

	// Load the description
	ss << "huntdat/menu/txt/area" << index << ".txt";
	std::cout << "  Desc file: " << ss.str();
	std::ifstream f(ss.str());

	if (f.is_open())
	{
		while (!f.eof())
		{
			std::string line = "";
			std::getline(f, line);
			a.m_Description.push_back(line);
		}
		f.close();
		std::cout << " -> OK (" << a.m_Description.size() << " lines)" << std::endl;
	}
	else
	{
		std::cout << " -> FAILED" << std::endl;
	}

	ss.str(""); ss.clear();
	ss << "Area " << index;

	// Set the readable name
	if (a.m_Description.size() > 0)
		a.m_Name = a.m_Description[0];
	else
		a.m_Name = ss.str();

	std::cout << "  Name: " << a.m_Name << std::endl;

	// Set the project name (filename for .MAP and .RSC files)
	ss.str(""); ss.clear();
	ss << "area" << index;
	a.m_ProjectName = ss.str();

	// Load the map thumbnail
	ss.str(""); ss.clear();
	ss << "huntdat/menu/pics/area" << index << ".tga";
	std::cout << "  Thumb: " << ss.str();
	LoadPicture(a.m_Thumbnail, ss.str());
	std::cout << " -> " << (a.m_Thumbnail.IsValid() ? "OK" : "FAILED") << std::endl;

	// Make sure the map exists. m_ProjectName stays "area6" (the slot's logical
	// identity, used by launch assembly and saved-hunt restore), but the vanilla
	// sixth slot stores its assets as external.map/.rsc -- so whichever basename
	// actually resolved is recorded in m_MapFile and used at launch. Launching a
	// name whose files do not exist halts the engine with "Error opening resource
	// file" (reproduced with prj=huntdat/areas/area6 on stock data), and launching
	// "external" without parser-side aliasing breaks the engine's script area
	// filtering (0xC0000005); ScriptParser.cpp now aliases external->area6, so the
	// resolved basename is always launchable.
	std::string mapName;

	// A complete pair settles the sixth slot up front: the engine opens both
	// <basename>.map and <basename>.rsc from that one name, so whichever
	// basename has both halves is the launchable one.
	std::string completePair;
	if (index == 6) {
		if (MissingAreaFile("external").empty())
			completePair = "external";
		else if (MissingAreaFile("area6").empty())
			completePair = "area6";
	}

	if (!completePair.empty()) {
		a.m_Valid = true;
		a.m_MapFile = completePair;
		mapName = "huntdat/areas/" + completePair + ".map";
		std::cout << "  Map:   " << mapName << " -> OK" << std::endl;
	}
	else if (index == 6) {
		mapName = "huntdat/areas/external.map";
		std::cout << "  Map:   " << mapName;
		f.open(mapName.c_str());
		a.m_Valid = f.is_open();
		std::cout << " -> " << (a.m_Valid ? "OK" : "FAILED") << std::endl;
		f.close();
		if (a.m_Valid) {
			a.m_MapFile = "external";
		} else {
			mapName = "huntdat/areas/area6.map";
			std::cout << "  Map:   " << mapName << " (fallback)";
			f.open(mapName.c_str());
			a.m_Valid = f.is_open();
			std::cout << " -> " << (a.m_Valid ? "OK" : "FAILED") << std::endl;
			f.close();
			if (a.m_Valid) a.m_MapFile = "area6";
		}
	} else {
		ss.str(""); ss.clear();
		ss << "huntdat/areas/area" << index << ".map";
		mapName = ss.str();
		std::cout << "  Map:   " << mapName;
		f.open(mapName.c_str());
		a.m_Valid = f.is_open();
		std::cout << " -> " << (a.m_Valid ? "OK" : "FAILED") << std::endl;
		f.close();
	}

	std::cout << "  Result: " << (a.m_Valid ? "VALID" : "INVALID (will be skipped)") << std::endl;
	if (a.m_Valid && !a.m_MapFile.empty()) {
		// The menu used to validate the .map alone, so a half-copied or
		// half-exported area looked playable and then halted the game. Name the
		// missing half here and let the launch paths refuse with that path in
		// the message.
		std::string missingHalf = MissingAreaFile(a.m_MapFile);
		if (!missingHalf.empty())
			std::cout << "  Missing: " << missingHalf
			          << " (incomplete area pair; the launch is refused)" << std::endl;
		std::cout << "  Launch: huntdat/areas/" << a.m_MapFile << std::endl;
	}

	return a;
}


/*
 * Skip a nested {} block in the _RES script.
 * Call this when encountering 'overwrite' or 'addition' keywords.
 * Handles nested braces correctly.
 */
static void SkipNestedBlock(FILE* stream, char* line, int maxLine)
{
    int depth = 0;

    // Count opening braces on the current line
    for (char* p = line; *p; p++) {
        if (*p == '{') depth++;
        if (*p == '}') depth--;
    }

    // Read lines until we find the matching closing brace
    while (depth > 0 && fgets(line, maxLine, stream)) {
        g_ScriptLine++;
        for (char* p = line; *p; p++) {
            if (*p == '{') depth++;
            if (*p == '}') depth--;
        }
    }
}


void ReadWeapons(FILE* stream)
{
	char line[256], * value;
	std::string sline = "";

	while (fgets(line, 255, stream))
	{
		g_ScriptLine++;
		if (strstr(line, "}")) break;
		if (strstr(line, "{"))
		{
			WeapInfo wi;

			std::stringstream spp;

			while (fgets(line, 255, stream))
			{
				g_ScriptLine++;
				sline = line;

				if (strstr(line, "}")) break;
				if (sline.empty()) continue;
				if (line[0] == ';') continue;

				// Skip overwrite/addition blocks (C2ME nested blocks)
				if (strstr(line, "overwrite") || strstr(line, "addition")) {
					if (strstr(line, "{")) {
						SkipNestedBlock(stream, line, 255);
					}
					continue;
				}

				value = strstr(line, "=");
				if (!value)
					throw script_error("Was expecting member assignment.", "ReadWeapons()", g_ScriptLine);
				value++;

				if (ScriptKeyIsIgnoreCase(line, "power"))  wi.m_Power = ReadScriptFloatValue(value, "ReadWeapons power");
				if (ScriptKeyIsIgnoreCase(line, "prec"))   wi.m_Prec = ReadScriptFloatValue(value, "ReadWeapons precision");
				if (ScriptKeyIsIgnoreCase(line, "loud"))   wi.m_Loud = ReadScriptFloatValue(value, "ReadWeapons loudness");
				if (ScriptKeyIsIgnoreCase(line, "rate"))   wi.m_Rate = ReadScriptFloatValue(value, "ReadWeapons rate");
				if (ScriptKeyIsIgnoreCase(line, "shots"))  wi.m_Shots = ReadScriptIntValue(value, "ReadWeapons shots");
				if (ScriptKeyIsIgnoreCase(line, "reload")) wi.m_Reload = ReadScriptIntValue(value, "ReadWeapons reload");
				if (ScriptKeyIsIgnoreCase(line, "trace"))  wi.m_TraceC = ReadScriptIntValue(value, "ReadWeapons trace") - 1;
				if (ScriptKeyIsIgnoreCase(line, "optic"))  wi.m_Optic = ReadScriptFloatValue(value, "ReadWeapons optic");
				if (ScriptKeyIsIgnoreCase(line, "fall"))   wi.m_Fall = ReadScriptIntValue(value, "ReadWeapons fall");
				if (ScriptKeyIsIgnoreCase(line, "price"))	wi.m_Price = ReadScriptIntValue(value, "ReadWeapons price");
				if (ScriptKeyIsIgnoreCase(line, "rank"))	wi.m_Rank = ReadScriptIntValue(value, "ReadWeapons rank");

				if (ScriptKeyIs(line, "name"))
					wi.m_Name = ReadAssignedText(value, "ReadWeapons()");

				if (ScriptKeyIs(line, "file"))
					wi.m_FilePath = ReadAssignedText(value, "ReadWeapons()");

				if (ScriptKeyIs(line, "pic1"))
					wi.m_BulletFilePath = ReadAssignedText(value, "ReadWeapons()");
			}

			spp << "huntdat/menu/pics/weapon" << (g_WeapInfo.size() + 1) << ".tga";
			LoadPicture(wi.m_Thumbnail, spp.str());

			g_WeapInfo.push_back(wi);
		}

	}

}


void ReadCharacters(FILE* stream)
{
	char line[256], * value;
	while (fgets(line, 255, stream))
	{
		g_ScriptLine++;
		if (strstr(line, "}")) break;
		if (strstr(line, "{"))
		{
			DinoInfo di;

			while (fgets(line, 255, stream))
			{
				g_ScriptLine++;

				if (strstr(line, "}"))
				{
					//AI_to_CIndex[DinoInfo[TotalC].AI] = TotalC;
					//TotalC++;
					break;
				}

				// Skip overwrite/addition blocks (C2ME nested blocks)
				if (strstr(line, "overwrite") || strstr(line, "addition")) {
					if (strstr(line, "{")) {
						SkipNestedBlock(stream, line, 255);
					}
					continue;
				}

				// Skip other C2ME sub-blocks that the menu doesn't need
				if (strstr(line, "spawninfo") || strstr(line, "spawngroup") ||
				    strstr(line, "killtype") || strstr(line, "tropinfo") ||
				    strstr(line, "deathtype") || strstr(line, "idlegroup") ||
				    strstr(line, "packinfo") || strstr(line, "packgroup") ||
				    strstr(line, "waterIgroup")) {
					if (strstr(line, "{")) {
						SkipNestedBlock(stream, line, 255);
					}
					continue;
				}

				value = strstr(line, "=");
				if (!value)
					throw script_error("Was expecting member assignment.", "ReadCharacters()", g_ScriptLine);
				value++;

				if (ScriptKeyIsIgnoreCase(line, "mass")) di.m_Mass = ReadScriptFloatValue(value, "ReadCharacters mass");
				if (ScriptKeyIsIgnoreCase(line, "length")) di.m_Length = ReadScriptFloatValue(value, "ReadCharacters length");
				if (ScriptKeyIsIgnoreCase(line, "radius")) di.m_Radius = ReadScriptFloatValue(value, "ReadCharacters radius");
				if (ScriptKeyIsIgnoreCase(line, "health")) di.m_BaseHealth = ReadScriptLegacyIntValue(value, "ReadCharacters health");
				if (ScriptKeyIsIgnoreCase(line, "basescore")) di.m_BaseScore = ReadScriptIntValue(value, "ReadCharacters base score");
				if (ScriptKeyIsIgnoreCase(line, "ai")) di.m_AI = ReadScriptIntValue(value, "ReadCharacters AI");
				if (MenuScriptKeyIsAny(line, "smell", "smellK")) di.m_SmellK = ReadScriptFloatValue(value, "ReadCharacters smell");
				if (MenuScriptKeyIsAny(line, "hear", "hearK")) di.m_HearK = ReadScriptFloatValue(value, "ReadCharacters hearing");
				if (MenuScriptKeyIsAny(line, "look", "lookK")) di.m_LookK = ReadScriptFloatValue(value, "ReadCharacters sight");
				if (ScriptKeyIsIgnoreCase(line, "shipdelta")) di.m_ShDelta = ReadScriptFloatValue(value, "ReadCharacters ship delta");
				if (ScriptKeyIsIgnoreCase(line, "scale0")) di.m_BaseScale = ReadScriptIntValue(value, "ReadCharacters scale0");
				if (ScriptKeyIsIgnoreCase(line, "scaleA")) di.m_ScaleA = ReadScriptIntValue(value, "ReadCharacters scaleA");
				if (ScriptKeyIsIgnoreCase(line, "danger")) di.m_DangerCall = true;

				if (ScriptKeyIs(line, "name"))
					di.m_Name = ReadAssignedText(value, "ReadCharacters()");

				if (ScriptKeyIs(line, "file"))
					di.m_FilePath = ReadAssignedText(value, "ReadCharacters()");

				if (ScriptKeyIs(line, "pic"))
					di.m_PicturePath = ReadAssignedText(value, "ReadCharacters()");
			}

			// Only add huntable dinosaurs (AI >= 10) to the menu list
			if (di.m_AI >= 10)
			{
				// Presentation assets follow the list position; an explicit
				// `pic` line overrides the default thumbnail. The resolved
				// path is kept on the entry so callers (and tests) can see
				// which picture the slot actually uses.
				const std::size_t menuSlot = HuntableMenuSlot(g_DinoInfo.size());
				if (di.m_PicturePath.empty())
				{
					std::stringstream spp;
					spp << "huntdat/menu/pics/dino" << menuSlot << ".tga";
					di.m_PicturePath = spp.str();
				}
				LoadPicture(di.m_Thumbnail, di.m_PicturePath);

				std::stringstream spp;
				spp << "huntdat/menu/pics/dino" << menuSlot << "no.tga";
				if (!LoadPicture(di.m_ThumbnailHidden, spp.str()))
				{
					di.m_ThumbnailHidden = di.m_Thumbnail;
				}

				g_DinoInfo.push_back(di);
			}
		}

	}
}


void ReadAreas(FILE* stream)
{
	char line[256], * value;
	while (fgets(line, 255, stream))
	{
		if (strstr(line, "}")) break;
		if (strstr(line, "{"))
		{
			AreaInfo area;

			while (fgets(line, 255, stream))
			{
				if (strstr(line, "}"))
				{
					break;
				}

				value = strstr(line, "=");
				if (!value)
					throw std::runtime_error("Script loading error");
				value++;

				if (ScriptKeyIsIgnoreCase(line, "price")) area.m_Price = ReadScriptIntValue(value, "ReadAreas price");
				if (ScriptKeyIsIgnoreCase(line, "rank"))  area.m_Rank = ReadScriptIntValue(value, "ReadAreas rank");

				if (ScriptKeyIs(line, "name"))
					area.m_Name = ReadAssignedText(value, "ReadAreas()");

				if (ScriptKeyIs(line, "pname"))
					area.m_ProjectName = ReadAssignedText(value, "ReadAreas()");

				if (ScriptKeyIs(line, "thumbnail"))
					(void)ReadAssignedText(value, "ReadAreas()"); // TODO: Load TPicture
			}

			g_AreaInfo.push_back(area);
		}

	}
}


/*
 * ReadAccessories()
 *
 * Parses the 'accessories { ... }' block in _MENU.TXT / _RES.TXT.
 * Format (one entry per line, key = value):
 *
 *     accessories
 *     {
 *         camo    = 0.85
 *         radar   = 0.70
 *         scent   = 0.80
 *         double  = 1.0
 *         tranq   = 1.25
 *         observe = 1.0
 *     }
 *     .
 *
 * Keys correspond to the command-line flag (without leading '-').
 * Populates the global g_AccessoryScoreMods map. Missing keys fall
 * back to hardcoded defaults applied in LoadResources().
 */
void ReadAccessories(FILE* stream)
{
	char line[256];
	uint32_t count = 0;

	while (fgets(line, 255, stream))
	{
		g_ScriptLine++;
		if (line[0] == '.') break; // end of block

		// Find the '=' separator
		const char* eq = strchr(line, '=');
		if (!eq) continue; // not a key=value line

		// Extract key
		std::string key(line, eq - line);
		// Trim whitespace from key
		while (!key.empty() && (key.back() == ' ' || key.back() == '\t' || key.back() == '\r' || key.back() == '\n'))
			key.pop_back();
		size_t kstart = key.find_first_not_of(" \t");
		if (kstart != std::string::npos) key = key.substr(kstart);
		if (key.empty() || key[0] == '#') continue; // empty or comment

		// Extract value
		std::string value(eq + 1);
		// Strip trailing whitespace
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
			value.pop_back();
		// Strip leading whitespace
		size_t vstart = value.find_first_not_of(" \t");
		if (vstart != std::string::npos) value = value.substr(vstart);
		// Strip inline comments
		size_t comment = value.find('#');
		if (comment != std::string::npos) value = value.substr(0, comment);
		// Trim again after comment strip
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
			value.pop_back();

		if (value.empty()) continue;

		float mod = ReadScriptFloatValue(value.c_str(), "ReadAccessories value");
		g_AccessoryScoreMods[key] = mod;
		count++;
		std::cout << "  accessory[" << key << "] = " << mod << std::endl;
	}

	std::cout << "Loaded " << count << " accessory score modifiers" << std::endl;
}


/*
 * LookupAccessoryScoreMod()
 *
 * Returns the modder-defined score multiplier for the given accessory
 * key, or the hardcoded default if the key was not present in the
 * accessories { ... } block.
 *
 * Keys: "camo", "radar", "scent", "double", "supply", "tranq", "observe"
 */
static float LookupAccessoryScoreMod(const std::string& key, float defaultValue)
{
	auto it = g_AccessoryScoreMods.find(key);
	if (it != g_AccessoryScoreMods.end()) {
		return it->second;
	}
	return defaultValue;
}


/*
 * Accessory script keys
 *
 * The 'accessory_icons { }' and 'accessory_descriptions { }' blocks accept
 * several spellings for each row. The older 'accessories { }' score block
 * still uses literal keys; its existing behavior is unchanged. For the path
 * blocks only, accepted spellings are:
 *
 *   - the score-block key ('camo', 'radar', 'scent', 'double', 'tranq',
 *     'nightvision', 'flashlight', 'observe') -- the canonical form,
 *   - the row's display name ('Cover scent', 'Tranquilizers'),
 *   - the full command string without its leading dash -- kept alive so a
 *     mod that already used the accidental compound 'tranq -tranquilizer'
 *     key from the first implementation keeps working.
 *
 * Keys are normalized (NormalizeAccessoryKey): case-insensitive, spaces,
 * tabs, hyphens and underscores ignored, so 'Night Vision', 'nightvision'
 * and 'night_vision' are the same key. The block readers store keys in
 * normalized form.
 */
static std::string NormalizeAccessoryKey(const std::string& key)
{
	std::string out;
	out.reserve(key.size());
	for (char c : key) {
		if (c == ' ' || c == '\t' || c == '-' || c == '_')
			continue;
		out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return out;
}

// Strip one pair of surrounding single quotes from a script string value --
// the same 'path' form the .c2map info block and the character reader
// accept, and the form the block examples are written in.
static void StripScriptQuotes(std::string& value)
{
	if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')
		value = value.substr(1, value.size() - 2);
}

/*
 * ReadAccessoryIcons() / ReadAccessoryDescriptions()
 *
 * Parse the optional 'accessory_icons { ... }' / 'accessory_descriptions
 * { ... }' blocks in _MENU.TXT. Same shape as 'accessories { ... }' but the
 * values are file paths, letting a mod supply its own art or blurb for any
 * accessory -- notably the ones the modernization added, which no stock
 * data ships art for:
 *
 *     accessory_icons
 *     {
 *         flashlight      = 'huntdat/menu/pics/my_flashlight.tga'
 *         'Cover scent'   = 'huntdat/menu/pics/my_scent.tga'
 *         nightvision     = 'huntdat/menu/pics/my_nv.tga'
 *     }
 *     .
 *
 * Put these blocks in _MENU.TXT only. The engine also reads _RES.TXT, does
 * not know these sections, and stops at the block's '.' closer -- a block
 * placed there silently truncates the engine's script load.
 *
 * Icons must be 16-bit TGA (the menu art format); LoadPicture() rejects
 * anything else and the row falls back to the empty icon box. Description
 * texts are plain text, one panel line per file line, under about 48
 * characters (the panel draws them unwrapped).
 */
// New shop blocks and top-level dispatch consume complete logical lines.
// Bound authored lines, but never reinterpret an overlong line's remainder.
static bool ReadMenuLine(FILE* stream, std::string& line)
{
	line.clear();
	bool overflow = false;
	int c;
	while ((c = fgetc(stream)) != EOF && c != '\n') {
		if (c == '\r') continue;
		if (line.size() < 4096) line.push_back(static_cast<char>(c));
		else overflow = true;
	}
	if (c == EOF && line.empty() && !overflow) return false;
	++g_ScriptLine;
	if (overflow) {
		LoadDiagnostics::Instance().Report("MenuResource", "script line",
			"line exceeds 4096 characters (whole line ignored)", nullptr);
		if (LoadDiagnostics::Instance().Strict())
			throw script_error("Menu script line exceeds 4096 characters.", "ReadMenuLine", g_ScriptLine);
		line.clear();
	}
	return true;
}

static std::string MenuLineWithoutComment(const std::string& line)
{
	bool quoted = false;
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] == '\'') quoted = !quoted;
		if (!quoted && (line[i] == '#' || line[i] == ';' ||
			(line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')))
			return line.substr(0, i);
	}
	return line;
}

static bool MenuSectionIs(const std::string& line, const char* key)
{
	const size_t begin = line.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos) return false;
	const size_t end = begin + strlen(key);
	return line.compare(begin, strlen(key), key) == 0 &&
		(end == line.size() || line[end] == '{' || std::isspace(static_cast<unsigned char>(line[end])));
}

static void ReadAccessoryPathBlock(FILE* stream, std::map<std::string, std::string>& target, const char* what)
{
	std::string line;
	uint32_t count = 0;

	while (ReadMenuLine(stream, line))
	{
		if (!line.empty() && line[0] == '.') break; // legacy column-zero closer
		line = MenuLineWithoutComment(line);
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;

		std::string key = line.substr(0, eq);
		while (!key.empty() && (key.back() == ' ' || key.back() == '\t' || key.back() == '\r' || key.back() == '\n'))
			key.pop_back();
		size_t kstart = key.find_first_not_of(" \t");
		if (kstart != std::string::npos) key = key.substr(kstart);
		if (key.empty() || key[0] == '#') continue;

		std::string value = line.substr(eq + 1);
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
			value.pop_back();
		size_t vstart = value.find_first_not_of(" \t");
		if (vstart != std::string::npos) value = value.substr(vstart);
		if (value.empty()) continue;

		// Keys may be quoted like the values ('Cover scent' = '...').
		StripScriptQuotes(key);
		StripScriptQuotes(value);
		key = NormalizeAccessoryKey(key);
		if (key.empty()) continue;

		target[key] = value;
		count++;
		std::cout << "  accessory_" << what << "[" << key << "] = " << value << std::endl;
	}

	std::cout << "Loaded " << count << " accessory " << what << " overrides" << std::endl;
}

void ReadAccessoryIcons(FILE* stream)
{
	ReadAccessoryPathBlock(stream, g_AccessoryIcons, "icon");
}

void ReadAccessoryDescriptions(FILE* stream)
{
	ReadAccessoryPathBlock(stream, g_AccessoryDescriptions, "description");
}

// Optional named prices in _MENU.TXT. Keep the legacy 'prices' block and its
// positional 'acces' lines untouched; these values only override a row's
// resolved positional/default price when that row is constructed.
void ReadAccessoryPrices(FILE* stream)
{
	std::string line;
	while (ReadMenuLine(stream, line)) {
		if (!line.empty() && line[0] == '.') break;
		line = MenuLineWithoutComment(line);
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string key = line.substr(0, eq);
		const size_t start = key.find_first_not_of(" \t");
		if (start == std::string::npos || key[start] == '#' || key[start] == ';') continue;
		key = key.substr(start);
		while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
		StripScriptQuotes(key);
		key = NormalizeAccessoryKey(key);
		if (key.empty()) continue;
		std::string value = line.substr(eq + 1);
		const ScriptIntResult parsed = ParseScriptIntStatus(value.c_str(), 0);
		char* end = nullptr;
		std::strtol(value.c_str(), &end, 10);
		while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
		if (parsed.status != ScriptScalarStatus::Ok || parsed.value < 0 || *end) {
			LoadDiagnostics::Instance().Report("accessory price", key.c_str(),
				"expected a nonnegative whole-number price (override ignored)", line.c_str());
			if (LoadDiagnostics::Instance().Strict())
				throw script_error("Invalid named accessory price.", "ReadAccessoryPrices", g_ScriptLine);
			continue;
		}
		g_AccessoryPriceOverrides[key] = parsed.value;
	}
}

/*
 * LookupAccessoryIcon() / LookupAccessoryText()
 *
 * Resolves one accessory row's override path. The row is addressed by its
 * score-block key (canonical), its display name or its legacy
 * command-derived key; the first match wins. Icon overrides replace the
 * built-in path outright and a missing file leaves the row with the empty
 * icon box, matching the original behavior -- the failure is reported so
 * the load summary says why. Text overrides must point at an existing
 * file: a missing one is reported and the row keeps its default path, so
 * the description can then fall back to its built-in text where provided.
 */
template <typename Value>
static const Value* FindAccessoryOverride(const std::map<std::string, Value>& overrides,
                                          const std::string& scoreKey, const char* name, const char* cmd)
{
	const std::string legacy = (cmd && cmd[0] == '-') ? std::string(cmd + 1) : std::string(cmd ? cmd : "");
	const std::string probes[3] = {
		NormalizeAccessoryKey(scoreKey),
		NormalizeAccessoryKey(name ? name : ""),
		NormalizeAccessoryKey(legacy),
	};
	for (const std::string& probe : probes) {
		if (probe.empty()) continue;
		auto it = overrides.find(probe);
		if (it != overrides.end()) {
			if constexpr (std::is_same_v<Value, std::string>) {
				if (it->second.empty()) continue;
			}
			return &it->second;
		}
	}
	return nullptr;
}

std::string LookupAccessoryIcon(const std::string& scoreKey, const char* name, const char* cmd,
                                const std::string& defaultPath)
{
	const std::string* overridePath = FindAccessoryOverride(g_AccessoryIcons, scoreKey, name, cmd);
	if (overridePath) {
		if (!std::filesystem::exists(*overridePath)) {
			LoadDiagnostics::Instance().Report("accessory icon", overridePath->c_str(),
				"override file not found (row shows the empty icon box)", nullptr);
		}
		return *overridePath;
	}
	return defaultPath;
}

std::string LookupAccessoryText(const std::string& scoreKey, const char* name, const char* cmd,
                               const std::string& defaultPath)
{
	const std::string* overridePath = FindAccessoryOverride(g_AccessoryDescriptions, scoreKey, name, cmd);
	if (overridePath) {
		if (std::filesystem::exists(*overridePath))
			return *overridePath;
		LoadDiagnostics::Instance().Report("accessory text", overridePath->c_str(),
			"override file not found (using the default description file)", nullptr);
	}
	return defaultPath;
}

static int32_t LookupAccessoryPrice(const std::string& scoreKey, const char* name,
                                    const char* cmd, int32_t fallback)
{
	const int32_t* price = FindAccessoryOverride(g_AccessoryPriceOverrides, scoreKey, name, cmd);
	return price ? *price : fallback;
}


void ReadPrices(FILE* stream)
{
	uint32_t CurA = 0;
	uint32_t CurW = 0;
	uint32_t CurD = 0;
	uint32_t CurU = 0;
	g_AccessoryPrices.clear();  // reset on each script load
	char line[256], * value;

	// Initialise the `CurD` variable to the first huntable index
	for (uint32_t i = 0; i < g_DinoInfo.size(); i++)
	{
		if (g_DinoInfo.at(i).m_AI == 10)
		{
			CurD = i;
			break;
		}
	}

	while (fgets(line, 255, stream))
	{
		g_ScriptLine++;
		std::string line_str = line;
		if (line_str.empty()) { continue; }
		if (line_str.compare("\n") == 0) { continue; }
		if (line_str.compare("\r\n") == 0) { continue; }
		if (strstr(line, "}")) { break; }

		value = strstr(line, "=");
		if (!value) { continue; }
		value++;

		// TODO: Add in error checking
		//throw script_error("Was expecting member assignment.", "ReadPrices()", g_ScriptLine);

		if (ScriptKeyIsIgnoreCase(line, "start")) {
                g_StartCredits = ReadScriptIntValue(value, "ReadPrices start credits");
		}
		else if (ScriptKeyIsIgnoreCase(line, "area")) {
			CurA++;  // Area indices start at 1
			g_AreaInfo.push_back(MakeOldAreaInfo(CurA, ReadScriptIntValue(value, "ReadPrices area price")));
			auto a = g_AreaInfo.end() - 1;
			if (!a->m_Valid)
				g_AreaInfo.pop_back();
		}
		else if (ScriptKeyIsIgnoreCase(line, "dino")) {
			// The prices block lists huntable dinos in roster order; more
			// entries than the loaded roster would write past the vector.
			if (CurD >= g_DinoInfo.size())
				throw script_error("More dinosaur prices than loaded dinosaurs.", "ReadPrices()", g_ScriptLine);
			g_DinoInfo[CurD].m_Price = ReadScriptIntValue(value, "ReadPrices dinosaur price");
			CurD++;
		}
		else if (ScriptKeyIsIgnoreCase(line, "weapon")) {
			if (CurW >= g_WeapInfo.size())
				throw script_error("More weapon prices than loaded weapons.", "ReadPrices()", g_ScriptLine);
			g_WeapInfo[CurW].m_Price = ReadScriptIntValue(value, "ReadPrices weapon price");
			CurW++;
		}
		else if (ScriptKeyIsIgnoreCase(line, "acces")) {
			g_AccessoryPrices.push_back(static_cast<int32_t>(ReadScriptIntValue(value, "ReadPrices accessory price")));
			CurU++;
		}
	}
}


/*
 * LoadC2Maps()
 *
 * Discover and parse .c2map custom-map descriptor files in
 * HUNTDAT/AREAS/. The format is the MEE format (Adelphospro 4.22.09):
 *
 *     info
 *     {
 *         name    = 'Delphaeus Hills'
 *         mapfile = 'huntdat\\areas\\area1'
 *         rscfile = 'huntdat\\areas\\area1.rsc'
 *         pic     = 'huntdat\\menu\\pics\\area1.tga'
 *         text    = 'huntdat\\menu\\txt\\area1.txt'
 *         price   = 20
 *     }
 *     .
 *
 * Unlike the MEE parser, we use a proper key/value tokeniser (the MEE
 * code does strstr(line, "txt") which only works because "text" is a
 * substring of "txt" -- a real footgun if anyone renames a key).
 *
 * Discovered maps are appended to g_AreaInfo unless the map's project
 * name already exists (the script-defined entry wins). Maps whose
 * .MAP file is missing are skipped with a warning.
 */
void LoadC2Maps()
{
	namespace fs = std::filesystem;
	const std::string areasDir = "huntdat/areas";

	std::error_code ec;
	if (!fs::is_directory(areasDir, ec)) {
		// No areas directory -- not a hard error, modder may not have any c2maps
		return;
	}

	int discovered = 0;
	int loaded = 0;
	int skipped = 0;
	int duplicates = 0;

	for (const auto& entry : fs::directory_iterator(areasDir, ec)) {
		if (ec) break;
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".c2map") continue;

		discovered++;
		const std::string c2mapPath = entry.path().string();

		std::ifstream f(c2mapPath);
		if (!f.is_open()) {
			std::cout << "LoadC2Maps: cannot open '" << c2mapPath << "'" << std::endl;
			skipped++;
			continue;
		}

		AreaInfo area;
		area.m_Valid = false;
		area.m_Rank = RANK_BEGINNER;
		bool inBlock = false;
		std::string mapfile;

		std::string line;
		while (std::getline(f, line)) {
			// Trim leading whitespace for prefix checks
			size_t start = line.find_first_not_of(" \t\r\n");
			if (start == std::string::npos) continue;
			if (line[start] == '#') continue; // comment

			if (line[start] == '.') break; // EOF marker

			if (!inBlock) {
				// The c2map format puts 'info' on one line and '{' on the next,
				// so we accept either: 'info {', 'info', or '{' alone (after
				// we've seen 'info' on the previous iteration).
				bool hasInfo = line.find("info", start) != std::string::npos;
				bool hasOpenBrace = line.find('{', start) != std::string::npos;
				if (hasInfo || hasOpenBrace) {
					inBlock = true;
				}
				continue;
			}

			// Inside the info { ... } block
			if (line.find('}', start) != std::string::npos) break;

			auto eq = line.find('=', start);
			if (eq == std::string::npos) continue;

			std::string key = line.substr(start, eq - start);
			// Trim trailing whitespace on the key
			while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
				key.pop_back();

			std::string value = line.substr(eq + 1);
			// Trim leading whitespace on the value
			size_t vstart = value.find_first_not_of(" \t");
			if (vstart != std::string::npos) value = value.substr(vstart);
			// Strip trailing whitespace + comments
			size_t comment = value.find('#');
			if (comment != std::string::npos) value = value.substr(0, comment);
			while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
				value.pop_back();

			// Strip surrounding single quotes from string values
			if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
				value = value.substr(1, value.size() - 2);
			}

			if (key == "price") {
				area.m_Price = ReadScriptIntValue(value.c_str(), "LoadC2Maps price");
			}
			else if (key == "rank") {
				area.m_Rank = ReadScriptIntValue(value.c_str(), "LoadC2Maps rank");
			}
			else if (key == "name") {
				area.m_Name = value;
			}
			else if (key == "mapfile") {
				mapfile = value;
				// Project name is the .MAP filename without extension.
				// Normalize backslashes to forward slashes first so
				// fs::path behaves the same on Windows and POSIX.
				std::string norm = value;
				for (auto& c : norm) if (c == '\\') c = '/';
				fs::path mp(norm);
				area.m_ProjectName = mp.stem().string();
			}
			else if (key == "rscfile") {
				// Stored in m_ProjectName + ".rsc" already, but keep raw for validation
				// (we don't currently validate .rsc, .MAP is the hard requirement)
			}
			else if (key == "pic") {
				if (!value.empty()) LoadPicture(area.m_Thumbnail, value);
			}
			else if (key == "text") {
				if (!value.empty()) LoadText(area.m_Description, value);
			}
		}
		f.close();

		// If name is empty, fall back to project name
		if (area.m_Name.empty() && !area.m_ProjectName.empty()) {
			area.m_Name = area.m_ProjectName;
		}

		// Validate the .MAP file actually exists. We try the candidates in
		// order of decreasing specificity:
		//   1. Whatever the modder wrote in 'mapfile' (normalized)
		//   2. <mapfile> with .map appended if no extension was given
		//   3. huntdat/areas/<stem>.map (fallback derived from project name)
		std::vector<std::string> mapCandidates;
		auto normalize = [](std::string s) {
			// MEE samples use double backslashes ('huntdat\\areas\\x') as a
			// Windows-INI escape; collapse them to single forward slashes
			// so fs::path / ifstream treat them uniformly on both platforms.
			for (auto& c : s) if (c == '\\') c = '/';
			return s;
		};
		auto endsWithMap = [](const std::string& s) {
			return s.size() >= 4 &&
			       s[s.size() - 4] == '.' && s[s.size() - 3] == 'm' &&
			       s[s.size() - 2] == 'a' && s[s.size() - 1] == 'p';
		};
		if (!mapfile.empty()) {
			std::string mf = normalize(mapfile);
			mapCandidates.push_back(mf);
			if (!endsWithMap(mf)) mapCandidates.push_back(mf + ".map");
		}
		if (!area.m_ProjectName.empty()) {
			mapCandidates.push_back(areasDir + "/" + area.m_ProjectName + ".map");
		}

		bool mapFound = false;
		std::string usedMap;
		for (const auto& candidate : mapCandidates) {
			std::ifstream mf(candidate, std::ios::binary);
			if (mf.is_open()) {
				mapFound = true;
				usedMap = candidate;
				mf.close();
				break;
			}
		}

		if (!mapFound) {
			std::cout << "LoadC2Maps: '" << c2mapPath
			          << "' skipped, no .MAP file found (tried "
			          << mapCandidates.size() << " candidates)" << std::endl;
			skipped++;
			continue;
		}

		// Check for duplicate project name (script-defined entry wins)
		bool duplicate = false;
		for (const auto& existing : g_AreaInfo) {
			if (existing.m_ProjectName == area.m_ProjectName) {
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			std::cout << "LoadC2Maps: '" << c2mapPath
			          << "' skipped, project name '" << area.m_ProjectName
			          << "' already in g_AreaInfo (script entry wins)"
			          << std::endl;
			duplicates++;
			continue;
		}

		// The launch prefixes the project stem with huntdat/areas (LaunchArgs.h),
		// so the pair that matters is <stem>.map/.rsc. A descriptor whose files
		// live anywhere else validates here but cannot launch; report the half
		// that is missing instead of letting the game halt on the path.
		std::string missingHalf = MissingAreaFile(area.m_ProjectName);

		area.m_Valid = true;
		g_AreaInfo.push_back(area);
		loaded++;
		std::cout << "LoadC2Maps: added '" << area.m_Name
		          << "' (map=" << usedMap
		          << ", price=" << area.m_Price
		          << ", rank=" << area.m_Rank << ")" << std::endl;
		if (!missingHalf.empty())
			std::cout << "LoadC2Maps: '" << area.m_Name
			          << "' launches as huntdat/areas/" << area.m_ProjectName
			          << " but " << missingHalf
			          << " is missing (the launch is refused)" << std::endl;
	}

	std::cout << "LoadC2Maps: discovered=" << discovered
	          << " loaded=" << loaded
	          << " skipped=" << skipped
	          << " duplicates=" << duplicates
	          << " total areas=" << g_AreaInfo.size() << std::endl;
}


void LoadResourcesScript()
{
	// _MENU.TXT/_RES.TXT are parsed below, before the menu's own LoadConfig()
	// runs, so the load policy is applied from config.cfg first; C2_STRICT_DATA
	// overrides it for CI/mod authoring. Default is lenient.
	ApplyLoadPolicyFromConfigFile(configpath::ResolveReadPath());

	FILE* file;
	std::string line;

	// Initialise some things
	g_StartCredits = 100; // Default
	g_ScriptLine = 0;
	g_AccessoryScoreMods.clear(); // reset on each script load
	g_AccessoryIcons.clear();      // same: a reload must not keep the previous file's icons
	g_AccessoryDescriptions.clear(); // same for the description overrides
	g_AccessoryPriceOverrides.clear(); // same for named prices
	g_AccessoryPrices.clear();

	// Try _MENU.TXT first (simplified menu data with prices)
	// Fall back to _res.txt if _MENU.TXT doesn't exist
	file = fopen("huntdat/_menu.txt", "r");
	if (!file) {
		file = fopen("huntdat/_res.txt", "r");
	}
	if (!file) {
		throw std::runtime_error("Can't open resources file _menu.txt or _res.txt");
		return;
	}

	std::unique_ptr<FILE, decltype(&fclose)> ownedFile(file, fclose);
	try
	{
		std::cout << "Loading resources script..." << std::endl;

		while (ReadMenuLine(file, line))
		{
			if (!line.empty() && line[0] == '.') break; // column-zero EOF
			line = MenuLineWithoutComment(line);
			const bool shopHeader = MenuSectionIs(line, "accessory_icons") ||
				MenuSectionIs(line, "accessory_descriptions") || MenuSectionIs(line, "accessory_prices");
			if (shopHeader && line.find('=') != std::string::npos) {
				LoadDiagnostics::Instance().Report("MenuResource", "shop section header",
					"put assignments on separate lines inside the block", line.c_str());
				if (LoadDiagnostics::Instance().Strict())
					throw script_error("Shop assignments must follow the section header.", "LoadResourcesScript", g_ScriptLine);
			}

			// Complete header tokens only; comments/paths cannot select readers.
			if (MenuSectionIs(line, "weapons")) {
				std::cout << "Found 'weapons' section at line " << g_ScriptLine << std::endl;
				ReadWeapons(file);
				std::cout << "Loaded " << g_WeapInfo.size() << " weapons" << std::endl;
			}
			else if (MenuSectionIs(line, "characters")) {
				std::cout << "Found 'characters' section at line " << g_ScriptLine << std::endl;
				ReadCharacters(file);
				std::cout << "Loaded " << g_DinoInfo.size() << " characters" << std::endl;
			}
			else if (MenuSectionIs(line, "accessory_prices")) {
				std::cout << "Found 'accessory_prices' section at line " << g_ScriptLine << std::endl;
				ReadAccessoryPrices(file);
			}
			else if (MenuSectionIs(line, "prices")) {
				std::cout << "Found 'prices' section at line " << g_ScriptLine << std::endl;
				ReadPrices(file);
				std::cout << "Loaded " << g_AreaInfo.size() << " areas" << std::endl;
			}
			else if (MenuSectionIs(line, "accessories")) {
				std::cout << "Found 'accessories' section at line " << g_ScriptLine << std::endl;
				ReadAccessories(file);
			}
			else if (MenuSectionIs(line, "accessory_icons")) {
				std::cout << "Found 'accessory_icons' section at line " << g_ScriptLine << std::endl;
				ReadAccessoryIcons(file);
			}
			else if (MenuSectionIs(line, "accessory_descriptions")) {
				std::cout << "Found 'accessory_descriptions' section at line " << g_ScriptLine << std::endl;
				ReadAccessoryDescriptions(file);
			}

			// C2ME sections we don't need - skip their blocks
			// Note: these checks require the keyword at the start of the line
			// to avoid matching substrings inside other sections
			else if (MenuSectionIs(line, "common") || MenuSectionIs(line, "areatable") ||
			         MenuSectionIs(line, "spawntable") || MenuSectionIs(line, "packtable") ||
			         MenuSectionIs(line, "trophytable") || MenuSectionIs(line, "oldambients") ||
			         MenuSectionIs(line, "corpseambients") || MenuSectionIs(line, "mapambients") ||
			         MenuSectionIs(line, "hunterinfo")) {
				if (line.find('{') != std::string::npos) {
					int depth = 0;
					do {
						for (char c : line) { if (c == '{') ++depth; else if (c == '}') --depth; }
					} while (depth > 0 && ReadMenuLine(file, line));
				}
			}
		}

		std::cout << "Finished loading resources script" << std::endl;
	}
	catch (script_error& e)
	{
		throw std::runtime_error(e.what());
	}

	ownedFile.reset();

	// Custom-map discovery (.c2map files in HUNTDAT/AREAS/).
	// Modder-friendly feature: ship a .c2map descriptor and the menu
	// will pick it up without editing _MENU.TXT / _RES.TXT.
	// We do this AFTER the main script load so script-defined entries
	// take precedence on duplicate project names.
	LoadC2Maps();

	// Report script/map diagnostics before loading menu art. Resource failures
	// occur later in LoadResources() and are reported there.
	LoadDiagnostics& diagnostics = LoadDiagnostics::Instance();
	if (diagnostics.Count() > 0) {
		std::cout << diagnostics.Summary();
		diagnostics.Clear();
	}
}


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

	// TODO: enable these and use them instead of GDI drawing for the trackbars
	//LoadPicture(g_TrackBar[0], "huntdat/menu/sl_bar.tga");
	//LoadPicture(g_TrackBar[1], "huntdat/menu/sl_but.tga");

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


bool ReadTGAFile(const std::string& path, TargaImage& tga)
{
	std::ifstream fs(path, std::ios::binary);

	if (!fs.is_open()) {
		std::cout << "Warning: Failed to open TGA file: " << path << std::endl;
		return false;
	}

	if (!fs.read(reinterpret_cast<char*>(&tga.m_Header), sizeof(TARGAINFOHEADER))) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"truncated TGA header", nullptr);
		return false;
	}

	// Menu pictures are 16-bit throughout, including the full-screen menu
	// images copied in Menu.cpp. Reject other depths before allocating data.
	switch (CheckTgaPictureFormat(tga.m_Header)) {
	case TgaPictureFormat::Ok:
		break;
	case TgaPictureFormat::Paletted:
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"TGA has a color palette (16-bit uncompressed truecolor required)", nullptr);
		return false;
	case TgaPictureFormat::NotTruecolor:
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"TGA is not uncompressed truecolor (16-bit uncompressed truecolor required)", nullptr);
		return false;
	case TgaPictureFormat::Not16Bit:
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"expected a 16-bit TGA (menu art is 16-bit 5551)", nullptr);
		return false;
	}
	const uint64_t size = static_cast<uint64_t>(tga.m_Header.tgaWidth) *
		tga.m_Header.tgaHeight * sizeof(uint16_t);
	if (!size || size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
		size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"invalid TGA dimensions", nullptr);
		return false;
	}
	fs.seekg(tga.m_Header.tgaIdentSize, std::ios::cur);
	const std::streampos start = fs.tellg();
	fs.seekg(0, std::ios::end);
	if (start == std::streampos(-1) || fs.tellg() - start < static_cast<std::streamoff>(size)) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"truncated TGA pixel data", nullptr);
		return false;
	}
	fs.seekg(start);
	std::unique_ptr<uint8_t[]> data(new uint8_t[static_cast<size_t>(size)]);
	if (!fs.read(reinterpret_cast<char*>(data.get()), static_cast<std::streamsize>(size))) {
		LoadDiagnostics::Instance().Report("menu picture", path.c_str(),
			"could not read TGA pixel data", nullptr);
		return false;
	}
	delete[] tga.m_Data;
	tga.m_Data = data.release();
	return true;
}


bool LoadMenuBackground(const std::string& path, TargaImage& tga)
{
	if (!ReadTGAFile(path, tga)) return false;
	if (tga.m_Header.tgaWidth != 800 || tga.m_Header.tgaHeight != 600) {
		LoadDiagnostics::Instance().Report("menu background", path.c_str(),
			"full-screen menu art must be 800x600", nullptr);
		return false;
	}
	return true;
}

bool LoadPicture(Picture& pic, const std::string& fpath)
{
	TargaImage tga;

	if (ReadTGAFile(fpath, tga))
	{
		// ReadTGAFile already validated 16-bit depth and the pixel payload.
		pic.m_Width = tga.m_Header.tgaWidth;
		pic.m_Height = tga.m_Header.tgaHeight;

		if (pic.m_Data)
			delete[] pic.m_Data;

		pic.m_Data = new uint16_t[pic.m_Width * pic.m_Height];
		// Menu pictures are 16-bit 5551 (alpha = bit 15). Some source TGAs
		// (e.g. equip_nv.tga) store pixels with bit 15 clear, which the menu
		// treats as fully transparent -> invisible icon. Force the alpha bit
		// so any loaded picture renders opaque like the stock equipment icons.
		const uint16_t* src = reinterpret_cast<const uint16_t*>(tga.m_Data);
		for (unsigned i = 0; i < pic.m_Width * pic.m_Height; ++i) {
			pic.m_Data[i] = static_cast<uint16_t>(src[i] | 0x8000u);
		}

		return true;
	}

	return false;
}


bool LoadText(std::vector<std::string>& txt, const std::string& path)
{
	std::ifstream tf(path);

	if (tf.is_open())
	{
		std::vector<std::string> loaded;
		std::string line;
		while (std::getline(tf, line)) loaded.push_back(line);
		if (tf.bad() || (tf.fail() && !tf.eof())) return false;
		txt.insert(txt.end(), loaded.begin(), loaded.end());
		return true;
	}
	else
	{
		std::cout << "LoadText() : Unable to open text file for reading: '" << path << "'" << std::endl;
	}

	return false;
}


// Load a file path with an optional built-in fallback. The row-specific
// redirect logic lives in LoadRowDescription() above.
void LoadAccessoryDescription(std::vector<std::string>& txt, const std::string& descFile,
                              const std::vector<std::string>* builtinDesc)
{
	if (!LoadText(txt, descFile) && builtinDesc) {
		txt = *builtinDesc;
	}
}


// Built-in flashlight blurb (see the Hunt.h declaration and the addUtil call
// that consumes it). A modernization accessory must not depend on shipped
// data: HUNTDAT comes from the user's own game installation, so most installs
// have no flashlight.nfo and would otherwise show a blank panel plus a
// LoadText() failure on stdout. Kept identical in wording to the file it
// stands in for, and every line under 48 characters because the shop panel
// draws them unwrapped. Defined here rather than in Hunt2.cpp (the usual home
// for EXTERNAL globals) so the menu resource tests can link it.
const std::vector<std::string> kFlashlightDescription = {
	"Accessory : Flashlight",
	"",
	"A torch that casts a warm beam ahead of",
	"you, letting you pick out terrain and game",
	"in the dark without lighting the whole map.",
	"The beam follows your aim and fades with",
	"distance. Press the configured key",
	"(default: F) to switch it on and off. Use",
	"of the flashlight does not affect your",
	"score."
};


bool LoadWave(SoundFX& sfx, const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	std::vector<short> samples;
	const char* error = nullptr;
	if (!file || !c2audio::ReadWave(file, samples, error)) {
		std::cout << "LoadWave(): " << (error ? error : "cannot open WAV file")
		          << ": '" << path << "'" << std::endl;
		return false;
	}
	const auto bytes = samples.size() * sizeof(short);
	std::unique_ptr<int16_t[]> data(new int16_t[samples.size()]);
	memcpy(data.get(), samples.data(), bytes);
	delete[] sfx.m_Data;
	sfx.m_Data = data.release();
	sfx.m_Length = static_cast<uint32_t>(bytes);
	sfx.m_Frequency = c2audio::kSampleRate;
	return true;
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


SoundFX::SoundFX() :
	m_Data(nullptr),
	m_Length(0U),
	m_Frequency(22050U)
{
}


SoundFX::SoundFX(const SoundFX& w) : SoundFX()
{
	if (w.m_Length && w.m_Data) {
		m_Data = new int16_t[(static_cast<size_t>(w.m_Length) + 1) / sizeof(int16_t)];
		memcpy(m_Data, w.m_Data, w.m_Length);
		m_Length = w.m_Length;
	}
	m_Frequency = w.m_Frequency;
}


SoundFX::~SoundFX()
{
	if (m_Data)
		delete[] m_Data;
}


Picture::Picture() :
	m_Width(0),
	m_Height(0),
	m_Data(nullptr)
{}


Picture::Picture(const Picture& p)
{
	m_Data = nullptr;

	if (p.m_Width && p.m_Height && p.m_Data) {
		m_Data = new uint16_t[p.m_Width * p.m_Height];
		memcpy(m_Data, p.m_Data, (p.m_Width * 2) * p.m_Height);
	}

	m_Width = p.m_Width;
	m_Height = p.m_Height;
}


Picture::~Picture()
{
	if (m_Data)
		delete[] m_Data;
}


Picture& Picture::operator= (const Picture& rhs)
{
	m_Data = nullptr;

	if (rhs.m_Width && rhs.m_Height && rhs.m_Data) {
		m_Data = new uint16_t[rhs.m_Width * rhs.m_Height];
		memcpy(m_Data, rhs.m_Data, (rhs.m_Width * 2u) * rhs.m_Height);
	}

	m_Width = rhs.m_Width;
	m_Height = rhs.m_Height;

	return *this;
}

bool Picture::IsValid() const
{
	return (m_Width > 0 && m_Height > 0 && m_Data);
}


// ================================================================
// config.cfg -- text-based settings file
// ================================================================
// Format: one setting per line, "key value".
// Lines starting with '#' are comments.  Unknown keys are ignored.
// This file is the single source of truth for settings that are
// not part of the legacy binary trophy format.
// ================================================================

// Write the current settings to config.cfg (EXE-directory based, shared
// with the game engine).
void SaveConfig()
{
	std::string configPath = configpath::ResolveWritePath();

	// Snapshot the existing bytes so everything this build does not own
	// (user comments, game-side envN_* audio keys, future keys) is carried
	// through byte-exact by the merge below. Without this, every save
	// silently deletes hand edits and the render exe's audio tweaks.
	std::string existing;
	{
		std::ifstream in(configPath, std::ios::binary);
		existing.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	// Fresh renderings of every key this build owns, in canonical order.
	// Conditional keys (resolution, hunt_*) are present only when valid --
	// a missing entry means "leave any existing line alone, append nothing".
	std::vector<std::pair<std::string, std::string>> fresh;
	fresh.emplace_back("renderer", std::to_string(g_Options.RenderAPI));
	fresh.emplace_back("fov", std::to_string(g_Options.FOV));
	fresh.emplace_back("object_detail", std::to_string(g_Options.ObjectDetail));
	fresh.emplace_back("fps_limit", std::to_string(g_Options.OptFpsLimit));
	fresh.emplace_back("verbose_logging", g_Options.VerboseLogging ? "1" : "0");
	fresh.emplace_back("nightvision_key", std::to_string(g_Options.NightVisionKey));
	fresh.emplace_back("flashlight_key", std::to_string(g_Options.FlashlightKey));

	// Persist the selected resolution as WxH so the render exe honours it
	// even though the legacy per-profile index (trophy0N.sav) is fragile
	// across differently-ordered mode lists.
	if (g_Options.Resolution >= 0 && g_Options.Resolution < g_ResCount) {
		std::string res = std::to_string(g_ResolutionList[g_Options.Resolution].w)
			+ "x" + std::to_string(g_ResolutionList[g_Options.Resolution].h);
		fresh.emplace_back("resolution", res);
	}

	// Persist the display mode (0=windowed, 1=exclusive fullscreen,
	// 2=borderless fullscreen). The render exe applies it on launch.
	int dm = g_Options.DisplayMode;
	if (dm < 0 || dm > 2) dm = 2;
	fresh.emplace_back("display_mode", std::to_string(dm));

	// Remember the last hunt setup (captured by Menu.cpp from the MenuHunt
	// lists). Guarded: the lists only exist after the first hunt-screen
	// visit, so a fresh boot or options-only session never clobbers the file
	// with empty defaults.
	if (g_HasSavedHunt) {
		fresh.emplace_back("hunt_area", g_SavedHuntArea);
		fresh.emplace_back("hunt_dinos", std::to_string(g_SavedHuntDinos));
		fresh.emplace_back("hunt_weapons", std::to_string(g_SavedHuntWeapons));
		fresh.emplace_back("hunt_utils", std::to_string(g_SavedHuntUtils));
		fresh.emplace_back("hunt_time", std::to_string(g_SavedHuntTime));
	}

	std::string output;
	if (existing.empty()) {
		// First run: canonical file, exactly as before (CRLF like the engine's
		// CreateDefaultConfig template; written in binary mode below).
		output = "# Carnivores 2 Modder's Engine configuration\r\n";
		output += "# Edit by hand if needed — values are validated on load.\r\n";
		output += "\r\n";
		for (const auto& kv : fresh)
			output += kv.first + " " + kv.second + "\r\n";
	} else {
		// Merge with the workbench's byte-preserving policy (Core/ConfigMerge.h):
		// only an owned key's value token is rewritten, comments and unowned
		// keys pass through verbatim, and absent keys append once. An earlier
		// duplicate of an owned key is left alone -- the readers let the last
		// occurrence win, so it is inert.
		const auto writable = [](const std::pair<std::string, std::string>& kv) {
			if (kv.first.empty() || kv.second.empty() || kv.first.front() == '#') return false;
			return kv.first.find_first_of(" \t\r\n") == std::string::npos &&
			       kv.second.find_first_of(" \t\r\n") == std::string::npos;
		};
		std::vector<configmerge::Entry> entries;
		for (const auto& kv : fresh) {
			// A value that cannot round-trip (empty, or a space-containing
			// hunt_area) is skipped with a notice; the rest of the save lands.
			if (!writable(kv)) {
				std::cout << "Config: skipping " << kv.first
				          << " (not a single-token key value)" << std::endl;
				continue;
			}
			entries.push_back({kv.first, kv.second});
		}
		const auto merged = configmerge::Merge(existing, entries);
		if (!merged.ok) {
			std::cout << "Config: merge failed (" << merged.error
			          << "); the file was left unchanged." << std::endl;
			return;
		}
		output = merged.text;
	}

	// Opened only after the new bytes are known, and binary so the merge's
	// terminators are written as-is (text mode would double the CRs); a
	// refusal never leaves a truncated file behind.
	std::ofstream fs(configPath, std::ios::trunc | std::ios::binary);
	if (!fs.is_open()) {
		std::cout << "Config: could not write " << configPath << std::endl;
		return;
	}
	fs << output;

	std::cout << "Config Saved (" << configPath << ")." << std::endl;
}

// Apply one scanned config.cfg field. Keys match case-insensitively, like the
// engine loader, so a hand-written "FOV 62" is not silently ignored here while
// the engine honours it; a missing value leaves the setting unchanged.
// Returns true if the key was recognised.
static bool ParseConfigField(const configscan::Field& field)
{
	std::istringstream iss(std::string(field.value));

	if (configscan::KeyEquals(field.key, "renderer")) {
		int v;
		if (iss >> v) {
			// 0=Software, 1=OpenGL. Legacy Direct3D values are normalized
			// to OpenGL so old configs do not launch v_d3d.ren.
			g_Options.RenderAPI = NormalizeMenuRenderAPI(v);
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "fov")) {
		int v;
		if (iss >> v) {
			if (v < kFovMin) v = kFovMin;
			if (v > kFovMax) v = kFovMax;
			g_Options.FOV = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "object_detail")) {
		int v;
		if (iss >> v) {
			v = ClampObjectDetail(v);
			g_Options.ObjectDetail = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "fps_limit")) {
		int v;
		if (iss >> v) {
			if (v < 0) v = 0;
			if (v >= kFpsLimitCount) v = kFpsLimitDefault;
			g_Options.OptFpsLimit = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "verbose_logging")) {
		int v;
		if (iss >> v) {
			g_Options.VerboseLogging = (v != 0);
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "nightvision_key")) {
		int v;
		if (iss >> v) {
			g_Options.NightVisionKey = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "flashlight_key")) {
		int v;
		if (iss >> v) {
			g_Options.FlashlightKey = ClampFlashlightKey(v);
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "resolution")) {
		int w = 0, h = 0;
		char x = 0;
		if (iss >> w >> x >> h && (x == 'x' || x == 'X') && w > 0 && h > 0) {
			// Translate WxH back to an index in the current mode list.
			for (int r = 0; r < g_ResCount; r++) {
				if (g_ResolutionList[r].w == w && g_ResolutionList[r].h == h) {
					g_Options.Resolution = r;
					break;
				}
			}
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "display_mode")) {
		int v;
		if (iss >> v) {
			if (v >= 0 && v <= 2)
				g_Options.DisplayMode = v;
		}
		return true;
	}

	// Last hunt setup (see SaveConfig). Everything is validated again
	// against the live lists on restore, so stale/modded values can only
	// fall back to defaults, never select out of range.
	if (configscan::KeyEquals(field.key, "hunt_area")) {
		std::string v;
		if (iss >> v && v.size() < 128) {
			g_SavedHuntArea = v;
			g_HasSavedHunt = true;
		}
		return true;
	}
	if (configscan::KeyEquals(field.key, "hunt_dinos") || configscan::KeyEquals(field.key, "hunt_weapons") || configscan::KeyEquals(field.key, "hunt_utils")) {
		unsigned long long v;
		if (iss >> v) {
			if (configscan::KeyEquals(field.key, "hunt_dinos")) g_SavedHuntDinos = v;
			else if (configscan::KeyEquals(field.key, "hunt_weapons")) g_SavedHuntWeapons = v;
			else g_SavedHuntUtils = v;
			g_HasSavedHunt = true;
		}
		return true;
	}
	if (configscan::KeyEquals(field.key, "hunt_time")) {
		int v;
		if (iss >> v && v >= HUNT_DAWN && v <= HUNT_NIGHT) {
			g_SavedHuntTime = v;
			g_HasSavedHunt = true;
		}
		return true;
	}

	// Unknown key -- ignore gracefully (forward-compat with newer configs)
	return false;
}

// Read config.cfg and override g_Options fields.
// If the file does not exist, write defaults (first-run migration).
void LoadConfig()
{
	std::string configPath = configpath::ResolveReadPath();
	std::ifstream fs(configPath);
	if (!fs.is_open()) {
		std::cout << "Config: " << configPath << " not found — writing defaults." << std::endl;
		SaveConfig();
		return;
	}

	std::string whole;
	size_t nulBytes = 0;
	if (!ReadConfigText(fs, whole, nulBytes)) {
		std::cout << "Config: unreadable or unsupported encoding; using defaults." << std::endl;
		return;
	}
	if (nulBytes) {
		std::cout << "Config: recovered " << nulBytes << " NUL padding bytes; please resave config.cfg." << std::endl;
	}
	for (const configscan::Field& field : configscan::Scan(whole)) {
		ParseConfigField(field);
	}

	std::cout << "Config Loaded (" << configPath << ")." << std::endl;
}
