// Unit tests for Hunt/Core/WaterWaveFade.h -- the distance falloff applied to
// the water-surface wave displacement in PreCashGroundModel.
#include <gtest/gtest.h>

#include "WaterWaveFade.h"

TEST(WaterWaveFadeTest, ZeroRingsAndTheOutermostRingsReproduceTheLegacyCutoff)
{
	// Independent of the ramp width, the outermost kWaterWaveCutoffRings
	// stay flat -- that is the pre-existing rule and this change only softens
	// the approach to it.
	for (int viewRadius : {24, 48, 96, 230}) {
		for (int falloff : {0, 1, 8, 32, 64}) {
			for (int ring = viewRadius - kWaterWaveCutoffRings; ring <= viewRadius + 3; ++ring) {
				EXPECT_FLOAT_EQ(WaterWaveFade(ring, viewRadius, falloff), 0.0f);
			}
		}
	}

	// Legacy gate was `r < ctViewR - 4`: full amplitude strictly inside the
	// cutoff, nothing from the cutoff outward. The ramp must reduce to exactly
	// that when falloffRings is 0, so the setting is a true opt-in.
	const int viewRadius = 48;
	const int cutoff = viewRadius - kWaterWaveCutoffRings;
	EXPECT_EQ(cutoff, 44);
	for (int ring = 0; ring < cutoff; ++ring) {
		EXPECT_FLOAT_EQ(WaterWaveFade(ring, viewRadius, 0), 1.0f) << "ring " << ring;
	}
	for (int ring = cutoff; ring <= viewRadius + 3; ++ring) {
		EXPECT_FLOAT_EQ(WaterWaveFade(ring, viewRadius, 0), 0.0f) << "ring " << ring;
	}
}

TEST(WaterWaveFadeTest, FalloffIsMonotonicAndWiderRampsQuieterNearTheCutoff)
{
	// Amplitude is full near the camera and decays toward the horizon, so
	// walking the rings outward must never increase.
	const int viewRadius = 48;
	const int falloff = 8;
	float previous = 1.0f;
	for (int ring = 0; ring <= viewRadius + 3; ++ring) {
		const float fade = WaterWaveFade(ring, viewRadius, falloff);
		EXPECT_GE(fade, 0.0f);
		EXPECT_LE(fade, 1.0f);
		EXPECT_LE(fade, previous) << "fade must not increase outward at ring " << ring;
		previous = fade;
	}
	EXPECT_FLOAT_EQ(WaterWaveFade(0, viewRadius, falloff), 1.0f);
	EXPECT_FLOAT_EQ(previous, 0.0f);

	// A wider ramp must spend more rings below full amplitude, so a single
	// ring near the cutoff is quieter with a wide ramp than a narrow one.
	const int cutoff = viewRadius - kWaterWaveCutoffRings;
	const int ring = cutoff - 1;
	EXPECT_LT(WaterWaveFade(ring, viewRadius, 16), WaterWaveFade(ring, viewRadius, 2));
	EXPECT_LT(WaterWaveFade(ring, viewRadius, 2), WaterWaveFade(ring, viewRadius, 0));
}

TEST(WaterWaveFadeTest, DegenerateInputDoesNotMisbehave)
{
	// The per-vertex loop evaluates this for every grid cell, including ones
	// outside the view radius, so odd values must clamp rather than invert.
	EXPECT_FLOAT_EQ(WaterWaveFade(0, 0, 0), 0.0f);
	EXPECT_FLOAT_EQ(WaterWaveFade(0, 0, 8), 0.0f);
	EXPECT_FLOAT_EQ(WaterWaveFade(10000, 48, 8), 0.0f);
	EXPECT_FLOAT_EQ(WaterWaveFade(-5, 48, 8), 1.0f);   // negative ring: fully interior
	EXPECT_FLOAT_EQ(WaterWaveFade(10, 48, -4), 1.0f);   // negative width: legacy gate
	EXPECT_FLOAT_EQ(WaterWaveFade(44, 48, -4), 0.0f);
}
