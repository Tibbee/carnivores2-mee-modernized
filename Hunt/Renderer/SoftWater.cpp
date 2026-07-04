// ==========================================================================
// SoftWater.cpp — Software renderer water surface rendering
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft

void RenderWater()
{
}

void ProcessWaterMap(int x, int y, int r)
{
  //WATERREVERSE = true;
  ReverseOn = (FMap[y][x] & fmReverse);
  TDirection = (FMap[y][x] & 3);

  int t1 = TMap1[y][x];
  int t2 = TMap2[y][x];

  x = x - CCX + 64;
  y = y - CCY + 64;

  if ((VMap2[y][x].DFlags & VMap2[y][x+1].DFlags & VMap2[y+1][x+1].DFlags & VMap2[y+1][x].DFlags) == 0xFFFF) return;

  if (VMap2[y][x].DFlags!=0xFFFF) ev[0] = VMap2[y][x];
  else ev[0] = VMap[y][x];
  if (VMap2[y][x+1].DFlags!=0xFFFF) ev[1] = VMap2[y][x+1];
  else ev[1] = VMap[y][x+1];

  if (ReverseOn)
    if (VMap2[y+1][x].DFlags!=0xFFFF) ev[2] = VMap2[y+1][x];
    else ev[2] = VMap[y+1][x];
  else if (VMap2[y+1][x+1].DFlags!=0xFFFF) ev[2] = VMap2[y+1][x+1];
  else ev[2] = VMap[y+1][x+1];

  lpTextureAddr = &(Textures[0]->DataB[0]);
  HLineT = (void*) HLineTBGlass25;

  if (!t1)
    if (r>4) DrawTPlane(false);
    else DrawTPlaneClip(false);

  if (ReverseOn)
  {
    ev[0] = ev[2];
    if (VMap2[y+1][x+1].DFlags!=0xFFFF) ev[2] = VMap2[y+1][x+1];
    else ev[2] = VMap[y+1][x+1];
  }
  else
  {
    ev[1] = ev[2];
    if (VMap2[y+1][x].DFlags!=0xFFFF) ev[2] = VMap2[y+1][x];
    else ev[2] = VMap[y+1][x];
  }


  if (!t2)
    if (r>4) DrawTPlane(true);
    else DrawTPlaneClip(true);
}

void ProcessMapW(int x, int y, int r)
{
  //if (RunMode) return;


  if (x>=ctMapSize-1 || y>=ctMapSize-1 || x<0 || y<0) return;

  int t1 = WaterList[ WMap[y][x] ].tindex;
  int hw = WaterList[ WMap[y][x] ].wlevel;

  ev[0] = VMap2[y - CCY + kViewGridCenter][x - CCX + kViewGridCenter];
  if (ev[0].v.z>BackViewR) return;


  ReverseOn = (FMap[y][x] & fmReverse);
  TDirection = 0;
//   if ( (HMap[y][x]>hw) || (HMap[y+1][x+1]>hw) ) ReverseOn = true;



  int _x = x;
  int _y = y;

  x = x - CCX + kViewGridCenter;
  y = y - CCY + kViewGridCenter;
  ev[1] = VMap2[y][x+1];
  if (ReverseOn) ev[2] = VMap2[y+1][x];
  else ev[2] = VMap2[y+1][x+1];

  float xx = (ev[0].v.x + VMap2[y+1][x+1].v.x) / 2;
  float yy = (ev[0].v.y + VMap2[y+1][x+1].v.y) / 2;
  float zz = (ev[0].v.z + VMap2[y+1][x+1].v.z) / 2;

  int zs;

  if ( fabs(xx*FOVK) > -zz + BackViewR) return;

  const float distanceSq = xx*xx + zz*zz + yy*yy;
  const float viewDistance = ctViewR * 256.0f;
  if (distanceSq > viewDistance * viewDistance) return;
  zs = static_cast<int>(sqrt(distanceSq));

  GlassL = 0;

  if (MIPMAP) ts = static_cast<int>(CameraW) * 4 * 128 / zs;
  else ts = 128;

  ts = 128;

  if (ts>=128)
  {
    lpTextureAddr = &(Textures[t1]->DataA[0]);
    HLineT = (void*) HLineTxGOURAUD;
  }
  else if (ts>=64)
  {
    lpTextureAddr = &(Textures[t1]->DataB[0]);
    HLineT = (void*) HLineTxB;
  }
  else
  {
    lpTextureAddr = &(Textures[t1]->DataC[0]);
    HLineT = (void*) HLineTxC;
  }

  if (IsUnderwater())
  {
    lpTextureAddr = &(Textures[t1]->DataB[0]);
    HLineT = (void*) HLineTBGlass25;
  }

  WATERREVERSE = IsUnderwater();

  if (ReverseOn)
  {
    if ( (HMap[_y][_x]>hw) || (HMap[_y][_x+1]>hw) || (HMap[_y+1][_x]>hw) )   goto S1;
  }
  else
  {
    if ( (HMap[_y][_x]>hw) || (HMap[_y][_x+1]>hw) || (HMap[_y+1][_x+1]>hw) )   goto S1;
  }

  if (r>6) DrawTPlane(false);
  else DrawTPlaneClip(false);
S1:
  if (ReverseOn)
  {
    ev[0] = ev[2];
    ev[2] = VMap2[y+1][x+1];
  }
  else
  {
    ev[1] = ev[2];
    ev[2] = VMap2[y+1][x];
  }

  if (ReverseOn)
  {
    if ( (HMap[_y][_x+1]>hw) || (HMap[_y+1][_x+1]>hw) || (HMap[_y+1][_x]>hw) )   goto S2;
  }
  else
  {
    if ( (HMap[_y][_x]>hw) || (HMap[_y+1][_x+1]>hw) || (HMap[_y+1][_x]>hw) )   goto S2;
  }

  if (r>6) DrawTPlane(true);
  else DrawTPlaneClip(true);
S2:
  WATERREVERSE = false;
}


void ProcessMapW2(int x, int y, int r)
{
  //if (RunMode) return;
  if (!( (FMap[y  ][x  ] & fmWaterA) &&
         (FMap[y  ][x+2] & fmWaterA) &&
         (FMap[y+2][x  ] & fmWaterA) &&
         (FMap[y+2][x+2] & fmWaterA) )) return;

  if (x>=ctMapSize-1 || y>=ctMapSize-1 || x<0 || y<0) return;

  int t1 = WaterList[ WMap[y][x] ].tindex;
  int hw = WaterList[ WMap[y][x] ].wlevel;

  ev[0] = VMap2[y - CCY + kViewGridCenter][x - CCX + kViewGridCenter];
  if (ev[0].v.z>BackViewR) return;


// ReverseOn = (FMap[y][x] & fmReverse);
  TDirection = 0;
  if ( (HMap[y][x]>hw) || (HMap[y+2][x+2]>hw) ) ReverseOn = true;


  int _x = x;
  int _y = y;

  x = x - CCX + kViewGridCenter;
  y = y - CCY + kViewGridCenter;
  ev[1] = VMap2[y][x+2];
  if (ReverseOn) ev[2] = VMap2[y+2][x];
  else ev[2] = VMap2[y+2][x+2];

  float xx = (ev[0].v.x + VMap2[y+1][x+2].v.x) / 2;
  float yy = (ev[0].v.y + VMap2[y+1][x+2].v.y) / 2;
  float zz = (ev[0].v.z + VMap2[y+1][x+2].v.z) / 2;

  int zs;

  if ( fabs(xx*FOVK) > -zz + BackViewR) return;

  const float distanceSq = xx*xx + zz*zz + yy*yy;
  const float viewDistance = ctViewR * 256.0f;
  if (distanceSq > viewDistance * viewDistance) return;
  zs = static_cast<int>(sqrt(distanceSq));


  GlassL = 0;

  if (MIPMAP) ts = static_cast<int>(CameraW) * 4 * 128 / zs;
  else ts = 128;


  if (IsUnderwater())
  {
    lpTextureAddr = &(Textures[t1]->DataB[0]);
    HLineT = (void*) HLineTBGlass25;
  }
  else
  {
    lpTextureAddr = &(Textures[t1]->DataC[0]);
    HLineT = (void*) HLineTxC;
  }


  WATERREVERSE = IsUnderwater();

  if (ReverseOn)
  {
    if ( (HMap[_y][_x]>hw) && (HMap[_y][_x+2]>hw) && (HMap[_y+2][_x]>hw) )   goto S1;
  }
  else
  {
    if ( (HMap[_y][_x]>hw) && (HMap[_y][_x+2]>hw) && (HMap[_y+2][_x+2]>hw) )   goto S1;
  }

  if (r>6) DrawTPlane(false);
  else DrawTPlaneClip(false);
S1:
  if (ReverseOn)
  {
    ev[0] = ev[2];
    ev[2] = VMap2[y+2][x+2];
  }
  else
  {
    ev[1] = ev[2];
    ev[2] = VMap2[y+2][x];
  }

  if (ReverseOn)
  {
    if ( (HMap[_y][_x+2]>hw) && (HMap[_y+2][_x+2]>hw) && (HMap[_y+2][_x]>hw) )   goto S2;
  }
  else
  {
    if ( (HMap[_y][_x]>hw) && (HMap[_y+2][_x+2]>hw) && (HMap[_y+2][_x]>hw) )   goto S2;
  }

  if (r>6) DrawTPlane(true);
  else DrawTPlaneClip(true);
S2:
  WATERREVERSE = false;
}

#endif // _soft
