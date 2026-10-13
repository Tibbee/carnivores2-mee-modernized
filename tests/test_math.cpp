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

TEST(MathTest, VectorLengthAndSquared) {
    EXPECT_FLOAT_EQ(VectorLength({1.0f, 0.0f, 0.0f}), 1.0f);
    EXPECT_FLOAT_EQ(VectorLength({0.0f, 0.0f, 0.0f}), 0.0f);
    EXPECT_FLOAT_EQ(VectorLength({3.0f, 4.0f, 0.0f}), 5.0f);
    EXPECT_FLOAT_EQ(VectorLength({2.0f, 3.0f, 6.0f}), 7.0f); // sqrt(4+9+36)
    EXPECT_FLOAT_EQ(VectorLength({0.0f, 0.0f, 1.0e-6f}), 1.0e-6f);
    EXPECT_FLOAT_EQ(VectorLengthSq({3.0f, 4.0f, 0.0f}), 25.0f);

    // Projectile safety/recovery (128/300), trophy interaction (148), and
    // ammo pickup (200) all use strict comparisons against VectorLength.
    for (float threshold : {128.0f, 148.0f, 200.0f, 300.0f}) {
        EXPECT_LT(VectorLength({threshold * 0.999f, 0.0f, 0.0f}), threshold);
        EXPECT_GT(VectorLength({threshold * 1.001f, 0.0f, 0.0f}), threshold);
        EXPECT_FLOAT_EQ(VectorLength({threshold, 0.0f, 0.0f}), threshold);
    }
}

TEST(MathTest, NormVector) {
    Vector3d v = {100.0f, -200.0f, 50.0f};
    NormVector(v, 1.0f);
    EXPECT_NEAR(VectorLength(v), 1.0f, 0.001f);

    v = {3.0f, 0.0f, 0.0f};
    NormVector(v, 5.0f);
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);

    // FastInvSqrt has ~0.17% error, so use a relaxed tolerance.
    v = {0.0f, 0.0f, 1.0f};
    NormVector(v, 1.0f);
    EXPECT_NEAR(VectorLength(v), 1.0f, 0.01f);

    // For very small magnitudes (<1e-9) the function clamps n to 1e-9, so
    // normalization is inaccurate; the result must stay finite and positive.
    v = {0.0f, 0.0f, 1e-10f};
    NormVector(v, 1.0f);
    EXPECT_GT(v.z, 0.0f);
}

TEST(MathTest, FindVectorAlpha) {
    EXPECT_FLOAT_EQ(FindVectorAlpha(1.0f, 0.0f), 0.0f);
    EXPECT_NEAR(FindVectorAlpha(0.0f, 1.0f), pi * 0.5f, 0.001f);
    EXPECT_NEAR(FindVectorAlpha(-1.0f, 0.0f), pi, 0.001f);
    EXPECT_NEAR(FindVectorAlpha(0.0f, -1.0f), pi * 1.5f, 0.001f);
    EXPECT_NEAR(FindVectorAlpha(1.0f, 1.0f), pi * 0.25f, 0.001f);
}

TEST(MathTest, ScalarHelpers) {
    EXPECT_FLOAT_EQ(SGN(42.0f), 1.0f);
    EXPECT_FLOAT_EQ(SGN(-3.14f), -1.0f);
    EXPECT_FLOAT_EQ(SGN(0.0f), 1.0f); // original: 0 returns 1
    EXPECT_EQ(siRand(0), 0);
    EXPECT_EQ(siRand(RAND_MAX), 0);
}

TEST(MathTest, VectorArithmetic) {
    Vector3d a = {5.0f, 3.0f, 1.0f};
    Vector3d b = {2.0f, 1.0f, 4.0f};
    Vector3d r = SubVectors(a, b);
    EXPECT_FLOAT_EQ(r.x, 3.0f);
    EXPECT_FLOAT_EQ(r.y, 2.0f);
    EXPECT_FLOAT_EQ(r.z, -3.0f);

    a = {1.0f, 2.0f, 3.0f};
    b = {4.0f, 5.0f, 6.0f};
    r = AddVectors(a, b);
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 7.0f);
    EXPECT_FLOAT_EQ(r.z, 9.0f);

    float dot = 0.0f;
    MulVectorsScal({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, dot);
    EXPECT_FLOAT_EQ(dot, 0.0f); // orthogonal
    MulVectorsScal({2.0f, 3.0f, 4.0f}, {2.0f, 3.0f, 4.0f}, dot);
    EXPECT_FLOAT_EQ(dot, 29.0f); // 4+9+16

    Vector3d cross;
    MulVectorsVect({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, cross);
    EXPECT_FLOAT_EQ(cross.x, 0.0f);
    EXPECT_FLOAT_EQ(cross.y, 0.0f);
    EXPECT_FLOAT_EQ(cross.z, 1.0f);
}
