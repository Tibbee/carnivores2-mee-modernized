// ==========================================================================
// test_billboard_math.cpp — Cylindrical billboard regression tests
// ==========================================================================

#include <gtest/gtest.h>

#include "BillboardMath.h"

TEST(BillboardMathTest, LevelCameraMatchesScreenAlignedOffsets)
{
    const auto offset = CalculateCylindricalBillboardViewOffset(
        12.0f, 80.0f, 1.0f, 0.0f);

    EXPECT_FLOAT_EQ(offset.x, 12.0f);
    EXPECT_FLOAT_EQ(offset.y, 80.0f);
    EXPECT_FLOAT_EQ(offset.z, 0.0f);
}

TEST(BillboardMathTest, CameraPitchRotatesWorldVerticalIntoViewDepth)
{
    const auto offset = CalculateCylindricalBillboardViewOffset(
        3.0f, 10.0f, 0.6f, 0.8f);

    EXPECT_FLOAT_EQ(offset.x, 3.0f);
    EXPECT_FLOAT_EQ(offset.y, 6.0f);
    EXPECT_FLOAT_EQ(offset.z, 8.0f);
}

TEST(BillboardMathTest, HorizontalAxisDoesNotInheritCameraPitch)
{
    const auto offset = CalculateCylindricalBillboardViewOffset(
        -25.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(offset.x, -25.0f);
    EXPECT_FLOAT_EQ(offset.y, 0.0f);
    EXPECT_FLOAT_EQ(offset.z, 0.0f);
}
