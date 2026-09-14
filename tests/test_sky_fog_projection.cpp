// ============================================================================
// test_sky_fog_projection.cpp -- Optic-invariant legacy sky fog projection
// ============================================================================

#include <gtest/gtest.h>

#include "Renderer/SkyFogProjection.h"

#include <algorithm>
#include <cmath>

namespace {

float LegacySkyFogDt(const skyfog::ProjectionCoefficients& coefficients,
                     float halfViewportWidth,
                     float sy)
{
    const Vector3d& q = coefficients.q;
    const Vector3d& p = coefficients.p;
    const Vector3d& r = coefficients.r;

    const float leftQ = q.x * (-halfViewportWidth) + q.y * sy + q.z;
    const float rightQ = q.x * halfViewportWidth + q.y * sy + q.z;
    const float leftU = (p.x * (-halfViewportWidth) + p.y * sy + p.z) /
                        (std::max)(std::fabs(leftQ), 0.001f);
    const float leftV = (r.x * (-halfViewportWidth) + r.y * sy + r.z) /
                        (std::max)(std::fabs(leftQ), 0.001f);
    const float rightU = (p.x * halfViewportWidth + p.y * sy + p.z) /
                         (std::max)(std::fabs(rightQ), 0.001f);
    const float rightV = (r.x * halfViewportWidth + r.y * sy + r.z) /
                         (std::max)(std::fabs(rightQ), 0.001f);

    const float dx = rightU - leftU;
    const float dy = rightV - leftV;
    return (std::clamp)(std::sqrt(dx * dx + dy * dy) / 96.0f - 6.0f,
                        0.0f, 10.0f);
}

skyfog::ProjectionCoefficients BuildProjection(float cameraW, float cameraH)
{
    // Camera yaw = pitch = 0 with the inherited -0.15-radian sky-plane
    // offset. These values exercise non-zero q.z/r.z terms without requiring
    // renderer globals or a GL context.
    constexpr float kSinPitchOffset = 0.149438132f;
    constexpr float kCosPitchOffset = 0.988771078f;
    const Vector3d normal = {0.0f, -kCosPitchOffset, -kSinPitchOffset};
    const Vector3d tangentX = {0.004f, 0.0f, 0.0f};
    const Vector3d tangentY = {0.0f, -0.004f * kSinPitchOffset,
                                0.004f * kCosPitchOffset};
    constexpr float kPlaneP = -32768.0f;
    constexpr float kDdx = 0.0f;
    const float ddy = 32768.0f * tangentY.y;

    return skyfog::BuildProjectionCoefficients(normal, tangentX, tangentY,
                                                kPlaneP, kDdx, ddy,
                                                cameraW, cameraH);
}

void ExpectCoefficientsNear(const skyfog::ProjectionCoefficients& actual,
                            const skyfog::ProjectionCoefficients& expected)
{
    EXPECT_NEAR(actual.q.x, expected.q.x, 0.0001f);
    EXPECT_NEAR(actual.q.y, expected.q.y, 0.0001f);
    EXPECT_NEAR(actual.q.z, expected.q.z, 0.0001f);
    EXPECT_NEAR(actual.p.x, expected.p.x, 0.0001f);
    EXPECT_NEAR(actual.p.y, expected.p.y, 0.0001f);
    EXPECT_NEAR(actual.p.z, expected.p.z, 0.0001f);
    EXPECT_NEAR(actual.r.x, expected.r.x, 0.0001f);
    EXPECT_NEAR(actual.r.y, expected.r.y, 0.0001f);
    EXPECT_NEAR(actual.r.z, expected.r.z, 0.0001f);
}

} // namespace

TEST(SkyFogProjection, ReferenceProjectionPreservesEquivalentOpticRays)
{
    constexpr float kBaseCameraW = 400.0f;
    constexpr float kBaseCameraH = 300.0f;
    constexpr float kHalfViewportWidth = 400.0f;
    const skyfog::ProjectionCoefficients baseProjection =
        BuildProjection(kBaseCameraW, kBaseCameraH);

    for (const float zoom : {1.5f, 2.0f, 3.0f}) {
        const float scopedCameraW = kBaseCameraW * zoom;
        const float scopedCameraH = kBaseCameraH * zoom;
        const skyfog::ProjectionCoefficients activeProjection =
            BuildProjection(scopedCameraW, scopedCameraH);
        const skyfog::ProjectionCoefficients fogReferenceProjection =
            BuildProjection(scopedCameraW / zoom, scopedCameraH / zoom);

        // The texture projection really does remain zoomed.
        EXPECT_GT(std::fabs(activeProjection.q.y - baseProjection.q.y), 0.01f);
        // The fog-only projection returns to the player's chosen base FOV.
        ExpectCoefficientsNear(fogReferenceProjection, baseProjection);

        for (const float baseSy : {-150.0f, -75.0f, 0.0f, 75.0f, 150.0f}) {
            const float expected = LegacySkyFogDt(baseProjection,
                                                   kHalfViewportWidth, baseSy);
            const float scopedSy = baseSy * zoom;
            const float actual = LegacySkyFogDt(fogReferenceProjection,
                                                 kHalfViewportWidth,
                                                 scopedSy / zoom);
            EXPECT_NEAR(actual, expected, 0.0001f)
                << "at " << zoom << "x and reference row " << baseSy;
        }
    }
}
