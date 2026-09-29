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

  // Sky cloud-texture placement, selected by config.cfg "sky_mode" (see
  // Core/SkyModeMapping.h, which both renderers share):
  //   0 = legacy camera-coupled pitch offset. The offset is applied after
  //       yaw, so the plane's tilt direction followed the camera and the
  //       cloud rows leaned by up to the full offset while turning.
  //   1 = world-level projected plane. Level with the world (no
  //       camera-coupled tilt); sky_horizon_drop lowers the compression
  //       singularity to a fixed elevation below the true horizon -- the C1
  //       offset's effect, but as a world-elevation shift, so the canopy
  //       never leans while turning.
  //   2 = dome sampling. OpenGL only; the software renderer falls back to
  //       the legacy plane and warns, so config.cfg never claims an effect
  //       this renderer does not have.
  //
  // The plane basis below is ALWAYS built from the legacy pitch, in every
  // mode, because the inherited sky fog is a scanline-width proxy: its
  // coefficients are the fog basis, not the texture basis. Changing sky_mode
  // must not change atmospheric density. Only the texture coordinate is
  // mode-dependent, so mode 1 derives it from the world view ray while the
  // fog continues to use the legacy fxa/fya computed here.
  //
  // The software renderer keeps its own frozen 0.15 rad offset rather than
  // the 0.20..0.10 altitude ramp OpenGL uses. With no mip chain, the steeper
  // ground-level tilt resolves into hard cloud banding instead of a soft
  // gradient. See ComputeSoftwareLegacySkyPitch.
  const int skyMode = skyl::ResolveSoftwareSkyMode(OptSkyMode);

  const float legacyPitch = skyl::ComputeSoftwareLegacySkyPitch(CameraBeta);
  cb = static_cast<float>(cos(legacyPitch));
  sb = static_cast<float>(sin(legacyPitch));

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

  // Mode 1 needs the world view ray at both ends of every scanline, plus the
  // world-anchored texture phase. The legacy mode needs none of this.
  const skyl::CameraBasis skyBasis = (skyMode == skyl::kModeLevel)
      ? skyl::ComputeCameraBasis(CameraAlpha, CameraBeta)
      : skyl::CameraBasis{};
  const float planeScale = skyl::kPlaneTexelScale;
  const float anchorX = skyl::kPlaneAnchorScale * CameraX;
  const float anchorY = -skyl::kPlaneAnchorScale * CameraZ;
  const float dropSin = skyl::HorizonDropSine(
      (OptSkyHorizonDrop >= kSkyHorizonDropMin && OptSkyHorizonDrop <= kSkyHorizonDropMax)
          ? OptSkyHorizonDrop : kSkyHorizonDropDefault);
  // The level-plane mapping diverges at the horizon, where the texture
  // coordinate runs away and the linear per-pixel step below turns the sky
  // into noise. The software sky has no mip chain to resolve it (the scanline
  // routines read a single 8-bit texel index per pixel), so the step is
  // bounded instead. This trades a stretched, stable band for aliasing noise
  // right at the singular ring, which terrain usually hides.
  const int kMaxSkyStep = 1 << 20;

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

    int dt = static_cast<int>((sqrt( (fxb-fxa)*(fxb-fxa) + (fyb-fya)*(fyb-fya) ) / 0x600000 )) - 7;
    if (dt>8) dt = 8;
    if (dt<lastdt) dt = lastdt;
    lastdt = dt;

    float u0, v0, u1, v1;
    if (skyMode == skyl::kModeLevel)
    {
      // World-level plane: sample from the world view ray, anchored to world
      // X/Z so the canopy stays put while the camera turns.
      float texU0, texV0, texU1, texV1;
      const Vector3d rayL = skyl::ComputeWorldRay(static_cast<float>(sx1), static_cast<float>(sy), CameraW, CameraH, skyBasis);
      const Vector3d rayR = skyl::ComputeWorldRay(static_cast<float>(sx2), static_cast<float>(sy), CameraW, CameraH, skyBasis);
      skyl::ComputeLevelPlaneTexel(rayL, planeScale, dropSin, anchorX, anchorY, &texU0, &texV0);
      skyl::ComputeLevelPlaneTexel(rayR, planeScale, dropSin, anchorX, anchorY, &texU1, &texV1);
      // SKYDTime scrolls U forward and V backward, as in the legacy path.
      u0 = texU0 + SKYDTime;
      v0 = texV0 - SKYDTime;
      u1 = texU1 + SKYDTime;
      v1 = texV1 - SKYDTime;
    }
    else
    {
      u0 = fxa; v0 = fya; u1 = fxb; v1 = fyb;
    }

    txa = (static_cast<int>(u0)) & ((256<<16) - 1);
    tya = (static_cast<int>(v0)) & ((256<<16) - 1);

    ctdx = static_cast<int>((u1-u0));
    ctdy = static_cast<int>((v1-v0));

    if (ctdx > kMaxSkyStep) ctdx = kMaxSkyStep;
    else if (ctdx < -kMaxSkyStep) ctdx = -kMaxSkyStep;
    if (ctdy > kMaxSkyStep) ctdy = kMaxSkyStep;
    else if (ctdy < -kMaxSkyStep) ctdy = -kMaxSkyStep;

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
