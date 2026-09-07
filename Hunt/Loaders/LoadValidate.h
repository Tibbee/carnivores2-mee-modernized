#ifndef HUNT_LOAD_VALIDATE_H
#define HUNT_LOAD_VALIDATE_H

// LoadValidate.h
// Checked arithmetic + file-data validation for the binary/text loaders.
//
// Background: the .CAR/.RSC/.MAP/BMP/TGA/WAV/_RES.TXT loaders historically
// trusted file-derived counts and dimensions. Shipped assets are all valid
// (audit-scanned), but modded or truncated files could drive signed-shift
// overflows, fixed-array overruns, and stack/heap overflows. These helpers
// fail fast (via the caller's DoHalt) instead of corrupting memory.
//
// All helpers are pure and header-inline so tests/test_load_validate.cpp
// covers them without linking engine code. Only ReadExact touches Win32,
// and it reports success/failure instead of halting so tests can drive it
// with a real temp file.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <windows.h>

// File-derived count must fit its fixed destination array.
inline bool IsValidCount(int value, int capacity)
{
    return value >= 0 && value <= capacity;
}

// size = a * b without 32-bit wrap. The allocator takes a DWORD, so the
// result must also fit 32 bits.
inline bool CheckedBytes2(size_t a, size_t b, size_t& out)
{
    const unsigned long long wide =
        static_cast<unsigned long long>(a) * static_cast<unsigned long long>(b);
    if (wide > static_cast<unsigned long long>(0xFFFFFFFFu))
        return false;
    out = static_cast<size_t>(wide);
    return true;
}

inline bool CheckedBytes3(size_t a, size_t b, size_t c, size_t& out)
{
    size_t ab = 0;
    if (!CheckedBytes2(a, b, ab))
        return false;
    return CheckedBytes2(ab, c, out);
}

// Face-vertex index must address a loaded vertex.
inline bool IsValidVertexIndex(int index, int vcount)
{
    return index >= 0 && index < vcount;
}

// BMP rows land in byte fRGB[800][3] on the stack: the width cap is
// structural, not aesthetic. Height is heap-checked via CheckedBytes.
inline bool IsValidBmpWidth(int w) { return w > 0 && w <= 800; }

// WAV data length sanity: non-negative, bounded (16 MiB of 16-bit audio is
// far beyond any shipped effect), so a corrupt header cannot drive a
// gigantic vector::assign.
inline bool IsValidWavLength(int length)
{
    return length >= 0 && length <= (16 << 20);
}

// Samples to allocate for a byte length (round UP: a malformed odd length
// previously overflowed the floor(length/2) allocation by one byte).
inline size_t WavAllocSamples(int length)
{
    return static_cast<size_t>(length) / sizeof(short int) +
           ((static_cast<size_t>(length) % sizeof(short int)) != 0 ? 1u : 0u);
}

// Exact Win32 read: success only when every requested byte arrives.
// Truncated files previously left stack locals uninitialized and drove
// downstream loops/allocations with garbage.
inline bool ReadExact(HANDLE hfile, void* buffer, DWORD bytes)
{
    if (bytes == 0)
        return true;
    DWORD got = 0;
    if (!ReadFile(hfile, buffer, bytes, &got, nullptr))
        return false;
    return got == bytes;
}

// Strip one layer of surrounding single quotes in place ('name' -> name).
// Script lines keep their trailing newline, so the old inline idiom was
// value[strlen(value)-2] = 0 with use of &value[1]: it dropped the closing
// quote and relied on the newline's position. This helper makes each step
// explicit and returns nullptr on malformed input (caller halts) instead
// of indexing before the buffer when the value is shorter than ''.
inline char* StripQuoted(char* value)
{
    if (!value)
        return nullptr;
    size_t len = strlen(value);
    while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r'))
        value[--len] = 0;
    if (len < 2 || value[0] != '\'' || value[len - 1] != '\'')
        return nullptr;
    value[len - 1] = 0;
    return value + 1;
}

// Bounded copy into a fixed char field. Returns false (caller halts)
// instead of overflowing; truncation is rejected because a silently
// shortened filename produces a confusing failure far from the cause.
inline bool CopyCapped(char* dst, size_t dstCap, const char* src)
{
    if (!dst || dstCap == 0 || !src)
        return false;
    const size_t len = strlen(src);
    if (len >= dstCap)
        return false;
    memcpy(dst, src, len + 1);
    return true;
}

#endif // HUNT_LOAD_VALIDATE_H
