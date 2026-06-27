// ==========================================================================
// PictureLoader.cpp
// ==========================================================================

#include "Hunt.h"

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