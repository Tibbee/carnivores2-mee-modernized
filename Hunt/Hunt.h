// Hunt.h — Umbrella header (legacy compatibility, will be removed)
// All declarations have been moved to focused headers in Core/.
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <type_traits>
#include <vector>
#include <array>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include "Memory.h"
#include "math.h"
#include "winuser.h"
#include "AppRes.h"
#include "ddraw.h"

#include "Core/Constants.h"
#include "Core/MathTypes.h"
#include "Core/AudioTypes.h"
#include "Core/RenderTypes.h"
#include "Core/ModelTypes.h"
#include "Core/GameTypes.h"
#include "Core/GameState.h"
#include "Core/GameMode.h"

#include "Renderer/RenderContext.h"
#include "Core/EngineAPI.h"

#ifdef _gl
#include "Renderer/GLRenderer.h"
#endif

#ifdef _d3d
#include "d3d.h"
#endif

// ----------------------------------------------------------------------------
// Phase 5 sizeof() verification (Gap #3 follow-up)
//
// These static_asserts lock the sizes of structs whose fields were migrated
// from raw pointers to smart pointers in Phase 5B.2. The checks prevent
// silent size regressions (e.g., if EBO fails on a unique_ptr alias, the
// struct would double in size). Expected values are for MSVC x86 with
// empty-base-optimization on HeapDeleter; adjust if the STL or compiler
// changes.
//
// See memory-system-migration.md §5 for the full verification protocol.
// ----------------------------------------------------------------------------

// TAni: 32 (aniName) + 3 ints (12) + unique_heap_ptr (4) = 48
static_assert(sizeof(TAni) == 48,
              "TAni size changed — verify unique_heap_ptr EBO is still active");

// TVTL: 3 ints (12) + unique_heap_ptr (4) = 16
static_assert(sizeof(TVTL) == 16,
              "TVTL size changed — verify unique_heap_ptr EBO is still active");

// TPicture: 2 ints (8) + unique_heap_ptr (4) = 12
static_assert(sizeof(TPicture) == 12,
              "TPicture size changed — verify unique_heap_ptr EBO is still active");

// TBMPModel: gVertex[4] (48) + unique_heap_ptr (4) = 52
static_assert(sizeof(TBMPModel) == 52,
              "TBMPModel size changed — verify unique_heap_ptr EBO is still active");

// TModel: 4 ints (16) + gVertex (4) + gFace union (4) + 3*lpTexture (12)
//         + VLight[4] (16) = 52 on both d3d and non-d3d (int* and float*
//         are both 4 bytes on x86)
static_assert(sizeof(TModel) == 52,
              "TModel size changed — this affects MObjects[256] layout and "
              "would shift the entire object array");

// TObject: contains TObjInfo + TBound[8] + TBMPModel + model + TVTL + ...
// Expected ~140 bytes. Range check catches catastrophic bloat without
// failing on minor alignment differences across toolchains.
static_assert(sizeof(TObject) >= 300 && sizeof(TObject) <= 400,
              "TObject size is outside expected range — MObjects[256] "
              "layout has shifted");

// TCharacterInfo: contains ModelName[32] + mptr + TAni[64] + TSFX[64] + ...
// The TAni[64] alone is 48*64 = 3072; TSFX[64] with vector is ~28*64 = 1792.
// Total is several KiB. Range check verifies no unexpected reordering.
static_assert(sizeof(TCharacterInfo) >= 3000 && sizeof(TCharacterInfo) <= 8000,
              "TCharacterInfo size is outside expected range — ChInfo[128] "
              "layout is affected");
