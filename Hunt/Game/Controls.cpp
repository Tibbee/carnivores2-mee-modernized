// ==========================================================================
// Controls.cpp
// ==========================================================================

#include "Hunt.h"
#include <algorithm>

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
  if (g_GameMode == GameMode::Swimming && !UNDERWATER && !(KeyFlags & kfJump)) {
    g_GameMode = GameMode::Normal;
  }
  if (!IsUnderwater() && (KeyFlags & kfJump) )
    if (PlayerY<hwater-148)
    {
      SWIM = true;
      g_GameMode = GameMode::Swimming;
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

  if (UNDERWATER)
  {
    UNDERWATER = (GetLandUpH(CameraX, CameraZ)-4>= CameraY);
    if (!UNDERWATER)
    {
      HeadY+=20;
      CameraY+=20;
      AddVoicev(fxWaterOut.length, fxWaterOut.lpData.data(), 256);
      AddWCircle(CameraX, CameraZ, 2.0);
      g_GameMode = GameMode::Normal;
    }
    if (UNDERWATER) {
        g_GameMode = GameMode::Underwater;
    }
  }
  else
  {
    UNDERWATER = (GetLandUpH(CameraX, CameraZ)+28 >= CameraY);
    if (UNDERWATER)
    {
      HeadY-=20;
      CameraY-=20;
      AddVoicev(fxWaterIn.length, fxWaterIn.lpData.data(), 256);
      AddWCircle(CameraX, CameraZ, 2.0);
      g_GameMode = GameMode::Underwater;
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

  // Cache the camera's underwater depth factor (0..1) once per frame for
  // fog/FX calculations.  CalcFogLevel() is called many times per frame
  // from multiple renderers; this avoids repeating the heightmap lookup.
  if (IsUnderwater()) {
    const float waterLevel = GetLandUpH(CameraX, CameraZ);
    CameraWaterDepthFactor = std::clamp((waterLevel - CameraY) / 1024.0f, 0.0f, 1.0f);
  } else {
    CameraWaterDepthFactor = 0.0f;
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