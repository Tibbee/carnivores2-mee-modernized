// ModelTypes.h — Model/animation-related type definitions
// Extracted from Hunt.h (Phase 0.1 — Split god header into focused headers)
#pragma once

#include "Memory.h"
#include "Core/MathTypes.h"
#include <cstdint>

struct TAni
{
  char aniName[32];
  int aniKPS, FramesCount, AniTime;
  unique_heap_ptr<short int[]> aniData;
};

struct TVTL
{
  int aniKPS, FramesCount, AniTime;
  unique_heap_ptr<short int[]> aniData;
};

struct TPoint3d
{
  float x;
  float y;
  float z;
  short owner;
  short hide;
};

struct TFace
{
  int v1, v2, v3;
  float tax, tbx, tcx, tay, tby, tcy;
  unsigned short Flags, DMask;
  int Distant, Next, group;
  char reserv[12];
};

// TFacef is now identical to TFace (unified in Phase 1.4). Kept as an alias
// for compatibility; the union in TModel can use either.
using TFacef = TFace;

struct TObj
{
  char OName[32];
  float ox;
  float oy;
  float oz;
  short owner;
  short hide;
};

struct TObjInfo
{
  int  Radius;
  int  YLo, YHi;
  int  linelenght, lintensity;
  int  circlerad, cintensity;
  int  flags;
  int  GrRad;
  int  DefLight;
  int  LastAniTime;
  float BoundR;
  unsigned char res[16];
};

struct TBMPModel
{
  Vector3d  gVertex[4];
  unique_heap_ptr<unsigned short[]> lpTexture;
};

struct TBound
{
  float cx, cy, a, b, y1, y2;
};

struct TModel
{
  int VCount, FCount, TextureSize, TextureHeight;
  unique_heap_ptr<TPoint3d[]> gVertex;

  union
  {
    TFace    *gFace;
    TFacef   *gFacef;
  };

  unique_heap_ptr<unsigned short[]> lpTexture, lpTexture2, lpTexture3;

  float*    VLight[4];

  TModel() = default;

  TModel(const TModel&) = delete;
  TModel& operator=(const TModel&) = delete;

  TModel(TModel&& other) noexcept
    : VCount(other.VCount), FCount(other.FCount),
      TextureSize(other.TextureSize), TextureHeight(other.TextureHeight),
      gVertex(std::move(other.gVertex)),
      gFace(other.gFace),
      lpTexture(std::move(other.lpTexture)),
      lpTexture2(std::move(other.lpTexture2)),
      lpTexture3(std::move(other.lpTexture3))
  {
    for (int i = 0; i < 4; i++) {
      VLight[i] = other.VLight[i];
      other.VLight[i] = nullptr;
    }
    other.gFace = nullptr;
  }

  TModel& operator=(TModel&& other) noexcept
  {
    if (this != &other) {
      VCount = other.VCount;
      FCount = other.FCount;
      TextureSize = other.TextureSize;
      TextureHeight = other.TextureHeight;
      gVertex = std::move(other.gVertex);
      gFace = other.gFace;
      lpTexture = std::move(other.lpTexture);
      lpTexture2 = std::move(other.lpTexture2);
      lpTexture3 = std::move(other.lpTexture3);
      for (int i = 0; i < 4; i++) {
        VLight[i] = other.VLight[i];
        other.VLight[i] = nullptr;
      }
      other.gFace = nullptr;
    }
    return *this;
  }

  ~TModel() = default;
};

struct TObject
{
  TObjInfo info;
  TBound   bound[8];
  TBMPModel bmpmodel;
  unique_obj_ptr<TModel> model;
  TVTL    vtl;
};
