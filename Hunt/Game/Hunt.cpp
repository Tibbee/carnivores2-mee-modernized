#include "Hunt.h"
#include "WndProc.h"
#include "Core/DpiAwareness.h"
#include "Core/FrameLimiter.h"
#include "Core/KeyBindings.h"
#include "Core/TerrainFog.h"
#include "Core/UnattendedCapture.h"
#include "Network/NetworkManager.h"
#include "Workbench/TuningRuntime.h"
#include "stdio.h"
#ifdef _gl
#include "Renderer/GLUtils.h"
#include "Workbench/WorkbenchUI.h"
#endif
#include <cmath>
#include <algorithm>

// Near-model overlays were authored for a 4:3 viewport. Scale them far
// enough to cover wider viewports without changing their vertical framing.
static float GetScopeAspectFillScale()
{
  constexpr float kReferenceAspect = 4.0f / 3.0f;
  if (WinH <= 0) return 1.0f;
  return (std::max)(1.0f, (static_cast<float>(WinW) / static_cast<float>(WinH)) / kReferenceAspect);
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







float CalcFogLevel(Vector3d v, int cachedFogIndex)
{
  if (!FOGON) return 0;
  BOOL vinfog = true;
  int cf;
  if (!IsUnderwater())
  {
    cf = cachedFogIndex >= 0
      ? cachedFogIndex
      : FogsMap[ (static_cast<int>((v.z + CameraZ)))>>9 ][ (static_cast<int>((v.x + CameraX)))>>9 ];
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

#ifdef _gl
  // Sun-fog colour shift -- modulate fog colour by sun elevation
  // and cloud visibility.  Delegated to ApplySunFogColourShift() so the
  // SAME shift reaches BOTH the terrain fog colour (via
  // GetFogColorForMapPoint, which now calls it) and the model/water
  // fog colour (via CurFogColor here).  Applied once per fog volume
  // (uniform across all vertices in this volume), NOT per-vertex, so it
  // doesn't create per-vertex colour variation.  Do NOT write back
  // to fptr->fogRGB.
  if (!IsUnderwater() && cf > 0 && cf < 127 && g_GLRenderer) {
      CurFogColor = ApplySunFogColourShift(fptr->fogRGB, g_GLRenderer->GetSunLight());
  }
#endif

  float d = VectorLength(v);

  v.y+=CameraY;

  // Pocket fog improvements (breathing, undulating floor):
  // Apply to pocket fog volumes only (cf 1..126, not underwater).
  float fogFloorY = fptr->YBegin * ctHScale;
  // Undulating fog floor -- let the fog follow the terrain relief
  // instead of sitting on a flat horizontal plane.  IMPORTANT: only the
  // SMOOTH terrain-following term is used.  The original used RandomMap
  // for a "fine-scale noise" offset, but RandomMap is white (per-2-cell)
  // noise, not smooth value-noise.  Feeding it into the fog floor height
  // injected per-vertex density noise, which the per-vertex fog blend
  // (mix(litColor, vFogColor, vFog)) turned into visible splotches of
  // fog colour.  Real low-frequency undulation should come from a proper
  // value/simplex noise field, not from RandomMap.
  // Gated to pocket fog volumes (cf 1..126), like the other pocket-fog work.
  if (!IsUnderwater() && cf > 0 && cf < 127)
  {
  float mx = v.x + CameraX;
  float mz = v.z + CameraZ;
  int tx = (static_cast<int>(mx / 256.0f)) & (ctMapSize - 1);
  int tz = (static_cast<int>(mz / 256.0f)) & (ctMapSize - 1);
  float terrainH = static_cast<float>(HMap[tz][tx]) * ctHScale;
  float heightDelta = (terrainH - fogFloorY) / ctHScale;
  // Smooth: terrainH is sampled from the (already smooth) heightmap at
  // the vertex's own map cell, so terrainFollow varies continuously.
  float terrainFollow = std::clamp(heightDelta * 0.15f, -2.0f, 2.0f);
  fogFloorY += terrainFollow * ctHScale;
  }

  float fla= -(v.y     - fogFloorY) / ctHScale;
  if (!vinfog) if (fla>0) fla=0;

  // Camera term. A foreign pocket must not inherit the camera's in-fog
  // envelope (nor the inside-fog boost below): the destination volume is
  // authoritative for its own demand, and the global envelope already
  // veils the scene from inside the camera's pocket.
  float flb = -(CameraY - fogFloorY) / ctHScale;
  flb = ResolvePocketCameraDepth(flb, CAMERAINFOG != 0, cf, CameraFogI);

  if (fla<0 && flb<0) return 0;

  if (fla<0)
  {
    d*= flb / (flb-fla);
    fla = 0;
  }
  if (flb<0)
  {
    if (fla > 0 && !IsUnderwater() && cf > 0 && cf < 127)
    {
      // From-above visibility.  Camera is above the fog layer but the
      // vertex is inside it.  The legacy d *= fla/(fla-flb) left the bank
      // nearly invisible: fl collapsed to just the vertex's thin vertical
      // depth (fla) while the distance term was shrunk to the in-fog slice.
      // Boost the fog column by how far the camera sits above the fog top, so
      // the layer reads as a real volumetric bank when viewed from a ridge.
      // Bounded (columnBoost <= 2.0) so it can't over-fog distant terrain.
      const float viewRatio   = std::fabs(flb) / std::max(fla, 1.0f);
      const float columnBoost = std::min(viewRatio * 0.3f, 2.0f);
      fla *= (1.0f + columnBoost);
      d *= fla / (fla - flb);
      flb = 0;
    }
    else
    {
      d *= fla / (fla - flb);
      flb = 0;
    }
  }

  float fl = (fla + flb);

  // Fog density breathes over time -- modulate effective Transp by
  // a slow sine seeded by the fog volume index.  The breathe factor
  // depends only on (cf, RealTime); RealTime is constant within a
  // frame, so precompute the 256-entry table once per frame and
  // look it up per vertex instead of calling sinf() per vertex.
  float effectiveTransp = fptr->Transp;
  if (!IsUnderwater() && cf > 0 && cf < 127)
  {
    static float s_breatheCache[256];
    static int   s_breatheTime = -1;
    static bool  s_breatheFilled = false;
    if (!s_breatheFilled || RealTime != s_breatheTime) {
        for (int i = 0; i < 256; ++i) {
            if (i == 0 || i >= 127) { s_breatheCache[i] = 1.0f; continue; }
            const float period = 8.0f + (i & 7) * 1.0f;
            const float phase  = i * 137.5f;
            s_breatheCache[i] = 1.0f + 0.08f * std::sin(static_cast<float>(RealTime) / (period * 60.0f) + phase);
        }
        s_breatheTime = RealTime;
        s_breatheFilled = true;
    }
    effectiveTransp = fptr->Transp / s_breatheCache[cf];
  }

  // distance-density term.  Kept LINEAR to match the original
  // 3dfx/D3D CalcFogLevel strength.  The earlier pow(distTerm, 1.1/1.2)
  // curve reduced near-volume density and made the fog look weaker than the
  // baseline.  (A subtle S-curve can be re-added later as a separate,
  // density-neutral tweak, e.g. applied symmetrically.)
  float distTerm = (d + effectiveTransp * 0.5f) / effectiveTransp;
  fl *= distTerm;

  // Inside-fog envelope.  The from-above boost above makes a
  // valley reads as a thick bank; when the camera is *inside* the same volume
  // the base formula leaves near/mid fog comparatively thin, so the layer
  // feels less enveloping than the bank seen from a ridge.  Lift the thin
  // (near/mid) fog up to 1.6x and taper to 1.0x as it approaches FLimit, so
  // the volume feels surrounding without white-ing out the already-opaque
  // far fog.  Pocket fog only, like the from-above case.
  if (flb > 0 && fla > 0 && !IsUnderwater() && cf > 0 && cf < 127) {
      const float opacity = std::clamp(fl / fptr->FLimit, 0.0f, 1.0f);
      fl *= 1.0f + 0.6f * (1.0f - opacity);
  }

  // Underwater: add depth-based fog increase.  Keep the existing
  // horizontal-distance fog as the base, then layer on a gentle
  // vertical gradient so deeper terrain gets foggier.
  if (IsUnderwater())
  {
    // Base density multiplier -- scales the horizontal-distance fog.
    fl *= UWFog_BaseDensityMult;

    // Beer-Lambert camera depth multiplier -- fog increases as camera
    // goes deeper (exponential curve).
    if (UWFog_CameraDepthMult > 0.01f) {
      float extinction = 1.0f - std::exp(-CameraWaterDepthFactor * 3.5f);
      fl *= 1.0f + extinction * UWFog_CameraDepthMult;
    }

    // Vertical fog gradient: deeper terrain vertices get more fog,
    // independent of horizontal distance from camera.
    //
    // v.y is already in world space (CameraY added above).
    float vertDepth = (std::max)(0.0f, fptr->YBegin * ctHScale - v.y);

    // Gradient range and curve from debug parameters.
    float vertFactor = std::clamp(vertDepth / UWFog_VertRange, 0.0f, 1.0f);
    vertFactor = (float)pow(vertFactor, UWFog_CurveExp);

    // Additive fog based on depth only.
    fl += vertFactor * UWFog_VertStrength;

    // Soft cap from debug parameters.
    float cameraBoost = (1.0f - std::exp(-CameraWaterDepthFactor * 2.0f)) * UWFog_CapCameraBoost;
    float softCap = fptr->FLimit + UWFog_CapBase + cameraBoost;
    return (std::min)(fl, softCap);
  }

  return MIN(fl, fptr->FLimit);
}












void DrawScene()
{
#ifdef GL_PERF_HOOKS
  PerfFrameBegin();
#endif

  dFacesCount = 0;

  // Capture-harness camera motion (-camspin): spin the camera every frame so
  // stale camera-space precache entries (VMap/VMap2 .v) surface as stretched
  // geometry instead of hiding behind a fixed camera, and oscillate pitch to
  // exercise the sweep bounds' pitch branches. The accumulator assigns (not
  // increments) the globals so the player-state derivation in Controls cannot
  // pin the view.
  if (CamFixed) {
    // Pin the complete pose: startup cursor motion and settling/collisions
    // must not change the terrain workload between matched benchmark runs.
    static const float fixedX = PlayerX;
    static const float fixedZ = PlayerZ;
    CameraX = fixedX;
    CameraZ = fixedZ;
    CameraY = GetLandQH(fixedX, fixedZ) + 220.0f;
    CameraAlpha = 0.0f;
    CameraBeta = 0.0f;
  }
  if (CamSpin) {
    static float camSpinAlpha = 0.0f;
    // Background correctness captures cover a full revolution in exactly
    // 120 frames regardless of GPU load/FPS; foreground retains real time.
    camSpinAlpha += unattended_capture::options.enabled ? static_cast<float>(2.0 * pi / unattended_capture::kCaptureFrames) : TimeDt / 1500.f;
    CameraAlpha = camSpinAlpha;
    CameraBeta = unattended_capture::options.enabled ? std::sin(camSpinAlpha * 2.0f) * 0.2f :
        static_cast<float>(sin(RealTime / 360.f)) * 0.2f;
    if (CamTravel) {
      // Render-only translation around the player: cross several 512-unit
      // CCX/CCY snap boundaries without changing saves or player physics.
      CameraX = PlayerX + 640.0f * std::sin(camSpinAlpha * 0.7f);
      CameraZ = PlayerZ + 640.0f * std::cos(camSpinAlpha * 0.9f);
    }
  }

  ca = static_cast<float>(cos(CameraAlpha));
  sa = static_cast<float>(sin(CameraAlpha));

  cb = static_cast<float>(cos(CameraBeta));
  sb = static_cast<float>(sin(CameraBeta));

  if (CamFixed || CamSpin) {
    // Controls computed these from PlayerBeta before the diagnostic pose
    // override. Match its policy to the camera actually being rendered.
    const float aspectScale = (std::max)(1.0f,
        (static_cast<float>(WinW) / static_cast<float>(WinH)) / (4.0f / 3.0f));
    BackViewR = CLIP3D ? (sb < 0.0f ? 320.0f - 1024.0f * sb : 320.0f + 512.0f * sb) * aspectScale : 300.0f;
    BackViewRR = CLIP3D ? static_cast<int>((380 + static_cast<int>(1024.0f * std::fabs(sb))) * aspectScale) : 380;
    if (CLIP3D && IsUnderwater()) BackViewR -= 512.0f * (std::min)(0.0f, sb) * aspectScale;
  }

  CCX = (static_cast<int>(CameraX) / 512) * 2;
  CCY = (static_cast<int>(CameraZ) / 512) * 2;

#ifdef GL_PERF_HOOKS
  GL_PERF_CPU_SCOPE("PreCashGroundModel");
#endif
  PreCashGroundModel();

  // compute the camera-in-fog global envelope once per frame so the
  // terrain and model shaders can fog the whole scene (not just in-volume
  // geometry) when the player is submerged in a tall pocket-fog volume.
#ifdef _gl
  if (g_GLRenderer) g_GLRenderer->UpdateCameraFogEnvelope();
#endif

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

#ifndef _soft
  // GL/D3D render the water surface as a POST-PASS, AFTER all world geometry
  // (terrain, models, shadows). The water surface uses glDepthMask(GL_FALSE)
  // (GLWater.cpp), so it does NOT write depth. If it were drawn before the
  // models, subsequently-drawn underwater creatures -- which are in front of
  // the terrain in the depth buffer -- would pass the depth test and paint
  // over the water, making them visible "through" the surface. As a post-pass
  // the water blends correctly on top of the already-drawn models.
  // The software renderer has no Z-buffer and instead draws the surface from
  // inside RenderGround's ring loop (ProcessMap -> ProcessMapW/ProcessMapW2),
  // interleaved with models by distance, so it must NOT be drawn here.
  if (NeedWater) RenderWater();
#endif

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

  const bool background = unattended_capture::options.enabled;
  hwndMain = CreateWindowEx(background ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0,
               "HuntRenderWindow","Carnivores 2 Renderer",
               background ? WS_POPUP : WS_VISIBLE | WS_POPUP,
               0, 0, 0, 0, nullptr,  nullptr, hInst, nullptr );

  if (hwndMain)
    PrintLog("Ok.\n");

  return true;
}

// Paces the frame from the shared FPS-limit option table (Core/Constants.h).
static void LimitFPS()
{
	static framelimiter::FrameLimiter limiter;
	limiter.WaitUntilFrameEnd(FpsLimitTarget(OptFpsLimit));
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

    PrintLog("Entered game\n");
    ReInitGame();
    CaptureMouse(true);

	if (Multiplayer) {
		if (!_MultiplayerState) {

			if (Host) {
				g_Network.StartServerCommsThread();
			} else {
				g_Network.StartClientCommsThread();
			}

		}
		_MultiplayerState = 1;
	}

  }

  _GameState = 1;

  if (NeedRVM)
  {
    if (!unattended_capture::options.enabled) {
      SetWindowPos(hwndMain, HWND_TOP, 0,0,0,0, SWP_SHOWWINDOW);
      SetFocus(hwndMain);
    }
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
  // DrawFrame replaces DrawScene() for GL and Soft -- it calls
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

  if (CanUseMap() && g_GameMode == GameMode::MapMode) DrawHMap();

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

  // Keep structured diagnostics separate from the legacy render.log stream.
  LogInit("carnivor.log");
  CreateLog();

  if (!unattended_capture::parse(__argc, __argv, unattended_capture::options)) {
    LOG_ERROR("Invalid unattended capture arguments");
    PrintLog("Invalid unattended capture arguments.\n");
    CloseLog(); LogClose();
    return ERROR_BAD_ARGUMENTS;
  }
#if !defined(_gl) || !defined(GL_PERF_HOOKS)
  if (unattended_capture::options.enabled) {
    LOG_ERROR("Unattended capture requires OpenGL with GL_PERF_HOOKS");
    CloseLog(); LogClose();
    return ERROR_NOT_SUPPORTED;
  }
#endif
  if (unattended_capture::options.enabled) {
    PrintLog("Unattended capture: hidden window, no input/focus/audio; normal process priority.\n");
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  }

  CreateMainWindow();

  Init3DHardware();
  InitEngine();
  if (!unattended_capture::options.enabled) InitAudioSystem(hwndMain, hlog, OptSound);

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
  LoadPictureTGA(TFX_SPECULAR,  "HUNTDAT\\FX\\specular.tga", MemoryTag::Global);


  PrintLog(" Done.\n");

  PrintLoad("Loading area...");
  LoadResources();

  PrintLoad("Starting game...");
  PrintLog("Loading area: Done.\n");

  EndLoading();

  ProcessSyncro();
  blActive = true;
#ifdef _gl
  workbench::NotifyUIFocus(true);
#endif

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
      if (blActive || unattended_capture::options.enabled) { ProcessGame(); LimitFPS(); }
      else Sleep(100);
    }
  }

  if (Multiplayer) {
	  if (Host) {
		  g_Network.StopServer();
	  }
	  else {
		  g_Network.StopClient();
	  }
  }

  AudioStop();
  Audio_Shutdown();

  ShutDown3DHardware();
#ifdef _gl
  workbench::ShutdownUI();
  CaptureMouse(false);
#endif

  ShutDownEngine();

  if (!unattended_capture::options.enabled) ShowCursor(true);
  PrintLog("Game normal shutdown.\n");
  LOG_INFO("Game normal shutdown");

  CloseLog();
  LogClose();
  return msg.wParam;
}
