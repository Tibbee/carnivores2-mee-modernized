// ==========================================================================
// SoundLoader.cpp
// ==========================================================================

#include "Hunt.h"
#include "LoadValidate.h"

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
  // Bound the chunk search by the real file size. A truncated file with no
  // 'data' chunk previously spun here forever: at EOF ReadFile fails with
  // l=0 but the loop never checked, re-reading nothing endlessly.
  const DWORD fileSize = GetFileSize(hfile, nullptr);
  if (fileSize == INVALID_FILE_SIZE)
    DoHalt("Sound loading error: cannot stat WAV file.");
  DWORD pos = SetFilePointer(hfile, 36, nullptr, FILE_BEGIN);
  if (pos == INVALID_SET_FILE_POINTER)
    DoHalt("Sound loading error: truncated WAV header.");

  char c[5];
  c[4] = 0;

  for ( ; ; )
  {
    if (pos >= fileSize)
      DoHalt("Sound loading error: WAV has no data chunk (truncated file).");
    if (!ReadExact(hfile, c, 1))
      DoHalt("Sound loading error: truncated WAV chunk scan.");
    pos += 1;
    if( c[0] == 'd' )
    {
      if (!ReadExact(hfile, &c[1], 3))
        DoHalt("Sound loading error: truncated WAV chunk header.");
      pos += 3;
      if( !lstrcmp( c, "data" ) ) break;
      else {
        SetFilePointer( hfile, -3, nullptr, FILE_CURRENT );
        pos -= 3;
      }
    }
  }

  if (!ReadExact(hfile, &sfx.length, 4))
    DoHalt("Sound loading error: truncated WAV data length.");
  l = 4;
  pos += 4;

  // sfx.length is in bytes; std::vector is element-counted. Bound it first
  // (corrupt values drove huge assigns) and round the allocation UP: an odd
  // length previously overflowed the floor(length/2) buffer by one byte.
  // assign() value-initializes to zero, matching the old HEAP_ZERO_MEMORY
  // behavior. Reject a short payload instead of silently accepting a
  // partially initialized sound.
  if (!IsValidWavLength(sfx.length))
    DoHalt("Sound loading error: WAV data length out of range.");
  sfx.lpData.assign(WavAllocSamples(sfx.length), 0);
  if (!ReadExact(hfile, sfx.lpData.data(), (DWORD)sfx.length))
    DoHalt("Sound loading error: truncated WAV data.");
  CloseHandle(hfile);
}