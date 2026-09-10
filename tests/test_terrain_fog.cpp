#include <gtest/gtest.h>

#include "TerrainFog.h"
#include "RenderTypes.h"

#include <array>

TEST(TerrainFogTest, ClearMountainRetainsCameraPocketFog)
{
	// Triassic AREA4: an empty mountain cell viewed from grey pocket 1.
	// A representative camera-to-point calculation was about 175/255;
	// collection used to discard it solely because the destination index is 0.
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 175, true, false, true, 1), 175.0f);
	EXPECT_EQ(ResolveTerrainFogIndex(0, true, 1), 1); // same pocket for colour
}

TEST(TerrainFogTest, ClearTerrainUsesGreenCameraPocketColourIndex)
{
	// Legacy AREA3's green pocket is slot 5. The fallback must not be a
	// hard-coded default fog (slot 1), sky colour (slot 0), or stale CurFogColor.
	EXPECT_EQ(ResolveTerrainFogIndex(0, true, 5), 5);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 200, true, false, true, 5), 200.0f);
}

TEST(TerrainFogTest, DestinationPocketKeepsItsOwnAmountAndColourIndex)
{
	// Preserve CalcFogLevel's dominant-volume rule at differently coloured
	// boundaries; this does not blend or rewrite authored/mortal volume indices.
	EXPECT_EQ(ResolveTerrainFogIndex(2, true, 5), 2);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(2, 90, true, false, true, 5), 90.0f);
}

TEST(TerrainFogTest, OutsideViewOfPocketIsUnchanged)
{
	EXPECT_EQ(ResolveTerrainFogIndex(5, false, 0), 5);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(5, 200, true, false, false, 0), 200.0f);
}

TEST(TerrainFogTest, ClearTerrainStaysClearWhenCameraIsOutside)
{
	// CameraFogI can remain nonzero when the eye is above the fog top.
	EXPECT_EQ(ResolveTerrainFogIndex(0, false, 5), 0);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 175, true, false, false, 5), 0.0f);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 175, true, false, true, 0), 0.0f);
}

TEST(TerrainFogTest, DisabledFogDoesNotRetainStaleAmounts)
{
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 175, false, false, true, 1), 0.0f);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(5, 200, false, false, true, 5), 0.0f);
}

TEST(TerrainFogTest, CameraFallbackDoesNotInventOrMultiplyFog)
{
	// Zero also occurs on above-water surfaces and submerged terrain viewed
	// from above water. Resolving a colour must not add fog to these samples.
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 0, true, false, true, 1), 0.0f);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 20, true, false, true, 1), 20.0f);
}

TEST(TerrainFogTest, PocketAmountsKeepExistingClamping)
{
	for (int index : {0, 5}) {
		EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(index, -1, true, false, true, 5), 0.0f);
		EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(index, 256, true, false, true, 5), 255.0f);
	}
}

TEST(TerrainFogTest, UnderwaterKeepsItsOwnSoftCapAndToggleBehaviour)
{
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(0, 300, false, true, false, 127), 300.0f);
	EXPECT_FLOAT_EQ(ResolveTerrainFogAmount(5, 200, true, true, true, 127), 200.0f);
}

TEST(TerrainFogTest, VolumeSelectionPreservesByteRangeUsedByLegacyCalculation)
{
	EXPECT_EQ(ResolveTerrainFogIndex(255, true, 5), 255);
	EXPECT_EQ(ResolveTerrainFogIndex(0, true, 255), 255);
}

TEST(TerrainUVTest, LookupPreservesAllLegacyMappings)
{
	// Each pair selects kMin (0) or kMax (1). Rows are indexed by
	// reverse:bit 3, second-triangle:bit 2, direction:bits 0-1.
	constexpr std::array<std::array<int, 6>, 16> expected = {{
		{{0,0, 1,0, 1,1}}, {{0,1, 0,0, 1,0}},
		{{1,1, 0,1, 0,0}}, {{1,0, 1,1, 0,1}},
		{{0,0, 1,1, 0,1}}, {{0,1, 1,0, 1,1}},
		{{1,1, 0,0, 1,0}}, {{1,0, 0,1, 0,0}},
		{{0,0, 1,0, 0,1}}, {{0,1, 0,0, 1,1}},
		{{1,1, 0,1, 1,0}}, {{1,0, 1,1, 0,0}},
		{{0,1, 1,0, 1,1}}, {{1,1, 0,0, 1,0}},
		{{1,0, 0,1, 0,0}}, {{0,0, 1,1, 0,1}}
	}};

	for (int reverse = 0; reverse < 2; ++reverse) {
		for (int second = 0; second < 2; ++second) {
			for (int direction = 0; direction < 4; ++direction) {
				const int index = reverse * 8 + second * 4 + direction;
				const auto& actual = TerrainUV::Get(reverse != 0, second != 0, direction);
				for (int vertex = 0; vertex < 3; ++vertex) {
					const float expectedU = expected[index][vertex * 2] ? TerrainUV::kMax : TerrainUV::kMin;
					const float expectedV = expected[index][vertex * 2 + 1] ? TerrainUV::kMax : TerrainUV::kMin;
					EXPECT_FLOAT_EQ(actual[vertex].x, expectedU) << "mapping=" << index << " vertex=" << vertex;
					EXPECT_FLOAT_EQ(actual[vertex].y, expectedV) << "mapping=" << index << " vertex=" << vertex;
				}
			}
		}
	}
}

TEST(TerrainUVTest, LookupReturnsStableTableStorage)
{
	const auto& first = TerrainUV::Get(false, false, 0);
	const auto& again = TerrainUV::Get(false, false, 0);
	EXPECT_EQ(&first, &again);
}
