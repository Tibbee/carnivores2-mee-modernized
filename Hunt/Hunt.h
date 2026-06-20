#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <type_traits>
#include <vector>
#include <array>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

//1.0.4		=4
//1.0.5		=5
//1.0.6		=6
//1.0.6.1	=7
//1.1		=8
inline constexpr int MODDERS_EDITION_VERSION_ID = 9; //1.1.1

inline constexpr int DEFAULT_BUFLEN = 512;
inline constexpr char DEFAULT_PORT[] = "1986";

#include "Memory.h"  // Phase 5A: tagged allocator types (MemoryTag, MemoryArena, smart pointers)

#include "math.h"
#include "windows.h"
#include "winuser.h"

#include "AppRes.h"

#include "ddraw.h"

#ifdef _d3d
#include "d3d.h"
#endif

inline constexpr int ctHScale = 64;
inline constexpr int PMORPHTIME = 256;
inline constexpr int HiColor(int R, int G, int B) { return ((R) << 10) + ((G) << 5) + (B); }


inline constexpr int TCMAX = (128 << 16) - 62024;
inline constexpr int TCMIN = (000 << 16) + 62024;

inline constexpr int DINOINFO_MAX = 128;
inline constexpr int TROPHY_COUNT = 24;
inline constexpr int TROPHY2_COUNT = 128; //.sab

#ifdef _MAIN_
#define _EXTORNOT
#else
#define _EXTORNOT extern
#endif

inline constexpr float pi = 3.1415926535f;
inline constexpr int ctMapSize = 1024;
inline constexpr int kViewGridCenter = 256;
inline constexpr int kViewGridSize = kViewGridCenter * 2;

// View distance option is stored as OptViewR in the 1660-byte trophy file.
// ctViewR is the derived terrain/entity/audio radius in 256-unit map cells.
inline constexpr int kViewOptMin = 0;
inline constexpr int kViewOptMax = 255;
inline constexpr int kViewOptDefault = 128;
inline constexpr int kViewDistanceMin = 42;
inline constexpr int kViewDistanceMax = 230;
inline constexpr int kViewDistanceDefault = 72;

inline int ClampViewOpt(int value)
{
	if (value < kViewOptMin) return kViewOptMin;
	if (value > kViewOptMax) return kViewOptMax;
	return value;
}

inline int ViewOptToCtViewR(int opt)
{
	opt = ClampViewOpt(opt);

	// Preserve the legacy 0..127 OptViewR curve, then use the upper half
	// of the menu's 0..255 range for the extended view-distance cap.
	if (opt <= 127)
		return 42 + (opt / 8) * 2;

	return 72 + ((opt - 127) * (kViewDistanceMax - 72)) / (kViewOptMax - 127);
}

// Bitmap sprite LOD distance. Higher values keep objects as 3D models farther out.
// This is stored in config.cfg because the legacy trophy format has no spare field.
inline constexpr int kObjectDetailMin = 24;
inline constexpr int kObjectDetailMax = 96;
inline constexpr int kObjectDetailStep = 4;
inline constexpr int kObjectDetailDefault = 48;

inline int ClampObjectDetail(int value)
{
	if (value < kObjectDetailMin) return kObjectDetailMin;
	if (value > kObjectDetailMax) return kObjectDetailMax;
	return value;
}

// Field of view (vertical, degrees) — modder-editable range
inline constexpr int kFovMin = 35;
inline constexpr int kFovMax = 90;
inline constexpr int kFovStep = 2;
inline constexpr int kFovDefault = 62;

// 1.0 / tan(deg * pi / 360) — used to scale the vertical view so the
// scene keeps its angular size when the FOV option changes.
inline float FovScaleFromDegrees(int fovDeg)
{
	return 1.0f / tanf((float)fovDeg * pi / 360.0f);
}

template <typename T, typename U>
inline constexpr std::common_type_t<T, U> MIN(T a, U b)
{
	return (a < b) ? a : b;
}

template <typename T, typename U>
inline constexpr std::common_type_t<T, U> MAX(T a, U b)
{
	return (a > b) ? a : b;
}

struct TMessageList
{
  int timeleft;
  char mtext[256];
};

struct TRGB
{
  BYTE B;
  BYTE G;
  BYTE R;
};

struct TAni
{
  char aniName[32];
  int aniKPS, FramesCount, AniTime;
  // Phase 5B.2: aniData is now unique_heap_ptr<short int[]>. Freed
  // automatically when the TAni is destroyed.
  unique_heap_ptr<short int[]> aniData;
};

struct TVTL
{
  int aniKPS, FramesCount, AniTime;
  // Phase 5B.2: aniData is now unique_heap_ptr<short int[]>. Freed
  // automatically when the TVTL is destroyed.
  unique_heap_ptr<short int[]> aniData;
};

struct TSFX
{
  int  length;
  // Phase 5B.1: lpData migrated from raw short int* to std::vector<short int>.
  // std::vector handles its own lifetime (no manual _HeapFree needed in
  // ReleaseResources), and size_t/iterators prevent the buffer overruns that
  // the raw-pointer version was prone to. Audio functions like AddVoicev take
  // a raw pointer + length, so call sites use sfx.lpData.data().
  //
  // sizeof(TSFX) grows from 8 (int + raw pointer) to 16 (int + 12-byte vector
  // on MSVC x86 with three pointer members). This affects every ChInfo[128]
  // (128 * 64 SoundFX = 8192 instances) and the global SFX array, adding
  // ~64 KiB to BSS. The doc documents this as the intended trade-off.
  std::vector<short int> lpData;
};

// Phase 5B.1: sizeof(TSFX) grows from 8 (int + raw pointer) to 16+ bytes
// (int + std::vector<short int>; the exact size depends on the std::vector
// layout in the target STL). This affects every ChInfo[128] (128 * 64
// SoundFX = 8192 instances) and the global SFX array, adding ~64+ KiB to
// BSS. The doc documents this as the intended trade-off. The range check
// below fires if TSFX is ever back to its old 8-byte raw-pointer layout
// (i.e., someone reverts the std::vector change without updating the doc).
static_assert(sizeof(TSFX) > 8,
              "TSFX is back to its old 8-byte raw-pointer layout — the Phase 5B.1 "
              "std::vector migration was reverted. Re-apply the migration or update "
              "the doc.");



struct TRD
{
  int  RNumber, RVolume, RFreq;
  WORD REnvir, Flags;
};

struct TRes
{
  int w, h;
};

struct TAmbient
{
  TSFX sfx;
  TRD  rdata[16];
  int  RSFXCount;
  int  AVolume;
  int  RndTime;
};


struct TEXTURE
{
  WORD DataA[128*128];
  WORD DataB[64*64];
  WORD DataC[32*32];
  WORD DataD[16*16];
  WORD SDataC[2][32*32];
  int mR, mG, mB;
};



struct TPicture
{
  int W,H;
  // Phase 5B.2: lpImage is now unique_heap_ptr<WORD[]>. Freed
  // automatically when the TPicture is destroyed.
  unique_heap_ptr<WORD[]> lpImage;
};


struct Vector3d
{
  float x,y,z;
};

struct TPoint3di
{
  int x,y,z;
};

struct Vector2di
{
  int x,y;
};

struct Vector2df
{
  float x,y;
};


struct ScrPoint
{
#ifdef _soft
  int   x,y, tx,ty;
#else
  float x,y, tx,ty;
#endif

  int Light, z, r2, r3;
};

struct MScrPoint
{
  int x,y, tx,ty;
};

struct CLIPPLANE
{
  Vector3d v1,v2,nv;
};





struct EPoint
{
  Vector3d v;
  WORD DFlags;
  short int ALPHA;
  int  scrx, scry, Light;
  float Fog;
};


struct ClipPoint
{
  EPoint ev;
  float tx, ty;
};


//================= MODEL ========================
struct TPoint3d
{
  float x;
  float y;
  float z;
  short owner;
  short hide;
};



struct TFace
{
  int v1, v2, v3;
#ifdef _soft
  int   tax, tbx, tcx, tay, tby, tcy;
#else
  float tax, tbx, tcx, tay, tby, tcy;
#endif
  WORD Flags,DMask;
  int Distant, Next, group;
  char reserv[12];
};


struct TFacef
{
  int v1, v2, v3;
  float tax, tbx, tcx, tay, tby, tcy;
  WORD Flags,DMask;
  int Distant, Next, group;
  char reserv[12];
};



struct TObj
{
  char OName [32];
  float ox;
  float oy;
  float oz;
  short owner;
  short hide;
};


struct TModel
{
  int VCount, FCount, TextureSize, TextureHeight;
  // Phase 5B.2: gVertex is now unique_heap_ptr<TPoint3d[]>. Allocated in
  // AllocateMemoryForModel and freed automatically when the TModel is
  // destroyed (via HeapDeleter -> ~TModel -> ~unique_heap_ptr).
  unique_heap_ptr<TPoint3d[]> gVertex;

  // gFace/gFacef stays as a raw pointer union: the union of TFace* and
  // TFacef* can't hold a unique_ptr (no two active members in a union
  // with a non-trivial destructor). The face array is still allocated
  // via _HeapAlloc and freed via _HeapFree in ReleaseModel; the smart
  // pointer migration does not touch this field.
  union
  {
    TFace    *gFace;
    TFacef   *gFacef;
  };

  // Phase 5B.2: texture pointers are now unique_heap_ptr<WORD[]>.
  unique_heap_ptr<WORD[]> lpTexture, lpTexture2, lpTexture3;

  // VLight[4] stays as raw pointers: it's a 4-channel view into a
  // single _HeapAlloc'd block (one allocation sliced into per-channel
  // offsets). The block is freed separately in ReleaseModel, not through
  // these pointers. The 4-channel structure must be preserved (C2 ME
  // addition over C1's single VLight).
#ifdef _d3d
  int*      VLight[4];
#else
  float*    VLight[4];
#endif

  // Phase 5B.2: TModel now has non-trivial members (unique_heap_ptr) so
  // it needs explicit special members. The default ctor is needed for
  // placement new in LoadModel/LoadModelEx. The copy ctor/assignment
  // are deleted because unique_ptr is not copyable (TModel is always
  // used through pointers in the codebase, so this is safe). The move
  // ctor/assignment transfer the smart pointers AND null the source's
  // raw pointers (gFace, VLight[4]) to prevent double-free.
  TModel() = default;

  TModel(const TModel&) = delete;
  TModel& operator=(const TModel&) = delete;

  TModel(TModel&& other) noexcept
    : VCount(other.VCount), FCount(other.FCount),
      TextureSize(other.TextureSize), TextureHeight(other.TextureHeight),
      gVertex(std::move(other.gVertex)),
      gFace(other.gFace),
      lpTexture(std::move(other.lpTexture)),
      lpTexture2(std::move(other.lpTexture2)),
      lpTexture3(std::move(other.lpTexture3))
  {
    for (int i = 0; i < 4; i++) {
      VLight[i] = other.VLight[i];
      other.VLight[i] = nullptr;
    }
    other.gFace = nullptr;
  }

  TModel& operator=(TModel&& other) noexcept
  {
    if (this != &other) {
      VCount = other.VCount;
      FCount = other.FCount;
      TextureSize = other.TextureSize;
      TextureHeight = other.TextureHeight;
      gVertex = std::move(other.gVertex);
      gFace = other.gFace;
      lpTexture = std::move(other.lpTexture);
      lpTexture2 = std::move(other.lpTexture2);
      lpTexture3 = std::move(other.lpTexture3);
      for (int i = 0; i < 4; i++) {
        VLight[i] = other.VLight[i];
        other.VLight[i] = nullptr;
      }
      other.gFace = nullptr;
    }
    return *this;
  }

  ~TModel() = default;  // smart pointers handle their own cleanup
};


//=========== END MODEL ==============================//


struct TObjInfo
{
  int  Radius;
  int  YLo, YHi;
  int  linelenght, lintensity;
  int  circlerad, cintensity;
  int  flags;
  int  GrRad;
  int  DefLight;
  int  LastAniTime;
  float BoundR;
  BYTE res[16];
};

struct TBMPModel
{
  Vector3d  gVertex[4];
  // Phase 5B.2: lpTexture is now unique_heap_ptr<WORD[]>. Freed
  // automatically when the TBMPModel is destroyed. Note: TBMPModel
  // is embedded by value in TObject, so this changes TObject's
  // size — see static_assert below.
  unique_heap_ptr<WORD[]> lpTexture;
};

struct TBound
{
  float cx, cy, a, b,  y1, y2;
};

struct TObject
{
  TObjInfo info;
  TBound   bound[8];
  TBMPModel bmpmodel;
  // Phase 5B.2: TObject::model is now unique_obj_ptr<TModel>. MObjects[256]
  // is 32 KiB; this changes TObject's size by 0 bytes (EBO on the smart
  // pointer). The struct is embedded by value in MObjects, so every
  // MObjects[m].model access now returns a unique_ptr that must be
  // .get()'d when passed to functions expecting TModel*.
  unique_obj_ptr<TModel> model;
  TVTL    vtl;
};


struct TCharacterInfo
{
  char ModelName[32];
  int AniCount,SfxCount;
  // Phase 5B.2: mptr is now unique_obj_ptr<TModel>. The model is
  // freed (via ~TModel + _HeapFree) automatically when the
  // TCharacterInfo is destroyed or when mptr is reset. TModel now
  // has a move ctor (added above) so this works with the smart
  // pointer.
  unique_obj_ptr<TModel> mptr;
  TAni Animation[64];
  TSFX SoundFX[64];
  int  Anifx[64];
};

struct TWeapon
{
  TCharacterInfo chinfo[10];
  TPicture       BulletPic[10];
  TPicture       ChambPic[10];
  TCharacterInfo Bullet[10];
  TPicture		 Flash[4];
  int FlashP;

  // Phase 5B.2: normals is now unique_heap_ptr<Vector3d[]>. Allocated
  // per-level in LoadResources (sized by maxWeaponVCount) and freed
  // automatically when the TWeapon is destroyed. Tagged as Level
  // (per-level) so it recycles with the arena in Phase 5C.
  unique_heap_ptr<Vector3d[]> normals;
  int state, FTime;
  float shakel;
  float breath;
  int BTime;
  bool HoldBreath;
  int breathPressed;
  int ammoIn;
};


struct TBullet
{
	float fallTotal;
	byte aqState; //0 land //1 aqua //2 min
	Vector3d a,dif,ldif,rpos,orig;
	int parent, state;
	int FTime, RTime;
	float alpha, beta;
	bool Danger;//damage hunter
	bool cDanger;//damage creature
	bool enemy;//damage creature
//	float power, speed, fall;
};

struct TWCircle
{
  Vector3d pos;
  float scale;
  int FTime;
};

struct TSnowType  {
	int snow_vSpd;//vertical
	int snow_hSpd;//horizontal
	int snow_dens;//density

	byte snow_r, snow_g, snow_b, snow_a;
	float snow_rad;//radius
	int addr; //start address in snow particle array
	int SnCount;//total number of snow particles
};


struct TSnowElement  {
	Vector3d pos;
	float hl, ftime;
};


struct TCharacter
{
  int CType, Clone;
  TCharacterInfo *pinfo;
  int StateF;
  int State;
  int NoWayCnt, NoFindCnt, AfraidTime, tgtime;
  int PPMorphTime, PrevPhase,PrevPFTime, Phase, FTime;

  int currentIdleGroup;
  int currentIdle2Group;

  float vspeed, rspeed, bend, scale;
  int Slide;
  float slidex, slidez;
  float tgx, tgz;

  Vector3d pos, rpos;
  float tgalpha, alpha, beta,
        tggamma,gamma,
        lookx, lookz;
  int Health, BloodTime, BloodTTime;

  //ICTH
  bool gliding = false;
  //  bool wingUp = false;
  bool notFlushed = false;
  int deathPhase;
  bool canSleep = false;
  float shakeTime = 0;
  float spawnAlt;

  //MOSA
  float depth, tdepth;
  float bdepth = 0;//bend
  float lastTBeta = 0;
  float turny = 0;

  int spcDepth;

  int SpawnGroupType;

  int packId;
  bool followLeader;

  int killType;
  int deathType;
  int roarAnim;
  int waterDieAnim;

  int dogPrey; // used by dog only. The dino currently being tracked

  bool awareHunter;
  bool heardShot;

  bool aquaticIdle;

  int tropAnim;

  int xdata, zdata, ydata;

  bool animateTrophy;

  int _PhaseM;

  Vector3d climbable;
  float climbY;
  BOOL gottaClimb;

  Vector3d sonar;
  BOOL showSonar;

  bool cpcpAquatic;//checkplacecollisionaquatic - can spawn in water,brach,icth,mosa,fish

//  int tropIndex;

  float packDensity;

  int tracker;
  int RTime;

  int tempScore; //stores killed stats
  int tempTime;
  int tempDate;
  float tempRange;
  bool claimed;

  //poacher
  int ammo;

};



struct TPlayer
{
  BOOL Active;
  unsigned int IPaddr;
  Vector3d pos;
  float alpha, beta, vspeed;
  int kbState;
  char NickName[16];
};


struct TDemoPoint
{
  Vector3d pos;
  int DemoTime, CIndex;
};

struct TLevelDef
{
  char FileName[64];
  char MapName[128];
  DWORD DinosAvail;
  WORD *lpMapImage;
};


struct TShipTask
{
  int tcount;
  int clist[255];
};

struct TShip
{
  Vector3d pos, rpos, tgpos, retpos;
  float alpha, tgalpha, speed, rspeed, DeltaY, beta, gamma, gspeed, bspeed;
  int State, cindex, FTime;
};

struct TBag
{
	Vector3d pos, rpos;
	int State;
	int FTime;
};

struct THitBox
{
	Vector3d pos, rpos;
	float alpha;
	int phase;
};


struct TLandingList
{
  int PCount;
  Vector2di list[64];
};


struct TPlayerR
{
  char PName[128];
  int  RegNumber;
  int  Score, Rank;
};

struct TTrophyItem
{
  int ctype, weapon, phase,
      height, weight, score,
      date, time;
  float scale, range;
  int r1, r2, r3, r4;
};


struct TStats
{
  int smade, success;
  float path, time;
};


struct TTrophyRoom
{
  char PlayerName[128];
  int  RegNumber;
  int  Score, Rank;

  TStats Last, Total;

  TTrophyItem Body[TROPHY_COUNT];
};



struct TTrophyItem2  //Add neccesary stuff here! (later, not now)
{
	int ctype, weapon, phase,
		height, weight, score,
		date, time;
	float scale, range;
	int r1, r2, r3, r4;
};



struct TTrophyRoom2
{
	int versionID;
	int survivalHighScore;
	TTrophyItem2 Body[TROPHY2_COUNT];
};


struct TDinoKill
{
	int anim;
	int offset;
	int hunteranim;
	int hunterswimanim;
	BOOL elevate, carryCorpse;
	BOOL dontloop;
	BOOL scream;
};

struct TTrophyType
{
	int group = -1;
	int ctype[TROPHY2_COUNT];
	int ctypeCh = 0;
	int xoffset, yoffset, zoffset;
	int xoffsetScale, yoffsetScale, zoffsetScale;
	int xdata, ydata, zdata;
	int alpha, beta, gamma; //degrees
	int anim;
	int trophyPos;
	bool playAnim;
};

struct TPackMember
{
	int ctype;
	float ratio;
};

struct TPackMember2
{
	int packGroup;
	float ratio;
};


struct TSpawnInfo
{
	int spawnGroup;//, spawnMax;
	float spawnRatio;
};

struct TPackType
{
	TSpawnInfo SpawnInfo[32];
	TPackMember packMember[32];
	int packMemberCh = 0;
	int SpawnInfoCh = 0;
	int packMax, packMin;
	float packDensity;
};

struct TDinoDeathType
{
	int die;
	int sleep;
	int fall;
	bool nosleep;
};

struct TDinoIdleType
{
	int anim[32];
	int count;
	float start;
	float end;
	bool endOnAny;
	bool startOnAny;
	bool instantRepeat;
};

struct TMenuDinoInfo
{
	TPicture CallIcon;
};

struct TDinoInfo
{
	int menuDino = -1;

  char Name[48], FName[48], PName[48];
  int Health0, Clone;
  float Mass, Length, Radius,
        SmellK, HearK, LookK,
        ShDelta;
  int   Scale0, ScaleA;
  float	  BaseScore;

  BOOL fearCall[64];
  BOOL Aquatic;
  int maxDepth, minDepth, spacingDepth;
  BOOL dontSwimAway;

  BOOL survivalDino;

  BOOL dontBend;
  //float bendOffset;

  float weaveRange;
  BOOL dontWeave;

  BOOL defensive;
  BOOL fearShot;
  BOOL fearHearShot;

  //BOOL noMoveNoRot;

  //int hunterDeathAnim, hunterDeathOffset;
  int aggress, killDist, flyDist;

  bool onRadar;
  float runspd, jmpspd, wlkspd, swmspd, flyspd, gldspd, tkfspd, lndspd, divspd;

  int maxGrad;
  float rotspdmulti;

//  int packMax, packMin;
//  float packDensity;

  int jumpRange;

  int runAnim, jumpAnim, walkAnim, swimAnim, flyAnim, diveAnim, glideAnim, takeoffAnim, landAnim,
	  slideAnim, shakeLandAnim, shakeWaterAnim, climbAnim, fireAnim;
  int reloadAnim = -1;

  TDinoDeathType deathType[32];
  int deathTypeCount;

  TDinoKill killType[32];
  int killTypeCount;

//  int tCounter;
//  int trophyCode;
//  int trophyLocTotal1;//CURRENTLY IN SAVE FILE
//  int trophyLocTotal2;//CURRENTLY IN SESSION - REPLACE WITH tlt1 UPON RESTART
 
  bool trophy = false;//counts the number of trophy slots
//  int tCounter; // used to count off trophy locs

  int waterDieAnim[32];
  int waterDieCount;

  TDinoIdleType idleGroup[32];
  int idleGroupCount;

  TDinoIdleType idle2Group[32];
  int idle2GroupCount;

 
  int lookAnim[32];//trex look
  int lookCount;
 
  int smellAnim[32]; //icth wateridle   trex smell
  int smellCount;
 

  int roarAnim[32];
  int roarCount;
 
  bool canSwim;
  int waterLevel;

  BOOL dogSmell;

  //bool trophySession;

  int partFrame1[50], partFrame2[50], partDist[50], partCnt[50], partMag[50], partOffset[50];
  bool partAngled[50], partCircle[50];

  bool DangerFish;
  bool TRexObjCollide;
  bool Mystery;
  bool HideBinoc;

  float camDemoPoint, camBase, camDemoPointWater, camBaseWater;

  float climbDist;

  byte radarRed, radarGreen, radarBlue, bloodRed, bloodGreen, bloodBlue;
  WORD radarColour565, radarColour555;


  int SpawnInfoCh=0;
  TSpawnInfo SpawnInfo[32];

  int packMember2Ch = 0;
  TPackMember2 packMember2[32];

  //poacher
  int Weapon;
  int Reload;

};

struct TPack
{
	TCharacter *leader;
	bool alert;
	bool _alert;
	bool attack;
	bool _attack;
};

struct TAIInfo  {
	float targetDistance;
	int noWayCntMin;
	int noFindWayMed;
	int noFindWayRange;
	float targetBendRotSpd;
	float targetBendMin;
	float targetBendDelta1;
	float targetBendDelta2;
	float walkTargetGammaRot;
	float targetGammaRot;
	int idleStart;
	float yBetaGamma1, yBetaGamma2, yBetaGamma3, yBetaGamma4;

	int agressMulti;
	float tGAIncrement;
	int idleStartD;
	bool jumper;

	bool iceAge;
	bool carnivore;

	float rot1, rot2, pWMin; //weaveRange

	bool sniffer;

};

struct TSpawnRegion
{
	int XMax, YMax, XMin, YMin;
};

struct TSpawnGroup
{
	int SpawnMax, SpawnMin;
	float SpawnRate;
	bool moveForward, Randomised, OnlyActiveNearby, stayInRegion;
	int densityMulti;

	int spawnRegionCh, avoidRegionCh;
	TSpawnRegion spawnRegion[16];
	TSpawnRegion avoidRegion[16];

	int packIndexCh;
	int packIndex[128];
	int spawnInfoIndex[128];
};


struct TWeapInfo
{
	bool pic2b = false;
	bool picch = false;
  char Name[48], FName[48], BFName[48], CFName[48], BLName[48], SFXName[48];
  bool MGSSound = false;
  bool bullet = false;
  bool retrieve;
  float Power, Prec, Loud, Rate, Veloc, Fall;
  int Shots, TraceC, Reload, SFXIndex;
  int shtAnim = -1;
  int getAnim = -1;
  int putAnim = -1;
  int rldAnim = -1;
  int rldAnimPart = -1;
  int pmpAnim = -1;
  int modAnim = -1;
  int emptyAnim = -1;
  int getEmpAnim = -1;
  int putEmpAnim = -1;
  int shtAqSnd = -1;
  int getAqSnd = -1;
  int putAqSnd = -1;
  int rldAqSnd = -1;
  int rldAqSndPart = -1;
  int pmpAqSnd = -1;
  int modAqSnd = -1;
  float shake, Optic;
  bool unzoom, harpoon;

  bool canRun;
  bool cannotMortal;

  bool fullauto = false;
  bool semiauto = false;

  bool aqLow; //parent velocAq < veloc

  bool mustPump;
  bool autoPump;
  bool autoReload;

  float PowerAq = -1;
  float PrecAq = -1;
  float VelocAq = -1;
  float FallAq = -1;

  bool onRadar;
  byte radarRed, radarGreen, radarBlue;
  WORD radarColour565, radarColour555;
  int radarTime;

  bool MuzzFlash, ChamFlash;
  bool cross;
  byte crossRed, crossGreen, crossBlue;
  WORD crossColour565, crossColour555;

  int recoil;

};


struct TFogEntity
{
  int fogRGB;
  float YBegin;
  BOOL  Mortal;
  float Transp, FLimit;
};


struct TWaterEntity
{
  int tindex, wlevel;
  float transp;
  int fogRGB;
};


struct TWind
{
  float alpha;
  float speed;
  Vector3d nv;
};




struct TElement
{
  Vector3d pos, speed;
  int     Flags;
  float   R;
};

struct TElements
{
  int Type, ECount, EDone, LifeTime;
  int Param1, Param2, Param3;
  DWORD RGBA, RGBA2;
  Vector3d pos;
  TElement EList[32];
};


struct TBloodP
{
  int LTime;
  Vector3d pos;
  int Owner;
};

struct TBTrail
{
  int Count;
  TBloodP Trail[512];
};


// ----------------------------------------------------------------------------
// Phase 5 sizeof() verification (Gap #3 follow-up)
//
// These static_asserts lock the sizes of structs whose fields were migrated
// from raw pointers to smart pointers in Phase 5B.2. The checks prevent
// silent size regressions (e.g., if EBO fails on a unique_ptr alias, the
// struct would double in size). Expected values are for MSVC x86 with
// empty-base-optimization on HeapDeleter; adjust if the STL or compiler
// changes.
//
// See memory-system-migration.md §5 for the full verification protocol.
// ----------------------------------------------------------------------------

// TAni: 32 (aniName) + 3 ints (12) + unique_heap_ptr (4) = 48
static_assert(sizeof(TAni) == 48,
              "TAni size changed — verify unique_heap_ptr EBO is still active");

// TVTL: 3 ints (12) + unique_heap_ptr (4) = 16
static_assert(sizeof(TVTL) == 16,
              "TVTL size changed — verify unique_heap_ptr EBO is still active");

// TPicture: 2 ints (8) + unique_heap_ptr (4) = 12
static_assert(sizeof(TPicture) == 12,
              "TPicture size changed — verify unique_heap_ptr EBO is still active");

// TBMPModel: gVertex[4] (48) + unique_heap_ptr (4) = 52
static_assert(sizeof(TBMPModel) == 52,
              "TBMPModel size changed — verify unique_heap_ptr EBO is still active");

// TModel: 4 ints (16) + gVertex (4) + gFace union (4) + 3*lpTexture (12)
//         + VLight[4] (16) = 52 on both d3d and non-d3d (int* and float*
//         are both 4 bytes on x86)
static_assert(sizeof(TModel) == 52,
              "TModel size changed — this affects MObjects[256] layout and "
              "would shift the entire object array");

// TObject: contains TObjInfo + TBound[8] + TBMPModel + model + TVTL + ...
// Expected ~140 bytes. Range check catches catastrophic bloat without
// failing on minor alignment differences across toolchains.
static_assert(sizeof(TObject) >= 300 && sizeof(TObject) <= 400,
              "TObject size is outside expected range — MObjects[256] "
              "layout has shifted");

// TCharacterInfo: contains ModelName[32] + mptr + TAni[64] + TSFX[64] + ...
// The TAni[64] alone is 48*64 = 3072; TSFX[64] with vector is ~28*64 = 1792.
// Total is several KiB. Range check verifies no unexpected reordering.
static_assert(sizeof(TCharacterInfo) >= 3000 && sizeof(TCharacterInfo) <= 8000,
              "TCharacterInfo size is outside expected range — ChInfo[128] "
              "layout is affected");


//============= functions ==========================//

void HLineTxB( void );
void HLineTxC( void );
void HLineTxGOURAUD( void );


void HLineTxModel25( void );
void HLineTxModel75( void );
void HLineTxModel50( void );

void HLineTxModel3( void );
void HLineTxModel2( void );
void HLineTxModel( void );

void HLineTDGlass75( void );
void HLineTDGlass50( void );
void HLineTDGlass25( void );
void HLineTBGlass25( void );


void SetVideoMode(int, int);
void SetFullScreen();
void CaptureMouse(BOOL);
void ResetMousePos();

void CreateDivTable();
void DrawTexturedFace();
int GetTextW(HDC, LPSTR);
void wait_mouse_release();

//============================== render =================================//
void ShowControlElements();
void InsertModelList(TModel* mptr, float x0, float y0, float z0, int light, float al, float bt);
void RenderGround();
#ifdef _gl
void RenderProjectedShadows();
#endif
void RenderWater();
void RenderElements();
void CreateChRenderList();
void RenderModelsList();
void ProcessMap  (int x, int y, int r);
void ProcessMap2 (int x, int y, int r);
void ProcessMapW (int x, int y, int r);
void ProcessMapW2(int x, int y, int r);

void DrawTPlane(BOOL);
void DrawTPlaneClip(BOOL);
void ClearVideoBuf();
// Phase 5E follow-up: clear renderer-side per-level texture caches before
// LoadResources loads new models. Only the GL renderer currently has such
// caches (m_modelTextureCache / m_bmpTextureCache keyed by TModel*); the
// other renderers implement this as a no-op.
void ClearRendererLevelCache();
void ClearRendererTerrainCache();
void ReleaseModelTexture(const TModel* mptr);
void DrawScoreText(int, int);
void DrawTrophyText(int, int);
void DrawSurvivalText(int, int);
void DrawHMap();
void RenderCharacter(TCharacter*);
void RenderShip();
void RenderSShip();
void RenderBag();
void RenderBullet(int);
void RenderPlayer(int);
void RenderSkyPlane();
void RenderHealthBar();
void Render_Cross(int, int);
void Render_LifeInfo(int);

void RenderModelClipEnvMap(TModel*, float, float, float, float, float);
void RenderModelClipPhongMap(TModel*, float, float, float, float, float);

void RenderModel         (TModel*, float, float, float, int, int, float, float);
void RenderBMPModel      (TBMPModel*, float, float, float, int);
void RenderModelClipWater(TModel*, float, float, float, int, int, float, float);
void RenderModelClip     (TModel*, float, float, float, int, int, float, float);
void RenderNearModel     (TModel*, float, float, float, int, float, float);
void DrawPicture         (int x, int y, TPicture &pic);
#ifdef _gl
void DrawScaledPicture   (int x, int y, int w, int h, TPicture &pic);
#endif
void DrawFlash		 (int x, int y, int w, int h, TPicture &pic);

void InitClips();
void InitDirectDraw();
void WaitRetrace();

//============= Characters =======================
void Characters_AddSecondaryOne(TCharacter *cptr);
void AddDeadBody(TCharacter *cptr, int, bool);
void PlaceCharacters();
void PlaceCharactersSurvival();
void PlaceMHunters(); //multiplayer
void PlaceTrophy();
void AnimateCharacters();
void AnimateMHunters(); //multiplayer
void MakeNoise(Vector3d, float);
void CheckAfraid();
void CreateChMorphedModel(TCharacter* cptr);
void CreateMorphedObject(TModel* mptr, TVTL &vtl, int FTime);
void CreateMorphedModel(TModel* mptr, TAni *aptr, int FTime, float scale);
void CreateMorphedModelBetaGamma(TModel* mptr, TAni *aptr, int FTime, float scale, float beta, float gamma);

//=============================== Math ==================================//

void CalcLights  (TModel* mptr);
void CalcModelGroundLight(TModel *mptr, float x0, float z0, int FI);
void CalcNormals (TModel* mptr, Vector3d *nvs);
void CalcGouraud (TModel* mptr, Vector3d *nvs);

void CalcPhongMapping(TModel* mptr, Vector3d *nv);
void CalcEnvMapping(TModel* mptr, Vector3d *nv);

void CalcBoundBox(TModel* mptr, TBound *bound);
void  NormVector(Vector3d&, float);
float SGN(float);
void  DeltaFunc(float &a, float b, float d);
void  MulVectorsScal(const Vector3d&, const Vector3d&, float&);
void  MulVectorsVect(const Vector3d&, const Vector3d&, Vector3d&);
Vector3d SubVectors( Vector3d&, Vector3d& );
Vector3d AddVectors( Vector3d&, Vector3d& );
Vector3d RotateVector(Vector3d&);
float VectorLength(Vector3d);
float VectorLengthSq(Vector3d);
int   siRand(int);
int   rRand(int);
void  CalcHitPoint(CLIPPLANE&, Vector3d&, Vector3d&, Vector3d&);
void  ClipVector(CLIPPLANE& C, int vn);
float FindVectorAlpha(float, float);
float AngleDifference(float a, float b);

int   TraceShot(float ax, float ay, float az,
                float &bx, float &by, float &bz,
	bool,bool);
int   TraceLook(float ax, float ay, float az,
                float bx, float by, float bz);


void CheckCollision(float&, float&);
float CalcFogLevel(Vector3d v);
//=================================================================//
void AddMessage(LPSTR mt);
void CreateTMap();


void LoadSky();
void LoadSkyMap();
void LoadTexture(TEXTURE*&);
void LoadWav(char* FName, TSFX &sfx);


void ApplyAlphaFlags(WORD*, int);
WORD conv_565(WORD c);
int  conv_xGx(int);
void conv_pic(TPicture &pic);
void LoadPicture(TPicture &pic, LPSTR pname, MemoryTag tag = MemoryTag::Global);
void LoadPictureTGA(TPicture &pic, LPSTR pname, MemoryTag tag = MemoryTag::Global);
void LoadCharacterInfo(TCharacterInfo&, char*, MemoryTag tag = MemoryTag::Global);
void LoadModelEx(unique_obj_ptr<TModel> &mptr, char* FName, MemoryTag tag = MemoryTag::Global);
void LoadModel(unique_obj_ptr<TModel> &mptr);
void LoadResources();
void ReleaseResources();
void ReleaseGlobalResources();
void ReleaseCharacterInfo(TCharacterInfo &chinfo);
void ReleaseModel(unique_obj_ptr<TModel> &mptr);
void ReInitGame();



void SaveScreenShot();

#ifdef GL_PERF_HOOKS
// F11 key handler: triggers a 1-second per-frame GL perf CSV capture
// (glperf-frame.csv in the working directory). No-op when the GL perf
// harness is not compiled in.
void PerfTriggerCapture();

// Frame boundary hooks for the GL perf harness. PerfFrameBegin() is
// called at the start of Hunt.cpp::DrawScene(); PerfFrameEnd() at the
// end of Hunt.cpp::DrawPostObjects(). Bracket the per-frame GL work so
// glperf.log can report a clean "frame total" alongside the per-pass
// scopes. No-op when the harness is not compiled in.
void PerfFrameBegin();
void PerfFrameEnd();
#endif

void CreateWaterTab();
void CreateFadeTab();
void CreateVideoDIB();
void CreateVideoDIB(int W, int H);
void RenderLightMap();

void MulVectorsVect(const Vector3d& v1, const Vector3d& v2, Vector3d& r );
void MulVectorsScal(const Vector3d& v1, const Vector3d& v2, float& r);
Vector3d SubVectors( Vector3d& v1, Vector3d& v2 );
Vector3d SubVectors2d(Vector3d& v1, Vector3d& v2);
void NormVector(Vector3d& v, float Scale);

LPVOID _HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes);
// Phase 5A: 4-arg overload with MemoryTag dispatch. No default for `tag` —
// MSVC's overload resolution treats a 3-arg call as ambiguous between this
// overload (using the default) and the 3-arg overload above, so the tag
// must be explicit. The 3-arg forwarder in Resources.cpp routes legacy
// 3-arg calls through this overload with MemoryTag::Level. Migration
// phases 5B-5E will update individual call sites to the explicit 4-arg
// form with the appropriate tag (Global for session-lifetime, Level for
// per-level).
LPVOID _HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, MemoryTag tag);
BOOL _HeapFree(HANDLE hHeap, DWORD  dwFlags, LPVOID lpMem);

// Phase 5A: per-level arena. Constructed in InitEngine() and destroyed in
// ShutDownEngine() (both Phase 5C). nullptr in Phase 5A — _HeapAlloc
// checks for nullptr and falls through to HeapAlloc, so pre-5A call
// sites are bit-for-bit unaffected.
_EXTORNOT MemoryArena *LevelArena;

//============ game ===========================//
float GetLandCeilH(float, float);
float GetLandH(float, float);
float GetLandOH(int, int);
float GetLandLt(float, float);
float GetLandUpH(float, float);
float GetLandQH(float, float);
float GetLandQHNoObj(float, float);
float GetLandHObj(float, float);
bool waterNear(float, float, float);

void LoadResourcesScript();
void InitEngine();
void ShutDownServer();
void ShutDownClient();
void ShutDownEngine();
void ProcessSyncro();
void AddShipSupply(float,float);
void AddShipTask(int);
void SubmitDinoScore(int);
void LoadTrophy();
//void LoadPlayersInfo();
void SaveTrophy();
void RemoveCurrentTrophy();
void MakeCall();
void AddBullet(float ax, float ay, float az,
              float bx, float by, float bz,
			  float blx, float bly, float blz,
	int, bool);
int AnimateBullet(float ax, float ay, float az,
	float bx, float by, float bz, int b);
void AnimateBullets();
void refillWeapons(bool);
void registerDamage(int, bool);

void AddBloodTrail(TCharacter *cptr);
void AddElements(float, float, float, int, int);
void AddElementsA(float, float, float, int, int, int, bool, float);
void AddWCircle(float, float, float);
void AnimateProcesses();
void DoHalt(LPSTR);
void DoHalt2(LPSTR);

_EXTORNOT   char logt[128];
void CreateLog();
void PrintLog(LPSTR l);
void PrintLogVerbose(LPSTR l);
void CloseLog();

_EXTORNOT   float BackViewR;
_EXTORNOT   int   BackViewRR;
_EXTORNOT   int   UnderWaterT;
_EXTORNOT   int   TotalTreeTable, TotalAreaInfo, TotalSpawnGroup, TotalC, TotalW, TotalMA, TotalTrophy;// , TotalRegion, TotalAvoid;


//========== multiplayer =============//

_EXTORNOT   char    ServerAddress[128];
_EXTORNOT   WSADATA wsaData;
_EXTORNOT   int iResult;
_EXTORNOT   SOCKET ListenSocket;
_EXTORNOT   SOCKET ClientSocket;
_EXTORNOT   SOCKET ConnectSocket;

_EXTORNOT   struct addrinfo *result;
_EXTORNOT   struct addrinfo hints;

_EXTORNOT   int iSendResult;

_EXTORNOT   HANDLE CommsThreadHandle;
_EXTORNOT   LPDWORD CommsThreadID;
_EXTORNOT   BOOL HaltThread;

_EXTORNOT   char recvbuf[DEFAULT_BUFLEN];
_EXTORNOT   int recvbuflen;

void StartupServerCommsThread();
void StartupClientCommsThread();




//========== common ==================//
_EXTORNOT   HWND    hwndMain;
_EXTORNOT   HINSTANCE  hInst;
_EXTORNOT   HANDLE  Heap;
_EXTORNOT   HDC     hdcMain, hdcCMain;
_EXTORNOT   BOOL    blActive;
_EXTORNOT   BYTE    KeyboardState[256];
_EXTORNOT   int     KeyFlags, _shotcounter;

_EXTORNOT   TMessageList MessageList;
_EXTORNOT   char    ProjectName[128];

_EXTORNOT   int     _GameState, _MultiplayerState;//multiplayer
_EXTORNOT   TSFX    fxBlip;
_EXTORNOT   TSFX    fxClick[3];
_EXTORNOT   TSFX    fxBreathIn;
_EXTORNOT   TSFX    fxBreathOut;
_EXTORNOT   TSFX    fxCollect[3];
_EXTORNOT   TSFX    fxImpactAquatic[3];
_EXTORNOT   TSFX    fxImpactGround[3];
_EXTORNOT   TSFX    fxImpactModel[3];
_EXTORNOT   TSFX    fxImpactWater[3];
_EXTORNOT   TSFX    fxImpactChar[3];
_EXTORNOT   TSFX    fxCall[10][3], fxScream[4];
_EXTORNOT   TSFX	fxGunShot[11];
_EXTORNOT   TSFX    fxUnderwater, fxWaterIn, fxWaterOut, fxJump, fxStep[3], fxStepW[3];
//========== map =====================//
_EXTORNOT   byte HMap[ctMapSize][ctMapSize];
_EXTORNOT   byte WMap[ctMapSize][ctMapSize];
_EXTORNOT   byte HMapO[ctMapSize][ctMapSize];
_EXTORNOT   WORD FMap[ctMapSize][ctMapSize];
_EXTORNOT   byte LMap[ctMapSize][ctMapSize];
_EXTORNOT   WORD TMap1[ctMapSize][ctMapSize];
_EXTORNOT   WORD TMap2[ctMapSize][ctMapSize];
_EXTORNOT   byte OMap[ctMapSize][ctMapSize];

_EXTORNOT   byte FogsMap[512][512];
_EXTORNOT   byte AmbMap[512][512];

_EXTORNOT   TFogEntity    FogsList[256];
_EXTORNOT   TWaterEntity  WaterList[256];
_EXTORNOT   TWind       Wind;
_EXTORNOT   TShip       Ship;
_EXTORNOT   TShip       SShip;
_EXTORNOT   TShipTask   ShipTask;
_EXTORNOT   TBag        AmmoBag;

_EXTORNOT   int SkyR, SkyG, SkyB, WaterR, WaterG, WaterB, WaterA,
            SkyTR,SkyTG,SkyTB, CurFogColor;
_EXTORNOT   int RandomMap[32][32];

_EXTORNOT   Vector2df *PhongMapping;
_EXTORNOT   TPicture TFX_SPECULAR, TFX_ENVMAP;
_EXTORNOT   WORD SkyPic[256*256];
_EXTORNOT   WORD SkyFade[9][128*128];
_EXTORNOT   BYTE SkyMap[128*128];

// Phase 5B.2: Textures is now std::array<unique_obj_ptr<TEXTURE>, 1024>.
// std::array is used (not a raw C array) so the size is fixed at
// compile time (matching the original 1024-element behavior) and the
// type system enforces it. All elements default-construct to null
// unique_ptrs, matching the old BSS zero-initialization. Access
// pattern is unchanged: Textures[t] returns a unique_obj_ptr<TEXTURE>&
// that supports operator bool, operator->, and .reset() the same way
// a raw TEXTURE* did.
_EXTORNOT   std::array<unique_obj_ptr<TEXTURE>, 1024> Textures;
_EXTORNOT   TAmbient Ambient[256];
_EXTORNOT   TSFX     RandSound[256];

//========= WEATHER =================//

_EXTORNOT TSnowType SnowInfo[32];
_EXTORNOT int SnowCh;

//========= GAME ====================//
_EXTORNOT int TargetDino, TargetArea, TargetWeapon, WeaponPres, TargetCall,
          ObservMode, Tranq, ObjectsOnLook, RenderHitBox,
          CurrentWeapon, ShotsLeft[10], AmmoMag[10],
	MagShotsLeft[10], Chambered[10], FiringMode[10]; //TrophyTime,

//firing mode 0-semiauto 1-fullauto

_EXTORNOT bool alreadyFired;

_EXTORNOT Vector3d answpos;
_EXTORNOT int answtime, answcall;

_EXTORNOT BOOL NightVisionMode, NightVisionOn;

_EXTORNOT BOOL ScentMode, CamoMode,
          RadarMode, LockLanding,
          TrophyMode, DoubleAmmo,
          DogMode, Multiplayer,
          Host, CiskMode, SonarMode,
          ScannerMode, SurvivalMode;

// Score multipliers for accessories. Defaults are set in Hunt/Game.cpp
// InitEngine() and match the legacy hardcoded values from
// SubmitDinoScore() so a hunt launched without a Menu-supplied 'smod='
// argument behaves identically to the original game. The Menu passes
// 'smod=camo,radar,scent,double,tranq,observer' in the same order to
// override these from _RES.TXT (see Menu/Resources.cpp ReadAccessories()).
_EXTORNOT float ScoreMod_Camo;
_EXTORNOT float ScoreMod_Radar;
_EXTORNOT float ScoreMod_Scent;
_EXTORNOT float ScoreMod_Double;
_EXTORNOT float ScoreMod_Tranq;
_EXTORNOT float ScoreMod_Observer;

_EXTORNOT float sonarPos;

_EXTORNOT TTrophyRoom TrophyRoom;
_EXTORNOT TTrophyRoom2 TrophyRoom2;
//_EXTORNOT TPlayerR PlayerR[16];
_EXTORNOT TPicture LandPic,DinoPic,DinoPicM, MapPic, WepPic;
_EXTORNOT HFONT fnt_BIG, fnt_Small, fnt_Midd;
_EXTORNOT TLandingList LandingList;

_EXTORNOT TBullet bullet[256];
_EXTORNOT int bulletCh;

_EXTORNOT Vector3d TraceB;


//======== MODEL ======================//
_EXTORNOT TObject  MObjects[256];
_EXTORNOT TModel* mptr;
_EXTORNOT TWeapon Weapon;



_EXTORNOT int   OCount, iModelFade, iModelBaseFade, Current;
_EXTORNOT Vector3d  *rVertex;
_EXTORNOT TObj      gObj[1024];
_EXTORNOT Vector2di *gScrp;

_EXTORNOT int MaxObjectVCount; // Maximum VCount of any (loaded) object

//============= Characters ==============//
_EXTORNOT TPicture  PausePic, ExitPic, TrophyExit, TrophyPic, TrophyNoCollectPic, ScorePic;
_EXTORNOT unique_obj_ptr<TModel> SunModel;
_EXTORNOT TCharacterInfo WCircleModel;
_EXTORNOT unique_obj_ptr<TModel> CompasModel;
_EXTORNOT unique_obj_ptr<TModel> Binocular;
_EXTORNOT TDinoInfo DinoInfo[DINOINFO_MAX];
_EXTORNOT TMenuDinoInfo MenuDinoInfo[16];
_EXTORNOT int sendGunShot;
_EXTORNOT int mGunShot[4];
_EXTORNOT int sendHunterCall;
_EXTORNOT int sendHunterCallType;
_EXTORNOT int mHunterCall[4];
_EXTORNOT int mHunterCallType[4];
_EXTORNOT int sendDamage[DINOINFO_MAX];
_EXTORNOT int mDamage[4][DINOINFO_MAX];
//Add these after dino positions alligned
//_EXTORNOT int sendDinoCall;
//_EXTORNOT int mDinoCall[4];
_EXTORNOT bool TreeTable[255];
_EXTORNOT TAIInfo AIInfo[DINOINFO_MAX];
//_EXTORNOT TRegion Region[256];
//_EXTORNOT TRegion Avoid[256];
_EXTORNOT TWeapInfo WeapInfo[10];
_EXTORNOT bool Muzz;
_EXTORNOT int MuzzFTime;
_EXTORNOT float MuzzGamma;
_EXTORNOT TCharacterInfo MuzzModel;
_EXTORNOT TCharacterInfo ShipModel;
_EXTORNOT TCharacterInfo SShipModel;
_EXTORNOT TCharacterInfo BagModel;
_EXTORNOT TSpawnGroup spawnGroup[256];
//_EXTORNOT int AI_to_CIndex[DINOINFO_MAX];
//_EXTORNOT int TrophyIndex[DINOINFO_MAX];
_EXTORNOT int trophyGroupCount;
_EXTORNOT TPackType packType[1024];
_EXTORNOT int packTypeCount;
_EXTORNOT TTrophyType trophyType[TROPHY2_COUNT];
_EXTORNOT int trophyTypeCount;
_EXTORNOT int ChCount, WCCount, ElCount,
          ShotDino, TrophyBody, HunterCount; //HunterCount is for multiplayer, up to 3 others
_EXTORNOT bool TrophyDisplay;
_EXTORNOT int TrophyDisplayC;
_EXTORNOT int ScoreDispTime;
_EXTORNOT int ScoreDisp;
_EXTORNOT TTrophyItem TrophyDisplayBody;
_EXTORNOT TCharacterInfo WindModel;
_EXTORNOT TCharacterInfo PlayerInfo;
_EXTORNOT TCharacterInfo ChInfo[DINOINFO_MAX];
_EXTORNOT TCharacterInfo MPlayerInfo[3]; //multiplayer
_EXTORNOT TCharacterInfo HitBoxModel;
_EXTORNOT TPack          Packs[256];
_EXTORNOT int PackCount;
_EXTORNOT TCharacter     Characters[256];
_EXTORNOT TCharacter     MPlayers[3]; //multiplayer
_EXTORNOT THitBox     HitBox;

_EXTORNOT int SurvivalSpawnX; //survival
_EXTORNOT int SurvivalSpawnZ;
_EXTORNOT float SurvivalSpawnA;
_EXTORNOT TSpawnRegion SurvivalDinoSpawn; //dino spawn zone
_EXTORNOT int SurvivalWave;
_EXTORNOT int SurvivalIndex[128];
_EXTORNOT int SurvivalIndexCh;

//_EXTORNOT int TropSlotData[128]; //ctype per trophySlot
//_EXTORNOT int TropSlotDataCh = 0;


_EXTORNOT TWCircle       WCircles[2096]; //increased

_EXTORNOT TSnowElement*  Snow;

_EXTORNOT TDemoPoint     DemoPoint;
_EXTORNOT TCharacter     *killerDino;
_EXTORNOT BOOL			 killedwater;

_EXTORNOT TPlayer        Players[16];
_EXTORNOT Vector3d       PlayerPos, CameraPos;

//========== Render ==================//
_EXTORNOT   LPDIRECTDRAW lpDD;
_EXTORNOT   LPDIRECTDRAW2 lpDD2;
//_EXTORNOT   LPDIRECTINPUT lpDI;

_EXTORNOT   void* lpVideoRAM;
_EXTORNOT   LPDIRECTDRAWSURFACE lpddsPrimary;
_EXTORNOT   BOOL DirectActive, FULLSCREEN, BORDERLESS, RestartMode;
_EXTORNOT   BOOL LoDetailSky;
_EXTORNOT   int  WinW,WinH,WinEX,WinEY,VideoCX,VideoCY,VideoPitch,VideoPitchB,iBytesPerLine,ts,r,MapMinY;
_EXTORNOT   float CameraW,CameraH,Soft_Persp_K, stepdy, stepdd, SunShadowK, FOVK;
_EXTORNOT   CLIPPLANE ClipA,ClipB,ClipC,ClipD,ClipZ,ClipW;
_EXTORNOT   int u,vused, CCX, CCY;

_EXTORNOT   DWORD Mask1,Mask2;
_EXTORNOT   DWORD HeapAllocated, HeapReleased;


_EXTORNOT   EPoint VMap[kViewGridSize][kViewGridSize];
_EXTORNOT   EPoint VMap2[kViewGridSize][kViewGridSize];
_EXTORNOT   EPoint ev[3];

_EXTORNOT   ClipPoint cp[16];
_EXTORNOT   ClipPoint hleft,hright;


_EXTORNOT   void  *HLineT;
_EXTORNOT   int   rTColor;
_EXTORNOT   int   SKYMin, SKYDTime, GlassL, ctViewR, ctViewR1, ctViewRM,
            dFacesCount, ReverseOn, TDirection;
_EXTORNOT   WORD  FadeTab[65][0x8000];
_EXTORNOT   TElements Elements[700];
_EXTORNOT   TBTrail   BloodTrail;

_EXTORNOT   int     PrevTime, TimeDt, T, Takt, RealTime, StepTime, MyHealth, ExitTime, WaveNoteTime,
            ChCallTime, CallLockTime, NextCall;
_EXTORNOT   float   DeltaT;
_EXTORNOT   float   CameraX, CameraY, CameraZ, CameraAlpha, CameraBeta;
_EXTORNOT   float   PlayerX, PlayerY, PlayerZ, PlayerAlpha, PlayerBeta,
            HeadY, HeadBackR, HeadBSpeed, HeadAlpha, HeadBeta,
            SSpeed,VSpeed,RSpeed,YSpeed;
_EXTORNOT   Vector3d PlayerNv;

_EXTORNOT	Vector3d Recoil;

_EXTORNOT   float   ca,sa,cb,sb, wpnDAlpha, wpnDBeta;
_EXTORNOT   void    *lpVideoBuf, *lpTextureAddr;
_EXTORNOT   HBITMAP hbmpVideoBuf;
_EXTORNOT   HCURSOR hcArrow;
_EXTORNOT   int     DivTbl[10240];

_EXTORNOT   Vector3d  v[3];
_EXTORNOT   ScrPoint  scrp[3];
_EXTORNOT   MScrPoint mscrp[3];
_EXTORNOT   Vector3d  nv, waterclipbase, Sun3dPos;


_EXTORNOT   struct _t
{
  int fkForward, fkBackward, fkReload, fkResupply, fkHoldBreath, fkFiringMode, fkFire, fkShow, fkSLeft, fkSRight, fkStrafe, fkJump, fkRun, fkCrouch, fkCall, fkCCall, fkBinoc;
} KeyMap;


inline constexpr DWORD kfForward = 0x00000001;
inline constexpr DWORD kfBackward = 0x00000002;
inline constexpr DWORD kfLeft = 0x00000004;
inline constexpr DWORD kfRight = 0x00000008;
inline constexpr DWORD kfLookUp = 0x00000010;
inline constexpr DWORD kfLookDn = 0x00000020;
inline constexpr DWORD kfJump = 0x00000040;
inline constexpr DWORD kfDown = 0x00000080;
inline constexpr DWORD kfCall = 0x00000100;

inline constexpr DWORD kfSLeft = 0x00001000;
inline constexpr DWORD kfSRight = 0x00002000;
inline constexpr DWORD kfStrafe = 0x00004000;

inline constexpr DWORD fmWater = 0x0080;
inline constexpr DWORD fmWater2 = 0x8000;
inline constexpr DWORD fmNOWAY = 0x0020;
inline constexpr DWORD fmReverse = 0x0010;

inline constexpr DWORD fmWaterA = 0x8080;


inline constexpr int tresGround = 1;
inline constexpr int tresWater = 2;
inline constexpr int tresModel = 3;
inline constexpr int tresHunter = 4;
inline constexpr int tresChar = 5;

inline constexpr DWORD sfDoubleSide = 1;
inline constexpr DWORD sfDarkBack = 2;
inline constexpr DWORD sfOpacity = 4;
inline constexpr DWORD sfTransparent = 8;
inline constexpr DWORD sfMortal = 0x0010;
inline constexpr DWORD sfPhong = 0x0030;
inline constexpr DWORD sfEnvMap = 0x0050;

inline constexpr DWORD sfNeedVC = 0x0080;
inline constexpr DWORD sfDark = 0x8000;

inline constexpr DWORD ofPLACEWATER = 1;
inline constexpr DWORD ofPLACEGROUND = 2;
inline constexpr DWORD ofPLACEUSER = 4;
inline constexpr DWORD ofCIRCLE = 8;
inline constexpr DWORD ofBOUND = 16;
inline constexpr DWORD ofNOBMP = 32;
inline constexpr DWORD ofNOLIGHT = 64;
inline constexpr DWORD ofDEFLIGHT = 128;
inline constexpr DWORD ofGRNDLIGHT = 256;
inline constexpr DWORD ofNOSOFT = 512;
inline constexpr DWORD ofNOSOFT2 = 1024;
inline constexpr DWORD ofANIMATED = 0x80000000;

inline constexpr DWORD csONWATER = 0x00010000;
inline constexpr int MAX_HEALTH = 128000;

inline constexpr int HUNT_EAT = 0;
inline constexpr int HUNT_BREATH = 1;
inline constexpr int HUNT_FALL = 2;
inline constexpr int HUNT_KILL = 3;





inline constexpr int AI_MOSH = 1;
inline constexpr int AI_GALL = 2;
inline constexpr int AI_DIMOR = 3;
inline constexpr int AI_PTERA = 4;
inline constexpr int AI_DIMET = 5;
inline constexpr int AI_PIG = 6;


inline constexpr int AI_HUNTDOG = 9;

inline constexpr int AI_PARA = 10;
inline constexpr int AI_ANKY = 11;
inline constexpr int AI_STEGO = 12;
inline constexpr int AI_ALLO = 13;
inline constexpr int AI_CHASM = 14;
inline constexpr int AI_VELO = 15;
inline constexpr int AI_SPINO = 16;
inline constexpr int AI_CERAT = 17;
inline constexpr int AI_TREX = 18;


inline constexpr int AI_PACH = 19;

inline constexpr int AI_BRONT = 20;
inline constexpr int AI_HOG = 21;
inline constexpr int AI_WOLF = 22;
inline constexpr int AI_RHINO = 23;
inline constexpr int AI_DEER = 24;
inline constexpr int AI_SMILO = 25;
inline constexpr int AI_MAMM = 26;
inline constexpr int AI_BEAR = 27;

inline constexpr int AI_TITAN = 28;
inline constexpr int AI_MICRO = 29;


inline constexpr int AI_BRACH = 30;
inline constexpr int AI_ICTH = 31;
inline constexpr int AI_FISH = 32;
inline constexpr int AI_MOSA = 33;
inline constexpr int AI_BRACHDANGER = 34;
inline constexpr int AI_LANDBRACH = 35;


inline constexpr int AI_POACHER = 8;

//#define AI_FINAL	  29 //Last AI of max huntable roster (menu can only display 10)

//#define AI_POACHER    22












_EXTORNOT BOOL WATERANI,Clouds,SKY,GOURAUD,
          MODELS,TIMER,BITMAPP,MIPMAP,
          NOCLIP,CLIP3D,NODARKBACK,CORRECTION, LOWRESTX,
          FOGENABLE, FOGON, CAMERAINFOG,
          WATERREVERSE,waterclip,UNDERWATER, ONWATER, NeedWater,
          SWIM, FLY, PAUSE, OPTICMODE, BINMODE, EXITMODE, MapMode, RunMode, CrouchMode;
_EXTORNOT int  CameraFogI;
_EXTORNOT int OptDayNight, OptAgres, OptDens, OptSens, OptRes, OptViewR,
          OptMsSens, OptBrightness, OptSound, OptRender, OptObjectDetail,
          OptText, OptSys, WaitKey, OPT_ALPHA_COLORKEY;
_EXTORNOT int  NightVisionKey;
_EXTORNOT int  OptFov;
_EXTORNOT int  OptFpsLimit;
_EXTORNOT int  OptTerrainLOD;
_EXTORNOT float UIScale;
_EXTORNOT int  CurRes, ResCount;
_EXTORNOT TRes ResolutionList[128];
_EXTORNOT BOOL SHADOWS3D,REVERSEMS;

_EXTORNOT BOOL SLOW, DEBUG, MORPHP, MORPHA;
_EXTORNOT bool g_VerboseLogging;
_EXTORNOT HANDLE hlog;


enum AudioSystemEnum {
	AUDIO_DIRECTSOUND = 0,
	AUDIO_OPENALSOFT = 1,
	AUDIO_BACKEND_COUNT = 2
};

inline int NormalizeAudioBackend(int driver)
{
	// Legacy saved values:
	// 0..3 = software / DirectSound / A3D / EAX → DirectSound
	// 4..5 = OpenAL / XAudio2 → OpenAL Soft
	return (driver == AUDIO_OPENALSOFT || driver >= 4) ? AUDIO_OPENALSOFT : AUDIO_DIRECTSOUND;
}

//========== for audio ==============//
void  AddVoicev  (int, short int*, int);
void  AddVoice3dv(int, short int*, float, float, float, int);
void  AddVoice3d (int, short int*, float, float, float);

void SetAmbient3d(int, short int*, float, float, float);
void SetAmbient(int, short int*, int);
void AudioSetCameraPos(float, float, float, float, float);
void InitAudioSystem(HWND, HANDLE, int);
void Audio_Restore();
void AudioStop();
void Audio_Shutdown();
void Audio_SetEnvironment(int, float);
void Audio_UploadGeometry();
//=================================
struct AudioQuad
{
  float x1,y1,z1;
  float x2,y2,z2;
  float x3,y3,z3;
  float x4,y4,z4;
};
_EXTORNOT int AudioFCount;
_EXTORNOT AudioQuad data[8192];
_EXTORNOT void UploadGeometry();
_EXTORNOT int Env;

//========== for 3d hardware =============//
_EXTORNOT BOOL HARD3D;
void ShowVideo();
void Init3DHardware();
void Activate3DHardware();
void ShutDown3DHardware();
void Render3DHardwarePosts();
void CopyBackToDIB();
void CopyHARDToDIB();
void Hardware_ZBuffer(BOOL zb);
void AllocateRenderTables(void);

void EnumerateResolutions();

//=========== loading =============
void StartLoading();
void EndLoading();
void PrintLoad(char *t);

#ifdef _MAIN_
_EXTORNOT char KeysName[256][24] =
{
  "...",
  "Esc",
  "1",
  "2",
  "3",
  "4",
  "5",
  "6",
  "7",
  "8",
  "9",
  "0",
  "-",
  "=",
  "BSpace",
  "Tab",
  "Q",
  "W",
  "E",
  "R",
  "T",
  "Y",
  "U",
  "I",
  "O",
  "P",
  "[",
  "]",
  "Enter",
  "Ctrl",
  "A",
  "S",
  "D",
  "F",
  "G",
  "H",
  "J",
  "K",
  "L",
  ";",
  "'",
  "~",
  "Shift",
  "\\",
  "Z",
  "X",
  "C",
  "V",
  "B",
  "N",
  "M",
  ",",
  ".",
  "/",
  "Shift",
  "*",
  "Alt",
  "Space",
  "CLock",
  "F1",
  "F2",
  "F3",
  "F4",
  "F5",
  "F6",
  "F7",
  "F8",
  "F9",
  "F10",
  "NLock",
  "SLock",
  "Home",
  "Up",
  "PgUp",
  "-",
  "Left",
  "Midle",
  "Right",
  "+",
  "End",
  "Down",
  "PgDn",
  "Ins",
  "Del",
  "",
  "",
  "",
  "F11",
  "F12",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "Mouse1",
  "Mouse2",
  "Mouse3",
  "<?>",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""
};
#else
_EXTORNOT char KeysName[128][24];
#endif
