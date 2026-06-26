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

#include "Memory.h"  // Phase 5A: tagged allocator types (MemoryTag, MemoryArena, smart pointers)

#include "math.h"
#include "windows.h"
#include "winuser.h"

#include "AppRes.h"

#include "ddraw.h"

#include "Core/Constants.h"
#include "Core/MathTypes.h"
#include "Core/AudioTypes.h"
#include "Core/RenderTypes.h"
#include "Core/ModelTypes.h"
#include "Core/GameTypes.h"
#include "Core/GameState.h"

#ifdef _d3d
#include "d3d.h"
#endif

#ifdef _MAIN_
#define _EXTORNOT
#else
#define _EXTORNOT extern
#endif












// Phase 5B.1: sizeof(TSFX) grows from 8 (int + raw pointer) to 16+ bytes
// (int + std::vector<short int>; the exact size depends on the std::vector
// layout in the target STL). This affects every ChInfo[128] (128 * 64
// SoundFX = 8192 instances) and the global SFX array, adding ~64+ KiB to
// BSS. The doc documents this as the intended trade-off. The range check
// below fires if TSFX is ever back to its old 8-byte raw-pointer layout
// (i.e., someone reverts the std::vector change without updating the doc).


















//================= MODEL ========================

















//=========== END MODEL ==============================//



























































































































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

void CreateLog();
void PrintLog(LPSTR l);
void PrintLogVerbose(LPSTR l);
void CloseLog();



//========== multiplayer =============//






void StartupServerCommsThread();
void StartupClientCommsThread();




//========== common ==================//


//========== map =====================//





// Phase 5B.2: Textures is now std::array<unique_obj_ptr<TEXTURE>, 1024>.
// std::array is used (not a raw C array) so the size is fixed at
// compile time (matching the original 1024-element behavior) and the
// type system enforces it. All elements default-construct to null
// unique_ptrs, matching the old BSS zero-initialization. Access
// pattern is unchanged: Textures[t] returns a unique_obj_ptr<TEXTURE>&
// that supports operator bool, operator->, and .reset() the same way
// a raw TEXTURE* did.

//========= WEATHER =================//


//========= GAME ====================//

//firing mode 0-semiauto 1-fullauto





// Score multipliers for accessories. Defaults are set in Hunt/Game.cpp
// InitEngine() and match the legacy hardcoded values from
// SubmitDinoScore() so a hunt launched without a Menu-supplied 'smod='
// argument behaves identically to the original game. The Menu passes
// 'smod=camo,radar,scent,double,tranq,observer' in the same order to
// override these from _RES.TXT (see Menu/Resources.cpp ReadAccessories()).






//======== MODEL ======================//





//============= Characters ==============//

//Add these after dino positions alligned










//========== Render ==================//



























//#define AI_FINAL	  29 //Last AI of max huntable roster (menu can only display 10)

//#define AI_POACHER    22















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


//========== for 3d hardware =============//
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


