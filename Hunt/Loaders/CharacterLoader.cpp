// ==========================================================================
// CharacterLoader.cpp
// ==========================================================================

#include "Hunt.h"

// Forward declarations
void PlaceHunter();

void LoadCharacters()
{
  BOOL pres[DINOINFO_MAX];
  FillMemory(pres, sizeof(pres), 0);
  pres[0]=true;
  for (int c=0; c<ChCount; c++)
  {
    pres[Characters[c].CType] = true;
  }

  for (int c=0; c<TotalC; c++) if (pres[c] || (g_GameMode == GameMode::SurvivalMode && DinoInfo[c].survivalDino))
    {

      if (!ChInfo[c].mptr)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\%s", DinoInfo[c].FName);
        LoadCharacterInfo(ChInfo[c], logt);
        PrintLog("Loading: ");
        PrintLog(logt);
        PrintLog("\n");
      }

    }

  for (int c=10; c<20; c++)
    if (TargetDino & (1<<c))
      if (!MenuDinoInfo[c-10].CallIcon.lpImage)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\MENU\\PICS\\call%d.tga", c-9);
        LoadPictureTGA(MenuDinoInfo[c - 10].CallIcon, logt, MemoryTag::Level);
        conv_pic(MenuDinoInfo[c - 10].CallIcon);
      }

  // Keep track of max VCount for the weapons available
  int maxWeaponVCount = 0;
  for (int c=0; c<TotalW; c++)
    if (WeaponPres & (1<<c))
    {
      if (!Weapon.chinfo[c].mptr)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].FName);
        LoadCharacterInfo(Weapon.chinfo[c], logt);
        PrintLog("Loading: ");
        PrintLog(logt);
        PrintLog("\n");
      }

	  if (WeapInfo[c].bullet) {
		  sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].BLName);
		  LoadCharacterInfo(Weapon.Bullet[c], logt);
		  PrintLog("Loading: ");
		  PrintLog(logt);
		  PrintLog("\n");
	  }

	  maxWeaponVCount = MAX(Weapon.chinfo[c].mptr->VCount, maxWeaponVCount);

      if (!Weapon.BulletPic[c].lpImage)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].BFName);
        LoadPictureTGA(Weapon.BulletPic[c], logt, MemoryTag::Level);
        conv_pic(Weapon.BulletPic[c]);
        PrintLog("Loading: ");
        PrintLog(logt);
        PrintLog("\n");
      }

	  if (!Weapon.ChambPic[c].lpImage && WeapInfo[c].picch)
	  {
		  sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].CFName);
		  LoadPictureTGA(Weapon.ChambPic[c], logt, MemoryTag::Level);
		  conv_pic(Weapon.ChambPic[c]);
		  PrintLog("Loading: ");
		  PrintLog(logt);
		  PrintLog("\n");
	  }

	    if (WeapInfo[c].MGSSound) {
			sprintf_s(logt, sizeof(logt), "MULTIPLAYER\\GUNSHOTS\\%s", WeapInfo[c].SFXName);
			LoadWav(logt, fxGunShot[c]);
			WeapInfo[c].SFXIndex = c;
		  } else WeapInfo[c].SFXIndex = -1;
	  
    }

  // Allocate space for normals fitting all available weapons. This is
  // a per-level allocation -- it's reallocated in LoadResources() on
  // each level load, and the old one is dropped by LevelArena->Reset()
  // in ReleaseResources(). Tag it Level so the arena holds it.
  Weapon.normals.reset((Vector3d*)_HeapAlloc(Heap, 0, sizeof(Vector3d) * maxWeaponVCount, MemoryTag::Level));

  for (int c=10; c<20; c++)
    if (TargetDino & (1<<c))
      if (fxCall[c-10][0].lpData.empty())
      {
        sprintf_s(logt, sizeof(logt),"HUNTDAT\\SOUNDFX\\CALLS\\call%d_a.wav", (c-9));
        LoadWav(logt, fxCall[c-10][0]);
        sprintf_s(logt, sizeof(logt),"HUNTDAT\\SOUNDFX\\CALLS\\call%d_b.wav", (c-9));
        LoadWav(logt, fxCall[c-10][1]);
        sprintf_s(logt, sizeof(logt),"HUNTDAT\\SOUNDFX\\CALLS\\call%d_c.wav", (c-9));
        LoadWav(logt, fxCall[c-10][2]);
      }

  sprintf_s(logt, sizeof(logt), "MULTIPLAYER\\AVATARS\\Hitbox.car");
  LoadCharacterInfo(HitBoxModel, logt);
  PrintLog("Loading: ");
  PrintLog(logt);
  PrintLog("\n");

  //multiplayer hunter models
  //test - 1 other player
  //test - add custom models at some point?
  if (Multiplayer) {
	  sprintf_s(logt, sizeof(logt), "MULTIPLAYER\\AVATARS\\Poacher.car");
	  LoadCharacterInfo(MPlayerInfo[0], logt);
	  PrintLog("Loading: ");
	  PrintLog(logt);
	  PrintLog("\n");
  }

  // Phase 5F.2: print per-level arena stats now that every per-level
  // allocation has been made. C1 has the same call at
  // Carnivores1/Hunt/Resources.cpp:1501. Useful for two things:
  //   1. Spotting memory-hungry levels (peak usage vs capacity).
  //   2. Verifying the arena reset works between level transitions
  //      (if the alloc count keeps growing across reloads, the reset
  //      is broken).
  // No-op in non-MEM_DEBUG builds (LogStats is a plain method, not
  // macro-gated, but the call site is the only place that needs the
  // report and it's useful even in release for production tuning).
  if (LevelArena != nullptr) {
    LevelArena->LogStats(" after load");
  }
}

void resetSSHip() {
	SShip.State = 0;
	SShip.alpha = 0;
	SShip.speed = 0;
	AmmoBag.State = 0;
}

void resetBullets() {
	for (int b = 0; b < bulletCh; b++) {
		bullet[b] = {};
	}
	bulletCh = 0;
}

void refillWeapons(bool init) {
	for (int w = 0; w < TotalW; w++)
		if (WeaponPres & (1 << w))
		{
			if (WeapInfo[w].fullauto) FiringMode[w] = 1; else FiringMode[w] = 0;

			ShotsLeft[w] = WeapInfo[w].Shots;
			if (DoubleAmmo) {
				if (WeapInfo[w].Reload || WeapInfo[w].rldAnim < 0) {
					ShotsLeft[w] *= 2;
				} else {
					AmmoMag[w] = 1;
					MagShotsLeft[w] = WeapInfo[w].Shots;
				}
			}
			
			if (WeapInfo[w].Reload) {
				if (init || WeapInfo[w].rldAnim < 0) {
					Chambered[w] = WeapInfo[w].Reload;
					ShotsLeft[w] -= WeapInfo[w].Reload;
				}
			} else {
				if (init || WeapInfo[w].pmpAnim < 0) {
					Chambered[w] = 1;
					ShotsLeft[w] -= 1;
				}
			}
			if (TargetWeapon == -1) TargetWeapon = w;
		}
}

void ReInitGame()
{
  PrintLog("ReInitGame();\n");

  // Initialize underwater fog debug parameters with default values.
  // These can be tweaked at runtime via the F10 debug menu.
  UnderwaterDebugMenu = 0;
  UnderwaterDebugSelected = 0;
  UWFog_VertRange = 400.0f;        // vertical gradient range (units)
  UWFog_VertStrength = 80.0f;      // additive fog at max depth
  UWFog_CurveExp = 3.0f;           // curve exponent (3=cubic)
  UWFog_CapBase = 100.0f;          // soft cap added to FLimit
  UWFog_CapCameraBoost = 50.0f;    // camera depth boost for cap
  UWFog_BaseDensityMult = 1.0f;    // base fog density multiplier (1.0 = unchanged)
  UWFog_CameraDepthMult = 0.0f;    // Beer-Lambert camera depth multiplier (0=off)

  // Water wave debug parameters
  UnderwaterDebugTab = 0;
  WWave1Amp = 18.0f;               // primary swell amplitude
  WWave2Amp = 10.0f;               // secondary cross-wave amplitude
  WWave3Amp = 5.0f;                // fine detail amplitude
  WWaveSpeed = 1.0f;               // time multiplier

  PlaceHunter();
  Muzz = false;
  MuzzFTime = 0;
  if (g_GameMode == GameMode::TrophyMode)	PlaceTrophy();
  else if (g_GameMode == GameMode::SurvivalMode) {
	  SurvivalWave = 0;
	  PlaceCharactersSurvival();
  } else {
	  PlaceCharacters();
	  resetSSHip();
	  resetBullets();
	  if (Multiplayer) {
		  sendGunShot = -1;
		  sendHunterCall = -1;
		  sendHunterCallType = -1;
		  for (int c = 0; c < ChCount; c++) {
			  sendDamage[c] = 0;
		  }
		  for (int i = 0; i < 4; i++) {
			  mGunShot[i] = -1;
			  mHunterCall[i] = -1;
			  mHunterCallType[i] = -1;
			  for (int c = 0; c < ChCount; c++) {
				  mDamage[i][c] = 0;
			  }
		  }
		  HunterCount = 0;
		  PlaceMHunters();//temp??
	  }
  }

  LoadCharacters();

  LockLanding = false;
  Wind.alpha = rRand(1024) * 2.f * pi / 1024.f;
  Wind.speed = 10;
  MyHealth = MAX_HEALTH;
  TargetWeapon = -1;

  refillWeapons(true);

  CurrentWeapon = TargetWeapon;

  Weapon.state = 0;
  Weapon.FTime = 0;
  PlayerAlpha = 0;
  PlayerBeta  = 0;
  Weapon.breath = 0.f;
  Weapon.BTime = 0;
  Weapon.HoldBreath = false;
  Weapon.breathPressed = 0;

  WCCount = 0;
  ElCount = 0;
  BloodTrail.Count = 0;
  g_GameMode = GameMode::Normal;
  g_GameMode = GameMode::Normal;
  g_GameMode = GameMode::Normal;
  g_GameMode = GameMode::Normal;

  if (g_GameMode == GameMode::SurvivalMode) {
	  PlayerAlpha = pi * 2 * SurvivalSpawnA / 360.f;
	  Weapon.state = 2;
	  if (WeapInfo[CurrentWeapon].Optic) g_GameMode = GameMode::OpticScope;
  }

  Ship.pos.x = PlayerX;
  Ship.pos.z = PlayerZ;
  Ship.pos.y = GetLandUpH(Ship.pos.x, Ship.pos.z) + 2048;
  Ship.State = -1;
  Ship.tgpos.x = Ship.pos.x;
  Ship.tgpos.z = Ship.pos.z + 60*256;
  Ship.cindex  = -1;
  Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + 2048;
  ShipTask.tcount = 0;

  if (g_GameMode != GameMode::TrophyMode)
  {
    TrophyRoom.Last.smade = 0;
    TrophyRoom.Last.success = 0;
    TrophyRoom.Last.path  = 0;
    TrophyRoom.Last.time  = 0;
  }

  DemoPoint.DemoTime = 0;
  RestartMode = false;
  TrophyDisplay=false;
  answtime = 0;
  ExitTime = 0;

  // Allocate (vertex count based) buffers for renderers. These are
  // per-level scratch (reset on each ReInitGame call before the new
  // level's vertices are read), so they go in the LevelArena. The
  // previous 3-arg form went to Heap; the arena will reclaim them on
  // LevelArena->Reset() during ReleaseResources().
  rVertex = (Vector3d*)_HeapAlloc(Heap, 0, sizeof(Vector3d) * MaxObjectVCount, MemoryTag::Level);
  gScrp = (Vector2di*)_HeapAlloc(Heap, 0, sizeof(Vector2di) * MaxObjectVCount, MemoryTag::Level);
  PhongMapping = (Vector2df*)_HeapAlloc(Heap, 0, sizeof(Vector2df) * MaxObjectVCount, MemoryTag::Level);
  AllocateRenderTables();
}