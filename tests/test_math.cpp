// ==========================================================================
// test_math.cpp -- Tests linked to the production Hunt/Math/Vector.cpp
// ==========================================================================
// Do not duplicate the math implementations: gameplay distance regressions
// must fail here even when an optimization originated in rendering work.

#include <gtest/gtest.h>
#include "Hunt.h"
#include <cmath>
#include <cstdlib>

// RotateVector's only engine-state dependencies. No game/window is launched.
float ca = 1.0f, sa = 0.0f, cb = 1.0f, sb = 0.0f;

// ===================== Tests =====================

TEST(VectorLengthTest, UnitX) {
    Vector3d v = {1.0f, 0.0f, 0.0f};
    EXPECT_FLOAT_EQ(VectorLength(v), 1.0f);
}

TEST(VectorLengthTest, Zero) {
    Vector3d v = {0.0f, 0.0f, 0.0f};
    EXPECT_FLOAT_EQ(VectorLength(v), 0.0f);
}

TEST(VectorLengthTest, Pythagoras) {
    Vector3d v = {3.0f, 4.0f, 0.0f};
    EXPECT_FLOAT_EQ(VectorLength(v), 5.0f);
}

TEST(VectorLengthTest, ThreeD) {
    Vector3d v = {2.0f, 3.0f, 6.0f};
    EXPECT_FLOAT_EQ(VectorLength(v), 7.0f); // sqrt(4+9+36) = sqrt(49)
}

TEST(VectorLengthTest, PreservesStrictGameplayThresholds) {
    // Projectile safety/recovery (128/300), trophy interaction (148), and
    // ammo pickup (200) all use strict comparisons against VectorLength.
    for (float threshold : {128.0f, 148.0f, 200.0f, 300.0f}) {
        EXPECT_LT(VectorLength({threshold * 0.999f, 0.0f, 0.0f}), threshold);
        EXPECT_GT(VectorLength({threshold * 1.001f, 0.0f, 0.0f}), threshold);
        EXPECT_FLOAT_EQ(VectorLength({threshold, 0.0f, 0.0f}), threshold);
    }
}

TEST(VectorLengthTest, TinyNonzeroVectorRetainsLength) {
    EXPECT_FLOAT_EQ(VectorLength({0.0f, 0.0f, 1.0e-6f}), 1.0e-6f);
}

TEST(VectorLengthSqTest, Simple) {
    Vector3d v = {3.0f, 4.0f, 0.0f};
    EXPECT_FLOAT_EQ(VectorLengthSq(v), 25.0f);
}

TEST(NormVectorTest, UnitScale) {
    Vector3d v = {100.0f, -200.0f, 50.0f};
    NormVector(v, 1.0f);
    EXPECT_NEAR(VectorLength(v), 1.0f, 0.001f);
}

TEST(NormVectorTest, ArbitraryScale) {
    Vector3d v = {3.0f, 0.0f, 0.0f};
    NormVector(v, 5.0f);
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(NormVectorTest, SmallMagnitude) {
    Vector3d v = {0.0f, 0.0f, 1.0f};
    NormVector(v, 1.0f);
    // FastInvSqrt has ~0.17% error, so use a relaxed tolerance
    EXPECT_NEAR(VectorLength(v), 1.0f, 0.01f);
}

TEST(NormVectorTest, TinyVectorClamped) {
    // For very small magnitudes (<1e-9), the function clamps n to 1e-9
    // which means normalization is inaccurate. This documents that behavior.
    Vector3d v = {0.0f, 0.0f, 1e-10f};
    NormVector(v, 1.0f);
    // Length will be close to sqrt(1e9) not 1.0, but it doesn't crash
    EXPECT_EQ(v.z > 0, true);
}

TEST(FindVectorAlphaTest, East) {
    EXPECT_FLOAT_EQ(FindVectorAlpha(1.0f, 0.0f), 0.0f);
}

TEST(FindVectorAlphaTest, North) {
    EXPECT_NEAR(FindVectorAlpha(0.0f, 1.0f), pi * 0.5f, 0.001f);
}

TEST(FindVectorAlphaTest, West) {
    EXPECT_NEAR(FindVectorAlpha(-1.0f, 0.0f), pi, 0.001f);
}

TEST(FindVectorAlphaTest, South) {
    EXPECT_NEAR(FindVectorAlpha(0.0f, -1.0f), pi * 1.5f, 0.001f);
}

TEST(FindVectorAlphaTest, Northeast) {
    float a = FindVectorAlpha(1.0f, 1.0f);
    EXPECT_NEAR(a, pi * 0.25f, 0.001f);
}

TEST(SGNTest, Positive) {
    EXPECT_FLOAT_EQ(SGN(42.0f), 1.0f);
}

TEST(SGNTest, Negative) {
    EXPECT_FLOAT_EQ(SGN(-3.14f), -1.0f);
}

TEST(SGNTest, Zero) {
    EXPECT_FLOAT_EQ(SGN(0.0f), 1.0f); // original: 0 returns 1
}

TEST(siRandTest, ZeroRange) {
    // siRand(0) should return 0
    EXPECT_EQ(siRand(0), 0);
}

TEST(siRandTest, RangeBounds) {
    // With R=RAND_MAX, returns 0
    EXPECT_EQ(siRand(RAND_MAX), 0);
}

TEST(SubVectorsTest, Basic) {
    Vector3d a = {5.0f, 3.0f, 1.0f};
    Vector3d b = {2.0f, 1.0f, 4.0f};
    Vector3d r = SubVectors(a, b);
    EXPECT_FLOAT_EQ(r.x, 3.0f);
    EXPECT_FLOAT_EQ(r.y, 2.0f);
    EXPECT_FLOAT_EQ(r.z, -3.0f);
}

TEST(AddVectorsTest, Basic) {
    Vector3d a = {1.0f, 2.0f, 3.0f};
    Vector3d b = {4.0f, 5.0f, 6.0f};
    Vector3d r = AddVectors(a, b);
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 7.0f);
    EXPECT_FLOAT_EQ(r.z, 9.0f);
}

TEST(MulVectorsScalTest, DotProduct) {
    Vector3d a = {1.0f, 0.0f, 0.0f};
    Vector3d b = {0.0f, 1.0f, 0.0f};
    float r;
    MulVectorsScal(a, b, r);
    EXPECT_FLOAT_EQ(r, 0.0f); // orthogonal
}

TEST(MulVectorsScalTest, SameDirection) {
    Vector3d a = {2.0f, 3.0f, 4.0f};
    Vector3d b = {2.0f, 3.0f, 4.0f};
    float r;
    MulVectorsScal(a, b, r);
    EXPECT_FLOAT_EQ(r, 29.0f); // 4+9+16
}

TEST(MulVectorsVectTest, CrossProduct) {
    Vector3d a = {1.0f, 0.0f, 0.0f};
    Vector3d b = {0.0f, 1.0f, 0.0f};
    Vector3d r;
    MulVectorsVect(a, b, r);
    EXPECT_FLOAT_EQ(r.x, 0.0f);
    EXPECT_FLOAT_EQ(r.y, 0.0f);
    EXPECT_FLOAT_EQ(r.z, 1.0f);
}
