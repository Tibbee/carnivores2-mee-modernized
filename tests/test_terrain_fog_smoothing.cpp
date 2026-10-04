#include <gtest/gtest.h>
#include "Core/TerrainFogSmoothing.h"
#include <array>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>

namespace {
void SameColor(const Vector3d& a, const Vector3d& b) {
    EXPECT_FLOAT_EQ(a.x, b.x);
    EXPECT_FLOAT_EQ(a.y, b.y);
    EXPECT_FLOAT_EQ(a.z, b.z);
}
}

TEST(TerrainFogSmoothing, KeepsOriginalThirtyPercentTileAverage) {
    const auto result = SmoothedTileFogColors({1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0});
    SameColor(result[0], {0.775f, 0.075f, 0.075f});
    SameColor(result[1], {0.075f, 0.775f, 0.075f});
    SameColor(result[2], {0.075f, 0.075f, 0.775f});
    SameColor(result[3], {0.075f, 0.075f, 0.075f});
}

TEST(TerrainFogSmoothing, ChunkTilesMatchStandaloneRegardlessOfOrderOrCullMask) {
    Vector3d grid[3][3];
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            grid[y][x] = {x * 0.3f, y * 0.4f, (x + y) * 0.2f};
    Vector3d original[3][3];
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) original[y][x] = grid[y][x];

    for (int mask = 0; mask < 16; ++mask) {
        std::array<int, 4> order{0, 1, 2, 3};
        do {
            for (int tile : order) {
                if (!(mask & (1 << tile))) continue;
                const int x = tile & 1, y = tile >> 1;
                const auto chunk = SmoothedTileFogColors(grid[y][x], grid[y][x+1],
                                                         grid[y+1][x], grid[y+1][x+1]);
                std::array<Vector3d, 4> standalone{original[y][x], original[y][x+1],
                                                  original[y+1][x], original[y+1][x+1]};
                SmoothFogColors(standalone[0], standalone[1], standalone[2], standalone[3]);
                for (int i = 0; i < 4; ++i) SameColor(chunk[i], standalone[i]);
            }
        } while (std::next_permutation(order.begin(), order.end()));
    }
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) SameColor(grid[y][x], original[y][x]);
}

TEST(TerrainFogSmoothing, UniformAndClearColorsArePreserved) {
    for (Vector3d color : {Vector3d{0,0,0}, Vector3d{0.2f,0.4f,0.8f}}) {
        const auto result = SmoothedTileFogColors(color, color, color, color);
        for (const auto& actual : result) SameColor(actual, color);
    }
}

TEST(TerrainFogSmoothing, RendererChunkUsesNonMutatingProductionHelper) {
    std::ifstream file(std::string(C2_SOURCE_ROOT) + "/Hunt/Renderer/GLTerrain.cpp");
    ASSERT_TRUE(file.good());
    const std::string source{std::istreambuf_iterator<char>(file), {}};
    const auto begin = source.find("void GLRenderer::CollectTerrainChunk2x2(");
    const auto end = source.find("void GLRenderer::RebuildOversizedObjectPlacements", begin);
    ASSERT_NE(begin, std::string::npos);
    ASSERT_NE(end, std::string::npos);
    const auto body = source.substr(begin, end - begin);
    EXPECT_NE(body.find("SmoothedTileFogColors("), std::string::npos);
    EXPECT_EQ(body.find("SmoothFogColors(fogCol"), std::string::npos);
}
