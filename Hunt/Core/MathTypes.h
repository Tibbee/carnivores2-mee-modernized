// MathTypes.h — Vector and geometry types
// Extracted from Hunt.h (Phase 0.1 — Split god header into focused headers)
#pragma once

struct Vector3d
{
  float x, y, z;
};

struct TPoint3di
{
  int x, y, z;
};

struct Vector2di
{
  int x, y;
};

struct Vector2df
{
  float x, y;
};

struct ScrPoint
{
#ifdef _soft
  int   x, y, tx, ty;
#else
  float x, y, tx, ty;
#endif
  int Light, z, r2, r3;
};

struct MScrPoint
{
  int x, y, tx, ty;
};

struct CLIPPLANE
{
  Vector3d v1, v2, nv;
};

struct EPoint
{
  Vector3d v;
  unsigned short DFlags;
  short int ALPHA;
  int  scrx, scry, Light;
  float Fog;
};

struct ClipPoint
{
  EPoint ev;
  float tx, ty;
};
