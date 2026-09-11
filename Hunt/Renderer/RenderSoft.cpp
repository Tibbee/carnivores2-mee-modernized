// ==========================================================================
// RenderSoft.cpp -- Software renderer global state definitions
//
// After the monolithic file was split into focused files (SoftHUD.cpp,
// SoftModel.cpp, SoftTerrain.cpp, SoftWater.cpp, SoftEntities.cpp,
// SoftVideo.cpp, SoftSky.cpp, SoftMisc.cpp), this file remains as the
// home for the shared global state and rasterizer pseudo-registers
// that are declared extern in SoftInternal.h.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft

TCharListLine ChRenderList[kViewDistanceMax + 1];

Vector2di ORList[2][2048];
int ORLCount[2];

int rmlistselector;

int xa;
int xb;
int xa16;
int xb16;
int v1, v2, v3,
    l1, l2, l3,
    x1, Y1, tx1, ty1, lt1, zdepth1,
    x2, y2, tx2, ty2, lt2, zdepth2,
    x3, y3, tx3, ty3, lt3, zdepth3;

int dx1,  dx2,  dx3,
    dtx1, dty1, dlt1,
    dtx2, dty2, dlt2,
    dtx3, dty3, dlt3,
    tyb, tya, txa, txb, lta, ltb,
    ddtx1, ddty1, ddtx2, ddty2, ddtx3, ddty3;

int ctdy, ctdx, cdlt;
int _sp;
float k;
BOOL LockWater;
int OpacityMode;

#endif // _soft
