// Unit tests for Hunt/Core/Flashlight.h -- the beam policy the GL shaders and
// the game state both read. The shader blocks in shaders/*.frag re-implement
// ConeTerm/DistanceTerm/SurfaceGain by hand (GLSL has no way to include this
// header), so the values pinned here are the curve those shaders must match.
#include <gtest/gtest.h>

#include "Flashlight.h"

namespace {

constexpr float kEps = 1e-5f;
} // namespace

TEST(FlashlightTest, DeadHunterCannotIlluminateEvenWithAStaleToggle)
{
    for (bool equipped : {false, true})
        for (bool switchedOn : {false, true})
            for (int intensity : {0, 500, 1000})
                for (int health : {-1, 0, 1, 100000})
                    EXPECT_EQ(FlashlightShouldIlluminate(equipped, switchedOn, intensity, health),
                        equipped && switchedOn && intensity > 0 && health > 0);
}

TEST(FlashlightTest, DefaultsDescribeATorchNotASearchlight)
{
    // The default key sits next to the other toggles and never collides with
    // the WASD block or the 'N' night-vision binding.
    EXPECT_EQ(kFlashlightKeyDefault, 'F');
    EXPECT_NE(kFlashlightKeyDefault, 0x4E); // 'N', night vision

    // A narrow, bright cone that reaches roughly a third of the default view
    // distance (OptViewR=128 -> ~73 cells -> ~18700 units).
    EXPECT_LT(kFlashlightInnerAngleDegDefault, kFlashlightOuterAngleDegDefault);
    EXPECT_GT(kFlashlightRangeDefault, 0.0f);
    EXPECT_LT(kFlashlightRangeDefault, 73.0f * 256.0f);

    // The tint is warm white, i.e. blue is the weakest channel.
    EXPECT_GT(kFlashlightColorR, kFlashlightColorG);
    EXPECT_GT(kFlashlightColorG, kFlashlightColorB);
}

TEST(FlashlightTest, CosineIsOrderedAgainstTheAngles)
{
    // The shaders compare cos(theta) against these thresholds. A wider angle
    // must therefore produce the SMALLER cosine, otherwise the cone is
    // inside-out and the beam lights everything except the aim direction.
    const float cosInner = FlashlightCosFromAngleDeg(kFlashlightInnerAngleDegDefault);
    const float cosOuter = FlashlightCosFromAngleDeg(kFlashlightOuterAngleDegDefault);
    EXPECT_LT(cosOuter, cosInner);
    EXPECT_NEAR(cosInner, 0.9702957f, kEps);
    EXPECT_NEAR(cosOuter, 0.8987940f, kEps);

    // Looking straight down the view axis is exactly cos 0.
    EXPECT_NEAR(FlashlightCosFromAngleDeg(0.0f), 1.0f, kEps);
    EXPECT_NEAR(FlashlightCosFromAngleDeg(90.0f), 0.0f, kEps);
}

TEST(FlashlightTest, ConeTermFadesFromFullToNothing)
{
    const float cosInner = FlashlightCosFromAngleDeg(kFlashlightInnerAngleDegDefault);
    const float cosOuter = FlashlightCosFromAngleDeg(kFlashlightOuterAngleDegDefault);

    EXPECT_FLOAT_EQ(FlashlightConeTerm(1.0f, cosInner, cosOuter), 1.0f);  // dead ahead
    EXPECT_FLOAT_EQ(FlashlightConeTerm(cosInner, cosInner, cosOuter), 1.0f);
    EXPECT_FLOAT_EQ(FlashlightConeTerm(cosOuter, cosInner, cosOuter), 0.0f);
    // Past the outer edge stays off rather than wrapping around.
    EXPECT_FLOAT_EQ(FlashlightConeTerm(0.0f, cosInner, cosOuter), 0.0f);
    EXPECT_FLOAT_EQ(FlashlightConeTerm(-1.0f, cosInner, cosOuter), 0.0f);

    // Monotonic between the edges, and the midpoint is the smoothstep
    // halfway value.
    const float mid = (cosInner + cosOuter) * 0.5f;
    EXPECT_NEAR(FlashlightConeTerm(mid, cosInner, cosOuter), 0.5f, kEps);
    EXPECT_LT(FlashlightConeTerm(cosOuter + 0.01f, cosInner, cosOuter),
              FlashlightConeTerm(mid, cosInner, cosOuter));
    EXPECT_GT(FlashlightConeTerm(mid + 0.01f, cosInner, cosOuter),
              FlashlightConeTerm(mid, cosInner, cosOuter));
}

TEST(FlashlightTest, ConeTermNeverRunsOnAnUnorderedPair)
{
    // A degenerate config (inner >= outer) would give smoothstep edge0 >=
    // edge1, which is undefined in GLSL. The CPU guard reports no beam.
    EXPECT_FLOAT_EQ(FlashlightConeTerm(1.0f, 0.5f, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(FlashlightConeTerm(1.0f, 0.5f, 0.9f), 0.0f);
}

TEST(FlashlightTest, DistanceTermReachesZeroExactlyAtTheRange)
{
    EXPECT_FLOAT_EQ(FlashlightDistanceTerm(0.0f, 6000.0f), 1.0f);
    EXPECT_FLOAT_EQ(FlashlightDistanceTerm(6000.0f, 6000.0f), 0.0f);
    EXPECT_FLOAT_EQ(FlashlightDistanceTerm(12000.0f, 6000.0f), 0.0f);

    // Squared linear falloff: halfway is a quarter of full strength.
    EXPECT_NEAR(FlashlightDistanceTerm(3000.0f, 6000.0f), 0.25f, kEps);
    EXPECT_GT(FlashlightDistanceTerm(1000.0f, 6000.0f),
              FlashlightDistanceTerm(2000.0f, 6000.0f));

    // A zero range cannot illuminate anything; a degenerate caller must not
    // produce NaN downstream.
    EXPECT_FLOAT_EQ(FlashlightDistanceTerm(0.0f, 0.0f), 0.0f);
}

TEST(FlashlightTest, SurfaceGainIsAHeadroomRollOff)
{
    // Without the blend-fix floor a multiplicative boost leaves black at
    // black; this floor is what makes unlit night ground show the beam. It has
    // to stay small: a large one lifts shadowed texels onto the same value and
    // erases the detail it was meant to reveal.
    EXPECT_GT(kFlashlightBlendFixFloor, 0.0f);
    EXPECT_LE(kFlashlightBlendFixFloor, 0.05f);

    // Black takes the whole beam, white none at all, monotonically between.
    EXPECT_NEAR(FlashlightSurfaceGain(0.0f), 1.0f, kEps);
    EXPECT_NEAR(FlashlightSurfaceGain(1.0f), 0.0f, kEps);
    EXPECT_GT(FlashlightSurfaceGain(0.2f), FlashlightSurfaceGain(0.5f));
    EXPECT_GT(FlashlightSurfaceGain(0.5f), FlashlightSurfaceGain(0.8f));
    // Out-of-range luminance clamps instead of inverting the curve.
    EXPECT_NEAR(FlashlightSurfaceGain(-5.0f), 1.0f, kEps);
    EXPECT_NEAR(FlashlightSurfaceGain(5.0f), 0.0f, kEps);
}

TEST(FlashlightTest, HeadroomCurveKeepsLitTerrainBelowClipping)
{
    // The wash-out regression. With a linear boost these surfaces pinned at
    // white and lost all visible texture detail; the cubed roll-off has to
    // leave each of them below 1.0 at the shipped default intensity.
    constexpr float kPeak = static_cast<float>(kFlashlightIntensityPctDefault) / 100.0f;
    const struct { const char* name; float luma; } kSurfaces[] = {
        {"night ground", 0.06f},   // what the beam exists to reveal
        {"dim rock", 0.25f},
        {"mid-tone", 0.50f},       // daylight average
        {"bright sand", 0.75f},
    };
    for (const auto& surface : kSurfaces) {
        const float amount = kPeak * FlashlightSurfaceGain(surface.luma);
        const float lit = surface.luma * (1.0f + amount);
        EXPECT_LT(lit, 1.0f) << surface.name << " clips at peak beam";
    }

    // Only the surfaces a hunter needs to see by torchlight have to gain
    // several stops. Bright ones are meant to be left almost alone -- asking
    // for 2x there is what produced the flat wash in the first place.
    EXPECT_GT(0.06f * (1.0f + kPeak * FlashlightSurfaceGain(0.06f)) / 0.06f, 2.0f);
    EXPECT_GT(0.25f * (1.0f + kPeak * FlashlightSurfaceGain(0.25f)) / 0.25f, 1.8f);

    // The dark end is where most of the beam has to land, or the torch does
    // not read as a torch.
    EXPECT_GT(FlashlightSurfaceGain(0.06f), 0.75f);
    // The bright end has to be effectively untouched.
    EXPECT_LT(FlashlightSurfaceGain(0.75f), 0.05f);
}

TEST(FlashlightTest, ConeNormalizationKeepsThePairOrdered)
{
    float inner = 0.0f;
    float outer = 0.0f;
    NormalizeFlashlightConeDeg(inner, outer);
    EXPECT_GE(inner, kFlashlightAngleDegMin);
    EXPECT_GE(outer, inner + 1.0f);

    // An inverted config (outer narrower than inner) is repaired rather than
    // passed to the shader, and never exceeds the ceiling.
    inner = 60.0f;
    outer = 10.0f;
    NormalizeFlashlightConeDeg(inner, outer);
    EXPECT_GT(outer, inner);

    inner = 50.0f;
    outer = 90.0f;
    NormalizeFlashlightConeDeg(inner, outer);
    EXPECT_LE(outer, kFlashlightAngleDegMax);

    // Both above the ceiling still ends up ordered and inside the cap.
    inner = 200.0f;
    outer = 300.0f;
    NormalizeFlashlightConeDeg(inner, outer);
    EXPECT_LE(outer, kFlashlightAngleDegMax);
    EXPECT_GE(outer, inner + 1.0f);
}

TEST(FlashlightTest, ConfigClampsStayInsideTheirRanges)
{
    EXPECT_EQ(ClampFlashlightKey(-1), 0);
    EXPECT_EQ(ClampFlashlightKey('F'), 'F');
    EXPECT_EQ(ClampFlashlightKey(9999), kFlashlightKeyMax);

    EXPECT_EQ(ClampFlashlightRange(0), static_cast<int>(kFlashlightRangeMin));
    EXPECT_EQ(ClampFlashlightRange(static_cast<int>(kFlashlightRangeDefault)),
              static_cast<int>(kFlashlightRangeDefault));
    EXPECT_EQ(ClampFlashlightRange(1000000), static_cast<int>(kFlashlightRangeMax));

    EXPECT_EQ(ClampFlashlightIntensityPct(-1), kFlashlightIntensityPctMin);
    EXPECT_EQ(ClampFlashlightIntensityPct(kFlashlightIntensityPctDefault),
              kFlashlightIntensityPctDefault);
    // The shipped default has to sit inside the range, not merely clamp into
    // it: a default the config parser would reject is a default nobody loads.
    EXPECT_LE(kFlashlightIntensityPctDefault, kFlashlightIntensityPctMax);
    EXPECT_EQ(ClampFlashlightIntensityPct(99999), kFlashlightIntensityPctMax);
    // Zero has to be reachable: it is how a user disables the boost entirely.
    EXPECT_EQ(kFlashlightIntensityPctMin, 0);
}
