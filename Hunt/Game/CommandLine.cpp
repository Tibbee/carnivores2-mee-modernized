// ==========================================================================
// CommandLine.cpp
// ==========================================================================

#include "Hunt.h"
#include "Core/ScoreMod.h"
#include "Network/NetworkManager.h"

// The engine's destination for each wire slot. Kept here rather than in
// ScoreMod.h because these globals belong to the engine, not to the format.
static float* ScoreModTarget(ScoreModSlot slot)
{
  switch (slot) {
  case ScoreModSlot::Camo:     return &ScoreMod_Camo;
  case ScoreModSlot::Radar:    return &ScoreMod_Radar;
  case ScoreModSlot::Scent:    return &ScoreMod_Scent;
  case ScoreModSlot::Double:   return &ScoreMod_Double;
  case ScoreModSlot::Tranq:    return &ScoreMod_Tranq;
  case ScoreModSlot::Observer: return &ScoreMod_Observer;
  case ScoreModSlot::Count:    break;
  }
  return nullptr;
}

static bool equals_nocase(const char* lhs, const char* rhs)
{
  return _stricmp(lhs, rhs) == 0;
}

static bool starts_with_nocase(const char* text, const char* prefix)
{
  return _strnicmp(text, prefix, strlen(prefix)) == 0;
}

void ProcessCommandLine()
{
  auto parse_resolution = [&](const char* value, int& width, int& height) -> bool {
    width = 0;
    height = 0;
    char sep = 0;
    // Single case-insensitive WxH parse (matches EngineInit LoadConfig).
    if (sscanf(value, "%d%c%d", &width, &sep, &height) != 3 ||
        (sep != 'x' && sep != 'X')) {
      return false;
    }
    return width > 0 && height > 0;
  };

  auto sync_resolution_option = [](int width, int height) {
    for (int r = 0; r < ResCount; r++) {
      if (ResolutionList[r].w == width && ResolutionList[r].h == height) {
        CurRes = r;
        OptRes = r;
        return;
      }
    }
  };

  int requestedWidth = WinW;
  int requestedHeight = WinH;
  BOOL requestedFullscreen = FULLSCREEN;
  BOOL requestedBorderless = BORDERLESS;
  bool hasRequestedResolution = false;
  bool hasRequestedFullscreen = false;
  bool hasRequestedBorderless = false;

  for (int a=0; a<__argc; a++)
  {
    LPSTR s = __argv[a];

    if (equals_nocase(s, "/nofullscreen") || equals_nocase(s, "-nofullscreen") ||
        equals_nocase(s, "/windowed") || equals_nocase(s, "-windowed")) {
      requestedFullscreen = false;
      requestedBorderless = false;
      hasRequestedFullscreen = true;
      hasRequestedBorderless = true;
      continue;
    }

    if (equals_nocase(s, "/fullscreen") || equals_nocase(s, "-fullscreen")) {
      requestedFullscreen = true;
      requestedBorderless = false;
      hasRequestedFullscreen = true;
      hasRequestedBorderless = true;
      continue;
    }

    if (equals_nocase(s, "/borderless") || equals_nocase(s, "-borderless")) {
      requestedFullscreen = false;
      requestedBorderless = true;
      hasRequestedFullscreen = true;
      hasRequestedBorderless = true;
      continue;
    }

    if (equals_nocase(s, "/vmode1")) { requestedWidth = 320; requestedHeight = 240; hasRequestedResolution = true; continue; }
    if (equals_nocase(s, "/vmode2")) { requestedWidth = 400; requestedHeight = 300; hasRequestedResolution = true; continue; }
    if (equals_nocase(s, "/vmode3")) { requestedWidth = 512; requestedHeight = 384; hasRequestedResolution = true; continue; }
    if (equals_nocase(s, "/vmode4")) { requestedWidth = 640; requestedHeight = 480; hasRequestedResolution = true; continue; }
    if (equals_nocase(s, "/vmode5")) { requestedWidth = 800; requestedHeight = 600; hasRequestedResolution = true; continue; }

    if (starts_with_nocase(s, "/res=") || starts_with_nocase(s, "-res=")) {
      int width, height;
      if (parse_resolution(strchr(s, '=') + 1, width, height)) {
        requestedWidth = width;
        requestedHeight = height;
        hasRequestedResolution = true;
      }
      continue;
    }

    if (strstr(s,"x="))
    {
      PlayerX = static_cast<float>(atof(&s[2]))*256.f;
      LockLanding = true;
    }
    if (strstr(s,"y="))
    {
      PlayerZ = static_cast<float>(atof(&s[2]))*256.f;
      LockLanding = true;
    }

    if (strstr(s,"reg=")) TrophyRoom.RegNumber = atoi(&s[4]);
    if (strstr(s,"prj=")) strcpy(ProjectName, (s+4));
    if (strstr(s,"din=")) TargetDino = (atoi(&s[4])*1024);
	if (strstr(s, "wep=")) WeaponPres = atoi(&s[4]);
	if (strstr(s, "dtm=")) OptDayNight = atoi(&s[4]);
    if (strstr(s, "server=")) strcpy(g_Network.m_serverAddress, (s + 7));

    if (strstr(s,"-debug"))   DEBUG = true;
    if (strstr(s,"-double"))  DoubleAmmo = true;
	if (strstr(s, "-huntdog"))  g_GameMode = GameMode::DogMode;
	if (strstr(s, "-nightvision")) { NightVisionMode = true; g_GameMode = GameMode::NightVision; }
    if (strstr(s,"-radar"))   RadarMode = true;
	if (strstr(s, "-survival"))  g_GameMode = GameMode::SurvivalMode;
	if (strstr(s, "-sonar"))   { SonarMode = true; g_GameMode = GameMode::SonarMode; }
	if (strstr(s, "-scanner"))   { ScannerMode = true; g_GameMode = GameMode::ScannerMode; }
	if (strstr(s, "-scent"))   ScentMode = true;
	if (strstr(s, "-camo"))   CamoMode = true;
	if (strstr(s, "-multiplayer"))   Multiplayer = true;
	if (strstr(s, "-host"))   Host = true;
	if (strstr(s, "-cisk"))   CiskMode = true;
    if (strstr(s,"-tranq")) Tranq = true;
    if (strstr(s,"-observ")) ObservMode = true;

	// smod=camo,radar,scent,double,tranq,observer. The order lives in
	// Hunt/Core/ScoreMod.h and is shared with the Menu's assembly, so the two
	// cannot drift apart. Modders can override these via the 'accessories {}'
	// block in _RES.TXT (parsed by Menu/Resources.cpp ReadAccessories()).
	if (strstr(s, "smod=")) {
		float mods[kScoreModSlotCount] = {0};
		const int got = ParseScoreModPayload(s + 5, mods);
		for (int i = 0; i < got; ++i) {
			const ScoreModSlot slot = kScoreModWireOrder[i];
			float* target = ScoreModTarget(slot);
			if (target) *target = mods[static_cast<int>(slot)];
		}
	}

  }

  if (hasRequestedFullscreen) FULLSCREEN = requestedFullscreen;
  if (hasRequestedBorderless) BORDERLESS = requestedBorderless;

  if (hasRequestedResolution) {
    if (ResCount > 0) {
      sync_resolution_option(requestedWidth, requestedHeight);
    }
    WinW = requestedWidth;
    WinH = requestedHeight;
  }
}