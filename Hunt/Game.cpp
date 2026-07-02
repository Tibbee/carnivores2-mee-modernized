#include "Hunt.h"


#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <timeapi.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "1986"




/*typedef struct tagAudioQuad
{
  float x1,y1,z1;
  float x2,y2,z2;
  float x3,y3,z3;
  float x4,y4,z4;
} AudioQuad;

AudioQuad data[8192];

  HMap[1024][1024];
*/


bool ShowFaces = true;

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
    // Prefer the DEVMODE values — they can be slightly different from
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
  if (ObservMode || g_GameMode == GameMode::TrophyMode) return;
  if (g_GameMode == GameMode::SurvivalMode) return;

  if (wptr->state == 0)
  {  
	//if (!ShotsLeft[CurrentWeapon]) return;
    if (WeapInfo[CurrentWeapon].Optic) g_GameMode = GameMode::OpticScope;
    
	if (IsUnderwater()) {
		if (WeapInfo[CurrentWeapon].getAqSnd >= 0)
			AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].getAqSnd].length,
				wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].getAqSnd].lpData.data(), 256);
	} else {
		int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].getAnim];
		if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
			wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
	}
    wptr->FTime = 0;
    wptr->state = 1;
    g_GameMode = GameMode::Normal;
    g_GameMode = GameMode::Normal;
    wptr->shakel = WeapInfo[CurrentWeapon].shake * 4.f;
	wptr->breath = 0.f;
	wptr->breathPressed = 0;
	wptr->HoldBreath = false;
    return;
  }

  if (wptr->state!=2 || wptr->FTime!=0) return;
  if (IsUnderwater()) {
	  if (WeapInfo[CurrentWeapon].putAqSnd >= 0)
		  AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].putAqSnd].length,
			  wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].putAqSnd].lpData.data(), 256);
  } else {
	  int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].putAnim];
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
  /*
      WeapInfo[0].Name = "Shotgun";
  	WeapInfo[0].Power = 1.5f;
  	WeapInfo[0].Prec  = 1.1f;
  	WeapInfo[0].Loud  = 0.3f;
  	WeapInfo[0].Rate  = 1.6f;
  	WeapInfo[0].Shots = 6;

  	WeapInfo[1].Name = "X-Bow";
  	WeapInfo[1].Power = 1.1f;
  	WeapInfo[1].Prec  = 0.7f;
  	WeapInfo[1].Loud  = 1.9f;
  	WeapInfo[1].Rate  = 1.2f;
  	WeapInfo[1].Shots = 8;

      WeapInfo[2].Name = "Sniper Rifle";
  	WeapInfo[2].Power = 1.0f;
  	WeapInfo[2].Prec  = 1.8f;
  	WeapInfo[2].Loud  = 0.6f;
  	WeapInfo[2].Rate  = 1.0f;
  	WeapInfo[2].Shots = 6;




  	DinoInfo[ 0].Name = "Moschops";
  	DinoInfo[ 0].Health0 = 2;
  	DinoInfo[ 0].Mass = 0.15f;

      DinoInfo[ 1].Name = "Galimimus";
  	DinoInfo[ 1].Health0 = 2;
  	DinoInfo[ 1].Mass = 0.1f;

  	DinoInfo[ 2].Name = "Dimorphodon";
      DinoInfo[ 2].Health0 = 1;
  	DinoInfo[ 2].Mass = 0.05f;

  	DinoInfo[ 3].Name = "Dimetrodon";
      DinoInfo[ 3].Health0 = 2;
  	DinoInfo[ 3].Mass = 0.22f;


  	DinoInfo[ 5].Name = "Parasaurolophus";
  	DinoInfo[ 5].Mass = 1.5f;
  	DinoInfo[ 5].Length = 5.8f;
  	DinoInfo[ 5].Radius = 320.f;
  	DinoInfo[ 5].Health0 = 5;
  	DinoInfo[ 5].BaseScore = 6;
  	DinoInfo[ 5].SmellK = 0.8f; DinoInfo[ 4].HearK = 1.f; DinoInfo[ 4].LookK = 0.4f;
  	DinoInfo[ 5].ShDelta = 48;

  	DinoInfo[ 6].Name = "Pachycephalosaurus";
  	DinoInfo[ 6].Mass = 0.8f;
  	DinoInfo[ 6].Length = 4.5f;
  	DinoInfo[ 6].Radius = 280.f;
  	DinoInfo[ 6].Health0 = 4;
  	DinoInfo[ 6].BaseScore = 8;
  	DinoInfo[ 6].SmellK = 0.4f; DinoInfo[ 5].HearK = 0.8f; DinoInfo[ 5].LookK = 0.6f;
  	DinoInfo[ 6].ShDelta = 36;

  	DinoInfo[ 7].Name = "Stegosaurus";
      DinoInfo[ 7].Mass = 7.f;
  	DinoInfo[ 7].Length = 7.f;
  	DinoInfo[ 7].Radius = 480.f;
  	DinoInfo[ 7].Health0 = 5;
  	DinoInfo[ 7].BaseScore = 7;
  	DinoInfo[ 7].SmellK = 0.4f; DinoInfo[ 6].HearK = 0.8f; DinoInfo[ 6].LookK = 0.6f;
  	DinoInfo[ 7].ShDelta = 128;

  	DinoInfo[ 8].Name = "Allosaurus";
  	DinoInfo[ 8].Mass = 0.5;
  	DinoInfo[ 8].Length = 4.2f;
  	DinoInfo[ 8].Radius = 256.f;
  	DinoInfo[ 8].Health0 = 3;
  	DinoInfo[ 8].BaseScore = 12;
  	DinoInfo[ 8].Scale0 = 1000;
  	DinoInfo[ 8].ScaleA = 600;
  	DinoInfo[ 8].SmellK = 1.0f; DinoInfo[ 7].HearK = 0.3f; DinoInfo[ 7].LookK = 0.5f;
  	DinoInfo[ 8].ShDelta = 32;
	DinoInfo[ 8].DangerCall = true;

  	DinoInfo[ 9].Name = "Chasmosaurus";
  	DinoInfo[ 9].Mass = 3.f;
  	DinoInfo[ 9].Length = 5.0f;
  	DinoInfo[ 9].Radius = 400.f;
  	DinoInfo[ 9].Health0 = 8;
  	DinoInfo[ 9].BaseScore = 9;
  	DinoInfo[ 9].SmellK = 0.6f; DinoInfo[ 8].HearK = 0.5f; DinoInfo[ 8].LookK = 0.4f;
  	//DinoInfo[ 8].ShDelta = 148;
  	DinoInfo[ 9].ShDelta = 108;

  	DinoInfo[10].Name = "Velociraptor";
  	DinoInfo[10].Mass = 0.3f;
  	DinoInfo[10].Length = 4.0f;
  	DinoInfo[10].Radius = 256.f;
  	DinoInfo[10].Health0 = 3;
  	DinoInfo[10].BaseScore = 16;
  	DinoInfo[10].ScaleA = 400;
  	DinoInfo[10].SmellK = 1.0f; DinoInfo[ 9].HearK = 0.5f; DinoInfo[ 9].LookK = 0.4f;
  	DinoInfo[10].ShDelta =-24;
	DinoInfo[10].DangerCall = true;

  	DinoInfo[11].Name = "T-Rex";
      DinoInfo[11].Mass = 6.f;
  	DinoInfo[11].Length = 12.f;
  	DinoInfo[11].Radius = 400.f;
  	DinoInfo[11].Health0 = 1024;
  	DinoInfo[11].BaseScore = 20;
  	DinoInfo[11].SmellK = 0.85f; DinoInfo[10].HearK = 0.8f; DinoInfo[10].LookK = 0.8f;
  	DinoInfo[11].ShDelta = 168;
	DinoInfo[11].DangerCall = true;

  	DinoInfo[ 4].Name = "Brahiosaurus";
      DinoInfo[ 4].Mass = 9.f;
  	DinoInfo[ 4].Length = 12.f;
  	DinoInfo[ 4].Radius = 400.f;
  	DinoInfo[ 4].Health0 = 1024;
  	DinoInfo[ 4].BaseScore = 0;
  	DinoInfo[ 4].SmellK = 0.85f; DinoInfo[16].HearK = 0.8f; DinoInfo[16].LookK = 0.8f;
  	DinoInfo[ 4].ShDelta = 168;
	DinoInfo[ 4].DangerCall = false;
  */
  LoadResourcesScript();
}


// MULTIPLAYER ===================================================




















static void LoadConfig();

void InitEngine()
{
  FULLSCREEN   = true;
  BORDERLESS   = false;
  DEBUG        = false;

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
    MessageBox(hwndMain,"Error creating heap.","Error",IDOK);
    return;
  }

  // Phase 5C.2: Construct the per-level MemoryArena. C1 does this at
  // Carnivores1/Hunt/Game.cpp:506 with 128 MiB; we use LEVEL_ARENA_SIZE
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
  OptViewR = kViewOptDefault;
  OptObjectDetail = kObjectDetailDefault;
OptFpsLimit = 0;  // 0 = unlimited

  LoadTrophy();
  OptViewR = ClampViewOpt(OptViewR);

  // Override settings from config.cfg (written by Carnivores2Menu).
  // This file is the single source of truth for settings that are not
  // part of the legacy binary trophy format (e.g. OptFov).
  LoadConfig();

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
  // (Carnivores1/Hunt/Game.cpp:660-670); C2 ME was missing them, so every
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
  // run. C1 has the same order (Carnivores1/Hunt/Game.cpp:669-670).
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
  if (ObservMode || g_GameMode == GameMode::TrophyMode) return;
  if (CallLockTime) return;

  CallLockTime=1024*3;

  NextCall+=(RealTime % 2)+1;
  NextCall%=3;

  AddVoicev(fxCall[TargetCall-10][NextCall].length,
            fxCall[TargetCall-10][NextCall].lpData.data(), 256);

  //multiplayer
  sendHunterCall = TargetCall - 10;
  sendHunterCallType = NextCall;

  float dminSq = (512 * 256) * (512 * 256);
  int ai = -1;

  for (int c=0; c<ChCount; c++)
  {
    TCharacter *cptr = &Characters[c];

	float dx = PlayerX - cptr->pos.x;
	float dy = PlayerY - cptr->pos.y;
	float dz = PlayerZ - cptr->pos.z;
	float dSq = dx * dx + dy * dy + dz * dz;
	float hearRange = (ctViewR * 400) * (DinoInfo[cptr->CType].HearK * 2);
	bool canHear = dSq < hearRange * hearRange;

	if (DinoInfo[cptr->CType].fearCall[TargetCall-10] && canHear
		&& DinoInfo[cptr->CType].Clone != AI_DIMOR && DinoInfo[cptr->CType].Clone != AI_PTERA
		&& DinoInfo[cptr->CType].Clone != AI_BRACH
		) { //ai that cannot flee, state always 0
		cptr->State = 2;
		cptr->AfraidTime = (10 + rRand(5)) * 1024;
	}

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



DWORD ColorSum(DWORD C1, DWORD C2)
{
  DWORD R,G,B;
  R = MIN(255, ((C1>> 0) & 0xFF) + ((C2>> 0) & 0xFF));
  G = MIN(255, ((C1>> 8) & 0xFF) + ((C2>> 8) & 0xFF));
  B = MIN(255, ((C1>>16) & 0xFF) + ((C2>>16) & 0xFF));
  return R + (G<<8) + (B<<16);
}


#define partBlood   1
#define partWater   2
#define partGround  3
#define partBubble  4


void AddElements(float x, float y, float z, int etype, int cnt)
{
	AddElementsA(x, y, z, etype, cnt, cnt, false, 0);
}

void AddElementsA(float x, float y, float z, int etype, int cnt, int mag, bool angled, float alph)
{
  if (ElCount > 697)
  {
    memcpy(&Elements[0], &Elements[1], (ElCount-1) * sizeof(TElements));
    ElCount--;
  }

  Elements[ElCount].EDone  = 0;
  Elements[ElCount].Type = etype;
  Elements[ElCount].ECount = MIN(30, cnt);
  int c;

  switch (etype)
  {
  case partBlood:
#ifdef _d3d
    //Elements[ElCount].RGBA = 0xE0600000;
    //Elements[ElCount].RGBA2= 0x20300000;
	  Elements[ElCount].RGBA = 0xE0000000 +
		  (DinoInfo[Characters[ShotDino].CType].bloodRed << 16) +
		  (DinoInfo[Characters[ShotDino].CType].bloodGreen << 8) +
		  DinoInfo[Characters[ShotDino].CType].bloodBlue;
	  Elements[ElCount].RGBA2 = 0x20000000 +
		  (DinoInfo[Characters[ShotDino].CType].bloodRed / 2 << 16) +
		  (DinoInfo[Characters[ShotDino].CType].bloodGreen / 2 << 8) +
		  DinoInfo[Characters[ShotDino].CType].bloodBlue / 2;
#else
  //Elements[ElCount].RGBA = 0xE0000060;
  //Elements[ElCount].RGBA2= 0x20000030;
	Elements[ElCount].RGBA = 0xE0000000 +
		(DinoInfo[Characters[ShotDino].CType].bloodBlue << 16) +
		(DinoInfo[Characters[ShotDino].CType].bloodGreen << 8) +
		DinoInfo[Characters[ShotDino].CType].bloodRed;
	Elements[ElCount].RGBA2= 0x20000000 +
		(DinoInfo[Characters[ShotDino].CType].bloodBlue/2 << 16) +
		(DinoInfo[Characters[ShotDino].CType].bloodGreen/2 << 8) +
		DinoInfo[Characters[ShotDino].CType].bloodRed/2;
#endif
    break;

  case partGround:
#ifdef _d3d
    Elements[ElCount].RGBA = 0xF0F09E55;
    Elements[ElCount].RGBA2= 0x10F09E55;
#else
    Elements[ElCount].RGBA = 0xF0559EF0;
    Elements[ElCount].RGBA2= 0x10559EF0;
#endif
    break;


  case partBubble:
    c = WaterList[ WMap[ static_cast<int>(z) / 256][ static_cast<int>(x) / 256] ].fogRGB;
#ifdef _d3d
    c = ColorSum( ((c & 0xFEFEFE)>>1), 0x152020);
#else
    c = ColorSum( ((c & 0xFEFEFE)>>1), 0x202015);
#endif
    Elements[ElCount].RGBA = 0x70000000 + (ColorSum(c, ColorSum(c,c)));
    Elements[ElCount].RGBA2= 0x40000000 + (ColorSum(c, c));
    break;

  case partWater:
    c = WaterList[ WMap[ static_cast<int>(z) / 256][ static_cast<int>(x) / 256] ].fogRGB;
#ifdef _d3d
    c = ColorSum( ((c & 0xFEFEFE)>>1), 0x152020);
#else
    c = ColorSum( ((c & 0xFEFEFE)>>1), 0x202015);
#endif
    Elements[ElCount].RGBA  = 0xB0000000 + ( ColorSum(c, ColorSum(c,c)) );
    Elements[ElCount].RGBA2 = 0x40000000 + (c);
    break;
  }

  Elements[ElCount].RGBA  = conv_xGx(Elements[ElCount].RGBA);
  Elements[ElCount].RGBA2 = conv_xGx(Elements[ElCount].RGBA2);

  float al = siRand(128) / 128.f * pi / 4.f;
  float ss = sin(al);
  float cc = cos(al);

  for (int e=0; e<Elements[ElCount].ECount; e++)
  {
    Elements[ElCount].EList[e].pos.x = x;
    Elements[ElCount].EList[e].pos.y = y;
    Elements[ElCount].EList[e].pos.z = z;
    Elements[ElCount].EList[e].R = 6 + rRand(5);
    Elements[ElCount].EList[e].Flags = 0;
    float v;

	float velo = mag * rRand(20)/20;

    switch (etype)
    {
    case partBlood:
      v = velo * 6 + rRand(96) + 220;
      Elements[ElCount].EList[e].speed.x =ss*ca*v + siRand(32);
      Elements[ElCount].EList[e].speed.y =cc * (v * 3);
      Elements[ElCount].EList[e].speed.z =ss*sa*v + siRand(32);
      break;
    case partGround:
      Elements[ElCount].EList[e].speed.x =siRand(52)-sa*64;
      Elements[ElCount].EList[e].speed.y =rRand(100) + 600 + velo * 20;
      Elements[ElCount].EList[e].speed.z =siRand(52)+ca*64;
      break;
    case partWater:
		Elements[ElCount].EList[e].speed.x = siRand(32);
		Elements[ElCount].EList[e].speed.z = siRand(32);
		Elements[ElCount].EList[e].speed.y =rRand(80) + 400 + velo * 40;
		if (angled) {
			Elements[ElCount].EList[e].speed.x = siRand(132) + (static_cast<float>(cos(alph)) * velo * 40);
			Elements[ElCount].EList[e].speed.z = siRand(132) + (static_cast<float>(sin(alph)) * velo * 40);
		}
      break;
    case partBubble:
      Elements[ElCount].EList[e].speed.x =siRand(40);
      Elements[ElCount].EList[e].speed.y =rRand(140) + 20;
      Elements[ElCount].EList[e].speed.z =siRand(40);
      break;
    }
  }

  ElCount++;
}



int AnimateBullet(float ax, float ay, float az,
              float bx, float by, float bz, int b)
{
  int sres;
    sres = TraceShot(ax, ay, az, bx, by, bz, bullet[b].Danger, bullet[b].cDanger);

//ENDTRACE:

	bool poon = false;
	if (WeapInfo[bullet[b].parent].harpoon &&
		GetLandUpH(bx, bz) > GetLandH(bx, bz) &&
		GetLandUpH(bx, bz) > by) {
		poon = true;
		if (!bullet[b].state) AddElements(bx, by, bz, partBubble, 1);
	}
  if (sres==-1) return sres;

  int mort = (sres & 0xFF00) && (Characters[ShotDino].Health);
  sres &= 0xFF;

  int powerL = WeapInfo[CurrentWeapon].Power;
  if (poon) powerL = WeapInfo[CurrentWeapon].PowerAq;
  if (powerL > 100) powerL = 100;
	  
	  //underwater model/ground impact sounds?

	  //if (sres != tresChar) return sres;
	  // add in underwater body impact sounds
	  //if (!Characters[ShotDino].Health) return sres;
  
	  if (sres == tresGround) {
		  if (!poon) AddElements(bx, by, bz, partGround, 6 + powerL * 4);
		  int sNo = rRand(2);
		  if (!IsUnderwater() && !poon) AddVoice3dv(fxImpactGround[sNo].length, fxImpactGround[sNo].lpData.data(), bx, by, bz, 256);
		  if (IsUnderwater() && poon) AddVoice3dv(fxImpactAquatic[sNo].length, fxImpactAquatic[sNo].lpData.data(), bx, by, bz, 256);
	  }
	  if (sres == tresModel) {
		  if (!poon) AddElements(bx, by, bz, partGround, 6 + powerL * 4);
		  int sNo = rRand(2);
		  if (!IsUnderwater() && !poon) AddVoice3dv(fxImpactModel[sNo].length, fxImpactModel[sNo].lpData.data(), bx, by, bz, 256);
		  if (IsUnderwater() && poon) AddVoice3dv(fxImpactAquatic[sNo].length, fxImpactAquatic[sNo].lpData.data(), bx, by, bz, 256); //change this to aquatic sound
	  }

	  if (sres == tresWater)
	  {
		  AddElements(bx, by, bz, partWater, 4 + powerL * 3);
		  //AddElements(bx, GetLandH(bx, bz), bz, partBubble);
		  //AddWCircle(bx, bz, 1.2);
		  AddWCircle(bx, bz, 1.2);
		  int sNo = rRand(2);
		  AddVoice3dv(fxImpactWater[sNo].length, fxImpactWater[sNo].lpData.data(), bx, by, bz, 256);
	  }
	  


	  if (sres != tresChar && sres != tresHunter) return sres;
	  if (!poon) AddElements(bx, by, bz, partBlood, 4 + powerL * 4);
	  int sNo = rRand(2);
	  if (!IsUnderwater() && !poon) AddVoice3dv(fxImpactChar[sNo].length, fxImpactChar[sNo].lpData.data(), bx, by, bz, 256);
	  if (IsUnderwater() && poon) AddVoice3dv(fxImpactAquatic[sNo].length, fxImpactAquatic[sNo].lpData.data(), bx, by, bz, 256); //change this to aquatic sound

	  if (sres == tresHunter) {
		AddDeadBody(nullptr, HUNT_EAT, true);
		Characters[ChCount - 1].alpha = PlayerAlpha - pi / 2;
		return sres;
	  } else if (!Characters[ShotDino].Health) return sres;

//======= character damage =========//

  if (WeapInfo[bullet[b].parent].onRadar) {
	  Characters[ShotDino].tracker = bullet[b].parent;
	  Characters[ShotDino].RTime = bullet[b].RTime;
  }

  if (Multiplayer && !Host) {
	  if (mort && !WeapInfo[bullet[b].parent].cannotMortal) sendDamage[ShotDino] = Characters[ShotDino].Health;
	  else {
		  if (poon) sendDamage[ShotDino] += WeapInfo[CurrentWeapon].PowerAq;
		  else sendDamage[ShotDino] += WeapInfo[CurrentWeapon].Power;
	  }
  } else {
	  if (mort && !WeapInfo[bullet[b].parent].cannotMortal) Characters[ShotDino].Health = 0;
	  else {
		  if (poon) Characters[ShotDino].Health -= WeapInfo[CurrentWeapon].PowerAq;
		  else Characters[ShotDino].Health -= WeapInfo[CurrentWeapon].Power;
	  }
	  if (Characters[ShotDino].Health < 0) Characters[ShotDino].Health = 0;
	  registerDamage(ShotDino, bullet[b].enemy);
  }
  
  return sres;
}




void AddBullet(float ax, float ay, float az,
	float Dx, float Dy, float Dz,
	float Dlx, float Dly, float Dlz,
	int parent, bool enemy)
{
	bullet[bulletCh].a.x = ax;
	bullet[bulletCh].a.y = ay;
	bullet[bulletCh].a.z = az;
	bullet[bulletCh].orig.x = ax;
	bullet[bulletCh].orig.y = ay;
	bullet[bulletCh].orig.z = az;
	bullet[bulletCh].dif.x = Dx;
	bullet[bulletCh].dif.y = Dy;
	bullet[bulletCh].dif.z = Dz;
	bullet[bulletCh].ldif.x = Dlx;
	bullet[bulletCh].ldif.y = Dly;
	bullet[bulletCh].ldif.z = Dlz;
	bullet[bulletCh].parent = parent;
	bullet[bulletCh].fallTotal = 0;
	bullet[bulletCh].state = 0;
	bullet[bulletCh].Danger = enemy;
	bullet[bulletCh].cDanger = !enemy;
	bullet[bulletCh].enemy = enemy;
	bullet[bulletCh].alpha = FindVectorAlpha(Dx, Dz);
	bullet[bulletCh].beta = FindVectorAlpha(sqrt(Dz*Dz + Dx*Dx), Dy);
	if (!enemy){
		if (WeapInfo[parent].onRadar) bullet[bulletCh].RTime = 1;
		if (WeapInfo[parent].radarTime) bullet[bulletCh].RTime = WeapInfo[parent].radarTime;
	}
	if (IsUnderwater()) bullet[bulletCh].aqState = 1;
	else bullet[bulletCh].aqState = 0;
	bulletCh++;
}

void AnimateBullets() {
	for (int b=0; b < bulletCh; b++) {

		if (bullet[b].RTime) {
			if (WeapInfo[bullet[b].parent].radarTime) bullet[b].RTime -= TimeDt;
			if (bullet[b].RTime < 0) bullet[b].RTime = 0;
		}

		if (bullet[b].state) {
			bullet[b].Danger = false;
			bullet[b].cDanger = false;
			if (VectorLength(SubVectors(PlayerPos, bullet[b].a)) < 300.f) {

				int maxAm = WeapInfo[bullet[b].parent].Shots;
				if (DoubleAmmo && (WeapInfo[bullet[b].parent].Reload || WeapInfo[bullet[b].parent].rldAnim < 0)) maxAm *= 2;
				if (ShotsLeft[bullet[b].parent] < maxAm) {
					int collectNo = rRand(2);
					AddVoicev(fxCollect[collectNo].length, fxCollect[collectNo].lpData.data(), 256);
					if (!Chambered[bullet[b].parent] &&
						((WeapInfo[bullet[b].parent].pmpAnim < 0 && !WeapInfo[bullet[b].parent].Reload) ||
						(WeapInfo[bullet[b].parent].rldAnim < 0 && WeapInfo[bullet[b].parent].Reload)))
						Chambered[bullet[b].parent]++;
					else ShotsLeft[bullet[b].parent]++;
					memcpy(&bullet[b], &bullet[b + 1], (bulletCh + 1 - b) * sizeof(TBullet));
					b--;
					bulletCh--;
				}
			}
		} else {

			if (!bullet[b].Danger)
				if (VectorLength(SubVectors(bullet[b].a, bullet[b].orig)) > 128.f)
					bullet[b].Danger = true;
			
			if (!bullet[b].cDanger)
				if (VectorLength(SubVectors(bullet[b].a, bullet[b].orig)) > 128.f)
					bullet[b].cDanger = true;

			Vector3d d = bullet[b].dif;
			bool poon = false;
			if (WeapInfo[bullet[b].parent].harpoon &&
				GetLandUpH(bullet[b].a.x, bullet[b].a.z) > GetLandH(bullet[b].a.x, bullet[b].a.z) &&
				GetLandUpH(bullet[b].a.x, bullet[b].a.z) > bullet[b].a.y)
				poon = true;

			if (bullet[b].aqState<2)
				if ((poon && bullet[b].aqState == 0) ||
					(!poon && bullet[b].aqState == 1)) {
					bullet[b].aqState = 2;
					bullet[b].dif = bullet[b].ldif;
					bullet[b].dif.y -= bullet[b].fallTotal;
					d = bullet[b].dif;
				}

			/*
			if (bullet[b].submerged) {

			//	d.x /= 2;
			//	d.z /= 2;
			//	d.y /= 2;
			}
   		    */

			int sres = AnimateBullet(
				bullet[b].a.x,
				bullet[b].a.y,
				bullet[b].a.z,
				bullet[b].a.x + d.x,
				bullet[b].a.y + d.y,
				bullet[b].a.z + d.z,
				b);

			//this ought to be adjusted on frame time

			float pdx = PlayerX - bullet[b].a.x;
			float pdz = PlayerZ - bullet[b].a.z;
			float pd = pdx * pdx + pdz * pdz;

			if ((sres > 0 && (!WeapInfo[bullet[b].parent].harpoon || sres!=tresWater)) || 
				pd > (256 * ctViewR) * (256 * ctViewR)) {
				if (WeapInfo[bullet[b].parent].retrieve && (sres == 1 || sres == 3)) {
					bullet[b].state = 1;
					bullet[b].a = TraceB;
				} else {
					memcpy(&bullet[b], &bullet[b + 1], (bulletCh + 1 - b) * sizeof(TBullet));
					b--;
					bulletCh--;
				}
			} else {
				bullet[b].a.x += d.x;
				bullet[b].a.y += d.y;
				bullet[b].a.z += d.z;
				if (poon) {
					bullet[b].dif.y -= WeapInfo[bullet[b].parent].FallAq;
					if (bullet[b].aqState<2) bullet[b].fallTotal += WeapInfo[bullet[b].parent].FallAq;
				} else {
					bullet[b].dif.y -= WeapInfo[bullet[b].parent].Fall;
					if (bullet[b].aqState < 2) bullet[b].fallTotal += WeapInfo[bullet[b].parent].Fall;
				}
				if (WeapInfo[bullet[b].parent].bullet) {
					bullet[b].FTime += TimeDt;
					if (bullet[b].FTime >= Weapon.Bullet[bullet[b].parent].Animation[0].AniTime)
						bullet[b].FTime %= Weapon.Bullet[bullet[b].parent].Animation[0].AniTime;
					bullet[b].alpha = FindVectorAlpha(bullet[b].dif.x, bullet[b].dif.z);
					bullet[b].beta = FindVectorAlpha(
						sqrt(bullet[b].dif.z*bullet[b].dif.z +
							bullet[b].dif.x*bullet[b].dif.x), bullet[b].dif.y);
				}
			}

		}

	}

}



void registerDamage(int Dino, bool enemyBullet) {

	if (!Characters[Dino].Health)
	{
		if ((DinoInfo[Characters[Dino].CType].BaseScore || DinoInfo[Characters[Dino].CType].trophy) && !Multiplayer && g_GameMode != GameMode::SurvivalMode && !enemyBullet) //No trophies in multiplayer for now - update this at later date?
		{
			TrophyRoom.Last.success++;
			SubmitDinoScore(Dino);
		}

		//No amb respawn in multiplayer for now - update this at later date?
		Characters_AddSecondaryOne(&Characters[Dino]);

	}
	else
	{
		Characters[Dino].awareHunter = true;
		Characters[Dino].AfraidTime = 60 * 1000;
		if (Characters[Dino].Clone != AI_TREX || Characters[Dino].State == 0)
			Characters[Dino].State = 2;

		Characters[Dino].BloodTTime += 90000;

	}

	if (Characters[Dino].Clone == AI_TREX)
		if (Characters[Dino].State)
			Characters[Dino].State = 5;
		else
			Characters[Dino].State = 1;

}



void RemoveCharacter(int index)
{
  if (index==-1) return;
  memcpy( &Characters[index], &Characters[index+1], (255 - index) * sizeof(TCharacter) );
  ChCount--;

  if (DemoPoint.CIndex > index) DemoPoint.CIndex--;

  for (int c=0; c<ShipTask.tcount; c++)
    if (ShipTask.clist[c]>index) ShipTask.clist[c]--;
}


void AnimateBag() {
	if (AmmoBag.State == 1) {
		AmmoBag.pos.y -= 4 * TimeDt;
		if (GetLandH(AmmoBag.pos.x, AmmoBag.pos.z) > AmmoBag.pos.y) {
			AmmoBag.pos.y = GetLandH(AmmoBag.pos.x, AmmoBag.pos.z);
			AmmoBag.State = 2;
			AddVoice3d(BagModel.SoundFX[0].length, BagModel.SoundFX[0].lpData.data(),
				AmmoBag.pos.x, AmmoBag.pos.y, AmmoBag.pos.z);
		}
	}
	if (AmmoBag.State == 2) {
		AmmoBag.FTime += TimeDt;
		if (AmmoBag.FTime >= BagModel.Animation[0].AniTime) {
			AmmoBag.State = 3;
			AmmoBag.FTime = 0;
		}
	}

	if (VectorLength(SubVectors(PlayerPos, AmmoBag.pos)) < 200.f && AmmoBag.State > 0) {
		AddVoicev(BagModel.SoundFX[1].length,
			BagModel.SoundFX[1].lpData.data(), 256);
		AmmoBag.State = -1;
		refillWeapons(false);
	}

}


void AnimateSShip() {
	if (SShip.State < 1) return;

	SetAmbient3d(SShipModel.SoundFX[0].length,
		SShipModel.SoundFX[0].lpData.data(),
		SShip.pos.x, SShip.pos.y, SShip.pos.z);
	
	int _TimeDt = TimeDt;

	TBEGIN:

	float L = VectorLength(SubVectors2d(SShip.tgpos, SShip.pos));
	float L2 = sqrt((SShip.tgpos.x - SShip.pos.x) * (SShip.tgpos.x - SShip.pos.x) +
		(SShip.tgpos.x - SShip.pos.x) * (SShip.tgpos.x - SShip.pos.x));

	if (L < 256.f && SShip.State == 1) {
		SShip.tgpos = SShip.retpos;
		SShip.State = 2;
		AmmoBag.pos = SShip.pos;
		AmmoBag.State = 1;
		goto TBEGIN;
	}
	if (SShip.State == 2 && VectorLength(SubVectors(PlayerPos, SShip.pos)) > (ctViewR + 2) * 256){
		SShip.State = -1;
		return;
	}

	SShip.pos.y += 0.3f*static_cast<float>(cos(RealTime / 256.f));

	SShip.tgalpha = FindVectorAlpha(SShip.tgpos.x - SShip.pos.x, SShip.tgpos.z - SShip.pos.z);
	float currspeed;
	float dalpha = static_cast<float>(fabs(SShip.tgalpha - SShip.alpha));
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;

	if (VectorLength(SubVectors(PlayerPos, SShip.pos)) < (ctViewR + 2) * 256 && SShip.State != 1 && dalpha < 1)
	{
		SShip.tgpos.x += static_cast<float>(cos(SShip.alpha)) * 256 * 6.f;
		SShip.tgpos.z += static_cast<float>(sin(SShip.alpha)) * 256 * 6.f;
	}


	//====== speed ===============//
	float vspeed = 64;
	//if (fabs(dalpha) > 0.4) vspeed = 0.f;
	float _s = SShip.speed;
	if (vspeed > SShip.speed) DeltaFunc(SShip.speed, vspeed, TimeDt / 200.f);
	else SShip.speed = vspeed;

	//====== fly ===========//
	float l = TimeDt * SShip.speed / 16.f;
	Vector3d _pos = SShip.pos;
	SShip.pos.x += static_cast<float>(cos(SShip.alpha))*l;
	SShip.pos.z += static_cast<float>(sin(SShip.alpha))*l;

	//======= y movement ============//
	float h = GetLandUpH(SShip.pos.x, SShip.pos.z);
	DeltaFunc(SShip.pos.y, SShip.tgpos.y, TimeDt / 4.f);
	if (SShip.pos.y < h + 1024)
	{
		SShip.pos.y = h + 1024;
	}



	//======= rotation ============//

	float tggamma = SShip.alpha;

	if (SShip.tgalpha > SShip.alpha) currspeed = 0.1f + static_cast<float>(fabs(drspd)) / 2.f;
	else currspeed = -0.1f - static_cast<float>(fabs(drspd)) / 2.f;

	if (fabs(dalpha) > pi) currspeed = -currspeed;


	DeltaFunc(SShip.rspeed, currspeed, static_cast<float>(TimeDt) / 420.f);

	float rspd = SShip.rspeed * TimeDt / 2024.f;
	if (fabs(drspd) < fabs(rspd))
	{
		SShip.alpha = SShip.tgalpha;
		SShip.rspeed /= 2;
	}
	else
	{
		SShip.alpha += rspd;
	}

	tggamma -= SShip.alpha;
	tggamma *= 100;
	if (SShip.alpha < 0) SShip.alpha += pi * 2;
	if (SShip.alpha > pi * 2) SShip.alpha -= pi * 2;

	float curgspeed;
	float dgamma = static_cast<float>(fabs(tggamma - SShip.gamma));
	float dgspd = dgamma;
	if (dgspd > pi) dgspd = 2 * pi - dgspd;
	if (tggamma > SShip.gamma) curgspeed = 0.1f + static_cast<float>(fabs(dgspd)) / 2.f;
	else curgspeed = -0.1f - static_cast<float>(fabs(dgspd)) / 2.f;
	curgspeed *= 2;

	if (fabs(dgamma) > pi) curgspeed = -curgspeed;

	DeltaFunc(SShip.gspeed, curgspeed, static_cast<float>(TimeDt) / 420.f);

	float gspd = SShip.gspeed * TimeDt / 2024.f;
	if (fabs(dgspd) < fabs(gspd))
	{
		SShip.gamma = tggamma;
		SShip.gspeed /= 2;
	}
	else
	{
		SShip.gamma += gspd;
	}
//	if (SShip.gamma < 0) SShip.gamma += pi * 2;
//	if (SShip.gamma > pi * 2) SShip.gamma -= pi * 2;

	//beta

	//bullet[b].beta = FindVectorAlpha(
	//sqrt(bullet[b].dif.z*bullet[b].dif.z +
	//	bullet[b].dif.x*bullet[b].dif.x), bullet[b].dif.y);

	
}

void AnimateShip()
{
  if (Ship.State==-1)
  {
    SetAmbient3d(0,0, 0,0,0);
    if (!ShipTask.tcount) return;
    InitShip(ShipTask.clist[0]);
    memcpy(&ShipTask.clist[0], &ShipTask.clist[1], 250*4);
    ShipTask.tcount--;
    return;
  }

  SetAmbient3d(ShipModel.SoundFX[0].length,
               ShipModel.SoundFX[0].lpData.data(),
               Ship.pos.x, Ship.pos.y, Ship.pos.z);

  int _TimeDt = TimeDt;

//====== get up/down time acceleration ===========//
  if (Ship.FTime)
  {
    int am = ShipModel.Animation[0].AniTime;
    if (Ship.FTime < 500) _TimeDt = TimeDt * (Ship.FTime + 48) / 548;
    if (am-Ship.FTime < 500) _TimeDt = TimeDt * (am-Ship.FTime + 48) / 548;
    if (_TimeDt<2) _TimeDt=2;
  }
//===================================

  float L  = VectorLength( SubVectors(Ship.tgpos, Ship.pos) );
  float L2 = sqrt ( (Ship.tgpos.x - Ship.pos.x) * (Ship.tgpos.x - Ship.pos.x) +
                    (Ship.tgpos.x - Ship.pos.x) * (Ship.tgpos.x - Ship.pos.x) );

  Ship.pos.y+=0.3f*static_cast<float>(cos(RealTime / 256.f));



  Ship.tgalpha    = FindVectorAlpha(Ship.tgpos.x - Ship.pos.x, Ship.tgpos.z - Ship.pos.z);
  float currspeed;
  float dalpha = static_cast<float>(fabs(Ship.tgalpha - Ship.alpha));
  float drspd = dalpha;
  if (drspd>pi) drspd = 2*pi - drspd;


//====== fly more away if I near =============//
  if (Ship.State)
    if (Ship.speed>1)
      if (L<4000)
        if (VectorLength(SubVectors(PlayerPos, Ship.pos))<(ctViewR+2)*256)
        {
          Ship.tgpos.x += static_cast<float>(cos(Ship.alpha)) * 256*6.f;
          Ship.tgpos.z += static_cast<float>(sin(Ship.alpha)) * 256*6.f;
          Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + Ship.DeltaY;
          Ship.tgpos.y = MAX(Ship.tgpos.y, GetLandUpH(Ship.pos.x, Ship.pos.z) + Ship.DeltaY);
        }
//==============================//



//========= animate down ==========//
  if (Ship.State==3)
  {
    Ship.FTime+=_TimeDt;
    if (Ship.FTime>=ShipModel.Animation[0].AniTime)
    {
      Ship.FTime=ShipModel.Animation[0].AniTime-1;
      Ship.State=2;
      AddVoicev(ShipModel.SoundFX[4].length,
                ShipModel.SoundFX[4].lpData.data(), 256);
      AddVoice3d(ShipModel.SoundFX[1].length, ShipModel.SoundFX[1].lpData.data(),
                 Ship.pos.x, Ship.pos.y, Ship.pos.z);
    }
    return;
  }


//========= get body on board ==========//
  if (Ship.State)
  {
    if (Ship.cindex!=-1)
    {
      DeltaFunc(Characters[Ship.cindex].pos.y, Ship.pos.y-650 - (Ship.DeltaY-2048), _TimeDt / 3.f);
      DeltaFunc(Characters[Ship.cindex].beta,  0, TimeDt / 4048.f);
      DeltaFunc(Characters[Ship.cindex].gamma, 0, TimeDt / 4048.f);
    }

    if (Ship.State==2)
    {
      Ship.FTime-=_TimeDt;
      if (Ship.FTime<0) Ship.FTime=0;

      if (Ship.FTime==0)
        if (fabs(Characters[Ship.cindex].pos.y - (Ship.pos.y-650 - (Ship.DeltaY-2048))) < 1.f)
        {
          Ship.State = 1;
          AddVoicev(ShipModel.SoundFX[5].length,
                    ShipModel.SoundFX[5].lpData.data(), 256);
          AddVoice3d(ShipModel.SoundFX[2].length, ShipModel.SoundFX[2].lpData.data(),
                     Ship.pos.x, Ship.pos.y, Ship.pos.z);
        }
      return;
    }
  }
//=====================================//


//====== speed ===============//
  float vspeed = 1.f + L / 128.f;
  if (vspeed > 24) vspeed = 24;
  if (Ship.State) vspeed = 24;
  if (fabs(dalpha) > 0.4) vspeed = 0.f;
  float _s = Ship.speed;
  if (vspeed>Ship.speed) DeltaFunc(Ship.speed, vspeed, TimeDt / 200.f);
  else Ship.speed = vspeed;

  if (Ship.speed>0 && _s==0)
    AddVoice3d(ShipModel.SoundFX[2].length, ShipModel.SoundFX[2].lpData.data(),
               Ship.pos.x, Ship.pos.y, Ship.pos.z);

//====== fly ===========//
  float l = TimeDt * Ship.speed / 16.f;

  if (fabs(dalpha) < 0.4)
    if (l<L)
    {
      if (l>L2) l = L2 * 0.5f;
      if (L2<0.1) l = 0;
      Ship.pos.x += static_cast<float>(cos(Ship.alpha))*l;
      Ship.pos.z += static_cast<float>(sin(Ship.alpha))*l;
    }
    else
    {
      if (Ship.State)
      {
        Ship.State = -1;
        RemoveCharacter(Ship.cindex);
        return;
      }
      else
      {
        Ship.pos = Ship.tgpos;
        Ship.State = 3;
        Ship.FTime = 1;
        Ship.tgpos = Ship.retpos;
        Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + Ship.DeltaY;
        Ship.tgpos.y = MAX(Ship.tgpos.y, GetLandUpH(Ship.pos.x, Ship.pos.z) + Ship.DeltaY);
        Characters[Ship.cindex].StateF = 0xFF;
        AddVoice3d(ShipModel.SoundFX[1].length, ShipModel.SoundFX[1].lpData.data(),
                   Ship.pos.x, Ship.pos.y, Ship.pos.z);
      }
    }

//======= y movement ============//
  float h = GetLandUpH(Ship.pos.x, Ship.pos.z);
  DeltaFunc(Ship.pos.y, Ship.tgpos.y, TimeDt / 4.f);
  if (Ship.pos.y < h + 1024)
  {
    if (Ship.State)
      if (Ship.cindex!=-1)
        Characters[Ship.cindex].pos.y+= h + 1024 - Ship.pos.y;
    Ship.pos.y = h + 1024;
  }



//======= rotation ============//

  if (Ship.tgalpha > Ship.alpha) currspeed = 0.1f + static_cast<float>(fabs(drspd))/2.f;
  else currspeed =-0.1f - static_cast<float>(fabs(drspd))/2.f;

  if (fabs(dalpha) > pi) currspeed=-currspeed;


  DeltaFunc(Ship.rspeed, currspeed, static_cast<float>(TimeDt) / 420.f);

  float rspd=Ship.rspeed * TimeDt / 1024.f;
  if (fabs(drspd) < fabs(rspd))
  {
    Ship.alpha = Ship.tgalpha;
    Ship.rspeed/=2;
  }
  else
  {
    Ship.alpha+=rspd;
    if (Ship.State)
      if (Ship.cindex!=-1)
        Characters[Ship.cindex].alpha+=rspd;
  }

  if (Ship.alpha<0) Ship.alpha+=pi*2;
  if (Ship.alpha>pi*2) Ship.alpha-=pi*2;

//======== move body ===========//
  if (Ship.State)
  {
    if (Ship.cindex!=-1)
    {
      Characters[Ship.cindex].pos.x = Ship.pos.x;
      Characters[Ship.cindex].pos.z = Ship.pos.z;
    }
    if (L>1000) Ship.tgpos.y+=TimeDt / 12.f;
  }
  else
  {
    Ship.tgpos.x = Characters[Ship.cindex].pos.x;
    Ship.tgpos.z = Characters[Ship.cindex].pos.z;
    Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + Ship.DeltaY;
    Ship.tgpos.y = MAX(Ship.tgpos.y, GetLandUpH(Ship.pos.x, Ship.pos.z) + Ship.DeltaY);
  }



}



void ProcessTrophy()
{
  TrophyBody = -1;

  for (int c=0; c<ChCount; c++)
  {
	  //Vector3d p = Characters[c].pos;
	  Vector3d p;
    //p.x+=Characters[c].lookx * 256*2.5f;
    //p.z+=Characters[c].lookz * 256*2.5f;
	  p.x = Characters[c].xdata;
	  p.z = Characters[c].zdata;
	  p.y = GetLandH(p.x, p.z) + Characters[c].ydata;

	//Characters[c].Phase = 1;

	if (VectorLength(SubVectors(p, PlayerPos)) < 148 && TrophyRoom2.Body[Characters[c].State].ctype) {
      TrophyBody = Characters[c].State;
	  TrophyDisplayBody.ctype = TrophyRoom2.Body[Characters[c].State].ctype;
	  TrophyDisplayBody.scale = TrophyRoom2.Body[Characters[c].State].scale;
	  TrophyDisplayBody.weapon = TrophyRoom2.Body[Characters[c].State].weapon;
	  TrophyDisplayBody.score = TrophyRoom2.Body[Characters[c].State].score;
	  TrophyDisplayBody.phase = TrophyRoom2.Body[Characters[c].State].phase;
 	  TrophyDisplayBody.time = TrophyRoom2.Body[Characters[c].State].time;
	  TrophyDisplayBody.date = TrophyRoom2.Body[Characters[c].State].date;
	  TrophyDisplayBody.range = TrophyRoom2.Body[Characters[c].State].range;
	}
  }

  //if (TrophyBody==-1) return;

  //TrophyBody = Characters[TrophyBody].State;
}


void RespawnSnow(int st, int s, BOOL rand)
{
	Snow[s].pos.x = PlayerX + nv.x + siRand(12 * 256);//12
	Snow[s].pos.z = PlayerZ + nv.z + siRand(12 * 256);//12
	Snow[s].hl = GetLandUpH(Snow[s].pos.x, Snow[s].pos.z);
	Snow[s].ftime = 0;
	if (rand) Snow[s].pos.y = Snow[s].hl + 256 + rRand(12 * 256);
	else Snow[s].pos.y = Snow[s].hl + (8 + rRand(5)) * 256;
}



void AnimateElements()
{
  for (int eg=0; eg<ElCount; eg++)
  {

    if  (Elements[eg].Type == partGround)
    {
      int a1 = Elements[eg].RGBA >> 24;
      a1-=TimeDt/4;
      if (a1<0) a1=0;
      Elements[eg].RGBA = (Elements[eg].RGBA  & 0x00FFFFFF) + (a1<<24);
      int a2 = Elements[eg].RGBA2>> 24;
      a2-=TimeDt/4;
      if (a2<0) a2=0;
      Elements[eg].RGBA2= (Elements[eg].RGBA2 & 0x00FFFFFF) + (a2<<24);
      if (a1 == 0 && a2==0) Elements[eg].ECount = 0;
    }

    if  (Elements[eg].Type == partWater)
      if (Elements[eg].EDone == Elements[eg].ECount)
        Elements[eg].ECount = 0;

    if  (Elements[eg].Type == partBubble)
      if (Elements[eg].EDone == Elements[eg].ECount)
        Elements[eg].ECount = 0;

	if (Elements[eg].Type == partBlood)
		if ((Takt & 3) == 0)
			
		  if (Elements[eg].EDone == Elements[eg].ECount)
		  {
			int a1 = Elements[eg].RGBA >> 24;
			a1--;
			if (a1<0) a1=0;
			Elements[eg].RGBA = (Elements[eg].RGBA  & 0x00FFFFFF) + (a1<<24);
			int a2 = Elements[eg].RGBA2>> 24;
			a2--;
			if (a2<0) a2=0;
			Elements[eg].RGBA2= (Elements[eg].RGBA2 & 0x00FFFFFF) + (a2<<24);
			if (a1 == 0 && a2==0) Elements[eg].ECount = 0;
		  }
		  

//====== remove finished process =========//
    if (!Elements[eg].ECount)
    {
      memcpy(&Elements[eg], &Elements[eg+1], (ElCount+1-eg) * sizeof(TElements));
      ElCount--;
      eg--;
      continue;
    }


    for (int e=0; e<Elements[eg].ECount; e++)
    {
      if (Elements[eg].EList[e].Flags) continue;
      Elements[eg].EList[e].pos.x+=Elements[eg].EList[e].speed.x * TimeDt / 1000.f;
      Elements[eg].EList[e].pos.y+=Elements[eg].EList[e].speed.y * TimeDt / 1000.f;
      Elements[eg].EList[e].pos.z+=Elements[eg].EList[e].speed.z * TimeDt / 1000.f;

      float h;
      h = GetLandUpH(Elements[eg].EList[e].pos.x, Elements[eg].EList[e].pos.z);
      BOOL OnWater = GetLandH(Elements[eg].EList[e].pos.x, Elements[eg].EList[e].pos.z) < h;

      switch (Elements[eg].Type)
      {
      case partBubble:
        Elements[eg].EList[e].speed.y += 2.0 * 256 * TimeDt / 1000.f;
        if (Elements[eg].EList[e].speed.y > 824) Elements[eg].EList[e].speed.y = 824;
        if (Elements[eg].EList[e].pos.y > h)
        {
          AddWCircle(Elements[eg].EList[e].pos.x, Elements[eg].EList[e].pos.z, 0.6);
          Elements[eg].EDone++;
          Elements[eg].EList[e].Flags = 1;
          if (OnWater) Elements[eg].EList[e].pos.y-= 10240;
        }
        break;

      default:
        Elements[eg].EList[e].speed.y -= 9.8 * 256 * TimeDt / 1000.f;
        if (Elements[eg].EList[e].pos.y < h)
        {
          if (OnWater) AddWCircle(Elements[eg].EList[e].pos.x, Elements[eg].EList[e].pos.z, 0.6);
          Elements[eg].EDone++;
          Elements[eg].EList[e].Flags = 1;
          if (OnWater) Elements[eg].EList[e].pos.y-= 10240;
          else Elements[eg].EList[e].pos.y = h + 4;
        }
        break;

      } //== switch ==//

    } // for(e) //
  } // for(eg) //

  AnimateBloodTrails();


  
  
  for (int st = 0; st < SnowCh; st++) {
	  nv = Wind.nv;
	  NormVector(nv, (4 + Wind.speed) * SnowInfo[st].snow_hSpd * TimeDt / 1000);//4

	  while (SnowInfo[st].SnCount < SnowInfo[st].snow_dens) {//2000
		  RespawnSnow(st, SnowInfo[st].addr + SnowInfo[st].SnCount, true);
		  SnowInfo[st].SnCount++;
	  }

	  for (int s = SnowInfo[st].addr; s < SnowInfo[st].addr+SnowInfo[st].SnCount; s++) {

		  if ((fabs(Snow[s].pos.x - PlayerX) > 14 * 256) ||
			  (fabs(Snow[s].pos.z - PlayerZ) > 14 * 256)) {
			  Snow[s].pos.x = PlayerX + siRand(12 * 256);
			  Snow[s].pos.z = PlayerZ + siRand(12 * 256);
			  Snow[s].pos.y = Snow[s].pos.y - Snow[s].hl;
			  Snow[s].hl = GetLandUpH(Snow[s].pos.x, Snow[s].pos.z);
			  Snow[s].pos.y += Snow[s].hl;
		  }

		  if (!Snow[s].ftime) {
			  float v = (((RealTime + s * 23) % 800) - 400) * TimeDt / 16000;
			  Snow[s].pos.x += ca * v;
			  Snow[s].pos.z += sa * v;

			  Snow[s].pos = AddVectors(Snow[s].pos, nv);
			  Snow[s].hl = GetLandUpH(Snow[s].pos.x, Snow[s].pos.z);
			  Snow[s].pos.y -= TimeDt * SnowInfo[st].snow_vSpd / 1000.f; //192
			  if (Snow[s].pos.y < Snow[s].hl + 8) {
				  Snow[s].pos.y = Snow[s].hl + 8;
				  Snow[s].ftime = 1;
			  }
		  }
		  else {
			  Snow[s].ftime += TimeDt;
			  Snow[s].pos.y -= TimeDt * (SnowInfo[st].snow_vSpd / 64) / 1000.f; //3
			  if (Snow[s].ftime > (2000 / (SnowInfo[st].snow_vSpd / 192)))  RespawnSnow(st, s, false); //2000
		  }

	  }


  }
  

}


void AnimateProcesses()
{
  AnimateElements();

  if ((Takt & 63)==0)
  {
    float al2 = CameraAlpha + siRand(60) * pi / 180.f;
    float c2 = cos(al2);
    float s2 = sin(al2);
    float l = 1024 + rRand(3120);
    float xx = CameraX + s2 * l;
    float zz = CameraZ - c2 * l;
    if (GetLandUpH(xx,zz) > GetLandH(xx,zz)+256)
      AddElements(xx, GetLandH(xx,zz), zz, 4, 6 + rRand(6));
  }

  if (!Multiplayer || Host) {
	  if (Takt & 1)
	  {
		  Wind.alpha += siRand(16) / 4096.f;
		  Wind.speed += siRand(400) / 6400.f;
	  }

	  if (Wind.speed < 4.f) Wind.speed = 4.f;
	  if (Wind.speed > 18.f) Wind.speed = 18.f;
  }
  Wind.nv.x = static_cast<float>(sin(Wind.alpha));
  Wind.nv.z = static_cast<float>(-cos(Wind.alpha));
  Wind.nv.y = 0.f;

  if (answtime)
  {
    answtime-=TimeDt;
    if (answtime<=0)
    {
      answtime = 0;
      int r = rRand(128) % 3;
      AddVoice3d(fxCall[answcall-10][r].length,  fxCall[answcall-10][r].lpData.data(),
                 answpos.x, answpos.y, answpos.z);
    }
  }



  if (CallLockTime)
  {
    CallLockTime-=TimeDt;
    if (CallLockTime<0) CallLockTime=0;
  }

  CheckAfraid();
  AnimateShip();
  AnimateSShip();
  AnimateBag();
  if (g_GameMode == GameMode::TrophyMode)
    ProcessTrophy();

  for (int w=0; w<WCCount; w++)
  {
    if (WCircles[w].scale > 1)
      WCircles[w].FTime+=static_cast<int>((TimeDt*3 / WCircles[w].scale));
    else
      WCircles[w].FTime+=TimeDt*3;
    if (WCircles[w].FTime >= 2000)
    {
      // Shift [w+1 .. WCCount-1] down to [w .. WCCount-2]. That is
      // (WCCount-1-w) elements. The old code used (WCCount+1-w), which
      // copied 2 extra elements and read/wrote one past the array end
      // when the buffer was full (w == WCCount-1 == 2095).
      memmove(&WCircles[w], &WCircles[w+1], sizeof(TWCircle) * (WCCount - 1 - w));
      w--;
      WCCount--;
    }
  }

  if (WaveNoteTime) {
	  WaveNoteTime -= TimeDt;
	  if (WaveNoteTime <= 0) WaveNoteTime = 0;
  }

  if (ExitTime)
  {
    ExitTime-=TimeDt;
    if (ExitTime<=0)
    {
      TrophyRoom.Total.time   +=TrophyRoom.Last.time;
      TrophyRoom.Total.smade  +=TrophyRoom.Last.smade;
      TrophyRoom.Total.success+=TrophyRoom.Last.success;
      TrophyRoom.Total.path   +=TrophyRoom.Last.path;

      if (MyHealth) SaveTrophy();
      else LoadTrophy();
      DoHalt("");
    }
  }
}



void RemoveCurrentTrophy()
{
  int p = 0;
  if (g_GameMode != GameMode::TrophyMode) return;
  if (!TrophyRoom2.Body[TrophyBody].ctype) return;

  PrintLogVerbose("Trophy removed: ");
  //PrintLog(DinoInfo[TrophyRoom.Body[TrophyBody].ctype].Name);
  PrintLogVerbose(DinoInfo[TrophyRoom2.Body[TrophyBody].ctype].Name);
  PrintLogVerbose("\n");

  
  for (int c=0; c<TrophyBody; c++)
    if (TrophyRoom2.Body[c].ctype) p++;

  Characters[p] = {};
  TrophyRoom2.Body[TrophyBody] = {};

  memcpy(&Characters[p],
         &Characters[p+1],
         (250-p) * sizeof(TCharacter) );
  ChCount--;

  
  TrophyDisplay = false;
  TrophyBody = -1;
}


void LoadTrophy2(int RegNumber) {

	FillMemory(&TrophyRoom2, sizeof(TrophyRoom2), 0);
	DWORD l;
	char fname2[128];
	sprintf_s(fname2, sizeof(fname2), "trophy0%d.sab", RegNumber);
	HANDLE hfile2 = CreateFile(fname2, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hfile2 == INVALID_HANDLE_VALUE)
	{
		PrintLog("===> Error loading trophyB!\n");
		return;
	}
	ReadFile(hfile2, &TrophyRoom2, sizeof(TrophyRoom2), &l, nullptr);

	CloseHandle(hfile2);

	TrophyRoom2.versionID = MODDERS_EDITION_VERSION_ID;

	PrintLog("TrophyB Loaded.\n");
}

void LoadTrophy()
{
  int pr = TrophyRoom.RegNumber;
  FillMemory(&TrophyRoom, sizeof(TrophyRoom), 0);
  TrophyRoom.RegNumber = pr;
  DWORD l;
  char fname[128];
  int rn = TrophyRoom.RegNumber;
  sprintf_s(fname, sizeof(fname), "trophy0%d.sav", TrophyRoom.RegNumber);
  HANDLE hfile = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hfile==INVALID_HANDLE_VALUE)
  {
    PrintLog("===> Error loading trophy!\n");
    return;
  }
  ReadFile(hfile, &TrophyRoom, sizeof(TrophyRoom), &l, nullptr);

  ReadFile(hfile, &OptAgres, 4, &l, nullptr);
  ReadFile(hfile, &OptDens, 4, &l, nullptr);
  ReadFile(hfile, &OptSens, 4, &l, nullptr);

  if (Multiplayer) OptDens = 128;

  ReadFile(hfile, &OptRes, 4, &l, nullptr);
  ReadFile(hfile, &FOGENABLE, 4, &l, nullptr);
  ReadFile(hfile, &OptText, 4, &l, nullptr);
  ReadFile(hfile, &OptViewR, 4, &l, nullptr);
  if (l != 4) OptViewR = kViewOptDefault;
  OptViewR = ClampViewOpt(OptViewR);
  ReadFile(hfile, &SHADOWS3D, 4, &l, nullptr);
  ReadFile(hfile, &OptMsSens, 4, &l, nullptr);
  ReadFile(hfile, &OptBrightness, 4, &l, nullptr);


  ReadFile(hfile, &KeyMap, sizeof(KeyMap), &l, nullptr);
  ReadFile(hfile, &REVERSEMS, 4, &l, nullptr);
  //  Ignore savefile settings for equipment — skip 4 DWORDs
  SetFilePointer(hfile, 16, nullptr, FILE_CURRENT);
  ReadFile(hfile, &OPT_ALPHA_COLORKEY, 4, &l, nullptr);

  ReadFile(hfile, &OptSys, 4, &l, nullptr);
  ReadFile(hfile, &OptSound, 4, &l, nullptr);
  ReadFile(hfile, &OptRender, 4, &l, nullptr);
  OptSound = NormalizeAudioBackend(OptSound);

  // OptFov and other extended settings are now in config.cfg.
  // LoadConfig() in InitEngine() will override OptFov after this point.

  SetupRes();

  CloseHandle(hfile);
  TrophyRoom.RegNumber = rn;

  PrintLog("Trophy Loaded.\n");

  if (TrophyRoom.Body[0].ctype) LoadTrophy2(TrophyRoom.RegNumber);
  else TrophyRoom.Body[0].ctype = 1;

//	TrophyRoom.Score = 299;
}


void SaveTrophy2(int RegNumber) {
	DWORD l2;
	char fname2[128];
	sprintf_s(fname2, sizeof(fname2), "trophy0%d.sab", RegNumber);

	HANDLE hfile2 = CreateFile(fname2, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hfile2 == INVALID_HANDLE_VALUE)
	{
		PrintLog("==>> Error saving trophy!\n");
		return;
	}
	WriteFile(hfile2, &TrophyRoom2, sizeof(TrophyRoom2), &l2, nullptr);
	CloseHandle(hfile2);
	PrintLog("TrophyB Saved.\n");
}

void SaveTrophy()
{

	//if (SurvivalMode) return;

  DWORD l;
  char fname[128];
  sprintf_s(fname, sizeof(fname), "trophy0%d.sav", TrophyRoom.RegNumber);

  int r = TrophyRoom.Rank;
  TrophyRoom.Rank = 0;
  if (TrophyRoom.Score >= 100) TrophyRoom.Rank = 1;
  if (TrophyRoom.Score >= 300) TrophyRoom.Rank = 2;


  HANDLE hfile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hfile == INVALID_HANDLE_VALUE)
  {
    PrintLog("==>> Error saving trophy!\n");
    return;
  }
  WriteFile(hfile, &TrophyRoom, sizeof(TrophyRoom), &l, nullptr);

  WriteFile(hfile, &OptAgres, 4, &l, nullptr);
  WriteFile(hfile, &OptDens, 4, &l, nullptr);
  WriteFile(hfile, &OptSens, 4, &l, nullptr);

  WriteFile(hfile, &OptRes, 4, &l, nullptr);
  WriteFile(hfile, &FOGENABLE, 4, &l, nullptr);
  WriteFile(hfile, &OptText, 4, &l, nullptr);
  WriteFile(hfile, &OptViewR, 4, &l, nullptr);
  WriteFile(hfile, &SHADOWS3D, 4, &l, nullptr);
  WriteFile(hfile, &OptMsSens, 4, &l, nullptr);
  WriteFile(hfile, &OptBrightness, 4, &l, nullptr);

  WriteFile(hfile, &KeyMap, sizeof(KeyMap), &l, nullptr);
  WriteFile(hfile, &REVERSEMS, 4, &l, nullptr);

  WriteFile(hfile, &ScentMode, 4, &l, nullptr);
  WriteFile(hfile, &CamoMode, 4, &l, nullptr);
  WriteFile(hfile, &RadarMode, 4, &l, nullptr);
  WriteFile(hfile, &Tranq, 4, &l, nullptr);
  WriteFile(hfile, &OPT_ALPHA_COLORKEY, 4, &l, nullptr);

  WriteFile(hfile, &OptSys, 4, &l, nullptr);
  WriteFile(hfile, &OptSound, 4, &l, nullptr);
  WriteFile(hfile, &OptRender, 4, &l, nullptr);
  // OptFov and other extended settings live in config.cfg, not here.
  CloseHandle(hfile);
  PrintLog("Trophy Saved.\n");

  SaveTrophy2(TrophyRoom.RegNumber);

}


// ================================================================
// config.cfg — text-based settings file (shared with Carnivores2Menu)
// ================================================================
// Resolve config.cfg relative to the game executable first, then fall
// back to the current working directory. This ensures the file is found
// regardless of how the game is launched (via Menu or directly from a
// command prompt in a different directory).
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

static void LoadConfig()
{
  char configPath[MAX_PATH];
  GetConfigPath(configPath, sizeof(configPath));

  HANDLE hfile = CreateFileA(configPath, GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hfile == INVALID_HANDLE_VALUE) {
    PrintLog("Config: config.cfg not found, using defaults.\n");
    return;
  }

  char buf[4096];
  DWORD bytesRead = 0;
  if (!ReadFile(hfile, buf, sizeof(buf) - 1, &bytesRead, nullptr) || bytesRead == 0) {
    CloseHandle(hfile);
    return;
  }
  buf[bytesRead] = '\0';
  CloseHandle(hfile);

  // Simple line-by-line parser: "key value"
  char* ctx = nullptr;
  char* line = strtok_s(buf, "\r\n", &ctx);
  while (line) {
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '\0') {
      line = strtok_s(nullptr, "\r\n", &ctx);
      continue;
    }

    char key[64];
    int value = 0;
    if (sscanf_s(line, "%63s %d", key, (unsigned)sizeof(key), &value) == 2) {
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
      else if (_stricmp(key, "nightvision_key") == 0) {
        NightVisionKey = value;
      }
      // Future settings: add else-if branches here
    }

    line = strtok_s(nullptr, "\r\n", &ctx);
  }

  PrintLog("Config Loaded (config.cfg).\n");
}

