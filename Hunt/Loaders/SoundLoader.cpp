// ==========================================================================
// SoundLoader.cpp
// ==========================================================================

#include "Hunt.h"

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