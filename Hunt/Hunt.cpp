#include "Hunt.h"
#include "stdio.h"
#include <timeapi.h>

float rav=0;
float rbv=0;

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
float BinocularPower  = 2.5;
float wpshy = 0;
float wpshz = 0;
float wpnb  = 0;
int wpnlight;

void HideWeapon();

char cheatcode[16] = "DEBUGUP";
int  cheati = 0;


void CaptureMouse(BOOL capture)
{
  if (!hwndMain) return;

  if (capture) {
    RECT rect;
    GetClientRect(hwndMain, &rect);

    POINT p1 = { rect.left, rect.top };
    POINT p2 = { rect.right, rect.bottom };
    ClientToScreen(hwndMain, &p1);
    ClientToScreen(hwndMain, &p2);
    SetRect(&rect, p1.x, p1.y, p2.x, p2.y);

    ClipCursor(&rect);
    while (ShowCursor(false) >= 0);
    ResetMousePos();
  } else {
    ClipCursor(nullptr);
    while (ShowCursor(true) < 0);
  }
}


void ResetMousePos()
{
  if (!hwndMain) return;

  if (blActive && _GameState && !IsPaused()) {
    POINT p = { VideoCX, VideoCY };
    ClientToScreen(hwndMain, &p);
    SetCursorPos(p.x, p.y);
  }
}



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

  return MIN(fl, fptr->FLimit);
}



void PreCashGroundModel()
{
  SKYDTime = RealTime>>1;
  int x,y;

  int kx = SKYDTime & 255;
  int ky = SKYDTime & 255;
  int SKYDT = SKYDTime>>8;

  int VideoCX16 = VideoCX * 16;
  int VideoCY16 = VideoCY * 16;
  float CameraW16 = CameraW * 16;
  float CameraH16 = CameraH * 16;

  BOOL FogFound = false;
  NeedWater = false;

  static float waveCache[32][32];
  static int waveCacheRandomMap[32][32] = {};
  static int waveCacheTime = 0;
  static bool waveCacheReady = false;

  if (!waveCacheReady || waveCacheTime != RealTime) {
    waveCacheTime = RealTime;
    for (int wy = 0; wy < 32; wy++) {
      for (int wx = 0; wx < 32; wx++) {
        waveCacheRandomMap[wy][wx] = RandomMap[wy][wx];
        waveCache[wy][wx] = static_cast<float>(sin(-pi/2 + waveCacheRandomMap[wy][wx] / 128 + static_cast<float>(RealTime) / 200.f));
      }
    }
    waveCacheReady = true;
  } else {
    for (int wy = 0; wy < 32; wy++) {
      for (int wx = 0; wx < 32; wx++) {
        if (waveCacheRandomMap[wy][wx] != RandomMap[wy][wx]) {
          waveCacheRandomMap[wy][wx] = RandomMap[wy][wx];
          waveCache[wy][wx] = static_cast<float>(sin(-pi/2 + waveCacheRandomMap[wy][wx] / 128 + static_cast<float>(RealTime) / 200.f));
        }
      }
    }
  }

  MapMinY = 10241024;
  Vector3d rv;


  for (y=-(ctViewR+3); y<(ctViewR+3); y++)
    for (x=-(ctViewR+3); x<(ctViewR+3); x++)
    {

      int r = MAX((MAX(y,-y)), (MAX(x,-x)));

      int xx = (CCX + x) & 1023;
      int yy = (CCY + y) & 1023;

      v[0].x = xx*256 - CameraX;
      v[0].z = yy*256 - CameraZ;
      v[0].y = static_cast<float>((static_cast<int>(HMap[yy][xx])))*ctHScale - CameraY;


//========= water section ===========//

      //if (RunMode)
      if ((FMap[yy][xx] & fmWaterA)>0)
      {
        rv = v[0];
        rv.y = WaterList[ WMap[yy][xx] ].wlevel*ctHScale - CameraY;

        // Use the per-RealTime wave offset cache built at the top of
        // PreCashGroundModel(). The cache reduces ~2,600 sin() calls per
        // frame to 1,024 per RealTime change (and zero in steady state).
        // The 4ab70c8 commit introduced the cache but never wired the
        // lookup; this change completes that fix.
        float wdelta = waveCache[yy & 31][xx & 31];

        if ( (FMap[yy][xx] & fmWater) && (r < ctViewR-4))
        {
          rv.x+=static_cast<float>(sin(xx+yy + RealTime/200.f)) * 16.f;
          rv.z+=static_cast<float>(sin(pi/2.f + xx+yy + RealTime/200.f)) * 16.f;
        }

        rv = RotateVector(rv);
        VMap2[kViewGridCenter + y][kViewGridCenter + x].v = rv;

        if (fabs(rv.x) > -rv.z + 1524)
        {
          VMap2[kViewGridCenter + y][kViewGridCenter + x].DFlags = 128;
        }
        else
        {
          NeedWater = true;
          VMap2[kViewGridCenter + y][kViewGridCenter + x].Light = 168-static_cast<int>((wdelta*24));

          float Alpha;
          if (IsUnderwater())
          {
            Alpha =	160 - VectorLength(rv)* 160 / 220 / ctViewR;
            if (Alpha<10) Alpha=10;
          }
          else if (r < ctViewR+2)
          {
            int wi = WMap[yy][xx];
            Alpha = static_cast<float>(((WaterList[wi].wlevel - HMap[yy][xx])*2+4))*WaterList[wi].transp;
            Alpha+=VectorLength(rv) / 256;
            Alpha+=wdelta*2;
            if (Alpha<0) Alpha=0;
            Vector3d va = v[0];
            NormVector(va,1.0f);
            va.y=-va.y;
            if (va.y<0) va.y=0;
            Alpha*=6.f/(va.y+0.1f);
            if (Alpha>255) Alpha=255.f;
          }
          else Alpha = 255.f;

          VMap2[kViewGridCenter + y][kViewGridCenter + x].ALPHA=static_cast<int>(Alpha);
          VMap2[kViewGridCenter + y][kViewGridCenter + x].Fog = 0;

          if (rv.z>-256.0) VMap2[kViewGridCenter + y][kViewGridCenter + x].DFlags=128;
          else
          {
#ifdef _soft
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX - static_cast<int>((rv.x / rv.z * CameraW));
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY + static_cast<int>((rv.y / rv.z * CameraH));

            int DF = 0;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)     DF+=1;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX) DF+=2;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry < 0)     DF+=4;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY) DF+=8;
#else
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX16 - static_cast<int>((rv.x / rv.z * CameraW16));
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY16 + static_cast<int>((rv.y / rv.z * CameraH16));

            int DF = 0;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)        DF+=1;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX*16) DF+=2;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry < 0)        DF+=4;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY*16) DF+=8;
#endif
            VMap2[kViewGridCenter + y][kViewGridCenter + x].DFlags = DF;

          }
        }
      }



#ifdef _soft
#else
#endif

      rv = RotateVector(v[0]);


      if (fabs(rv.x * FOVK) > -rv.z + 1600)
      {
        VMap[kViewGridCenter + y][kViewGridCenter + x].v = rv;
        VMap[kViewGridCenter + y][kViewGridCenter + x].DFlags = 128;
        continue;
      }


      if (HARD3D)
        if (  ((FMap[yy][xx] & fmWater)==0) || IsUnderwater())
          VMap[kViewGridCenter + y][kViewGridCenter + x].Fog = CalcFogLevel(v[0]);
        else
          VMap[kViewGridCenter + y][kViewGridCenter + x].Fog = 0;

      VMap[kViewGridCenter + y][kViewGridCenter + x].ALPHA = 255;

      v[0]=rv;

      if (v[0].z<1024)
        if (FOGENABLE)
          if (FogsMap[yy>>1][xx>>1]) FogFound = true;

      VMap[kViewGridCenter + y][kViewGridCenter + x].v = v[0];

      int  DF = 0;
      int  db = 0;

      if (v[0].z<256)
      {
        if (Clouds)
        {
          int shmx = (xx + SKYDT) & 127;
          int shmy = (yy + SKYDT) & 127;

          int db1 = SkyMap[shmy * 128 + shmx ];
          int db2 = SkyMap[shmy * 128 + ((shmx+1) & 127) ];
          int db3 = SkyMap[((shmy+1) & 127) * 128 + shmx ];
          int db4 = SkyMap[((shmy+1) & 127) * 128 + ((shmx+1) & 127) ];
          db = (db1 * (256 - kx) + db2 * kx) * (256-ky) +
               (db3 * (256 - kx) + db4 * kx) * ky;
          db>>=17;
          db = db - 40;
          if (db<0) db=0;
          if (db>48) db=48;
        }

        int clt = LMap[yy][xx];
        clt= MAX(64, clt-db);
        VMap[kViewGridCenter + y][kViewGridCenter + x].Light = clt;
      }



      if (v[0].z>-256.0) DF+=128;
      else
      {

#ifdef _soft
        VMap[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX - static_cast<int>((v[0].x / v[0].z * CameraW));
        VMap[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY + static_cast<int>((v[0].y / v[0].z * CameraH));

        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)        DF+=1;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX)    DF+=2;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry < 0)        DF+=4;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY)    DF+=8;
#else
        VMap[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX16 - static_cast<int>((v[0].x / v[0].z * CameraW16));
        VMap[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY16 + static_cast<int>((v[0].y / v[0].z * CameraH16));

        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)        DF+=1;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX*16) DF+=2;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry < 0)        DF+=4;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY*16) DF+=8;
#endif

      }

      VMap[kViewGridCenter + y][kViewGridCenter + x].DFlags = DF;
    }

  FOGON = FogFound || IsUnderwater();
}




void AddShadowCircle(int x, int y, int R, int D)
{
  if (IsUnderwater()) return;

  int cx = x / 256;
  int cy = y / 256;
  int cr = 1 + R / 256;
  for (int yy=-cr; yy<=cr; yy++)
    for (int xx=-cr; xx<=cr; xx++)
    {
      int tx = (cx+xx)*256;
      int ty = (cy+yy)*256;
      int r = static_cast<int>(sqrt(static_cast<float>(((tx-x)*(tx-x) + (ty-y)*(ty-y))) ));
      if (r>R) continue;
      VMap[cy + yy - CCY + kViewGridCenter][cx + xx - CCX + kViewGridCenter].Light-= D * (R-r) / R;
      if (VMap[cy + yy - CCY + kViewGridCenter][cx + xx - CCX + kViewGridCenter].Light < 32)
        VMap[cy + yy - CCY + kViewGridCenter][cx + xx - CCX + kViewGridCenter].Light = 32;
    }
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


void ProcessReload() {

	TWeapon *wptr = &Weapon;

	if (wptr->state == 2 && wptr->FTime == 0)
		if (WeapInfo[CurrentWeapon].Reload) {
			if (Chambered[CurrentWeapon] < WeapInfo[CurrentWeapon].Reload &&
				ShotsLeft[CurrentWeapon]) {

				wptr->ammoIn = WeapInfo[CurrentWeapon].Reload - Chambered[CurrentWeapon];
				if (ShotsLeft[CurrentWeapon] < WeapInfo[CurrentWeapon].Reload) wptr->ammoIn = ShotsLeft[CurrentWeapon];

				if ((Chambered[CurrentWeapon] || ShotsLeft[CurrentWeapon] < WeapInfo[CurrentWeapon].Reload)
					&& WeapInfo[CurrentWeapon].rldAnimPart >= 0) {

					//state 5
					if (wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].rldAnimPart].AniTime)
					{
						wptr->state = 5;
						wptr->FTime = 1;
						if (IsUnderwater()) {
							if (WeapInfo[CurrentWeapon].rldAqSndPart >= 0)
								AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSndPart].length,
									wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSndPart].lpData.data(), 256);
						}
						else {
							int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].rldAnimPart];
							if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
								wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
						}
					}

				}
				else {

					//state 4
					if (wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].rldAnim].AniTime)
					{
						wptr->state = 4;
						wptr->FTime = 1;
						if (IsUnderwater()) {
							if (WeapInfo[CurrentWeapon].rldAqSnd >= 0)
								AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSnd].length,
									wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSnd].lpData.data(), 256);
						}
						else {
							int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].rldAnim];
							if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
								wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
						}
					}

				}


			}

		}
		else if (AmmoMag[CurrentWeapon]) {

			/*
			int temp = MagShotsLeft[CurrentWeapon];
			MagShotsLeft[CurrentWeapon] = ShotsLeft[CurrentWeapon];
			ShotsLeft[CurrentWeapon] = temp;

			if (!MagShotsLeft[CurrentWeapon]) AmmoMag[CurrentWeapon]--;

			if (!Chambered[CurrentWeapon]) {
				Chambered[CurrentWeapon] = 1;
				ShotsLeft[CurrentWeapon]--;
			}
			*/

			if (Chambered[CurrentWeapon] && WeapInfo[CurrentWeapon].rldAnimPart >= 0) {


				if (wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].rldAnimPart].AniTime)
				{

					wptr->state = 5;
					wptr->FTime = 1;
					if (IsUnderwater()) {
						if (WeapInfo[CurrentWeapon].rldAqSndPart >= 0)
							AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSndPart].length,
								wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSndPart].lpData.data(), 256);
					}
					else {
						int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].rldAnimPart];
						if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
							wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
					}
				}
			}
			else {

				if (wptr->chinfo[CurrentWeapon].Animation[WeapInfo[CurrentWeapon].rldAnim].AniTime)
				{

					wptr->state = 4;
					wptr->FTime = 1;
					if (IsUnderwater()) {
						if (WeapInfo[CurrentWeapon].rldAqSnd >= 0)
							AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSnd].length,
								wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].rldAqSnd].lpData.data(), 256);
					}
					else {
						int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].rldAnim];
						if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
							wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
					}

				}

			}




		}

}

void ProcessFireMode() {
	TWeapon *wptr = &Weapon;

	if (!WeapInfo[CurrentWeapon].semiauto) return;
	if (!WeapInfo[CurrentWeapon].fullauto) return;
	if (WeapInfo[CurrentWeapon].modAnim <= 0) return;

	if (wptr->state == 2 && wptr->FTime == 0) {
		wptr->state = 7;
		wptr->FTime = 1;
		if (IsUnderwater()) {
			if (WeapInfo[CurrentWeapon].modAqSnd >= 0)
				AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].modAqSnd].length,
					wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].modAqSnd].lpData.data(), 256);
		}
		else {
			int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].modAnim];
			if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
				wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
		}
	}

}


void ProcessPump() {
	TWeapon *wptr = &Weapon;

	if (WeapInfo[CurrentWeapon].pmpAnim <= 0) return;

	if (wptr->state == 2 && wptr->FTime == 0)
		if (!WeapInfo[CurrentWeapon].Reload) {
			Chambered[CurrentWeapon] = 0;
			//if (!ShotsLeft[CurrentWeapon]) return;
			wptr->state = 6;
			wptr->FTime = 1;
			if (IsUnderwater()) {
				if (WeapInfo[CurrentWeapon].pmpAqSnd >= 0)
					AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].pmpAqSnd].length,
						wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].pmpAqSnd].lpData.data(), 256);
			}
			else {
				int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].pmpAnim];
				if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
					wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
			}
		}
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





void SwitchMode(LPSTR lps, BOOL& b)
{
  b = !b;
  char buf[200];
  if (b) sprintf_s(buf, sizeof(buf),"%s is ON", lps);
  else sprintf_s(buf, sizeof(buf),"%s is OFF", lps);
  MessageBeep(0xFFFFFFFF);
  AddMessage(buf);
}


void ChangeViewR(int d1, int d2, int d3)
{
  char buf[200];
  (void)d2;
  ctViewR +=d1;
  ctViewRM+=d3;
  if (ctViewR<kViewDistanceMin) ctViewR = kViewDistanceMin;
  if (ctViewR>kViewDistanceMax) ctViewR = kViewDistanceMax;
  ctViewR1 = (ctViewR * OptTerrainLOD) / 100;
  if (ctViewRM < kObjectDetailMin) ctViewRM = kObjectDetailMin;
  if (ctViewRM > kObjectDetailMax) ctViewRM = kObjectDetailMax;

  sprintf_s(buf, sizeof(buf),"ViewR = %d BMP at %d", ctViewR, ctViewRM);
  //MessageBeep(0xFFFFFFFF);
  AddMessage(buf);

}


void ChangeCall()
{
  if (!TargetDino) return;
  if (ChCallTime)
    for (int t=0; t<32; t++)
    {
      TargetCall++;
      if (TargetCall>32) TargetCall=10;
      if (TargetDino & (1<<TargetCall)) break;
    }
  //sprintf_s(logt, sizeof(logt),"Call: %s", DinoInfo[ AI_to_CIndex[TargetCall] ].Name);
  //AddMessage(logt);
  //CallLockTime+= 1024;
  ChCallTime = 2048;
}

void ToggleBinocular()
{
  if (Weapon.state) return;
  if (IsUnderwater()) return;
  if (!MyHealth) return;
  g_GameMode = (g_GameMode == GameMode::Binocular) ? GameMode::Normal : GameMode::Binocular;
  if (g_GameMode == GameMode::Binocular) AddMessage("Binocular view");
}


void ToggleRunMode()
{
  RunMode = !RunMode;
  if (RunMode) AddMessage("Run mode is ON");
  else AddMessage("Run mode is OFF");
}

void ToggleCrouchMode()
{
	g_GameMode = (g_GameMode == GameMode::Crouching) ? GameMode::Normal : GameMode::Crouching;
	HitBox.phase = g_GameMode == GameMode::Crouching;
	if (g_GameMode == GameMode::Crouching) AddMessage("Crouch mode is ON");
	else AddMessage("Crouch mode is OFF");
}


void ToggleMapMode()
{
  if (!MyHealth) return;
  if (g_GameMode == GameMode::Binocular) return;
  if (Weapon.state) return;
  g_GameMode = (g_GameMode == GameMode::MapMode) ? GameMode::Normal : GameMode::MapMode;
}




void ShowShifts()
{
  sprintf(logt, "Y=%3.4f  Z=%3.4f  A=%3.4f", wpshy/2, wpshz/2, wpnb*180/3.1415);
  AddMessage(logt);
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




void ProcessShoot()
{
  //if (HeadBackR) return;
	
  TWeapon *wptr = &Weapon;
  if (IsUnderwater() && !WeapInfo[CurrentWeapon].harpoon)
  {
    HideWeapon();
    return;
  }

  if (wptr->state == 2 && wptr->FTime==0)
  {
	  int clickNo = rRand(2);
	  if (!Chambered[CurrentWeapon]) {
		  if (!alreadyFired && !IsUnderwater()) AddVoicev(fxClick[clickNo].length, fxClick[clickNo].lpData.data(), 256);
		  return;
	  }

	  if (alreadyFired && FiringMode[CurrentWeapon] == 0) return;

    wptr->FTime = 1;
    HeadBackR=64;
	Recoil.y = -static_cast<float>(WeapInfo[CurrentWeapon].recoil) / 100.f;
	float rx = rRand(8);
	rx -= 4;
	rx /= 8;
	rx *= static_cast<float>(WeapInfo[CurrentWeapon].recoil) / 100.f;
	Recoil.x += rx;

	if (IsUnderwater()) {
		if (WeapInfo[CurrentWeapon].shtAqSnd >= 0)
			AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].shtAqSnd].length,
				wptr->chinfo[CurrentWeapon].SoundFX[WeapInfo[CurrentWeapon].shtAqSnd].lpData.data(), 256);
	}
	else {
		int fx = wptr->chinfo[CurrentWeapon].Anifx[WeapInfo[CurrentWeapon].shtAnim];
		if (fx >= 0) AddVoicev(wptr->chinfo[CurrentWeapon].SoundFX[fx].length,
			wptr->chinfo[CurrentWeapon].SoundFX[fx].lpData.data(), 256);
	}
	
	TrophyRoom.Last.smade++;

	if (WeapInfo[CurrentWeapon].MuzzFlash|| WeapInfo[CurrentWeapon].ChamFlash)wptr->FlashP = 1;

	if (WeapInfo[CurrentWeapon].MuzzFlash) {
		Muzz = true;
		MuzzGamma = rRand(100);
		MuzzGamma /= 50;
		MuzzGamma *= pi;
	}

    for (int s=0; s<=WeapInfo[CurrentWeapon].TraceC; s++)
    {
      float rA = 0;
      float rB = 0;

	  if (IsUnderwater()) {
		  rA = siRand(128) * 0.00010 * (2.f - WeapInfo[CurrentWeapon].PrecAq);
		  rB = siRand(128) * 0.00010 * (2.f - WeapInfo[CurrentWeapon].PrecAq);
	  }else{
		  rA = siRand(128) * 0.00010 * (2.f - WeapInfo[CurrentWeapon].Prec);
		  rB = siRand(128) * 0.00010 * (2.f - WeapInfo[CurrentWeapon].Prec);
	  }


      float ca = static_cast<float>(cos(PlayerAlpha + wpnDAlpha + rA));
      float sa = static_cast<float>(sin(PlayerAlpha + wpnDAlpha + rA));
      float cb = static_cast<float>(cos(PlayerBeta + wpnDBeta + rB));
      float sb = static_cast<float>(sin(PlayerBeta + wpnDBeta + rB));

      nv.x=sa;
      nv.y=0;
      nv.z=-ca;

      nv.x*=cb;
      nv.y=-sb;
      nv.z*=cb;

	  float v = WeapInfo[CurrentWeapon].Veloc;
	  if (IsUnderwater()) v = WeapInfo[CurrentWeapon].VelocAq;
	  float l = WeapInfo[CurrentWeapon].Veloc;
	  if (WeapInfo[CurrentWeapon].aqLow) l = WeapInfo[CurrentWeapon].VelocAq;

      AddBullet(PlayerX, PlayerY+HeadY, PlayerZ,
               nv.x * 64* v,
               nv.y * 64* v,
	           nv.z * 64* v,
		  nv.x * 64 * l,
		  nv.y * 64 * l,
		  nv.z * 64 * l,
			   CurrentWeapon,
			   false);
    }

	//Multiplayer)
	sendGunShot = CurrentWeapon;
	

    Vector3d v;
    v.x = PlayerX;
    v.y = PlayerY;
    v.z = PlayerZ;
    if (!IsUnderwater()) MakeNoise(v, ctViewR*200 * WeapInfo[CurrentWeapon].Loud);
    Chambered[CurrentWeapon]-=1;
//	else if (WeapInfo[CurrentWeapon].Reload) {
//		if (!Chambered[CurrentWeapon]) Chambered[CurrentWeapon] = WeapInfo[CurrentWeapon].Reload;
//	}
  }
}


void ProcessSlide()
{
  if (NOCLIP || IsUnderwater()) return;
  float ch = GetLandQHNoObj(PlayerX, PlayerZ);
  float mh = ch;
  float chh;
  int   sd = 0;

  chh=GetLandQHNoObj(PlayerX - 16, PlayerZ);
  if (chh<mh)
  {
    mh = chh;
    sd = 1;
  }
  chh=GetLandQHNoObj(PlayerX + 16, PlayerZ);
  if (chh<mh)
  {
    mh = chh;
    sd = 2;
  }
  chh=GetLandQHNoObj(PlayerX, PlayerZ - 16);
  if (chh<mh)
  {
    mh = chh;
    sd = 3;
  }
  chh=GetLandQHNoObj(PlayerX, PlayerZ + 16);
  if (chh<mh)
  {
    mh = chh;
    sd = 4;
  }

  chh=GetLandQHNoObj(PlayerX - 12, PlayerZ - 12);
  if (chh<mh)
  {
    mh = chh;
    sd = 5;
  }
  chh=GetLandQHNoObj(PlayerX + 12, PlayerZ - 12);
  if (chh<mh)
  {
    mh = chh;
    sd = 6;
  }
  chh=GetLandQHNoObj(PlayerX - 12, PlayerZ + 12);
  if (chh<mh)
  {
    mh = chh;
    sd = 7;
  }
  chh=GetLandQHNoObj(PlayerX + 12, PlayerZ + 12);
  if (chh<mh)
  {
    mh = chh;
    sd = 8;
  }

  if (!NOCLIP)
    if (mh<ch-16)
    {
      float delta = (ch-mh) / 4;
      if (sd == 1)
      {
        PlayerX -= delta;
      }
      if (sd == 2)
      {
        PlayerX += delta;
      }
      if (sd == 3)
      {
        PlayerZ -= delta;
      }
      if (sd == 4)
      {
        PlayerZ += delta;
      }

      delta*=0.7f;
      if (sd == 5)
      {
        PlayerX -= delta;
        PlayerZ -= delta;
      }
      if (sd == 6)
      {
        PlayerX += delta;
        PlayerZ -= delta;
      }
      if (sd == 7)
      {
        PlayerX -= delta;
        PlayerZ += delta;
      }
      if (sd == 8)
      {
        PlayerX += delta;
        PlayerZ += delta;
      }
    }
}



void ProcessPlayerMovement()
{

  POINT ms;

  GetCursorPos(&ms);
  ScreenToClient(hwndMain, &ms);
  if (REVERSEMS) ms.y = -ms.y+VideoCY*2;
  // The per-frame mouse delta naturally scales with frame time because the
  // cursor is reset to the centre every frame, so ms-VideoCX/Y ~= V*T. The
  // original `rav += D * K` is therefore already framerate-independent in
  // terms of per-second sensitivity (K*V constant). Do NOT normalize by
  // TimeDt here: in an uncapped game that makes sensitivity scale linearly
  // with framerate (4x faster look at 240 FPS vs 60 FPS).
  rav += static_cast<float>((ms.x-VideoCX)) * (OptMsSens+64) / 600.f / 192.f;
  rbv += static_cast<float>((ms.y-VideoCY)) * (OptMsSens+64) / 600.f / 192.f;
//  if (KeyFlags & kfStrafe)
//    SSpeed+= static_cast<float>(rav) * 10;
//  else
    PlayerAlpha += rav;
  PlayerBeta  += rbv;

  // Per-second exponential decay (10 ms time constant) so the smoothing is
  // framerate-independent. Replaces the old per-frame `/(2 + TimeDt/20)`
  // which was much stronger at low FPS and made the look sluggish on slow
  // frames.
  float decay = expf(-static_cast<float>(TimeDt) / 10.0f);
  rav *= decay;
  rbv *= decay;
  ResetMousePos();



  if ( !(KeyFlags & (kfForward | kfBackward)))
    if (VSpeed>0) VSpeed=MAX(0,VSpeed-DeltaT*2);
    else VSpeed=MIN(0,VSpeed+DeltaT*2);

  if ( !(KeyFlags & (kfSLeft | kfSRight)))
    if (SSpeed>0) SSpeed=MAX(0,SSpeed-DeltaT*2);
    else SSpeed=MIN(0,SSpeed+DeltaT*2);

  if (KeyFlags & kfForward)  if (VSpeed>0) VSpeed+=DeltaT;
    else VSpeed+=DeltaT*4;
  if (KeyFlags & kfBackward) if (VSpeed<0) VSpeed-=DeltaT;
    else VSpeed-=DeltaT*4;

  if (KeyFlags & kfSRight )  if (SSpeed>0) SSpeed+=DeltaT;
    else SSpeed+=DeltaT*4;
  if (KeyFlags & kfSLeft  )  if (SSpeed<0) SSpeed-=DeltaT;
    else SSpeed-=DeltaT*4;


  if (g_GameMode == GameMode::Swimming)
  {
    if (VSpeed > 0.25f) VSpeed = 0.25f;
    if (VSpeed <-0.25f) VSpeed =-0.25f;
    if (SSpeed > 0.25f) SSpeed = 0.25f;
    if (SSpeed <-0.25f) SSpeed =-0.25f;
  }
  if ( RunMode && (HeadY == 220.f) && (Weapon.state==0 || WeapInfo[CurrentWeapon].canRun))
  {
    if (VSpeed > 0.7f) VSpeed = 0.7f;
    if (VSpeed <-0.7f) VSpeed =-0.7f;
    if (SSpeed > 0.7f) SSpeed = 0.7f;
    if (SSpeed <-0.7f) SSpeed =-0.7f;
  }
  else
  {
    if (VSpeed > 0.3f) VSpeed = 0.3f;
    if (VSpeed <-0.3f) VSpeed =-0.3f;
    if (SSpeed > 0.30f) SSpeed = 0.30f;
    if (SSpeed <-0.30f) SSpeed =-0.30f;
  }

  if (KeyboardState[KeyMap.fkFire] & 128) {
	  ProcessShoot();
	  alreadyFired = true;
  } else alreadyFired = false;

  //STRAFE - PUMP
//  if (KeyFlags & kfStrafe)
  if (KeyboardState[KeyMap.fkStrafe] & 128) ProcessPump(); 

  //FIRING MODE
  if (KeyboardState[KeyMap.fkFiringMode] & 128) ProcessFireMode();

  if (KeyboardState[KeyMap.fkReload] & 128) ProcessReload();

  //menu option/already used check needed - TODO
  if (KeyboardState[KeyMap.fkResupply] & 128) AddShipSupply(PlayerX,PlayerZ);

  if (Weapon.state) {
	  if (KeyboardState[KeyMap.fkHoldBreath] & 128 && !IsUnderwater()) {
		  if (Weapon.breathPressed == 0) {
			  AddVoicev(fxBreathIn.length, fxBreathIn.lpData.data(), 256);
			  Weapon.breathPressed = 1;
		  }
		  if (!Weapon.HoldBreath) {
			  Weapon.HoldBreath = true;
		  }
	  }
	  else {
		  if (Weapon.HoldBreath) {
			  Weapon.HoldBreath = false;
			  if (Weapon.breathPressed == 1 && !IsUnderwater()) AddVoicev(fxBreathOut.length, fxBreathOut.lpData.data(), 256);
		  }
		  Weapon.breathPressed = 0;
	  }
  }

  if (Multiplayer && Host) {
	  for (int c = 0; c < 6; c++) {
		  if (mDamage[0][c]) {
			  Characters[c].Health -= mDamage[0][c];
			  mDamage[0][c] = 0;
			  if (Characters[c].Health < 0) Characters[c].Health = 0;
			  registerDamage(c, false);// this needs to register enemy damage!
		  }
	  }
  }

  if (KeyboardState[VK_RETURN] & 128) if (TrophyDisplay && !ScoreDispTime && !Characters[TrophyDisplayC].claimed && !Tranq) AddShipTask(TrophyDisplayC);

  if (KeyboardState [KeyMap.fkShow] & 128) HideWeapon();

  if (g_GameMode == GameMode::Binocular)
  {
    if (KeyboardState[VK_ADD     ] & 128) BinocularPower+=BinocularPower * TimeDt / 4000.f;
    if (KeyboardState[VK_SUBTRACT] & 128) BinocularPower-=BinocularPower * TimeDt / 4000.f;
    if (BinocularPower < 1.5f) BinocularPower = 1.5f;
    if (BinocularPower > 3.0f) BinocularPower = 3.0f;
  }

  if (KeyFlags & kfCall) MakeCall();

  if (DEBUG)
    if (KeyboardState [VK_CONTROL] & 128)
      if (KeyFlags & kfBackward) VSpeed =-8;
      else VSpeed = 8;

  if (KeyFlags & kfJump)
    if (YSpeed == 0 && g_GameMode != GameMode::Swimming)
    {
      YSpeed = 600 + static_cast<float>(fabs(VSpeed)) * 600;
      AddVoicev(fxJump.length, fxJump.lpData.data(), 256);
    }

//=========  rotation =========//
  if (KeyFlags & kfRight)  PlayerAlpha+=DeltaT*1.5f;
  if (KeyFlags & kfLeft )  PlayerAlpha-=DeltaT*1.5f;
//  if (KeyFlags & kfLookUp) PlayerBeta-=DeltaT;
//  if (KeyFlags & kfLookDn) PlayerBeta+=DeltaT;

//========= movement ==========//

  ca = static_cast<float>(cos(PlayerAlpha));
  sa = static_cast<float>(sin(PlayerAlpha));
  cb = static_cast<float>(cos(PlayerBeta));
  sb = static_cast<float>(sin(PlayerBeta));

  nv.x=sa;
  nv.y=0;
  nv.z=-ca;


  PlayerNv = nv;
  if (IsUnderwater() || FLY)
  {
    nv.x*=cb;
    nv.y=-sb;
    nv.z*=cb;
    PlayerNv = nv;
  }
  else
  {
    PlayerNv.x*=cb;
    PlayerNv.y=-sb;
    PlayerNv.z*=cb;
  }

  Vector3d sv = nv;
  nv.x*=static_cast<float>(TimeDt)*VSpeed;
  nv.y*=static_cast<float>(TimeDt)*VSpeed;
  nv.z*=static_cast<float>(TimeDt)*VSpeed;

  sv.x*=static_cast<float>(TimeDt)*SSpeed;
  sv.y=0;
  sv.z*=static_cast<float>(TimeDt)*SSpeed;

  if (g_GameMode != GameMode::TrophyMode)
  {
    TrophyRoom.Last.path+=(TimeDt*VSpeed) / 128.f;
    TrophyRoom.Last.time+=TimeDt/1000.f;
  }

//if (SWIM & (VSpeed>0.1) & (sb>0.60)) HeadY-=40;

  int mvi = 1 + TimeDt / 16;

  for (int mvc = 0; mvc<mvi; mvc++)
  {
    PlayerX+=nv.x / mvi;
    PlayerY+=nv.y / mvi;
    PlayerZ+=nv.z / mvi;

    PlayerX-=sv.z / mvi;
    PlayerZ+=sv.x / mvi;

    if (!NOCLIP) CheckCollision(PlayerX, PlayerZ);

    if (PlayerY <= GetLandQHNoObj(PlayerX, PlayerZ)+16)
    {
      ProcessSlide();
      ProcessSlide();
    }
  }

  if (PlayerY <= GetLandQHNoObj(PlayerX, PlayerZ)+16)
  {
    ProcessSlide();
    ProcessSlide();
  }
//===========================================================
}


void ProcessDemoMovement()
{
  g_GameMode = GameMode::Normal;

  g_GameMode = GameMode::Normal;
  g_GameMode = GameMode::Normal;

  if (DemoPoint.DemoTime>6*1000)
    if (!IsPaused())
    {
      g_GameMode = GameMode::ExitCountdown;
      ResetMousePos();
    }

  if (DemoPoint.DemoTime>12*1000)
  {
    //ResetMousePos();
    //DemoPoint.DemoTime = 0;
    //LoadTrophy();
    DoHalt("");
    return;
  }

  VSpeed = 0.f;

  /*
  DemoPoint.pos = Characters[DemoPoint.CIndex].pos;
  DemoPoint.pos.y+=256;

  float base = 824;
  if (killerDino) {
	  if (killerDino->Clone == AI_TREX && !killedwater)
	  {
		  DemoPoint.pos.y += 512;
		  base = 1424;
	  }
	  if (killerDino->Clone == AI_BRACHDANGER || killerDino->Clone == AI_LANDBRACH)
	  {
		  DemoPoint.pos.y += 850;
		  base = 1424;
	  }

  }
  */

  DemoPoint.pos = Characters[DemoPoint.CIndex].pos;
  float base;
  if (killerDino) {
	  base = DinoInfo[killerDino->CType].camBase;
	  if (killedwater) {
		  base = DinoInfo[killerDino->CType].camBaseWater;
		  DemoPoint.pos.y += DinoInfo[killerDino->CType].camDemoPointWater;
	  }
	  else {
		  DemoPoint.pos.y += DinoInfo[killerDino->CType].camDemoPoint;
	  }
  } else base = 824; //for drowning/poison fog
  

  //if (Characters[DemoPoint.CIndex].Clone ==AI_TREX) DemoPoint.pos.y+=512;


  Vector3d nv = SubVectors(DemoPoint.pos,  CameraPos);
  Vector3d pp = DemoPoint.pos;
  pp.y = CameraPos.y;
  float l = VectorLength( SubVectors(pp,  CameraPos) );

  if (DemoPoint.DemoTime==1)
    if (l < base) DemoPoint.DemoTime = 2;
  NormVector(nv, 1.0f);

  if (DemoPoint.DemoTime == 1)
  {
    DeltaFunc(CameraX, DemoPoint.pos.x, static_cast<float>(fabs(nv.x)) * TimeDt * 3.f);
    DeltaFunc(CameraZ, DemoPoint.pos.z, static_cast<float>(fabs(nv.z)) * TimeDt * 3.f);
  }
  else
  {
    DemoPoint.DemoTime+=TimeDt;
    CameraAlpha+=TimeDt / 1224.f;
    ca = static_cast<float>(cos(CameraAlpha));
    sa = static_cast<float>(sin(CameraAlpha));
    //float k = (base - l) / 350.f;
    DeltaFunc(CameraX, DemoPoint.pos.x  - sa * base, static_cast<float>(TimeDt) );
    DeltaFunc(CameraZ, DemoPoint.pos.z  + ca * base, static_cast<float>(TimeDt) );
  }

  float b = FindVectorAlpha( static_cast<float>(sqrt ( (DemoPoint.pos.x - CameraX)*(DemoPoint.pos.x - CameraX) +
                                    (DemoPoint.pos.z - CameraZ)*(DemoPoint.pos.z - CameraZ) )),
                             DemoPoint.pos.y - CameraY - 400.f);
  if (b>pi) b = b - 2*pi;
  DeltaFunc(CameraBeta, -b, TimeDt / 4000.f);



  float h = GetLandQH(CameraX, CameraZ);
  DeltaFunc(CameraY, h+128, TimeDt / 8.f);
  if (CameraY < h + 80) CameraY = h + 80;
}




void ProcessControls()
{
  int _KeyFlags = KeyFlags;
  KeyFlags = 0;
  GetKeyboardState(KeyboardState);

  
  if (KeyboardState[KeyMap.fkReload] & 128)  KeyFlags += kfLookUp;
  if (KeyboardState[KeyMap.fkResupply] & 128)  KeyFlags += kfLookDn;

  if (g_GameMode != GameMode::SurvivalMode) {
    if (KeyboardState [KeyMap.fkStrafe] & 128) KeyFlags+=kfStrafe;

	if (KeyboardState [KeyMap.fkForward ] & 128) KeyFlags+=kfForward;
	if (KeyboardState [KeyMap.fkBackward] & 128) KeyFlags+=kfBackward;
	//if (KeyboardState[KeyMap.fkCrouch] & 128) KeyFlags += kfDown;


	/*
  if (KeyFlags & kfStrafe)
  {
    if (KeyboardState [KeyMap.fkLeft ] & 128)  KeyFlags+=kfSLeft;
    if (KeyboardState [KeyMap.fkRight] & 128) KeyFlags+=kfSRight;
  }
  else
  {
    if (KeyboardState [KeyMap.fkLeft ] & 128)  KeyFlags+=kfLeft;
    if (KeyboardState [KeyMap.fkRight] & 128) KeyFlags+=kfRight;
  }*/

  if (KeyboardState [KeyMap.fkSLeft]  & 128) KeyFlags+=kfSLeft;
  if (KeyboardState [KeyMap.fkSRight] & 128) KeyFlags+=kfSRight;


  if (KeyboardState [KeyMap.fkJump] & 128) KeyFlags+=kfJump;

  if (KeyboardState [KeyMap.fkCall] & 128)
    if (!(_KeyFlags & kfCall)) KeyFlags+=kfCall;

  }

  DeltaT = static_cast<float>(TimeDt) / 1000.f;

  if ( DemoPoint.DemoTime) ProcessDemoMovement();
  if (!DemoPoint.DemoTime) ProcessPlayerMovement();


//======= Y movement ===========//
  HeadAlpha = HeadBackR / 20000;
  HeadBeta =-HeadBackR / 10000;
  if (HeadBackR)
  {
    HeadBackR-=DeltaT*(80 + (32-static_cast<float>(fabs(HeadBackR - 32)))*4);
    if (HeadBackR<=0)
    {
      HeadBackR = 0;
      HeadBSpeed = 0;
    }
  }

  if ((g_GameMode == GameMode::Crouching) | (IsUnderwater()) )
  {
    if (HeadY<110.f) HeadY = 110.f;
    HeadY-=DeltaT*(60 + (HeadY-110)*5);
    if (HeadY<110.f) HeadY = 110.f;
  }
  else
  {
    if (HeadY>220.f) HeadY = 220.f;
    HeadY+=DeltaT*(60 + (220 - HeadY) * 5);
    if (HeadY>220.f) HeadY = 220.f;
  }


  float h  = GetLandQH(PlayerX, PlayerZ);
  float hu = GetLandCeilH(PlayerX, PlayerZ)-64;
  float hwater = GetLandUpH(PlayerX, PlayerZ);

  if (DemoPoint.DemoTime) goto SKIPYMOVE;

  if (!IsUnderwater())
  {
    if (PlayerY>h) YSpeed-=DeltaT*3000;
  }
  else if (YSpeed<0)
  {
    YSpeed+=DeltaT*4000;
    if (YSpeed>0) YSpeed=0;
  }

  if (FLY) YSpeed=0;
  PlayerY+=YSpeed*DeltaT;


  if (PlayerY+HeadY>hu)
  {
    if (YSpeed>0) YSpeed=-1;
    PlayerY = hu - HeadY;
    if (PlayerY<h)
    {
      PlayerY = h;
      HeadY = hu - PlayerY;
      if (HeadY<110) HeadY = 110;
    }
  }

  if (PlayerY<h)
  {
    if (YSpeed<-800) HeadY+=YSpeed/100;
    if (PlayerY < h-80) PlayerY = h - 80;
    PlayerY+=(h-PlayerY+32)*DeltaT*4;
    if (PlayerY>h) PlayerY = h;
    if (YSpeed<-600)
      AddVoicev(fxStep[(RealTime % 3)].length,
                fxStep[(RealTime % 3)].lpData.data(), 64);
    YSpeed = 0;
  }

SKIPYMOVE:

  SWIM = false;
  if (!IsUnderwater() && (KeyFlags & kfJump) )
    if (PlayerY<hwater-148)
    {
      SWIM = true;
      PlayerY = hwater-148;
      YSpeed = 0;
    }

  float _s = stepdy;

  if (g_GameMode == GameMode::Swimming) stepdy = static_cast<float>(sin(static_cast<float>(RealTime) / 360)) * 20;
  else stepdy = static_cast<float>(MIN(1.f,fabs(VSpeed) + static_cast<float>(fabs(SSpeed)))) * static_cast<float>(sin(static_cast<float>(RealTime) / 80.f)) * 22.f;
  float d = stepdy - _s;

  if (!IsUnderwater())
    if (PlayerY<h+64)
      if (d<0 && stepdd >= 0)
        if (ONWATER)
        {
          AddWCircle(CameraX, CameraZ, 1.2);
          AddVoicev(fxStepW[(RealTime % 3)].length,
                    fxStepW[(RealTime % 3)].lpData.data(), 64+static_cast<int>((VSpeed*30.f)));
        }
        else
          AddVoicev(fxStep[(RealTime % 3)].length,
                    fxStep[(RealTime % 3)].lpData.data(), 24+static_cast<int>((VSpeed*50.f)));
  stepdd = d;

  if (PlayerBeta> 1.46f) {
    PlayerBeta= 1.46f;
    // Don't let rbv keep accumulating against the clamp — when the user
    // reverses direction the accumulated positive rbv would otherwise
    // need 3-10 frames to decay before the new negative deltas can move
    // PlayerBeta back down, causing a sluggish "push through" section
    // and a jittery release near the vertical extremes.
    if (rbv > 0) rbv = 0;
  }
  if (PlayerBeta<-1.26f) {
    PlayerBeta=-1.26f;
    if (rbv < 0) rbv = 0;
  }


//======== set camera pos ===================//

  if (Recoil.y < 0) {
	  Recoil.y += 0.005;
	  if (Recoil.y > 0) Recoil.y = 0;
  }
  if (Recoil.x != 0) DeltaFunc(Recoil.x, 0, 0.005);

  if (!DemoPoint.DemoTime)
  {

	PlayerAlpha += Recoil.x;
	PlayerBeta += Recoil.y;

    CameraAlpha = PlayerAlpha + HeadAlpha;
    CameraBeta  = PlayerBeta  + HeadBeta;

	CameraX = PlayerX - sa * HeadBackR;
    CameraY = PlayerY + HeadY + stepdy;// + 2024;
    CameraZ = PlayerZ + ca * HeadBackR;
  }

  if (CLIP3D)
  {
    // Scale the cull distance by the aspect ratio so terrain triangles
    // at the screen edges aren't culled on widescreen displays. At 4:3
    // aspectScale=1.0 (no change vs the legacy hardcoded values); at
    // 16:9 it's 1.333, at 21:9 it's 1.75. Floor at 1.0 so a taller
    // screen never shrinks the cull distance. C1 has the same logic
    // here and at the binocular near-model site in InsertModelList.
    float aspectScale = (static_cast<float>(WinW) / static_cast<float>(WinH)) / (4.0f / 3.0f);
    if (aspectScale < 1.0f) aspectScale = 1.0f;
    if (sb<0) BackViewR = (320.f - 1024.f * sb) * aspectScale;
    else BackViewR = (320.f + 512.f * sb) * aspectScale;
    BackViewRR = static_cast<int>(((380 + static_cast<int>((1024 * fabs(sb)))) * aspectScale));
    if (IsUnderwater()) BackViewR -= 512.f * static_cast<float>(MIN(0,sb)) * aspectScale;
  }
  else
  {
    BackViewR = 300;
    BackViewRR = 380;
  }


//==================== SWIM & UNDERWATER =========================//
  ONWATER = GetLandUpH(CameraX, CameraZ) > GetLandH(CameraX, CameraZ);

  if (IsUnderwater())
  {
    UNDERWATER = (GetLandUpH(CameraX, CameraZ)-4>= CameraY);
    if (!IsUnderwater())
    {
      HeadY+=20;
      CameraY+=20;
      AddVoicev(fxWaterOut.length, fxWaterOut.lpData.data(), 256);
      AddWCircle(CameraX, CameraZ, 2.0);
    }
  }
  else
  {
    UNDERWATER = (GetLandUpH(CameraX, CameraZ)+28 >= CameraY);
    if (IsUnderwater())
    {
      HeadY-=20;
      CameraY-=20;
      g_GameMode = GameMode::Normal;
      AddVoicev(fxWaterIn.length, fxWaterIn.lpData.data(), 256);
      AddWCircle(CameraX, CameraZ, 2.0);
    }
  }

  if (MyHealth)
    if (IsUnderwater())
    {
      MyHealth-=TimeDt*12;
      //if ( !(Takt & 31)) AddElements(CameraX + sa*64*cb, CameraY - 32 - sb*64, CameraZ - ca*64*cb, 4);
      if (MyHealth<=0)
        AddDeadBody(nullptr, HUNT_BREATH, true);
    }

  if (IsUnderwater() && !WeapInfo[CurrentWeapon].harpoon)
    if (Weapon.state) HideWeapon();

  if (!IsUnderwater()) UnderWaterT = 0;
  else if (UnderWaterT<512) UnderWaterT += TimeDt;
  else UnderWaterT = 512;

  if (IsUnderwater())
  {
	  if (MyHealth) {
		// Underwater camera has a wobble + dive-recovery effect. The base
		// FovScaleFromDegrees(OptFov) is the same as the above-water case
		// (drives vertical FOV) and the wobble terms add to it. C1 uses
		// VideoCY for the base too, so the underwater H-FOV matches
		// above-water H-FOV (i.e. the wobble affects both axes equally).
		CameraH = static_cast<float>(VideoCY) * (FovScaleFromDegrees(OptFov) + (1.f + static_cast<float>(sin(RealTime / 180.f))) / 30 - (1.f - static_cast<float>(sin(UnderWaterT / 512.f*pi / 2))) / 16.f);
		CameraW = static_cast<float>(VideoCY) * (FovScaleFromDegrees(OptFov) + (1.f + static_cast<float>(cos(RealTime / 180.f))) / 30 + (1.f - static_cast<float>(sin(UnderWaterT / 512.f*pi / 2))) / 1.5f);
		// Keep square pixels (see SetVideoMode comment).
		// The old C2 ME code dropped the *1.25f from C1 and used
		// VideoCX (a horizontal term) which made the underwater effect
		// aspect-dependent in confusing ways. Mirroring C1's structure
		// here keeps the underwater camera consistent across resolutions.

		CameraAlpha += static_cast<float>(cos(RealTime / 360.f)) / 120;
		CameraBeta += static_cast<float>(sin(RealTime / 360.f)) / 100;
		CameraY -= static_cast<float>(sin(RealTime / 360.f)) * 4;
	  }

	int w = WMap[((static_cast<int>((CameraZ)))>>8) ][ ((static_cast<int>((CameraX)))>>8) ];
    FogsList[127].YBegin = static_cast<float>(WaterList[w].wlevel);
    FogsList[127].fogRGB = WaterList[w].fogRGB;
  }
  else
  {
    // See Interface.cpp:SetVideoMode for why we use VideoCY (not
    // VideoCX) and FovScaleFromDegrees(OptFov) here. Matches the
    // SetVideoMode() formula so the per-frame camera matches the
    // startup camera, with no drift between SetVideoMode and the
    // per-frame reset.
    CameraH = static_cast<float>(VideoCY) * FovScaleFromDegrees(OptFov);
    CameraW = CameraH;
  }


  if (g_GameMode == GameMode::Binocular)
  {
    CameraW*=BinocularPower;
    CameraH*=BinocularPower;
  }
  else if (g_GameMode == GameMode::OpticScope && (!WeapInfo[CurrentWeapon].unzoom || Weapon.state == 2))
  {
	  CameraW *= WeapInfo[CurrentWeapon].Optic;
	  CameraH *= WeapInfo[CurrentWeapon].Optic;
  }

  // FOVK is a frustum-cull coefficient used by the renderer (e.g.
  // RenderSoft.cpp: `if (fabs(xx*FOVK) > -zz + BackR) return;`). The
  // correct coefficient for the new projection is CameraW/VideoCX:
  // solving `|xx| * FOVK = -zz` for the screen edge gives
  // FOVK = CameraW/VideoCX. The old `CameraW / (VideoCX*1.25f)` was
  // tied to the previous CameraW = VideoCX*1.25f formula and gave
  // FOVK = 1.0 at 4:3; the new projection is calibrated by V-FOV
  // instead, so the 1.25f no longer applies.
  FOVK = CameraW / static_cast<float>(VideoCX);

  InitClips();

  if (g_GameMode == GameMode::Swimming)
  {
    if (!(Takt & 31)) AddWCircle(CameraX, CameraZ, 1.5);
    CameraBeta -=static_cast<float>(cos(RealTime/360.f)) / 80;
    PlayerX+=DeltaT*32;
    PlayerZ+=DeltaT*32;
  }


  CameraFogI = FogsMap [(static_cast<int>(CameraZ))>>9][(static_cast<int>(CameraX))>>9];
  if (IsUnderwater()) CameraFogI=127;
  if (FogsList[CameraFogI].YBegin*ctHScale> CameraY)
    CAMERAINFOG = (CameraFogI>0);
  else
    CAMERAINFOG = false;

  if (CAMERAINFOG)
    if (MyHealth)
      if (FogsList[CameraFogI].Mortal)
      {
        if (MyHealth>100000) MyHealth = 100000;
        MyHealth-=TimeDt*64;
        if (MyHealth<=0)
          AddDeadBody(nullptr, HUNT_EAT, true);
      }

  int CameraAmb = AmbMap [(static_cast<int>(CameraZ))>>9][(static_cast<int>(CameraX))>>9];


  if (IsUnderwater())
  {
    SetAmbient(fxUnderwater.length,
               fxUnderwater.lpData.data(),
               240);
    Audio_SetEnvironment(8, ctViewR*256);
  }
  else
  {
    SetAmbient(Ambient[CameraAmb].sfx.length,
               Ambient[CameraAmb].sfx.lpData.data(),
               Ambient[CameraAmb].AVolume);
    Audio_SetEnvironment(Ambient[CameraAmb].rdata[0].REnvir, ctViewR*256);

    Env = Ambient[CameraAmb].rdata[0].REnvir;

    if (Ambient[CameraAmb].RSFXCount)
    {
      Ambient[CameraAmb].RndTime-=TimeDt;
      if (Ambient[CameraAmb].RndTime<=0)
      {
        Ambient[CameraAmb].RndTime = (Ambient[CameraAmb].rdata[0].RFreq / 2 + rRand(Ambient[CameraAmb].rdata[0].RFreq)) * 1000;
        int rr = (rand() % Ambient[CameraAmb].RSFXCount);
        int r = Ambient[CameraAmb].rdata[rr].RNumber;
        AddVoice3dv(RandSound[r].length, RandSound[r].lpData.data(),
                    CameraX + siRand(4096),
                    CameraY + siRand(256),
                    CameraZ + siRand(4096),
                    Ambient[CameraAmb].rdata[rr].RVolume);
      }
    }
  }


  if (NOCLIP) CameraY+=1024;
  //======= results ==========//
  if (CameraBeta> 1.46f) CameraBeta= 1.46f;
  if (CameraBeta<-1.26f) CameraBeta=-1.26f;

  PlayerPos.x = PlayerX;
  PlayerPos.y = PlayerY;
  PlayerPos.z = PlayerZ;

  CameraPos.x = CameraX;
  CameraPos.y = CameraY;
  CameraPos.z = CameraZ;

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

  // Phase 2.1: build per-frame render context and pass to the renderer
  RenderFrameContext ctx = RenderFrameContext::FromGlobals();
#ifdef _gl
  if (g_GLRenderer) g_GLRenderer->DrawFrame(ctx);
#endif

  DrawScene();

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
