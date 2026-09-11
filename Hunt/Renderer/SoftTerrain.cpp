// ==========================================================================
// SoftTerrain.cpp -- Software renderer terrain and ground rendering
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft
void _RenderObject(int x, int y)
{
  int ob = OMap[y][x];

  if (!MObjects[ob].model)
  {
    //return;
    sprintf_s(logt, sizeof(logt),"Incorrect model at [%d][%d]!", x, y);
    DoHalt(logt);
  }

  int FI = (FMap[y][x] >> 2) & 3;
  float fi = CameraAlpha + static_cast<float>((FI * 2.f*pi / 4.f));

  int mlight;
  if (MObjects[ob].info.flags & (ofDEFLIGHT+ofGRNDLIGHT) )
    mlight = MObjects[ob].info.DefLight;
  else /*
	  if (MObjects[ob].info.flags & ofGRNDLIGHT)
	  {
		  mlight = 128;
		  CalcModelGroundLight(MObjects[ob].model.get(), x*256+128, y*256+128, FI);
		  FI = 0;
	  }
	  else */
    mlight = -(RandomMap[y & 31][x & 31] >> 5) + (LMap[y][x]>>1) + 96;

  if (mlight >192) mlight =192;
  if (mlight < 64) mlight = 64;

  v[0].x = x*256+128 - CameraX;
  v[0].z = y*256+128 - CameraZ;
  v[0].y = static_cast<float>((HMapO[y][x])) * ctHScale - CameraY;

  waterclip = false;

  if (!IsUnderwater())
    if (FMap[y][x] & fmWaterA)
      if (HMapO[y][x] < WaterList[ WMap[y][x] ].wlevel)
      {

        if (WaterList[ WMap[y][x] ].wlevel * ctHScale >
            HMapO[y][x] * ctHScale + MObjects[ob].info.YHi) return;

        waterclipbase  = v[0];
        waterclipbase.y = WaterList[ WMap[y][x] ].wlevel * ctHScale - CameraY;
        waterclipbase = RotateVector(waterclipbase);
        waterclip = true;
      }


  float zs = VectorLength(v[0]);

  if (v[0].y + MObjects[ob].info.YHi < static_cast<int>((HMap[y][x]+HMap[y+1][x+1])) / 2 * ctHScale - CameraY) return;

  v[0] = RotateVector(v[0]);
  GlassL = 0;

  if (zs > 256 * (ctViewR-4))
    GlassL = static_cast<int>((std::min)(255.0f, zs / 4.0f - 64.0f * (ctViewR - 4)));

  if (GlassL==255) return;


  if (MObjects[ob].info.flags & ofANIMATED)
    if (MObjects[ob].info.LastAniTime!=RealTime)
    {
      MObjects[ob].info.LastAniTime=RealTime;
      CreateMorphedObject(MObjects[ob].model.get(),
                          MObjects[ob].vtl,
                          RealTime % MObjects[ob].vtl.AniTime);
    }



  if (MObjects[ob].info.flags & ofNOBMP) zs = 0;
  if (zs>ctViewRM*256)
    RenderBMPModel(&MObjects[ob].bmpmodel, v[0].x, v[0].y, v[0].z, mlight-16);
  else if (v[0].z<-256*12 && !waterclip)
    RenderModel(MObjects[ob].model.get(), v[0].x, v[0].y, v[0].z, mlight, FI, fi, CameraBeta);
  else
    RenderModelClip(MObjects[ob].model.get(), v[0].x, v[0].y, v[0].z, mlight, FI, fi, CameraBeta);


}




void RenderMList()
{
  rmlistselector=1-rmlistselector;

  for (int o=0; o<ORLCount[rmlistselector]; o++)
    _RenderObject(ORList[rmlistselector][o].x,
                  ORList[rmlistselector][o].y);
  ORLCount[rmlistselector] = 0;
}




void RenderGround()
{

  rmlistselector = 0;
  ORLCount[0] = 0;
  ORLCount[1] = 0;
  LockWater = false;


  for (r=ctViewR; r>0; r--)
  {
    for (int x=-r; x<=r; x++)
    {
      ProcessMap(CCX+x, CCY+r, r);
      ProcessMap(CCX+x, CCY-r, r);
    }
    for (int y=-r+1; y<r; y++)
    {
      ProcessMap(CCX+r, CCY+y, r);
      ProcessMap(CCX-r, CCY+y, r);
    }
    RenderMList();
    RenderMList();
    RenderChList(r);
  }

  ProcessMap(CCX, CCY, 0);
  RenderMList();
  RenderMList();
  RenderChList(0);
}



void RenderObject(int x, int y)
{
  if (OMap[y][x]==255) return;
  if (!MODELS) return;
  int o = ORLCount[rmlistselector];
  if (o>2000) return;
  ORList[rmlistselector][o].x = x;
  ORList[rmlistselector][o].y = y;
  ORLCount[rmlistselector]++;
}







void ProcessMap2(int x, int y, int r)
{
  //WATERREVERSE = false;
  if (x>=ctMapSize-2 || y>=ctMapSize-2 ||
      x<0 || y<0) return;

  float BackR = BackViewR;
  if (OMap[y][x]!=255) BackR+=MObjects[OMap[y][x]].info.BoundR;

  ev[0] = VMap[y - CCY + kViewGridCenter][x - CCX + kViewGridCenter];
  if (ev[0].v.z>BackR) return;


  int t1 = TMap2[y][x];
  int hw = WaterList[ WMap[y][x] ].wlevel-1;

  ReverseOn = false;
  TDirection = ((FMap[y][x]>>8) & 3);


  int _x = x;
  int _y = y;
  x = x - CCX + kViewGridCenter;
  y = y - CCY + kViewGridCenter;
  ev[1] = VMap[y][x+2];
  if (ReverseOn) ev[2] = VMap[y+2][x];
  else ev[2] = VMap[y+2][x+2];

  float xx = (ev[0].v.x + VMap[y+2][x+2].v.x) / 2;
  float yy = (ev[0].v.y + VMap[y+2][x+2].v.y) / 2;
  float zz = (ev[0].v.z + VMap[y+2][x+2].v.z) / 2;
  int zs;

  // FOVK-scaled: without it wide FOV (FOVK < 1) culls visible side tiles.
  // DrawTPlaneClip's ClipA-D planes are FOV-correct, so this only widens
  // acceptance toward the true frustum (strictly fewer false culls).
  if ( fabs(xx*FOVK) > -zz + BackR) return;

  const float distanceSq = xx*xx + zz*zz + yy*yy;
  const float viewDistance = ctViewR * 256.0f;
  if (distanceSq > viewDistance * viewDistance) return;
  zs = static_cast<int>(sqrt(distanceSq));
  GlassL = 0;

  if (MIPMAP) ts = static_cast<int>(CameraW) * 4 * 128 / zs;
  else ts = 128;

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

  if (zs > 256 * (ctViewR-4))
  {
    GlassL = static_cast<int>((std::min)(255.0f, zs / 4.0f - 64.0f * (ctViewR - 4)));

    if (GlassL) lpTextureAddr = &(Textures[t1]->SDataC[1]);
    if (GlassL)
      if (GlassL>160) HLineT = (void*) HLineTDGlass25;
      else if (GlassL>80 ) HLineT = (void*) HLineTDGlass50;
      else
        HLineT = (void*) HLineTDGlass75;
  }




  if (!IsUnderwater())
    if (ReverseOn)
    {
      if ( (HMap[_y][_x]<hw) && (HMap[_y][_x+2]<hw) && (HMap[_y+2][_x]<hw) )   goto S1;
    }
    else
    {
      if ( (HMap[_y][_x]<hw) && (HMap[_y][_x+2]<hw) && (HMap[_y+2][_x+1]<hw) )   goto S1;
    }

  DrawTPlane(false);

S1:

  if (!IsUnderwater())
    if (ReverseOn)
    {
      if ( (HMap[_y][_x+2]<hw) && (HMap[_y+2][_x+2]<hw) && (HMap[_y+2][_x]<hw) )   goto S2;
    }
    else
    {
      if ( (HMap[_y][_x]<hw) && (HMap[_y+2][_x+2]<hw) && (HMap[_y+2][_x]<hw) )   goto S2;
    }

  if (ReverseOn)
  {
    ev[0] = ev[2];
    ev[2] = VMap[y+2][x+2];
  }
  else
  {
    ev[1] = ev[2];
    ev[2] = VMap[y+2][x];
  }

  DrawTPlane(true);
S2:

  x = x + CCX - kViewGridCenter;
  y = y + CCY - kViewGridCenter;

  if (!LockWater)
  {
    RenderObject(x, y);
    RenderObject(x+1, y);
    RenderObject(x, y+1);
    RenderObject(x+1, y+1);
    if (FMap[y][x] & fmWaterA) ProcessMapW2(x,y,r);
  }
}



void ProcessMap(int x, int y, int r)
{
  if (x>=ctMapSize-1 || y>=ctMapSize-1 ||
      x<0 || y<0) return;

  float BackR = BackViewR;
  if (OMap[y][x]!=255) BackR+=MObjects[OMap[y][x]].info.BoundR;
  int zs;
  float xx,yy,zz;
  int hw = WaterList[ WMap[y][x] ].wlevel;


  ev[0] = VMap[y - CCY + kViewGridCenter][x - CCX + kViewGridCenter];
  if (ev[0].v.z>BackR) return;

  BOOL
  wpr = ((FMap[y  ][x  ] & fmWaterA) &&
         (FMap[y  ][x+1] & fmWaterA) &&
         (FMap[y+1][x  ] & fmWaterA) &&
         (FMap[y+1][x+1] & fmWaterA) );



  int ob = OMap[y][x];
  if (!MODELS) ob=255;

  int t1 = TMap1[y][x];
  ReverseOn = (FMap[y][x] & fmReverse);
  TDirection = (FMap[y][x] & 3);

  int _x = x;
  int _y = y;
  x = x - CCX + kViewGridCenter;
  y = y - CCY + kViewGridCenter;


  ev[1] = VMap[y][x+1];
  if (ReverseOn) ev[2] = VMap[y+1][x];
  else ev[2] = VMap[y+1][x+1];

  xx = (ev[0].v.x + VMap[y+1][x+1].v.x) / 2;
  yy = (ev[0].v.y + VMap[y+1][x+1].v.y) / 2;
  zz = (ev[0].v.z + VMap[y+1][x+1].v.z) / 2;


  // FOVK-scaled (see ProcessMap2): without it wide FOV culls visible sides.
  if ( fabs(xx*FOVK) > -zz + BackR) return;

  const float distanceSq = xx*xx + zz*zz + yy*yy;
  const float viewDistance = ctViewR * 256.0f;
  if (distanceSq > viewDistance * viewDistance) return;
  zs = static_cast<int>(sqrt(distanceSq));

  GlassL = 0;


  if (MIPMAP) ts = static_cast<int>(CameraW) * 4 * 128 / zs;
  else ts = 128;

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

  if (!IsUnderwater())
    if (wpr)
      if (ReverseOn)
      {
        if ( (HMap[_y][_x]<hw) || (HMap[_y][_x+1]<hw) || (HMap[_y+1][_x]<hw) )   goto S1;
      }
      else
      {
        if ( (HMap[_y][_x]<hw) || (HMap[_y][_x+1]<hw) || (HMap[_y+1][_x+1]<hw) )   goto S1;
      }


  if (r>6) DrawTPlane(false);
  else DrawTPlaneClip(false);
S1:
  if (ReverseOn)
  {
    ev[0] = ev[2];
    ev[2] = VMap[y+1][x+1];
  }
  else
  {
    ev[1] = ev[2];
    ev[2] = VMap[y+1][x];
  }

  if (!IsUnderwater())
    if (wpr)
      if (ReverseOn)
      {
        if ( (HMap[_y][_x+1]<hw) || (HMap[_y+1][_x+1]<hw) || (HMap[_y+1][_x]<hw) )   goto S2;
      }
      else
      {
        if ( (HMap[_y][_x]<hw) || (HMap[_y+1][_x+1]<hw) || (HMap[_y+1][_x]<hw) )   goto S2;
      }

  if (r>6) DrawTPlane(true);
  else DrawTPlaneClip(true);
S2:
  x = x + CCX - kViewGridCenter;
  y = y + CCY - kViewGridCenter;

SKIP:

  RenderObject(x, y);

  if (wpr) ProcessMapW(x,y,r);

}






void ClipVector(CLIPPLANE& C, int vn)
{
  int ClipRes = 0;
  float s,s1,s2;
  int vleft  = (vn-1);
  if (vleft <0) vleft=vused-1;
  int vright = (vn+1);
  if (vright>=vused) vright=0;

  MulVectorsScal(cp[vn].ev.v, C.nv, s); /*s=SGN(s-0.01f);*/
  if (s>=0) return;

  MulVectorsScal(cp[vleft ].ev.v, C.nv, s1); /* s1=SGN(s1+0.01f); */ //s1+=0.001f;
  MulVectorsScal(cp[vright].ev.v, C.nv, s2); /* s2=SGN(s2+0.01f); */ //s2+=0.001f;

  if (s1>0)
  {
    ClipRes+=1;

    /*
    CalcHitPoint(C,cp[vn].ev.v,
                   cp[vleft].ev.v, hleft.ev.v);

    float ll = VectorLength(SubVectors(cp[vleft].ev.v, cp[vn].ev.v));
    float lc = VectorLength(SubVectors(hleft.ev.v, cp[vn].ev.v));
    lc = lc / ll;
    */


    float lc = -s / (s1-s);
    hleft.ev.v.x = cp[vn].ev.v.x + ((cp[vleft].ev.v.x - cp[vn].ev.v.x) * lc);
    hleft.ev.v.y = cp[vn].ev.v.y + ((cp[vleft].ev.v.y - cp[vn].ev.v.y) * lc);
    hleft.ev.v.z = cp[vn].ev.v.z + ((cp[vleft].ev.v.z - cp[vn].ev.v.z) * lc);

    hleft.tx = cp[vn].tx + static_cast<int>(((cp[vleft].tx - cp[vn].tx) * lc));
    hleft.ty = cp[vn].ty + static_cast<int>(((cp[vleft].ty - cp[vn].ty) * lc));
    hleft.ev.Light = cp[vn].ev.Light + static_cast<int>(((cp[vleft].ev.Light - cp[vn].ev.Light) * lc));
  }

  if (s2>0)
  {
    ClipRes+=2;
    /*
    CalcHitPoint(C,cp[vn].ev.v,
                   cp[vright].ev.v, hright.ev.v);

    float ll = VectorLength(SubVectors(cp[vright].ev.v, cp[vn].ev.v));
    float lc = VectorLength(SubVectors(hright.ev.v, cp[vn].ev.v));
    lc = lc / ll;
     */

    float lc = -s / (s2-s);
    hright.ev.v.x = cp[vn].ev.v.x + ((cp[vright].ev.v.x - cp[vn].ev.v.x) * lc);
    hright.ev.v.y = cp[vn].ev.v.y + ((cp[vright].ev.v.y - cp[vn].ev.v.y) * lc);
    hright.ev.v.z = cp[vn].ev.v.z + ((cp[vright].ev.v.z - cp[vn].ev.v.z) * lc);

    hright.tx = cp[vn].tx + static_cast<int>(((cp[vright].tx - cp[vn].tx) * lc));
    hright.ty = cp[vn].ty + static_cast<int>(((cp[vright].ty - cp[vn].ty) * lc));
    hright.ev.Light = cp[vn].ev.Light + static_cast<int>(((cp[vright].ev.Light - cp[vn].ev.Light) * lc));
  }

  if (ClipRes == 0)
  {
    u--;
    vused--;
    cp[vn] = cp[vn+1];
    cp[vn+1] = cp[vn+2];
    cp[vn+2] = cp[vn+3];
    cp[vn+3] = cp[vn+4];
    cp[vn+4] = cp[vn+5];
    cp[vn+5] = cp[vn+6];
    //memcpy(&cp[vn], &cp[vn+1], (15-vn)*sizeof(ClipPoint));
  }
  if (ClipRes == 1)
  {
    cp[vn] = hleft;
  }
  if (ClipRes == 2)
  {
    cp[vn] = hright;
  }
  if (ClipRes == 3)
  {
    u++;
    vused++;
    //memcpy(&cp[vn+1], &cp[vn], (15-vn)*sizeof(ClipPoint));
    cp[vn+6] = cp[vn+5];
    cp[vn+5] = cp[vn+4];
    cp[vn+4] = cp[vn+3];
    cp[vn+3] = cp[vn+2];
    cp[vn+2] = cp[vn+1];
    cp[vn+1] = cp[vn];

    cp[vn] = hleft;
    cp[vn+1] = hright;
  }
}










int  SGNi(int f)
{
  if (f<0) return -1;
  else return  1;
}


void DrawTPlaneClip(BOOL SECONT)
{
  int n;

  if (!WATERREVERSE)
  {
    MulVectorsVect(SubVectors(ev[1].v, ev[0].v), SubVectors(ev[2].v, ev[0].v), nv);
    if (nv.x*ev[0].v.x  +  nv.y*ev[0].v.y  +  nv.z*ev[0].v.z<0) return;
  }

  cp[0].ev = ev[0];
  cp[1].ev = ev[1];
  cp[2].ev = ev[2];

  if (ReverseOn)
    if (SECONT)
    {
      switch (TDirection)
      {
      case 0:
        cp[0].tx = TCMIN;
        cp[0].ty = TCMAX;
        cp[1].tx = TCMAX;
        cp[1].ty = TCMIN;
        cp[2].tx = TCMAX;
        cp[2].ty = TCMAX;
        break;
      case 1:
        cp[0].tx = TCMAX;
        cp[0].ty = TCMAX;
        cp[1].tx = TCMIN;
        cp[1].ty = TCMIN;
        cp[2].tx = TCMAX;
        cp[2].ty = TCMIN;
        break;
      case 2:
        cp[0].tx = TCMAX;
        cp[0].ty = TCMIN;
        cp[1].tx = TCMIN;
        cp[1].ty = TCMAX;
        cp[2].tx = TCMIN;
        cp[2].ty = TCMIN;
        break;
      case 3:
        cp[0].tx = TCMIN;
        cp[0].ty = TCMIN;
        cp[1].tx = TCMAX;
        cp[1].ty = TCMAX;
        cp[2].tx = TCMIN;
        cp[2].ty = TCMAX;
        break;
      }
    }
    else
    {
      switch (TDirection)
      {
      case 0:
        cp[0].tx = TCMIN;
        cp[0].ty = TCMIN;
        cp[1].tx = TCMAX;
        cp[1].ty = TCMIN;
        cp[2].tx = TCMIN;
        cp[2].ty = TCMAX;
        break;
      case 1:
        cp[0].tx = TCMIN;
        cp[0].ty = TCMAX;
        cp[1].tx = TCMIN;
        cp[1].ty = TCMIN;
        cp[2].tx = TCMAX;
        cp[2].ty = TCMAX;
        break;
      case 2:
        cp[0].tx = TCMAX;
        cp[0].ty = TCMAX;
        cp[1].tx = TCMIN;
        cp[1].ty = TCMAX;
        cp[2].tx = TCMAX;
        cp[2].ty = TCMIN;
        break;
      case 3:
        cp[0].tx = TCMAX;
        cp[0].ty = TCMIN;
        cp[1].tx = TCMAX;
        cp[1].ty = TCMAX;
        cp[2].tx = TCMIN;
        cp[2].ty = TCMIN;
        break;
      }
    }
  else if (SECONT)
  {
    switch (TDirection)
    {
    case 0:
      cp[0].tx = TCMIN;
      cp[0].ty = TCMIN;
      cp[1].tx = TCMAX;
      cp[1].ty = TCMAX;
      cp[2].tx = TCMIN;
      cp[2].ty = TCMAX;
      break;
    case 1:
      cp[0].tx = TCMIN;
      cp[0].ty = TCMAX;
      cp[1].tx = TCMAX;
      cp[1].ty = TCMIN;
      cp[2].tx = TCMAX;
      cp[2].ty = TCMAX;
      break;
    case 2:
      cp[0].tx = TCMAX;
      cp[0].ty = TCMAX;
      cp[1].tx = TCMIN;
      cp[1].ty = TCMIN;
      cp[2].tx = TCMAX;
      cp[2].ty = TCMIN;
      break;
    case 3:
      cp[0].tx = TCMAX;
      cp[0].ty = TCMIN;
      cp[1].tx = TCMIN;
      cp[1].ty = TCMAX;
      cp[2].tx = TCMIN;
      cp[2].ty = TCMIN;
      break;
    }
  }
  else
  {
    switch (TDirection)
    {
    case 0:
      cp[0].tx = TCMIN;
      cp[0].ty = TCMIN;
      cp[1].tx = TCMAX;
      cp[1].ty = TCMIN;
      cp[2].tx = TCMAX;
      cp[2].ty = TCMAX;
      break;
    case 1:
      cp[0].tx = TCMIN;
      cp[0].ty = TCMAX;
      cp[1].tx = TCMIN;
      cp[1].ty = TCMIN;
      cp[2].tx = TCMAX;
      cp[2].ty = TCMIN;
      break;
    case 2:
      cp[0].tx = TCMAX;
      cp[0].ty = TCMAX;
      cp[1].tx = TCMIN;
      cp[1].ty = TCMAX;
      cp[2].tx = TCMIN;
      cp[2].ty = TCMIN;
      break;
    case 3:
      cp[0].tx = TCMAX;
      cp[0].ty = TCMIN;
      cp[1].tx = TCMAX;
      cp[1].ty = TCMAX;
      cp[2].tx = TCMIN;
      cp[2].ty = TCMAX;
      break;
    }
  }

  /*
     cp[0].tx = TCMIN;   cp[0].ty = TCMIN;

     if (SECONT) {
      cp[1].tx = TCMAX;   cp[1].ty = TCMAX;
  	cp[2].tx = TCMIN;   cp[2].ty = TCMAX;
     } else {
      cp[1].tx = TCMAX;   cp[1].ty = TCMIN;
      cp[2].tx = TCMAX;   cp[2].ty = TCMAX;
     }*/

  vused = 3;

  for (u=0; u<vused; u++) cp[u].ev.v.z+=12.0f;
  for (u=0; u<vused; u++) ClipVector(ClipZ,u);
  for (u=0; u<vused; u++) cp[u].ev.v.z-=12.0f;
  if (vused<3) return;

  for (u=0; u<vused; u++) ClipVector(ClipA,u);
  if (vused<3) return;
  for (u=0; u<vused; u++) ClipVector(ClipB,u);
  if (vused<3) return;
  for (u=0; u<vused; u++) ClipVector(ClipC,u);
  if (vused<3) return;
  for (u=0; u<vused; u++) ClipVector(ClipD,u);
  if (vused<3) return;

  //float dy = -1.1f;

  //if (WATERREVERSE) dy = 0;
  for (u=0; u<vused; u++)
  {
    cp[u].ev.scrx = VideoCX - static_cast<int>((cp[u].ev.v.x / cp[u].ev.v.z * CameraW));
    cp[u].ev.scry = VideoCY + static_cast<int>((cp[u].ev.v.y / cp[u].ev.v.z * CameraH));
  }



  scrp[0].x     = cp[0].ev.scrx;
  scrp[0].y     = cp[0].ev.scry;
  scrp[0].Light = cp[0].ev.Light/4;
  scrp[0].tx    = cp[0].tx;
  scrp[0].ty    = cp[0].ty;
  scrp[0].z     = static_cast<int>(cp[0].ev.v.z);

  for (u=0; u<vused-2; u++)
  {
    for (n=1; n<3; n++)
    {
      scrp[n].x     = cp[n+u].ev.scrx;
      scrp[n].y     = cp[n+u].ev.scry;
      scrp[n].Light = cp[n+u].ev.Light/4;
      scrp[n].tx    = cp[n+u].tx;
      scrp[n].ty    = cp[n+u].ty;
      scrp[n].z     = static_cast<int>(cp[n+u].ev.v.z);
    }
    if (CORRECTION) DrawCorrectedTexturedFace();
    else DrawTexturedFace();
  }
}



void DrawTPlane(BOOL SECONT)
{
  int n;

  if (!WATERREVERSE)
    if ((ev[1].scrx-ev[0].scrx)*(ev[2].scry-ev[0].scry) -
        (ev[1].scry-ev[0].scry)*(ev[2].scrx-ev[0].scrx) < 0) return;

  Mask1=0x007F;
  for (n=0; n<3; n++)
  {
    if (ev[n].DFlags & 128) return;
    Mask1=Mask1 & ev[n].DFlags;
  }
  if (Mask1>0) return;

  for (n=0; n<3; n++)
  {
    scrp[n].x = ev[n].scrx;
    scrp[n].y = ev[n].scry;
    scrp[n].Light = ev[n].Light / 4;
  }

  if (ReverseOn)
    if (SECONT)
    {
      switch (TDirection)
      {
      case 0:
        scrp[0].tx = TCMIN;
        scrp[0].ty = TCMAX;
        scrp[1].tx = TCMAX;
        scrp[1].ty = TCMIN;
        scrp[2].tx = TCMAX;
        scrp[2].ty = TCMAX;
        break;
      case 1:
        scrp[0].tx = TCMAX;
        scrp[0].ty = TCMAX;
        scrp[1].tx = TCMIN;
        scrp[1].ty = TCMIN;
        scrp[2].tx = TCMAX;
        scrp[2].ty = TCMIN;
        break;
      case 2:
        scrp[0].tx = TCMAX;
        scrp[0].ty = TCMIN;
        scrp[1].tx = TCMIN;
        scrp[1].ty = TCMAX;
        scrp[2].tx = TCMIN;
        scrp[2].ty = TCMIN;
        break;
      case 3:
        scrp[0].tx = TCMIN;
        scrp[0].ty = TCMIN;
        scrp[1].tx = TCMAX;
        scrp[1].ty = TCMAX;
        scrp[2].tx = TCMIN;
        scrp[2].ty = TCMAX;
        break;
      }
    }
    else
    {
      switch (TDirection)
      {
      case 0:
        scrp[0].tx = TCMIN;
        scrp[0].ty = TCMIN;
        scrp[1].tx = TCMAX;
        scrp[1].ty = TCMIN;
        scrp[2].tx = TCMIN;
        scrp[2].ty = TCMAX;
        break;
      case 1:
        scrp[0].tx = TCMIN;
        scrp[0].ty = TCMAX;
        scrp[1].tx = TCMIN;
        scrp[1].ty = TCMIN;
        scrp[2].tx = TCMAX;
        scrp[2].ty = TCMAX;
        break;
      case 2:
        scrp[0].tx = TCMAX;
        scrp[0].ty = TCMAX;
        scrp[1].tx = TCMIN;
        scrp[1].ty = TCMAX;
        scrp[2].tx = TCMAX;
        scrp[2].ty = TCMIN;
        break;
      case 3:
        scrp[0].tx = TCMAX;
        scrp[0].ty = TCMIN;
        scrp[1].tx = TCMAX;
        scrp[1].ty = TCMAX;
        scrp[2].tx = TCMIN;
        scrp[2].ty = TCMIN;
        break;
      }
    }
  else if (SECONT)
  {
    switch (TDirection)
    {
    case 0:
      scrp[0].tx = TCMIN;
      scrp[0].ty = TCMIN;
      scrp[1].tx = TCMAX;
      scrp[1].ty = TCMAX;
      scrp[2].tx = TCMIN;
      scrp[2].ty = TCMAX;
      break;
    case 1:
      scrp[0].tx = TCMIN;
      scrp[0].ty = TCMAX;
      scrp[1].tx = TCMAX;
      scrp[1].ty = TCMIN;
      scrp[2].tx = TCMAX;
      scrp[2].ty = TCMAX;
      break;
    case 2:
      scrp[0].tx = TCMAX;
      scrp[0].ty = TCMAX;
      scrp[1].tx = TCMIN;
      scrp[1].ty = TCMIN;
      scrp[2].tx = TCMAX;
      scrp[2].ty = TCMIN;
      break;
    case 3:
      scrp[0].tx = TCMAX;
      scrp[0].ty = TCMIN;
      scrp[1].tx = TCMIN;
      scrp[1].ty = TCMAX;
      scrp[2].tx = TCMIN;
      scrp[2].ty = TCMIN;
      break;
    }
  }
  else
  {
    switch (TDirection)
    {
    case 0:
      scrp[0].tx = TCMIN;
      scrp[0].ty = TCMIN;
      scrp[1].tx = TCMAX;
      scrp[1].ty = TCMIN;
      scrp[2].tx = TCMAX;
      scrp[2].ty = TCMAX;
      break;
    case 1:
      scrp[0].tx = TCMIN;
      scrp[0].ty = TCMAX;
      scrp[1].tx = TCMIN;
      scrp[1].ty = TCMIN;
      scrp[2].tx = TCMAX;
      scrp[2].ty = TCMIN;
      break;
    case 2:
      scrp[0].tx = TCMAX;
      scrp[0].ty = TCMAX;
      scrp[1].tx = TCMIN;
      scrp[1].ty = TCMAX;
      scrp[2].tx = TCMIN;
      scrp[2].ty = TCMIN;
      break;
    case 3:
      scrp[0].tx = TCMAX;
      scrp[0].ty = TCMIN;
      scrp[1].tx = TCMAX;
      scrp[1].ty = TCMAX;
      scrp[2].tx = TCMIN;
      scrp[2].ty = TCMAX;
      break;
    }
  }

  DrawTexturedFace();
}








#endif // _soft
