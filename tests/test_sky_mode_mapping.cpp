// Unit tests for the shared sky cloud-texture placement policy.
//
// The point of Core/SkyModeMapping.h is that the OpenGL path (evaluated in
// shaders/sky.frag) and the software path (evaluated in SoftSky.cpp) cannot
// drift apart, so these lock down the parts they both depend on.

#include <gtest/gtest.h>

#include "Core/SkyModeMapping.h"

using namespace skyl;

namespace {

// The engine's scale factor, as used by ComputeLegacySkyPitch's caller.
constexpr float kHeightScale = 0.01f;

float Deg(float radians) { return radians * 57.29577951308232f; }

float Dot(const Vector3d& a, const Vector3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(const Vector3d& v)
{
    return std::sqrt(Dot(v, v));
}

} // namespace

TEST(SkyModeMappingTest, ModeConstantsMatchTheEngineEnum)
{
    // Mirrors SkyMappingMode in Core/GameState.h, which static_asserts the
    // same agreement. Duplicated here so a rename cannot silently pass.
    EXPECT_EQ(kModeLegacy, 0);
    EXPECT_EQ(kModeLevel, 1);
    EXPECT_EQ(kModeDome, 2);
    EXPECT_EQ(kModeCount, 3);
}

TEST(SkyModeMappingTest, SoftwareResolvesLegacyAndLevelButNotDome)
{
    // The dome is a sky.frag feature with no software implementation, so it
    // must fall back rather than silently produce something else.
    EXPECT_EQ(ResolveSoftwareSkyMode(kModeLegacy), kModeLegacy);
    EXPECT_EQ(ResolveSoftwareSkyMode(kModeLevel), kModeLevel);
    EXPECT_EQ(ResolveSoftwareSkyMode(kModeDome), kModeLegacy);
    // Out-of-range and negative requests are also refused, not clamped up.
    EXPECT_EQ(ResolveSoftwareSkyMode(3), kModeLegacy);
    EXPECT_EQ(ResolveSoftwareSkyMode(-1), kModeLegacy);
}

TEST(SkyModeMappingTest, LegacyPitchOffsetShrinksWithAltitude)
{
    // At ground level the plane is offset by the full 0.20 rad...
    const float ground = ComputeLegacySkyPitch(0.0f, 0.0f, kHeightScale);
    EXPECT_NEAR(ground, -0.20f, 1e-5f);

    // ...and at 200 units of altitude the offset is halved to 0.10 rad, so a
    // high camera is not looking at the plane nearly edge-on.
    const float high = ComputeLegacySkyPitch(0.0f, -200.0f * kHeightScale, kHeightScale);
    EXPECT_NEAR(high, -0.10f, 1e-5f);

    // Beyond full altitude the offset stops shrinking.
    const float higher = ComputeLegacySkyPitch(0.0f, -5000.0f * kHeightScale, kHeightScale);
    EXPECT_NEAR(higher, -0.10f, 1e-5f);

    // A camera below terrain (positive Y) is treated as ground level rather
    // than inverting the altitude factor.
    const float below = ComputeLegacySkyPitch(0.0f, 5000.0f * kHeightScale, kHeightScale);
    EXPECT_NEAR(below, -0.20f, 1e-5f);

    // The pitch tracks the camera: it is an offset, not an absolute angle.
    EXPECT_NEAR(ComputeLegacySkyPitch(0.5f, 0.0f, kHeightScale), 0.5f - 0.20f, 1e-5f);
}

TEST(SkyModeMappingTest, SoftwareLegacyPitchKeepsThe1998FrozenOffset)
{
    // The software renderer deliberately does NOT use the altitude ramp: with
    // no mip chain the steeper ground-level tilt bands visibly. This test
    // pins the divergence so nobody "fixes" it back into a regression.
    for (float beta = -1.0f; beta <= 1.0f; beta += 0.25f) {
        EXPECT_NEAR(ComputeSoftwareLegacySkyPitch(beta), beta - 0.15f, 1e-6f);
    }

    // The OpenGL path ramps and the software path does not, so they CROSS at
    // mid altitude rather than differing by a constant. The frozen 0.15 is
    // the midpoint of the 0.20..0.10 ramp, which is why the 1998 constant was
    // a sensible choice: it is exactly right at half the altitude range.
    const float ground = ComputeLegacySkyPitch(0.0f, 0.0f, kHeightScale);
    const float mid = ComputeLegacySkyPitch(0.0f, -100.0f * kHeightScale, kHeightScale);
    const float full = ComputeLegacySkyPitch(0.0f, -200.0f * kHeightScale, kHeightScale);
    EXPECT_NEAR(ground, -0.20f, 1e-5f);
    EXPECT_NEAR(mid, -0.15f, 1e-5f);
    EXPECT_NEAR(full, -0.10f, 1e-5f);
    // Software subtracts MORE than GL near the ground and LESS at altitude.
    EXPECT_NEAR(ground - ComputeSoftwareLegacySkyPitch(0.0f), -0.05f, 1e-5f);
    EXPECT_NEAR(mid - ComputeSoftwareLegacySkyPitch(0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(full - ComputeSoftwareLegacySkyPitch(0.0f), 0.05f, 1e-5f);
}

TEST(SkyModeMappingTest, CameraBasisIsOrthonormalAndRightHanded)
{
    const float alpha = 0.7f;
    const float beta = -0.3f;
    const CameraBasis basis = ComputeCameraBasis(alpha, beta);

    // No camera roll means the right axis is always horizontal.
    EXPECT_NEAR(basis.right.y, 0.0f, 1e-6f);

    EXPECT_NEAR(Length(basis.right), 1.0f, 1e-5f);
    EXPECT_NEAR(Length(basis.up), 1.0f, 1e-5f);
    EXPECT_NEAR(Length(basis.forward), 1.0f, 1e-5f);
    EXPECT_NEAR(Dot(basis.right, basis.up), 0.0f, 1e-5f);
    EXPECT_NEAR(Dot(basis.right, basis.forward), 0.0f, 1e-5f);
    EXPECT_NEAR(Dot(basis.up, basis.forward), 0.0f, 1e-5f);
}

TEST(SkyModeMappingTest, WorldRayIsNormalisedAndIgnoresCameraPosition)
{
    const CameraBasis basis = ComputeCameraBasis(0.4f, 0.2f);
    const Vector3d centre = ComputeWorldRay(0.0f, 0.0f, 640.0f, 480.0f, basis);
    const Vector3d corner = ComputeWorldRay(320.0f, 240.0f, 640.0f, 480.0f, basis);

    // The screen centre is the view axis, which is the camera's forward
    // direction in world space -- not world +Z, because the camera is
    // yawed and pitched. (Positive CameraBeta pitches down in this engine.)
    EXPECT_NEAR(Length(centre), 1.0f, 1e-5f);
    EXPECT_NEAR(centre.x, basis.forward.x, 1e-5f);
    EXPECT_NEAR(centre.y, basis.forward.y, 1e-5f);
    EXPECT_NEAR(centre.z, basis.forward.z, 1e-5f);
    EXPECT_NEAR(Length(corner), 1.0f, 1e-5f);

    // A rightward pixel must tilt the ray to the right, and an upward pixel
    // (positive sy) must raise its elevation relative to the view axis.
    EXPECT_GT(corner.x, centre.x);
    EXPECT_GT(corner.y, centre.y);

    // Yawing the camera must change the ray. A view ray is the INVERSE of the
    // engine's world->view rotation, so a wrong-direction basis would leave
    // the ray unchanged; this pins the direction.
    const Vector3d yawed = ComputeWorldRay(0.0f, 0.0f, 640.0f, 480.0f,
                                           ComputeCameraBasis(0.4f + 1.0f, 0.2f));
    EXPECT_NE(centre.z, yawed.z);
}

TEST(SkyModeMappingTest, LevelPlaneFloorsTheSingularityInsteadOfDividingByZero)
{
    // A ray pointing exactly at the horizon has ray.y == 0, and with no drop
    // that is the compression singularity. The mapping must stay finite.
    const Vector3d horizon = {1.0f, 0.0f, 0.0f};
    float u = 0.0f;
    float v = 0.0f;
    ComputeLevelPlaneTexel(horizon, kPlaneTexelScale, 0.0f, 0.0f, 0.0f, &u, &v);
    EXPECT_TRUE(std::isfinite(u));
    EXPECT_TRUE(std::isfinite(v));

    // The floor is 0.001, so the coordinate is bounded by planeScale/0.001
    // texels, then carried in fixed point.
    const float bound = kPlaneTexelScale / 0.001f * kTexCoordScale;
    EXPECT_LE(std::fabs(u), bound * 1.001f);
}

TEST(SkyModeMappingTest, HorizonDropShiftsTheSingularityBelowTheHorizon)
{
    // With a drop, the singularity moves off ray.y == 0 and onto
    // ray.y == -dropSin. Sampling just above and just below that elevation
    // must be finite and continuous in magnitude.
    const float drop = HorizonDropSine(12.0f);
    EXPECT_GT(drop, 0.0f);

    const Vector3d atSingularity = {0.0f, -drop, 1.0f};
    float u = 0.0f;
    float v = 0.0f;
    ComputeLevelPlaneTexel(atSingularity, kPlaneTexelScale, drop, 0.0f, 0.0f, &u, &v);
    EXPECT_TRUE(std::isfinite(u));

    // The drop relocates the singularity from ray.y == 0 down to
    // ray.y == -dropSin, so the mapping must grow without bound as a ray
    // descends toward that new elevation. This is the property that keeps
    // the canopy compressed toward the horizon instead of running off to
    // the opposite side.
    auto magnitudeAt = [&](float elevation) {
        float u = 0.0f; float v = 0.0f;
        ComputeLevelPlaneTexel(Vector3d{1.0f, elevation, 0.0f},
                               kPlaneTexelScale, drop, 0.0f, 0.0f, &u, &v);
        return std::fabs(u);
    };
    const float nearSingular = magnitudeAt(-drop + 0.02f);
    const float midway = magnitudeAt(-drop + 0.20f);
    const float highInSky = magnitudeAt(0.40f);
    EXPECT_GT(nearSingular, midway);
    EXPECT_GT(midway, highInSky);
    // The drop also *reduces* the coordinate for a ray already high in the
    // sky, because the singularity moved away from it.
    float uWithDrop = 0.0f, vWithDrop = 0.0f, uNoDrop = 0.0f, vNoDrop = 0.0f;
    const Vector3d ray = {0.5f, 0.25f, 1.0f};
    ComputeLevelPlaneTexel(ray, kPlaneTexelScale, drop, 0.0f, 0.0f, &uWithDrop, &vWithDrop);
    ComputeLevelPlaneTexel(ray, kPlaneTexelScale, 0.0f, 0.0f, 0.0f, &uNoDrop, &vNoDrop);
    EXPECT_LT(std::fabs(uWithDrop), std::fabs(uNoDrop));
}

TEST(SkyModeMappingTest, LevelPlaneFollowsWorldAxesAndAnchor)
{
    // The mapping is driven by the world ray, so the canopy stays anchored to
    // world X/Z and does not lean while the camera turns. Two rays differing
    // only in azimuth must sample different parts of the texture.
    const CameraBasis north = ComputeCameraBasis(0.0f, 0.0f);
    const CameraBasis east  = ComputeCameraBasis(1.0f, 0.0f);
    float un = 0.0f, vn = 0.0f, ue = 0.0f, ve = 0.0f;
    const Vector3d rn = ComputeWorldRay(200.0f, 100.0f, 640.0f, 480.0f, north);
    const Vector3d re = ComputeWorldRay(200.0f, 100.0f, 640.0f, 480.0f, east);
    ComputeLevelPlaneTexel(rn, kPlaneTexelScale, 0.5f, 0.0f, 0.0f, &un, &vn);
    ComputeLevelPlaneTexel(re, kPlaneTexelScale, 0.5f, 0.0f, 0.0f, &ue, &ve);
    EXPECT_NE(un, ue);
    EXPECT_NE(vn, ve);

    // The anchor is a pure translation of the texture phase, so it cannot
    // change the scale of the mapping, only where it starts. In fixed point.
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    ComputeLevelPlaneTexel(rn, kPlaneTexelScale, 0.5f, 0.0f, 0.0f, &u0, &v0);
    ComputeLevelPlaneTexel(rn, kPlaneTexelScale, 0.5f, 100.0f, -50.0f, &u1, &v1);
    EXPECT_NEAR(u1 - u0, 100.0f * kTexCoordScale, 1.0f);
    EXPECT_NEAR(v1 - v0, -50.0f * kTexCoordScale, 1.0f);
}

TEST(SkyModeMappingTest, HorizonDropSineMatchesDegrees)
{
    EXPECT_NEAR(HorizonDropSine(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(HorizonDropSine(90.0f), 1.0f, 1e-4f);
    // The documented default of 12 degrees.
    EXPECT_NEAR(HorizonDropSine(12.0f), std::sin(12.0f * 3.14159265358979323846f / 180.0f), 1e-6f);
}

TEST(SkyModeMappingTest, TextureCoordinateIsFixedPointNotRawTexels)
{
    // Regression guard. The software scanline routines read the texel index
    // as (coord >> 16), so a coordinate expressed in raw texels collapses
    // everything to texel 0 and the sky renders from a single texel. The
    // level-plane mapping produces texel counts of order 10..130 at a normal
    // viewpoint, so it MUST be scaled into fixed point.
    EXPECT_EQ(kTexCoordScale, 65536.0f);

    // A ray a little above the horizon, at the capture scenario's camera.
    // In raw texels this is single digits; in fixed point it is a usable
    // in-range coordinate whose index is neither 0 nor saturated.
    const CameraBasis basis = ComputeCameraBasis(0.0f, 0.0f);
    const Vector3d ray = ComputeWorldRay(300.0f, 200.0f, 640.0f, 480.0f, basis);
    float u = 0.0f;
    float v = 0.0f;
    ComputeLevelPlaneTexel(ray, kPlaneTexelScale, HorizonDropSine(12.0f), 0.0f, 0.0f, &u, &v);

    // Raw texel value for the same ray, for contrast.
    const float denom = ray.y + HorizonDropSine(12.0f);
    const float rawTexels = ray.x * (kPlaneTexelScale / denom);
    EXPECT_LT(std::fabs(rawTexels), 512.0f);

    // In fixed point the index is a real texel number, not zero.
    const int index = static_cast<int>(u) >> 16;
    EXPECT_GT(std::abs(index), 0);
    EXPECT_LE(std::abs(index), 255);

    // The mask must still wrap on a 256-texel period.
    EXPECT_EQ(kTexCoordMask, 0xFFFFFF);
    EXPECT_EQ((kTexCoordMask >> 16) + 1, 256);
}
