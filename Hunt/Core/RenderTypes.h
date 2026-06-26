// RenderTypes.h — Rendering-related type definitions
// Extracted from Hunt.h (Phase 0.1 — Split god header into focused headers)
#pragma once

#include "Memory.h"
#include "Core/Constants.h"
#include <cstdint>

struct TMessageList
{
  int timeleft;
  char mtext[256];
};

struct TRGB
{
  unsigned char B;
  unsigned char G;
  unsigned char R;
};

struct TRes
{
  int w, h;
};

struct TEXTURE
{
  unsigned short DataA[128*128];
  unsigned short DataB[64*64];
  unsigned short DataC[32*32];
  unsigned short DataD[16*16];
  unsigned short SDataC[2][32*32];
  int mR, mG, mB;
};

struct TPicture
{
  int W, H;
  unique_heap_ptr<unsigned short[]> lpImage;
};

struct TFogEntity
{
  int fogRGB;
  float YBegin;
  int Mortal;
  float Transp, FLimit;
};
