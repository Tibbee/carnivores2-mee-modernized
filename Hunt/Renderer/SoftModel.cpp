// ==========================================================================
// SoftModel.cpp — Software renderer model rendering
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft
void BuildTree()
{
  Vector2di v[3];
  Current = -1;
  TFace* fptr;
  int sg;

  for (int f=0; f<mptr->FCount; f++)
  {
    fptr = &mptr->gFace[f];
    v[0] = gScrp[fptr->v1];
    v[1] = gScrp[fptr->v2];
    v[2] = gScrp[fptr->v3];

    if (v[0].x == 0xFFFFFF) continue;
    if (v[1].x == 0xFFFFFF) continue;
    if (v[2].x == 0xFFFFFF) continue;

    //fptr->Flags &= 0x00FF;

    if (fptr->Flags & (sfDarkBack + sfNeedVC) )
    {
      sg = (v[1].x-v[0].x)*(v[2].y-v[1].y) - (v[1].y-v[0].y)*(v[2].x-v[1].x);
      if (sg<0) continue;
      /*
                if (fptr->Flags & sfNeedVC) { if (sg<0) continue; }
                                       else if (sg<0) fptr->Flags |= sfDark; */
    }

    //if (NODARKBACK) fptr->Flags &= 0x00FF;

    fptr->Distant = static_cast<int>((-(rVertex[fptr->v1].z + rVertex[fptr->v2].z + rVertex[fptr->v3].z)));
    fptr->Next=-1;
    if (Current==-1) Current=f;
    else if (mptr->gFace[Current].Distant < fptr->Distant)
    {
      fptr->Next=Current;
      Current=f;
    }
    else
    {
      int n=Current;
      while (mptr->gFace[n].Next!=-1 && mptr->gFace[mptr->gFace[n].Next].Distant > fptr->Distant)
        n=mptr->gFace[n].Next;
      fptr->Next = mptr->gFace[n].Next;
      mptr->gFace[n].Next = f;
    }
  }
}

void BuildTreeNoSort()
{
  Vector2di v[3];
  Current = -1;
  int LastFace = -1;
  TFace* fptr;
  int sg;

  for (int f=0; f<mptr->FCount; f++)
  {
    fptr = &mptr->gFace[f];
    v[0] = gScrp[fptr->v1];
    v[1] = gScrp[fptr->v2];
    v[2] = gScrp[fptr->v3];

    if (v[0].x == 0xFFFFFF) continue;
    if (v[1].x == 0xFFFFFF) continue;
    if (v[2].x == 0xFFFFFF) continue;

    if (fptr->Flags & (sfDarkBack+sfNeedVC))
    {
      sg = (v[1].x-v[0].x)*(v[2].y-v[1].y) - (v[1].y-v[0].y)*(v[2].x-v[1].x);
      if (sg<0) continue;
    }

    fptr->Next=-1;
    if (Current==-1)
    {
      Current=f;
      LastFace = f;
    }
    else
    {
      mptr->gFace[LastFace].Next=f;
      LastFace=f;
    }

  }
}














void BuildTreeClip()
{
  Current = -1;
  TFace* fptr;

  for (int f=0; f<mptr->FCount; f++)
  {
    fptr = &mptr->gFace[f];

    if (fptr->Flags & (sfDarkBack + sfNeedVC) )
    {
      MulVectorsVect(SubVectors(rVertex[fptr->v2], rVertex[fptr->v1]), SubVectors(rVertex[fptr->v3], rVertex[fptr->v1]), nv);
      if (nv.x*rVertex[fptr->v1].x  +  nv.y*rVertex[fptr->v1].y  +  nv.z*rVertex[fptr->v1].z<0) continue;
    }

    fptr->Distant = static_cast<int>((-(rVertex[fptr->v1].z + rVertex[fptr->v2].z + rVertex[fptr->v3].z)));
    fptr->Next=-1;
    if (Current==-1) Current=f;
    else if (mptr->gFace[Current].Distant < fptr->Distant)
    {
      fptr->Next=Current;
      Current=f;
    }
    else
    {
      int n=Current;
      while (mptr->gFace[n].Next!=-1 && mptr->gFace[mptr->gFace[n].Next].Distant > fptr->Distant)
        n=mptr->gFace[n].Next;
      fptr->Next = mptr->gFace[n].Next;
      mptr->gFace[n].Next = f;
    }
  }
}



void BuildTreeClipNoSort()
{
  Current = -1;
  int LastFace = -1;
  TFace* fptr;

  for (int f=0; f<mptr->FCount; f++)
  {
    fptr = &mptr->gFace[f];

    if (fptr->Flags & (sfDarkBack + sfNeedVC) )
    {
      MulVectorsVect(SubVectors(rVertex[fptr->v2], rVertex[fptr->v1]), SubVectors(rVertex[fptr->v3], rVertex[fptr->v1]), nv);
      if (nv.x*rVertex[fptr->v1].x  +  nv.y*rVertex[fptr->v1].y  +  nv.z*rVertex[fptr->v1].z<0) continue;
    }

    fptr->Next=-1;
    if (Current==-1)
    {
      Current=f;
      LastFace = f;
    }
    else
    {
      mptr->gFace[LastFace].Next=f;
      LastFace=f;
    }

  }
}


















void RenderModelClip(TModel* _mptr, float x0, float y0, float z0, int light, int VT, float al, float bt)
{
  int f,CMASK;

  mptr = _mptr;

  float ca = static_cast<float>(cos(al));
  float sa = static_cast<float>(sin(al));

  float cb = static_cast<float>(cos(bt));
  float sb = static_cast<float>(sin(bt));



  __asm
  {
    mov eax,light
    shr eax,2
    shl eax,16
    add eax, offset FadeTab
    mov iModelFade,eax
    mov iModelBaseFade,eax
  }


  HLineT = (void*) HLineTxModel;
  lpTextureAddr = mptr->lpTexture.get();

  BOOL BL = false;
  for (int s=0; s<mptr->VCount; s++)
  {
    rVertex[s].x = (mptr->gVertex[s].x * ca + mptr->gVertex[s].z * sa)   + x0;
    float vz = mptr->gVertex[s].z * ca - mptr->gVertex[s].x * sa;
    rVertex[s].y = (mptr->gVertex[s].y * cb - vz * sb) + y0;
    rVertex[s].z = (vz * cb + mptr->gVertex[s].y * sb) + z0;
    if (rVertex[s].z<0) BL=true;

    if (rVertex[s].z>-64)
    {
      gScrp[s].x = 0xFFFFFF;
      gScrp[s].y = 0xFF;
    }
    else
    {
      int f = 0;
      int sx =  VideoCX + static_cast<int>((rVertex[s].x / (-rVertex[s].z) * CameraW));
      int sy =  VideoCY - static_cast<int>((rVertex[s].y / (-rVertex[s].z) * CameraH));

      if (sx<=0    ) f+=2;
      if (sx>=WinEX) f+=1;
      if (sy>=WinEY) f+=4;
      if (sy<=0    ) f+=8;


      if (f)
      {
        gScrp[s].x = 0xFFFFFF;
        gScrp[s].y = f;
      }
      else
      {
        gScrp[s].x = sx;
        gScrp[s].y = sy;
      }
    }

  }

  if (!BL) return;

  BuildTreeClip();

  f = Current;
  while( f!=-1 )
  {

    vused = 3;
    TFace *fptr = &mptr->gFace[f];


//======== if face fully on screen NO 3D CLIPPING ====================/
    if (!waterclip)
      if (gScrp[fptr->v1].x != 0xFFFFFF && gScrp[fptr->v2].x != 0xFFFFFF && gScrp[fptr->v3].x != 0xFFFFFF )
      {
        mscrp[0].x  = gScrp[fptr->v1].x;
        mscrp[0].y  = gScrp[fptr->v1].y;
        mscrp[0].tx = fptr->tax;
        mscrp[0].ty = fptr->tay;

        mscrp[1].x  = gScrp[fptr->v2].x;
        mscrp[1].y  = gScrp[fptr->v2].y;
        mscrp[1].tx = fptr->tbx;
        mscrp[1].ty = fptr->tby;

        mscrp[2].x  = gScrp[fptr->v3].x;
        mscrp[2].y  = gScrp[fptr->v3].y;
        mscrp[2].tx = fptr->tcx;
        mscrp[2].ty = fptr->tcy;

        OpacityMode = (fptr->Flags & (sfOpacity + sfTransparent));
        if (fptr->Flags & sfDark)
          iModelFade = iModelBaseFade + 12*256*256;
        else iModelFade = iModelBaseFade;

        DrawModelFace();
        goto LNEXT;
      }


    CMASK = 0;
    if (gScrp[fptr->v1].x == 0xFFFFFF) CMASK|=gScrp[fptr->v1].y;
    if (gScrp[fptr->v2].x == 0xFFFFFF) CMASK|=gScrp[fptr->v2].y;
    if (gScrp[fptr->v3].x == 0xFFFFFF) CMASK|=gScrp[fptr->v3].y;
//  CMASK = 0xFF;


    cp[0].ev.v = rVertex[fptr->v1];
    cp[0].tx = fptr->tax;
    cp[0].ty = fptr->tay;
    cp[1].ev.v = rVertex[fptr->v2];
    cp[1].tx = fptr->tbx;
    cp[1].ty = fptr->tby;
    cp[2].ev.v = rVertex[fptr->v3];
    cp[2].tx = fptr->tcx;
    cp[2].ty = fptr->tcy;

    if (CMASK == 0xFF)
    {
      for (u=0; u<vused; u++) cp[u].ev.v.z+=12.0f;
      for (u=0; u<vused; u++) ClipVector(ClipZ,u);
      for (u=0; u<vused; u++) cp[u].ev.v.z-=12.0f;
      if (vused<3) goto LNEXT;
    }

    if (waterclip)
    {
      for (u=0; u<vused; u++)
      {
        cp[u].ev.v.x-=waterclipbase.x;
        cp[u].ev.v.y-=waterclipbase.y;
        cp[u].ev.v.z-=waterclipbase.z;
      }
      for (u=0; u<vused; u++) ClipVector(ClipW,u);
      for (u=0; u<vused; u++)
      {
        cp[u].ev.v.x+=waterclipbase.x;
        cp[u].ev.v.y+=waterclipbase.y;
        cp[u].ev.v.z+=waterclipbase.z;
      }
      if (vused<3) goto LNEXT;
    }

    if (CMASK & 1) for (u=0; u<vused; u++) ClipVector(ClipA,u);
    if (vused<3) goto LNEXT;
    if (CMASK & 2) for (u=0; u<vused; u++) ClipVector(ClipC,u);
    if (vused<3) goto LNEXT;

    if (CMASK & 4) for (u=0; u<vused; u++) ClipVector(ClipB,u);
    if (vused<3) goto LNEXT;
    if (CMASK & 8) for (u=0; u<vused; u++) ClipVector(ClipD,u);
    if (vused<3) goto LNEXT;

    for (u=0; u<vused; u++)
    {
      cp[u].ev.scrx = VideoCX - static_cast<int>((cp[u].ev.v.x / cp[u].ev.v.z * CameraW));
      cp[u].ev.scry = VideoCY + static_cast<int>((cp[u].ev.v.y / cp[u].ev.v.z * CameraH));
    }

    mscrp[0].x     = cp[0].ev.scrx;
    mscrp[0].y     = cp[0].ev.scry;
    mscrp[0].tx    = cp[0].tx;
    mscrp[0].ty    = cp[0].ty;

    OpacityMode = (fptr->Flags & (sfOpacity + sfTransparent));

    for (u=0; u<vused-2; u++)
    {
      for (int n=1; n<3; n++)
      {
        mscrp[n].x     = cp[n+u].ev.scrx;
        mscrp[n].y     = cp[n+u].ev.scry;
        mscrp[n].tx    = cp[n+u].tx;
        mscrp[n].ty    = cp[n+u].ty;
      }

      DrawModelFace();
    }
LNEXT:
    f = mptr->gFace[f].Next;
  }
}





void RenderModelClipWater(TModel* _mptr, float x0, float y0, float z0, int light, int VT, float al, float bt)
{
  int f;

  mptr = _mptr;

  float ca = static_cast<float>(cos(al));
  float sa = static_cast<float>(sin(al));

  float cb = static_cast<float>(cos(bt));
  float sb = static_cast<float>(sin(bt));



  __asm
  {
    mov eax,light
    shr eax,2
    shl eax,16
    add eax, offset FadeTab
    mov iModelFade,eax
    mov iModelBaseFade,eax
  }


//=================== select mipmap & glass =============================//

  if (!GlassL) HLineT = (void*) HLineTxModel;
  else if (GlassL>160) HLineT = (void*) HLineTxModel25;
  else if (GlassL>80 ) HLineT = (void*) HLineTxModel50;
  else
    HLineT = (void*) HLineTxModel75;

  lpTextureAddr = mptr->lpTexture.get();

  if (GlassL) lpTextureAddr = mptr->lpTexture3.get();
  else if (ts <=64)
    if (ts > 32)
    {
      lpTextureAddr = mptr->lpTexture2.get();
      HLineT = (void*) HLineTxModel2;
    }
    else
    {
      lpTextureAddr = mptr->lpTexture3.get();
      HLineT = (void*) HLineTxModel3;
    }



  BOOL BL = false;
  float sg;
  for (int s=0; s<mptr->VCount; s++)
  {
    rVertex[s].x = (mptr->gVertex[s].x * ca + mptr->gVertex[s].z * sa)   + x0;
    float vz = mptr->gVertex[s].z * ca - mptr->gVertex[s].x * sa;
    rVertex[s].y = (mptr->gVertex[s].y * cb - vz * sb) + y0;
    rVertex[s].z = (vz * cb + mptr->gVertex[s].y * sb) + z0;

    v[0] = SubVectors(rVertex[s], waterclipbase);
    MulVectorsScal(v[0], ClipW.nv, sg);
    if (sg>=0) gScrp[s].x = 0;
    else gScrp[s].x=1;

    if (rVertex[s].z<0) BL=true;
  }

  if (!BL) return;

  if (fabs(z0) + fabs(x0)>256*10)
    BuildTreeClipNoSort();
  else BuildTreeClip();


  f = Current;
  while( f!=-1 )
  {

    vused = 3;
    TFace *fptr = &mptr->gFace[f];

    if (rVertex[fptr->v1].z > -128) goto LNEXT;
    if (rVertex[fptr->v2].z > -128) goto LNEXT;
    if (rVertex[fptr->v3].z > -128) goto LNEXT;

    if (gScrp[fptr->v1].x & gScrp[fptr->v2].x & gScrp[fptr->v3].x)  goto LNEXT;

    cp[0].ev.v = rVertex[fptr->v1];
    cp[0].tx = fptr->tax;
    cp[0].ty = fptr->tay;
    cp[1].ev.v = rVertex[fptr->v2];
    cp[1].tx = fptr->tbx;
    cp[1].ty = fptr->tby;
    cp[2].ev.v = rVertex[fptr->v3];
    cp[2].tx = fptr->tcx;
    cp[2].ty = fptr->tcy;

    if (!(gScrp[fptr->v1].x | gScrp[fptr->v2].x | gScrp[fptr->v3].x))  goto LNOCLIP;

    for (u=0; u<vused; u++)
    {
      cp[u].ev.v.x-=waterclipbase.x;
      cp[u].ev.v.y-=waterclipbase.y;
      cp[u].ev.v.z-=waterclipbase.z;
    }
    for (u=0; u<vused; u++) ClipVector(ClipW,u);
    for (u=0; u<vused; u++)
    {
      cp[u].ev.v.x+=waterclipbase.x;
      cp[u].ev.v.y+=waterclipbase.y;
      cp[u].ev.v.z+=waterclipbase.z;
    }
    if (vused<3) goto LNEXT;

LNOCLIP:
    for (u=0; u<vused; u++)
    {
      cp[u].ev.scrx = VideoCX - static_cast<int>((cp[u].ev.v.x / cp[u].ev.v.z * CameraW));
      cp[u].ev.scry = VideoCY + static_cast<int>((cp[u].ev.v.y / cp[u].ev.v.z * CameraH));
    }

    mscrp[0].x     = cp[0].ev.scrx;
    mscrp[0].y     = cp[0].ev.scry;
    mscrp[0].tx    = cp[0].tx;
    mscrp[0].ty    = cp[0].ty;

    OpacityMode = (fptr->Flags & (sfOpacity + sfTransparent));

    for (u=0; u<vused-2; u++)
    {
      for (int n=1; n<3; n++)
      {
        mscrp[n].x     = cp[n+u].ev.scrx;
        mscrp[n].y     = cp[n+u].ev.scry;
        mscrp[n].tx    = cp[n+u].tx;
        mscrp[n].ty    = cp[n+u].ty;
      }

      DrawModelFace();
    }
LNEXT:
    f = mptr->gFace[f].Next;
  }
}










void RenderModel(TModel* _mptr, float x0, float y0, float z0, int light, int VT, float al, float bt)
{
  int f;

  mptr = _mptr;

  float ca = static_cast<float>(cos(al));
  float sa = static_cast<float>(sin(al));

  float cb = static_cast<float>(cos(bt));
  float sb = static_cast<float>(sin(bt));

  int minx = 10241024;
  int maxx =-10241024;
  int miny = 10241024;
  int maxy =-10241024;



  __asm
  {
    mov eax,light
    shr eax,2
    shl eax,16
    add eax, offset FadeTab
    mov iModelFade,eax
    mov iModelBaseFade,eax
  }

  if (!GlassL) HLineT = (void*) HLineTxModel;
  else if (GlassL>160) HLineT = (void*) HLineTxModel25;
  else if (GlassL>80 ) HLineT = (void*) HLineTxModel50;
  else
    HLineT = (void*) HLineTxModel75;

  lpTextureAddr = mptr->lpTexture.get();

  if (GlassL) lpTextureAddr = mptr->lpTexture3.get();
  else if (ts <=64)
    if (ts > 32)
    {
      lpTextureAddr = mptr->lpTexture2.get();
      HLineT = (void*) HLineTxModel2;
    }
    else
    {
      lpTextureAddr = mptr->lpTexture3.get();
      HLineT = (void*) HLineTxModel3;
    }




  for (int s=0; s<mptr->VCount; s++)
  {
    rVertex[s].x = (mptr->gVertex[s].x * ca + mptr->gVertex[s].z * sa)   + x0;

    float vz = mptr->gVertex[s].z * ca - mptr->gVertex[s].x * sa;

    rVertex[s].y = (mptr->gVertex[s].y * cb - vz * sb) + y0;
    rVertex[s].z = (vz * cb + mptr->gVertex[s].y * sb) + z0;

    if (rVertex[s].z>-64) gScrp[s].x = 0xFFFFFF;
    else
    {
      gScrp[s].x = VideoCX + static_cast<int>((rVertex[s].x / (-rVertex[s].z) * CameraW));
      gScrp[s].y = VideoCY - static_cast<int>((rVertex[s].y / (-rVertex[s].z) * CameraH));
    }

    if (gScrp[s].x > maxx) maxx = gScrp[s].x;
    if (gScrp[s].x < minx) minx = gScrp[s].x;
    if (gScrp[s].y > maxy) maxy = gScrp[s].y;
    if (gScrp[s].y < miny) miny = gScrp[s].y;
  }

  if (minx == 10241024) return;
  if (minx>WinW || maxx<0 || miny>WinH || maxy<0) return;

  if (fabs(z0) + fabs(x0)>256*10)
    BuildTreeNoSort();
  else BuildTree();


  if (Current != -1) DrawModelFaces();
  return;

  f = Current;
  while( f!=-1 )
  {
    mscrp[0].x = gScrp[mptr->gFace[f].v1].x;
    mscrp[0].y = gScrp[mptr->gFace[f].v1].y;
    mscrp[0].tx = mptr->gFace[f].tax;
    mscrp[0].ty = mptr->gFace[f].tay;

    mscrp[1].x = gScrp[mptr->gFace[f].v2].x;
    mscrp[1].y = gScrp[mptr->gFace[f].v2].y;
    mscrp[1].tx = mptr->gFace[f].tbx;
    mscrp[1].ty = mptr->gFace[f].tby;

    mscrp[2].x = gScrp[mptr->gFace[f].v3].x;
    mscrp[2].y = gScrp[mptr->gFace[f].v3].y;
    mscrp[2].tx = mptr->gFace[f].tcx;
    mscrp[2].ty = mptr->gFace[f].tcy;

    OpacityMode = (mptr->gFace[f].Flags & (sfOpacity + sfTransparent));
    if (mptr->gFace[f].Flags & sfDark)
      iModelFade = iModelBaseFade + 12*256*256;
    else iModelFade = iModelBaseFade;

    DrawModelFace();
    f = mptr->gFace[f].Next;
  }
}




void RenderBMPModel(TBMPModel* _mptr, float x0, float y0, float z0, int light)
{
  int f;

  TBMPModel *mptr = _mptr;


  int minx = 10241024;
  int maxx =-10241024;
  int miny = 10241024;
  int maxy =-10241024;

  __asm
  {
    mov eax,light
    shr eax,2
    shl eax,16
    add eax, offset FadeTab
    mov iModelFade,eax
    mov iModelBaseFade,eax
  }


  lpTextureAddr = mptr->lpTexture.get();
  HLineT = (void*) HLineTxModel2;


  for (int s=0; s<4; s++)
  {

    rVertex[s].x = mptr->gVertex[s].x + x0;
    rVertex[s].y = mptr->gVertex[s].y + y0;
    rVertex[s].z = z0;

    if (rVertex[s].z<-256)
    {
      gScrp[s].x = VideoCX + static_cast<int>((rVertex[s].x / (-rVertex[s].z) * CameraW));
      gScrp[s].y = VideoCY - static_cast<int>((rVertex[s].y / (-rVertex[s].z) * CameraH));
    }
    else return;

    if (gScrp[s].x > maxx) maxx = gScrp[s].x;
    if (gScrp[s].x < minx) minx = gScrp[s].x;
    if (gScrp[s].y > maxy) maxy = gScrp[s].y;
    if (gScrp[s].y < miny) miny = gScrp[s].y;
  }

  if (minx == 10241024) return;
  if (minx>WinW || maxx<0 || miny>WinH || maxy<0) return;

  //int w  =  gScrp[1].x - gScrp[0].x;
  int h  =  gScrp[2].y - gScrp[1].y;
  //int xo = (gScrp[1].x + gScrp[0].x) / 2;
  int yo =  gScrp[0].y;

  if (!h) return;

  xa = gScrp[0].x<<16;
  xb = gScrp[1].x<<16;

  Y1 = yo;
  int DTY = 128*256*256 / h;
  int TY = -DTY;
  for (int y=0; y<h; y++)
  {
    Y1++;
    TY+=DTY;
    if (Y1< 0   ) continue;
    if (Y1>=WinH) continue;
    lpTextureAddr = (void*) (mptr->lpTexture.get() + (TY>>16)*128);
    HLineTxModelBMP();
  }
}







void RenderNearModel(TModel* _mptr, float x0, float y0, float z0, int light, float al, float bt)
{
  int f;

  mptr = _mptr;

  float ca = static_cast<float>(cos(al));
  float sa = static_cast<float>(sin(al));

  float cb = static_cast<float>(cos(bt));
  float sb = static_cast<float>(sin(bt));


  //light = 0;
  __asm
  {
    mov eax,light
    shr eax,2
    shl eax,16
    add eax, offset FadeTab
    mov iModelFade,eax
    mov iModelBaseFade,eax
  }


  HLineT = (void*) HLineTxModel;
  lpTextureAddr = mptr->lpTexture.get();

  BOOL BL = false;
  for (int s=0; s<mptr->VCount; s++)
  {
    rVertex[s].x = (mptr->gVertex[s].x * ca + mptr->gVertex[s].z * sa)   + x0;
    float vz = mptr->gVertex[s].z * ca - mptr->gVertex[s].x * sa;
    rVertex[s].y = (mptr->gVertex[s].y * cb - vz * sb) + y0;
    rVertex[s].z = (vz * cb + mptr->gVertex[s].y * sb) + z0;
    if (rVertex[s].z<0) BL=true;
  }

  if (!BL) return;

  BuildTreeClip();

  f = Current;
  while( f!=-1 )
  {

    vused = 3;
    TFace *fptr = &mptr->gFace[f];

    cp[0].ev.v = rVertex[fptr->v1];
    cp[0].tx = fptr->tax;
    cp[0].ty = fptr->tay;
    cp[1].ev.v = rVertex[fptr->v2];
    cp[1].tx = fptr->tbx;
    cp[1].ty = fptr->tby;
    cp[2].ev.v = rVertex[fptr->v3];
    cp[2].tx = fptr->tcx;
    cp[2].ty = fptr->tcy;

    for (u=0; u<vused; u++) cp[u].ev.v.z+=12.0f;
    for (u=0; u<vused; u++) ClipVector(ClipZ,u);
    for (u=0; u<vused; u++) cp[u].ev.v.z-=12.0f;
    if (vused<3) goto LNEXT;

    // near models (weapon, compass, wind dial) are placed in screen space
    // via VideoCX/VideoCY and CameraW/CameraH. The fixed frustum planes
    // (designed for the world view) clip them too aggressively in
    // widescreen/high-res. The GL renderer avoids this by deriving the
    // frustum from the current viewport. The rasterizer has its own X/Y
    // scissor (WinEX/WinEY/WinW), so skip ClipA/B/C/D here.

    for (u=0; u<vused; u++)
    {
      cp[u].ev.scrx = VideoCX - static_cast<int>((cp[u].ev.v.x / cp[u].ev.v.z * CameraW));
      cp[u].ev.scry = VideoCY + static_cast<int>((cp[u].ev.v.y / cp[u].ev.v.z * CameraH));
    }

    scrp[0].x     = cp[0].ev.scrx;
    scrp[0].y     = cp[0].ev.scry;
    scrp[0].z     = static_cast<int>(cp[0].ev.v.z);
    scrp[0].tx    = cp[0].tx;
    scrp[0].ty    = cp[0].ty;


    OpacityMode = (fptr->Flags & (sfOpacity + sfTransparent));
    if (CORRECTION) Soft_Persp_K = 2.0f;
    else Soft_Persp_K = 0.0f;
    for (u=0; u<vused-2; u++)
    {
      for (int n=1; n<3; n++)
      {
        scrp[n].x     = cp[n+u].ev.scrx;
        scrp[n].y     = cp[n+u].ev.scry;
        scrp[n].z     = static_cast<int>(cp[n+u].ev.v.z);
        scrp[n].tx    = cp[n+u].tx;
        scrp[n].ty    = cp[n+u].ty;
      }

      DrawCorrectedTexturedFace();
    }
    Soft_Persp_K = 1.5f;
LNEXT:
    f = mptr->gFace[f].Next;
  }
}







#endif // _soft
