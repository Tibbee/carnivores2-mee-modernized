// ==========================================================================
// Effects.cpp
// ==========================================================================

#include "Hunt.h"
#include <cmath>

void PreCashGroundModel()
{
  SKYDTime = RealTime>>1;
  int x,y;

  int kx = SKYDTime & 255;
  int ky = SKYDTime & 255;
  int SKYDT = SKYDTime>>8;

  int VideoCX16 = VideoCX * 16;
  int VideoCY16 = VideoCY * 16;
  float CameraW16 = CameraW * 16;
  float CameraH16 = CameraH * 16;

  BOOL FogFound = false;
  NeedWater = false;

  static float waveCache[32][32];
  static int waveCacheRandomMap[32][32] = {};
  static int waveCacheTime = 0;
  static bool waveCacheReady = false;

  // §3.6: Multi-component wave cache for water surface displacement
  struct WaveCacheEntry { float wx[3], wy[3], wz[3]; };
  static WaveCacheEntry waveCacheSurface[32][32];
  static int waveCacheSurfaceTime = 0;
  static bool waveCacheSurfaceReady = false;

  if (!waveCacheReady || waveCacheTime != RealTime) {
    waveCacheTime = RealTime;
    for (int wy = 0; wy < 32; wy++) {
      for (int wx = 0; wx < 32; wx++) {
        waveCacheRandomMap[wy][wx] = RandomMap[wy][wx];
        waveCache[wy][wx] = static_cast<float>(sin(-pi/2 + waveCacheRandomMap[wy][wx] / 128 + static_cast<float>(RealTime) / 200.f));
      }
    }
    waveCacheReady = true;
  } else {
    for (int wy = 0; wy < 32; wy++) {
      for (int wx = 0; wx < 32; wx++) {
        if (waveCacheRandomMap[wy][wx] != RandomMap[wy][wx]) {
          waveCacheRandomMap[wy][wx] = RandomMap[wy][wx];
          waveCache[wy][wx] = static_cast<float>(sin(-pi/2 + waveCacheRandomMap[wy][wx] / 128 + static_cast<float>(RealTime) / 200.f));
        }
      }
    }
  }

  // §3.6: Pre-compute multi-component wave offsets for water surface
  if (!waveCacheSurfaceReady || waveCacheSurfaceTime != RealTime) {
    waveCacheSurfaceTime = RealTime;
    float t = RealTime / 200.f;
    for (int wy = 0; wy < 32; wy++) {
      for (int wx = 0; wx < 32; wx++) {
        int r1 = RandomMap[wy][wx];
        int r2 = RandomMap[(wy + 11) & 31][(wx + 7) & 31];
        float px = static_cast<float>(wx) * 0.5f;
        float py = static_cast<float>(wy) * 0.5f;
        // Wave 1: primary swell — large slow heave
        waveCacheSurface[wy][wx].wx[0] = static_cast<float>(sin(px + py + t)) * 18.f;
        waveCacheSurface[wy][wx].wy[0] = static_cast<float>(sin(px * 0.8f + py * 0.6f + t * 0.9f)) * 14.f;
        waveCacheSurface[wy][wx].wz[0] = static_cast<float>(sin(pi/2.f + px + py + t)) * 18.f;
        // Wave 2: secondary cross-wave — medium amplitude, different direction
        waveCacheSurface[wy][wx].wx[1] = static_cast<float>(sin(px * 1.5f - py * 0.7f + t * 1.3f + r1 * 0.01f)) * 10.f;
        waveCacheSurface[wy][wx].wy[1] = static_cast<float>(sin(px * 1.2f - py * 0.9f + t * 1.1f + r1 * 0.01f)) * 8.f;
        waveCacheSurface[wy][wx].wz[1] = static_cast<float>(sin(pi/3.f + px * 1.5f - py * 0.7f + t * 1.3f + r2 * 0.01f)) * 10.f;
        // Wave 3: fine detail — small fast ripples
        waveCacheSurface[wy][wx].wx[2] = static_cast<float>(sin(px * 2.3f + py * 1.2f + t * 2.1f + r2 * 0.01f)) * 5.f;
        waveCacheSurface[wy][wx].wy[2] = static_cast<float>(sin(px * 2.0f + py * 1.5f + t * 1.8f + r2 * 0.01f)) * 4.f;
        waveCacheSurface[wy][wx].wz[2] = static_cast<float>(sin(pi/4.f + px * 2.3f + py * 1.2f + t * 2.1f + r1 * 0.01f)) * 5.f;
      }
    }
    waveCacheSurfaceReady = true;
  }

  MapMinY = 10241024;
  Vector3d rv;


  for (y=-(ctViewR+3); y<(ctViewR+3); y++)
    for (x=-(ctViewR+3); x<(ctViewR+3); x++)
    {

      int r = MAX((MAX(y,-y)), (MAX(x,-x)));

      int xx = (CCX + x) & 1023;
      int yy = (CCY + y) & 1023;

      v[0].x = xx*256 - CameraX;
      v[0].z = yy*256 - CameraZ;
      v[0].y = static_cast<float>((static_cast<int>(HMap[yy][xx])))*ctHScale - CameraY;


//========= water section ===========//

      //if (RunMode)
      if ((FMap[yy][xx] & fmWaterA)>0)
      {
        rv = v[0];
        rv.y = WaterList[ WMap[yy][xx] ].wlevel*ctHScale - CameraY;

        // Use the per-RealTime wave offset cache built at the top of
        // PreCashGroundModel(). The cache reduces ~2,600 sin() calls per
        // frame to 1,024 per RealTime change (and zero in steady state).
        // The 4ab70c8 commit introduced the cache but never wired the
        // lookup; this change completes that fix.
        float wdelta = waveCache[yy & 31][xx & 31];

        if ( (FMap[yy][xx] & fmWater) && (r < ctViewR-4))
        {
          // §3.6: Multi-component wave displacement from cache
          const WaveCacheEntry& wc = waveCacheSurface[yy & 31][xx & 31];
          rv.x += wc.wx[0] + wc.wx[1] + wc.wx[2];
          rv.y += wc.wy[0] + wc.wy[1] + wc.wy[2];
          rv.z += wc.wz[0] + wc.wz[1] + wc.wz[2];
        }

        rv = RotateVector(rv);
        VMap2[kViewGridCenter + y][kViewGridCenter + x].v = rv;

        if (fabs(rv.x) > -rv.z + 1524)
        {
#if !defined(_gl)
          VMap2[kViewGridCenter + y][kViewGridCenter + x].DFlags = 128;
#endif
        }
        else
        {
          NeedWater = true;
          VMap2[kViewGridCenter + y][kViewGridCenter + x].Light = 168-static_cast<int>((wdelta*24));

          float Alpha;
          if (IsUnderwater())
          {
            Alpha =	160 - VectorLength(rv)* 160 / 220 / ctViewR;
            if (Alpha<10) Alpha=10;
          }
          else if (r < ctViewR+2)
          {
            int wi = WMap[yy][xx];
            Alpha = static_cast<float>(((WaterList[wi].wlevel - HMap[yy][xx])*2+4))*WaterList[wi].transp;
            Alpha+=VectorLength(rv) / 256;
            Alpha+=wdelta*2;
            if (Alpha<0) Alpha=0;
            Vector3d va = v[0];
            NormVector(va,1.0f);
            va.y=-va.y;
            if (va.y<0) va.y=0;
            Alpha*=6.f/(va.y+0.1f);
            if (Alpha>255) Alpha=255.f;
          }
          else Alpha = 255.f;

          VMap2[kViewGridCenter + y][kViewGridCenter + x].ALPHA=static_cast<int>(Alpha);

          // Water surface fog: apply depth-based fog when underwater
          // so the surface fades out at depth (matching terrain fog behavior).
          if (IsUnderwater()) {
              float extinction = 1.0f - std::exp(-CameraWaterDepthFactor * 3.5f);
              float fogAmount = extinction * 200.0f;
              VMap2[kViewGridCenter + y][kViewGridCenter + x].Fog = static_cast<int>(fogAmount);
          } else {
              VMap2[kViewGridCenter + y][kViewGridCenter + x].Fog = 0;
          }

#if !defined(_gl)
          if (rv.z>-256.0) VMap2[kViewGridCenter + y][kViewGridCenter + x].DFlags=128;
          else
          {
#ifdef _soft
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX - static_cast<int>((rv.x / rv.z * CameraW));
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY + static_cast<int>((rv.y / rv.z * CameraH));

            int DF = 0;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)     DF+=1;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX) DF+=2;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry < 0)     DF+=4;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY) DF+=8;
#else
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX16 - static_cast<int>((rv.x / rv.z * CameraW16));
            VMap2[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY16 + static_cast<int>((rv.y / rv.z * CameraH16));

            int DF = 0;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)        DF+=1;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX*16) DF+=2;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry < 0)        DF+=4;
            if (VMap2[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY*16) DF+=8;
#endif
            VMap2[kViewGridCenter + y][kViewGridCenter + x].DFlags = DF;

          }
#endif
        }
      }



#ifdef _soft
#else
#endif

      rv = RotateVector(v[0]);


      if (fabs(rv.x * FOVK) > -rv.z + 1600)
      {
        VMap[kViewGridCenter + y][kViewGridCenter + x].v = rv;
#if !defined(_gl)
        VMap[kViewGridCenter + y][kViewGridCenter + x].DFlags = 128;
#endif
        continue;
      }


      if (HARD3D)
        if (  ((FMap[yy][xx] & fmWater)==0) || IsUnderwater())
          VMap[kViewGridCenter + y][kViewGridCenter + x].Fog = CalcFogLevel(v[0]);
        else
          VMap[kViewGridCenter + y][kViewGridCenter + x].Fog = 0;

#if !defined(_gl)
      VMap[kViewGridCenter + y][kViewGridCenter + x].ALPHA = 255;
#endif

      v[0]=rv;

      if (v[0].z<1024)
        if (FOGENABLE)
          if (FogsMap[yy>>1][xx>>1]) FogFound = true;

      VMap[kViewGridCenter + y][kViewGridCenter + x].v = v[0];

      int  DF = 0;
      int  db = 0;

      if (v[0].z<256)
      {
        if (Clouds)
        {
          int shmx = (xx + SKYDT) & 127;
          int shmy = (yy + SKYDT) & 127;

          int db1 = SkyMap[shmy * 128 + shmx ];
          int db2 = SkyMap[shmy * 128 + ((shmx+1) & 127) ];
          int db3 = SkyMap[((shmy+1) & 127) * 128 + shmx ];
          int db4 = SkyMap[((shmy+1) & 127) * 128 + ((shmx+1) & 127) ];
          db = (db1 * (256 - kx) + db2 * kx) * (256-ky) +
               (db3 * (256 - kx) + db4 * kx) * ky;
          db>>=17;
          db = db - 40;
          if (db<0) db=0;
          if (db>48) db=48;
        }

        int clt = LMap[yy][xx];
        clt= MAX(64, clt-db);
        VMap[kViewGridCenter + y][kViewGridCenter + x].Light = clt;
      }



#if !defined(_gl)
      if (v[0].z>-256.0) DF+=128;
      else
      {

#ifdef _soft
        VMap[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX - static_cast<int>((v[0].x / v[0].z * CameraW));
        VMap[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY + static_cast<int>((v[0].y / v[0].z * CameraH));

        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)        DF+=1;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX)    DF+=2;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry < 0)        DF+=4;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY)    DF+=8;
#else
        VMap[kViewGridCenter + y][kViewGridCenter + x].scrx = VideoCX16 - static_cast<int>((v[0].x / v[0].z * CameraW16));
        VMap[kViewGridCenter + y][kViewGridCenter + x].scry = VideoCY16 + static_cast<int>((v[0].y / v[0].z * CameraH16));

        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx < 0)        DF+=1;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scrx > WinEX*16) DF+=2;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry < 0)        DF+=4;
        if (VMap[kViewGridCenter + y][kViewGridCenter + x].scry > WinEY*16) DF+=8;
#endif

      }

      VMap[kViewGridCenter + y][kViewGridCenter + x].DFlags = DF;
#endif
    }

  FOGON = FogFound || IsUnderwater();
}

void AddShadowCircle(int x, int y, int R, int D)
{
  if (IsUnderwater()) return;

  int cx = x / 256;
  int cy = y / 256;
  int cr = 1 + R / 256;
  for (int yy=-cr; yy<=cr; yy++)
    for (int xx=-cr; xx<=cr; xx++)
    {
      int tx = (cx+xx)*256;
      int ty = (cy+yy)*256;
      int r = static_cast<int>(sqrt(static_cast<float>(((tx-x)*(tx-x) + (ty-y)*(ty-y))) ));
      if (r>R) continue;
      VMap[cy + yy - CCY + kViewGridCenter][cx + xx - CCX + kViewGridCenter].Light-= D * (R-r) / R;
      if (VMap[cy + yy - CCY + kViewGridCenter][cx + xx - CCX + kViewGridCenter].Light < 32)
        VMap[cy + yy - CCY + kViewGridCenter][cx + xx - CCX + kViewGridCenter].Light = 32;
    }
}