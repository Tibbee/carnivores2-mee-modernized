#include <gtest/gtest.h>
#include "Renderer/CelestialState.h"
#include "Renderer/SunDepthOcclusion.h"
#include "Core/Constants.h"
#include "Core/NightLighting.h"
#include <limits>

TEST(CelestialState, StartupBehindCameraNeverUsesTheCorner) {
    // Stock night direction from InitEngine: positive Z faces behind the
    // default north-facing camera, explaining the owner's startup report.
    const auto moon = ProjectCelestialBody({3048, 3048, 3048}, 1280, 720, 1200, 1200);
    EXPECT_FALSE(moon.valid);
    EXPECT_EQ(moon.x, -1);
    EXPECT_EQ(moon.y, -1);
    EXPECT_FALSE(SunTracePositionOnScreen(moon.x, moon.y, 2560, 1440));
    for (int time : {0, 1, 2})
        EXPECT_FLOAT_EQ(CelestialHaloVisibility(moon.valid, CelestialInitialVisibility(time)), 0.0f);
}

TEST(CelestialState, CurrentProjectionTracksTurnsWithoutKeepingTheLastPosition) {
    const auto first = ProjectCelestialBody({0, 0, -4000}, 1280, 720, 1200, 1200);
    ASSERT_TRUE(first.valid);
    EXPECT_EQ(first.x, 1280);
    const auto moved = ProjectCelestialBody({1000, 200, -4000}, 1280, 720, 1200, 1200);
    ASSERT_TRUE(moved.valid);
    EXPECT_EQ(moved.x, 1580);
    EXPECT_EQ(moved.y, 660);
    const auto behind = ProjectCelestialBody({0, 0, 4000}, 1280, 720, 1200, 1200);
    EXPECT_FALSE(behind.valid);
    EXPECT_EQ(behind.x, -1);
    EXPECT_FLOAT_EQ(CelestialHaloVisibility(behind.valid, .25f), 0.0f);
    EXPECT_FLOAT_EQ(CelestialHaloVisibility(first.valid, .25f), .25f);
}

TEST(CelestialState, ProjectionPreservesLegacyOffsetTruncationAndDepthGate) {
    for (float x : {-4048.0f, -1500.25f, -3.9f, 0.0f, 3.9f, 1500.25f, 4048.0f})
    for (float y : {-3048.0f, -3.9f, 0.0f, 3.9f, 3048.0f})
    for (int fov : {35, 62, 90}) {
        const float w = 720 * FovScaleFromDegrees(fov);
        const auto p = ProjectCelestialBody({x, y, -4048}, 1280, 720, w, w);
        ASSERT_TRUE(p.valid);
        EXPECT_EQ(p.x, 1280 + static_cast<int>(x / 4048.0f * w));
        EXPECT_EQ(p.y, 720 - static_cast<int>(y / 4048.0f * w));
    }
    EXPECT_FALSE(ProjectCelestialBody({0, 0, -2024}, 1280, 720, 1200, 1200).valid);
    EXPECT_TRUE(ProjectCelestialBody({0, 0, -2025}, 1280, 720, 1200, 1200).valid);
}

TEST(CelestialState, ZoomResizeAndOffScreenPositionsRemainCurrent) {
    const float z = -std::sqrt(2048.0f * 2048.0f * 2.0f + 4048.0f * 4048.0f - 1500.0f * 1500.0f);
    const float w = 720 * FovScaleFromDegrees(35);
    const auto normal = ProjectCelestialBody({1500, 0, z}, 1280, 720, w, w);
    const auto zoom = ProjectCelestialBody({1500, 0, z}, 1280, 720, 2*w, 2*w);
    EXPECT_TRUE(SunTracePositionOnScreen(normal.x, normal.y, 2560, 1440));
    EXPECT_FALSE(SunTracePositionOnScreen(zoom.x, zoom.y, 2560, 1440));
    EXPECT_TRUE(zoom.valid); // Off-screen isn't behind; natural halo edge falloff stays.
    const auto resized = ProjectCelestialBody({0, 0, -4000}, 960, 540, 900, 900);
    EXPECT_EQ(resized.x, 960);
    EXPECT_EQ(resized.y, 540);
}

TEST(CelestialState, MalformedDirectionsCannotOverflowScreenCoordinates) {
    const float inf = (std::numeric_limits<float>::infinity)();
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    for (Vector3d direction : {Vector3d{inf, 0, -4000}, {0, nan, -4000}, {0, 0, -inf}, {1e38f, 0, -4000}})
        EXPECT_FALSE(ProjectCelestialBody(direction, 1280, 720, 1200, 1200).valid);
    EXPECT_FALSE(ProjectCelestialBody({0, 0, -4000}, 1280, 720, 0, 1200).valid);
}

TEST(CelestialState, MoonStartsAtItsClearSkyRemapRatherThanTheSunBaseline) {
    EXPECT_FLOAT_EQ(CelestialInitialVisibility(0), 1.0f);
    EXPECT_FLOAT_EQ(CelestialInitialVisibility(1), 1.0f);
    EXPECT_FLOAT_EQ(CelestialInitialVisibility(2), .32f);
    const float initial = CelestialInitialVisibility(2);
    for (float raw : {.05f, .2f, .5f, .9f, 1.0f}) {
        const float sampled = CelestialVisibilityFromSkySample(2, raw);
        EXPECT_FLOAT_EQ(sampled, .12f + raw / 5.0f);
        EXPECT_LE(sampled, initial);
        EXPECT_FLOAT_EQ(CelestialVisibilityFromSkySample(1, raw), raw);
    }
    const float alpha = NightCelestialDiscAlpha(2) * std::pow(initial, .6f);
    EXPECT_FLOAT_EQ(alpha, NightCelestialDiscAlpha(2) * std::pow(CelestialVisibilityFromSkySample(2, 1), .6f));
    EXPECT_LT(alpha, NightCelestialDiscAlpha(2));
}
