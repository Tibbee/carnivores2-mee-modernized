#include <gtest/gtest.h>
#include "Hunt.h"
#include "Renderer/GLUtils.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

// Minimal engine state for the real GLFog.cpp sampler. These tests verify
// delegation/colour/coordinate contracts, NOT CalcFogLevel's density formula.
unsigned char FogsMap[512][512] = {};
TFogEntity FogsList[256] = {};
float CameraX, CameraY, CameraZ, CameraWaterDepthFactor;
int UNDERWATER, FOGON, CAMERAINFOG, CameraFogI, CurFogColor, ctViewR;

namespace {
int calcCalls, sampledIndex, calculatedColor;
float calculatedAmount;
Vector3d sampledPoint;
const Vector3d horizonColor = {0.3f, 0.4f, 0.8f};
}

float CalcFogLevel(Vector3d point, int cachedFogIndex)
{
    ++calcCalls;
    sampledPoint = point;
    sampledIndex = cachedFogIndex;
    CurFogColor = calculatedColor;
    return calculatedAmount;
}

float VectorLength(Vector3d v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
Vector3d GetDistanceFogColor() { return horizonColor; }
Vector3d DecodeFogColor(int rgb)
{
    return {(rgb & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, ((rgb >> 16) & 255) / 255.0f};
}
Vector3d DecodeFogColorBGR(int rgb)
{
    const Vector3d c = DecodeFogColor(rgb);
    return {c.z, c.y, c.x};
}

class FogSamplingTest : public testing::Test {
protected:
    void SetUp() override
    {
        std::memset(FogsMap, 0, sizeof(FogsMap));
        std::memset(FogsList, 0, sizeof(FogsList));
        CameraX = CameraY = CameraZ = CameraWaterDepthFactor = 0.0f;
        UNDERWATER = CAMERAINFOG = CameraFogI = 0;
        FOGON = 1;
        ctViewR = 96;
        CurFogColor = 0x00FF0000; // deliberately stale
        calcCalls = 0;
        sampledIndex = -1;
        calculatedAmount = 175.0f;
        calculatedColor = 0x00538680; // distinct R/G/B, green Legacy-like pocket
    }

    void ExpectColor(const Vector3d& actual, const Vector3d& expected)
    {
        EXPECT_FLOAT_EQ(actual.x, expected.x);
        EXPECT_FLOAT_EQ(actual.y, expected.y);
        EXPECT_FLOAT_EQ(actual.z, expected.z);
    }
};

TEST_F(FogSamplingTest, InlinePocketPackingMatchesPublicSampler)
{
    // Exercise the exact loop's constant policy and byte-rounding boundaries.
    // CalcFogLevel remains the test double above: this checks sampler routing,
    // side effects, and packing rather than the pocket-density formula.
    const auto pack = [](const FogSample& fog) {
        const auto byte = [](float value) {
            return static_cast<std::uint8_t>(
                std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        return std::array<std::uint8_t, 4>{
            byte(fog.amount), byte(fog.color.x), byte(fog.color.y), byte(fog.color.z)};
    };
    const auto compare = [&](const Vector3d& point) {
        calcCalls = 0;
        sampledIndex = -1;
        CurFogColor = 0x123456;
        const auto expected = pack(SamplePocketFogAtPoint(point, false));
        const int expectedCalls = calcCalls;
        const int expectedIndex = sampledIndex;
        const int expectedColor = CurFogColor;

        calcCalls = 0;
        sampledIndex = -1;
        CurFogColor = 0x123456;
        EXPECT_EQ(pack(SampleFogAtPointInline<false>(point, false)), expected);
        EXPECT_EQ(calcCalls, expectedCalls);
        EXPECT_EQ(sampledIndex, expectedIndex);
        EXPECT_EQ(CurFogColor, expectedColor);
    };

    for (int z = 0; z < 512; ++z)
        for (int x = 0; x < 512; ++x)
            FogsMap[z][x] = static_cast<unsigned char>((x + z * 3) & 255);
    CameraX = 1024.25f;
    CameraZ = 2048.5f;
    for (int i = -1; i <= 256; ++i) {
        const float boundary = static_cast<float>(i) + 0.5f;
        for (float amount : {std::nextafter(boundary, -1000.0f), boundary,
                             std::nextafter(boundary, 1000.0f)}) {
            calculatedAmount = amount;
            calculatedColor = (i & 255) | (((i + 71) & 255) << 8) |
                              (((i + 129) & 255) << 16);
            for (int enabled : {0, 1}) {
                FOGON = enabled;
                for (float distance : {-262656.0f, -512.0f, 0.0f, 511.99f, 512.0f,
                                       18432.0f, 21504.0f, 24576.0f, 262144.0f})
                    compare({distance, -64.0f, distance});
            }
        }
    }

    UNDERWATER = 1;
    auto& water = FogsList[127];
    water.YBegin = 32.0f;
    water.fogRGB = 0x123456;
    for (float cameraY : {0.0f, 2048.0f, 2200.0f})
        for (float vertexY : {-500.0f, 0.0f, 500.0f})
            for (float depth : {0.0f, 0.25f, 0.5f, 1.0f})
                for (float transparency : {64.0f, 512.0f, 4096.0f})
                    for (float cap : {0.0f, 100.0f, 200.0f, 255.0f}) {
                        CameraY = cameraY;
                        CameraWaterDepthFactor = depth;
                        water.Transp = transparency;
                        water.FLimit = cap;
                        compare({512.0f, vertexY, 1000.0f});
                    }
}

TEST_F(FogSamplingTest, ClearCharacterCellRetainsCameraSegmentAndShiftedColour)
{
    CAMERAINFOG = 1;
    CameraFogI = 5;
    FogsList[5].fogRGB = 0x007F7F7F; // unshifted authored colour must NOT win
    const FogSample fog = SampleFogAtPoint({0.0f, 0.0f, 2000.0f}, false);
    EXPECT_EQ(calcCalls, 1);
    EXPECT_EQ(sampledIndex, 0); // critical: do not pre-resolve to slot 5!
    EXPECT_FLOAT_EQ(fog.amount, 175.0f / 255.0f);
    ExpectColor(fog.color, DecodeFogColor(calculatedColor));
}

TEST_F(FogSamplingTest, DestinationPocketIndexIsPassedThrough)
{
    CAMERAINFOG = 1;
    CameraFogI = 5;
    FogsMap[3][2] = 2;
    SampleFogAtPoint({1024.0f, 0.0f, 1536.0f}, false);
    EXPECT_EQ(sampledIndex, 2);
}

TEST_F(FogSamplingTest, CameraRelativePointIsNotTranslatedTwice)
{
    CameraX = 1024.0f; CameraY = 500.0f; CameraZ = 2048.0f;
    FogsMap[3][3] = 7;
    const Vector3d point = {512.0f, -300.0f, -512.0f};
    SampleFogAtPoint(point, false);
    EXPECT_EQ(sampledIndex, 7);
    ExpectColor(sampledPoint, point);
}

TEST_F(FogSamplingTest, MapBoundaryUsesMaskedIndexInsteadOfUncheckedLookup)
{
    FogsMap[511][511] = 255;
    SampleFogAtPoint({-512.0f, 0.0f, -512.0f}, false);
    EXPECT_EQ(sampledIndex, 255);
}

TEST_F(FogSamplingTest, DisabledPocketFogDoesNotInvokeCalculation)
{
    FOGON = 0;
    CAMERAINFOG = 1;
    CameraFogI = 5;
    const FogSample fog = SampleFogAtPoint({0.0f, 0.0f, 2000.0f}, false);
    EXPECT_EQ(calcCalls, 0);
    EXPECT_FLOAT_EQ(fog.amount, 0.0f);
    ExpectColor(fog.color, horizonColor);
}

TEST_F(FogSamplingTest, NearModelBypassSkipsPocketAndDistanceSampling)
{
    const FogSample fog = SampleFogAtPoint({0.0f, 0.0f, 100000.0f}, true);
    EXPECT_EQ(calcCalls, 0);
    EXPECT_FLOAT_EQ(fog.amount, 0.0f);
}

TEST_F(FogSamplingTest, ClearDestinationOutsidePocketSkipsCalculation)
{
    const FogSample fog = SampleFogAtPoint({0.0f, 0.0f, 2000.0f}, false);
    EXPECT_EQ(calcCalls, 0);
    EXPECT_FLOAT_EQ(fog.amount, 0.0f);
    ExpectColor(fog.color, horizonColor);
}

TEST_F(FogSamplingTest, ZeroContributionDoesNotLeakStalePocketColour)
{
    FogsMap[3][0] = 5;
    calculatedAmount = 0.0f;
    const FogSample fog = SampleFogAtPoint({0.0f, 0.0f, 2000.0f}, false);
    EXPECT_EQ(calcCalls, 1);
    EXPECT_FLOAT_EQ(fog.amount, 0.0f);
    ExpectColor(fog.color, horizonColor);
}

TEST_F(FogSamplingTest, FullSamplerKeepsCpuHorizonFallbackForScreenSpaceConsumers)
{
    const float halfway = ctViewR * 224.0f;
    FogsMap[42][0] = 5;
    calculatedAmount = 0.0f;
    const FogSample fog = SampleFogAtPoint({0.0f, 0.0f, halfway}, false);
    EXPECT_FLOAT_EQ(fog.amount, 0.5f);
    ExpectColor(fog.color, horizonColor);
}

TEST_F(FogSamplingTest, PocketOnlySamplerDoesNotDoubleApplyShaderDistanceFog)
{
    const FogSample fog = SamplePocketFogAtPoint(
        {0.0f, 0.0f, ctViewR * 224.0f}, false);
    EXPECT_EQ(calcCalls, 0);
    EXPECT_FLOAT_EQ(fog.amount, 0.0f);
    ExpectColor(fog.color, horizonColor);
}

TEST_F(FogSamplingTest, ShadowGroundSampleStillReceivesPocketFog)
{
    CameraY = 2000.0f;
    const Vector3d ground = {512.0f, -1500.0f, 2000.0f};
    FogsMap[3][1] = 5;
    const FogSample fog = SamplePocketFogAtPoint(ground, false);
    EXPECT_FLOAT_EQ(fog.amount, 175.0f / 255.0f);
    ExpectColor(sampledPoint, ground);
    ExpectColor(fog.color, DecodeFogColor(calculatedColor));
}

TEST_F(FogSamplingTest, PocketAmountIsClampedWithoutDensityMultiplier)
{
    FogsMap[3][0] = 5;
    for (float amount : {-10.0f, 20.0f, 200.0f, 256.0f, 300.0f}) {
        calculatedAmount = amount;
        const FogSample fog = SamplePocketFogAtPoint({0.0f, 0.0f, 2000.0f}, false);
        EXPECT_FLOAT_EQ(fog.amount, std::clamp(amount / 255.0f, 0.0f, 1.0f));
    }
}

TEST_F(FogSamplingTest, UnderwaterPreservesLegacyGradientCapAndBgrColour)
{
    UNDERWATER = 1;
    FOGON = 0; // underwater is independent of the pocket toggle
    auto& water = FogsList[127];
    water.YBegin = 32.0f;
    water.Transp = 512.0f;
    water.FLimit = 200.0f;
    water.fogRGB = 0x00123456;
    // Reference is the pre-correction underwater sampler formula.
    for (float cameraY : {0.0f, 2048.0f, 2200.0f}) {
        for (float vertexY : {-500.0f, 0.0f, 500.0f}) {
            for (float depth : {0.0f, 0.5f, 1.0f}) {
                CameraY = cameraY;
                CameraWaterDepthFactor = depth;
                Vector3d point = {0.0f, vertexY, 1000.0f};
                float d = VectorLength(point);
                auto originalBase = [&]() {
                    float a = -(point.y + CameraY - water.YBegin * ctHScale) / ctHScale;
                    float b = -(CameraY - water.YBegin * ctHScale) / ctHScale;
                    if (a < 0.0f && b < 0.0f) return 0.0f;
                    if (a < 0.0f) { d *= b / (b-a); a = 0.0f; }
                    if (b < 0.0f) { d *= a / (a-b); b = 0.0f; }
                    return std::clamp((a+b) * (d+water.Transp*0.5f) / water.Transp, 0.0f, water.FLimit);
                };
                float fl = originalBase();
                if (fl <= 0.0f) fl = (d+water.Transp*0.5f) / water.Transp;
                fl *= 1.0f + (1.0f-std::exp(-depth*3.5f))*0.5f;
                const float vert = (std::max)(0.0f, water.YBegin*ctHScale-(point.y+CameraY));
                fl *= 1.0f + std::clamp(vert/512.0f, 0.0f, 1.0f)*2.5f;
                const float cap = water.FLimit + (1.0f-std::exp(-depth*2.0f))*50.0f;
                const float expected = std::clamp((std::min)(fl, cap)/255.0f, 0.0f, cap/255.0f);
                const FogSample fog = SampleFogAtPoint(point, false);
                EXPECT_FLOAT_EQ(fog.amount, expected);
                ExpectColor(fog.color, DecodeFogColorBGR(water.fogRGB));
            }
        }
    }
    EXPECT_EQ(calcCalls, 0);
}
