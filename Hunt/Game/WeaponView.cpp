// WeaponView.cpp -- the weapon viewmodel/HUD/screen-overlay pass
// (split from Hunt.cpp)
// ==========================================================================
// DrawPostObjects owns the first-person weapon state machine (states 1-7),
// muzzle, viewmodel PHONG/ENVMAP overlays, ammo HUD, call icon and the
// trophy/exit/pause/score overlays.
// ==========================================================================

#include "Hunt.h"

#include <algorithm>
#include <cmath>
#include <stdio.h>

// Flags/toggles defined in Hunt.cpp (shared with WndProc.cpp).
extern BOOL PHONG, GOUR, ENVMAP;

// Near-model overlays were authored for a 4:3 viewport. Scale them far
// enough to cover wider viewports without changing their vertical framing.
static float GetScopeAspectFillScale()
{
  constexpr float kReferenceAspect = 4.0f / 3.0f;
  if (WinH <= 0) return 1.0f;
  return (std::max)(1.0f, (static_cast<float>(WinW) / static_cast<float>(WinH)) / kReferenceAspect);
}

static TAni* RequireCurrentWeaponAnimation(int animationIndex)
{
  TAni* animation = FindWeaponAnimation(Weapon, CurrentWeapon, animationIndex);
  if (animation)
    return animation;

  char message[160];
  sprintf_s(message, sizeof(message),
            "Runtime weapon animation index is invalid (weapon=%d, animation=%d).",
            CurrentWeapon, animationIndex);
  DoHalt(message);
  return nullptr;
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
  // Keep the original sun position available to the label path. Declaring it
  // after the SKIPWEAPON jumps made MSVC warn that its initializer could be
  // skipped; nothing before the assignment below changes Sun3dPos.
  Vector3d v = Sun3dPos;
  // Weapon animation phase for the morph/HUD sections below. Declared above
  // the SKIPWEAPON jumps so no path can skip it; the state switch further
  // down assigns the active animation.
  int phas = WeapInfo[CurrentWeapon].getAnim;

  Hardware_ZBuffer(false);

  if (DemoPoint.DemoTime) goto SKIPWEAPON;

  GlassL = 0;
  // Keep near-model projection anchored to the classic 4:3 FOV so
  // viewmodels and HUD-like near renders do not shrink or drift when
  // the world FOV changes. The aspectScale correction makes binocular
  // and scope overlays fill the widescreen width (C1 has the same
  // logic in InsertModelList).
  float nearModelScale = FovScaleFromDegrees(kFovDefault) / FovScaleFromDegrees(OptFov);
  // Scope-overlay predicates live here so both the wind/compass skip below
  // and the viewmodel scaling later share one definition of "mask up".
  // A breath-aim weapon never raises a mask: plain viewmodel at every
  // magnification, HUD untouched (see the aspect-fill pop note below).
  const bool opticZoomActive =
      IsScopeView() &&
      (!WeapInfo[CurrentWeapon].unzoom || Weapon.state == 2) &&
      ScopePower > 1.01f && !WeapInfo[CurrentWeapon].breathaim;
  // The magnification guard matters: optic = 1.0 (red-dot style sights,
  // no actual zoom) must NOT take the mask treatment -- without it the
  // viewmodel pops ~33% bigger at 16:9 the moment the draw finishes,
  // and wind/compass vanish under a mask that was never drawn.
  const bool embeddedScopeActive =
      WeapInfo[CurrentWeapon].Optic > 1.0f &&
      !WeapInfo[CurrentWeapon].cross && !WeapInfo[CurrentWeapon].breathaim &&
      Weapon.state == 2;
  if (g_GameMode == GameMode::Binocular)
  {
    float oldCW = CameraW;
    float oldCH = CameraH;
    float scale = nearModelScale * GetScopeAspectFillScale();
    CameraW *= scale;
    CameraH *= scale;
    RenderNearModel(Binocular.get(), 0, 0, 2*(216-72 * BinocularPower), 192,  0,0);
    CameraW = oldCW;
    CameraH = oldCH;
    ScanLifeForms();
  }

  //goto SKIPWIND;
  // The wind indicator and compass sit where the sniper's scope mask covers
  // the screen, so they hide only while the mask is actually up (or under
  // the binocular overlay). A breath-aim weapon keeps both at every
  // magnification -- including mid breath-zoom.
  if (g_GameMode == GameMode::Binocular || embeddedScopeActive) goto SKIPWIND;

  if (g_GameMode != GameMode::TrophyMode && g_GameMode != GameMode::SurvivalMode)
    if (!(KeyboardState[VK_CAPITAL] & 1))
    {
      BOOL lr = LOWRESTX;
      LOWRESTX = true;

      const int hudCenter = WinW / 2;
      const int hudSpread = static_cast<int>((static_cast<float>(WinW) / 3.0f * UIScale));
      const int hudBottomInset = static_cast<int>((static_cast<float>(WinH) * 0.012f));
      const int hudY = WinH - (WinH * 10 / 23) - hudBottomInset;

      // Counter-scale the active world zoom: these near-model HUD pieces
      // share the (possibly magnified) CameraW/H projection, so without
      // this they drift half out of frame as the frustum narrows.
      const float hudUnzoom = nearModelScale / ActiveWorldZoom();
      VideoCX = hudCenter - hudSpread;
      VideoCY = hudY;
      CreateMorphedModel(WindModel.mptr.get(), &WindModel.Animation[0], static_cast<int>((Wind.speed*50.f)), 1.0);
      {
        const float savedCW = CameraW;
        const float savedCH = CameraH;
        CameraW *= hudUnzoom;
        CameraH *= hudUnzoom;
        RenderNearModel(WindModel.mptr.get(), -10, -37, -96, 192,  CameraAlpha-Wind.alpha,0);
        CameraW = savedCW;
        CameraH = savedCH;
      }

      VideoCX = hudCenter + hudSpread;
      VideoCY = hudY;
      {
        const float savedCW = CameraW;
        const float savedCH = CameraH;
        CameraW *= hudUnzoom;
        CameraH *= hudUnzoom;
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

  // Drawing or holding a raised weapon closes the area map (original MEE
  // behaviour: the old engine wrote MapMode = FALSE here every frame the
  // weapon was active; the GameMode-enum refactor dropped it, leaving the
  // map stuck open over a drawn weapon until the player lowered it).
  if (g_GameMode == GameMode::MapMode) g_GameMode = GameMode::Normal;

  if (g_GameMode != GameMode::SurvivalMode) {
	  float tempT = static_cast<float>(TimeDt) / 10000.f;
	  wptr->shakel += tempT;
	  if (wptr->shakel > 4.0f) wptr->shakel = 4.0f;
  }

  // Underwater halves the viewmodel animation rate; compute once per frame.
  const float frameAdvance = IsUnderwater() ? TimeDt / 2.f : static_cast<float>(TimeDt);

  if (wptr->state == 1)
  {
	  wptr->FTime += frameAdvance;
    if (wptr->FTime >= RequireCurrentWeaponAnimation(WeapInfo[CurrentWeapon].getAnim)->AniTime)
    {
      wptr->FTime = 0;
      wptr->state = 2;
    }
  }

  if (wptr->state == 4)
  {
	  wptr->FTime += frameAdvance;
	  if (wptr->FTime >= RequireCurrentWeaponAnimation(WeapInfo[CurrentWeapon].rldAnim)->AniTime)
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
	  wptr->FTime += frameAdvance;
	  if (wptr->FTime >= RequireCurrentWeaponAnimation(WeapInfo[CurrentWeapon].rldAnimPart)->AniTime)
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
	  wptr->FTime += frameAdvance;
	  if (Muzz && !IsUnderwater()) {
		if (wptr->FTime > MuzzModel.Animation[0].AniTime) {
			Muzz = false;
			MuzzFTime = 0;
		} else MuzzFTime = wptr->FTime;
	}

    if (wptr->FTime >= RequireCurrentWeaponAnimation(WeapInfo[CurrentWeapon].shtAnim)->AniTime)
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
	  wptr->FTime += frameAdvance;
	  if (wptr->FTime >= RequireCurrentWeaponAnimation(WeapInfo[CurrentWeapon].pmpAnim)->AniTime)
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
	  wptr->FTime += frameAdvance;
	  if (wptr->FTime >= RequireCurrentWeaponAnimation(WeapInfo[CurrentWeapon].modAnim)->AniTime)
	  {
		  if (!FiringMode[CurrentWeapon]) FiringMode[CurrentWeapon] = 1; else FiringMode[CurrentWeapon] = 0;
		  wptr->FTime = 0;
		  wptr->state = 2;
	  }
  }

  if (wptr->state == 3)
  {
	  wptr->FTime += frameAdvance;
    if (wptr->FTime >= RequireCurrentWeaponAnimation(WeapInfo[CurrentWeapon].putAnim)->AniTime)
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
                     RequireCurrentWeaponAnimation(phas), wptr->FTime, 1.0);

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

  nv.z = 0;

  //==================== render weapon ===================//

  Sun3dPos = RotateVector(Sun3dPos);
  CalcNormals(wptr->chinfo[CurrentWeapon].mptr.get(), wptr->normals.get());

  if (GOUR)
    CalcGouraud(wptr->chinfo[CurrentWeapon].mptr.get(), wptr->normals.get());
  else
    for (int c=0; c<1000; c++)
      wptr->chinfo[CurrentWeapon].mptr->VLight[0][c] = 0;

  if (HARD3D) wpnlight = 96 + GetLandLt(PlayerX, PlayerZ) / 4;
  else wpnlight = 200;

  float weaponOverlayScale = nearModelScale;
  // A breath-aim weapon keeps a normal viewmodel at every magnification:
  // only the world zooms, so the gun never pops when the breath ease
  // crosses 1x (the aspect-fill below would snap it ~33% bigger at 16:9
  // while the world still sits near 1x). Scoped weapons keep the mask
  // aspect-fill; their magnification never crosses the 1x boundary.
  if (opticZoomActive || embeddedScopeActive) {
    // The sniper mask is embedded in the weapon's firing animation rather
    // than drawn as a separate HUD texture.  Some game-state paths can leave
    // that animation visible after OpticScope mode has been cleared; in that
    // case CameraW/H no longer contain the weapon's optic magnification and
    // the legacy 800x600 mask appears as a small rectangle.  Supply the
    // missing magnification here, then aspect-fill the 4:3-authored mask.
    if (!opticZoomActive)
      weaponOverlayScale *= ScopePower;
    weaponOverlayScale *= GetScopeAspectFillScale();
  }

  {
    // Keep the near-model (weapon viewmodel, muzzle flash) projection
    // anchored to the classic 4:3 FOV so viewmodels do not shrink or
    // drift when the world FOV changes. In optic mode, also scale by
    // the aspect ratio so the scope overlay fills the widescreen width.
    // C1 has the same logic in InsertModelList.
    float savedCW = CameraW;
    float savedCH = CameraH;
    CameraW *= weaponOverlayScale;
    CameraH *= weaponOverlayScale;

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
  if ((!WeapInfo[CurrentWeapon].Optic || IsScopeView()) && WeapInfo[CurrentWeapon].cross
      && (!WeapInfo[CurrentWeapon].unzoom || Weapon.state == 2)) {
    // rVertex was generated with the near-model projection. Reapply that
    // projection while converting its reticle anchor to screen coordinates.
    const float savedCW = CameraW;
    const float savedCH = CameraH;
    CameraW *= weaponOverlayScale;
    CameraH *= weaponOverlayScale;
    DrawOpticCross(wptr->chinfo[CurrentWeapon].mptr->VCount - 1);
    CameraW = savedCW;
    CameraH = savedCH;
  }

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
    // The counter follows the weapon's visibility: hide it for the whole
    // put-away animation (state 3), not only once it completes and the
    // state reaches 0. It previously stayed up for the full holster
    // duration -- after the gun had already left the screen and, on scoped
    // weapons, after the FOV had already snapped back (user report:
    // "sometimes when the weapon is put away the ammo counter still
    // remains instead of being hidden"). Stock holster animations run
    // 323-700 ms (jager2/sniper/x-bow putaway), so the leftover window
    // was clearly visible on every weapon.
    if (g_GameMode != GameMode::SurvivalMode && wptr->state != 3)
    {
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
			const TAni* animation = FindWeaponAnimation(*wptr, CurrentWeapon, phas);
			if (animation && animation->AniTime > 0) {
				float d = -cos(pi/2+(pi/2 * (static_cast<float>(wptr->FTime) / static_cast<float>(animation->AniTime))));
				if (WeapInfo[CurrentWeapon].Reload) {
					x1 -= d * bulletW * wptr->ammoIn;
					x2 -= d * ((bulletW * (WeapInfo[CurrentWeapon].Reload - Chambered[CurrentWeapon])) + hudGap);
				} else {
					d *= (y2 - y1);
					y1 += d;
					y2 -= d;
				}
			}
		}
		if (!WeapInfo[CurrentWeapon].Reload)
		if ((wptr->state == 2 && !WeapInfo[CurrentWeapon].mustPump) || wptr->state == 6) {
			const TAni* animation = FindWeaponAnimation(*wptr, CurrentWeapon, phas);
			if (animation && animation->AniTime > 0) {
				float d = (static_cast<float>(wptr->FTime) / static_cast<float>(animation->AniTime));
				d = 0.5*(1 - cos(pi * (static_cast<float>(wptr->FTime) / static_cast<float>(animation->AniTime))));
				wptr->ammoIn = 1;
				x1 -= d * bulletW * wptr->ammoIn;
				x2 -= d * ((bulletW * wptr->ammoIn) + hudGap);
			}
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

  if (InTrophyRoom())
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
	  if (InTrophyRoom() || TrophyDisplay)
		  if (TrophyBody != -1 || TrophyDisplay)
		  {
			  const float uiscale = static_cast<float>(WinH) / 600.0f * UIScale;
			  TPicture *Pic = &TrophyPic;
			  if (!InTrophyRoom() && (Tranq || Characters[TrophyDisplayC].claimed)) {
				  Pic = &TrophyNoCollectPic;
			  }
			  const int trophyW = static_cast<int>((Pic->W * uiscale));
			  const int trophyH = static_cast<int>((Pic->H * uiscale));
			  int x0 = WinW - trophyW - static_cast<int>((16.0f * uiscale));
			  int y0 = WinH - trophyH - static_cast<int>((12.0f * uiscale));
			  if (!InTrophyRoom())
				  x0 = VideoCX - trophyW / 2;

			  DrawScaledPicture(x0, y0, trophyW, trophyH, *Pic);
			  DrawTrophyText(x0, y0);

		  }
  }

}
