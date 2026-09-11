// ==========================================================================
// test_menu_slider.cpp -- Regression tests for discrete menu sliders
// ==========================================================================

#include <gtest/gtest.h>

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
