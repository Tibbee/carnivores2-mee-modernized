// ==========================================================================
// test_projectile_math.cpp -- Regression tests for projectile range math
// ==========================================================================

#include <gtest/gtest.h>

#include <limits>

#include "ProjectileMath.h"

TEST(ProjectileRangeTest, DefaultViewRangeUsesWorldUnits)
{
    EXPECT_FLOAT_EQ(ProjectileViewRangeSquared(72), 18432.0f * 18432.0f);
}

TEST(ProjectileRangeTest, MaximumViewRangeDoesNotOverflow)
{
    const float rangeSquared = ProjectileViewRangeSquared(230);

    EXPECT_GT(rangeSquared, static_cast<float>((std::numeric_limits<int>::max)()));
    EXPECT_FLOAT_EQ(rangeSquared, 58880.0f * 58880.0f);
    EXPECT_GE(rangeSquared, 0.0f);
}
