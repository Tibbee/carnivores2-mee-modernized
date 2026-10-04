#include <gtest/gtest.h>
#include "Renderer/SunDepthOcclusion.h"
#include "Core/Constants.h"
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>

TEST(SunDepthOcclusion, OnScreenRetainsCachedDepthAttenuation)
{
    for (float factor : {0.0f, 0.25f, 0.75f, 1.0f})
        EXPECT_FLOAT_EQ(ApplySunDepthFactor(140.0f, factor, 1280, 720, 2560, 1440),
                        140.0f * factor);
}

TEST(SunDepthOcclusion, KeepsExistingInclusiveSampleMargin)
{
    for (const auto& point : std::array<std::array<int, 2>, 4>{
             {{10, 10}, {2550, 10}, {10, 1430}, {2550, 1430}}}) {
        EXPECT_TRUE(SunTracePositionOnScreen(point[0], point[1], 2560, 1440));
        EXPECT_FLOAT_EQ(ApplySunDepthFactor(80.0f, 0.5f,
            point[0], point[1], 2560, 1440), 40.0f);
    }
}

TEST(SunDepthOcclusion, EveryCurrentScreenEdgeZerosEvenWithVisiblePreviousDepth)
{
    for (const auto& point : std::array<std::array<int, 2>, 8>{
             {{9, 720}, {2551, 720}, {1280, 9}, {1280, 1431},
              {-100, 720}, {2800, 720}, {1280, -100}, {1280, 1600}}}) {
        EXPECT_FALSE(SunTracePositionOnScreen(point[0], point[1], 2560, 1440));
        EXPECT_FLOAT_EQ(ApplySunDepthFactor(140.0f, 1.0f,
            point[0], point[1], 2560, 1440), 0.0f);
    }
}

TEST(SunDepthOcclusion, BinocularZoomZerosCurrentProjectionImmediately)
{
    // Standard daytime sun can rotate to this direction. Both phases use
    // the production FOV projection, not a copied projection implementation.
    const float x = 1500.0f;
    const float z = -std::sqrt(2048.0f * 2048.0f * 2.0f +
                              4048.0f * 4048.0f - x * x);
    const float cameraW = 720.0f * FovScaleFromDegrees(35);
    const int normalX = 1280 + static_cast<int>(x / -z * cameraW);
    const int binocularX = 1280 + static_cast<int>(x / -z * cameraW * 2.0f);
    ASSERT_TRUE(SunTracePositionOnScreen(normalX, 720, 2560, 1440));
    ASSERT_FALSE(SunTracePositionOnScreen(binocularX, 720, 2560, 1440));
    const float light = 220.0f - x * 220.0f / 2048.0f;
    ASSERT_GT(light, 1.0f);
    const float cachedFactor = 0.75f; // previous, unzoomed depth sample
    EXPECT_GT(ApplySunDepthFactor(light, cachedFactor, normalX, 720, 2560, 1440), 1.0f);
    EXPECT_FLOAT_EQ(ApplySunDepthFactor(light, cachedFactor,
        binocularX, 720, 2560, 1440), 0.0f);
}

TEST(SunDepthOcclusion, EdgeExitDoesNotResetSmoothingOrBypassReentryLatency)
{
    const float cachedFactor = 0.5f;
    EXPECT_FLOAT_EQ(ApplySunDepthFactor(80.0f, cachedFactor, 2551, 720, 2560, 1440), 0.0f);
    EXPECT_FLOAT_EQ(ApplySunDepthFactor(80.0f, cachedFactor, 1280, 720, 2560, 1440), 40.0f);
    // A previous off-screen sample remains zero on the first reentry frame.
    EXPECT_FLOAT_EQ(ApplySunDepthFactor(80.0f, 0.0f, 1280, 720, 2560, 1440), 0.0f);
}

TEST(SunDepthOcclusion, CurrentViewportControlsGuardAfterResize)
{
    EXPECT_GT(ApplySunDepthFactor(80.0f, 1.0f, 2000, 720, 2560, 1440), 0.0f);
    EXPECT_FLOAT_EQ(ApplySunDepthFactor(80.0f, 1.0f, 2000, 720, 1920, 1080), 0.0f);
}

TEST(SunDepthOcclusion, RendererUsesProductionPolicyWithoutEndOfFrameReadback)
{
    std::ifstream file(std::string(C2_SOURCE_ROOT) + "/Hunt/Renderer/GLSky.cpp");
    ASSERT_TRUE(file.good());
    const std::string source{std::istreambuf_iterator<char>(file), {}};
    const auto start = source.find("void GLRenderer::ApplySunDepthOcclusion()");
    ASSERT_NE(start, std::string::npos);
    const auto end = source.find("\n}", start);
    ASSERT_NE(end, std::string::npos);
    const auto body = source.substr(start, end - start);
    EXPECT_NE(body.find("ApplySunDepthFactor(m_sunLight, m_traceKApplied,"), std::string::npos);
    EXPECT_NE(body.find("m_sunScrX, m_sunScrY, WinW, WinH"), std::string::npos);
    EXPECT_EQ(body.find("glReadPixels"), std::string::npos);
    EXPECT_EQ(body.find("m_traceKApplied ="), std::string::npos);
    EXPECT_NE(source.find("if (!SunTracePositionOnScreen(x, y, WinW, WinH))"), std::string::npos);
}
