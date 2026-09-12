// ==========================================================================
// test_menu_slider.cpp -- Regression tests for discrete menu sliders
// ==========================================================================

#include <gtest/gtest.h>

#include "ListMath.h"
#include "SliderMath.h"

namespace {
constexpr int kDetailMin = 24;
constexpr int kDetailMax = 96;
constexpr int kDetailStep = 4;
constexpr int kTrackWidth = 123;
}

TEST(MenuSliderTest, ObjectDetailClampsAtEndpoints)
{
    EXPECT_EQ(DiscreteSliderValue(-0.1f, kDetailMin, kDetailMax, kDetailStep), kDetailMin);
    EXPECT_EQ(DiscreteSliderValue(1.1f, kDetailMin, kDetailMax, kDetailStep), kDetailMax);
}

TEST(MenuSliderTest, ObjectDetailMaximumHasUsableHitArea)
{
    EXPECT_EQ(DiscreteSliderValue(119.0f / kTrackWidth, kDetailMin, kDetailMax, kDetailStep), 92);
    for (int x = 120; x <= kTrackWidth; ++x) {
        EXPECT_EQ(DiscreteSliderValue(static_cast<float>(x) / kTrackWidth,
                                      kDetailMin, kDetailMax, kDetailStep),
                  kDetailMax);
    }
}

TEST(MenuListTest, VisibleRowsUseScrolledDataIndices)
{
    EXPECT_EQ(HuntListDataIndex(0, 12), 12u);
    EXPECT_EQ(HuntListDataIndex(9, 12), 21u);
    EXPECT_EQ(HuntListVisibleEnd(12, 69), 22u);
    EXPECT_EQ(HuntListVisibleEnd(64, 69), 69u);
}

TEST(MenuListTest, ScrollingClampsWithoutUnsignedWraparound)
{
    EXPECT_EQ(ScrolledHuntListOffset(0, 69, 1), 0u);
    EXPECT_EQ(ScrolledHuntListOffset(0, 69, -1), 1u);
    EXPECT_EQ(ScrolledHuntListOffset(59, 69, -1), 59u);
    EXPECT_EQ(ScrolledHuntListOffset(59, 69, 1), 58u);
    EXPECT_EQ(ScrolledHuntListOffset(0, 10, -1), 0u);
}
