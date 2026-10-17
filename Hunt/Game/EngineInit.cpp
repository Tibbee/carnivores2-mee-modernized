// EngineInit.cpp -- engine bootstrap (InitEngine/ShutDownEngine + helpers)
// ==========================================================================
// config.cfg creation/loading lives in ConfigFile.cpp (split 2026-10-17).
// ==========================================================================

#include "Hunt.h"
#include <mmsystem.h>
#include "ConfigFile.h"
#include "Core/ConfigPath.h"
#include "Core/ResolutionList.h"
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
void EnumerateResolutions()
{
  resolutions::Enumerate(ResolutionList, ResCount);
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
              ANSI_CHARSET,
              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);




  fnt_Small = CreateFont(
                static_cast<int>((16 * UIScale)), static_cast<int>((7 * UIScale)), 0, 0,
				100, 0,0,0,
	  
	  //14, 5, 0, 0,
	  //100, 0, 0, 0,
                ANSI_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, nullptr);


  fnt_Midd  = CreateFont(
			    static_cast<int>((16 * UIScale)), static_cast<int>((7 * UIScale)), 0, 0,
	            550, 0, 0, 0,
                ANSI_CHARSET,
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

  // The load policy must be known before InitGameInfo() parses _RES.TXT; the
  // full LoadConfig() runs later in InitEngine. C2_STRICT_DATA overrides the
  // config for CI/mod authoring.
  ApplyLoadPolicyFromConfigFile(configpath::ResolveReadPath());
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
  OptBoundBoxFit = 0;                           // config.cfg boundbox_fit (1 = experimental fit)
  OptBoundBoxFitWin = kBoundBoxFitWinDefault;   // config.cfg boundbox_fit_win
  OptBoundBoxSlope = 0;                         // config.cfg boundbox_slope (1 = sloped surfaces)

  OptViewR = kViewOptDefault;
  OptObjectDetail = kObjectDetailDefault;
OptFpsLimit = kFpsLimitDefault;  // 0 remains available for unlimited

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
    char msg[192];
    sprintf_s(msg, sizeof(msg), "Config effective: fps_limit=%d (%d FPS; 0=unlimited), fov=%d, object_detail=%d\n",
              OptFpsLimit, FpsLimitTarget(OptFpsLimit), OptFov, OptObjectDetail);
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
