// ==========================================================================
// test_water_object_visibility.cpp -- Water/object visibility regression tests
// ==========================================================================

#include <gtest/gtest.h>

#include "WaterObjectVisibility.h"

TEST(WaterObjectVisibilityTest, PartiallySubmergedObjectIntersectsSurface)
{
    EXPECT_TRUE(ObjectIntersectsWaterSurface(100.0f, 300.0f, 200.0f));
    EXPECT_TRUE(ObjectIntersectsWaterSurface(100.0f, 200.0f, 200.0f));
}

TEST(WaterObjectVisibilityTest, FullySubmergedObjectDoesNotNeedSurfaceClipping)
{
    EXPECT_FALSE(ObjectIntersectsWaterSurface(100.0f, 199.0f, 200.0f));
}

TEST(WaterObjectVisibilityTest, ObjectAboveWaterDoesNotIntersectSurface)
{
    EXPECT_FALSE(ObjectIntersectsWaterSurface(200.0f, 300.0f, 200.0f));
}
