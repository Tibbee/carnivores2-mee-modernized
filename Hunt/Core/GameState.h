// GameState.h -- All global state declarations (replaces _EXTORNOT pattern)
// GLOBAL=extern normally, empty when GLOBAL_DEFINE is defined (StateDefs.cpp)
#pragma once

#ifdef GLOBAL_DEFINE
#define GLOBAL
#else
#define GLOBAL extern
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include "Memory.h"
#include "Core/Constants.h"
#include "Core/MathTypes.h"
#include "Core/AudioTypes.h"
#include "Core/RenderTypes.h"
#include "Core/ModelTypes.h"
#include <array>
#include "ddraw.h"
#include "Core/GameTypes.h"

class MemoryArena;
GLOBAL MemoryArena *LevelArena;

GLOBAL   char logt[128];

GLOBAL   float BackViewR;

GLOBAL   int   BackViewRR;

GLOBAL   int   UnderWaterT;

GLOBAL   int   TotalTreeTable, TotalAreaInfo, TotalSpawnGroup, TotalC, TotalW, TotalMA, TotalTrophy;// , TotalRegion, TotalAvoid;

GLOBAL   char    ServerAddress[128];

GLOBAL   WSADATA wsaData;

GLOBAL   int iResult;

GLOBAL   SOCKET ListenSocket;

GLOBAL   SOCKET ClientSocket;

GLOBAL   SOCKET ConnectSocket;

GLOBAL   struct addrinfo *result;

GLOBAL   struct addrinfo hints;

GLOBAL   int iSendResult;

GLOBAL   HANDLE CommsThreadHandle;

GLOBAL   LPDWORD CommsThreadID;

GLOBAL   BOOL HaltThread;

GLOBAL   char recvbuf[DEFAULT_BUFLEN];

GLOBAL   int recvbuflen;

GLOBAL   HWND    hwndMain;

GLOBAL   HINSTANCE  hInst;

GLOBAL   HANDLE  Heap;

GLOBAL   HDC     hdcMain, hdcCMain;

GLOBAL   BOOL    blActive;

GLOBAL   BYTE    KeyboardState[256];

GLOBAL   int     KeyFlags, _shotcounter;

GLOBAL   TMessageList MessageList;

GLOBAL   char    ProjectName[128];

GLOBAL   int     _GameState, _MultiplayerState;//multiplayer

GLOBAL   TSFX    fxBlip;

GLOBAL   TSFX    fxClick[3];

GLOBAL   TSFX    fxBreathIn;

GLOBAL   TSFX    fxBreathOut;

GLOBAL   TSFX    fxCollect[3];

GLOBAL   TSFX    fxImpactAquatic[3];

GLOBAL   TSFX    fxImpactGround[3];

GLOBAL   TSFX    fxImpactModel[3];

GLOBAL   TSFX    fxImpactWater[3];

GLOBAL   TSFX    fxImpactChar[3];

GLOBAL   TSFX    fxCall[10][3], fxScream[4];

GLOBAL   TSFX	fxGunShot[11];

GLOBAL   TSFX    fxUnderwater, fxWaterIn, fxWaterOut, fxJump, fxStep[3], fxStepW[3];

GLOBAL   unsigned char HMap[ctMapSize][ctMapSize];

GLOBAL   unsigned char WMap[ctMapSize][ctMapSize];

GLOBAL   unsigned char HMapO[ctMapSize][ctMapSize];

GLOBAL   WORD FMap[ctMapSize][ctMapSize];

GLOBAL   unsigned char LMap[ctMapSize][ctMapSize];

GLOBAL   WORD TMap1[ctMapSize][ctMapSize];

GLOBAL   WORD TMap2[ctMapSize][ctMapSize];

GLOBAL   unsigned char OMap[ctMapSize][ctMapSize];

GLOBAL   unsigned char FogsMap[512][512];

GLOBAL   unsigned char AmbMap[512][512];

GLOBAL   TFogEntity    FogsList[256];

GLOBAL   TWaterEntity  WaterList[256];

GLOBAL   TWind       Wind;

GLOBAL   TShip       Ship;

GLOBAL   TShip       SShip;

GLOBAL   TShipTask   ShipTask;

GLOBAL   TBag        AmmoBag;

GLOBAL   int SkyR, SkyG, SkyB, WaterR, WaterG, WaterB, WaterA,
            SkyTR,SkyTG,SkyTB, CurFogColor;

GLOBAL   int RandomMap[32][32];

GLOBAL   Vector2df *PhongMapping;

GLOBAL   TPicture TFX_SPECULAR, TFX_ENVMAP;

GLOBAL   WORD SkyPic[256*256];

GLOBAL   WORD SkyFade[9][128*128];

GLOBAL   BYTE SkyMap[128*128];

GLOBAL   std::array<unique_obj_ptr<TEXTURE>, 1024> Textures;

GLOBAL   TAmbient Ambient[256];

GLOBAL   TSFX     RandSound[256];

GLOBAL TSnowType SnowInfo[32];

GLOBAL int SnowCh;

GLOBAL int TargetDino, TargetArea, TargetWeapon, WeaponPres, TargetCall,
          ObservMode, Tranq, ObjectsOnLook, RenderHitBox,
          CurrentWeapon, ShotsLeft[10], AmmoMag[10],
	MagShotsLeft[10], Chambered[10], FiringMode[10]; //TrophyTime,

GLOBAL bool alreadyFired;

GLOBAL Vector3d answpos;

GLOBAL int answtime, answcall;

GLOBAL BOOL NightVisionMode, NightVisionOn;

GLOBAL BOOL ScentMode, CamoMode,
          RadarMode, LockLanding,
          TrophyMode, DoubleAmmo,
          DogMode, Multiplayer,
          Host, CiskMode, SonarMode,
          ScannerMode, SurvivalMode;

GLOBAL float ScoreMod_Camo;

GLOBAL float ScoreMod_Radar;

GLOBAL float ScoreMod_Scent;

GLOBAL float ScoreMod_Double;

GLOBAL float ScoreMod_Tranq;

GLOBAL float ScoreMod_Observer;

GLOBAL float sonarPos;

GLOBAL TTrophyRoom TrophyRoom;

GLOBAL TTrophyRoom2 TrophyRoom2;

GLOBAL TPicture LandPic,DinoPic,DinoPicM, MapPic, WepPic;

GLOBAL HFONT fnt_BIG, fnt_Small, fnt_Midd;

GLOBAL TLandingList LandingList;

GLOBAL TBullet bullet[256];

GLOBAL int bulletCh;

GLOBAL Vector3d TraceB;

GLOBAL TObject  MObjects[256];

GLOBAL TModel* mptr;

GLOBAL TWeapon Weapon;

GLOBAL int   OCount, iModelFade, iModelBaseFade, Current;

GLOBAL Vector3d  *rVertex;

GLOBAL TObj      gObj[1024];

GLOBAL Vector2di *gScrp;

GLOBAL int MaxObjectVCount; // Maximum VCount of any (loaded) object

GLOBAL TPicture  PausePic, ExitPic, TrophyExit, TrophyPic, TrophyNoCollectPic, ScorePic;

GLOBAL unique_obj_ptr<TModel> SunModel;

GLOBAL TCharacterInfo WCircleModel;

GLOBAL unique_obj_ptr<TModel> CompasModel;

GLOBAL unique_obj_ptr<TModel> Binocular;

GLOBAL TDinoInfo DinoInfo[DINOINFO_MAX];

GLOBAL TMenuDinoInfo MenuDinoInfo[16];

GLOBAL int sendGunShot;

GLOBAL int mGunShot[4];

GLOBAL int sendHunterCall;

GLOBAL int sendHunterCallType;

GLOBAL int mHunterCall[4];

GLOBAL int mHunterCallType[4];

GLOBAL int sendDamage[DINOINFO_MAX];

GLOBAL int mDamage[4][DINOINFO_MAX];

GLOBAL bool TreeTable[255];

GLOBAL TAIInfo AIInfo[DINOINFO_MAX];

GLOBAL TWeapInfo WeapInfo[10];

GLOBAL bool Muzz;

GLOBAL int MuzzFTime;

GLOBAL float MuzzGamma;

GLOBAL TCharacterInfo MuzzModel;

GLOBAL TCharacterInfo ShipModel;

GLOBAL TCharacterInfo SShipModel;

GLOBAL TCharacterInfo BagModel;

GLOBAL TSpawnGroup spawnGroup[256];

GLOBAL int trophyGroupCount;

GLOBAL TPackType packType[1024];

GLOBAL int packTypeCount;

GLOBAL TTrophyType trophyType[TROPHY2_COUNT];

GLOBAL int trophyTypeCount;

GLOBAL int ChCount, WCCount, ElCount,
          ShotDino, TrophyBody, HunterCount; //HunterCount is for multiplayer, up to 3 others

GLOBAL bool TrophyDisplay;

GLOBAL int TrophyDisplayC;

GLOBAL int ScoreDispTime;

GLOBAL int ScoreDisp;

GLOBAL TTrophyItem TrophyDisplayBody;

GLOBAL TCharacterInfo WindModel;

GLOBAL TCharacterInfo PlayerInfo;

GLOBAL TCharacterInfo ChInfo[DINOINFO_MAX];

GLOBAL TCharacterInfo MPlayerInfo[3]; //multiplayer

GLOBAL TCharacterInfo HitBoxModel;

GLOBAL TPack          Packs[256];

GLOBAL int PackCount;

GLOBAL TCharacter     Characters[256];

GLOBAL TCharacter     MPlayers[3]; //multiplayer

GLOBAL THitBox     HitBox;

GLOBAL int SurvivalSpawnX; //survival

GLOBAL int SurvivalSpawnZ;

GLOBAL float SurvivalSpawnA;

GLOBAL TSpawnRegion SurvivalDinoSpawn; //dino spawn zone

GLOBAL int SurvivalWave;

GLOBAL int SurvivalIndex[128];

GLOBAL int SurvivalIndexCh;

GLOBAL TWCircle       WCircles[2096]; //increased

GLOBAL TSnowElement*  Snow;

GLOBAL TDemoPoint     DemoPoint;

GLOBAL TCharacter     *killerDino;

GLOBAL BOOL			 killedwater;

GLOBAL TPlayer        Players[16];

GLOBAL Vector3d       PlayerPos, CameraPos;

GLOBAL   LPDIRECTDRAW lpDD;

GLOBAL   LPDIRECTDRAW2 lpDD2;

GLOBAL   void* lpVideoRAM;

GLOBAL   LPDIRECTDRAWSURFACE lpddsPrimary;

GLOBAL   BOOL DirectActive, FULLSCREEN, BORDERLESS, RestartMode;

GLOBAL   BOOL LoDetailSky;

GLOBAL   int  WinW,WinH,WinEX,WinEY,VideoCX,VideoCY,VideoPitch,VideoPitchB,iBytesPerLine,ts,r,MapMinY;

GLOBAL   float CameraW,CameraH,Soft_Persp_K, stepdy, stepdd, SunShadowK, FOVK;

GLOBAL   CLIPPLANE ClipA,ClipB,ClipC,ClipD,ClipZ,ClipW;

GLOBAL   int u,vused, CCX, CCY;

GLOBAL   DWORD Mask1,Mask2;

GLOBAL   DWORD HeapAllocated, HeapReleased;

GLOBAL   EPoint VMap[kViewGridSize][kViewGridSize];

GLOBAL   EPoint VMap2[kViewGridSize][kViewGridSize];

GLOBAL   EPoint ev[3];

GLOBAL   ClipPoint cp[16];

GLOBAL   ClipPoint hleft,hright;

GLOBAL   void  *HLineT;

GLOBAL   int   rTColor;

GLOBAL   int   SKYMin, SKYDTime, GlassL, ctViewR, ctViewR1, ctViewRM,
            dFacesCount, ReverseOn, TDirection;

GLOBAL   WORD  FadeTab[65][0x8000];

GLOBAL   TElements Elements[700];

GLOBAL   TBTrail   BloodTrail;

GLOBAL   int     PrevTime, TimeDt, T, Takt, RealTime, StepTime, MyHealth, ExitTime, WaveNoteTime,
            ChCallTime, CallLockTime, NextCall;

GLOBAL   float   DeltaT;

GLOBAL   float   CameraX, CameraY, CameraZ, CameraAlpha, CameraBeta;

GLOBAL   float   PlayerX, PlayerY, PlayerZ, PlayerAlpha, PlayerBeta,
            HeadY, HeadBackR, HeadBSpeed, HeadAlpha, HeadBeta,
            SSpeed,VSpeed,RSpeed,YSpeed;

GLOBAL   Vector3d PlayerNv;

GLOBAL   float   ca,sa,cb,sb, wpnDAlpha, wpnDBeta;

GLOBAL   void    *lpVideoBuf, *lpTextureAddr;

GLOBAL   HBITMAP hbmpVideoBuf;

GLOBAL   HCURSOR hcArrow;

GLOBAL   int     DivTbl[10240];

GLOBAL   Vector3d  v[3];

GLOBAL   ScrPoint  scrp[3];

GLOBAL   MScrPoint mscrp[3];

GLOBAL   Vector3d  nv, waterclipbase, Sun3dPos;

GLOBAL   struct _t
{
  int fkForward, fkBackward, fkReload, fkResupply, fkHoldBreath, fkFiringMode, fkFire, fkShow, fkSLeft, fkSRight, fkStrafe, fkJump, fkRun, fkCrouch, fkCall, fkCCall, fkBinoc;
} KeyMap;

GLOBAL int WATERANI, Clouds, SKY, GOURAUD,
          MODELS, TIMER, BITMAPP, MIPMAP,
          NOCLIP, CLIP3D, NODARKBACK, CORRECTION, LOWRESTX,
          FOGENABLE, FOGON, CAMERAINFOG,
          WATERREVERSE, waterclip, UNDERWATER, ONWATER, NeedWater,
          SWIM, FLY, PAUSE, OPTICMODE, BINMODE, EXITMODE, MapMode, RunMode, CrouchMode;

GLOBAL int  CameraFogI;

GLOBAL int OptDayNight, OptAgres, OptDens, OptSens, OptRes, OptViewR,
          OptMsSens, OptBrightness, OptSound, OptRender, OptObjectDetail,
          OptText, OptSys, WaitKey, OPT_ALPHA_COLORKEY;

GLOBAL int  NightVisionKey;

GLOBAL int  OptFov;

GLOBAL int  OptFpsLimit;

GLOBAL int  OptTerrainLOD;

GLOBAL float UIScale;

GLOBAL int  CurRes, ResCount;

GLOBAL TRes ResolutionList[128];

GLOBAL BOOL SHADOWS3D,REVERSEMS;

GLOBAL BOOL SLOW, DEBUG, MORPHP, MORPHA;

GLOBAL bool g_VerboseLogging;

GLOBAL HANDLE hlog;

GLOBAL int AudioFCount;

GLOBAL AudioQuad data[8192];

GLOBAL void UploadGeometry();

GLOBAL int Env;

GLOBAL BOOL HARD3D;


GLOBAL char KeysName[256][24];

GLOBAL Vector3d Recoil;
