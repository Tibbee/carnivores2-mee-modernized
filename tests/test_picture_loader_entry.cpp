// test_picture_loader_entry.cpp -- production BMP/TGA-loader contracts

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "Hunt.h"
#include "Loaders/TgaFormat.h"
#include "test_support.h"

namespace {

using test_support::LoaderHalt;

void WriteBmp(const char* path, LONG width, LONG height, bool writePixels)
{
    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + 6;

    BITMAPINFOHEADER infoHeader{};
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = BI_RGB;

    FILE* file = nullptr;
    ASSERT_EQ(fopen_s(&file, path, "wb"), 0);
    ASSERT_EQ(fwrite(&fileHeader, 1, sizeof(fileHeader), file), sizeof(fileHeader));
    ASSERT_EQ(fwrite(&infoHeader, 1, sizeof(infoHeader), file), sizeof(infoHeader));
    if (writePixels)
    {
        const unsigned char pixels[6] = {0, 0, 255, 0, 255, 0};
        ASSERT_EQ(fwrite(pixels, 1, sizeof(pixels), file), sizeof(pixels));
    }
    fclose(file);
}

// 18-byte TGA header + optional ID field + bottom-up 16-bit pixel rows with
// increasing values so row orientation stays observable.
void WriteTga(const char* path, uint16_t width, uint16_t height, uint8_t imageType,
              uint8_t bits, uint8_t colorMapType, uint8_t identSize, bool writePixels)
{
    unsigned char header[18] = {};
    header[0] = identSize;
    header[1] = colorMapType;
    header[2] = imageType;
    header[12] = static_cast<unsigned char>(width & 0xFF);
    header[13] = static_cast<unsigned char>(width >> 8);
    header[14] = static_cast<unsigned char>(height & 0xFF);
    header[15] = static_cast<unsigned char>(height >> 8);
    header[16] = bits;

    FILE* file = nullptr;
    ASSERT_EQ(fopen_s(&file, path, "wb"), 0);
    ASSERT_EQ(fwrite(header, 1, sizeof(header), file), sizeof(header));
    for (uint8_t i = 0; i < identSize; ++i)
    {
        const unsigned char id = 0;
        ASSERT_EQ(fwrite(&id, 1, 1, file), 1u);
    }
    if (writePixels)
    {
        for (int p = 0; p < width * height; ++p)
        {
            const uint16_t pixel = static_cast<uint16_t>(0x1000 + p);
            ASSERT_EQ(fwrite(&pixel, 1, sizeof(pixel), file), sizeof(pixel));
        }
    }
    fclose(file);
}

}  // namespace

HANDLE Heap = nullptr;
BOOL NightVisionMode = FALSE;
BOOL NightVisionOn = FALSE;
BOOL HARD3D = FALSE;

void DoHalt(LPSTR message)
{
    throw LoaderHalt(message);
}

void PrintLog(LPSTR)
{}

WORD conv_565(WORD color)
{
    return color;
}

LPVOID _HeapAlloc(HANDLE, DWORD, DWORD bytes)
{
    return std::malloc(bytes == 0 ? 1 : bytes);
}

LPVOID _HeapAlloc(HANDLE, DWORD, DWORD bytes, MemoryTag)
{
    return std::malloc(bytes == 0 ? 1 : bytes);
}

BOOL _HeapFree(HANDLE, DWORD, LPVOID memory)
{
    std::free(memory);
    return TRUE;
}

TEST(PictureLoaderEntry, MissingBmpUsesProductionFailurePath)
{
    char path[] = "__c2_picture_contract_missing__.bmp";
    TPicture picture{};

    EXPECT_THROW(LoadPicture(picture, path, MemoryTag::Global), LoaderHalt);
}

TEST(PictureLoaderEntry, TruncatedBmpUsesProductionFailurePath)
{
    const char* path = "__c2_picture_contract_truncated__.bmp";
    FILE* file = nullptr;
    ASSERT_EQ(fopen_s(&file, path, "wb"), 0);
    const unsigned char byte = 0;
    ASSERT_EQ(fwrite(&byte, 1, sizeof(byte), file), sizeof(byte));
    fclose(file);

    char mutablePath[] = "__c2_picture_contract_truncated__.bmp";
    TPicture picture{};
    EXPECT_THROW(LoadPicture(picture, mutablePath, MemoryTag::Global), LoaderHalt);

    remove(path);
}

TEST(PictureLoaderEntry, RejectsBmpRowWidthThatExceedsStackBuffer)
{
    const char* path = "__c2_picture_contract_wide__.bmp";
    WriteBmp(path, 801, 1, false);

    char mutablePath[] = "__c2_picture_contract_wide__.bmp";
    TPicture picture{};
    EXPECT_THROW(LoadPicture(picture, mutablePath, MemoryTag::Global), LoaderHalt);

    remove(path);
}

TEST(PictureLoaderEntry, ValidBmpUsesProductionEntryPoint)
{
    const char* path = "__c2_picture_contract_valid__.bmp";
    WriteBmp(path, 2, 1, true);

    char mutablePath[] = "__c2_picture_contract_valid__.bmp";
    TPicture picture{};
    EXPECT_NO_THROW(LoadPicture(picture, mutablePath, MemoryTag::Global));
    EXPECT_EQ(picture.W, 2);
    EXPECT_EQ(picture.H, 1);
    ASSERT_NE(picture.lpImage, nullptr);
    EXPECT_EQ(picture.lpImage[0], 31u << 10);
    EXPECT_EQ(picture.lpImage[1], 31u << 5);

    picture.lpImage.reset();
    remove(path);
}

TEST(PictureLoaderEntry, RejectsPalettedTga)
{
    const char* path = "__c2_picture_contract_paletted__.tga";
    WriteTga(path, 1, 1, TGA_IMAGETYPE_RGB, 16, /*colorMapType=*/1, 0, false);

    char mutablePath[] = "__c2_picture_contract_paletted__.tga";
    TPicture picture{};
    EXPECT_THROW(LoadPictureTGA(picture, mutablePath, MemoryTag::Global), LoaderHalt);

    remove(path);
}

TEST(PictureLoaderEntry, RejectsNonTruecolorTga)
{
    const char* path = "__c2_picture_contract_rle__.tga";
    WriteTga(path, 1, 1, /*imageType=*/10, 16, 0, 0, false);

    char mutablePath[] = "__c2_picture_contract_rle__.tga";
    TPicture picture{};
    EXPECT_THROW(LoadPictureTGA(picture, mutablePath, MemoryTag::Global), LoaderHalt);

    remove(path);
}

TEST(PictureLoaderEntry, RejectsNonSixteenBitTga)
{
    const char* path = "__c2_picture_contract_24bit__.tga";
    WriteTga(path, 1, 1, TGA_IMAGETYPE_RGB, /*bits=*/24, 0, 0, false);

    char mutablePath[] = "__c2_picture_contract_24bit__.tga";
    TPicture picture{};
    EXPECT_THROW(LoadPictureTGA(picture, mutablePath, MemoryTag::Global), LoaderHalt);

    remove(path);
}

TEST(PictureLoaderEntry, ValidTgaUsesProductionEntryPointAndFlipsRowsBottomUp)
{
    const char* path = "__c2_picture_contract_valid__.tga";
    WriteTga(path, 1, 2, TGA_IMAGETYPE_RGB, 16, 0, 0, true);

    char mutablePath[] = "__c2_picture_contract_valid__.tga";
    TPicture picture{};
    EXPECT_NO_THROW(LoadPictureTGA(picture, mutablePath, MemoryTag::Global));
    EXPECT_EQ(picture.W, 1);
    EXPECT_EQ(picture.H, 2);
    ASSERT_NE(picture.lpImage, nullptr);
    // File rows are bottom-up: the last file row lands first in memory.
    EXPECT_EQ(picture.lpImage[0], 0x1001);
    EXPECT_EQ(picture.lpImage[1], 0x1000);

    picture.lpImage.reset();
    remove(path);
}

TEST(PictureLoaderEntry, TgaIdentFieldIsSkippedBeforePixelRows)
{
    const char* path = "__c2_picture_contract_ident__.tga";
    WriteTga(path, 1, 1, TGA_IMAGETYPE_RGB, 16, 0, /*identSize=*/5, true);

    char mutablePath[] = "__c2_picture_contract_ident__.tga";
    TPicture picture{};
    EXPECT_NO_THROW(LoadPictureTGA(picture, mutablePath, MemoryTag::Global));
    EXPECT_EQ(picture.W, 1);
    EXPECT_EQ(picture.H, 1);
    ASSERT_NE(picture.lpImage, nullptr);
    EXPECT_EQ(picture.lpImage[0], 0x1000);

    picture.lpImage.reset();
    remove(path);
}

TEST(PictureLoaderEntry, TruncatedTgaPayloadUsesProductionFailurePath)
{
    const char* path = "__c2_picture_contract_truncated__.tga";
    WriteTga(path, 2, 2, TGA_IMAGETYPE_RGB, 16, 0, 0, false);

    char mutablePath[] = "__c2_picture_contract_truncated__.tga";
    TPicture picture{};
    EXPECT_THROW(LoadPictureTGA(picture, mutablePath, MemoryTag::Global), LoaderHalt);

    remove(path);
}
