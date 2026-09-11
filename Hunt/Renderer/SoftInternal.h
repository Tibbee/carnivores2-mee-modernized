// ==========================================================================
// SoftInternal.h -- Shared internal declarations for the software renderer
//
// Global state and structs used across the software renderer's split
// implementation files (SoftHUD, SoftModel, SoftTerrain, SoftWater,
// SoftEntities, SoftVideo, SoftSky, SoftMisc).
// ==========================================================================

#ifndef SOFTINTERNAL_H
#define SOFTINTERNAL_H

#pragma once

#include "Hunt.h"
#include <algorithm>

#ifdef _soft

// ============================================================================
// Character/object render list structures
// ============================================================================

struct TCharListItem
{
  int CType, Index;
};

struct TCharListLine
{
  int ICount;
  TCharListItem Items[256];
};

extern TCharListLine ChRenderList[];

extern Vector2di ORList[2][2048];
extern int ORLCount[2];
extern int rmlistselector;

// ============================================================================
// Inline-asm rasterizer global state
//
// These variables are read/written across model rendering, terrain texturing,
// and clipping functions. They serve as pseudo-registers for the inline-asm
// rasterizer pipeline and must remain as module-level globals.
// ============================================================================

extern int xa;
extern int xb;
extern int xa16;
extern int xb16;
extern int v1, v2, v3,
           l1, l2, l3,
           x1, Y1, tx1, ty1, lt1, zdepth1,
           x2, y2, tx2, ty2, lt2, zdepth2,
           x3, y3, tx3, ty3, lt3, zdepth3;

extern int dx1,  dx2,  dx3,
           dtx1, dty1, dlt1,
           dtx2, dty2, dlt2,
           dtx3, dty3, dlt3,
           tyb, tya, txa, txb, lta, ltb,
           ddtx1, ddty1, ddtx2, ddty2, ddtx3, ddty3;

extern int ctdy, ctdx, cdlt;
extern int _sp;
extern float k;
extern BOOL LockWater;
extern int OpacityMode;

// ============================================================================
// Assembly-routine forward declarations
// ============================================================================

extern void DrawCorrectedTexturedFace();
extern void DrawModelFace();
extern void DrawModelFaces();
extern void HLineTxModelBMP();
extern void RenderSkyLineFadeLo(int, int);
extern void RenderSkyLineFade(int, int);
extern void RenderSkyLineLo(int);
extern void RenderSkyLine(int);

// ============================================================================
// Internal function forward declarations
// ============================================================================

void RenderBMPModel2(TBMPModel*, float, float, float, int);

// Render list management (defined in SoftEntities.cpp, called from SoftTerrain.cpp)
void RenderChList(int r);

void _FillMemoryWord(int maddr, int count, WORD w);
void FillMemoryWord(int maddr, int count, WORD w);
void _memcpy(void* daddr, void* saddr, int count);

#endif // _soft
#endif // SOFTINTERNAL_H
