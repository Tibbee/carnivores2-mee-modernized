// ==========================================================================
// SoftSky.cpp -- Software renderer sky plane rendering
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft
void RotateVVector(Vector3d& v)
{
  float x = v.x * ca - v.z * sa;
  float y = v.y;
  float z = v.z * ca + v.x * sa;

  float xx = x;
  float xy = y * cb + z * sb;
  float xz = z * cb - y * sb;

  v.x = xx;
  v.y = xy;
  v.z = xz;
}



void RenderSkyPlane()
{
  ClearVideoBuf();
  Vector3d v,vbase;
  Vector3d tx,ty,nv;
  float p,q, qx, qy, qz, px, py, pz, rx, ry, rz, ddx, ddy;
  int lastdt = 0;

  cb = static_cast<float>(cos(CameraBeta));
  sb = static_cast<float>(sin(CameraBeta));
  SKYDTime = (RealTime*256) & ((256<<16) - 1);

  float sh = - CameraY;
  if (MapMinY==10241024) MapMinY=0;
  sh = static_cast<float>((static_cast<int>(MapMinY)))*ctHScale - CameraY;

  v.x = 0;
  v.z = (ctViewR*4.f)/5.f*256.f;
  v.y = sh;

  vbase.x = v.x;
  vbase.y = v.y * cb + v.z * sb;
  vbase.z = v.z * cb - v.y * sb;

  if (vbase.z < 128) vbase.z = 128;

  int scry = VideoCY - static_cast<int>((vbase.y / vbase.z * CameraH));

  if (scry<0) return;
  if (scry>WinEY) scry = WinEY;


  cb = static_cast<float>(cos(CameraBeta-0.15));
  sb = static_cast<float>(sin(CameraBeta-0.15));

  tx.x=0.004f;
  tx.y=0;
  tx.z=0;
  ty.x=0.0f;
  ty.y=0;
  ty.z=0.004f;
  nv.x=0;
  nv.y=-1.f;
  nv.z=0;

  tx.x*=0x10000;
  ty.z*=0x10000;

  RotateVVector(tx);
  RotateVVector(ty);
  RotateVVector(nv);

  sh = 4*512*16;
  vbase.x = -CameraX;
  vbase.y = sh;
  vbase.z = +CameraZ;
  RotateVVector(vbase);

//============= calc render params =================//
  p = nv.x * vbase.x + nv.y * vbase.y + nv.z * vbase.z;
  ddx = vbase.x * tx.x  +  vbase.y * tx.y  +  vbase.z * tx.z;
  ddy = vbase.x * ty.x  +  vbase.y * ty.y  +  vbase.z * ty.z;

  qx = CameraH * nv.x;
  qy = CameraW * nv.y;
  qz = CameraW*CameraH  * nv.z;
  px = p*CameraH*tx.x;
  py = p*CameraW*tx.y;
  pz = p*CameraW*CameraH* tx.z;
  rx = p*CameraH*ty.x;
  ry = p*CameraW*ty.y;
  rz = p*CameraW*CameraH* ty.z;

  px=px - ddx*qx;
  py=py - ddx*qy;
  pz=pz - ddx*qz;
  rx=rx - ddy*qx;
  ry=ry - ddy*qy;
  rz=rz - ddy*qz;

  int sx1 = - VideoCX;
  int sx2 = + VideoCX;

  float qx1 = qx * sx1 + qz;
  float qx2 = qx * sx2 + qz;
  float qyy;


  for (int sky=0; sky<=scry; sky++)
  {
    int sy = VideoCY - sky;
    qyy = qy * sy;

    q = qx1 + qyy;
    float fxa = (px * sx1 + py * sy + pz) / q;
    float fya = (rx * sx1 + ry * sy + rz) / q;

    q = qx2 + qyy;
    float fxb = (px * sx2 + py * sy + pz) / q;
    float fyb = (rx * sx2 + ry * sy + rz) / q;

    txa = (static_cast<int>(fxa) + SKYDTime) & ((256<<16) - 1);
    tya = (static_cast<int>(fya) - SKYDTime) & ((256<<16) - 1);

    ctdx = static_cast<int>((fxb-fxa));
    ctdy = static_cast<int>((fyb-fya));

    int dt = static_cast<int>((sqrt( (fxb-fxa)*(fxb-fxa) + (fyb-fya)*(fyb-fya) ) / 0x600000 )) - 7;
    if (dt>8) dt = 8;
    if (dt<lastdt) dt = lastdt;
    lastdt = dt;

    ctdx/=WinW;
    ctdy/=WinW;

    if (LoDetailSky)
      if (dt) RenderSkyLineFadeLo(sky, dt);
      else RenderSkyLineLo(sky);
    else if (dt) RenderSkyLineFade(sky,dt);
    else RenderSkyLine(sky);
  }
}












#endif // _soft
