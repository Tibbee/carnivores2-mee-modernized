// ScriptParse.cpp -- menu _MENU.TXT/_RES.TXT parsing (split from Resources.cpp)
// ==========================================================================
// Strict/lenient script readers for weapons, characters, areas, accessories,
// prices and .c2map discovery. The engine's parser is
// Hunt/Loaders/ScriptParser.cpp; this TU owns only the standalone menu's
// grammar and diagnostics.
// ==========================================================================

#include "Hunt.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "Core/ConfigPath.h"
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
float LookupAccessoryScoreMod(const std::string& key, float defaultValue)
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

int32_t LookupAccessoryPrice(const std::string& scoreKey, const char* name,
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
