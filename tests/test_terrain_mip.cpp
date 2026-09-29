// Unit tests for Hunt/Core/TerrainMip.h -- the mip-level policy shared by the
// software rasterisers and the GL terrain texture array.
#include <gtest/gtest.h>

#include "TerrainMip.h"

TEST(TerrainMipTest, ChainLayoutMatchesGeneratedTextures)
{
	// Loaders/ModelLoader.cpp LoadTexture writes DataA 128, DataB 64,
	// DataC 32, DataD 16 into the chain the GL array uploads as levels 0..3.
	EXPECT_EQ(kTerrainMipLevelCount, 4);
	EXPECT_EQ(kTerrainMaxMipMax, kTerrainMipLevelCount - 1);
	// The rasterisers never sample DataD: it belongs to the map thumbnail.
	EXPECT_EQ(kTerrainRasterMipLevels, 3);
}

TEST(TerrainMipTest, TexelSizeUsesTheLegacyEstimator)
{
	// CameraW * 4 * 128 / distance, i.e. the screen-space size of a 128x128
	// texture at that distance. Close ground stays at full resolution.
	EXPECT_EQ(ComputeMipTexelSize(1280, 1), 655360);
	EXPECT_EQ(ComputeMipTexelSize(1280, 5120), 128);
	EXPECT_EQ(ComputeMipTexelSize(1280, 10240), 64);
	EXPECT_EQ(ComputeMipTexelSize(1280, 20480), 32);
	EXPECT_EQ(ComputeMipTexelSize(1280, 61440), 10);
	// Wider viewport minifies faster at the same distance.
	EXPECT_GT(ComputeMipTexelSize(1920, 10000), ComputeMipTexelSize(1280, 10000));
}

TEST(TerrainMipTest, TexelSizeClampsTheDegenerateCameraTile)
{
	// The tile the camera stands in can have its centre a fraction of a world
	// unit from the eye, so the legacy expression divided by zero (#DE) and
	// killed the frame. Both clamps must return a value, never trap.
	EXPECT_GT(ComputeMipTexelSize(1280, 0), 0);
	EXPECT_GT(ComputeMipTexelSize(1280, -1), 0);
	EXPECT_EQ(ComputeMipTexelSize(1280, 0), ComputeMipTexelSize(1280, 1));
	EXPECT_GT(ComputeMipTexelSize(0, 4096), 0);
	EXPECT_GT(ComputeMipTexelSize(-1280, 4096), 0);
	// The clamped tile is the closest possible tile, so it must pick level 0.
	EXPECT_EQ(SelectRasterMipLevel(ComputeMipTexelSize(1280, 0)), 0);
}

TEST(TerrainMipTest, RasterLevelThresholdsMatchLegacy)
{
	// Preserve the legacy three-way selection exactly: >=128 DataA,
	// >=64 DataB, otherwise DataC.
	EXPECT_EQ(SelectRasterMipLevel(128), 0);
	EXPECT_EQ(SelectRasterMipLevel(127), 1);
	EXPECT_EQ(SelectRasterMipLevel(64), 1);
	EXPECT_EQ(SelectRasterMipLevel(63), 2);
	EXPECT_EQ(SelectRasterMipLevel(0), 2);
	// Level 3 (DataD) is never reachable from the raster path.
	EXPECT_LT(SelectRasterMipLevel(1), kTerrainRasterMipLevels);
}

TEST(TerrainMipTest, MaxLevelDefaultsToFullChainAndClamps)
{
	// 3 is the owner-reviewed default: the whole DataA/B/C/D chain, all of
	// which the loader already uploads.
	EXPECT_EQ(kTerrainMaxMipDefault, 3);
	EXPECT_EQ(kTerrainMaxMipMin, 0);
	EXPECT_EQ(kTerrainMaxMipMax, 3);
	EXPECT_EQ(ClampTerrainMaxMipLevel(kTerrainMaxMipDefault), 3);
	// 1 remains reachable and is the stock D3D/3DFX cap; 0 disables mip
	// filtering.
	EXPECT_EQ(ClampTerrainMaxMipLevel(1), 1);
	EXPECT_EQ(ClampTerrainMaxMipLevel(3), 3);
	EXPECT_EQ(ClampTerrainMaxMipLevel(0), 0);
	// A hand-edited config must not index past the allocated levels.
	EXPECT_EQ(ClampTerrainMaxMipLevel(4), 3);
	EXPECT_EQ(ClampTerrainMaxMipLevel(99), 3);
	EXPECT_EQ(ClampTerrainMaxMipLevel(-1), 0);
}
