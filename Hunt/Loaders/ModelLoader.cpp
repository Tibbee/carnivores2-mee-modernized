// ==========================================================================
// ModelLoader.cpp � Model loading, conversion, and release
// ==========================================================================

#include "Hunt.h"

// Forward declarations (from original Resources.cpp)
void GenerateModelMipMaps(TModel *mptr, MemoryTag tag);
void GenerateAlphaFlags(TModel *mptr);
void CreateMipMapMT(WORD* dst, WORD* src, int H);
void CreateMipMapMT2(WORD* dst, WORD* src, int H);

int DitherHi(int C)
{
  int d = C & 255;
  C = C / 256;
  if (rand() * 255 / RAND_MAX < d) C++;
  if (C>31) C=31;
  return C;
}


void GenerateModelMipMaps(TModel *mptr, MemoryTag tag);
void GenerateAlphaFlags(TModel *mptr);


void CreateMipMap(WORD* src, WORD* dst, int Ls, int Ld)
{
  int scale = Ls / Ld;

  int R[64][64], G[64][64], B[64][64];

  FillMemory(R, sizeof(R), 0);
  FillMemory(G, sizeof(R), 0);
  FillMemory(B, sizeof(R), 0);

  for (int y=0; y<Ls; y++)
    for (int x=0; x<Ls; x++)
    {
      WORD C = *(src + x + y*Ls);
      B[ y/scale ][ x/scale ]+= (C>> 0) & 31;
      G[ y/scale ][ x/scale ]+= (C>> 5) & 31;
      R[ y/scale ][ x/scale ]+= (C>>10) & 31;
    }

  scale*=scale;

  for (int y=0; y<Ld; y++)
    for (int x=0; x<Ld; x++)
    {
      R[y][x]/=scale;
      G[y][x]/=scale;
      B[y][x]/=scale;
      *(dst + x + y*Ld) = (R[y][x]<<10) + (G[y][x]<<5) + B[y][x];
    }
}

int CalcImageDifference(WORD* A, WORD* B, int L)
{
  int r = 0;
  L*=L;
  for (int l=0; l<L; l++)
  {
    WORD C1 = *(A + l);
    WORD C2 = *(B + l);
    int R1 = (C1>>10) & 31;
    int G1 = (C1>> 5) & 31;
    int B1 = (C1>> 0) & 31;
    int R2 = (C2>>10) & 31;
    int G2 = (C2>> 5) & 31;
    int B2 = (C2>> 0) & 31;

    r+=(R1-R2)*(R1-R2) +
       (G1-G2)*(G1-G2) +
       (B1-B2)*(B1-B2);
  }

  return r;
}

void RotateImage(WORD* src, WORD* dst, int L)
{
  for (int y=0; y<L; y++)
    for (int x=0; x<L; x++)
      *(dst + x*L + (L-1-y) ) = *(src + x + y*L);
}

void BrightenTexture(WORD* A, int L)
{
  int factor=OptBrightness + 128;
  //if (factor > 256) factor = (factor-256)*3/2 + 256;
  for (int c=0; c<L; c++)
  {
    WORD w = *(A +  c);
    int B = (w>> 0) & 31;
    int G = (w>> 5) & 31;
    int R = (w>>10) & 31;
    B = (B * factor) >> 8;
    if (B > 31) B = 31;
    G = (G * factor) >> 8;
    if (G > 31) G = 31;
    R = (R * factor) >> 8;
    if (R > 31) R = 31;

    *(A + c) = (B) + (G<<5) + (R<<10);
  }
}

void GenerateMipMap(WORD* A, WORD* D, int L)
{
  for (int y=0; y<L; y++)
    for (int x=0; x<L; x++)
    {
      int C1 = *(A + x*2 +   (y*2+0)*2*L);
      int C2 = *(A + x*2+1 + (y*2+0)*2*L);
      int C3 = *(A + x*2 +   (y*2+1)*2*L);
      int C4 = *(A + x*2+1 + (y*2+1)*2*L);
      //C4 = C1;
      /*
      if (L==64)
       C3=((C3 & 0x7bde) +  (C1 & 0x7bde))>>1;
       */
      int B = ( ((C1>>0) & 31) + ((C2>>0) & 31) + ((C3>>0) & 31) + ((C4>>0) & 31) +2 ) >> 2;
      int G = ( ((C1>>5) & 31) + ((C2>>5) & 31) + ((C3>>5) & 31) + ((C4>>5) & 31) +2 ) >> 2;
      int R = ( ((C1>>10) & 31) + ((C2>>10) & 31) + ((C3>>10) & 31) + ((C4>>10) & 31) +2 ) >> 2;
      *(D + x + y * L) = HiColor(R,G,B);
    }
}

int CalcColorSum(WORD* A, int L)
{
  int R = 0, G = 0, B = 0;
  for (int x=0; x<L; x++)
  {
    B+= (*(A+x) >> 0) & 31;
    G+= (*(A+x) >> 5) & 31;
    R+= (*(A+x) >>10) & 31;
  }
  return HiColor(R/L, G/L, B/L);
}

void GenerateShadedMipMap(WORD* src, WORD* dst, int L)
{
  for (int x=0; x<16*16; x++)
  {
    int B = (*(src+x) >> 0) & 31;
    int G = (*(src+x) >> 5) & 31;
    int R = (*(src+x) >>10) & 31;
    R=DitherHi(SkyR*L/8 + R*(256-L)+6);
    G=DitherHi(SkyG*L/8 + G*(256-L)+6);
    B=DitherHi(SkyB*L/8 + B*(256-L)+6);
    *(dst + x) = HiColor(R,G,B);
  }
}

void GenerateShadedSkyMipMap(WORD* src, WORD* dst, int L)
{
  for (int x=0; x<128*128; x++)
  {
    int B = (*(src+x) >> 0) & 31;
    int G = (*(src+x) >> 5) & 31;
    int R = (*(src+x) >>10) & 31;
    R=DitherHi(SkyR*L/8 + R*(256-L)+6);
    G=DitherHi(SkyG*L/8 + G*(256-L)+6);
    B=DitherHi(SkyB*L/8 + B*(256-L)+6);
    *(dst + x) = HiColor(R,G,B);
  }
}

void DATASHIFT(WORD* d, int cnt)
{
  cnt>>=1;
  /*
  for (int l=0; l<cnt; l++)
    *(d+l)=(*(d+l)) & 0x3e0;
  */
  if (HARD3D) return;

  for (l=0; l<cnt; l++)
    *(d+l)*=2;

}

void ApplyAlphaFlags(WORD* tptr, int cnt)
{
#ifdef _d3d
  for (int w=0; w<cnt; w++)
    *(tptr+w)|=0x8000;
#endif
}

void CalcMidColor(WORD* tptr, int l, int &mr, int &mg, int &mb)
{
  for (int w=0; w<l; w++)
  {
    WORD c = *(tptr + w);
    mb+=((c>> 0) & 31)*8;
    mg+=((c>> 5) & 31)*8;
    mr+=((c>>10) & 31)*8;
  }

  mr/=l;
  mg/=l;
  mb/=l;
}

void LoadTexture(unique_obj_ptr<TEXTURE> &T)
{
  // Phase 5E follow-up (Gap #2): LoadTexture is only called from
  // LoadResources (per-level). Tag as Level so the arena reclaims
  // per-level textures in bulk on Reset().
  T.reset((TEXTURE*) _HeapAlloc(Heap, 0, sizeof(TEXTURE), MemoryTag::Level));
  DWORD L;
  ReadFile(hfile, T->DataA, 128*128*2, &L, nullptr);
  for (int y=0; y<128; y++)
    for (int x=0; x<128; x++)
      if (!T->DataA[y*128+x]) T->DataA[y*128+x]=1;

  BrightenTexture(T->DataA, 128*128);

  CalcMidColor(T->DataA, 128*128, T->mR, T->mG, T->mB);

  GenerateMipMap(T->DataA, T->DataB, 64);
  GenerateMipMap(T->DataB, T->DataC, 32);
  GenerateMipMap(T->DataC, T->DataD, 16);
  memcpy(T->SDataC[0], T->DataC, 32*32*2);
  memcpy(T->SDataC[1], T->DataC, 32*32*2);

  DATASHIFT((unsigned short *)T.get(), sizeof(TEXTURE));
  for (int w=0; w<32*32; w++)
    T->SDataC[1][w] = FadeTab[48][T->SDataC[1][w]>>1];

  ApplyAlphaFlags(T->DataA, 128*128);
  ApplyAlphaFlags(T->DataB, 64*64);
  ApplyAlphaFlags(T->DataC, 32*32);
}

void LoadSky()
{
  SetFilePointer(hfile, 256*512*OptDayNight, nullptr, FILE_CURRENT);
  ReadFile(hfile, SkyPic, 256*256*2, &l, nullptr);
  SetFilePointer(hfile, 256*512*(2-OptDayNight), nullptr, FILE_CURRENT);

  BrightenTexture(SkyPic, 256*256);

  for (int y=0; y<128; y++)
    for (int x=0; x<128; x++)
      SkyFade[0][y*128+x] = SkyPic[y*2*256  + x*2];

  for (int l=1; l<8; l++)
    GenerateShadedSkyMipMap(SkyFade[0], SkyFade[l], l*32-16);
  GenerateShadedSkyMipMap(SkyFade[0], SkyFade[8], 250);
  ApplyAlphaFlags(SkyPic, 256*256);
  //DATASHIFT(SkyPic, 256*256*2);
}

void LoadSkyMap()
{
  ReadFile(hfile, SkyMap, 128*128, &l, nullptr);
}

void fp_conv(LPVOID d)
{
  int i;
  float f;
  memcpy(&i, d, 4);
#ifdef _d3d
  f = (static_cast<float>(i)) / 256.f;
#else
  f = (static_cast<float>(i));
#endif
  memcpy(d, &f, 4);
}

void CorrectModel(TModel *mptr, MemoryTag tag)
{
	// Allocating for 2x the faces here, since the code below could potentially
	// result in duplicated faces when sfOpacity & sfTransparent is set for the same
	// face.
	TFace *tface = (TFace*)_HeapAlloc(Heap, 0, sizeof(TFace) * mptr->FCount * 2, tag);

  for (int f=0; f<mptr->FCount; f++)
  {
    if (!(mptr->gFace[f].Flags & sfDoubleSide))
      mptr->gFace[f].Flags |= sfNeedVC;
#ifdef _soft
    {
        // Phase 1.4: TFace is unified to float. Convert the loaded integer UV
        // (bitcast through float) to 16.16 fixed-point for the software renderer.
        int raw;
        memcpy(&raw, &mptr->gFace[f].tax, sizeof(int));
        raw = (raw << 16) + 0x8000;
        memcpy(&mptr->gFace[f].tax, &raw, sizeof(int));
        memcpy(&raw, &mptr->gFace[f].tay, sizeof(int));
        raw = (raw << 16) + 0x8000;
        memcpy(&mptr->gFace[f].tay, &raw, sizeof(int));
        memcpy(&raw, &mptr->gFace[f].tbx, sizeof(int));
        raw = (raw << 16) + 0x8000;
        memcpy(&mptr->gFace[f].tbx, &raw, sizeof(int));
        memcpy(&raw, &mptr->gFace[f].tby, sizeof(int));
        raw = (raw << 16) + 0x8000;
        memcpy(&mptr->gFace[f].tby, &raw, sizeof(int));
        memcpy(&raw, &mptr->gFace[f].tcx, sizeof(int));
        raw = (raw << 16) + 0x8000;
        memcpy(&mptr->gFace[f].tcx, &raw, sizeof(int));
        memcpy(&raw, &mptr->gFace[f].tcy, sizeof(int));
        raw = (raw << 16) + 0x8000;
        memcpy(&mptr->gFace[f].tcy, &raw, sizeof(int));
    }
#else
    fp_conv(&mptr->gFace[f].tax);
    fp_conv(&mptr->gFace[f].tay);
    fp_conv(&mptr->gFace[f].tbx);
    fp_conv(&mptr->gFace[f].tby);
    fp_conv(&mptr->gFace[f].tcx);
    fp_conv(&mptr->gFace[f].tcy);
#endif
  }


  int fp = 0;
  for (int f=0; f<mptr->FCount; f++)
    if ( (mptr->gFace[f].Flags & (sfOpacity | sfTransparent))==0)
    {
      tface[fp] = mptr->gFace[f];
      fp++;
    }

  for (int f=0; f<mptr->FCount; f++)
    if ( (mptr->gFace[f].Flags & sfOpacity)!=0)
    {
      tface[fp] = mptr->gFace[f];
      fp++;
    }

  for (int f=0; f<mptr->FCount; f++)
    if ( (mptr->gFace[f].Flags & sfTransparent)!=0)
    {
      tface[fp] = mptr->gFace[f];
      fp++;
    }



  memcpy( mptr->gFace, tface, mptr->FCount << 6 );
  (void)_HeapFree(Heap, 0, tface);
}

void AllocateMemoryForModel(TModel* mptr, MemoryTag tag) {
	mptr->gVertex.reset((TPoint3d*)_HeapAlloc(Heap, 0, mptr->VCount << 4, tag));
	mptr->gFace = (TFace*)_HeapAlloc(Heap, 0, mptr->FCount << 6, tag);

	// Keep track of maximum VCount value
	MaxObjectVCount = MAX(MaxObjectVCount, mptr->VCount);

	float *lightBuffer = static_cast<float*>(_HeapAlloc(Heap, 0, mptr->VCount * 4 * sizeof(float), tag));
	mptr->VLight[0] = lightBuffer;
	mptr->VLight[1] = lightBuffer + mptr->VCount;
	mptr->VLight[2] = lightBuffer + mptr->VCount * 2;
	mptr->VLight[3] = lightBuffer + mptr->VCount * 3;
}

void LoadModel(unique_obj_ptr<TModel> &mptr, MemoryTag tag)
{
  // Per-level MObjects models go to the heap, NOT the arena.
  // Rationale: the GL renderer caches GPU resources (textures
  // in m_modelTextureCache and geometry in m_staticMeshCache)
  // keyed by TModel*. If the arena recycled TModel* addresses
  // across level transitions, both caches would serve stale
  // data for completely different models. Keeping TModels on
  // the heap guarantees stable addresses and correct cache
  // behaviour across level loads. Per-level texture pixel data
  // and animations still go to the arena.
  TModel* raw = (TModel*) _HeapAlloc(Heap, 0, sizeof(TModel));
  mptr.reset(new(raw) TModel());

  ReadFile( hfile, &mptr->VCount,      4,         &l, nullptr );
  ReadFile( hfile, &mptr->FCount,      4,         &l, nullptr );
  ReadFile( hfile, &OCount,            4,         &l, nullptr );
  ReadFile( hfile, &mptr->TextureSize, 4,         &l, nullptr );

  AllocateMemoryForModel(mptr.get(), tag);

  ReadFile( hfile, mptr->gFace,        mptr->FCount<<6, &l, nullptr );
  ReadFile( hfile, mptr->gVertex.get(),      mptr->VCount<<4, &l, nullptr );
  ReadFile( hfile, gObj,               OCount*48, &l, nullptr );

  if (HARD3D) CalcLights(mptr.get());

  int ts = mptr->TextureSize;

  if (HARD3D) mptr->TextureHeight = 256;
  else  mptr->TextureHeight = mptr->TextureSize>>9;

  mptr->TextureSize = mptr->TextureHeight*512;

  mptr->lpTexture.reset(static_cast<WORD*>(_HeapAlloc(Heap, 0, mptr->TextureSize, tag)));

  ReadFile(hfile, mptr->lpTexture.get(), ts, &l, nullptr);
  BrightenTexture(mptr->lpTexture.get(), ts/2);

  for (int v=0; v<mptr->VCount; v++)
  {
    mptr->gVertex[v].x*=2.f;
    mptr->gVertex[v].y*=2.f;
    mptr->gVertex[v].z*=-2.f;
  }

  CorrectModel(mptr.get(), tag);

  DATASHIFT(mptr->lpTexture.get(), mptr->TextureSize);
}

void LoadAnimation(TVTL &vtl)
{
  int vc;
  DWORD l;

  ReadFile( hfile, &vc,          4,    &l, nullptr );
  ReadFile( hfile, &vc,          4,    &l, nullptr );
  ReadFile( hfile, &vtl.aniKPS,  4,    &l, nullptr );
  ReadFile( hfile, &vtl.FramesCount,  4,    &l, nullptr );
  vtl.FramesCount++;

  // Phase 5E follow-up (Gap #2): LoadAnimation is only called from
  // LoadResources (per-level). Tag as Level for arena reclamation.
  vtl.AniTime = (vtl.FramesCount * 1000) / vtl.aniKPS;
  vtl.aniData.reset((short int*)
                _HeapAlloc(Heap, 0, (vc*vtl.FramesCount*6), MemoryTag::Level));
  ReadFile( hfile, vtl.aniData.get(), (vc*vtl.FramesCount*6), &l, nullptr);

}

void LoadModelEx(unique_obj_ptr<TModel> &mptr, char* FName, MemoryTag tag)
{

  hfile = CreateFile(FName,
                     GENERIC_READ, FILE_SHARE_READ,
                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hfile==INVALID_HANDLE_VALUE)
  {
    char sz[512];
    sprintf_s(sz, sizeof(sz), "Error opening file\n%s.", FName );
    DoHalt(sz);
  }

  TModel* raw = (TModel*) _HeapAlloc(Heap, 0, sizeof(TModel), tag);
  mptr.reset(new(raw) TModel());

  ReadFile( hfile, &mptr->VCount,      4,         &l, nullptr );
  ReadFile( hfile, &mptr->FCount,      4,         &l, nullptr );
  ReadFile( hfile, &OCount,            4,         &l, nullptr );
  ReadFile( hfile, &mptr->TextureSize, 4,         &l, nullptr );

  AllocateMemoryForModel(mptr.get(), tag);

  ReadFile( hfile, mptr->gFace,        mptr->FCount<<6, &l, nullptr );
  ReadFile( hfile, mptr->gVertex.get(),      mptr->VCount<<4, &l, nullptr );
  ReadFile( hfile, gObj,               OCount*48, &l, nullptr );

  int ts = mptr->TextureSize;
  if (HARD3D) mptr->TextureHeight = 256;
  else  mptr->TextureHeight = mptr->TextureSize>>9;
  mptr->TextureSize = mptr->TextureHeight*512;

  mptr->lpTexture.reset(static_cast<WORD*>(_HeapAlloc(Heap, 0, mptr->TextureSize, tag)));

  ReadFile(hfile, mptr->lpTexture.get(), ts, &l, nullptr);
  BrightenTexture(mptr->lpTexture.get(), ts/2);

  for (int v=0; v<mptr->VCount; v++)
  {
    mptr->gVertex[v].x*=2.f;
    mptr->gVertex[v].y*=2.f;
    mptr->gVertex[v].z*=-2.f;
  }

  CorrectModel(mptr.get(), tag);

  DATASHIFT(mptr->lpTexture.get(), mptr->TextureSize);
  GenerateModelMipMaps(mptr.get(), tag);
  GenerateAlphaFlags(mptr.get());
}

void GetObjectCaracteristics(TModel* mptr, int& ylo, int& yhi)
{
  ylo = 10241024;
  yhi =-10241024;
  for (int v=0; v<mptr->VCount; v++)
  {
    if (mptr->gVertex[v].y < ylo) ylo = static_cast<int>(mptr->gVertex[v].y);
    if (mptr->gVertex[v].y > yhi) yhi = static_cast<int>(mptr->gVertex[v].y);
  }
  if (yhi<ylo) yhi=ylo+1;
}

void GenerateAlphaFlags(TModel *mptr)
{
#ifdef _d3d

  int w;
  BOOL Opacity = false;
  WORD* tptr = mptr->lpTexture.get();

  for (w=0; w<mptr->FCount; w++)
    if ((mptr->gFace[w].Flags & sfOpacity)>0) Opacity = true;

  if (Opacity)
  {
    for (w=0; w<256*256; w++)
      if ( *(tptr+w)>0 ) *(tptr+w)=(*(tptr+w)) + 0x8000;
  }
  else
    for (w=0; w<256*256; w++)
      *(tptr+w)=(*(tptr+w)) + 0x8000;

  tptr = mptr->lpTexture2.get();
  if (tptr==nullptr) return;

  if (Opacity)
  {
    for (w=0; w<128*128; w++)
      if ( (*(tptr+w))>0 ) *(tptr+w)=(*(tptr+w)) + 0x8000;
  }
  else
    for (w=0; w<128*128; w++)
      *(tptr+w)=(*(tptr+w)) + 0x8000;

  tptr = mptr->lpTexture3.get();
  if (tptr==nullptr) return;

  if (Opacity)
  {
    for (w=0; w<64*64; w++)
      if ( (*(tptr+w))>0 ) *(tptr+w)=(*(tptr+w)) + 0x8000;
  }
  else
    for (w=0; w<64*64; w++)
      *(tptr+w)=(*(tptr+w)) + 0x8000;

#endif
}

void GenerateModelMipMaps(TModel *mptr, MemoryTag tag)
{
  int th = (mptr->TextureHeight) / 2;
  mptr->lpTexture2.reset(
    static_cast<WORD*>(_HeapAlloc(Heap, HEAP_ZERO_MEMORY, (1+th)*128*2, tag)));
  CreateMipMapMT(mptr->lpTexture2.get(), mptr->lpTexture.get(), th);

  th = (mptr->TextureHeight) / 4;
  mptr->lpTexture3.reset(
    static_cast<WORD*>(_HeapAlloc(Heap, HEAP_ZERO_MEMORY, (1+th)*64*2, tag)));
  CreateMipMapMT2(mptr->lpTexture3.get(), mptr->lpTexture2.get(), th);
}

void GenerateMapImage()
{
  int YShift = 23;
  int XShift = 11;
  int lsw = MapPic.W;
  for (int y=0; y<256; y++)
    for (int x=0; x<256; x++)
    {
      int t;
      WORD c;

      if (FMap[y<<2][x<<2] & fmWater)
      {
        t = WaterList[WMap[y<<2][x<<2]].tindex;
        c= Textures[t]->DataD[(y & 15)*16+(x&15)];
      }
      else
      {
        t = TMap1[y<<2][x<<2];
        c= Textures[t]->DataC[(y & 31)*32+(x&31)];
      }

      if (!HARD3D) c=c>>1;
      else c=conv_565(c);
      *(MapPic.lpImage.get() + (y+YShift)*lsw + x + XShift) = c;
    }
}

void LoadBMPModel(TObject &obj)
{
  // Phase 5E follow-up (Gap #2): LoadBMPModel is per-level (called from
  // LoadResources). Tag as Level so the arena reclaims this allocation.
  obj.bmpmodel.lpTexture.reset(static_cast<WORD*>(_HeapAlloc(Heap, 0, 128 * 128 * 2, MemoryTag::Level)));
  //WORD * lpT             = static_cast<WORD*>(_HeapAlloc(Heap, 0, 256 * 256 * 2));
  //ReadFile(hfile, lpT, 256*256*2, &l, nullptr);
  //DATASHIFT(obj.bmpmodel.lpTexture.get(), 128*128*2);
  //BrightenTexture(lpT, 256*256);
  ReadFile(hfile, obj.bmpmodel.lpTexture.get(), 128*128*2, &l, nullptr);
  BrightenTexture(obj.bmpmodel.lpTexture.get(), 128*128);
  DATASHIFT(obj.bmpmodel.lpTexture.get(), 128*128*2);
  //CreateMipMapMT(obj.bmpmodel.lpTexture, lpT, 128);

  //_HeapFree(Heap, 0, lpT);

  if (HARD3D)
    for (int x=0; x<128; x++)
      for (int y=0; y<128; y++)
        if ( *(obj.bmpmodel.lpTexture.get() + x + y*128) )
          *(obj.bmpmodel.lpTexture.get() + x + y*128) |= 0x8000;

  float mxx = obj.model->gVertex[0].x+0.5f;
  float mnx = obj.model->gVertex[0].x-0.5f;

  float mxy = obj.model->gVertex[0].x+0.5f;
  float mny = obj.model->gVertex[0].y-0.5f;

  for (int v=0; v<obj.model->VCount; v++)
  {
    float x = obj.model->gVertex[v].x;
    float y = obj.model->gVertex[v].y;
    if (x > mxx) mxx=x;
    if (x < mnx) mnx=x;
    if (y > mxy) mxy=y;
    if (y < mny) mny=y;
  }

  obj.bmpmodel.gVertex[0].x = mnx;
  obj.bmpmodel.gVertex[0].y = mxy;
  obj.bmpmodel.gVertex[0].z = 0;

  obj.bmpmodel.gVertex[1].x = mxx;
  obj.bmpmodel.gVertex[1].y = mxy;
  obj.bmpmodel.gVertex[1].z = 0;

  obj.bmpmodel.gVertex[2].x = mxx;
  obj.bmpmodel.gVertex[2].y = mny;
  obj.bmpmodel.gVertex[2].z = 0;

  obj.bmpmodel.gVertex[3].x = mnx;
  obj.bmpmodel.gVertex[3].y = mny;
  obj.bmpmodel.gVertex[3].z = 0;
}

void ReleaseModel(unique_obj_ptr<TModel> &mptr)
{
  // Release the GL texture cache entry for this model before dropping
  // the TModel. C1 calls renderer->ReleaseModelTextures() here for the
  // same reason: the GL renderer caches GPU textures keyed by TModel*,
  // and if this TModel* is reused (arena recycling) the stale cache
  // entry would serve the wrong texture. ReleaseModelTexture does the
  // cache removal + glDeleteTextures on the GL side; the unique_ptrs
  // below handle the CPU-side memory.
  if (!mptr) return;
  ReleaseModelTexture(mptr.get());
  mptr->lpTexture.reset();
  mptr->lpTexture2.reset();
  mptr->lpTexture3.reset();

  // gFace and VLight[0] are raw pointers (not smart pointers) because
  // gFace lives in a union and VLight is a 4-channel view into one
  // allocation. They are heap-allocated by AllocateMemoryForModel and
  // must be freed explicitly. _HeapFree safely no-ops for arena-owned
  // addresses, so this is correct regardless of the MemoryTag used at
  // allocation time.
  if (mptr->gFace) {
    (void)_HeapFree(Heap, 0, mptr->gFace);
    mptr->gFace = nullptr;
  }
  if (mptr->VLight[0]) {
    (void)_HeapFree(Heap, 0, mptr->VLight[0]);
    for (int i = 0; i < 4; i++) {
      mptr->VLight[i] = nullptr;
    }
  }

  mptr.reset();
}

void ReleaseCharacterInfo(TCharacterInfo &chinfo)
{
  if (!chinfo.mptr) return;

  chinfo.mptr.reset();

  for (int c = 0; c<64; c++)
  {
    if (chinfo.Animation[c].aniData.get() == nullptr) break;
    chinfo.Animation[c].aniData.reset();
  }

  // Phase 5B.1: TSFX::lpData is now std::vector; the vector destructor
  // reclaims the buffer on Reset, so no manual _HeapFree is needed.
  for (int c = 0; c<64; c++)
  {
    if (chinfo.SoundFX[c].lpData.empty()) break;
    chinfo.SoundFX[c].lpData.clear();
  }

  chinfo.AniCount = 0;
  chinfo.SfxCount = 0;
}

void LoadCharacterInfo(TCharacterInfo &chinfo, char* FName, MemoryTag tag)
{
  ReleaseCharacterInfo(chinfo);

  HANDLE hfile = CreateFile(FName,
                            GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hfile==INVALID_HANDLE_VALUE)
  {
    char sz[512];
    sprintf_s(sz, sizeof(sz), "Error opening character file:\n%s.", FName );
    DoHalt(sz);
  }

  ReadFile(hfile, chinfo.ModelName, 32, &l, nullptr);
  ReadFile(hfile, &chinfo.AniCount,  4, &l, nullptr);
  ReadFile(hfile, &chinfo.SfxCount,  4, &l, nullptr);

//============= read model =================//

  TModel* chraw = (TModel*) _HeapAlloc(Heap, 0, sizeof(TModel), tag);
  chinfo.mptr.reset(new(chraw) TModel());

  ReadFile( hfile, &chinfo.mptr->VCount,      4,         &l, nullptr );
  ReadFile( hfile, &chinfo.mptr->FCount,      4,         &l, nullptr );
  ReadFile( hfile, &chinfo.mptr->TextureSize, 4,         &l, nullptr );

  AllocateMemoryForModel(chinfo.mptr.get(), tag);

  ReadFile( hfile, chinfo.mptr->gFace,        chinfo.mptr->FCount<<6, &l, nullptr );
  ReadFile( hfile, chinfo.mptr->gVertex.get(),      chinfo.mptr->VCount<<4, &l, nullptr );

  int ts = chinfo.mptr->TextureSize;
  if (HARD3D) chinfo.mptr->TextureHeight = 256;
  else  chinfo.mptr->TextureHeight = chinfo.mptr->TextureSize>>9;
  chinfo.mptr->TextureSize = chinfo.mptr->TextureHeight*512;

  chinfo.mptr->lpTexture.reset(static_cast<WORD*>(_HeapAlloc(Heap, 0, chinfo.mptr->TextureSize, tag)));

  ReadFile(hfile, chinfo.mptr->lpTexture.get(), ts, &l, nullptr);
  BrightenTexture(chinfo.mptr->lpTexture.get(), ts/2);

  DATASHIFT(chinfo.mptr->lpTexture.get(), chinfo.mptr->TextureSize);
  GenerateModelMipMaps(chinfo.mptr.get(), tag);
  GenerateAlphaFlags(chinfo.mptr.get());
  //CalcLights(chinfo.mptr.get());

  //ApplyAlphaFlags(chinfo.mptr->lpTexture, 256*256);
  //ApplyAlphaFlags(chinfo.mptr->lpTexture2, 128*128);
//============= read animations =============//
  for (int a=0; a<chinfo.AniCount; a++)
  {
    ReadFile(hfile, chinfo.Animation[a].aniName, 32, &l, nullptr);
    ReadFile(hfile, &chinfo.Animation[a].aniKPS, 4, &l, nullptr);
    ReadFile(hfile, &chinfo.Animation[a].FramesCount, 4, &l, nullptr);
    chinfo.Animation[a].AniTime = (chinfo.Animation[a].FramesCount * 1000) / chinfo.Animation[a].aniKPS;
    chinfo.Animation[a].aniData.reset((short int*)
                                  _HeapAlloc(Heap, 0, (chinfo.mptr->VCount*chinfo.Animation[a].FramesCount*6), tag));

    ReadFile(hfile, chinfo.Animation[a].aniData.get(), (chinfo.mptr->VCount*chinfo.Animation[a].FramesCount*6), &l, nullptr);
  }

//============= read sound fx ==============//
  BYTE tmp[32];
  for (int s=0; s<chinfo.SfxCount; s++)
  {
    ReadFile(hfile, tmp, 32, &l, nullptr);
    ReadFile(hfile, &chinfo.SoundFX[s].length, 4, &l, nullptr);
    // Phase 5B.1: lpData is now std::vector<short int>.
    const size_t sfxSampleCount = chinfo.SoundFX[s].length / sizeof(short int);
    chinfo.SoundFX[s].lpData.assign(sfxSampleCount, 0);
    ReadFile(hfile, chinfo.SoundFX[s].lpData.data(), chinfo.SoundFX[s].length, &l, nullptr);
  }

  for (int v=0; v<chinfo.mptr->VCount; v++)
  {
    chinfo.mptr->gVertex[v].x*=2.f;
    chinfo.mptr->gVertex[v].y*=2.f;
    chinfo.mptr->gVertex[v].z*=-2.f;
  }

  CorrectModel(chinfo.mptr.get(), tag);


  ReadFile(hfile, chinfo.Anifx, 64*4, &l, nullptr);
  if (l!=256)
    for (l=0; l<64; l++) chinfo.Anifx[l] = -1;
  CloseHandle(hfile); 
}


void CreateMipMapMT(WORD* dst, WORD* src, int H)
{
  for (int y=0; y<H; y++)
    for (int x=0; x<127; x++)
    {
      int C1 = *(src + (x*2+0) + (y*2+0)*256);
      int C2 = *(src + (x*2+1) + (y*2+0)*256);
      int C3 = *(src + (x*2+0) + (y*2+1)*256);
      int C4 = *(src + (x*2+1) + (y*2+1)*256);

      if (!HARD3D)
      {
        C1>>=1;
        C2>>=1;
        C3>>=1;
        C4>>=1;
      }

      /*if (C1 == 0 && C2!=0) C1 = C2;
        if (C1 == 0 && C3!=0) C1 = C3;
        if (C1 == 0 && C4!=0) C1 = C4;*/

      if (C1 == 0)
      {
        *(dst + x + y*128) = 0;
        continue;
      }

      //C4 = C1;

      if (!C2) C2=C1;
      if (!C3) C3=C1;
      if (!C4) C4=C1;

      int B = ( ((C1>> 0) & 31) + ((C2 >>0) & 31) + ((C3 >>0) & 31) + ((C4 >>0) & 31) +2 ) >> 2;
      int G = ( ((C1>> 5) & 31) + ((C2 >>5) & 31) + ((C3 >>5) & 31) + ((C4 >>5) & 31) +2 ) >> 2;
      int R = ( ((C1>>10) & 31) + ((C2>>10) & 31) + ((C3>>10) & 31) + ((C4>>10) & 31) +2 ) >> 2;
      if (!HARD3D) *(dst + x + y * 128) = HiColor(R,G,B)*2;
      else *(dst + x + y * 128) = HiColor(R,G,B);
    }
}

void CreateMipMapMT2(WORD* dst, WORD* src, int H)
{
  for (int y=0; y<H; y++)
    for (int x=0; x<63; x++)
    {
      int C1 = *(src + (x*2+0) + (y*2+0)*128);
      int C2 = *(src + (x*2+1) + (y*2+0)*128);
      int C3 = *(src + (x*2+0) + (y*2+1)*128);
      int C4 = *(src + (x*2+1) + (y*2+1)*128);

      if (!HARD3D)
      {
        C1>>=1;
        C2>>=1;
        C3>>=1;
        C4>>=1;
      }

      if (C1 == 0)
      {
        *(dst + x + y*64) = 0;
        continue;
      }

      //C2 = C1;

      if (!C2) C2=C1;
      if (!C3) C3=C1;
      if (!C4) C4=C1;

      int B = ( ((C1>> 0) & 31) + ((C2 >>0) & 31) + ((C3 >>0) & 31) + ((C4 >>0) & 31) +2 ) >> 2;
      int G = ( ((C1>> 5) & 31) + ((C2 >>5) & 31) + ((C3 >>5) & 31) + ((C4 >>5) & 31) +2 ) >> 2;
      int R = ( ((C1>>10) & 31) + ((C2>>10) & 31) + ((C3>>10) & 31) + ((C4>>10) & 31) +2 ) >> 2;
      if (!HARD3D) *(dst + x + y * 64) = HiColor(R,G,B)*2;
      else *(dst + x + y * 64) = HiColor(R,G,B);
    }
}