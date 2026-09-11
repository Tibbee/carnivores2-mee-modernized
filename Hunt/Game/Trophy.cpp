// Trophy.cpp -- auto-extracted from Game.cpp
// ==========================================================================
// Auto-extracted from Game.cpp
// ==========================================================================

#include "Hunt.h"
#include <mmsystem.h>

// Constants from Projectiles.cpp
#define partBlood   1
#define partWater   2
#define partGround  3
#define partBubble  4

// Forward declarations from Ships.cpp
extern void AnimateShip();
extern void AnimateSShip();
extern void AnimateBag();

// Forward declaration from EngineInit.cpp
extern void SetupRes();


void ProcessTrophy()
{
  TrophyBody = -1;

  for (int c=0; c<ChCount; c++)
  {
  // Only stuffed mounts feed the plaque: live characters carry AI
  // states that can numerically equal a body slot (a companion at
  // state 5 beside an occupied slot 5 shows a stranger trophy plaque).
  // Mounts are exactly the StateF == 0xFF characters (see PlaceTrophy).
  // Hunt claim plaques are populated separately by AnimateCharacters.
  if (Characters[c].StateF != 0xFF) continue;
  const int slot = Characters[c].State;
  if (slot < 0 || slot >= TROPHY2_COUNT) continue;


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
  // The proximity scan feeds the info plaque (TrophyBody). It must run
  // in the trophy room too: the room runs in Normal mode (trophy-ness
  // is only the map name), so gating on TrophyMode alone left every
  // mount mute. Harmless elsewhere: in hunts TrophyBody was already set.
  if (InTrophyRoom())
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
  if (!InTrophyRoom()) return;
  if (TrophyBody < 0 || TrophyBody >= TROPHY2_COUNT) return;
  if (!TrophyRoom2.Body[TrophyBody].ctype) return;

  // Placement may skip invalid saved species, so find the mount by slot.
  int p = 0;
  while (p < ChCount &&
         (Characters[p].StateF != 0xFF || Characters[p].State != TrophyBody)) ++p;
  if (p == ChCount) return;



  PrintLogVerbose("Trophy removed: ");
  //PrintLog(DinoInfo[TrophyRoom.Body[TrophyBody].ctype].Name);
  PrintLogVerbose(DinoInfo[TrophyRoom2.Body[TrophyBody].ctype].Name);
  PrintLogVerbose("\n");

  
  TrophyRoom2.Body[TrophyBody] = {};

  for (int c = p; c + 1 < ChCount; ++c)
    Characters[c] = Characters[c + 1];
  Characters[--ChCount] = {};





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


  {
    // Saves can predate the modernization or come from other mods with a
    // different layout. A short read here used to leave KeyMap half-filled
    // with garbage, so every key felt broken until rebound in the menu
    // (sprint was the visible one: fkRun read as 0). Keep the InitEngine
    // defaults unless the file actually holds a full KeyMap.
    struct _t savedKeys;
    DWORD kl = 0;
    ReadFile(hfile, &savedKeys, sizeof(savedKeys), &kl, nullptr);
    if (kl == sizeof(savedKeys)) {
      KeyMap = savedKeys;
    } else {
      char msg[128];
      sprintf_s(msg, sizeof(msg), "Trophy: short KeyMap read (%u of %u), keeping defaults.\n",
                (unsigned)kl, (unsigned)sizeof(savedKeys));
      PrintLog(msg);
      CloseHandle(hfile);
      TrophyRoom.RegNumber = rn;
      SetupRes();
      PrintLog("Trophy Loaded (legacy/short save, defaults kept).\n");
      if (TrophyRoom.Body[0].ctype) LoadTrophy2(TrophyRoom.RegNumber);
      else TrophyRoom.Body[0].ctype = 1;
      return;
    }
  }
  ReadFile(hfile, &REVERSEMS, 4, &l, nullptr);
  //  Ignore savefile settings for equipment -- skip 4 DWORDs
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
