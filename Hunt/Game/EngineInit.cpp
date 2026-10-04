// EngineInit.cpp -- auto-extracted from Game.cpp
// ==========================================================================
// Auto-extracted from Game.cpp
// ==========================================================================

#include "Hunt.h"
#include <mmsystem.h>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include "Core/ConfigParse.h"
#include "Core/ConfigText.h"
#include "Core/TerrainMip.h"
#include "Core/WaterWaveFade.h"
#include "Core/Flashlight.h"
#include "Core/NightLighting.h"
#include "Core/UnattendedCapture.h"
#include "Loaders/LoadDiagnostics.h"

#ifdef _gl
#include "Renderer/GLPerf.h"
#endif

// Imported from Game.cpp
extern bool ShowFaces;

void UploadGeometry()
{
  int x,y,xx,yy;
  byte temp;

  AudioFCount = 0;

  int MaxView = 18;
  int HalfView = static_cast<int>((MaxView/2))+1;

  for (x = 0; x < MaxView; x++)
    for (y = 0; y < MaxView; y++)
    {
      xx = (x - HalfView)*2;
      yy = (y - HalfView)*2;
      data[AudioFCount].x1 = (CCX+xx) * 256 - CameraX;
      data[AudioFCount].y1 = HMap[CCY+yy][CCX+xx] * ctHScale - CameraY;
      data[AudioFCount].z1 = (CCY+yy) * 256 - CameraZ;

      xx = ((x+1) - HalfView)*2;
      yy = (y - HalfView)*2;
      data[AudioFCount].x2 = (CCX+xx) * 256 - CameraX;
      data[AudioFCount].y2 = HMap[CCY+yy][CCX+xx] * ctHScale - CameraY;
      data[AudioFCount].z2 = (CCY+yy) * 256 - CameraZ;

      xx = ((x+1) - HalfView)*2;
      yy = ((y+1) - HalfView)*2;
      data[AudioFCount].x3 = (CCX+xx) * 256 - CameraX;
      data[AudioFCount].y3 = HMap[CCY+yy][CCX+xx] * ctHScale - CameraY;
      data[AudioFCount].z3 = (CCY+yy) * 256 - CameraZ;

      xx = (x - HalfView)*2;
      yy = ((y+1) - HalfView)*2;
      data[AudioFCount].x4 = (CCX+xx) * 256 - CameraX;
      data[AudioFCount].y4 = HMap[CCY+yy][CCX+xx] * ctHScale - CameraY;
      data[AudioFCount].z4 = (CCY+yy) * 256 - CameraZ;

      AudioFCount++;
    }

//     MessageBeep(-1);

  if (ShowFaces)
  {
    sprintf_s(logt, sizeof(logt),"Audio_UpdateGeometry: %i faces uploaded\n", AudioFCount);
    PrintLog(logt);

    ShowFaces = false;
  }
}
void SetupRes()
{
  // OptRes is an index into ResolutionList[]. Fall back to the first
  // 800x600 entry (or 0) if the saved index is out of range.
  if (ResCount <= 0) {
    WinW = 800;
    WinH = 600;
    return;
  }
  if (OptRes < 0 || OptRes >= ResCount) {
    OptRes = 0;
    for (int r = 0; r < ResCount; r++) {
      if (ResolutionList[r].w == 800 && ResolutionList[r].h == 600) {
        OptRes = r;
        break;
      }
    }
  }
  WinW = ResolutionList[OptRes].w;
  WinH = ResolutionList[OptRes].h;
}
static void AddResolution(int w, int h)
{
  // Append (w, h) to ResolutionList[] if not already present.
  for (int r = 0; r < ResCount; r++) {
    if (ResolutionList[r].w == w && ResolutionList[r].h == h)
      return;
  }
  if (ResCount >= 128) return;
  ResolutionList[ResCount].w = w;
  ResolutionList[ResCount].h = h;
  ResCount++;
}
void EnumerateResolutions()
{
  // Populate ResolutionList[] from the display's available modes.
  // Replaces the old hardcoded 8-entry table in SetupRes(). The list
  // is built at startup, deduplicated, and capped at 128 entries.
  // 16-bit minimum (matches the DIB depth in CreateVideoDIB).
  //
  // Top cap: the current desktop mode (ENUM_CURRENT_SETTINGS). This is
  // the monitor's active resolution. We always include it explicitly
  // even if the driver doesn't report it through the enumeration loop,
  // so a 2560x1440 native panel always has its native mode selectable.
  // We never offer modes wider/taller than the desktop because
  // SetVideoMode() can't actually display them.
  ResCount = 0;

  int desktopW = GetSystemMetrics(SM_CXSCREEN);
  int desktopH = GetSystemMetrics(SM_CYSCREEN);

  DEVMODE current;
  ZeroMemory(&current, sizeof(current));
  current.dmSize = sizeof(current);
  if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &current)) {
    // Prefer the DEVMODE values -- they can be slightly different from
    // GetSystemMetrics in multi-monitor / DPI-scaled setups.
    desktopW = current.dmPelsWidth;
    desktopH = current.dmPelsHeight;
  }

  DEVMODE dm;
  ZeroMemory(&dm, sizeof(dm));
  dm.dmSize = sizeof(dm);
  for (int i = 0; EnumDisplaySettings(nullptr, i, &dm); i++) {
    if (dm.dmBitsPerPel < 16) continue;
    if (dm.dmPelsWidth  > desktopW ||
        dm.dmPelsHeight > desktopH)
      continue;
    AddResolution(dm.dmPelsWidth, dm.dmPelsHeight);
  }

  // Always include the current desktop resolution itself. Some drivers
  // don't enumerate the native panel mode, so without this the list
  // would silently cap below the monitor's actual capability.
  AddResolution(desktopW, desktopH);

  // Guarantee at least one entry: 800x600 (the historical default).
  if (ResCount == 0) {
    ResolutionList[0].w = 800;
    ResolutionList[0].h = 600;
    ResCount = 1;
  }
}
void SubmitDinoScore (int cindex) {
	float score = DinoInfo[Characters[cindex].CType].BaseScore;

	if (TrophyRoom.Last.success > 1)
		score *= (1.f + TrophyRoom.Last.success / 10.f);

	//if (!(TargetDino & (1<<DinoInfo[Characters[cindex].CType].menuDino)) ) score/=2.f;

	SYSTEMTIME st;
	GetLocalTime(&st);
	// Score multipliers are now driven by the Menu (see smod= in
	// ProcessCommandLine) so modders can tune them via _RES.TXT.
	// Defaults match the original hardcoded values when no smod= is
	// supplied (see InitEngine).
	if (Tranq) score *= ScoreMod_Tranq;
	if (RadarMode) score *= ScoreMod_Radar;
	if (ScentMode) score *= ScoreMod_Scent;
	if (CamoMode) score *= ScoreMod_Camo;
	TrophyRoom.Score += static_cast<int>(score);
	Characters[cindex].tempScore = static_cast<int>(score);
	Characters[cindex].tempDate = (st.wYear << 20) + (st.wMonth << 10) + st.wDay;
	Characters[cindex].tempTime = (st.wHour << 10) + st.wMinute;
	Characters[cindex].tempRange = VectorLength(SubVectors(Characters[cindex].pos, PlayerPos)) / 64.f;

	ScoreDispTime = 2500;
	ScoreDisp = static_cast<int>(score);

}
void HideWeapon()
{
  TWeapon *wptr = &Weapon;
  if (IsUnderwater() && !wptr->state && !WeapInfo[CurrentWeapon].harpoon) return;
  // Belt and braces: the mode check plus the stable map-name check (the mode
  // value has many writers and can be flipped mid-session; ProjectName cannot).
  if (ObservMode || g_GameMode == GameMode::TrophyMode || InTrophyRoomMap()) return;
  if (g_GameMode == GameMode::SurvivalMode) return;

  if (wptr->state == 0)
  {  
	//if (!ShotsLeft[CurrentWeapon]) return;
    // ScopePower is only meaningful for optic weapons; non-optic raises
    // leave it untouched (it is unused while not in OpticScope). An
    // explicit init lives in StateDefs.cpp so the first scoped frame is sane.
    // Breath-aim weapons rest unzoomed and magnify only while holding
    // breath, so they raise at 1x; scoped weapons rest at their optic.
    if (WeapInfo[CurrentWeapon].Optic) {
      g_GameMode = GameMode::OpticScope;
      ScopePower = WeapInfo[CurrentWeapon].breathaim ? 1.0f : WeapInfo[CurrentWeapon].Optic;
    }
    
	if (IsUnderwater()) {
		const int sound = FindWeaponSound(*wptr, CurrentWeapon,
			WeapInfo[CurrentWeapon].getAqSnd);
		if (sound >= 0)
			AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[sound].length,
				wptr->chinfo[CurrentWeapon].SoundFX[sound].lpData.data(), 256);
	} else {
		const int fx = FindWeaponAnimationSound(*wptr, CurrentWeapon,
			WeapInfo[CurrentWeapon].getAnim);
		if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
			wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
	}
    wptr->FTime = 0;
    wptr->state = 1;
    // Clear only binoculars here; the optic scope (if any) must stay active
    // through the raise animation. The previous code reset the whole game
    // mode, which stomped the OpticScope just set above and silently
    // disabled every scoped weapon outside Survival mode.
    if (g_GameMode == GameMode::Binocular) g_GameMode = GameMode::Normal;
    wptr->shakel = WeapInfo[CurrentWeapon].shake * 4.f;
	wptr->breath = 0.f;
	wptr->breathPressed = 0;
	wptr->HoldBreath = false;
    return;
  }

  if (wptr->state!=2 || wptr->FTime!=0) return;
  if (IsUnderwater()) {
	  const int sound = FindWeaponSound(*wptr, CurrentWeapon,
		  WeapInfo[CurrentWeapon].putAqSnd);
	  if (sound >= 0)
		  AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[sound].length,
			  wptr->chinfo[CurrentWeapon].SoundFX[sound].lpData.data(), 256);
  } else {
	  const int fx = FindWeaponAnimationSound(*wptr, CurrentWeapon,
		  WeapInfo[CurrentWeapon].putAnim);
	  if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
		  wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
  }
  wptr->state = 3;
  wptr->FTime = 0;
  g_GameMode = GameMode::Normal;
  return ;
}
void InitGameInfo()
{
  for (int c=0; c< DINOINFO_MAX; c++)
  {
    DinoInfo[c].Scale0 = 800;
    DinoInfo[c].ScaleA = 600;
    DinoInfo[c].ShDelta = 0;
  }
  LoadResourcesScript();
}


// MULTIPLAYER ===================================================




















static void CreateDefaultConfig();
static void LoadConfig();
static void GetConfigPath(char* buf, size_t bufsz);

// The load policy must be known before InitGameInfo() parses _RES.TXT.
// config.cfg is re-read here for just this key (the full LoadConfig() runs
// later in InitEngine); C2_STRICT_DATA overrides it for CI/mod authoring.
static void LoadLoadPolicy()
{
  char configPath[MAX_PATH];
  GetConfigPath(configPath, sizeof(configPath));

  std::ifstream input(configPath, std::ios::binary);
  if (input)
  {
    std::string text;
    size_t nulBytes = 0;
    if (ReadConfigText(input, text, nulBytes))
      InitLoadPolicyFromConfigText(text.c_str());
  }
  InitLoadPolicyFromEnvironment();
}

void InitEngine()
{
  FULLSCREEN   = true;
  BORDERLESS   = false;
  DEBUG        = false;

  // Explicit zoom defaults: zero-init would collapse CameraW/H (which are
  // multiplied by these in OpticScope/Binocular) to a degenerate frustum
  // on any frame that runs before HideWeapon()/the per-frame clamp.
  ScopePower = 1.0f;
  BinocularPower = 2.0f;

  WATERANI     = true;
  NODARKBACK   = true;
  LoDetailSky  = true;
  CORRECTION   = true;
  FOGON        = true;
  FOGENABLE    = true;
  UIScale      = 1.0f;
  Clouds       = true;
  SKY          = true;
  GOURAUD      = true;
  MODELS       = true;
  TIMER        = DEBUG;
  BITMAPP      = false;
  MIPMAP       = true;
  NOCLIP       = false;
  CLIP3D       = true;


  SLOW         = false;
  LOWRESTX     = false;
  MORPHP       = true;
  MORPHA       = true;

  _GameState = 0;
  _MultiplayerState = 0;

  RadarMode    = false;
  NightVisionMode = false;
  NightVisionOn   = false;
  NightVisionKey  = 0x4E; // Default: 'N' key
  NightLightingOptions() = NightLightingSettings{};

  // Flashlight accessory: off until a hunt is launched with -flashlight.
  FlashlightMode = false;
  FlashlightOn   = false;
  FlashlightKey  = kFlashlightKeyDefault;
  OptFlashlightInnerAngleDeg = static_cast<int>(kFlashlightInnerAngleDegDefault);
  OptFlashlightOuterAngleDeg = static_cast<int>(kFlashlightOuterAngleDegDefault);
  OptFlashlightRange         = static_cast<int>(kFlashlightRangeDefault);
  OptFlashlightIntensityPct  = kFlashlightIntensityPctDefault;

  // Accessory score multipliers. Defaults match the legacy hardcoded
  // values that used to live in SubmitDinoScore() so legacy hunts
  // (launched without a 'smod=' argument) keep the same final score.
  ScoreMod_Camo     = 0.85f;
  ScoreMod_Radar    = 0.70f;
  ScoreMod_Scent    = 0.80f;
  ScoreMod_Double   = 1.0f;
  ScoreMod_Tranq    = 1.25f;
  ScoreMod_Observer = 1.0f;

  //multiplayer
  Multiplayer = false;
  Host = false;
  result = nullptr;

  fnt_BIG = CreateFont(
              static_cast<int>((23 * UIScale)), static_cast<int>((10 * UIScale)), 0, 0,
              600, 0,0,0,
#ifdef __rus
              RUSSIAN_CHARSET,
#else
              ANSI_CHARSET,
#endif
              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);




  fnt_Small = CreateFont(
                static_cast<int>((16 * UIScale)), static_cast<int>((7 * UIScale)), 0, 0,
				100, 0,0,0,
	  
	  //14, 5, 0, 0,
	  //100, 0, 0, 0,
#ifdef __rus
                RUSSIAN_CHARSET,
#else
                ANSI_CHARSET,
#endif
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);


  fnt_Midd  = CreateFont(
			    static_cast<int>((16 * UIScale)), static_cast<int>((7 * UIScale)), 0, 0,
	            550, 0, 0, 0,
#ifdef __rus
                RUSSIAN_CHARSET,
#else
                ANSI_CHARSET,
#endif
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);


  Heap = HeapCreate( 0, 60000000, 0 );
  if( Heap == nullptr )
  {
    if (unattended_capture::options.enabled) DoHalt2("Error creating heap.");
    MessageBox(hwndMain,"Error creating heap.","Error",IDOK);
    return;
  }

  // Phase 5C.2: Construct the per-level MemoryArena. C1 does this at
  // Carnivores1/Hunt/Game/GameLifecycle.cpp with 128 MiB; we use LEVEL_ARENA_SIZE
  // (256 MiB) because C2 ME has more resident state (8 weapons, 128
  // dino types, multiplayer, snow, etc.). The arena must be created
  // AFTER HeapCreate (the Heap variable is referenced by all the
  // _HeapAlloc dispatch) and BEFORE LoadResources (which tags most
  // per-level allocations as MemoryTag::Level, causing them to land
  // here instead of in Heap).
  //
  // Defensive: verify the smart pointer layout assumption that all of
  // Phase 5 depends on -- if sizeof(unique_heap_ptr<T>) ever drifts
  // away from a raw pointer, every struct field we migrated will
  // silently change size and break the save-game / multiplayer
  // protocols. C1 has the same static_assert in its InitEngine.
  static_assert(sizeof(unique_heap_ptr<WORD[]>) == sizeof(void*),
                "unique_heap_ptr<T[]> must be the same size as a raw pointer (x86 EBO)");
  static_assert(sizeof(unique_obj_ptr<TModel>) == sizeof(void*),
                "unique_obj_ptr<TModel> must be the same size as a raw pointer (x86 EBO)");
  LevelArena = new MemoryArena(LEVEL_ARENA_SIZE, "LevelArena");

  // Phase 5E: tag as MemoryTag::Global. This is a one-time
  // allocation in InitEngine that lives for the whole session --
  // it backs the "null" texture at index 255 (used as a fallback
  // when a model references a missing texture). It's released by
  // ReleaseGlobalResources at shutdown. Goes to the heap, not the
  // arena, because it must survive all LevelArena->Reset() calls.
  Textures[255].reset((TEXTURE*) _HeapAlloc(Heap, 0, sizeof(TEXTURE), MemoryTag::Global));

  WaterR = 10;
  WaterG = 38;
  WaterB = 46;
  WaterA = 10;
  TargetDino = 1<<10;
  TargetCall = 10;
  WeaponPres = 1;
  MessageList.timeleft = 0;

  LoadLoadPolicy();
  InitGameInfo();

  CreateFadeTab();
  CreateDivTable();
  InitClips();

  TrophyRoom.RegNumber=0;
  PlayerZ = (ctMapSize / 3) * 256;

  ProcessCommandLine();

  switch (OptDayNight)
  {
  case 0:
    SunShadowK = 0.7;
    Sun3dPos.x = - 4048;
    Sun3dPos.y = + 2048;
    Sun3dPos.z = - 4048;
    break;
  case 1:
    SunShadowK = 0.5;
    Sun3dPos.x = - 2048;
    Sun3dPos.y = + 4048;
    Sun3dPos.z = - 2048;
    break;
  case 2:
    SunShadowK = -0.7;
    Sun3dPos.x = + 3048;
    Sun3dPos.y = + 3048;
    Sun3dPos.z = + 3048;
    break;
  }

  // EnumerateResolutions() must come before LoadTrophy() so SetupRes()
  // (called inside LoadTrophy via ReadFile -> SetupRes) can use
  // ResolutionList[] to translate the saved OptRes index into a real
  // WinW/WinH. If we enumerated after, LoadTrophy would have no
  // resolution table to apply.
  EnumerateResolutions();

  // OptFov is the vertical field-of-view in degrees, range [kFovMin,
  // kFovMax]. It drives CameraH = VideoCY * FovScaleFromDegrees(OptFov)
  // in SetVideoMode() and the per-frame camera setup in Hunt.cpp. The
  // default is set unconditionally here as a safety net for first
  // launch (no save file) and for old saves written before the
  // FOV-slider port that don't include the OptFov field. LoadTrophy()
  // overwrites this default with the persisted value (or keeps this
  // default if the saved value is missing/out-of-range).
  OptFov = kFovDefault;
  g_gpuFeatures = kGpuFeaturesDefault;  // GPU-optimization kill-switch (see GameState.h)
#ifdef _soft
  // The software renderer defaults to the legacy camera-coupled plane. Mode 1
  // is implemented and selectable here, but it is banded rather than smooth:
  // the software sky point-samples one texel per pixel with no mip chain, and
  // the level plane minifies far harder than the legacy one. OpenGL's default
  // stays the level plane, where trilinear mip filtering resolves the same
  // geometry cleanly.
  OptSkyMode = kSkyModeLegacy;
#else
  OptSkyMode = kSkyModeLevel;           // sky cloud-texture placement (config.cfg sky_mode)
#endif
  // sky_mode_soft: software-only, always defaulting to the legacy plane.
  OptSkyModeSoft = kSkyModeLegacy;
  OptSkyDomeScale = kSkyDomeScaleDefault;
  OptSkyHorizonDrop = kSkyHorizonDropDefault;
  OptTerrainMaxMipLevel = kTerrainMaxMipDefault; // config.cfg terrain_mip_level
  OptWaterMipmap = 1;                           // config.cfg water_mipmap
  OptWaterWaveFadeRings = kWaterWaveFadeDefault; // config.cfg water_wave_fade_rings
  OptBoundBoxFit = 0;                           // config.cfg boundbox_fit (1/2 = experimental fit)
  OptBoundBoxFitWin = kBoundBoxFitWinDefault;   // config.cfg boundbox_fit_win

  OptViewR = kViewOptDefault;
  OptObjectDetail = kObjectDetailDefault;
OptFpsLimit = 1;  // 1 = 60 FPS (0 remains available for unlimited)

  // Default key bindings (mirror Menu Options::Default). LoadTrophy()
  // overwrites these from the save; without defaults a missing/short save
  // (first launch, pre-modernization file) leaves KeyMap zeroed and every
  // bound action -- notably sprint -- dead until the user rebinds in the menu.
  KeyMap.fkForward = 'W';
  KeyMap.fkBackward = 'S';
  KeyMap.fkSLeft = 'A';
  KeyMap.fkSRight = 'D';
  KeyMap.fkFire = VK_LBUTTON;
  KeyMap.fkShow = VK_RBUTTON;
  KeyMap.fkJump = VK_SPACE;
  KeyMap.fkCall = VK_MENU;
  KeyMap.fkBinoc = 'B';
  KeyMap.fkCrouch = 'C';
  KeyMap.fkRun = VK_LSHIFT;
  KeyMap.fkReload = 'R';
  KeyMap.fkResupply = 'T';
  KeyMap.fkHoldBreath = VK_LCONTROL;
  KeyMap.fkFiringMode = 'V';
  KeyMap.fkStrafe = 'G';

  LoadTrophy();
  OptViewR = ClampViewOpt(OptViewR);

  // Create default config.cfg if it doesn't exist (first launch).
  CreateDefaultConfig();

  // Override settings from config.cfg (written by Carnivores2Menu).
  // This file is the single source of truth for settings that are not
  // part of the legacy binary trophy format (e.g. OptFov).
  LoadConfig();
  {
    const int fpsValues[] = {0, 60, 120, 240};
    char msg[192];
    sprintf_s(msg, sizeof(msg), "Config effective: fps_limit=%d (%d FPS; 0=unlimited), fov=%d, object_detail=%d\n",
              OptFpsLimit, fpsValues[OptFpsLimit], OptFov, OptObjectDetail);
    PrintLog(msg);
  }

#ifdef _gl
  // Init3DHardware runs before InitEngine, so apply the config value now
  // that glperf_logging has been loaded. Disabled profiling must remain out
  // of the per-frame render path even when GL_PERF_HOOKS is compiled in.
  glperf_set_logging(g_glperfLoggingEnabled);
#endif

  // CreateVideoDIB() must come after ProcessCommandLine() so WinW/WinH reflect
  // any /res command-line override.
  ProcessCommandLine();
  CreateVideoDIB();

  if (g_GameMode == GameMode::SurvivalMode) OptViewR = kViewOptMax;

  ctViewR = ViewOptToCtViewR(OptViewR);
  // Cap the character-processing view radius independently of the
  // rendering radius.  At ctViewR=230 the game processes characters
  // within a ~4B sq-unit area, causing multi-second frame freezes.
  // A cap of 120 keeps character LOD ~2.2x default while preventing
  // the worst scalability cliff.
  charViewR = (std::min)(ctViewR, 120);
  ctViewRM = ClampObjectDetail(OptObjectDetail);

  Soft_Persp_K = 1.5f;
  HeadY = 220;

  FogsList[0].fogRGB = 0x000000;
  FogsList[0].YBegin = 0;
  FogsList[0].Transp = 000;
  FogsList[0].FLimit = 000;

  FogsList[127].fogRGB = 0x00504000;
  FogsList[127].Mortal = false;
  // Underwater fog density.  Transp=220, FLimit=200: builds up moderately
  // fast, ~78% max opacity.  Was Transp=460 which gave a sparse, dark
  // underwater look.  The depth-based multiplier in CalcFogLevel() ramps
  // density up further as the camera goes deeper, so close-range vertices
  // already look heavily tinted at depth and far vertices saturate at
  // FLimit+60 (260) which the per-vertex shader clamps to 1.0.
  FogsList[127].Transp = 220;
  FogsList[127].FLimit = 200;

  FillMemory( FogsMap, sizeof(FogsMap), 0);
  PrintLog("Init Engine: Ok.\n");
}
void ShutDownEngine()
{
  // Phase 5C.1: Release per-level and global resources before tearing
  // down the heap and the window DC. C1's ShutDownEngine has these calls
  // (Carnivores1/Hunt/Game/GameLifecycle.cpp); C2 ME was missing them, so every
  // Quit leaked the level resources, the weapon character info, the
  // Sun/Compass/Binocular models, and the menu pictures. The LevelArena
  // construction is still pending in Phase 5C.2; once it's in, the
  // ReleaseResources() call will also trigger LevelArena->Reset().
  ReleaseResources();
  ReleaseGlobalResources();
  ReleaseDC(hwndMain,hdcMain);

  // Phase 5F.2: Print the leak report to carnivor.log before tearing
  // down the arena. Must run AFTER Release* (so the per-level
  // allocations and global allocations have been released) and BEFORE
  // delete LevelArena (so the pointer values in the report are still
  // valid -- the report is informational only, but the doc explicitly
  // notes that printing after the arena is freed is wasteful). In
  // non-MEM_DEBUG builds the call is a no-op (the function expands to
  // a single branch and returns immediately).
#ifdef MEM_DEBUG
  PrintMemoryLeaks();
#endif

  // Phase 5C.2: Tear down the LevelArena after all _HeapFree calls have
  // run. C1 has the same order (Carnivores1/Hunt/Game/GameLifecycle.cpp).
  // LevelArena->Reset() in ReleaseResources() expects LevelArena to be
  // alive; delete must come AFTER that. The VirtualFree on the arena's
  // base pointer is the only thing the destructor does.
  if (LevelArena) {
    delete LevelArena;
    LevelArena = nullptr;
  }
}
void ProcessSyncro()
{
  RealTime = timeGetTime();
  srand( (unsigned) RealTime );
  if (SLOW) RealTime/=4;
  TimeDt = RealTime - PrevTime;
  if (TimeDt<0) TimeDt = 10;
  if (TimeDt>10000) TimeDt = 10;
  if (TimeDt>1000) TimeDt = 1000;
  PrevTime = RealTime;
  Takt++;
  if (!IsPaused())
    if (MyHealth) MyHealth+=TimeDt*4;
  if (MyHealth>MAX_HEALTH) MyHealth = MAX_HEALTH;
}
void MakeCall()
{
  if (!TargetDino) return;
  if (IsUnderwater()) return;
  if (ObservMode || g_GameMode == GameMode::TrophyMode || InTrophyRoomMap()) return;
  if (CallLockTime) return;

  CallLockTime=1024*3;

  NextCall+=(RealTime % 2)+1;
  NextCall%=3;

  AddVoicev(fxCall[TargetCall-10][NextCall].length,
            fxCall[TargetCall-10][NextCall].lpData.data(), 256);

  //multiplayer
  sendHunterCall = TargetCall - 10;
  sendHunterCallType = NextCall;

  ReactToHunterCall(PlayerPos, TargetCall - 10);

  // NOTE: float-first arithmetic is load-bearing here. (512*256)^2 overflows
  // int32 (it is exactly 4*2^32 and wraps to 0), which made dSq<dminSq false
  // for every dino and silently disabled all call answers (issue #1).
  float dminSq = 512.f * 256.f * 512.f * 256.f;
  int ai = -1;

  for (int c=0; c<ChCount; c++)
  {
    TCharacter *cptr = &Characters[c];

	float dx = PlayerX - cptr->pos.x;
	float dy = PlayerY - cptr->pos.y;
	float dz = PlayerZ - cptr->pos.z;
	float dSq = dx * dx + dy * dy + dz * dz;
	float hearRange = (GameplayViewRadiusCells(ctViewR) * 400.0f)
		* (DinoInfo[cptr->CType].HearK * 2);
	bool canHear = dSq < hearRange * hearRange;

	/*
    if (DinoInfo[AI_to_CIndex[TargetCall] ].DangerCall)
      if (cptr->AI<10)
      {
        cptr->State=2;
        cptr->AfraidTime = (10 + rRand(5)) * 1024;
      }
	  */

	if (DinoInfo[cptr->CType].menuDino != TargetCall-10) continue;
	if (cptr->AfraidTime) continue;
    if (cptr->State) continue;

    
    if (canHear)
    {
      if (rRand(128) > 32)
        if (dSq<dminSq)
        {
          dminSq = dSq;
          ai = c;
        }
      cptr->tgx = PlayerX + siRand(1800);
      cptr->tgz = PlayerZ + siRand(1800);
    }
  }

  if (ai!=-1)
  {
    answpos = SubVectors(Characters[ai].pos, PlayerPos);
    answpos.x/=-3.f;
    answpos.y/=-3.f;
    answpos.z/=-3.f;
    answpos = SubVectors(PlayerPos, answpos);
    answtime = 2000 + rRand(2000);
    answcall = TargetCall;
  }

}
static void GetConfigPath(char* buf, size_t bufsz)
{
  // Try EXE directory first
  char mod[MAX_PATH];
  DWORD len = GetModuleFileNameA(nullptr, mod, sizeof(mod));
  if (len > 0 && len < sizeof(mod)) {
    char* sep = strrchr(mod, '\\');
    if (sep) {
      *(sep + 1) = '\0';
      strcat_s(mod, sizeof(mod), "config.cfg");
      if (GetFileAttributesA(mod) != INVALID_FILE_ATTRIBUTES) {
        strcpy_s(buf, bufsz, mod);
        return;
      }
    }
  }
  // Fall back to CWD
  strcpy_s(buf, bufsz, "config.cfg");
}

// Create a default config.cfg with all available settings documented.
// Called when no config file exists (first launch or manual deletion).
static void CreateDefaultConfig()
{
  char configPath[MAX_PATH];

  // Prefer EXE directory (shared with Carnivores2Menu), fall back to CWD
  char mod[MAX_PATH];
  DWORD len = GetModuleFileNameA(nullptr, mod, sizeof(mod));
  char* writePath = configPath;
  if (len > 0 && len < sizeof(mod)) {
    char* sep = strrchr(mod, '\\');
    if (sep) {
      *(sep + 1) = '\0';
      strcat_s(mod, sizeof(mod), "config.cfg");
      writePath = mod;
    }
  }
  if (writePath == configPath) {
    strcpy_s(configPath, sizeof(configPath), "config.cfg");
  }

  // Don't overwrite if file already exists (e.g. created by menu)
  if (GetFileAttributesA(writePath) != INVALID_FILE_ATTRIBUTES) {
    return;
  }

  HANDLE hfile = CreateFileA(writePath, GENERIC_WRITE, 0, nullptr,
                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hfile == INVALID_HANDLE_VALUE) {
    return;
  }

  // Build the default config content (kept in sync with the
  // clean config.cfg generated by tools/release/make-c2-package.ps1).
  char buf[8192];
  int len2 = snprintf(buf, sizeof(buf),
    "# Carnivores 2 Modder's Engine - Configuration\r\n"
    "# This file is auto-generated on first launch.\r\n"
    "# Edit values as needed — they are validated on load.\r\n"
    "#\r\n"
    "# The menu updates the settings it owns when you change them.\r\n"
    "# Comments and other keys (including audio overrides) are preserved.\r\n"
    "\r\n"
    "# Renderer: 0=Software, 1=OpenGL (default: 1)\r\n"
    "renderer 1\r\n"
    "\r\n"
    "# Field of view in degrees (default: 62, range: 35-90)\r\n"
    "fov %d\r\n"
    "\r\n"
    "# Object detail / view distance (default: 48, range: 24-96)\r\n"
    "object_detail %d\r\n"
    "\r\n"
    "# FPS limit: 0=Unlimited, 1=60, 2=120, 3=240 (default: 1)\r\n"
    "fps_limit 1\r\n"
    "\r\n"
    "# Verbose logging: 0=off, 1=on (default: 0)\r\n"
    "verbose_logging 0\r\n"
    "\r\n"
    "# AI awareness trace: 0=off, 1=on (default: 0). One line per hunter\r\n"
    "# event and a per-second state line per reacting creature, written to\r\n"
    "# render.log. Independent of verbose_logging, which also enables it.\r\n"
    "ai_logging 0\r\n"
    "\r\n"
    "# Recover safe legacy/mod script values by default. For mod validation,\r\n"
    "# use load_mode strict to stop on the first recoverable error.\r\n"
    "load_mode lenient\r\n"
    "\r\n"
    "# Nightvision key VK code (default: 78 = 'N')\r\n"
    "nightvision_key 78\r\n"
    "\r\n"
    "# OpenGL Moonlit night brightness and desaturation.\r\n"
    "# Brightness is 5..100 percent; desaturation is 0..100 percent.\r\n"
    "# Flashlight restores local brightness/color; night vision bypasses it.\r\n"
    "night_world_brightness 22\r\n"
    "night_desaturation 85\r\n"
    "night_sky_brightness 60\r\n"
    "# Moon disc only: RGB brightness 0..1000 percent, 100 = original.\r\n"
    "night_moon_brightness 128\r\n"
    "# Moon halo only: strength 0..1000 percent, 100 = existing glow.\r\n"
    "night_moon_glow_strength 100\r\n"
    "\r\n"
    "# Flashlight key VK code (default: 70 = 'F'). The flashlight is only\r\n"
    "# available on hunts launched with the Flashlight accessory selected.\r\n"
    "flashlight_key 70\r\n"
    "\r\n"
    "# Flashlight beam shape. inner/outer are half-angles in degrees from the\r\n"
    "# view axis (full strength inside `inner`, faded out by `outer`); range is\r\n"
    "# world units (256 = one terrain cell) and intensity is the peak boost as\r\n"
    "# a percent, so 500 multiplies lit surface colours by up to 6.0 before\r\n"
    "# the headroom roll-off that keeps lit terrain from clipping.\r\n"
    "flashlight_inner_angle 14\r\n"
    "flashlight_outer_angle 26\r\n"
    "flashlight_range 6000\r\n"
    "flashlight_intensity 500\r\n"
    "\r\n"
    "# Window resolution, e.g. 1920x1080. Overrides the per-profile\r\n"
    "# resolution saved in trophy0N.sav. Omit to use the profile value.\r\n"
    "# The menu writes this automatically when you change Resolution.\r\n"
    "#resolution 1920x1080\r\n"
    "\r\n"
    "# Display mode: 0=windowed, 1=exclusive fullscreen, 2=borderless\r\n"
    "# fullscreen (default: 2). The menu writes this when you change the\r\n"
    "# Display Mode video option.\r\n"
    "display_mode 2\r\n"
    "\r\n"
    "# Sky cloud-texture placement: 0=legacy camera-coupled offset,\r\n"
    "# 1=world-level projected plane, 2=direction-based dome.\r\n"
    "# 1 is the OpenGL default. The software renderer has its own key,\r\n"
    "# because both read this file and it bands on 1; see sky_mode_soft.\r\n"
    "sky_mode 1\r\n"
    "\r\n"
    "# Software renderer only (v_soft.ren): same 0/1/2 values, separate\r\n"
    "# key because both renderers read this file and want different\r\n"
    "# defaults. Mode 1 bands in software, so this defaults to 0.\r\n"
    "sky_mode_soft 0\r\n"
    "\r\n"
    "# sky_mode 1 horizon drop in degrees: lowers the projected plane's\r\n"
    "# compression singularity below the true horizon (C1 used ~17 deg).\r\n"
    "# 0 = plain level plane.\r\n"
    "sky_horizon_drop 12\r\n"
    "\r\n"
    "# Dome (sky_mode 2) canopy scale: texture texels per radian at the\r\n"
    "# horizon. Larger = smaller clouds (384: one 256-texel texture spans\r\n"
    "# ~38 degrees; zenith clouds are 2x larger).\r\n"
    "sky_dome_scale 384\r\n"
    "\r\n"
    "# Highest mip level the OpenGL terrain/water sampler may select from\r\n"
    "# the DataA 128 / DataB 64 / DataC 32 / DataD 16 chain: 0..3.\r\n"
    "# 3 = whole chain (default, owner-reviewed). 1 = stock D3D/3DFX\r\n"
    "# behaviour (128 + 64 only). 0 = no mip filtering.\r\n"
    "terrain_mip_level 3\r\n"
    "\r\n"
    "# Software renderer only: 1 = water rasterises from the distance mip\r\n"
    "# level, 0 = legacy full-resolution water (always the 128 level).\r\n"
    "water_mipmap 1\r\n"
    "\r\n"
    "# Rings over which the water wave displacement fades to zero before\r\n"
    "# the legacy cutoff (the outermost 4 rings are never displaced).\r\n"
    "# 0 = legacy hard cutoff. Larger = gradual, seam-free falloff.\r\n"
    "water_wave_fade_rings 8\r\n"
    "\r\n"
    "# ofBOUND collision-box fit (experimental): 0=off (stock axis-aligned\r\n"
    "# boxes), 1=fit oriented boxes using 0/45-degree candidates, 2=min-area\r\n"
    "# rectangle (any baked angle). Floor/ceiling heights never change; only\r\n"
    "# the X/Z footprint does. A fit is used only when its padded footprint\r\n"
    "# beats the axis-aligned box by boundbox_fit_win percent.\r\n"
    "boundbox_fit 0\r\n"
    "\r\n"
    "# Minimum padded-area win (percent, 0..100) before a fitted collision box\r\n"
    "# replaces the axis-aligned one. Keeps stock content as-is.\r\n"
    "boundbox_fit_win 20\r\n"
    "\r\n"
    "# GPU features bitmask (default: all optimizations enabled)\r\n"
    "# Set to 0 to disable all GPU optimizations.\r\n"
    "gpufeatures %u\r\n"
    "\r\n"
    "# GL performance logging: 0=off, 1=on (default: 0)\r\n"
    "# Requires GL_PERF_HOOKS build. Writes glperf-*.log files.\r\n"
    "glperf_logging 0\r\n"
    "\r\n"
    "# Audio reverb tuning (OpenAL EFX only; restart the game to apply).\r\n"
    "# envN: 0=Generic, 1=Plate, 2=Forest, 3=Mountain, 4=Canyon,\r\n"
    "# 5=Cave, 6=Special-1, 7=intentional no-op, 8=Underwater.\r\n"
    "# decay: seconds [0.1,20]; decayhf: HF decay ratio [0.1,2];\r\n"
    "# diffusion: [0,1]; reverb: late-reverb level in mB [-10000,0].\r\n"
    "# Compiled defaults for every effective preset; change fields to override.\r\n"
    "# Environment 7 is an intentional no-op and has no effective tuning.\r\n"
    "# 0: Generic\r\n"
    "env0_decay 1.49\r\n"
    "env0_decayhf 0.3\r\n"
    "env0_diffusion 0.4\r\n"
    "env0_reverb -620\r\n"
    "# 1: Plate\r\n"
    "env1_decay 1.2\r\n"
    "env1_decayhf 0.2\r\n"
    "env1_diffusion 0.2\r\n"
    "env1_reverb -600\r\n"
    "# 2: Forest\r\n"
    "env2_decay 1.49\r\n"
    "env2_decayhf 0.18\r\n"
    "env2_diffusion 0.3\r\n"
    "env2_reverb -700\r\n"
    "# 3: Mountain\r\n"
    "env3_decay 2.6\r\n"
    "env3_decayhf 0.2\r\n"
    "env3_diffusion 0.326\r\n"
    "env3_reverb -500\r\n"
    "# 4: Canyon\r\n"
    "env4_decay 1.1\r\n"
    "env4_decayhf 0.6\r\n"
    "env4_diffusion 0.275\r\n"
    "env4_reverb -700\r\n"
    "# 5: Cave\r\n"
    "env5_decay 3.2\r\n"
    "env5_decayhf 0.6\r\n"
    "env5_diffusion 0.9\r\n"
    "env5_reverb -200\r\n"
    "# 6: Special\r\n"
    "env6_decay 1.8\r\n"
    "env6_decayhf 0.4\r\n"
    "env6_diffusion 0.4\r\n"
    "env6_reverb -400\r\n"
    "# 8: Underwater\r\n"
    "env8_decay 1.5\r\n"
    "env8_decayhf 0.1\r\n"
    "env8_diffusion 0.1\r\n"
    "env8_reverb -200\r\n"
    "\r\n"
    "# Last hunt setup (written by the menu when you launch a hunt).\r\n"
    "# The menu reselects these on the hunt screen; delete them to\r\n"
    "# return to entry defaults. These examples do not force a selection.\r\n"
    "#hunt_area area1\r\n"
    "#hunt_dinos 0\r\n"
    "#hunt_weapons 0\r\n"
    "#hunt_utils 0\r\n"
    "#hunt_time 0\r\n",
    kFovDefault,
    kObjectDetailDefault,
    kGpuFeaturesDefault
  );

  // A truncated template would cut a key mid-line and silently drop the
  // tail settings, so refuse to write and remove the placeholder file
  // (otherwise the next launch sees a file and never retries).
  if (len2 < 0 || static_cast<size_t>(len2) >= sizeof(buf)) {
    PrintLog("Config: default template exceeds the write buffer; config.cfg not written.\n");
    CloseHandle(hfile);
    DeleteFileA(writePath);
    return;
  }

  DWORD written = 0;
  WriteFile(hfile, buf, (DWORD)len2, &written, nullptr);
  CloseHandle(hfile);

  char msg[MAX_PATH + 64];
  sprintf_s(msg, sizeof(msg), "Config: Created default config.cfg at %s\n", writePath);
  PrintLog(msg);
}

static void LoadConfig()
{
  char configPath[MAX_PATH];
  GetConfigPath(configPath, sizeof(configPath));

  std::ifstream input(configPath, std::ios::binary);
  if (!input) {
    PrintLog("Config: config.cfg not found, using defaults.\n");
    return;
  }

  std::string text;
  size_t nulBytes = 0;
  if (!ReadConfigText(input, text, nulBytes)) {
    PrintLog("Config: unreadable, unsupported encoding, or larger than 1 MiB; using defaults.\n");
    return;
  }
  if (nulBytes) {
    char msg[128];
    sprintf_s(msg, sizeof(msg), "Config: recovered %u NUL padding bytes; please resave config.cfg.\n",
              static_cast<unsigned>(nulBytes));
    PrintLog(msg);
  }

  // Simple line-by-line parser: "key value"
  char* ctx = nullptr;
  char* line = strtok_s(text.data(), "\r\n", &ctx);
  while (line) {
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '\0') {
      line = strtok_s(nullptr, "\r\n", &ctx);
      continue;
    }

    char key[64];
    char keyval[64] = "";
    int tokens = sscanf_s(line, "%63s %63s", key, (unsigned)sizeof(key), keyval, (unsigned)sizeof(keyval));
    if (tokens >= 1) {
      if (tokens < 2) {
        char msg[96];
        sprintf_s(msg, sizeof(msg), "Config: '%s' missing value, ignoring.\n", key);
        PrintLog(msg);
      } else {
      int value = (int)strtol(keyval, nullptr, 10);
      if (_stricmp(key, "fov") == 0) {
        if (value >= kFovMin && value <= kFovMax) {
          OptFov = value;
        } else {
          char msg[128];
          sprintf_s(msg, sizeof(msg), "Config: fov %d out of range [%d..%d], ignoring.\n",
                    value, kFovMin, kFovMax);
          PrintLog(msg);
        }
      }
      else if (_stricmp(key, "object_detail") == 0) {
        if (value >= kObjectDetailMin && value <= kObjectDetailMax) {
          OptObjectDetail = value;
        } else {
          char msg[128];
          sprintf_s(msg, sizeof(msg), "Config: object_detail %d out of range [%d..%d], ignoring.\n",
                    value, kObjectDetailMin, kObjectDetailMax);
          PrintLog(msg);
        }
      }
      else if (_stricmp(key, "fps_limit") == 0) {
        // 0=unlimited, 1=60, 2=120, 3=240
        if (value >= 0 && value <= 3) {
          OptFpsLimit = value;
        }
      }
      else if (_stricmp(key, "verbose_logging") == 0) {
        g_VerboseLogging = (value != 0);
      }
      else if (_stricmp(key, "ai_logging") == 0) {
        g_AILogging = (value != 0);
        char msg[64];
        sprintf_s(msg, sizeof(msg), "Config: ai_logging = %d\n", g_AILogging ? 1 : 0);
        PrintLog(msg);
      }
      else if (_stricmp(key, "nightvision_key") == 0) {
        NightVisionKey = value;
      }
      else if (_stricmp(key, "night_lighting") == 0) {
        // Compatibility with existing configs: there is only one night look.
        PrintLog("Config: night_lighting is obsolete; Moonlit night is always used.\n");
      }
      else if (_stricmp(key, "night_world_brightness") == 0 ||
               _stricmp(key, "night_desaturation") == 0 ||
               _stricmp(key, "night_sky_brightness") == 0 ||
               _stricmp(key, "night_moon_brightness") == 0 ||
               _stricmp(key, "night_moon_glow_strength") == 0) {
        const NightLightingOption option = _stricmp(key, "night_world_brightness") == 0
          ? NightLightingOption::WorldBrightness : _stricmp(key, "night_desaturation") == 0
          ? NightLightingOption::Desaturation : _stricmp(key, "night_sky_brightness") == 0
          ? NightLightingOption::SkyBrightness : _stricmp(key, "night_moon_brightness") == 0
          ? NightLightingOption::MoonBrightness : NightLightingOption::MoonGlowStrength;
        const bool applied = NightLightingConfigValueFits(line, sizeof(keyval)) &&
          ParseNightLightingOption(option, keyval, NightLightingOptions());
        char msg[256];
        sprintf_s(msg, sizeof(msg), applied
          ? "Config: %s = %s (OpenGL night lighting).\n"
          : "Config: invalid %s = %s, ignoring (brightness 5..100, desaturation 0..100, moon 0..1000).\n",
          key, keyval);
        PrintLog(msg);
      }
      else if (_stricmp(key, "flashlight_key") == 0) {
        // Toggle binding for the flashlight accessory. Clamped to the same
        // 0..255 span KeyDownMatches() accepts, so an out-of-range value can
        // never produce a binding no key press can satisfy.
        FlashlightKey = ClampFlashlightKey(value);
      }
      else if (_stricmp(key, "flashlight_inner_angle") == 0 ||
               _stricmp(key, "flashlight_outer_angle") == 0) {
        // Half-angles in degrees. Parsed strictly so "abc" cannot silently
        // mean a one-degree pinhole beam.
        const bool inner = (_stricmp(key, "flashlight_inner_angle") == 0);
        int degrees = inner ? OptFlashlightInnerAngleDeg : OptFlashlightOuterAngleDeg;
        if (ParseConfigInt(keyval,
                           static_cast<int>(kFlashlightAngleDegMin),
                           static_cast<int>(kFlashlightAngleDegMax), degrees)) {
          // Keeps inner < outer: the shader feeds these to smoothstep(),
          // whose edges must stay ordered.
          float innerDeg = inner ? static_cast<float>(degrees)
                                 : static_cast<float>(OptFlashlightInnerAngleDeg);
          float outerDeg = inner ? static_cast<float>(OptFlashlightOuterAngleDeg)
                                 : static_cast<float>(degrees);
          NormalizeFlashlightConeDeg(innerDeg, outerDeg);
          OptFlashlightInnerAngleDeg = static_cast<int>(innerDeg + 0.5f);
          OptFlashlightOuterAngleDeg = static_cast<int>(outerDeg + 0.5f);
          char msg[80];
          sprintf_s(msg, sizeof(msg), "Config: %s = %d deg\n", key, degrees);
          PrintLog(msg);
        } else {
          PrintLog("Config: flashlight inner/outer angle must be 1..90, ignoring.\n");
        }
      }
      else if (_stricmp(key, "flashlight_range") == 0) {
        // Beam reach in world units (256 units = one terrain cell).
        int range = static_cast<int>(kFlashlightRangeDefault);
        if (ParseConfigInt(keyval, static_cast<int>(kFlashlightRangeMin),
                           static_cast<int>(kFlashlightRangeMax), range)) {
          OptFlashlightRange = ClampFlashlightRange(range);
          char msg[72];
          sprintf_s(msg, sizeof(msg), "Config: flashlight_range = %d\n", range);
          PrintLog(msg);
        } else {
          PrintLog("Config: flashlight_range must be 256..60000, ignoring.\n");
        }
      }
      else if (_stricmp(key, "flashlight_intensity") == 0) {
        // Peak beam boost as a percent. 0 disables it entirely, which is the
        // documented way to turn the effect into a pure no-op.
        int percent = kFlashlightIntensityPctDefault;
        if (ParseConfigInt(keyval, kFlashlightIntensityPctMin,
                           kFlashlightIntensityPctMax, percent)) {
          OptFlashlightIntensityPct = ClampFlashlightIntensityPct(percent);
          char msg[80];
          sprintf_s(msg, sizeof(msg), "Config: flashlight_intensity = %d\n", percent);
          PrintLog(msg);
        } else {
          PrintLog("Config: flashlight_intensity must be 0..1000, ignoring.\n");
        }
      }
      else if (_stricmp(key, "terrain_mip_level") == 0) {
        // Highest mip level the GL terrain/water sampler may select from the
        // DataA/B/C/D chain (see Core/TerrainMip.h). 1 = stock D3D/3DFX
        // behaviour (DataA 128 + DataB 64); 3 = whole chain (adds DataC 32
        // and DataD 16). Parsed strictly so "abc" cannot silently mean 0.
        int level = kTerrainMaxMipDefault;
        if (ParseConfigInt(keyval, kTerrainMaxMipMin, kTerrainMaxMipMax, level)) {
          OptTerrainMaxMipLevel = level;
          char msg[72];
          sprintf_s(msg, sizeof(msg), "Config: terrain_mip_level = %d\n", level);
          PrintLog(msg);
        } else {
          PrintLog("Config: terrain_mip_level must be 0..3, ignoring.\n");
        }
      }
      else if (_stricmp(key, "water_mipmap") == 0) {
        // Software renderer only: 1 = water rasterises from the distance
        // mip level (DataA/B/C), 0 = legacy full-resolution water.
        int enabled = 1;
        if (ParseConfigInt(keyval, 0, 1, enabled)) {
          OptWaterMipmap = enabled;
          char msg[64];
          sprintf_s(msg, sizeof(msg), "Config: water_mipmap = %d\n", enabled);
          PrintLog(msg);
        } else {
          PrintLog("Config: water_mipmap must be 0 or 1, ignoring.\n");
        }
      }
      else if (_stricmp(key, "water_wave_fade_rings") == 0) {
        // Number of rings over which the water wave displacement fades to
        // zero before the legacy cutoff (see Core/WaterWaveFade.h). 0 = the
        // legacy hard cutoff; larger = a gradual, seam-free falloff.
        int rings = kWaterWaveFadeDefault;
        if (ParseConfigInt(keyval, kWaterWaveFadeMin, kWaterWaveFadeMax, rings)) {
          OptWaterWaveFadeRings = rings;
          char msg[72];
          sprintf_s(msg, sizeof(msg), "Config: water_wave_fade_rings = %d\n", rings);
          PrintLog(msg);
        } else {
          PrintLog("Config: water_wave_fade_rings must be 0..64, ignoring.\n");
        }
      }
      else if (_stricmp(key, "boundbox_fit") == 0) {
        // Experimental ofBOUND oriented-box fit (see Core/BoundFit.h).
        // 0 = legacy axis-aligned boxes (stock behaviour); 1 = fit 0/45-degree
        // oriented boxes; 2 = min-area rectangle (any baked angle).
        // Acceptance is controlled by boundbox_fit_win below.
        int mode = 0;
        if (ParseConfigInt(keyval, 0, 2, mode)) {
          OptBoundBoxFit = mode;
          char msg[64];
          sprintf_s(msg, sizeof(msg), "Config: boundbox_fit = %d\n", mode);
          PrintLog(msg);
        } else {
          PrintLog("Config: boundbox_fit must be 0..2, ignoring.\n");
        }
      }
      else if (_stricmp(key, "boundbox_fit_win") == 0) {
        // Minimum padded-area win (percent) before a fitted box replaces the
        // legacy axis-aligned one; keeps stock content and organic parts as-is.
        int win = kBoundBoxFitWinDefault;
        if (ParseConfigInt(keyval, 0, 100, win)) {
          OptBoundBoxFitWin = win;
          char msg[64];
          sprintf_s(msg, sizeof(msg), "Config: boundbox_fit_win = %d\n", win);
          PrintLog(msg);
        } else {
          PrintLog("Config: boundbox_fit_win must be 0..100, ignoring.\n");
        }
      }
      else if (_stricmp(key, "gpufeatures") == 0) {
        // Runtime GPU-optimization kill-switch bitmask (see GpuFeature in GameState.h).
        // 0 disables all new GPU optimizations; bits toggle features individually.
        // Parse as unsigned: the default (all bits set) exceeds INT_MAX, so
        // atoi/strtol would clamp and could never restore the default.
        char *end = nullptr;
        unsigned long v = strtoul(keyval, &end, 10);
        if (end != keyval) g_gpuFeatures = static_cast<uint32_t>(v);
      }
      else if (_stricmp(key, "glperf_logging") == 0) {
        // Runtime toggle for GL performance harness logging.
        // Only effective when GL_PERF_HOOKS is compiled in.
        // 0=disabled (default), 1=enabled
        g_glperfLoggingEnabled = (value != 0);
        {
          char msg[64];
          sprintf_s(msg, sizeof(msg), "Config: glperf_logging = %d\n", g_glperfLoggingEnabled ? 1 : 0);
          PrintLog(msg);
        }
      }
      else if (_stricmp(key, "sky_horizon_drop") == 0) {
        // Mode 1 horizon drop in degrees (see GameState.h).
        float f = 0.0f;
        if (ParseConfigFloat(keyval, kSkyHorizonDropMin, kSkyHorizonDropMax, f)) {
          OptSkyHorizonDrop = f;
          char msg[80];
          sprintf_s(msg, sizeof(msg), "Config: sky_horizon_drop = %.1f deg\n", (double)f);
          PrintLog(msg);
        } else {
          PrintLog("Config: sky_horizon_drop expects 0..30 degrees, ignoring.\n");
        }
      }
      else if (_stricmp(key, "sky_dome_scale") == 0) {
        // Dome canopy scale (sky_mode 2): texture texels per radian at the
        // horizon. Float; see GameState.h for the valid range.
        float f = 0.0f;
        if (ParseConfigFloat(keyval, kSkyDomeScaleMin, kSkyDomeScaleMax, f)) {
          OptSkyDomeScale = f;
          char msg[80];
          sprintf_s(msg, sizeof(msg), "Config: sky_dome_scale = %.1f\n", (double)f);
          PrintLog(msg);
#ifdef _soft
          // Only the OpenGL dome mode consumes this. The software renderer
          // has no dome, so the value parses and validates but is inert --
          // say so instead of logging it as if it were effective.
          PrintLog("Config: sky_dome_scale has no effect in the software renderer (dome mode is OpenGL-only).\n");
#endif
        } else {
          PrintLog("Config: sky_dome_scale expects 32..4096, ignoring.\n");
        }
      }
      else if (_stricmp(key, "sky_mode") == 0) {
        // Sky cloud-texture placement (see SkyMappingMode in GameState.h).
        // 0=legacy offset, 1=world-level plane (default), 2=dome. Parsed
        // strictly: "abc" must not silently mean 0 (legacy).
        int mode = 0;
        if (ParseConfigInt(keyval, kSkyModeLegacy, kSkyModeCount - 1, mode)) {
          OptSkyMode = mode;
          char msg[64];
          sprintf_s(msg, sizeof(msg), "Config: sky_mode = %d\n", OptSkyMode);
          PrintLog(msg);
#ifdef _soft
          // sky_mode configures the OpenGL renderer. The software renderer
          // reads sky_mode_soft, so this must not be left looking effective
          // here while changing nothing.
          PrintLog("Config: sky_mode applies to the OpenGL renderer; the software renderer uses sky_mode_soft.\n");
#endif
        } else {
          PrintLog("Config: sky_mode must be 0 (legacy), 1 (level) or 2 (dome), ignoring.\n");
        }
      }
      else if (_stricmp(key, "sky_mode_soft") == 0) {
        // Software renderer only (see OptSkyModeSoft). Same strict parsing
        // and range as sky_mode, but an independent value: both renderers
        // share one config.cfg and want different defaults.
        int mode = 0;
        if (ParseConfigInt(keyval, kSkyModeLegacy, kSkyModeCount - 1, mode)) {
#ifdef _soft
          OptSkyModeSoft = mode;
          char msg[64];
          sprintf_s(msg, sizeof(msg), "Config: sky_mode_soft = %d\n", OptSkyModeSoft);
          PrintLog(msg);
          if (OptSkyModeSoft == kSkyModeDome) {
            OptSkyModeSoft = kSkyModeLegacy;
            PrintLog("Config: sky_mode_soft 2 (dome) is OpenGL-only; using the legacy plane.\n");
          }
#else
          PrintLog("Config: sky_mode_soft applies to the software renderer; ignored by OpenGL.\n");
#endif
        } else {
          PrintLog("Config: sky_mode_soft must be 0 (legacy) or 1 (level), ignoring.\n");
        }
      }
      else if (_stricmp(key, "resolution") == 0) {
        // Override the saved profile resolution. Format: WxH, e.g. "1920x1080".
        // Applies after SetupRes() (trophy file) so config.cfg wins over the
        // per-profile legacy setting; a command-line -res= still overrides this.
        int w = 0, h = 0;
        char sep = 0;
        if (sscanf_s(keyval, "%d%c%d", &w, &sep, (unsigned)sizeof(sep), &h) == 3 &&
            (sep == 'x' || sep == 'X')) {
          if (w > 0 && h > 0) {
            // Sync OptRes when the size exists in the enumerated list so the
            // legacy index stays meaningful; WinW/H always win regardless.
            // Reset first: an exotic size must not leave a stale profile index.
            OptRes = -1;
            for (int r = 0; r < ResCount; r++) {
              if (ResolutionList[r].w == w && ResolutionList[r].h == h) { OptRes = r; break; }
            }
            WinW = w;
            WinH = h;
          } else {
            PrintLog("Config: resolution values must be positive, ignoring.\n");
          }
        } else {
          PrintLog("Config: resolution expects WxH (e.g. 1920x1080), ignoring.\n");
        }
      }
      else if (_stricmp(key, "display_mode") == 0) {
        // Display mode: 0=windowed, 1=exclusive fullscreen, 2=borderless.
        // Written by the menu's Display Mode video option. Command-line
        // flags (-windowed/-fullscreen/-borderless) override this later
        // in ProcessCommandLine().
        if (value >= 0 && value <= 2) {
#ifdef _soft
          // The software renderer has no borderless presentation; map its
          // "borderless" pick to exclusive fullscreen (classic behaviour).
          if (value == 2) value = 1;
#endif
          FULLSCREEN = (value == 1);
          BORDERLESS = (value == 2);
        } else {
          PrintLog("Config: display_mode must be 0 (windowed), 1 (fullscreen) or 2 (borderless), ignoring.\n");
        }
      }
      else if (_strnicmp(key, "env", 3) == 0) {
        // Runtime reverb preset tuning: env<0-8>_<decay|decayhf|diffusion|reverb>.
        // Floats (e.g. env0_decay 1.49). Unset fields read compiled defaults;
        // out-of-range values are rejected with a log line. room/envID have
        // no keys -- the EFX path does not consume them.
        // Require one preset digit and consume the entire numeric token.
        // Parsing after the case-insensitive prefix also permits ENV0_decay.
        const int env = key[3] - '0';
        const char* field = strlen(key) >= 5 ? key + 5 : "";
        char* end = nullptr;
        errno = 0;
        const float f = std::strtof(keyval, &end);
        if (env >= 0 && env <= 8 && key[4] == '_' &&
            end != keyval && *end == '\0' && errno != ERANGE) {
          int fi = -1;
          if (_stricmp(field, "decay") == 0) fi = 0;
          else if (_stricmp(field, "decayhf") == 0) fi = 1;
          else if (_stricmp(field, "diffusion") == 0) fi = 2;
          else if (_stricmp(field, "reverb") == 0) fi = 3;
          if (fi >= 0 && Audio_SetEnvParam(env, fi, f)) {
            char msg[96];
            sprintf_s(msg, sizeof(msg), "Config: env%d_%s = %.3g\n", env, field, (double)f);
            PrintLog(msg);
          } else {
            // Two tokens of up to 63 chars plus the fixed diagnostic.
            char msg[256];
            sprintf_s(msg, sizeof(msg), "Config: '%s %s' invalid (env 0-8, known field, range), ignoring.\n", key, keyval);
            PrintLog(msg);
          }
        } else {
          char msg[256];
          sprintf_s(msg, sizeof(msg), "Config: '%s' expects env<0-8>_<decay|decayhf|diffusion|reverb> value, ignoring.\n", key);
          PrintLog(msg);
        }
      }
      // Future settings: add else-if branches here
      } // tokens == 2
    }

    line = strtok_s(nullptr, "\r\n", &ctx);
  }

  PrintLog("Config Loaded (config.cfg).\n");
}
