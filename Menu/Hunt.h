
// -- Define UNICODE for Wide Characters WCHAR (extra symbols, Japanese Text, etc)
//#define UNICODE

// -- Define no secure warnings for sanity when compiling
#ifndef _CRT_SECURE_NO_WARNINGS
//#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Loaders/TgaFormat.h"

#include <Windows.h>
//std
#include <array>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <map>



#include "Version.h"
#include "Core/Constants.h"
#include "Core/ResolutionList.h"

enum HuntTimeEnum {
	HUNT_DAWN = 0,
	HUNT_DAY = 1,
	HUNT_NIGHT = 2
};

#ifdef _MAIN_
#	define EXTERNAL
#else
#	define EXTERNAL extern
#endif


// ======================================================================= //
// Global Enumerators
// ======================================================================= //
enum RankEnum
{
	RANK_NOVICE = 0,
	RANK_BEGINNER = 0,
	RANK_INTERMEDIATE = 1,
	RANK_ADVANCED = 1,
	RANK_MASTER = 2,
	RANK_EXPERT = 2,
	RANK_MAX
};

// ResolutionsEnum is kept as a documentation/legacy reference only.
// The actual resolution list is built at runtime by EnumerateResolutions()
// and stored in g_ResolutionList[]. The menu and the render exe agree on
// the same dynamic-list semantics: g_Options.Resolution is an index into
// g_ResolutionList[], not a constant from this enum.
//
// Historical values (kept here for reference when reading older profiles):
//   0=320x240, 1=400x300, 2=512x384, 3=640x480, 4=800x600, 5=1024x768,
//   6=1280x1024, 7=1600x1200.
enum ResolutionsEnum
{
	RES_320x240 = 0,
	RES_400x300 = 1,
	RES_512x384 = 2,
	RES_640x480 = 3,
	RES_800x600 = 4,
	RES_1024x768 = 5,
	RES_1280x1024 = 6,
	RES_1600x1200 = 7,
	RES_MAX = resolutions::kMaxResolutions
};

extern TRes g_ResolutionList[resolutions::kMaxResolutions];
extern int  g_ResCount;

void EnumerateResolutions();
/*
if (!HARD3D && OptRes > 5) { OptRes = 0; }
if (OptRes==0) { WinW = 320; WinH=240; }
if (OptRes==1) { WinW = 400; WinH=300; }
if (OptRes==2) { WinW = 512; WinH=384; }
if (OptRes==3) { WinW = 640; WinH=480; }
if (OptRes==4) { WinW = 800; WinH=600; }
if (OptRes==5) { WinW =1024; WinH=768; }
if (OptRes==6) { WinW =1280; WinH=1024; }
if (OptRes==7) { WinW =1600; WinH=1200; }
*/

// ======================================================================= //
// Global Types & Classes
// ======================================================================= //

#pragma pack(push, 1)
struct Color16 {
	int r : 5;
	int g : 5;
	int b : 5;
	int a : 1;

	Color16(Color16 c, int a) :
		r(c.r),
		g(c.g),
		b(c.b),
		a(a)
	{}

	Color16(uint16_t c, int a) :
		r(c & 5),
		g((c >> 5) & 5),
		b((c >> 10) & 5),
		a(a)
	{}
};
#pragma pack(pop)
using RGBA16 = Color16;


class TargaImage
{
public:
	TARGAINFOHEADER m_Header;
	uint8_t* m_Data;

	TargaImage() : m_Header(), m_Data(nullptr) {}
	~TargaImage() {
		if (m_Data) delete[] m_Data;
	}
};


class Timer
{
private:
	static std::chrono::steady_clock::duration m_TimeStart;

public:

	static void Init()
	{
		Timer::m_TimeStart = std::chrono::steady_clock::now().time_since_epoch();
	}

	/* Replacement for winmm.lib : timeGetTime() */
	static int64_t GetTime()
	{
		auto t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch() - Timer::m_TimeStart);
		return t.count();
	}

};


class SoundFX
{
public:
	int16_t* m_Data;
	uint32_t m_Length;
	uint32_t m_Frequency;

	SoundFX();
	SoundFX(const SoundFX& w);
	~SoundFX();
};


class MenuItem
{
private:
	std::array<bool, 256> m_ElementSet;

public:
	uint16_t m_Image[800 * 600];
	uint16_t m_Image_On[800 * 600];
	uint8_t m_Image_Map[400 * 300];

	uint8_t GetID(int x, int y) const {
		if (x >= 400 || y >= 300)
			throw std::out_of_range("GetID() coordinates are beyond the range of 400x300!");
		return m_Image_Map[x + (y * 400)];
	}

	void ResetElementSet() {
		for (unsigned i = 0; i < m_ElementSet.size(); ++i) {
			m_ElementSet[i] = false;
		}
	}

	bool ToggleIsElementSet(unsigned id) {
		if (!(id < m_ElementSet.size())) {
			throw std::out_of_range("ToggleIsElementSet() `id` is out of range!");
		}
		m_ElementSet[id] = !m_ElementSet[id];
		return (m_ElementSet[id]);
	}

	bool GetIsElementSet(unsigned id) const {
		if (!(id < m_ElementSet.size())) {
			throw std::out_of_range("GetIsElementSet() `id` is out of range!");
		}
		return m_ElementSet[id];
	}

	void SetIsElementSet(unsigned id, bool b) {
		if (!(id < m_ElementSet.size())) {
			throw std::out_of_range("SetIsElementSet() `id` is out of range!");
		}
		m_ElementSet[id] = b;
	}
};


class Picture
{
public:
	unsigned m_Width;
	unsigned m_Height;
	uint16_t* m_Data;

	Picture();
	Picture(const Picture& p);
	~Picture();

	Picture& operator= (const Picture& rhs);

	bool IsValid() const;

	// TODO: Add Getters and Setters, move member variables to private
};


class DinoInfo
{
public:
	std::string	m_Name; // Creature name
	std::string	m_FilePath; // Character file path
	std::string	m_PicturePath; // Picture file path

	std::vector<std::string> m_Description;

	int32_t m_BaseHealth; // Base health
	int32_t	m_AI; // AI index

	bool m_DangerCall; // Dangerous lure call

	float m_Mass; // Weight of creature
	float m_Length; // Length of creature
	float m_Radius; // Area the creature occupies (for pathing and hit detection)
	float m_SmellK; // Scent Sensitivity
	float m_HearK; // Audio Sensitivity
	float m_LookK; // Visual Sensitivity
	float m_ShDelta; // Height of ship hook from ground
	int32_t m_BaseScale; // Base Scale
	int32_t m_ScaleA; // Scale Variation
	int32_t m_BaseScore; // Base score of creature
	int32_t m_Price; // Credits cost
	int32_t m_Rank; // Rank required

	Picture m_CallIcon;	//<- Icon associated with this species
	Picture m_Thumbnail; //<- Menu thumbnail associated with this species
	Picture m_ThumbnailHidden; //<- 'Hidden' Menu thumbnail associated with this species

	DinoInfo() :
		m_Name(""),
		m_FilePath(""),
		m_PicturePath(""),
		m_Description(),
		m_BaseHealth(0),
		m_AI(0),
		m_DangerCall(false),
		m_Mass(0.0f),
		m_Length(0.0f),
		m_Radius(0.0f),
		m_SmellK(0.0f),
		m_HearK(0.0f),
		m_LookK(0.0f),
		m_ShDelta(0.0f),
		m_BaseScale(1000),
		m_ScaleA(800),
		m_BaseScore(0),
		m_Price(0),
		m_Rank(RANK_NOVICE),
		m_CallIcon(),
		m_Thumbnail(),
		m_ThumbnailHidden()
	{}
};


class WeapInfo
{
public:
	std::string m_Name; // Weapon Name
	std::string m_FilePath; // Weapon Character File Name
	std::string m_BulletFilePath; // Bullet Icon File Name

	std::vector<std::string> m_Description;

	float m_Power; // Damage Per Shot of Weapon [0.0~]
	float m_Prec; // Precision of Weapon [0.0 - 1.1]
	float m_Loud; // Volume of Weapon [0.0 - 1.1]
	float m_Rate; // Rate Of Fire, Shots Per Second
	float m_Recoil; // Force of recoil [0-9999]
	float m_Optic; // Zoom of weapon when iron-sighted [1-10]

	int32_t m_Shots; // Number of Bullets Per Clip/Mag/Tube
	int32_t m_Fall; // Gravity on projectile during ray tracing (used for X-Bow)
	int32_t m_TraceC; // Amount of projectiles per shot
	int32_t m_Reload; // Shots before reload
	int32_t	m_Price; // Credits price
	int32_t m_Rank; // Rank required

	Picture m_Thumbnail; // Preview icon/image associated with this area
	Picture m_ThumbnailHidden; // 'Hidden' Preview icon/image associated with this area

	WeapInfo() :
		m_Name(""),
		m_FilePath(""),
		m_BulletFilePath(""),
		m_Description(),
		m_Power(0.0f),
		m_Prec(0.0f),
		m_Loud(0.0f),
		m_Rate(0.0f),
		m_Recoil(0.0f),
		m_Optic(0.0f),
		m_Shots(0),
		m_Fall(0),
		m_TraceC(0),
		m_Reload(0),
		m_Price(0),
		m_Rank(RANK_NOVICE),
		m_Thumbnail(),
		m_ThumbnailHidden()
	{}
};


class AreaInfo
{
public:
	/*
	Added this member after P.Rex's Mandibles mod was crashing the menu.
	P.Rex's _RES had 8 areas listed in the prices{} block, but only had
	6 areas to actually play in.
	*/
	bool m_Valid; // Is this a valid area?

	std::string	m_Name; // Area Name
	std::string	m_ProjectName; // MAP & RSC filename
	std::string	m_MapFile; // Basename that actually resolved during validation ("external" or "area6"); empty unless validation found the map

	std::vector<std::string> m_Description;

	int32_t m_Price; // Credits cost
	int32_t m_Rank; // Rank required/recommended to play map

	std::vector<int> m_DinosAvail; // A list of animal AI indexes that are available on this map.

	Picture m_Thumbnail; // Preview icon/image associated with this area
	Picture m_ThumbnailHidden; // 'Hidden' Preview icon/image associated with this area

	AreaInfo() :
		m_Valid(false),
		m_Name(""),
		m_ProjectName(""),
		m_MapFile(""),
		m_Description(),
		m_Price(0),
		m_Rank(RANK_BEGINNER),
		m_DinosAvail(),
		m_Thumbnail()
	{
	}

	AreaInfo(const AreaInfo& a) :
		m_Valid(a.m_Valid),
		m_Name(a.m_Name),
		m_ProjectName(a.m_ProjectName),
		m_MapFile(a.m_MapFile),
		m_Description(a.m_Description),
		m_Price(a.m_Price),
		m_Rank(a.m_Rank),
		m_DinosAvail(a.m_DinosAvail),
		m_Thumbnail(a.m_Thumbnail)
	{
	}
};


class UtilInfo
{
protected:
public:

	std::string m_Name;
	std::vector<std::string> m_Description;
	std::string m_Command;
	Picture m_Thumbnail; // Preview icon/image associated with this area
	int32_t m_Price; // Credits cost for this accessory (0 = free)
	float m_ScoreMod; // Score multiplier when this accessory is selected (1.0 = neutral, 0.85 = -15%)
	// NOTE: all accessories are always listed (night vision uses a built-in
	// description/icon fallback on mods that predate it), so there is no
	// availability filter. MenuHunt[3].Item and g_UtilInfo stay parallel;
	// indices below use kAcc* constants in Menu.cpp.

//public:

	UtilInfo() :
		m_Name(""),
		m_Description(),
		m_Command(""),
		m_Thumbnail(),
		m_Price(0),
		m_ScoreMod(1.0f)
	{
	}

	UtilInfo(const std::string& name, std::vector<std::string> description, const std::string& command, const std::string& thumbnail) :
		m_Name(name),
		m_Description(description),
		m_Command(command),
		m_Thumbnail(),
		m_Price(0),
		m_ScoreMod(1.0f)
	{
		//if (!thumbnail.empty())
			//LoadPicture(this->m_Thumbnail, thumbnail);
	}

	UtilInfo(const UtilInfo& ui) :
		m_Name(ui.m_Name),
		m_Description(ui.m_Description),
		m_Command(ui.m_Command),
		m_Thumbnail(ui.m_Thumbnail),
		m_Price(ui.m_Price),
		m_ScoreMod(ui.m_ScoreMod)
	{
	}
};


class TKeyMap
{
public:
	int32_t	fkForward,
		fkBackward,
		fkReload,
		fkResupply,
		fkHoldBreath,
		fkFiringMode,
		fkFire,
		fkShow,
		fkSLeft,
		fkSRight,
		fkStrafe, // Pump / Rack the shotgun (MEE engine also uses this for strafe movement)
		fkJump,
		fkRun,
		fkCrouch,
		fkCall,
		fkCCall,
		fkBinoc;
#ifdef _iceage
	int32_t	fkSupply;
#endif
};

enum MenuRenderAPI : int32_t
{
	kRenderAPI_Software = 0,
	kRenderAPI_OpenGL = 1,
	kRenderAPI_Count = 2
};

inline int32_t NormalizeMenuRenderAPI(int32_t api)
{
	return api == kRenderAPI_Software ? kRenderAPI_Software : kRenderAPI_OpenGL;
}

class Options
{
public:
	/* Game */

	bool ScentMode;
	bool CamoMode;
	bool RadarMode;
	bool TranqMode;
	int32_t	Aggression;
	int32_t Density;
	int32_t Sensitivity;
	int32_t OptSys; // Metric or Imperial

	/* Graphics */

	int32_t	Resolution; // ResolutionsEnum
	int32_t DisplayMode; // 0=windowed, 1=exclusive fullscreen, 2=borderless fullscreen
	int32_t Textures;
	int32_t ViewRange; // OptViewR, 0..255; mapped to ctViewR by the engine
	int32_t ObjectDetail; // ctViewRM, bitmap sprite LOD distance in map cells
	int32_t Brightness;
	int32_t FOV; // Vertical FOV in degrees, [kFovMin..kFovMax]
	int32_t AlphaColorKey;
	int32_t RenderAPI; // The Render API/Version to use
	bool Fog; // Render volumetric fog
	bool Shadows; // Render real-time shadows

	int32_t	SoundAPI; // Audio system
	int32_t OptFpsLimit; // FPS limit, 0=Unlimited, 1=60, 2=120, 3=240
	bool VerboseLogging; // Enable verbose (per-frame) debug logging

	TKeyMap KeyMap; // Controls mapping
	bool	MouseInvert;
	int32_t	MouseSensitivity;
	int32_t	NightVisionKey; // Key to toggle night vision (default: 'N' = 0x4E)
	// Deliberately NOT a TKeyMap field: that struct is blobsaved with
	// sizeof(TKeyMap) into trophy0N.sav, so growing it would shift every
	// field after it for existing profiles. Bindings that are not part of the
	// movement block live out here, the way NightVisionKey does.
	int32_t	FlashlightKey; // Key to toggle the flashlight (default: 'F' = 0x46)

	void Default();
};


class TrophyItem
{
public:
	int32_t m_CType;
	int32_t m_Weapon;
	int32_t m_Phase;
	int32_t m_Height;
	int32_t m_Weight;
	int32_t m_Score;
	int32_t m_Date;
	int32_t m_Time;
	float m_Scale;
	float m_Range;
	int32_t m_Reserved[4];
};


class ProfileStats
{
public:
	int32_t smade;
	int32_t success;
	float path;
	float time;
};

class Profile
{
public:

	char Name[128]; // Leaving this as a char array for convenience
	int32_t RegNumber;
	int32_t Score;
	int32_t Rank;
	ProfileStats Last;
	ProfileStats Total;
	TrophyItem Body[24];

	void New(const std::string& name, int32_t index = -1);

};

class ProfileShort
{
public:

	std::string	m_Name;
	int32_t	m_RegNumber;
	int32_t	m_Score;
	int32_t	m_Rank;
};

// ======================================================================= //
// Global Enums
// ======================================================================= //
enum MenuStateEnum {
	//MENU_SPLASH, // Rework the menu to support this
	MENU_REGISTER,
	MENU_REGISTRY_DELETE,
	MENU_REGISTRY_WAIVER,
	MENU_MAIN,
	MENU_STATISTICS,
	MENU_HUNT,
	MENU_TROPHY,
	MENU_OPTIONS,
	MENU_CREDITS,
	MENU_QUIT,
};

// ======================================================================= //
// Global Variables
// ======================================================================= //
EXTERNAL HINSTANCE				hInst;
EXTERNAL HWND					hwndMain;
EXTERNAL HDC					hdcMain;
EXTERNAL HFONT					fnt_Small;
EXTERNAL HFONT					fnt_Midd;
EXTERNAL HFONT					g_FontOptions;
EXTERNAL HFONT					fnt_Big;

// Menu scaling: 1 = 800x600, 2 = 1600x1200, etc.
// Fullscreen mode stretches the 800x600 buffer to the primary monitor.
#define MENU_BASE_WIDTH  800
#define MENU_BASE_HEIGHT 600
EXTERNAL int					g_MenuScale;
EXTERNAL int					g_ClientWidth;
EXTERNAL int					g_ClientHeight;
EXTERNAL float				g_ScaleX;
EXTERNAL float				g_ScaleY;

EXTERNAL MenuItem				g_MenuItem;
EXTERNAL std::int32_t			g_PrevMenuState;
EXTERNAL std::int32_t			g_MenuState;
EXTERNAL std::string			g_TypingBuffer;

EXTERNAL uint32_t				g_GameState;

EXTERNAL std::vector<DinoInfo>	g_DinoInfo;
EXTERNAL std::vector<WeapInfo>	g_WeapInfo;
EXTERNAL std::vector<AreaInfo>	g_AreaInfo;
EXTERNAL std::vector<UtilInfo>	g_UtilInfo;
EXTERNAL UtilInfo				g_TranqInfo, g_ObserverInfo;
EXTERNAL std::map<std::string, float> g_AccessoryScoreMods; // populated by ReadAccessories()
EXTERNAL std::map<std::string, std::string> g_AccessoryIcons; // populated by ReadAccessoryIcons(): accessory key -> shop icon path
EXTERNAL std::map<std::string, std::string> g_AccessoryDescriptions; // populated by ReadAccessoryDescriptions(): accessory key -> description text file path
EXTERNAL std::map<std::string, int32_t> g_AccessoryPriceOverrides; // accessory_prices block: named prices override positional values
EXTERNAL std::vector<int32_t> g_AccessoryPrices; // parsed from 'acces' lines in _MENU.TXT prices block
// Built-in shop blurb for the flashlight, used only when the loaded HUNTDAT
// ships no huntdat/menu/txt/flashlight.nfo. Defined in Menu/AssetLoad.cpp (not
// Hunt2.cpp) so the menu resource test target can read it without linking the
// Win32 menu against it. See tests/test_menu_resource_entry.cpp for the width
// contract its lines have to satisfy.
EXTERNAL const std::vector<std::string> kFlashlightDescription;
// Loads an accessory description: tries the mod-supplied .nfo first and only
// falls back to builtinDesc when that file is absent.
void LoadAccessoryDescription(std::vector<std::string>& txt, const std::string& descFile,
                              const std::vector<std::string>* builtinDesc);
// Resolve one accessory row's art / blurb file from the 'accessory_icons' /
// 'accessory_descriptions' blocks. scoreKey is the canonical key ('scent',
// 'tranq'), and the row's display name and legacy command-derived key are
// accepted too (see the accessory key comment in Menu/ScriptParse.cpp).
std::string LookupAccessoryIcon(const std::string& scoreKey, const char* name, const char* cmd,
                                const std::string& defaultPath);
std::string LookupAccessoryText(const std::string& scoreKey, const char* name, const char* cmd,
                                const std::string& defaultPath);
// Score multiplier and price for one accessory row (mod overrides win).
float LookupAccessoryScoreMod(const std::string& key, float defaultValue);
int32_t LookupAccessoryPrice(const std::string& scoreKey, const char* name, const char* cmd,
                             int32_t fallback);
EXTERNAL std::vector<unsigned int> g_DinoList;
EXTERNAL uint32_t				g_ProfileIndex;
EXTERNAL uint32_t				g_HiliteProfileIndex;
EXTERNAL uint32_t				g_StartCredits;
EXTERNAL Profile				g_UserProfile;
EXTERNAL ProfileShort			g_Profiles[10];
EXTERNAL Options				g_Options;
EXTERNAL Picture*				g_HuntSelectPic;
EXTERNAL int32_t				g_TimeOfDay;
EXTERNAL bool					g_ObserverMode;
EXTERNAL int64_t				g_ScoreDebit; // current setup cost, never stored in the profile
// Last hunt setup, persisted in config.cfg so the hunt screen reselects it
// on entry instead of resetting to defaults. Menu.cpp owns capture/restore
// (MenuHunt lists are file-local there); Resources.cpp owns the file format.
EXTERNAL std::string			g_SavedHuntArea;
EXTERNAL unsigned long long		g_SavedHuntDinos;
EXTERNAL unsigned long long		g_SavedHuntWeapons;
EXTERNAL unsigned long long		g_SavedHuntUtils;
EXTERNAL int32_t				g_SavedHuntTime;
EXTERNAL bool					g_HasSavedHunt;

EXTERNAL uint8_t                g_KeyboardState[256];

EXTERNAL SoundFX				g_MenuSound_Go;
EXTERNAL SoundFX				g_MenuSound_Ambient;
EXTERNAL SoundFX				g_MenuSound_Move;
EXTERNAL SoundFX				g_MenuSound_Type;
EXTERNAL SoundFX				g_MenuSound_TypeGo;


// ======================================================================= //
// Global Functions
// ======================================================================= //

int LaunchProcess(const std::string& exe_name, std::string cmd_line);
void ShowErrorMessage(const std::string&);
// Path of the missing half of an area's <basename>.map/.rsc pair, or an empty
// string when both exist. The engine derives both filenames from the single
// "prj=" token it is launched with (LaunchArgs.h), so this is the same test the
// launch itself will run -- not a stricter one.
std::string MissingAreaFile(const std::string& base);
void PrintLogSeparater();
void HuntWindowResize();

/*** Menu ***/
void InitInterface();
void ShutdownInterface();
void InterfaceClear(uint16_t);
void InterfaceBlt();
void InterfaceSetFont(HFONT);
void DrawTextShadow(int x, int y, const std::string& text, uint32_t color, int align);
void LoadGameMenu(int32_t);
void ChangeMenuState(int32_t);
void MenuKeyCharEvent(uint16_t);
void MenuMouseScrollEvent(int32_t, int32_t);
void ProcessMenu();

/*** Resources ***/
void LoadResources();
void LoadResourcesScript();
void ReleaseResources();
void TrophyLoad(Profile& profile, int pr);
void TrophySave(Profile& profile);
void LoadConfig();
void SaveConfig();
void RequestMenuExit(int exitCode = 0);
void TrophyDelete(uint32_t);
bool ReadTGAFile(const std::string& path, TargaImage& tga);
bool LoadMenuBackground(const std::string& path, TargaImage& tga);
bool LoadPicture(Picture& pic, const std::string& fpath);
bool LoadText(std::vector<std::string>& txt, const std::string& path);
bool LoadWave(SoundFX& sfx, const std::string& path);

/*** Networking ***/
void InitNetwork();
void ShutdownNetwork();

/*** Audio ***/
void Audio_Shutdown();
void InitAudioSystem(HWND hw, HANDLE hlog, int  driver);
void AudioStop();
void Audio_Restore();
void AudioSetCameraPos(float cx, float cy, float cz, float ca, float cb);
void Audio_SetEnvironment(int e, float f);
void SetAmbient(int length, short int* lpdata, int av);
void SetAmbient3d(int length, short int* lpdata, float cx, float cy, float cz);
void AddVoice3dv(int length, short int* lpdata, float cx, float cy, float cz, int vol);
void AddVoice3d(int length, short int* lpdata, float cx, float cy, float cz);
void AddVoicev(int length, short int* lpdata, int v);
void AddVoice(int length, short int* lpdata);

// Menu audio feedback (OpenAL Soft)
bool MenuAudioInit();
void MenuAudioShutdown();
void MenuAudioStartAmbient();
void MenuAudioStopAmbient();
void MenuAudioPlayHover();
void MenuAudioPlayClick();
void MenuAudioPlayType();
void MenuAudioPlayTypeGo();


#ifdef _MAIN_
EXTERNAL char g_KeyNames[256][24] = {
"...",
"Esc",
"1",
"2",
"3",
"4",
"5",
"6",
"7",
"8",
"9",
"0",
"-",
"=",
"BackSpace",
"Tab",
"Q",
"W",
"E",
"R",
"T",
"Y",
"U",
"I",
"O",
"P",
"[",
"]",
"Enter",
"Ctrl",
"A",
"S",
"D",
"F",
"G",
"H",
"J",
"K",
"L",
";",
"'",
"~",
"Shift",
"\\",
"Z",
"X",
"C",
"V",
"B",
"N",
"M",
",",
".",
"/",
"Shift",
"*",
"Alt",
"Space",
"CLock",
"F1",
"F2",
"F3",
"F4",
"F5",
"F6",
"F7",
"F8",
"F9",
"F10",
"NLock",
"SLock",
"Home",
"Up",
"PgUp",
"-",
"Left",
"Middle",
"Right",
"+",
"End",
"Down",
"PgDn",
"Ins",
"Del",
"",
"",
"",
"F11",
"F12",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"MouseL",
"MouseR",
"MouseM",
"<?>",
"Mouse4",
"Mouse5",
"", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""
};
#else
EXTERNAL char g_KeyNames[256][24];
#endif
