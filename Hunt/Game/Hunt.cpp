#include "Hunt.h"
#include "stdio.h"
#include <timeapi.h>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

static void EnablePerMonitorV2DpiAwareness()
{
  using SetProcessDpiAwarenessContextProc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
  auto setProcessDpiAwarenessContext =
    reinterpret_cast<SetProcessDpiAwarenessContextProc>(
      GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext"));

  if (setProcessDpiAwarenessContext) {
    setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  }
}

#ifdef _soft
BOOL PHONG = false;
BOOL GOUR  = false;
BOOL ENVMAP = false;
#else
BOOL PHONG = true;
BOOL GOUR  = true;
BOOL ENVMAP = true;
#endif

BOOL NeedRVM = true;

void HideWeapon();







float CalcFogLevel(Vector3d v)
{
  if (!FOGON) return 0;
  BOOL vinfog = true;
  int cf;
  if (!IsUnderwater())
  {
    cf = FogsMap[ (static_cast<int>((v.z + CameraZ)))>>9 ][ (static_cast<int>((v.x + CameraX)))>>9 ];
    if ((!cf) && CAMERAINFOG)
    {
      cf = CameraFogI;
      vinfog = false;
    }
  }
  else cf = 127;


  if (! (CAMERAINFOG | cf) ) return 0;
  TFogEntity *fptr;
  fptr = &FogsList[cf];
  CurFogColor = fptr->fogRGB;

  float d = VectorLength(v);

  v.y+=CameraY;

  float fla= -(v.y     - fptr->YBegin*ctHScale) / ctHScale;
  if (!vinfog) if (fla>0) fla=0;

  float flb = -(CameraY - fptr->YBegin*ctHScale) / ctHScale;
  if (!CAMERAINFOG) if (flb>0) flb=0;

  if (fla<0 && flb<0) return 0;

  if (fla<0)
  {
    d*= flb / (flb-fla);
    fla = 0;
  }
  if (flb<0)
  {
    d*= fla / (fla-flb);
    flb = 0;
  }

  float fl = (fla + flb);

  fl *= (d+(fptr->Transp/2)) / fptr->Transp;

  // Underwater: amplify fog density with camera depth below the water
  // surface.  The base fla+flb term already gives a linear depth
  // dependence.  The multiplicative boost and cap increase are kept
  // modest (0.15× / +25 cap) so the water stays relatively clear near
  // the surface and fogs up gradually — objects remain visible longer.
  // The sky/sun use a separate, faster fade (see GLSky.cpp shader).
  // CameraWaterDepthFactor is computed once per frame in ProcessControls().
  if (IsUnderwater())
  {
    fl *= 1.0f + CameraWaterDepthFactor * 0.15f;
    return MIN(fl, fptr->FLimit + CameraWaterDepthFactor * 25.0f);
  }

  return MIN(fl, fptr->FLimit);
}












void DrawScene()
{
#ifdef GL_PERF_HOOKS
  PerfFrameBegin();
#endif

  dFacesCount = 0;

  ca = static_cast<float>(cos(CameraAlpha));
  sa = static_cast<float>(sin(CameraAlpha));

  cb = static_cast<float>(cos(CameraBeta));
  sb = static_cast<float>(sin(CameraBeta));

  CCX = (static_cast<int>(CameraX) / 512) * 2;
  CCY = (static_cast<int>(CameraZ) / 512) * 2;

  PreCashGroundModel();

#ifdef _soft
  CreateChRenderList();
#endif

  RenderSkyPlane();

  cb = static_cast<float>(cos(CameraBeta));
  sb = static_cast<float>(sin(CameraBeta));


  RenderGround();

  RenderModelsList();

  Render3DHardwarePosts();

#ifdef _gl
  RenderProjectedShadows();
#endif

  if (NeedWater) RenderWater();

  RenderElements();
}




void DrawOpticCross( int v)
{
  int sx =  VideoCX + static_cast<int>((rVertex[v].x / (-rVertex[v].z) * CameraW));
  int sy =  VideoCY - static_cast<int>((rVertex[v].y / (-rVertex[v].z) * CameraH));

  if (  (fabs(static_cast<float>((VideoCX - sx))) > WinW / 2) ||
        (fabs(static_cast<float>((VideoCY - sy))) > WinH / 4) ) return;

  Render_Cross(sx, sy);
}



void ScanLifeForms()
{
  int li = -1;
  float dm = static_cast<float>((ctViewR+2))*256;
  for (int c=0; c<ChCount; c++)
  {
    TCharacter *cptr = &Characters[c];
	if (DinoInfo[cptr->CType].Aquatic) continue;
	if (DinoInfo[cptr->CType].HideBinoc) continue;
    if (!cptr->Health) continue;
    if (cptr->rpos.z > -512) continue;
    float d = static_cast<float>(sqrt( cptr->rpos.x*cptr->rpos.x + cptr->rpos.y*cptr->rpos.y + cptr->rpos.z*cptr->rpos.z ));
    if (d > ctViewR*256) continue;
    float r = static_cast<float>((fabs(cptr->rpos.x) + fabs(cptr->rpos.y))) / d;
    if (r > 0.15) continue;
    if (d<dm)
      if (!TraceLook(cptr->pos.x, cptr->pos.y+220, cptr->pos.z,
                     CameraX, CameraY, CameraZ) )
      {

        dm = d;
        li = c;
      }

  }

  if (li==-1) return;
  Render_LifeInfo(li);
}







void DrawPostObjects()
{
  float b;
  TWeapon* wptr = &Weapon;

  Hardware_ZBuffer(false);

  if (DemoPoint.DemoTime) goto SKIPWEAPON;

  GlassL = 0;
  // Keep near-model projection anchored to the classic 4:3 FOV so
  // viewmodels and HUD-like near renders do not shrink or drift when
  // the world FOV changes. The aspectScale correction makes binocular
  // and scope overlays fill the widescreen width (C1 has the same
  // logic in InsertModelList).
  float nearModelScale = FovScaleFromDegrees(kFovDefault) / FovScaleFromDegrees(OptFov);
  if (g_GameMode == GameMode::Binocular)
  {
    float oldCW = CameraW;
    float oldCH = CameraH;
    float scale = nearModelScale;
    float aspectScale = static_cast<float>(WinW) / (static_cast<float>(WinH) * 1.3333333f);
    if (aspectScale > 1.0f) scale *= aspectScale;
    CameraW *= scale;
    CameraH *= scale;
    RenderNearModel(Binocular.get(), 0, 0, 2*(216-72 * BinocularPower), 192,  0,0);
    CameraW = oldCW;
    CameraH = oldCH;
    ScanLifeForms();
  }

  //goto SKIPWIND;
  if (g_GameMode == GameMode::Binocular || (g_GameMode == GameMode::OpticScope && (!WeapInfo[CurrentWeapon].unzoom || Weapon.state==2))) goto SKIPWIND;

  if (g_GameMode != GameMode::TrophyMode && g_GameMode != GameMode::SurvivalMode)
    if (!KeyboardState[VK_CAPITAL] & 1)
    {
      BOOL lr = LOWRESTX;
      LOWRESTX = true;

      const int hudCenter = WinW / 2;
      const int hudSpread = static_cast<int>((static_cast<float>(WinW) / 3.0f * UIScale));
      const int hudBottomInset = static_cast<int>((static_cast<float>(WinH) * 0.012f));
      const int hudY = WinH - (WinH * 10 / 23) - hudBottomInset;

      VideoCX = hudCenter - hudSpread;
      VideoCY = hudY;
      CreateMorphedModel(WindModel.mptr.get(), &WindModel.Animation[0], static_cast<int>((Wind.speed*50.f)), 1.0);
      {
        const float savedCW = CameraW;
        const float savedCH = CameraH;
        CameraW *= nearModelScale;
        CameraH *= nearModelScale;
        RenderNearModel(WindModel.mptr.get(), -10, -37, -96, 192,  CameraAlpha-Wind.alpha,0);
        CameraW = savedCW;
        CameraH = savedCH;
      }

      VideoCX = hudCenter + hudSpread;
      VideoCY = hudY;
      {
        const float savedCW = CameraW;
        const float savedCH = CameraH;
        CameraW *= nearModelScale;
        CameraH *= nearModelScale;
        RenderNearModel(CompasModel.get(), +8, -38, -96, 192,  CameraAlpha,0);
        CameraW = savedCW;
        CameraH = savedCH;
      }

      VideoCX = WinW / 2;
      VideoCY = WinH / 2;
      LOWRESTX = lr;
    }

SKIPWIND:


  if (wptr->state == 0) {
	  if (Weapon.BTime) {
		  Weapon.BTime -= TimeDt * 2;
		  if (Weapon.BTime < 0) Weapon.BTime = 0;
	  }
	  goto SKIPWEAPON;
  }

  if (g_GameMode != GameMode::SurvivalMode) {
	  float tempT = static_cast<float>(TimeDt) / 10000.f;
	  wptr->shakel += tempT;
	  if (wptr->shakel > 4.0f) wptr->shakel = 4.0f;
  }

  if (wptr->state == 1)
  {
	  if (IsUnderwater()) wptr->FTime += TimeDt/2.f;
	  else wptr->FTime+=TimeDt;
    if (wptr->FTime >= wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].getAnim].AniTime)
    {
      wptr->FTime = 0;
      wptr->state = 2;
    }
  }

  if (wptr->state == 4)
  {
	  if (IsUnderwater()) wptr->FTime += TimeDt / 2.f;
	  else wptr->FTime += TimeDt;
	  if (wptr->FTime >= wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].rldAnim].AniTime)
	  {
		wptr->FTime = 0;
		wptr->state = 2;
		if (WeapInfo[CurrentWeapon].Reload) {
			if (g_GameMode != GameMode::SurvivalMode) ShotsLeft[CurrentWeapon] -= wptr->ammoIn;
			Chambered[CurrentWeapon] += wptr->ammoIn;
		} else {
		  int temp = MagShotsLeft[CurrentWeapon];
		  MagShotsLeft[CurrentWeapon] = ShotsLeft[CurrentWeapon];
		  ShotsLeft[CurrentWeapon] = temp;
		  if (!MagShotsLeft[CurrentWeapon]) AmmoMag[CurrentWeapon]--;
		  if (!Chambered[CurrentWeapon])
			  if (WeapInfo[CurrentWeapon].pmpAnim <= 0) {
				  Chambered[CurrentWeapon] = 1;
				  ShotsLeft[CurrentWeapon]--;  
			  } else {
				  if (WeapInfo[CurrentWeapon].autoPump) ProcessPump();
			  }
		}
	  }
  }

  if (wptr->state == 5)
  {
	  if (IsUnderwater()) wptr->FTime += TimeDt / 2.f;
	  else wptr->FTime += TimeDt;
	  if (wptr->FTime >= wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].rldAnimPart].AniTime)
	  {
		  wptr->FTime = 0;
		  wptr->state = 2;
		  if (WeapInfo[CurrentWeapon].Reload) {
			if (g_GameMode != GameMode::SurvivalMode) ShotsLeft[CurrentWeapon] -= wptr->ammoIn;
			Chambered[CurrentWeapon] += wptr->ammoIn;
		  } else {
			  int temp = MagShotsLeft[CurrentWeapon];
			  MagShotsLeft[CurrentWeapon] = ShotsLeft[CurrentWeapon];
			  ShotsLeft[CurrentWeapon] = temp;
			  if (!MagShotsLeft[CurrentWeapon]) AmmoMag[CurrentWeapon]--;
		  }
	  }
  }

  if (wptr->state == 2 && wptr->FTime>0)
  {
	  if (IsUnderwater()) wptr->FTime += TimeDt / 2.f;
	  else wptr->FTime += TimeDt;
	  if (Muzz && !IsUnderwater()) {
		if (wptr->FTime > MuzzModel.Animation[0].AniTime) {
			Muzz = false;
			MuzzFTime = 0;
		} else MuzzFTime = wptr->FTime;
	}

    if (wptr->FTime >= wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].shtAnim].AniTime)
    {
      wptr->FTime = 0;
      wptr->state = 2;
	  Muzz = false;
	  MuzzFTime = 0;
	  if (!WeapInfo[CurrentWeapon].Reload && !WeapInfo[CurrentWeapon].mustPump)
		  if (ShotsLeft[CurrentWeapon]) {
			  Chambered[CurrentWeapon] = 1;
			  if (g_GameMode != GameMode::SurvivalMode) ShotsLeft[CurrentWeapon]--;
		  }
	  if (WeapInfo[CurrentWeapon].mustPump && WeapInfo[CurrentWeapon].autoPump && ShotsLeft[CurrentWeapon]) ProcessPump();

	  if (WeapInfo[CurrentWeapon].autoReload && !Chambered[CurrentWeapon]
		  && (WeapInfo[CurrentWeapon].Reload || !ShotsLeft[CurrentWeapon])) ProcessReload();

    }
  }

  if (wptr->state == 6)
  {
	  if (IsUnderwater()) wptr->FTime += TimeDt / 2.f;
	  else wptr->FTime += TimeDt;
	  if (wptr->FTime >= wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].pmpAnim].AniTime)
	  {
		  wptr->FTime = 0;
		  wptr->state = 2;
		  if (ShotsLeft[CurrentWeapon]){
			Chambered[CurrentWeapon] = 1;
			if (g_GameMode != GameMode::SurvivalMode) ShotsLeft[CurrentWeapon]--;
		  }
	  }
  }


  if (wptr->state == 7)
  {
	  if (IsUnderwater()) wptr->FTime += TimeDt / 2.f;
	  else wptr->FTime += TimeDt;
	  if (wptr->FTime >= wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].modAnim].AniTime)
	  {
		  if (!FiringMode[CurrentWeapon]) FiringMode[CurrentWeapon] = 1; else FiringMode[CurrentWeapon] = 0;
		  wptr->FTime = 0;
		  wptr->state = 2;
	  }
  }

  if (wptr->state == 3)
  {
	  if (IsUnderwater()) wptr->FTime += TimeDt / 2.f;
	  else wptr->FTime+=TimeDt;
    if (wptr->FTime >= wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].putAnim].AniTime)
    {
      wptr->FTime = 0;
      wptr->state = 0;
      if (CurrentWeapon != TargetWeapon)
      {
        CurrentWeapon = TargetWeapon;
        HideWeapon();
      }
      goto SKIPWEAPON;
    }
  }


/*
  if (!ShotsLeft[CurrentWeapon])
  {
    HideWeapon();
    for (int w=0; w<10; w++)
      if (ShotsLeft[w])
      {
        TargetWeapon=w;
        break;
      }
  }
  */

  int phas;
  switch (wptr->state)
  {
  case 1:
	  if (WeapInfo[CurrentWeapon].getEmpAnim > 0 && !Chambered[CurrentWeapon])
		  phas = WeapInfo[CurrentWeapon].getEmpAnim;
	  else phas = WeapInfo[CurrentWeapon].getAnim;
	  break;
  case 2:

	  if (WeapInfo[CurrentWeapon].emptyAnim > 0 && !Chambered[CurrentWeapon] && !wptr->FTime)
		  phas = WeapInfo[CurrentWeapon].emptyAnim;
	  else phas = WeapInfo[CurrentWeapon].shtAnim;
	  break;
  case 3:
	  if (WeapInfo[CurrentWeapon].putEmpAnim > 0 && !Chambered[CurrentWeapon])
		  phas = WeapInfo[CurrentWeapon].putEmpAnim;
	  else phas = WeapInfo[CurrentWeapon].putAnim;
	  break;
  case 4:
	  phas = WeapInfo[CurrentWeapon].rldAnim;
	  break;
  case 5:
	  phas = WeapInfo[CurrentWeapon].rldAnimPart;
	  break;
  case 6:
	  phas = WeapInfo[CurrentWeapon].pmpAnim;
	  break;
  case 7:
	  phas = WeapInfo[CurrentWeapon].modAnim;
	  break;
  }

  CreateMorphedModel(wptr->chinfo[CurrentWeapon].mptr.get(),
                     &wptr->chinfo[CurrentWeapon].Animation[phas], wptr->FTime, 1.0);

  if (Weapon.HoldBreath) {
	  Weapon.BTime += TimeDt;
	  if (Weapon.BTime >= 4000) {
		  Weapon.BTime = 4000;
		  Weapon.HoldBreath = false;
		  if (Weapon.breathPressed==1 && !IsUnderwater()) AddVoicev(fxBreathOut.length, fxBreathOut.lpData.data(), 256);
		  Weapon.breathPressed = 2;
	  }
  } else if (Weapon.BTime && !IsUnderwater()) {
	  Weapon.BTime -= TimeDt*2;
	  if (Weapon.BTime < 0) Weapon.BTime = 0;
  }
  if (Weapon.HoldBreath) Weapon.breath += 0.05;
  else Weapon.breath -= 0.2;
  if (Weapon.breath > 3.0f) Weapon.breath = 3.0f;
  if (Weapon.breath < 0.f) Weapon.breath = 0.f;

  b = static_cast<float>(sin(static_cast<float>(RealTime) / 300.f)) / 100.f;
  float temp = wptr->shakel -wptr->breath;
  if (temp < 0.2f) temp = 0.2f;
  if (temp > 4.0f) temp = 4.0f;
  wpnDAlpha = temp * static_cast<float>(sin((static_cast<float>(RealTime)) / 300.f+pi/2)) / 200.f;
  wpnDBeta  = temp * static_cast<float>(sin((static_cast<float>(RealTime)) / 300.f)) / 400.f;

  //if (wptr->shakel < 0.2f) wptr->shakel = 0.2f;
  //if (wptr->shakel > 4.0f) wptr->shakel = 4.0f;
  //wpnDAlpha = wptr->shakel * static_cast<float>(sin((static_cast<float>(RealTime)) / 300.f + pi / 2)) / 200.f;
  //wpnDBeta = wptr->shakel * static_cast<float>(sin((static_cast<float>(RealTime)) / 300.f)) / 400.f;

  nv.z = 0;

  //==================== render weapon ===================//


  Vector3d v = Sun3dPos;
  Sun3dPos = RotateVector(Sun3dPos);
  CalcNormals(wptr->chinfo[CurrentWeapon].mptr.get(), wptr->normals.get());


  if (GOUR)
    CalcGouraud(wptr->chinfo[CurrentWeapon].mptr.get(), wptr->normals.get());
  else
    for (int c=0; c<1000; c++)
      wptr->chinfo[CurrentWeapon].mptr->VLight[0][c] = 0;

  if (HARD3D) wpnlight = 96 + GetLandLt(PlayerX, PlayerZ) / 4;
  else wpnlight = 200;

  {
    // Keep the near-model (weapon viewmodel, muzzle flash) projection
    // anchored to the classic 4:3 FOV so viewmodels do not shrink or
    // drift when the world FOV changes. In optic mode, also scale by
    // the aspect ratio so the scope overlay fills the widescreen width.
    // C1 has the same logic in InsertModelList.
    float savedCW = CameraW;
    float savedCH = CameraH;
    float opticScale = nearModelScale;
    if (g_GameMode == GameMode::OpticScope) {
      float arScale = static_cast<float>(WinW) / (static_cast<float>(WinH) * 1.3333333f);
      if (arScale > 1.0f) opticScale *= arScale;
    }
    CameraW *= opticScale;
    CameraH *= opticScale;

    if (Muzz && !IsUnderwater()) {
    CreateMorphedModelBetaGamma(MuzzModel.mptr.get(),
	    &MuzzModel.Animation[0], MuzzFTime, 1.0, 0, MuzzGamma);
    RenderNearModel(MuzzModel.mptr.get(), 0, wpshy, wpshz, wpnlight,
	    -wpnDAlpha, -wpnDBeta + wpnb);
    }

    RenderNearModel(wptr->chinfo[CurrentWeapon].mptr.get(), 0, wpshy, wpshz, wpnlight,
                    -wpnDAlpha, -wpnDBeta + wpnb);

    CameraW = savedCW;
    CameraH = savedCH;
  }


#ifdef _soft
#else
  if (PHONG)
  {
    CalcPhongMapping(wptr->chinfo[CurrentWeapon].mptr.get(), wptr->normals.get());
    RenderModelClipPhongMap(wptr->chinfo[CurrentWeapon].mptr.get(), 0, wpshy, wpshz, -wpnDAlpha, -wpnDBeta+wpnb);
  }

  if (ENVMAP)
  {
    CalcEnvMapping(wptr->chinfo[CurrentWeapon].mptr.get(), wptr->normals.get());
    RenderModelClipEnvMap(wptr->chinfo[CurrentWeapon].mptr.get(), 0, wpshy, wpshz, -wpnDAlpha, -wpnDBeta+wpnb);
  }
#endif

  Sun3dPos = v;


  //Render_Cross(VideoCX, VideoCY);
  if ((!WeapInfo[CurrentWeapon].Optic || g_GameMode == GameMode::OpticScope) && WeapInfo[CurrentWeapon].cross
	  && (!WeapInfo[CurrentWeapon].unzoom || Weapon.state == 2))
	  DrawOpticCross(wptr->chinfo[CurrentWeapon].mptr->VCount-1);

SKIPWEAPON:

  if (ChCallTime)
  {
    ChCallTime-=TimeDt;
    if (ChCallTime<0) ChCallTime=0;
    DrawPicture(WinW - 10 - MenuDinoInfo[TargetCall-10].CallIcon.W, 7,
		MenuDinoInfo[TargetCall-10].CallIcon);
  }

  Hardware_ZBuffer(true);

  if (Weapon.state && MyHealth)
  {
    int y0 = 5;
    if (g_GameMode != GameMode::SurvivalMode)
    {

		
		/*
		if (WeapInfo[w].Reload || WeapInfo[w].rldAnim < 0) {
					Chambered[w] = WeapInfo[w].Reload;
		*/

      const float uiscale = static_cast<float>(WinH) / 600.0f * UIScale;

		int ind = 9;
		int ch = 1;
		if (WeapInfo[CurrentWeapon].Reload) ch = WeapInfo[CurrentWeapon].Reload;
		int y1 = 5;
		int y2 = Weapon.BulletPic[CurrentWeapon].H + 9;
		int x1 = 0;
		int x2 = 0;
      const int bulletW = MAX(1, static_cast<int>((Weapon.BulletPic[CurrentWeapon].W * uiscale)));
      const int bulletH = MAX(1, static_cast<int>((Weapon.BulletPic[CurrentWeapon].H * uiscale)));
      const int chamberW = MAX(1, static_cast<int>((Weapon.ChambPic[CurrentWeapon].W * uiscale)));
      const int chamberH = MAX(1, static_cast<int>((Weapon.ChambPic[CurrentWeapon].H * uiscale)));
      const int hudGap = static_cast<int>((3.0f * uiscale));
      y0 = static_cast<int>((5.0f * uiscale));
      y1 = y0;
      y2 = static_cast<int>(((Weapon.BulletPic[CurrentWeapon].H + 9.0f) * uiscale));
      ind = static_cast<int>((9.0f * uiscale));

		if (wptr->state == 4 || wptr->state == 5) {
			float d = -cos(pi/2+(pi/2 * (static_cast<float>(wptr->FTime) / static_cast<float>(wptr->chinfo[CurrentWeapon].Animation[phas].AniTime))));
			if (WeapInfo[CurrentWeapon].Reload) {
				x1 -= d * bulletW * wptr->ammoIn;
				//x2 -= d * ((Weapon.BulletPic[CurrentWeapon].W * wptr->ammoIn) + 3);
				x2 -= d * ((bulletW * (WeapInfo[CurrentWeapon].Reload - Chambered[CurrentWeapon])) + hudGap);
			} else {
				d *= (y2 - y1);
				y1 += d;
				y2 -= d;
			}
		}
		if (!WeapInfo[CurrentWeapon].Reload)
		if ((wptr->state == 2 && !WeapInfo[CurrentWeapon].mustPump) || wptr->state == 6) {
			float d = (static_cast<float>(wptr->FTime) / static_cast<float>(wptr->chinfo[CurrentWeapon].Animation[phas].AniTime));
			d = 0.5*(1 - cos(pi * (static_cast<float>(wptr->FTime) / static_cast<float>(wptr->chinfo[CurrentWeapon].Animation[phas].AniTime))));
			wptr->ammoIn = 1;
			x1 -= d * bulletW * wptr->ammoIn;
			x2 -= d * ((bulletW * wptr->ammoIn) + hudGap);
		}

		if (WeapInfo[CurrentWeapon].picch)
			DrawScaledPicture(static_cast<int>((5.0f * uiscale)),
				(y0 - static_cast<int>(uiscale)) + (bulletH - (chamberH - 2 * static_cast<int>(uiscale))),
				chamberW, chamberH,
				Weapon.ChambPic[CurrentWeapon]);

		if (wptr->FlashP) {
			wptr->FlashP++;
			if (wptr->FlashP > 4)wptr->FlashP = 0;
			else DrawFlash(static_cast<int>((6.0f * uiscale)) + Chambered[CurrentWeapon] * bulletW,
					y0,
					bulletW,
					bulletH,
					wptr->Flash[wptr->FlashP - 1]
				);
		}

		for (int bl = 0; bl < Chambered[CurrentWeapon]; bl++)
			DrawScaledPicture(static_cast<int>((6.0f * uiscale)) + bl * bulletW, y0, bulletW, bulletH, Weapon.BulletPic[CurrentWeapon]);

		for (int bl = 0; bl < ShotsLeft[CurrentWeapon]; bl++) {
			if (bl < wptr->ammoIn) DrawScaledPicture(ind + x2 + ch * bulletW + bl * bulletW, y1, bulletW, bulletH, Weapon.BulletPic[CurrentWeapon]);
			else DrawScaledPicture(ind + x1 + ch * bulletW + bl * bulletW, y1, bulletW, bulletH, Weapon.BulletPic[CurrentWeapon]);
		}

	  if (AmmoMag[CurrentWeapon])
		  for (int bl=0; bl< MagShotsLeft[CurrentWeapon]; bl++)
			  DrawScaledPicture(ind + ch * bulletW + bl*bulletW, y2, bulletW, bulletH, Weapon.BulletPic[CurrentWeapon]);
	}
  }


  if (g_GameMode == GameMode::TrophyMode)
  {
    const float uiscale = static_cast<float>(WinH) / 600.0f * UIScale;
    DrawScaledPicture(VideoCX - static_cast<int>((TrophyExit.W * uiscale)) / 2, 2,
      static_cast<int>((TrophyExit.W * uiscale)), static_cast<int>((TrophyExit.H * uiscale)), TrophyExit);
  }

  if (g_GameMode == GameMode::ExitCountdown) {
	  const float uiscale = static_cast<float>(WinH) / 600.0f * UIScale;
	  const int exitW = static_cast<int>((ExitPic.W * uiscale));
	  const int exitH = static_cast<int>((ExitPic.H * uiscale));
	  DrawScaledPicture((WinW - exitW) / 2, (WinH - exitH) / 2, exitW, exitH, ExitPic);
	  if (g_GameMode == GameMode::SurvivalMode) {
		  DrawSurvivalText(
			  (WinW - exitW) / 2,
			  (WinH - exitH) / 2
		  );
	  }
  }

  if (IsPaused())
  {
    const float uiscale = static_cast<float>(WinH) / 600.0f * UIScale;
    DrawScaledPicture((WinW - static_cast<int>((PausePic.W * uiscale))) / 2,
      (WinH - static_cast<int>((PausePic.H * uiscale))) / 2,
      static_cast<int>((PausePic.W * uiscale)), static_cast<int>((PausePic.H * uiscale)), PausePic);
  }

  if (ScoreDispTime) {

	  const float uiscale = static_cast<float>(WinH) / 600.0f * UIScale;
	  const int scoreW = static_cast<int>((ScorePic.W * uiscale));
	  const int scoreH = static_cast<int>((ScorePic.H * uiscale));
	  int x0 = VideoCX - scoreW /2;
	  int y0 = WinH - scoreH - static_cast<int>((12.0f * uiscale));
	  DrawScaledPicture(x0, y0, scoreW, scoreH, ScorePic);
	  DrawScoreText(x0, y0);

	  if (ScoreDispTime)
	  {
		  ScoreDispTime -= TimeDt;
		  if (ScoreDispTime < 0)
			  ScoreDispTime = 0;
	  }

  } else {
	  if (g_GameMode == GameMode::TrophyMode || TrophyDisplay)
		  if (TrophyBody != -1 || TrophyDisplay)
		  {
			  const float uiscale = static_cast<float>(WinH) / 600.0f * UIScale;
			  TPicture *Pic = &TrophyPic;
			  if (g_GameMode != GameMode::TrophyMode && (Tranq || Characters[TrophyDisplayC].claimed)) {
				  Pic = &TrophyNoCollectPic;
			  }
			  const int trophyW = static_cast<int>((Pic->W * uiscale));
			  const int trophyH = static_cast<int>((Pic->H * uiscale));
			  int x0 = WinW - trophyW - static_cast<int>((16.0f * uiscale));
			  int y0 = WinH - trophyH - static_cast<int>((12.0f * uiscale));
			  if (g_GameMode != GameMode::TrophyMode)
				  x0 = VideoCX - trophyW / 2;

			  DrawScaledPicture(x0, y0, trophyW, trophyH, *Pic);
			  DrawTrophyText(x0, y0);

		  }
  }

#ifdef GL_PERF_HOOKS
  PerfFrameEnd();
#endif
}




















LONG APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
  BOOL A = (GetActiveWindow() == hWnd);

  if (A!=blActive)
  {
    blActive = A;

    if (blActive) SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    else SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS);

    if (!blActive)
    {
      CaptureMouse(false);
      ShutDown3DHardware();
      NeedRVM = true;
    }

    if (blActive)
    {
      Audio_Restore();
      NeedRVM = true;
      if (_GameState && !IsPaused()) CaptureMouse(true);
    }

  }

  if (message == WM_KEYDOWN)
  {
    if (static_cast<int>(wParam) == KeyMap.fkBinoc && g_GameMode != GameMode::SurvivalMode) ToggleBinocular();
    if (static_cast<int>(wParam) == KeyMap.fkCCall && g_GameMode != GameMode::SurvivalMode) ChangeCall();
    if (static_cast<int>(wParam) == KeyMap.fkRun  && g_GameMode != GameMode::SurvivalMode) ToggleRunMode();
	if (static_cast<int>(wParam) == KeyMap.fkCrouch && g_GameMode != GameMode::SurvivalMode) ToggleCrouchMode();
    if (static_cast<int>(wParam) == NightVisionKey && NightVisionMode) {
      NightVisionOn = !NightVisionOn;
      g_GameMode = NightVisionOn ? GameMode::NightVision : GameMode::Normal;
      if (NightVisionOn)
        AddMessage("Night vision ON");
      else
        AddMessage("Night vision OFF");
    }
    if (static_cast<int>(wParam) == cheatcode[cheati] && g_GameMode != GameMode::SurvivalMode)
    {
      cheati++;
      if (cheati>6)
      {
        cheati=0;
        SwitchMode("Debug mode",DEBUG);
      }
    }
    else cheati=0;
  }


  switch (message)
  {
  case WM_CREATE:
    return 0;

  case WM_SYSKEYDOWN:
    if (static_cast<int>(wParam) == VK_RETURN && g_GameMode != GameMode::SurvivalMode) {
      SetFullScreen();
      return 0;
    }
    break;


  case WM_KEYDOWN:
  {
    BOOL CTRL = (GetKeyState(VK_SHIFT) & 0x8000);
    switch( static_cast<int>(wParam) )
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    {
		if (g_GameMode == GameMode::SurvivalMode) break;
      if (Weapon.FTime) break;
      int w;
      if (wParam == '0')
        w = 9;
      else
        w = (static_cast<int>(wParam) - '1');
      if (!Chambered[w] && !ShotsLeft[w] && !AmmoMag[w])
      {
        AddMessage("No weapon");
        break;
      }
      TargetWeapon = w;
	  if (!IsUnderwater() || WeapInfo[TargetWeapon].harpoon) {
		  if (!Weapon.state)
			  CurrentWeapon = TargetWeapon;
		  HideWeapon();
	  }
      break;
    }

    case 'U':
      if (DEBUG) ChangeViewR(0, 0, -2);
      break;
    case 'I':
      if (DEBUG) ChangeViewR(0, 0, +2);
      break;
    case 'O':
      if (DEBUG) ChangeViewR(0, -2, 0);
      break;
    //case 'P': if (DEBUG) ChangeViewR(0, +2, 0); break;
    case 219:
      if (DEBUG) ChangeViewR(-2, 0, 0);
      break;
    case 221:
      if (DEBUG) ChangeViewR(+2, 0, 0);
      break;

    /*
    case '0': wpshy=0; wpshz=0; wpnb=0; break;
    case '7': if (CTRL) wpshy-=0.25; else wpshy+=0.25;
           ShowShifts();
           break;
    case '8': if (CTRL) wpshz-=0.25; else wpshz+=0.25;
           ShowShifts();
           break;
    case '9': if (CTRL) wpnb-=0.005; else wpnb+=0.005;
           ShowShifts();
           break;*/


    case 'S':
      if (DEBUG && CTRL) SwitchMode("Slow mode",SLOW);
      break;
    case 'T':
      if (DEBUG && CTRL) SwitchMode("Timer",TIMER);
      break;


    case 'M':
      if (CTRL) SwitchMode("Draw 3D models",MODELS);
      break;
    case 'F':
      if (CTRL) SwitchMode("V.Fog",FOGENABLE);
      break;
    case 'L':
      if (CTRL) SwitchMode("Fly",FLY);
      break;
    case 'C':
      if (CTRL) SwitchMode("Clouds shadow",Clouds);
      break;

    case 'E':
      if (CTRL) SwitchMode("Env.Mapping",ENVMAP);
      break;
    case 'G':
      if (CTRL) SwitchMode("Gour.Mapping",GOUR);
      break;
    case 'P':
		if (!CTRL) { if (DEBUG) ChangeViewR(0, +2, 0); } else  SwitchMode("Phong Mapping", PHONG);
		break;

//	case VK_UP:

//	case VK_RIGHT:

//	case VK_LEFT:

//	case VK_DOWN:

    case VK_TAB:
      if (g_GameMode != GameMode::TrophyMode) ToggleMapMode();
      break;

    case VK_PAUSE:
		if (g_GameMode != GameMode::SurvivalMode) {
      if (IsPaused()) {
        g_GameMode = GameMode::Normal;
        CaptureMouse(true);
      } else {
        g_GameMode = GameMode::Paused;
        CaptureMouse(false);
      }
      ResetMousePos();
      break;
		}

    case 'N':
      if (g_GameMode == GameMode::ExitCountdown) g_GameMode = GameMode::Normal;
      break;

    case VK_ESCAPE:
      if (g_GameMode == GameMode::TrophyMode || g_GameMode == GameMode::SurvivalMode)
      {
        SaveTrophy();
        ExitTime = 1;
      }
      else
      {
        if (IsPaused()) { g_GameMode = GameMode::Normal; CaptureMouse(true); }
        else { g_GameMode = (g_GameMode == GameMode::ExitCountdown) ? GameMode::Normal : GameMode::ExitCountdown; CaptureMouse(true); }
        if (ExitTime) g_GameMode = GameMode::Normal;
        ResetMousePos();
      }
      break;

    case 'Y':
		if (g_GameMode == GameMode::ExitCountdown && g_GameMode != GameMode::SurvivalMode)
		{
			if (MyHealth) ExitTime = 4000;
			else ExitTime = 1;
			g_GameMode = GameMode::Normal;
		}
		break;

    case VK_RETURN:
      if (g_GameMode == GameMode::ExitCountdown )
      {
		if (MyHealth && g_GameMode != GameMode::SurvivalMode) ExitTime = 4000;
        else ExitTime = 1;
        g_GameMode = GameMode::Normal;
      }
      break;

	case 'Q':
		if (g_GameMode == GameMode::ExitCountdown && g_GameMode == GameMode::SurvivalMode)
		{
			ExitTime = 1;
			g_GameMode = GameMode::Normal;
		}
		break;

    case 'R':
	if (TrophyBody!=-1) RemoveCurrentTrophy();
      if (g_GameMode == GameMode::ExitCountdown)
      {
		  if (g_GameMode == GameMode::SurvivalMode) {
			  SurvivalWave = 0;
			  ChCount = 0;
		  }
		  else LoadTrophy();
        RestartMode = true;
		
        _GameState = 0;
        //DoHalt("");
      }
      break;

    case VK_F9:
      ShutDown3DHardware();
      AudioStop();
      DoHalt("");
      break;

    case VK_F12:
      SaveScreenShot();
      break;

#ifdef GL_PERF_HOOKS
    case VK_F11:
      PerfTriggerCapture();
      break;
#endif

    }   // switch
    break;
  }

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  // WM_PAINT / WM_ERASEBKGND: the game loop renders every frame via
  // ShowVideo() and SwapBuffers, so the WndProc must NOT let
  // DefWindowProc paint anything. If it does, DefWindowProc fills the
  // window with the background brush (black by default), which
  // manifests as a black screen after any event that generates
  // WM_PAINT — Alt-tabbing back, uncovering the window, and crucially
  // for the OpenGL renderer, external screenshot tools that send
  // WM_PRINT -> WM_PRINTCLIENT -> WM_PAINT.
  //
  // BeginPaint/EndPaint validates the update region without painting,
  // which is the standard idiom for windows rendered by a separate
  // API (OpenGL, Direct3D, etc.). Returning 1 from WM_ERASEBKGND tells
  // Windows the background is already erased (it isn't, but the next
  // SwapBuffers will overwrite it).
  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    return 0;
  }
  case WM_ERASEBKGND:
    return 1;

  // WM_PRINT / WM_PRINTCLIENT: PrintWindow() sends these to capture
  // window content. Returning TRUE tells the caller we've handled it;
  // the caller then falls back to whatever is currently in the window
  // (DWM-composited SwapBuffers output for the GL renderer, swap
  // chain present for D3D). Without this, DefWindowProc paints the
  // background brush over the GPU content, producing a black capture.
  case WM_PRINT:
  case WM_PRINTCLIENT:
    return 1;

  default:
    return (DefWindowProc(hWnd, message, wParam, lParam));
  }
  return 0;
}




BOOL CreateMainWindow()
{
  PrintLog("Creating main window...");
  WNDCLASS wc;
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = (WNDPROC)MainWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInst;
  wc.hIcon = wc.hIcon = (HICON)LoadIcon(hInst,"ACTION");
  wc.hCursor = nullptr;
  wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
  wc.lpszMenuName = nullptr;
  //wc.lpfnWndProc  = nullptr;
  wc.lpszClassName = "HuntRenderWindow";
  if (!RegisterClass(&wc)) return false;

  hwndMain = CreateWindow(
               "HuntRenderWindow","Carnivores 2 Renderer",
               WS_VISIBLE |  WS_POPUP,
               0, 0, 0, 0, nullptr,  nullptr, hInst, nullptr );

  if (hwndMain)
    PrintLog("Ok.\n");

  return true;
}






























// FPS limit values matching menu indexes: 0=Unlimited, 1=60, 2=120, 3=240
static const int kFpsValues[] = { 0, 60, 120, 240 };

static void LimitFPS()
{
	int targetFps = kFpsValues[OptFpsLimit];
	if (targetFps <= 0) return;

	static LARGE_INTEGER freq = { 0 };
	static LARGE_INTEGER frameStart = { 0 };
	static bool init = false;

	if (!init) {
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&frameStart);
		timeBeginPeriod(1);
		init = true;
	}

	INT64 target_us = 1000000 / targetFps;
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	INT64 elapsed_us = (now.QuadPart - frameStart.QuadPart) * 1000000 / freq.QuadPart;

	while (elapsed_us < target_us) {
		if (target_us - elapsed_us > 2000)
			Sleep(1);
		QueryPerformanceCounter(&now);
		elapsed_us = (now.QuadPart - frameStart.QuadPart) * 1000000 / freq.QuadPart;
	}

	QueryPerformanceCounter(&frameStart);
}

void ProcessGame()
{
  if (RestartMode)
  {
    ShutDown3DHardware();
    AudioStop();
    NeedRVM = true;

  }

  if (!_GameState)
  {

	  /*
	  for (int di = 0; di < DINOINFO_MAX; di++) {
		  DinoInfo[di].trophyLocTotal1 = 0;
		  DinoInfo[di].trophyLocTotal2 = 0;
	  }

	  char Buff[100];
	  for (int c = 0; c < TROPHY2_COUNT; c++) {
		  sprintf(Buff, "SETTING LOC TOTAL INC %i", c);
		  PrintLog(Buff);
		  sprintf(Buff, " = %i\n", TrophyIndex[c]);
		  PrintLog(Buff);
		  DinoInfo[TrophyIndex[c]].trophyLocTotal1++;
		  DinoInfo[TrophyIndex[c]].trophyLocTotal2++;
	  }
	  */

    PrintLog("Entered game\n");
    ReInitGame();
    CaptureMouse(true);

	if (Multiplayer) {
		if (!_MultiplayerState) {

			if (Host) {
				StartupServerCommsThread();
			} else {
				StartupClientCommsThread();
			}

		}
		_MultiplayerState = 1;
	}


//test
	/*
	long long_data = PlayerX;

	char printable2[25];
	_itoa(long_data, printable2, 10);
	PrintLog("SENDINGVALUE:");
	PrintLog(printable2);
	PrintLog("\n");

	byte tdata[4];
	tdata[0] = static_cast<int>(((long_data >> 24) & 0xFF));
	tdata[1] = static_cast<int>(((long_data >> 16) & 0xFF));
	tdata[2] = static_cast<int>(((long_data >> 8) & 0XFF));
	tdata[3] = static_cast<int>(((long_data & 0XFF)));

	const char *p = reinterpret_cast<const char*>(tdata);
	const byte *tdata2 = reinterpret_cast<const byte*>(p);

	long anotherLongInt = ((tdata2[0] << 24)
		+ (tdata2[1] << 16)
		+ (tdata2[2] << 8)
		+ (tdata2[3]));

	char printable[25];
	_itoa(anotherLongInt, printable, 10);

	PrintLog("SENTVALUE:");
	PrintLog(printable);
	PrintLog("\n");
	*/



  }

  _GameState = 1;

  if (NeedRVM)
  {
	SetWindowPos(hwndMain, HWND_TOP, 0,0,0,0,  SWP_SHOWWINDOW);
	SetFocus(hwndMain);
	Activate3DHardware();
	NeedRVM = false;
  }

  ProcessSyncro();

  if (!IsPaused() || !MyHealth)
  {
    ProcessControls();
    AudioSetCameraPos(CameraX, CameraY, CameraZ, CameraAlpha, CameraBeta);
    Audio_UploadGeometry();
    AnimateCharacters();
	AnimateBullets();
	if (Multiplayer) {
		AnimateMHunters();
	}
    AnimateProcesses();
  }

  if (DEBUG || ObservMode || g_GameMode == GameMode::TrophyMode)
    if (MyHealth) MyHealth = MAX_HEALTH;
  if (DEBUG) ShotsLeft[CurrentWeapon] = WeapInfo[CurrentWeapon].Shots;

  // Phase 2.1: build per-frame render context and pass to the renderer.
  // DrawFrame replaces DrawScene() for GL and Soft — it calls
  // PreCashGroundModel first, then renders ground, water, sky, models,
  // and shadows via the renderer. Other renderers use the free-function
  // DrawScene().
  RenderFrameContext ctx = RenderFrameContext::FromGlobals();
#ifdef _gl
  if (g_GLRenderer) g_GLRenderer->DrawFrame(ctx);
#elif defined(_soft)
  if (g_SoftRenderer) g_SoftRenderer->DrawFrame(ctx);
#else
  DrawScene();
#endif

  if (g_GameMode != GameMode::TrophyMode)
    if (g_GameMode == GameMode::MapMode) DrawHMap();

  DrawPostObjects();

  ShowControlElements();

  ShowVideo();
}



int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpszCmdLine, int nCmdShow)
{
	
	MSG msg;

  hInst = hInstance;
  EnablePerMonitorV2DpiAwareness();

  CreateLog();

  CreateMainWindow();

  Init3DHardware();
  InitEngine();
  InitAudioSystem(hwndMain, hlog, OptSound);

  StartLoading();
  PrintLoad("Loading...");

  PrintLog("== Loading resources ==\n");
  hcArrow = LoadCursor(nullptr, IDC_ARROW);


  PrintLog("Loading common resources:");
  PrintLoad("Loading common resources...");

  if (OptDayNight==2)
    LoadModelEx(SunModel,    "HUNTDAT\\MOON.3DF");
  else
    LoadModelEx(SunModel,    "HUNTDAT\\SUN2.3DF");
  LoadModelEx(CompasModel, "HUNTDAT\\COMPAS.3DF");
  LoadModelEx(Binocular,   "HUNTDAT\\BINOCUL.3DF");

  LoadCharacterInfo(WCircleModel, "HUNTDAT\\WCIRCLE2.CAR");
  LoadCharacterInfo(ShipModel, "HUNTDAT\\ship2a.car");
  LoadCharacterInfo(SShipModel, "HUNTDAT\\sship.car");
  LoadCharacterInfo(BagModel, "HUNTDAT\\bag1.car");
  LoadCharacterInfo(WindModel, "HUNTDAT\\WIND.CAR");

  LoadCharacterInfo(MuzzModel, "HUNTDAT\\MUZZ4.CAR");

  LoadWav("HUNTDAT\\SOUNDFX\\a_underw.wav",  fxUnderwater);

  LoadWav("HUNTDAT\\SOUNDFX\\blip.wav", fxBlip);

  LoadWav("HUNTDAT\\SOUNDFX\\click1.wav", fxClick[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\click2.wav", fxClick[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\click3.wav", fxClick[2]);

  LoadWav("HUNTDAT\\SOUNDFX\\breath1.wav", fxBreathIn);
  LoadWav("HUNTDAT\\SOUNDFX\\breath2.wav", fxBreathOut);

  LoadWav("HUNTDAT\\SOUNDFX\\collect1.wav", fxCollect[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\collect2.wav", fxCollect[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\collect3.wav", fxCollect[2]);
  
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\aquatic1.wav", fxImpactAquatic[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\aquatic2.wav", fxImpactAquatic[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\aquatic3.wav", fxImpactAquatic[2]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\ground1.wav", fxImpactGround[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\ground2.wav", fxImpactGround[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\ground3.wav", fxImpactGround[2]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\model1.wav", fxImpactModel[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\model2.wav", fxImpactModel[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\model3.wav", fxImpactModel[2]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\water1.wav", fxImpactWater[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\water2.wav", fxImpactWater[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\water3.wav", fxImpactWater[2]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\char1.wav", fxImpactChar[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\char2.wav", fxImpactChar[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\IMPACT\\char3.wav", fxImpactChar[2]);

  LoadWav("HUNTDAT\\SOUNDFX\\STEPS\\hwalk1.wav",  fxStep[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\STEPS\\hwalk2.wav",  fxStep[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\STEPS\\hwalk3.wav",  fxStep[2]);

  LoadWav("HUNTDAT\\SOUNDFX\\STEPS\\footw1.wav",  fxStepW[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\STEPS\\footw2.wav",  fxStepW[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\STEPS\\footw3.wav",  fxStepW[2]);

  LoadWav("HUNTDAT\\SOUNDFX\\hum_die1.wav",  fxScream[0]);
  LoadWav("HUNTDAT\\SOUNDFX\\hum_die2.wav",  fxScream[1]);
  LoadWav("HUNTDAT\\SOUNDFX\\hum_die3.wav",  fxScream[2]);
  LoadWav("HUNTDAT\\SOUNDFX\\hum_die4.wav",  fxScream[3]);

  LoadPictureTGA(PausePic,   "HUNTDAT\\MENU\\pause.tga", MemoryTag::Global);
  conv_pic(PausePic);
  if (g_GameMode == GameMode::SurvivalMode) LoadPictureTGA(ExitPic, "HUNTDAT\\MENU\\exit_s.tga", MemoryTag::Global);
  else LoadPictureTGA(ExitPic,    "HUNTDAT\\MENU\\exit.tga", MemoryTag::Global);
  conv_pic(ExitPic);
  LoadPictureTGA(TrophyExit, "HUNTDAT\\MENU\\trophy_e.tga", MemoryTag::Global);
  conv_pic(TrophyExit);
  LoadPictureTGA(MapPic,     "HUNTDAT\\MENU\\mapframe.tga", MemoryTag::Global);
  conv_pic(MapPic);

  LoadPictureTGA(TFX_ENVMAP,    "HUNTDAT\\FX\\envmap.tga", MemoryTag::Global);
  ApplyAlphaFlags(TFX_ENVMAP.lpImage.get(), TFX_ENVMAP.W*TFX_ENVMAP.W);
  LoadPictureTGA(TFX_SPECULAR,  "HUNTDAT\\FX\\specular.tga", MemoryTag::Global);
  ApplyAlphaFlags(TFX_SPECULAR.lpImage.get(), TFX_SPECULAR.W*TFX_SPECULAR.W);


  PrintLog(" Done.\n");

  PrintLoad("Loading area...");
  LoadResources();

  PrintLoad("Starting game...");
  PrintLog("Loading area: Done.\n");

  EndLoading();

  ProcessSyncro();
  blActive = true;

  alreadyFired = false;

  PrintLog("Entering messages loop.\n");
  for( ; ; ){
    if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
    {
      if (msg.message == WM_QUIT)  break;
      TranslateMessage( &msg );
      DispatchMessage( &msg );
    }
    else
    {
      if (blActive) { ProcessGame(); LimitFPS(); }
      else Sleep(100);
    }
  }

  if (Multiplayer) {
	  if (Host) {
		  ShutDownServer();
	  }
	  else {
		  ShutDownClient();
	  }
  }

  AudioStop();
  Audio_Shutdown();

  ShutDown3DHardware();

  ShutDownEngine();

  ShowCursor(true);
  PrintLog("Game normal shutdown.\n");

  CloseLog();
  return msg.wParam;
}
