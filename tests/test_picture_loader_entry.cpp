// test_picture_loader_entry.cpp -- production BMP-loader contracts

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "Hunt.h"

namespace {

class LoaderHalt final : public std::runtime_error
{
public:
    explicit LoaderHalt(const char* message)
        : std::runtime_error(message ? message : "loader halt")
    {}
};

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
