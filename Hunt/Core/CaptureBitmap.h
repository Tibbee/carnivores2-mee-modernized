#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace capture_bitmap {
// 32-bit BI_RGB BMP, bottom-up BGRA: exactly glReadPixels(GL_BGRA)'s layout.
// No renderer dependencies, no screen/window capture, no extra row padding.
inline bool makeHeader(int width, int height, std::array<unsigned char, 54>& header,
                       std::uint32_t& pixelBytes) {
    if (width <= 0 || height <= 0) return false;
    const auto bytes = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4;
    if (bytes > (std::numeric_limits<std::uint32_t>::max)() - header.size()) return false;
    pixelBytes = static_cast<std::uint32_t>(bytes);
    header.fill(0);
    auto put32 = [&](int offset, std::uint32_t value) {
        for (int i = 0; i < 4; ++i) header[offset + i] = static_cast<unsigned char>(value >> (8 * i));
    };
    header[0] = 'B'; header[1] = 'M';
    put32(2, pixelBytes + static_cast<std::uint32_t>(header.size()));
    put32(10, static_cast<std::uint32_t>(header.size()));
    put32(14, 40);
    put32(18, static_cast<std::uint32_t>(width));
    put32(22, static_cast<std::uint32_t>(height));
    header[26] = 1;
    header[28] = 32;
    put32(34, pixelBytes);
    return true;
}

inline bool write(const char* path, int width, int height, const unsigned char* bgra) {
    std::array<unsigned char, 54> header{};
    std::uint32_t pixelBytes = 0;
    if (!bgra || !makeHeader(width, height, header, pixelBytes)) return false;
    FILE* file = std::fopen(path, "wb");
    if (!file) return false;
    const bool written = std::fwrite(header.data(), 1, header.size(), file) == header.size() &&
                         std::fwrite(bgra, 1, pixelBytes, file) == pixelBytes;
    const bool closed = std::fclose(file) == 0;
    return written && closed;
}
} // namespace capture_bitmap
