#pragma once

// ==========================================================================
// TgaFormat.h -- Header layout and acceptance rule for 16-bit TGA pictures
// ==========================================================================
// Shipped and modded picture/menu art is 16-bit uncompressed truecolor (5551)
// TGA with no palette. The engine's picture loader and the menu's resource
// loader both read that format; this header owns the on-disk header layout,
// the image-type constants and the one acceptance rule. Readers keep their
// own I/O, orientation handling and failure reporting.
// ==========================================================================

#include <cstdint>

#define TGA_IMAGETYPE_NONE 0
#define TGA_IMAGETYPE_INDEXED 1
#define TGA_IMAGETYPE_RGB 2
#define TGA_IMAGETYPE_GREYSCALE 3

#pragma pack(push, 1)
struct TARGAINFOHEADER
{
    uint8_t	    tgaIdentSize;          // size of ID field that follows 18 byte header (0 usually)
    uint8_t	    tgaColorMapType;      // type of colour map 0=none, 1=has palette
    uint8_t	    tgaImageType;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

    uint16_t	tgaColorMapOffset;     // first colour map entry in palette
    uint16_t   	tgaColorMapLength;    // number of colours in palette
    uint8_t	    tgaColorMapBits;      // number of bits per palette entry 15,16,24,32

    uint16_t	tgaXStart;             // image x origin
    uint16_t	tgaYStart;             // image y origin
    uint16_t	tgaWidth;              // image width in pixels
    uint16_t	tgaHeight;             // image height in pixels
    uint8_t	    tgaBits;               // image bits per pixel 8,16,24,32
    uint8_t	    tgaDescriptor;         // image descriptor bits (vh flip bits)
};
#pragma pack(pop)

static_assert(sizeof(TARGAINFOHEADER) == 18, "TGA header must stay the on-disk 18 bytes");

// Why a picture was rejected; callers keep their own messages per case.
enum class TgaPictureFormat { Ok, Paletted, NotTruecolor, Not16Bit };

// The one acceptance rule shared by the picture readers.
inline TgaPictureFormat CheckTgaPictureFormat(const TARGAINFOHEADER& header)
{
    if (header.tgaColorMapType != 0) return TgaPictureFormat::Paletted;
    if (header.tgaImageType != TGA_IMAGETYPE_RGB) return TgaPictureFormat::NotTruecolor;
    if (header.tgaBits != 16) return TgaPictureFormat::Not16Bit;
    return TgaPictureFormat::Ok;
}
