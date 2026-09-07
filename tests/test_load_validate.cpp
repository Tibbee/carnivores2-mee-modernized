// test_load_validate.cpp
// Unit tests for Hunt/Loaders/LoadValidate.h — the checked arithmetic and
// file-data validation helpers used by the binary/text loaders.
//
// The header is pure (only ReadExact touches Win32, via a caller-provided
// HANDLE), so no engine linkage is needed.

#include <gtest/gtest.h>

#include <cstdio>

#include "Loaders/LoadValidate.h"

namespace {

TEST(LoadValidate, CountsFitFixedArrays) {
    EXPECT_TRUE(IsValidCount(0, 64));
    EXPECT_TRUE(IsValidCount(64, 64));
    EXPECT_TRUE(IsValidCount(22, 64));  // shipped .CAR AniCount maximum
    EXPECT_FALSE(IsValidCount(-1, 64));
    EXPECT_FALSE(IsValidCount(65, 64));
    EXPECT_FALSE(IsValidCount(1025, 1024));  // gObj capacity
}

TEST(LoadValidate, CheckedSizesRejectWrap) {
    size_t out = 0;
    EXPECT_TRUE(CheckedBytes2(1989, 16, out));  // shipped max VCount * 16
    EXPECT_EQ(out, 1989u * 16u);
    EXPECT_TRUE(CheckedBytes3(1989, 365, 6, out));
    EXPECT_FALSE(CheckedBytes2(0xFFFFFFFFu, 2, out));  // 32-bit wrap
    EXPECT_FALSE(CheckedBytes2(1 << 20, 1 << 20, out));
    EXPECT_FALSE(CheckedBytes3(65536, 65536, 65536, out));
    EXPECT_FALSE(CheckedBytes2(static_cast<size_t>(-1), 2, out));
}

TEST(LoadValidate, VertexIndicesStayInRange) {
    EXPECT_TRUE(IsValidVertexIndex(0, 1989));
    EXPECT_TRUE(IsValidVertexIndex(1988, 1989));
    EXPECT_FALSE(IsValidVertexIndex(-1, 1989));
    EXPECT_FALSE(IsValidVertexIndex(1989, 1989));
    EXPECT_FALSE(IsValidVertexIndex(100000, 1989));
}

TEST(LoadValidate, BmpWidthGuardsStackBuffer) {
    EXPECT_TRUE(IsValidBmpWidth(1));
    EXPECT_TRUE(IsValidBmpWidth(800));
    EXPECT_FALSE(IsValidBmpWidth(0));
    EXPECT_FALSE(IsValidBmpWidth(-40));
    EXPECT_FALSE(IsValidBmpWidth(801));
}

TEST(LoadValidate, WavLengthsAreSane) {
    EXPECT_TRUE(IsValidWavLength(0));
    EXPECT_TRUE(IsValidWavLength(44100 * 2));
    EXPECT_FALSE(IsValidWavLength(-1));
    EXPECT_FALSE(IsValidWavLength((16 << 20) + 1));
    // Odd lengths allocate a rounded-up sample count (no 1-byte overflow).
    EXPECT_EQ(WavAllocSamples(100), 50u);
    EXPECT_EQ(WavAllocSamples(101), 51u);
    EXPECT_EQ(WavAllocSamples(0), 0u);
}

TEST(LoadValidate, StripQuotedMatchesScriptIdiom) {
    char good[] = "'para.car'\n";
    EXPECT_STREQ(StripQuoted(good), "para.car");
    char crlf[] = "'area1'\r\n";
    EXPECT_STREQ(StripQuoted(crlf), "area1");
    char noNewline[] = "'bag1.car'";
    EXPECT_STREQ(StripQuoted(noNewline), "bag1.car");
    char tooShort[] = "'\n";
    EXPECT_EQ(StripQuoted(tooShort), nullptr);
    char empty[] = "";
    EXPECT_EQ(StripQuoted(empty), nullptr);
    EXPECT_EQ(StripQuoted(nullptr), nullptr);
    char unquoted[] = "paracar\n";
    EXPECT_EQ(StripQuoted(unquoted), nullptr);
}

TEST(LoadValidate, CopyCappedRejectsOverflow) {
    char dst[48];
    EXPECT_TRUE(CopyCapped(dst, sizeof(dst), "jager.car"));
    EXPECT_STREQ(dst, "jager.car");
    char exact[4];
    EXPECT_TRUE(CopyCapped(exact, sizeof(exact), "abc"));
    EXPECT_FALSE(CopyCapped(exact, sizeof(exact), "abcd"));  // needs NUL room
    EXPECT_FALSE(CopyCapped(nullptr, 48, "x"));
    EXPECT_FALSE(CopyCapped(dst, 0, "x"));
    EXPECT_FALSE(CopyCapped(dst, sizeof(dst), nullptr));
}

TEST(LoadValidate, ReadExactDetectsTruncation) {
    const char* path = "load_validate_probe.bin";
    const unsigned char data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    FILE* f = nullptr;
    ASSERT_EQ(fopen_s(&f, path, "wb"), 0);
    ASSERT_EQ(fwrite(data, 1, sizeof(data), f), sizeof(data));
    fclose(f);

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(h, INVALID_HANDLE_VALUE);
    unsigned char buf[16];
    EXPECT_TRUE(ReadExact(h, buf, 8));
    EXPECT_EQ(memcmp(buf, data, 8), 0);
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    EXPECT_FALSE(ReadExact(h, buf, 9));  // one byte past EOF
    EXPECT_TRUE(ReadExact(h, buf, 0));   // zero-length reads succeed
    CloseHandle(h);
    remove(path);
}

}  // namespace
