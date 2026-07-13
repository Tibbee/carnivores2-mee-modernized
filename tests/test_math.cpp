// ==========================================================================
// test_math.cpp — Unit tests for dependency-free math functions
// ==========================================================================
// These functions are self-contained (no game state), making them ideal
// first test candidates. Implementations are reproduced inline so the test
// binary does not depend on Hunt.h or other game headers.

#include <gtest/gtest.h>

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>

// --- Vector3d (simplified copy from Core/MathTypes.h) ---
struct Vector3d { float x, y, z; };

constexpr float pi = 3.14159265358979323846f;

// --- Helper to compare float vectors ---
::testing::AssertionResult VectorNear(const Vector3d& a, const Vector3d& b, float eps = 0.001f) {
    if (std::fabs(a.x - b.x) > eps || std::fabs(a.y - b.y) > eps || std::fabs(a.z - b.z) > eps)
        return ::testing::AssertionFailure()
            << "expected (" << a.x << ", " << a.y << ", " << a.z << ") "
            << "got (" << b.x << ", " << b.y << ", " << b.z << ")";
    return ::testing::AssertionSuccess();
}

// ===================== Implementation under test =====================

static float FastInvSqrt(float x) {
    float xhalf = 0.5f * x;
    union { float f; int i; } uf;
    uf.f = x;
    uf.i = 0x5f3759df - (uf.i >> 1);
    uf.f *= (1.5f - (xhalf * uf.f * uf.f));
    return uf.f;
}

void NormVector(Vector3d& v, float Scale) {
    double n = v.x * v.x + v.y * v.y + v.z * v.z;
    if (n < 0.000000001) n = 0.000000001;
    if (Scale == 1.0f) {
        float factor = FastInvSqrt(static_cast<float>(n));
        v.x *= factor; v.y *= factor; v.z *= factor;
    } else {
        float factor = static_cast<float>(static_cast<double>(Scale) / std::sqrt(n));
        v.x *= factor; v.y *= factor; v.z *= factor;
    }
}

float VectorLengthSq(Vector3d v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float VectorLength(Vector3d v) {
    return static_cast<float>(std::sqrt(VectorLengthSq(v)));
}

int siRand(int R) {
    if (R == RAND_MAX) return 0;
    return (rand() * (R * 2 + 1)) / RAND_MAX - R;
}

float SGN(float f) {
    return (f < 0) ? -1.0f : 1.0f;
}

float FindVectorAlpha(float vx, float vy) {
    float alpha = std::atan2(vy, vx);
    if (alpha < 0) alpha += 2.0f * pi;
    return alpha;
}

void MulVectorsScal(const Vector3d& v1, const Vector3d& v2, float& r) {
    r = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

void MulVectorsVect(const Vector3d& v1, const Vector3d& v2, Vector3d& r) {
    r.x = v1.y * v2.z - v2.y * v1.z;
    r.y = -v1.x * v2.z + v2.x * v1.z;
    r.z = v1.x * v2.y - v2.x * v1.y;
}

Vector3d SubVectors(Vector3d& v1, Vector3d& v2) {
    Vector3d res;
    res.x = v1.x - v2.x;
    res.y = v1.y - v2.y;
    res.z = v1.z - v2.z;
    return res;
}

Vector3d AddVectors(Vector3d& v1, Vector3d& v2) {
    Vector3d res;
    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    res.z = v1.z + v2.z;
    return res;
}

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
