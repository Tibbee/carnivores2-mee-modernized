#include "Hunt.h"
#include "stdio.h"
#include "timeapi.h"

// Forward declarations for functions in ModelLoader.cpp
void LoadBMPModel(TObject &obj);
void LoadAnimation(TVTL &vtl);
void GenerateMapImage();

#ifdef MEM_DEBUG
#include <mutex>
#endif

#ifdef MEM_DEBUG
std::mutex g_AllocMutex;
std::map<void*, AllocationInfo>* g_Allocations = nullptr;
#endif

void GenerateModelMipMaps(TModel *mptr, MemoryTag tag);
void GenerateAlphaFlags(TModel *mptr);

// Phase 5A: the 3-arg _HeapAlloc is preserved as a one-line forwarder to
// the new 4-arg overload that takes a MemoryTag. Call sites can be migrated
// incrementally (per phase 5B-5E) by switching to the explicit 4-arg form
// with the appropriate tag.
//
// The default tag is MemoryTag::Global (heap allocation), NOT
// MemoryTag::Level. This is a safety fix from the Phase 5C.2 testing:
// with the arena wired in, any 3-arg call that lands in the arena gets
// its memory reclaimed by LevelArena->Reset() the next time a level
// loads. The visible symptoms were a garbled exit menu (ExitPic's
// pixel data was arena-owned and got freed between level loads and
// the Escape press) and a broken gun envmap in OpenGL (the envmap
// data is loaded once at startup and used every frame; if it's
// arena-owned, the first LevelArena->Reset() corrupts it). Changing
// the default to Global means unmigrated call sites go to the heap
// (safe, survive arena reset) and only the explicitly-tagged Level
// allocations land in the arena. The trade-off is that the arena is
// underutilized until the remaining 3-arg per-level call sites are
// audited in Phase 5E; that's a missed optimization, not a
// correctness issue.
LPVOID _HeapAlloc(HANDLE hHeap,
                  DWORD dwFlags,
                  DWORD dwBytes)
{
  return _HeapAlloc(hHeap, dwFlags, dwBytes, MemoryTag::Global);
}

// 4-arg _HeapAlloc: dispatches between arena and heap based on `tag`.
//   tag == MemoryTag::Level AND LevelArena != nullptr → allocate from
//     the per-level arena. The arena does not zero-initialize, so the
//     returned block is explicitly memset to 0 (matches C1's behavior;
//     the heap path gets HEAP_ZERO_MEMORY for the same effect).
//   otherwise → HeapAlloc on the game heap with HEAP_ZERO_MEMORY.
//
// HeapAllocated is incremented for ALL allocations (arena + heap),
// matching C1. The name is a misnomer — it's really "total bytes
// allocated" — but the counter feeds carnivor.log lines that downstream
// tooling may parse, so the accounting is preserved verbatim.
//
// In Phase 5A LevelArena is null so the arena branch is never taken;
// in Phase 5C InitEngine() will construct it and Level-tagged
// allocations will start landing in the arena.
LPVOID _HeapAlloc(HANDLE hHeap,
                  DWORD dwFlags,
                  DWORD dwBytes,
                  MemoryTag tag)
{
  LPVOID res = nullptr;

  if (tag == MemoryTag::Level && LevelArena != nullptr)
  {
    res = LevelArena->Allocate(dwBytes);
    if (res)
      memset(res, 0, dwBytes);
  }
  else
  {
    res = HeapAlloc(hHeap,
                    dwFlags | HEAP_ZERO_MEMORY,
                    dwBytes);
  }

  if (!res)
    DoHalt("Memory allocation error!");

  HeapAllocated += dwBytes;
  return res;
}

#ifdef MEM_DEBUG
// 5-arg _HeapAlloc (Phase 5F): additive overload that captures the
// call-site file/line for the leak report. Forwards the actual work to
// the 4-arg overload (the dispatch logic lives in one place) and then
// records the allocation in g_Allocations under g_AllocMutex.
//
// The map and mutex are allocated lazily and are themselves NOT
// tracked (recursive tracking would be unsafe; see the bootstrap note
// in Memory.h). The map entry is the source of truth for the leak
// report at shutdown.
LPVOID _HeapAlloc(HANDLE hHeap,
                  DWORD dwFlags,
                  DWORD dwBytes,
                  MemoryTag tag,
                  const char* file,
                  int line)
{
  std::lock_guard<std::mutex> lock(g_AllocMutex);
  if (!g_Allocations) g_Allocations = new std::map<void*, AllocationInfo>();

  LPVOID res = _HeapAlloc(hHeap, dwFlags, dwBytes, tag);

  (*g_Allocations)[res] = { (size_t)dwBytes, tag,
                            file ? file : "unknown", line };
  return res;
}
#endif

BOOL _HeapFree(HANDLE hHeap,
               DWORD  dwFlags,
               LPVOID lpMem)
{
  if (!lpMem) return false;

  // Phase 5F: remove the entry from the leak map (if recording is on)
  // before the pointer is freed. For Level-tagged allocations the
  // pointer is owned by the arena and never reaches HeapFree, but we
  // still want to remove its map entry so it doesn't show up as a
  // leak. The erase is a no-op for pointers that aren't tracked.
#ifdef MEM_DEBUG
  {
    std::lock_guard<std::mutex> lock(g_AllocMutex);
    if (g_Allocations) {
      auto it = g_Allocations->find(lpMem);
      if (it != g_Allocations->end()) {
        if (it->second.tag == MemoryTag::Level) {
          g_Allocations->erase(it);
          return TRUE;  // arena-owned: don't fall through to HeapFree
        }
        g_Allocations->erase(it);
      }
    }
  }
#endif

  // Phase 5A: arena allocations are silently ignored. The arena owns
  // them and will reclaim them in bulk on Reset() (LevelArena is null
  // in Phase 5A, so this check is always false and behavior is identical
  // to pre-5A).
  if (LevelArena != nullptr && LevelArena->Contains(lpMem))
    return true;

  HeapReleased+=
    HeapSize(hHeap, HEAP_NO_SERIALIZE, lpMem);

  BOOL res = HeapFree(hHeap,
                      dwFlags,
                      lpMem);
  if (!res)
    DoHalt("Heap free error!");

  return res;
}

#ifdef MEM_DEBUG
// Phase 5F: human-readable tag name for the leak report. C1 has the
// same function at Carnivores1/Hunt/Resources.cpp:97-107. Kept as a
// free function (not a method on MemoryTag) so it can be called from
// PrintMemoryLeaks without dragging the enum into a public header.
const char* MemoryTagToString(MemoryTag tag) {
    switch (tag) {
        case MemoryTag::Global:   return "Global";
        case MemoryTag::Level:    return "Level";
        case MemoryTag::Graphics: return "Graphics";
        case MemoryTag::Audio:    return "Audio";
        case MemoryTag::AI:       return "AI";
        case MemoryTag::Physics:  return "Physics";
        default:                  return "Unknown";
    }
}

// Phase 5F: walk g_Allocations and print every remaining entry (these
// are the leaks). Prints a per-tag summary at the end, then deletes
// the map. Called from Game.cpp ShutDownEngine() before delete
// LevelArena so the pointers in the report are still valid.
//
// Output goes to PrintLog, which writes to carnivor.log. The log is
// flushed by CloseLog() after the call returns (the doc explicitly
// warns about ordering: PrintMemoryLeaks must run before the log is
// closed, which it does because ShutDownEngine is called before
// CloseLog in WinMain's cleanup path).
void PrintMemoryLeaks()
{
    if (!g_Allocations || g_Allocations->empty()) {
        PrintLog("No memory leaks detected.\n");
        if (g_Allocations) {
            delete g_Allocations;
            g_Allocations = nullptr;
        }
        return;
    }

    char buf[512];
    sprintf(buf, "Memory leaks detected: %u blocks\n", (unsigned)g_Allocations->size());
    PrintLog(buf);

    std::map<MemoryTag, size_t> tagTotals;
    size_t total = 0;

    for (auto const& kv : *g_Allocations) {
        void* ptr = kv.first;
        const AllocationInfo& info = kv.second;
        sprintf(buf, "[%s] Leak: %p, size: %u, at %s:%d\n",
                MemoryTagToString(info.tag), ptr,
                (unsigned)info.size, info.file.c_str(), info.line);
        PrintLog(buf);
        tagTotals[info.tag] += info.size;
        total += info.size;
    }

    PrintLog("\nMemory leaks summary by category:\n");
    for (auto const& kv : tagTotals) {
        sprintf(buf, "  %-10s: %u bytes\n",
                MemoryTagToString(kv.first), (unsigned)kv.second);
        PrintLog(buf);
    }

    sprintf(buf, "Total leaked memory: %u bytes\n", (unsigned)total);
    PrintLog(buf);

    delete g_Allocations;
    g_Allocations = nullptr;
}

// Phase 5F: strip every entry with the given tag out of g_Allocations.
// Called from ReleaseResources() after LevelArena->Reset() so the
// per-level entries (which were arena-owned and just got bulk-freed)
// don't show up as leaks in the shutdown report. C1 does the same
// thing in its ReleaseResources.
//
// C1's version only clears MemoryTag::Level. We follow that -- the
// other tags don't have the same lifetime mismatch because the
// Global/Graphics/Audio/etc. allocations are _HeapFree'd explicitly
// by their owners.
void ClearTagAllocations(MemoryTag tag)
{
    if (!g_Allocations) return;
    std::lock_guard<std::mutex> lock(g_AllocMutex);
    for (auto it = g_Allocations->begin(); it != g_Allocations->end(); ) {
        if (it->second.tag == tag) {
            it = g_Allocations->erase(it);
        } else {
            ++it;
        }
    }
}
#endif // MEM_DEBUG

void AddMessage(LPSTR mt)
{
  MessageList.timeleft = timeGetTime() + 2 * 1000;
  lstrcpy(MessageList.mtext, mt);
}

void PlaceHunter()
{
  if (LockLanding) return;

  if (g_GameMode == GameMode::TrophyMode)
  {
    PlayerX = 76*256+128;
    PlayerZ = 70*256+128;
    PlayerY = GetLandQH(PlayerX, PlayerZ);
    return;
  }

  if (g_GameMode == GameMode::SurvivalMode) {
	  PlayerX = SurvivalSpawnX * 256 + 128;
	  PlayerZ = SurvivalSpawnZ * 256 + 128;
	  PlayerY = GetLandQH(PlayerX, PlayerZ);
	  return;
  }

  int p = (timeGetTime() % LandingList.PCount);
  PlayerX = static_cast<float>(LandingList.list[p].x) * 256+128;
  PlayerZ = static_cast<float>(LandingList.list[p].y) * 256+128;
  PlayerY = GetLandQH(PlayerX, PlayerZ);
}

void CreateWaterTab()
{
  for (int c=0; c<0x8000; c++)
  {
    int R = (c >> 10);
    int G = (c >>  5) & 31;
    int B = c & 31;
    R =  1+(R * 8 ) / 28;
    if (R>31) R=31;
    G =  2+(G * 18) / 28;
    if (G>31) G=31;
    B =  3+(B * 22) / 28;
    if (B>31) B=31;
    FadeTab[64][c] = HiColor(R, G, B);
  }
}

void CreateFadeTab()
{
#ifdef _soft
  for (int l=0; l<64; l++)
    for (int c=0; c<0x8000; c++)
    {
      int R = (c >> 10);
      int G = (c >>  5) & 31;
      int B = c & 31;

      R = static_cast<int>((static_cast<float>(R) * (l) / 60.f + static_cast<float>(rand()) *0.2f / RAND_MAX));
      if (R>31) R=31;
      G = static_cast<int>((static_cast<float>(G) * (l) / 60.f + static_cast<float>(rand()) *0.2f / RAND_MAX));
      if (G>31) G=31;
      B = static_cast<int>((static_cast<float>(B) * (l) / 60.f + static_cast<float>(rand()) *0.2f / RAND_MAX));
      if (B>31) B=31;
      FadeTab[l][c] = HiColor(R, G, B);
    }

  CreateWaterTab();
#endif
}

void CreateDivTable()
{
  DivTbl[0] = 0x7fffffff;
  DivTbl[1] = 0x7fffffff;
  DivTbl[2] = 0x7fffffff;
  for( int i = 3; i < 10240; i++ )
    DivTbl[i] = static_cast<int>((static_cast<float>(0x100000000) / i));

  for (int y=0; y<32; y++)
    for (int x=0; x<32; x++)
      RandomMap[y][x] = rand() * 1024 / RAND_MAX;
}

void CreateVideoDIB()
{
  CreateVideoDIB(WinW, WinH);
}

void CreateVideoDIB(int W, int H)
{
  if (hdcMain == nullptr) {
    hdcMain = GetDC(hwndMain);
    hdcCMain = CreateCompatibleDC(hdcMain);

    SelectObject(hdcMain,  fnt_Midd);
    SelectObject(hdcCMain, fnt_Midd);
  }

  if (hbmpVideoBuf) {
    DeleteObject(hbmpVideoBuf);
    hbmpVideoBuf = nullptr;
  }

  BITMAPINFOHEADER bmih;
  bmih.biSize = sizeof( BITMAPINFOHEADER );
  bmih.biWidth  = W;
  bmih.biHeight = -H;
  bmih.biPlanes = 1;
  bmih.biBitCount = 16;
  bmih.biCompression = BI_RGB;
  bmih.biSizeImage = 0;
  bmih.biXPelsPerMeter = 400;
  bmih.biYPelsPerMeter = 400;
  bmih.biClrUsed = 0;
  bmih.biClrImportant = 0;

  BITMAPINFO binfo;
  binfo.bmiHeader = bmih;
  hbmpVideoBuf =
    CreateDIBSection(hdcMain, &binfo, DIB_RGB_COLORS, &lpVideoBuf, nullptr, 0);
}

int GetObjectH(int x, int y, int R)
{
  x = (x<<8) + 128;
  y = (y<<8) + 128;
  float hr,h;
  hr =GetLandH(static_cast<float>(x),    static_cast<float>(y));
  h = GetLandH( static_cast<float>(x)+R, static_cast<float>(y));
  if (h < hr) hr = h;
  h = GetLandH( static_cast<float>(x)-R, static_cast<float>(y));
  if (h < hr) hr = h;
  h = GetLandH( static_cast<float>(x),   static_cast<float>(y)+R);
  if (h < hr) hr = h;
  h = GetLandH( static_cast<float>(x),   static_cast<float>(y)-R);
  if (h < hr) hr = h;
  hr += 15;
  return  static_cast<int>((hr / ctHScale));
}

int GetObjectHWater(int x, int y)
{
  if (FMap[y][x] & fmReverse)
    return static_cast<int>((HMap[y][x+1]+HMap[y+1][x])) / 2 + 48;
  else
    return static_cast<int>((HMap[y][x]+HMap[y+1][x+1])) / 2 + 48;
}

void CreateTMap()
{
  int x,y;
  LandingList.PCount = 0;
  for (y=0; y<ctMapSize; y++)
    for (x=0; x<ctMapSize; x++)
    {
      if (TMap1[y][x]==0xFFFF) TMap1[y][x] = 1;
      if (TMap2[y][x]==0xFFFF) TMap2[y][x] = 1;
    }

  /*
    for (y=1; y<ctMapSize-1; y++)
       for (x=1; x<ctMapSize-1; x++)
  		 if (!(FMap[y][x] & fmWater) ) {

  			 if (FMap[y  ][x+1] & fmWater) { FMap[y][x]|= fmWater2; WMap[y][x] = WMap[y  ][x+1];}
  			 if (FMap[y+1][x  ] & fmWater) { FMap[y][x]|= fmWater2; WMap[y][x] = WMap[y+1][x  ];}
  			 if (FMap[y  ][x-1] & fmWater) { FMap[y][x]|= fmWater2; WMap[y][x] = WMap[y  ][x-1];}
  			 if (FMap[y-1][x  ] & fmWater) { FMap[y][x]|= fmWater2; WMap[y][x] = WMap[y-1][x  ];}

  			 if (FMap[y][x] & fmWater2)
  			     if (HMap[y][x] > WaterList[WMap[y][x]].wlevel) HMap[y][x]=WaterList[WMap[y][x]].wlevel;
  		 }

    for (y=1; y<ctMapSize-1; y++)
       for (x=1; x<ctMapSize-1; x++)
  		 if (FMap[y][x] & fmWater2) {
  			 FMap[y][x]-=fmWater2;
  			 FMap[y][x]+=fmWater;
  		 }
  */

  for (y=1; y<ctMapSize-1; y++)
    for (x=1; x<ctMapSize-1; x++)
      if (!(FMap[y][x] & fmWater) )
      {

        if (FMap[y  ][x+1] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y  ][x+1];
        }
        if (FMap[y+1][x  ] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y+1][x  ];
        }
        if (FMap[y  ][x-1] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y  ][x-1];
        }
        if (FMap[y-1][x  ] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y-1][x  ];
        }

        BOOL l = true;

#ifdef _soft
        if (FMap[y][x] & fmWater2)
        {
          l = false;
          if (HMap[y][x] > WaterList[WMap[y][x]].wlevel) HMap[y][x]=WaterList[WMap[y][x]].wlevel;
          HMap[y][x]=WaterList[WMap[y][x]].wlevel;
        }
#endif

        if (FMap[y-1][x-1] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y-1][x-1];
        }
        if (FMap[y-1][x+1] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y-1][x+1];
        }
        if (FMap[y+1][x-1] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y+1][x-1];
        }
        if (FMap[y+1][x+1] & fmWater)
        {
          FMap[y][x]|= fmWater2;
          WMap[y][x] = WMap[y+1][x+1];
        }

        if (l)
          if (FMap[y][x] & fmWater2)
            if (HMap[y][x] == WaterList[WMap[y][x]].wlevel) HMap[y][x]+=1;

        //if (FMap[y][x] & fmWater2)

      }

#ifdef _soft
  for (y=0; y<1024; y++)
    for (x=0; x<1024; x++ )
    {
      if( abs( HMap[y][x]-HMap[y+1][x+1] ) > abs( HMap[y+1][x]-HMap[y][x+1] ) )
        FMap[y][x] |= fmReverse;
      else
        FMap[y][x] &= ~fmReverse;
    }
#endif

  for (y=0; y<ctMapSize; y++)
    for (x=0; x<ctMapSize; x++)
    {

      if (!(FMap[y][x] & fmWaterA))
        WMap[y][x]=255;

#ifdef _soft
      if (MObjects[OMap[y][x]].info.flags & ofNOSOFT2)
        if ( (x+y) & 1 )
          OMap[y][x]=255;

      if (MObjects[OMap[y][x]].info.flags & ofNOSOFT)
        OMap[y][x]=255;
#endif

      if (OMap[y][x]==254)
      {
        LandingList.list[LandingList.PCount].x = x;
        LandingList.list[LandingList.PCount].y = y;
        LandingList.PCount++;
		//MessageBox(hwndMain, "FOUND A LANDER!", "Woah wee", IDOK);
        OMap[y][x]=255;
      }

      int ob = OMap[y][x];
      if (ob == 255)
      {
        HMapO[y][x] = 0;
        continue;
      }

      //HMapO[y][x] = GetObjectH(x,y);
      if (MObjects[ob].info.flags & ofPLACEGROUND) HMapO[y][x] = GetObjectH(x,y, MObjects[ob].info.GrRad);
      //if (MObjects[ob].info.flags & ofPLACEWATER)  HMapO[y][x] = GetObjectHWater(x,y);

    }

  if (!LandingList.PCount && g_GameMode != GameMode::TrophyMode)
  {
	//MessageBox(hwndMain, "URRRR WHAT?", "Woah what the fuck", IDOK);
    LandingList.list[LandingList.PCount].x = 256;
    LandingList.list[LandingList.PCount].y = 256;
    LandingList.PCount=1;
  }

  /*
  if (g_GameMode == GameMode::TrophyMode)
  {
    LandingList.PCount = 0;
    for (x=0; x<6; x++)
    {
      LandingList.list[LandingList.PCount].x = 69 + x*3;
      LandingList.list[LandingList.PCount].y = 66;
      LandingList.PCount++;
    }

    for (y=0; y<6; y++)
    {
      LandingList.list[LandingList.PCount].x = 87;
      LandingList.list[LandingList.PCount].y = 69 + y*3;
      LandingList.PCount++;
    }

    for (x=0; x<6; x++)
    {
      LandingList.list[LandingList.PCount].x = 84 - x*3;
      LandingList.list[LandingList.PCount].y = 87;
      LandingList.PCount++;
    }

    for (y=0; y<6; y++)
    {
      LandingList.list[LandingList.PCount].x = 66;
      LandingList.list[LandingList.PCount].y = 84 - y*3;
      LandingList.PCount++;
    }
  }
  */

}

void LoadWav(char* FName, TSFX &sfx)
{
  DWORD l;

  HANDLE hfile = CreateFile(FName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if( hfile==INVALID_HANDLE_VALUE )
  {
    char sz[512];
    sprintf_s(sz, sizeof(sz), "Error opening file\n%s.", FName );
    DoHalt(sz);
  }

  // Phase 5B.1: sfx.lpData is now std::vector<short int>, so the previous
  // manual _HeapFree is replaced by the vector's own destructor (handled
  // implicitly when the vector is reassigned/resized below). The nullptr
  // reset is also unnecessary.
  SetFilePointer( hfile, 36, nullptr, FILE_BEGIN );

  char c[5];
  c[4] = 0;

  for ( ; ; )
  {
    ReadFile( hfile, c, 1, &l, nullptr );
    if( c[0] == 'd' )
    {
      ReadFile( hfile, &c[1], 3, &l, nullptr );
      if( !lstrcmp( c, "data" ) ) break;
      else SetFilePointer( hfile, -3, nullptr, FILE_CURRENT );
    }
  }

  ReadFile( hfile, &sfx.length, 4, &l, nullptr );

  // sfx.length is in bytes; std::vector is element-counted. Round down to
  // whole short ints (WAV data is always 16-bit, so this is exact in
  // practice). resize() value-initializes new elements to zero, matching
  // the HEAP_ZERO_MEMORY behavior of the previous _HeapAlloc call.
  const size_t sampleCount = sfx.length / sizeof(short int);
  sfx.lpData.assign(sampleCount, 0);
  ReadFile( hfile, sfx.lpData.data(), sfx.length, &l, nullptr );
  CloseHandle(hfile);
}

WORD conv_565(WORD c)
{
  return (c & 31) + ( (c & 0xFFE0) << 1 );
}

int conv_xGx(int c)
{
  if (!NightVisionOn) return c;
  DWORD a = c;
  int r = ((c>> 0) & 0xFF);
  int g = ((c>> 8) & 0xFF);
  int b = ((c>>16) & 0xFF);
  c = MAX(r,g);
  c = MAX(c,b);
  return (c<<8) + (a & 0xFF000000);
}

void conv_pic(TPicture &pic)
{
  if (!HARD3D) return;
  for (int y=0; y<pic.H; y++)
    for (int x=0; x<pic.W; x++)
      *(pic.lpImage.get() + x + y*pic.W) = conv_565(*(pic.lpImage.get() + x + y*pic.W));
}

void LoadPicture(TPicture &pic, LPSTR pname, MemoryTag tag)
{
  int C;
  byte fRGB[800][3];
  BITMAPFILEHEADER bmpFH;
  BITMAPINFOHEADER bmpIH;
  DWORD l;
  HANDLE hfile;

  hfile = CreateFile(pname, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
  if( hfile==INVALID_HANDLE_VALUE )
  {
    char sz[512];
    sprintf_s(sz, sizeof(sz), "Error opening file\n%s.", pname );
    DoHalt(sz);
  }

  ReadFile( hfile, &bmpFH, sizeof( BITMAPFILEHEADER ), &l, nullptr );
  ReadFile( hfile, &bmpIH, sizeof( BITMAPINFOHEADER ), &l, nullptr );

  pic.lpImage.reset();
  pic.lpImage = nullptr;

  pic.W = bmpIH.biWidth;
  pic.H = bmpIH.biHeight;
  pic.lpImage.reset(static_cast<WORD*>(_HeapAlloc(Heap, 0, pic.W * pic.H * 2, tag)));

  for (int y=0; y<pic.H; y++)
  {
    ReadFile( hfile, fRGB, 3*pic.W, &l, nullptr );
    for (int x=0; x<pic.W; x++)
    {
      C = (static_cast<int>(fRGB[x][2])/8<<10) + (static_cast<int>(fRGB[x][1])/8<< 5) + (static_cast<int>(fRGB[x][0])/8) ;
      *(pic.lpImage.get() + (pic.H-y-1)*pic.W+x) = C;
    }
  }

  CloseHandle( hfile );
}

void LoadPictureTGA(TPicture &pic, LPSTR pname, MemoryTag tag)
{
  DWORD l;
  WORD w,h;
  HANDLE hfile;

  hfile = CreateFile(pname, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
  if( hfile==INVALID_HANDLE_VALUE )
  {
    char sz[512];
    sprintf_s(sz, sizeof(sz), "Error opening file\n%s.", pname );
    DoHalt(sz);
  }

  SetFilePointer(hfile, 12, 0, FILE_BEGIN);

  ReadFile( hfile, &w, 2, &l, nullptr );
  ReadFile( hfile, &h, 2, &l, nullptr );

  SetFilePointer(hfile, 18, 0, FILE_BEGIN);

  pic.lpImage.reset();
  pic.lpImage = nullptr;

  pic.W = w;
  pic.H = h;
  pic.lpImage.reset(static_cast<WORD*>(_HeapAlloc(Heap, 0, pic.W * pic.H * 2, tag)));

  for (int y=0; y<pic.H; y++)
    ReadFile( hfile, (void*)(pic.lpImage.get() + (pic.H-y-1)*pic.W), 2*pic.W, &l, nullptr );

  CloseHandle( hfile );
}



void ReleaseResources()
{
  HeapReleased=0;

  // Release per-level objects before rewinding the arena. _HeapFree() knows
  // how to ignore arena-owned pointers, and this also covers Phase 5A builds
  // where LevelArena is null and the allocations really are heap-backed.
  for (int t=0; t<1024; t++)
    if (Textures[t].get())
    {
      Textures[t].reset();
      Textures[t] = nullptr;
    }
    else break;

  for (int m=0; m<255; m++)
  {
    TModel *mptr = MObjects[m].model.get();
    if (mptr)
    {
      MObjects[m].bmpmodel.lpTexture.reset();
      MObjects[m].bmpmodel.lpTexture = nullptr;

      if (MObjects[m].vtl.FramesCount>0)
      {
        MObjects[m].vtl.aniData.reset();
        MObjects[m].vtl.aniData = nullptr;
      }

      // Remove GL texture cache entry before freeing the TModel.
      ReleaseModelTexture(mptr);

      mptr->lpTexture.reset();
      mptr->lpTexture  = nullptr;
      mptr->lpTexture2.reset();
      mptr->lpTexture2 = nullptr;
      mptr->lpTexture3.reset();
      mptr->lpTexture3 = nullptr;

      // gFace and VLight[0] are raw heap pointers allocated by
      // AllocateMemoryForModel. They are not managed by unique_ptr
      // (~TModel() is default) and must be freed explicitly before
      // the model is destroyed.
      if (mptr->gFace) {
        (void)_HeapFree(Heap, 0, mptr->gFace);
        mptr->gFace = nullptr;
      }
      if (mptr->VLight[0]) {
        (void)_HeapFree(Heap, 0, mptr->VLight[0]);
        mptr->VLight[0] = nullptr;
        mptr->VLight[1] = nullptr;
        mptr->VLight[2] = nullptr;
        mptr->VLight[3] = nullptr;
      }

      MObjects[m].model.reset();
      MObjects[m].model = nullptr;
      MObjects[m].vtl.FramesCount = 0;
    }
    else break;
  }

  // Phase 5B.1: TSFX::lpData is now std::vector<short int>; the vector
  // destructor reclaims the buffer on Reset. The presence check is also
  // updated to use the vector's empty()/size() rather than a null pointer.
  for (int a=0; a<255; a++)
  {
    if (Ambient[a].sfx.lpData.empty()) break;
    Ambient[a].sfx.lpData.clear();
  }

  for (int r=0; r<255; r++)
  {
    if (RandSound[r].lpData.empty()) break;
    RandSound[r].lpData.clear();
    RandSound[r].length = 0;
  }

  // Per-level UI pictures and weapon scratch buffers loaded from the
  // current .rsc/map pass.
  MapPic.lpImage.reset();
  TrophyPic.lpImage.reset();
  TrophyNoCollectPic.lpImage.reset();
  ScorePic.lpImage.reset();
  for (int i=0; i<4; i++)
    Weapon.Flash[i].lpImage.reset();
  for (int i=0; i<10; i++)
  {
    Weapon.BulletPic[i].lpImage.reset();
    Weapon.ChambPic[i].lpImage.reset();
  }
  for (int i=0; i<16; i++)
    MenuDinoInfo[i].CallIcon.lpImage.reset();

  Weapon.normals.reset();

  // Raw per-level render scratch buffers. _HeapFree() ignores arena-owned
  // pointers and frees them when LevelArena is null (Phase 5A compatibility).
  if (rVertex)
  {
    (void)_HeapFree(Heap, 0, rVertex);
    rVertex = nullptr;
  }
  if (gScrp)
  {
    (void)_HeapFree(Heap, 0, gScrp);
    gScrp = nullptr;
  }
  if (PhongMapping)
  {
    (void)_HeapFree(Heap, 0, PhongMapping);
    PhongMapping = nullptr;
  }

  // Clear the GL renderer's per-level caches now that all per-level
  // objects have been released. This prevents unbounded VRAM growth
  // (model textures, BMP textures, static geometry VBO/IBO offsets)
  // and ensures the terrain texture upload cache is fresh for the next
  // level. C1 does the same in its ReleaseResources via
  // renderer->ClearLevelTextureCache() and ResetTerrainTextureCache().
  ClearRendererLevelCache();
  ClearRendererTerrainCache();

  // Phase 5F.2: reset the per-level arena after per-level owners have
  // dropped their pointers. Keep the MEM_DEBUG cleanup after the reset so
  // the arena still owns the addresses while the leak map is pruned.
  if (LevelArena != nullptr) {
    LevelArena->Reset();
  }

#ifdef MEM_DEBUG
  if (LevelArena != nullptr) {
    ClearTagAllocations(MemoryTag::Level);
  }
#endif
}

void ReleaseGlobalResources()
{
  // Phase 5C.1: Release the global (cross-level) resources that the menu
  // and base game load once at startup. Without this, every Quit leaks the
  // weapon character info, the Sun/Compass/Binocular models, the menu
  // pictures, and the per-character global allocations in ChInfo[].
  //
  // Mirrors C1's ReleaseGlobalResources in Carnivores1/Hunt/Resources.cpp
  // (lines 1220-1256), adapted for C2 ME's larger ChInfo[128] array and
  // the absence of the menu SFX globals (fxMenuGo/Mov/Amb -- C2 ME handles
  // menu audio differently and doesn't have them as globals).

  for (int c = 0; c < DINOINFO_MAX; c++)
  {
    ReleaseCharacterInfo(ChInfo[c]);
  }

  ReleaseCharacterInfo(ShipModel);
  ReleaseCharacterInfo(SShipModel);
  ReleaseCharacterInfo(WindModel);

  for (int w = 0; w < 10; w++)
  {
    ReleaseCharacterInfo(Weapon.chinfo[w]);
  }

  ReleaseModel(SunModel);
  ReleaseModel(CompasModel);
  ReleaseModel(Binocular);

  // Menu pictures -- unique_heap_ptr<WORD[]>, so .reset() is enough.
  PausePic.lpImage.reset();
  ExitPic.lpImage.reset();
  TrophyExit.lpImage.reset();
  TrophyPic.lpImage.reset();
  TrophyNoCollectPic.lpImage.reset();
  ScorePic.lpImage.reset();
  LandPic.lpImage.reset();
  DinoPic.lpImage.reset();
  DinoPicM.lpImage.reset();
  MapPic.lpImage.reset();
  WepPic.lpImage.reset();

  // OpenGL/D3D/3DFX effect lookup textures loaded once in WinMain().
  TFX_SPECULAR.lpImage.reset();
  TFX_ENVMAP.lpImage.reset();

  // The "null" texture (index 255) is the one InitEngine allocates
  // before any level loads; it's never reset by ReleaseResources()
  // because ReleaseResources iterates from 0 and breaks on the first
  // null pointer. Release it here.
  Textures[255].reset();
}

void LoadResources()
{

  int  FadeRGB[3][3];
  int TransRGB[3][3];

  int tc,mc;
  char MapName[128],RscName[128];
  HeapAllocated=0;
  if (strstr(ProjectName, "trophy"))
  {
    g_GameMode = GameMode::TrophyMode;
    ctViewR = 60;
    ctViewR1 = ctViewR;
  }
  sprintf_s(MapName, sizeof(MapName),"%s%s", ProjectName, ".map");
  sprintf_s(RscName, sizeof(RscName),"%s%s", ProjectName, ".rsc");

  ReleaseResources();

  hfile = CreateFile(RscName,
                     GENERIC_READ, FILE_SHARE_READ,
                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hfile==INVALID_HANDLE_VALUE)
  {
    char sz[512];
    sprintf_s(sz, sizeof(sz), "Error opening resource file\n%s.", RscName );
    DoHalt(sz);
    return;
  }

  ReadFile(hfile, &tc, 4, &l, nullptr);
  ReadFile(hfile, &mc, 4, &l, nullptr);

  ReadFile(hfile,  FadeRGB, 4*3*3, &l, nullptr);
  ReadFile(hfile, TransRGB, 4*3*3, &l, nullptr);

  SkyR  =  FadeRGB[OptDayNight][0];
  SkyG  =  FadeRGB[OptDayNight][1];
  SkyB  =  FadeRGB[OptDayNight][2];

  SkyTR = TransRGB[OptDayNight][0];
  SkyTG = TransRGB[OptDayNight][1];
  SkyTB = TransRGB[OptDayNight][2];

  if (OptDayNight==2)
  {
    SkyR = 0;
    SkyB = 0;
    SkyTR = 0;
    SkyTB = 0;
  }

  SkyTR = MIN(255,SkyTR * (OptBrightness + 128) / 256);
  SkyTG = MIN(255,SkyTG * (OptBrightness + 128) / 256);
  SkyTB = MIN(255,SkyTB * (OptBrightness + 128) / 256);

  SkyR = MIN(255,SkyR * (OptBrightness + 128) / 256);
  SkyG = MIN(255,SkyG * (OptBrightness + 128) / 256);
  SkyB = MIN(255,SkyB * (OptBrightness + 128) / 256);

  PrintLog("Loading textures:");
  for (int tt=0; tt<tc; tt++)
    LoadTexture(Textures[tt]);
  PrintLog(" Done.\n");

  PrintLog("Loading models:");
  PrintLoad("Loading models...");
  for (int mm=0; mm<mc; mm++)
  {
    ReadFile(hfile, &MObjects[mm].info, 64, &l, nullptr);
    MObjects[mm].info.Radius*=2;
    MObjects[mm].info.YLo*=2;
    MObjects[mm].info.YHi*=2;
    MObjects[mm].info.linelenght = (MObjects[mm].info.linelenght / 128) * 128;
    LoadModel(MObjects[mm].model);
    LoadBMPModel(MObjects[mm]);

    if (MObjects[mm].info.flags & ofNOLIGHT)
    {
        FillMemory(MObjects[mm].model->VLight[0], 4 * MObjects[mm].model->VCount, 0);
        FillMemory(MObjects[mm].model->VLight[1], 4 * MObjects[mm].model->VCount, 0);
        FillMemory(MObjects[mm].model->VLight[2], 4 * MObjects[mm].model->VCount, 0);
        FillMemory(MObjects[mm].model->VLight[3], 4 * MObjects[mm].model->VCount, 0);
    }

    if (MObjects[mm].info.flags & ofANIMATED)
      LoadAnimation(MObjects[mm].vtl);

    MObjects[mm].info.BoundR = 0;
    for (int v=0; v<MObjects[mm].model->VCount; v++)
    {
      float r = static_cast<float>(sqrt(MObjects[mm].model->gVertex[v].x * MObjects[mm].model->gVertex[v].x +
                            MObjects[mm].model->gVertex[v].z * MObjects[mm].model->gVertex[v].z ));
      if (r>MObjects[mm].info.BoundR) MObjects[mm].info.BoundR=r;
    }

    if (MObjects[mm].info.flags & ofBOUND)
      CalcBoundBox(MObjects[mm].model.get(), MObjects[mm].bound);

    GenerateModelMipMaps(MObjects[mm].model.get(), MemoryTag::Level);
    GenerateAlphaFlags(MObjects[mm].model.get());
  }
  PrintLog(" Done.\n");

  PrintLoad("Finishing with .res...");
  PrintLog("Finishing with .res:");
  LoadSky();
  LoadSkyMap();

  int FgCount;
  ReadFile(hfile, &FgCount, 4, &l, nullptr);
  ReadFile(hfile, &FogsList[1], FgCount * sizeof(TFogEntity), &l, nullptr);

  for (int f=0; f<=FgCount; f++)
  {
    int fb = (FogsList[f].fogRGB >> 00) & 0xFF;
    int fg = (FogsList[f].fogRGB >>  8) & 0xFF;
    int fr = (FogsList[f].fogRGB >> 16) & 0xFF;
#ifdef _d3d
    FogsList[f].fogRGB = (fr) + (fg<<8) + (fb<<16);
#endif
    // Night vision green fog tint removed — handled by per-frame overlay
  }

  int RdCount, AmbCount, WtrCount;

  ReadFile(hfile, &RdCount, 4, &l, nullptr);
  for (int r=0; r<RdCount; r++)
  {
    ReadFile(hfile, &RandSound[r].length, 4, &l, nullptr);
    // Phase 5B.1: lpData is now std::vector<short int>. assign() value-
    // initializes to zero (matches the previous HEAP_ZERO_MEMORY behavior).
    const size_t sampleCount = RandSound[r].length / sizeof(short int);
    RandSound[r].lpData.assign(sampleCount, 0);
    ReadFile(hfile, RandSound[r].lpData.data(), RandSound[r].length, &l, nullptr);
  }

  ReadFile(hfile, &AmbCount, 4, &l, nullptr);
  for (int a=0; a<AmbCount; a++)
  {
    ReadFile(hfile, &Ambient[a].sfx.length, 4, &l, nullptr);
    const size_t ambSampleCount = Ambient[a].sfx.length / sizeof(short int);
    Ambient[a].sfx.lpData.assign(ambSampleCount, 0);
    ReadFile(hfile, Ambient[a].sfx.lpData.data(), Ambient[a].sfx.length, &l, nullptr);

    ReadFile(hfile, Ambient[a].rdata, sizeof(Ambient[a].rdata), &l, nullptr);
    ReadFile(hfile, &Ambient[a].RSFXCount, 4, &l, nullptr);
    ReadFile(hfile, &Ambient[a].AVolume, 4, &l, nullptr);

    if (Ambient[a].RSFXCount)
      Ambient[a].RndTime = (Ambient[a].rdata[0].RFreq / 2 + rRand(Ambient[a].rdata[0].RFreq)) * 1000;

    int F = Ambient[a].rdata[0].RFreq;
    int E = Ambient[a].rdata[0].REnvir;
/////////////////

    //sprintf_s(logt, sizeof(logt),"Env=%d  Flag=%d  Freq=%d\n", E, Ambient[a].rdata[0].Flags, F);
    //PrintLog(logt);

    if (OptDayNight==2)
      for (int r=0; r<Ambient[a].RSFXCount; r++)
        if (Ambient[a].rdata[r].Flags)
        {
          if (r!=15) memcpy(&Ambient[a].rdata[r], &Ambient[a].rdata[r+1], (15-r)*sizeof(TRD));
          Ambient[a].RSFXCount--;
          r--;
        }

    Ambient[a].rdata[0].RFreq = F;
    Ambient[a].rdata[0].REnvir = E;

  }

  ReadFile(hfile, &WtrCount, 4, &l, nullptr);
  ReadFile(hfile, WaterList, 16*WtrCount, &l, nullptr);

  WaterList[255].wlevel = 0;
  for (int w=0; w<WtrCount; w++)
  {
#ifdef _3dfx
    WaterList[w].fogRGB = (Textures[WaterList[w].tindex]->mR) +
                          (Textures[WaterList[w].tindex]->mG<<8) +
                          (Textures[WaterList[w].tindex]->mB<<16);
#else
    WaterList[w].fogRGB = (Textures[WaterList[w].tindex]->mB) +
                          (Textures[WaterList[w].tindex]->mG<<8) +
                          (Textures[WaterList[w].tindex]->mR<<16);
#endif
  }
  CloseHandle(hfile);
  PrintLog(" Done.\n");

//================ Load MAPs file ==================//
  PrintLoad("Loading .map...");
  PrintLog("Loading .map:");
  hfile = CreateFile(MapName,
                     GENERIC_READ, FILE_SHARE_READ,
                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hfile==INVALID_HANDLE_VALUE)
    DoHalt("Error opening map file.");

  ReadFile(hfile, HMap,    1024*1024, &l, nullptr);
  ReadFile(hfile, TMap1,   1024*1024*2, &l, nullptr);
  ReadFile(hfile, TMap2,   1024*1024*2, &l, nullptr);
  ReadFile(hfile, OMap,    1024*1024, &l, nullptr);
  ReadFile(hfile, FMap,    1024*1024*2, &l, nullptr);
  SetFilePointer(hfile, 1024*1024*OptDayNight, nullptr, FILE_CURRENT);
  ReadFile(hfile, LMap,    1024*1024, &l, nullptr);
  SetFilePointer(hfile, 1024*1024*(2-OptDayNight), nullptr, FILE_CURRENT);
  ReadFile(hfile, WMap,   1024*1024, &l, nullptr);
  ReadFile(hfile, HMapO,   1024*1024, &l, nullptr);
  ReadFile(hfile, FogsMap, 512*512, &l, nullptr);
  ReadFile(hfile, AmbMap,  512*512, &l, nullptr);

  if (FogsList[1].YBegin>1.f)
    for (int x=0; x<510; x++)
      for (int y=0; y<510; y++)
        if (!FogsMap[y][x])
          if (HMap[y*2+0][x*2+0]<FogsList[1].YBegin || HMap[y*2+0][x*2+1]<FogsList[1].YBegin || HMap[y*2+0][x*2+2] < FogsList[1].YBegin ||
              HMap[y*2+1][x*2+0]<FogsList[1].YBegin || HMap[y*2+1][x*2+1]<FogsList[1].YBegin || HMap[y*2+1][x*2+2] < FogsList[1].YBegin ||
              HMap[y*2+2][x*2+0]<FogsList[1].YBegin || HMap[y*2+2][x*2+1]<FogsList[1].YBegin || HMap[y*2+2][x*2+2] < FogsList[1].YBegin)
            FogsMap[y][x] = 1;

  CloseHandle(hfile);
  PrintLog(" Done.\n");

//======= Post load rendering ==============//
  PrintLoad("Prepearing maps...");
  CreateTMap();
  RenderLightMap();

  LoadPictureTGA(MapPic, "HUNTDAT\\MENU\\mapframe.tga", MemoryTag::Level);
  conv_pic(MapPic);

  GenerateMapImage();

  for (int i = 0; i < 4;i++) {
	  char buff[100];
	  sprintf(buff, "HUNTDAT\\WEAPONS\\flash%i.tga", i+1);
	  LoadPictureTGA(Weapon.Flash[i], buff, MemoryTag::Level);
	  conv_pic(Weapon.Flash[i]);
  }

  if (g_GameMode == GameMode::TrophyMode) LoadPictureTGA(TrophyPic, "HUNTDAT\\MENU\\trophy.tga", MemoryTag::Level);
  else {
	  LoadPictureTGA(TrophyPic, "HUNTDAT\\MENU\\collect.tga", MemoryTag::Level);
	  LoadPictureTGA(TrophyNoCollectPic, "HUNTDAT\\MENU\\trophy_g.tga", MemoryTag::Level);
	  conv_pic(TrophyNoCollectPic);
  }
  conv_pic(TrophyPic);
  LoadPictureTGA(ScorePic, "HUNTDAT\\MENU\\score.tga", MemoryTag::Level);
  conv_pic(ScorePic);

//    ReInitGame();
}

void LoadCharacters()
{
  BOOL pres[DINOINFO_MAX];
  FillMemory(pres, sizeof(pres), 0);
  pres[0]=true;
  for (int c=0; c<ChCount; c++)
  {
    pres[Characters[c].CType] = true;
  }

  for (int c=0; c<TotalC; c++) if (pres[c] || (g_GameMode == GameMode::SurvivalMode && DinoInfo[c].survivalDino))
    {

      if (!ChInfo[c].mptr)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\%s", DinoInfo[c].FName);
        LoadCharacterInfo(ChInfo[c], logt);
        PrintLog("Loading: ");
        PrintLog(logt);
        PrintLog("\n");
      }

    }

  for (int c=10; c<20; c++)
    if (TargetDino & (1<<c))
      if (!MenuDinoInfo[c-10].CallIcon.lpImage)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\MENU\\PICS\\call%d.tga", c-9);
        LoadPictureTGA(MenuDinoInfo[c - 10].CallIcon, logt, MemoryTag::Level);
        conv_pic(MenuDinoInfo[c - 10].CallIcon);
      }

  // Keep track of max VCount for the weapons available
  int maxWeaponVCount = 0;
  for (int c=0; c<TotalW; c++)
    if (WeaponPres & (1<<c))
    {
      if (!Weapon.chinfo[c].mptr)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].FName);
        LoadCharacterInfo(Weapon.chinfo[c], logt);
        PrintLog("Loading: ");
        PrintLog(logt);
        PrintLog("\n");
      }

	  if (WeapInfo[c].bullet) {
		  sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].BLName);
		  LoadCharacterInfo(Weapon.Bullet[c], logt);
		  PrintLog("Loading: ");
		  PrintLog(logt);
		  PrintLog("\n");
	  }

	  maxWeaponVCount = MAX(Weapon.chinfo[c].mptr->VCount, maxWeaponVCount);

      if (!Weapon.BulletPic[c].lpImage)
      {
        sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].BFName);
        LoadPictureTGA(Weapon.BulletPic[c], logt, MemoryTag::Level);
        conv_pic(Weapon.BulletPic[c]);
        PrintLog("Loading: ");
        PrintLog(logt);
        PrintLog("\n");
      }

	  if (!Weapon.ChambPic[c].lpImage && WeapInfo[c].picch)
	  {
		  sprintf_s(logt, sizeof(logt), "HUNTDAT\\WEAPONS\\%s", WeapInfo[c].CFName);
		  LoadPictureTGA(Weapon.ChambPic[c], logt, MemoryTag::Level);
		  conv_pic(Weapon.ChambPic[c]);
		  PrintLog("Loading: ");
		  PrintLog(logt);
		  PrintLog("\n");
	  }

	    if (WeapInfo[c].MGSSound) {
			sprintf_s(logt, sizeof(logt), "MULTIPLAYER\\GUNSHOTS\\%s", WeapInfo[c].SFXName);
			LoadWav(logt, fxGunShot[c]);
			WeapInfo[c].SFXIndex = c;
		  } else WeapInfo[c].SFXIndex = -1;
	  
    }

  // Allocate space for normals fitting all available weapons. This is
  // a per-level allocation -- it's reallocated in LoadResources() on
  // each level load, and the old one is dropped by LevelArena->Reset()
  // in ReleaseResources(). Tag it Level so the arena holds it.
  Weapon.normals.reset((Vector3d*)_HeapAlloc(Heap, 0, sizeof(Vector3d) * maxWeaponVCount, MemoryTag::Level));

  for (int c=10; c<20; c++)
    if (TargetDino & (1<<c))
      if (fxCall[c-10][0].lpData.empty())
      {
        sprintf_s(logt, sizeof(logt),"HUNTDAT\\SOUNDFX\\CALLS\\call%d_a.wav", (c-9));
        LoadWav(logt, fxCall[c-10][0]);
        sprintf_s(logt, sizeof(logt),"HUNTDAT\\SOUNDFX\\CALLS\\call%d_b.wav", (c-9));
        LoadWav(logt, fxCall[c-10][1]);
        sprintf_s(logt, sizeof(logt),"HUNTDAT\\SOUNDFX\\CALLS\\call%d_c.wav", (c-9));
        LoadWav(logt, fxCall[c-10][2]);
      }

  sprintf_s(logt, sizeof(logt), "MULTIPLAYER\\AVATARS\\Hitbox.car");
  LoadCharacterInfo(HitBoxModel, logt);
  PrintLog("Loading: ");
  PrintLog(logt);
  PrintLog("\n");

  //multiplayer hunter models
  //test - 1 other player
  //test - add custom models at some point?
  if (Multiplayer) {
	  sprintf_s(logt, sizeof(logt), "MULTIPLAYER\\AVATARS\\Poacher.car");
	  LoadCharacterInfo(MPlayerInfo[0], logt);
	  PrintLog("Loading: ");
	  PrintLog(logt);
	  PrintLog("\n");
  }

  // Phase 5F.2: print per-level arena stats now that every per-level
  // allocation has been made. C1 has the same call at
  // Carnivores1/Hunt/Resources.cpp:1501. Useful for two things:
  //   1. Spotting memory-hungry levels (peak usage vs capacity).
  //   2. Verifying the arena reset works between level transitions
  //      (if the alloc count keeps growing across reloads, the reset
  //      is broken).
  // No-op in non-MEM_DEBUG builds (LogStats is a plain method, not
  // macro-gated, but the call site is the only place that needs the
  // report and it's useful even in release for production tuning).
  if (LevelArena != nullptr) {
    LevelArena->LogStats(" after load");
  }
}

void resetSSHip() {
	SShip.State = 0;
	SShip.alpha = 0;
	SShip.speed = 0;
	AmmoBag.State = 0;
}

void resetBullets() {
	for (int b = 0; b < bulletCh; b++) {
		bullet[b] = {};
	}
	bulletCh = 0;
}

void refillWeapons(bool init) {
	for (int w = 0; w < TotalW; w++)
		if (WeaponPres & (1 << w))
		{
			if (WeapInfo[w].fullauto) FiringMode[w] = 1; else FiringMode[w] = 0;

			ShotsLeft[w] = WeapInfo[w].Shots;
			if (DoubleAmmo) {
				if (WeapInfo[w].Reload || WeapInfo[w].rldAnim < 0) {
					ShotsLeft[w] *= 2;
				} else {
					AmmoMag[w] = 1;
					MagShotsLeft[w] = WeapInfo[w].Shots;
				}
			}
			
			if (WeapInfo[w].Reload) {
				if (init || WeapInfo[w].rldAnim < 0) {
					Chambered[w] = WeapInfo[w].Reload;
					ShotsLeft[w] -= WeapInfo[w].Reload;
				}
			} else {
				if (init || WeapInfo[w].pmpAnim < 0) {
					Chambered[w] = 1;
					ShotsLeft[w] -= 1;
				}
			}
			if (TargetWeapon == -1) TargetWeapon = w;
		}
}

void ReInitGame()
{
  PrintLog("ReInitGame();\n");
  PlaceHunter();
  Muzz = false;
  MuzzFTime = 0;
  if (g_GameMode == GameMode::TrophyMode)	PlaceTrophy();
  else if (g_GameMode == GameMode::SurvivalMode) {
	  SurvivalWave = 0;
	  PlaceCharactersSurvival();
  } else {
	  PlaceCharacters();
	  resetSSHip();
	  resetBullets();
	  if (Multiplayer) {
		  sendGunShot = -1;
		  sendHunterCall = -1;
		  sendHunterCallType = -1;
		  for (int c = 0; c < ChCount; c++) {
			  sendDamage[c] = 0;
		  }
		  for (int i = 0; i < 4; i++) {
			  mGunShot[i] = -1;
			  mHunterCall[i] = -1;
			  mHunterCallType[i] = -1;
			  for (int c = 0; c < ChCount; c++) {
				  mDamage[i][c] = 0;
			  }
		  }
		  HunterCount = 0;
		  PlaceMHunters();//temp??
	  }
  }

  LoadCharacters();

  LockLanding = false;
  Wind.alpha = rRand(1024) * 2.f * pi / 1024.f;
  Wind.speed = 10;
  MyHealth = MAX_HEALTH;
  TargetWeapon = -1;

  refillWeapons(true);

  CurrentWeapon = TargetWeapon;

  Weapon.state = 0;
  Weapon.FTime = 0;
  PlayerAlpha = 0;
  PlayerBeta  = 0;
  Weapon.breath = 0.f;
  Weapon.BTime = 0;
  Weapon.HoldBreath = false;
  Weapon.breathPressed = 0;

  WCCount = 0;
  ElCount = 0;
  BloodTrail.Count = 0;
  g_GameMode = GameMode::Normal;
  g_GameMode = GameMode::Normal;
  g_GameMode = GameMode::Normal;
  g_GameMode = GameMode::Normal;

  if (g_GameMode == GameMode::SurvivalMode) {
	  PlayerAlpha = pi * 2 * SurvivalSpawnA / 360.f;
	  Weapon.state = 2;
	  if (WeapInfo[CurrentWeapon].Optic) g_GameMode = GameMode::OpticScope;
  }

  Ship.pos.x = PlayerX;
  Ship.pos.z = PlayerZ;
  Ship.pos.y = GetLandUpH(Ship.pos.x, Ship.pos.z) + 2048;
  Ship.State = -1;
  Ship.tgpos.x = Ship.pos.x;
  Ship.tgpos.z = Ship.pos.z + 60*256;
  Ship.cindex  = -1;
  Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + 2048;
  ShipTask.tcount = 0;

  if (g_GameMode != GameMode::TrophyMode)
  {
    TrophyRoom.Last.smade = 0;
    TrophyRoom.Last.success = 0;
    TrophyRoom.Last.path  = 0;
    TrophyRoom.Last.time  = 0;
  }

  DemoPoint.DemoTime = 0;
  RestartMode = false;
  TrophyDisplay=false;
  answtime = 0;
  ExitTime = 0;

  // Allocate (vertex count based) buffers for renderers. These are
  // per-level scratch (reset on each ReInitGame call before the new
  // level's vertices are read), so they go in the LevelArena. The
  // previous 3-arg form went to Heap; the arena will reclaim them on
  // LevelArena->Reset() during ReleaseResources().
  rVertex = (Vector3d*)_HeapAlloc(Heap, 0, sizeof(Vector3d) * MaxObjectVCount, MemoryTag::Level);
  gScrp = (Vector2di*)_HeapAlloc(Heap, 0, sizeof(Vector2di) * MaxObjectVCount, MemoryTag::Level);
  PhongMapping = (Vector2df*)_HeapAlloc(Heap, 0, sizeof(Vector2df) * MaxObjectVCount, MemoryTag::Level);
  AllocateRenderTables();
}

//================ light map ========================//

void FillVector(int x, int y, Vector3d& v)
{
  v.x = static_cast<float>(x)*256;
  v.z = static_cast<float>(y)*256;
  v.y = static_cast<float>((static_cast<int>(HMap[y][x])))*ctHScale;
}

BOOL TraceVector(Vector3d v, Vector3d lv)
{
  v.y+=4;
  NormVector(lv,64);
  for (int l=0; l<32; l++)
  {
    v.x-=lv.x;
    v.y-=lv.y/6;
    v.z-=lv.z;
    if (v.y>255 * ctHScale) return true;
    if (GetLandH(v.x, v.z) > v.y) return false;
  }
  return true;
}

void AddShadow(int x, int y, int d)
{
  if (x<0 || y<0 || x>1023 || y>1023) return;
  int l = LMap[y][x];
  l-=d;
  if (l<32) l=32;
  LMap[y][x]=l;
}

void RenderShadowCircle(int x, int y, int R, int D)
{
  int cx = x / 256;
  int cy = y / 256;
  int cr = 1 + R / 256;
  for (int yy=-cr; yy<=cr; yy++)
    for (int xx=-cr; xx<=cr; xx++)
    {
      int tx = (cx+xx)*256;
      int ty = (cy+yy)*256;
      int r = static_cast<int>(sqrt( static_cast<double>(((tx-x)*(tx-x) + (ty-y)*(ty-y))) ));
      if (r>R) continue;
      AddShadow(cx+xx, cy+yy, D * (R-r) / R);
    }
}

void RenderLightMap()
{

  Vector3d lv;
  int x,y;

  lv.x = - 412;
  lv.z = - 412;
  lv.y = - 1024;
  NormVector(lv, 1.0f);

  for (y=1; y<ctMapSize-1; y++)
    for (x=1; x<ctMapSize-1; x++)
    {
      int ob = OMap[y][x];
      if (ob == 255) continue;

      int l = MObjects[ob].info.linelenght / 128;
      int s = 1;
      if (OptDayNight==2) s=-1;
      if (OptDayNight!=1) l = MObjects[ob].info.linelenght / 70;
      if (l>0) RenderShadowCircle(x*256+128,y*256+128, 256, MObjects[ob].info.lintensity * 2);
      for (int i=1; i<l; i++)
        AddShadow(x+i*s, y+i*s, MObjects[ob].info.lintensity);

      l = MObjects[ob].info.linelenght * 2;
      RenderShadowCircle(x*256+128+l*s,y*256+128+l*s,
                         MObjects[ob].info.circlerad*2,
                         MObjects[ob].info.cintensity*4);
    }

}

void SaveScreenShot()
{

  HANDLE hf;                  /* file handle */
  BITMAPFILEHEADER hdr;       /* bitmap file-header */
  BITMAPINFOHEADER bmi;       /* bitmap info-header */
  DWORD dwTmp;

  if (WinW>1024) return;

  //MessageBeep(0xFFFFFFFF);
  CopyHARDToDIB();

  bmi.biSize = sizeof(BITMAPINFOHEADER);
  bmi.biWidth = WinW;
  bmi.biHeight = WinH;
  bmi.biPlanes = 1;
  bmi.biBitCount = 24;
  bmi.biCompression = BI_RGB;

  bmi.biSizeImage = WinW*WinH*3;
  bmi.biClrImportant = 0;
  bmi.biClrUsed = 0;

  hdr.bfType = 0x4d42;
  hdr.bfSize = static_cast<DWORD>((sizeof(BITMAPFILEHEADER) +
                        bmi.biSize + bmi.biSizeImage));
  hdr.bfReserved1 = 0;
  hdr.bfReserved2 = 0;
  hdr.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER)) +
                  bmi.biSize;

  char t[12];
  sprintf_s(t, sizeof(t),"HUNT%004d.BMP",++_shotcounter);
  hf = CreateFile(t,
                  GENERIC_READ | GENERIC_WRITE,
                  static_cast<DWORD>(0),
                  (LPSECURITY_ATTRIBUTES) nullptr,
                  CREATE_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL,
                  (HANDLE) nullptr);

  WriteFile(hf, static_cast<LPVOID>(&hdr), sizeof(BITMAPFILEHEADER), static_cast<LPDWORD>(&dwTmp), (LPOVERLAPPED) nullptr);

  WriteFile(hf, &bmi, sizeof(BITMAPINFOHEADER), static_cast<LPDWORD>(&dwTmp), (LPOVERLAPPED) nullptr);

  byte fRGB[1024][3];

  for (int y=0; y<WinH; y++)
  {
    for (int x=0; x<WinW; x++)
    {
      WORD C = *(static_cast<WORD*>(lpVideoBuf) + (WinEY-y)*VideoPitch+x);
      fRGB[x][0] = (C       & 31)<<3;
#if defined(_gl)
      fRGB[x][1] = ((C>> 5) & 31)<<3;
      fRGB[x][2] = ((C>>10) & 31)<<3;
#else
      if (HARD3D)
      {
        fRGB[x][1] = ((C>> 5) & 63)<<2;
        fRGB[x][2] = ((C>>11) & 31)<<3;
      }
      else
      {
        fRGB[x][1] = ((C>> 5) & 31)<<3;
        fRGB[x][2] = ((C>>10) & 31)<<3;
      }
#endif
    }
    WriteFile( hf, fRGB, 3*WinW, &dwTmp, nullptr );
  }

  CloseHandle(hf);
  //MessageBeep(0xFFFFFFFF);
}

//===============================================================================================
//===============================================================================================

void CreateLog()
{

  hlog = CreateFile("render.log",
                    GENERIC_WRITE,
                    FILE_SHARE_READ, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

#ifdef _d3d
  PrintLog("CarnivoresII  D3D video driver.");
#endif

#ifdef _3dfx
  PrintLog("CarnivoresII 3DFX video driver.");
#endif

#ifdef _soft
  PrintLog("CarnivoresII Soft video driver.");
#endif
  PrintLog(" Build v2.04. Sep.24 1999.\n");
}

void PrintLog(LPSTR l)
{
  DWORD w;

  if (l[strlen(l)-1]==0x0A)
  {
    BYTE b = 0x0D;
    WriteFile(hlog, l, strlen(l)-1, &w, nullptr);
    WriteFile(hlog, &b, 1, &w, nullptr);
    b = 0x0A;
    WriteFile(hlog, &b, 1, &w, nullptr);
  }
  else
    WriteFile(hlog, l, strlen(l), &w, nullptr);

}

void PrintLogVerbose(LPSTR l)
{
  if (!g_VerboseLogging) return;
  PrintLog(l);
}

void CloseLog()
{
  CloseHandle(hlog);
}