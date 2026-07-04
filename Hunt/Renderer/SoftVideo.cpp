// ==========================================================================
// SoftVideo.cpp — Software renderer frame post-processing
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft
void _FillMemoryWord(int maddr, int count, WORD w)
{
  __asm
  {
    mov edi,maddr
    mov ecx,count
    shr ecx,2
    mov ax,w
    shl eax,16
    mov ax,w
    rep stosd
  }
}

void FillMemoryWord(int maddr, int count, WORD w)
{
  WORD ww[4];
  ww[0] = w;
  ww[1] = w;
  ww[2] = w;
  ww[3] = w;
  __asm
  {
    EMMS
    mov edi,maddr
    mov ecx,count
    shr ecx,3
    MOVQ MM0,ww
} L1: __asm
  {
    MOVQ [edi],MM0
    add edi,8
    dec cx
    jnz L1
    EMMS
  }
}



void _memcpy(void* daddr, void* saddr, int count)
{
  __asm
  {
    EMMS
    mov edi,daddr
    mov esi,saddr
    mov ecx,count
    shr ecx,3
} L1: __asm
  {
    MOVQ MM0,[esi]
    MOVQ [edi],MM0
    add edi,8
    add esi,8
    dec cx
    jnz L1
    EMMS
  }
}


void ClearVideoBuf()
{
  WORD w = HiColor(SkyR/8, SkyG/8, SkyB/8);

  for(int y=0; y<WinH; y++)
  {
    _FillMemoryWord( static_cast<int>(reinterpret_cast<intptr_t>(lpVideoBuf)) + y*VideoPitchB, WinW*2, w);
  }
}



void ShowVideo()
{
  HDC _hdc =  hdcCMain;
  HBITMAP hbmpOld = reinterpret_cast<HBITMAP>(SelectObject(_hdc,hbmpVideoBuf));

  if (IsUnderwater() && CORRECTION)
    for (int y=0; y<WinH; y++)
      for (int x=0; x<WinW; x++)
        *(static_cast<WORD*>(lpVideoBuf) + y*VideoPitch + x) = FadeTab[64][*(static_cast<WORD*>(lpVideoBuf) + y*VideoPitch + x) & 0x7FFF];

  // Night darkness + desaturation overlay (when night hunt and night vision is off)
  // Partial desaturation to 60%: mix original color with luminance grayscale, then darken
  if (OptDayNight == 2 && !NightVisionOn) {
    for (int y=0; y<WinH; y++)
      for (int x=0; x<WinW; x++) {
        WORD* p = static_cast<WORD*>(lpVideoBuf) + y*VideoPitch + x;
        int r = (*p >> 10) & 0x1F;
        int g = (*p >> 5) & 0x1F;
        int b = *p & 0x1F;
        // Partial desaturate: mix 40% original + 60% luminance grayscale
        int gray = (r*10 + g*19 + b*3) / 32;  // 5-bit luminance weights
        r = (r * 2 + gray * 3) / 5;  // 40% original, 60% gray
        g = (g * 2 + gray * 3) / 5;
        b = (b * 2 + gray * 3) / 5;
        // Darken to ~50%
        r = r / 2;
        g = g / 2;
        b = b / 2;
        *p = static_cast<WORD>((r << 10) | (g << 5) | b);
      }
  }

  // Night vision green overlay (toggleable via equipment + keybind)
  if (NightVisionOn) {
    for (int y=0; y<WinH; y++)
      for (int x=0; x<WinW; x++) {
        WORD* p = static_cast<WORD*>(lpVideoBuf) + y*VideoPitch + x;
        int r = (*p >> 10) & 0x1F;
        int g = (*p >> 5) & 0x1F;
        int b = *p & 0x1F;
        // Boost green channel, reduce red and blue (night vision effect)
        g = (g + 8 > 31) ? 31 : g + 8;
        r = r * 3 / 4;
        b = b * 3 / 4;
        *p = static_cast<WORD>((r << 10) | (g << 5) | b);
      }
  }

  RenderHealthBar();

  if (!FULLSCREEN && WinW > 6 && WinH > 6) {
    FillMemory(static_cast<WORD*>(lpVideoBuf), WinW*2, 0);
    FillMemory(static_cast<WORD*>(lpVideoBuf)+1*VideoPitch, WinW*2, 0);
    FillMemory(static_cast<WORD*>(lpVideoBuf)+2*VideoPitch, WinW*2, 0);

    FillMemory(static_cast<WORD*>(lpVideoBuf)+(WinH-1)*VideoPitch, WinW*2, 0);
    FillMemory(static_cast<WORD*>(lpVideoBuf)+(WinH-2)*VideoPitch, WinW*2, 0);
    FillMemory(static_cast<WORD*>(lpVideoBuf)+(WinH-3)*VideoPitch, WinW*2, 0);

    for (int y=1; y<WinH-1; y++)
    {
      for (int x=0; x<3; x++) {
        int c;
        if (x==1) c=0x5294; else c=0;
        *(static_cast<WORD*>(lpVideoBuf) + (y*VideoPitch) + x) = c;
        *(static_cast<WORD*>(lpVideoBuf) + (y*VideoPitch) + WinW-x-1) = c;
      }
    }

    for (int x=1; x<WinW-2; x++) {
      *(static_cast<WORD*>(lpVideoBuf) + (1*VideoPitch) + x) = 0x5294;
      *(static_cast<WORD*>(lpVideoBuf) + ((WinH-2)*VideoPitch) + x) = 0x5294;
    }
  }

  BitBlt(hdcMain,0,0,WinW,WinH, _hdc,0,0, SRCCOPY);

  SelectObject(_hdc,hbmpOld);
  //DeleteDC(_hdc);
}




#endif // _soft
