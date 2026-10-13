// ==========================================================================
// SoftWater.cpp -- Software renderer water surface rendering
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"
#include "Core/TerrainMip.h"

#ifdef _soft

void RenderWater()
{
  // Drive the shared ProcessMapW over every grid cell in view. ProcessMapW
  // renders the water surface and already handles the underwater case
  // internally (glass blending via IsUnderwater()), so this is what makes
  // the surface visible from below. Mirrors the D3D/3DFX RenderWater loop.
  for (int r=ctViewR; r>0; r--)
  {
    for (int x=-r; x<=r; x++)
    {
      ProcessMapW(CCX+x, CCY+r, r);
      ProcessMapW(CCX+x, CCY-r, r);
    }
    for (int y=-r+1; y<r; y++)
    {
      ProcessMapW(CCX+r, CCY+y, r);
      ProcessMapW(CCX-r, CCY+y, r);
    }
  }
}

void ProcessMapW(int x, int y, int r)
{
  //if (RunMode) return;


  if (x>=ctMapSize-1 || y>=ctMapSize-1 || x<0 || y<0) return;

  // Only draw cells whose four corners are all water. VMap2 (the water-plane
  // vertices) is populated in Effects.cpp exclusively for fmWaterA cells, so
  // for any other cell its entries are stale and yield garbage/spike
  // triangles. The terrain path already enforces this via its "wpr" check;
  // mirror it here so RenderWater() (which walks every grid cell in the ring)
  // never reads stale VMap2.
  if (!((FMap[y][x]   & fmWaterA) &&
        (FMap[y][x+1] & fmWaterA) &&
        (FMap[y+1][x] & fmWaterA) &&
        (FMap[y+1][x+1] & fmWaterA))) return;

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
  WaterTileDraw = 1;

  // The legacy source computed this level and then threw it away with an
  // unconditional "ts = 128" on the next line, so ProcessMapW always
  // rasterised water from DataA at full resolution while the terrain path
  // beside it honoured the result. Water is seen at grazing incidence --
  // exactly the case mipmapping exists for -- so honour the level now.
  //
  // Safe with respect to model LOD: Hunt.cpp draws RenderGround() (terrain
  // and every RenderObject) before RenderWater(), and the model rasteriser
  // reuses this same ts global, so nothing reads a water-derived value
  // before RenderGround reassigns it next frame.
  if (MIPMAP && OptWaterMipmap) ts = ComputeMipTexelSize(CameraW, zs);
  else ts = 128;

  switch (SelectRasterMipLevel(ts))
  {
  case 0:
    lpTextureAddr = &(Textures[t1]->DataA[0]);
    HLineT = (void*) HLineTxGOURAUD;
    break;
  case 1:
    lpTextureAddr = &(Textures[t1]->DataB[0]);
    HLineT = (void*) HLineTxB;
    break;
  default:
    lpTextureAddr = &(Textures[t1]->DataC[0]);
    HLineT = (void*) HLineTxC;
    break;
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

  DrawTPlaneClip(false);
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

  DrawTPlaneClip(true);
S2:
  WATERREVERSE = false;
  WaterTileDraw = 0;
}


void ProcessMapW2(int x, int y, int r)
{
  //if (RunMode) return;

  // UNREACHABLE since e4cdd32 ("drop 2x2 terrain LOD, use 1x1 tiles
  // everywhere"): the only caller was ProcessMap2, and RenderGround walks
  // ProcessMap alone. Kept in step with the software renderer in case the
  // 2x2 far-LOD system ever returns. Note this function never consumed the
  // mip level in the legacy source either -- it draws at a fixed LOD -- so
  // no ts is computed here.
  if (x >= ctMapSize - 2 || y >= ctMapSize - 2 || x < 0 || y < 0) return;

  if (!( (FMap[y  ][x  ] & fmWaterA) &&
         (FMap[y  ][x+2] & fmWaterA) &&
         (FMap[y+2][x  ] & fmWaterA) &&
         (FMap[y+2][x+2] & fmWaterA) )) return;

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


  // Legacy behaviour, restored: seen from underwater the surface blends 25%
  // glass over the scene from DataB; from above it is opaque from DataC.
  // The modernization dropped this branch (keeping only WATERREVERSE), so
  // swimming left far water drawn opaque from the 32x32 level.
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

  DrawTPlaneClip(false);
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

  DrawTPlaneClip(true);
S2:
  WATERREVERSE = false;
}

#endif // _soft
