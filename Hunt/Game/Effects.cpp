// ==========================================================================
// Effects.cpp
// ==========================================================================

#include "Hunt.h"

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
          rv.x+=static_cast<float>(sin(xx+yy + RealTime/200.f)) * 16.f;
          rv.z+=static_cast<float>(sin(pi/2.f + xx+yy + RealTime/200.f)) * 16.f;
        }

        rv = RotateVector(rv);
        VMap2[kViewGridCenter + y][kViewGridCenter + x].v = rv;

        if (fabs(rv.x) > -rv.z + 1524)
        {
          VMap2[kViewGridCenter + y][kViewGridCenter + x].DFlags = 128;
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
          VMap2[kViewGridCenter + y][kViewGridCenter + x].Fog = 0;

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
        }
      }



#ifdef _soft
#else
#endif

      rv = RotateVector(v[0]);


      if (fabs(rv.x * FOVK) > -rv.z + 1600)
      {
        VMap[kViewGridCenter + y][kViewGridCenter + x].v = rv;
        VMap[kViewGridCenter + y][kViewGridCenter + x].DFlags = 128;
        continue;
      }


      if (HARD3D)
        if (  ((FMap[yy][xx] & fmWater)==0) || IsUnderwater())
          VMap[kViewGridCenter + y][kViewGridCenter + x].Fog = CalcFogLevel(v[0]);
        else
          VMap[kViewGridCenter + y][kViewGridCenter + x].Fog = 0;

      VMap[kViewGridCenter + y][kViewGridCenter + x].ALPHA = 255;

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