// ==========================================================================
// test_idle_group_select.cpp -- Shared idle-group phase selection
// ==========================================================================
// Pins SelectIdleGroupPhase(), the merged selector behind the per-dino
// animators (AnimateBrahi, AnimateBrahiOld, AnimateClassicAmbient,
// AnimateHuntable, AnimateTRex and AnimateIcth's idle/idle2 sets).

#include <gtest/gtest.h>

#include "Hunt.h"
#include "Core/SpawnRegion.h"
#include "Game/CharacterInternal.h"
#include <cstdlib>

namespace {

constexpr int kFallbackPhase = 777;

// Two 2-animation groups with distinct anim ids so the chosen entry stays
// observable; the chance fields are set per scenario.
void InitGroups(TDinoIdleType (&groups)[2])
{
    for (TDinoIdleType& group : groups) {
        group = TDinoIdleType{};
        group.count = 2;
        group.anim[0] = 10;
        group.anim[1] = 11;
    }
    groups[1].anim[0] = 20;
    groups[1].anim[1] = 21;
}

} // namespace

TEST(SpawnRegion, LegacyPositiveZeroAndReversedSpansKeepResultsAndRngOrder)
{
    // -115 is the shipped area7 trap's ymax=487 minus ymin=602.
    for (int maximum : {976, 1, 0, -2, -115}) {
        EXPECT_EQ(SpawnRegionRandomMaximum(602, 602 + maximum), maximum);
        for (unsigned seed : {0u, 1u, 42u, 731u, 9999u}) {
            std::srand(seed);
            const int legacy = rRand(maximum);
            const int legacyNext = std::rand();
            std::srand(seed);
            const int candidate = rRand(SpawnRegionRandomMaximum(602, 602 + maximum));
            EXPECT_EQ(candidate, legacy);
            EXPECT_EQ(std::rand(), legacyNext);
        }
    }
}

TEST(SpawnRegion, UndefinedSpanArithmeticRecoversWithoutCallingInvalidRand)
{
    EXPECT_EQ(SpawnRegionRandomMaximum(602, 601), 0); // rRand(-1) divides by zero.
    const int low = (std::numeric_limits<int>::min)();
    const int high = (std::numeric_limits<int>::max)();
    EXPECT_EQ(SpawnRegionRandomMaximum(0, high), 0); // r + 1 overflows.
    EXPECT_EQ(SpawnRegionRandomMaximum(low, high), 0); // difference overflows int.
    EXPECT_EQ(SpawnRegionRandomMaximum(high, low), 0);
}

TEST(IdleGroupSelectTest, ZeroStartChanceLeavesFallback)
{
    TDinoIdleType groups[2];
    InitGroups(groups);
    groups[0].start = 0.0f;
    groups[1].start = 0.0f;

    int phase = 4242;
    int current = -1;
    EXPECT_FALSE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(phase, kFallbackPhase);
    EXPECT_EQ(current, -1);
}

TEST(IdleGroupSelectTest, CertainStartPicksAGroupAndItsFirstAnimation)
{
    TDinoIdleType groups[2];
    InitGroups(groups);
    groups[0].start = 1.0f;
    groups[1].start = 1.0f; // both qualify; the roll loop keeps the last one

    int phase = 4242;
    int current = -1;
    EXPECT_TRUE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(current, 1);
    EXPECT_EQ(phase, 20); // startOnAny false -> first animation

    // startOnAny picks a random animation inside the chosen group.
    groups[1].startOnAny = true;
    current = -1;
    EXPECT_TRUE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(current, 1);
    EXPECT_TRUE(phase == 20 || phase == 21);
}

TEST(IdleGroupSelectTest, CurrentGroupContinuesUntilItsEndChance)
{
    TDinoIdleType groups[2];
    InitGroups(groups);
    // end=0: the end chance can never fire.
    const int currentGroup = 0;
    int phase = 4242;
    int current = currentGroup;
    EXPECT_TRUE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(current, currentGroup);
    EXPECT_TRUE(phase == 10 || phase == 11);

    // endOnAny=false and a phase that is not the last animation: even a
    // certain end chance cannot end the run early.
    groups[0].end = 1.0f;
    groups[0].endOnAny = false;
    phase = 10; // not the group's last animation (11)
    current = currentGroup;
    EXPECT_TRUE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(current, currentGroup);
    EXPECT_TRUE(phase == 10 || phase == 11);
}

TEST(IdleGroupSelectTest, GroupEndReleasesOrRestarts)
{
    TDinoIdleType groups[2];
    InitGroups(groups);
    groups[0].end = 1.0f;
    groups[0].endOnAny = true;
    groups[0].instantRepeat = false;

    int phase = 10;
    int current = 0;
    // An ended group releases the phase to the fallback and clears its id.
    EXPECT_TRUE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(phase, kFallbackPhase);
    EXPECT_EQ(current, -1);

    // instantRepeat restarts the selection; with no start chance left the
    // roll falls back without a group.
    groups[0].instantRepeat = true;
    groups[0].start = 0.0f;
    groups[1].start = 0.0f;
    phase = 10;
    current = 0;
    EXPECT_FALSE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(phase, kFallbackPhase);
    EXPECT_EQ(current, -1);

    // instantRepeat with a live start chance picks a new group in the same call.
    groups[1].start = 1.0f;
    phase = 10;
    current = 0;
    EXPECT_TRUE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(current, 1);
    EXPECT_EQ(phase, 20);
}

TEST(IdleGroupSelectTest, EmptyOrOversizedGroupsFallBack)
{
    TDinoIdleType groups[2];
    InitGroups(groups);
    groups[0].count = 0;
    groups[1].count = 0;
    groups[0].start = 1.0f;
    groups[1].start = 1.0f;

    int phase = 4242;
    int current = -1;
    EXPECT_FALSE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(phase, kFallbackPhase);
    EXPECT_EQ(current, -1);

    // A current group that becomes empty is released, not indexed.
    groups[0].start = 0.0f;
    groups[1].start = 0.0f;
    phase = 10;
    current = 0;
    EXPECT_FALSE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(phase, kFallbackPhase);
    EXPECT_EQ(current, -1);

    // Counts beyond the fixed animation array are rejected as well.
    groups[0].count = 40;
    groups[0].start = 1.0f;
    groups[0].anim[0] = 10;
    phase = 4242;
    current = -1;
    EXPECT_FALSE(SelectIdleGroupPhase(phase, groups, 2, current, kFallbackPhase));
    EXPECT_EQ(phase, kFallbackPhase);
    EXPECT_EQ(current, -1);
}
