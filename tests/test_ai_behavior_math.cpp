// ==========================================================================
// test_ai_behavior_math.cpp -- Regression tests for rendering-independent AI range
// ==========================================================================

#include <gtest/gtest.h>

#include "AIBehavior.h"
#include "Constants.h"

TEST(AIBehaviorMathTest, LegacyViewDistancesRemainUnchanged)
{
    EXPECT_EQ(GameplayViewRadiusCells(42), 42);
    EXPECT_EQ(GameplayViewRadiusCells(64), 64);
    EXPECT_EQ(GameplayViewRadiusCells(72), 72);
}

TEST(AIBehaviorMathTest, ExtendedRenderingDoesNotExtendGameplayRange)
{
    EXPECT_EQ(GameplayViewRadiusCells(120), 72);
    EXPECT_EQ(GameplayViewRadiusCells(196), 72);
    EXPECT_EQ(GameplayViewRadiusCells(230), 72);
}

TEST(AIBehaviorMathTest, GunshotNoiseUsesTheLegacyGameplayCeiling)
{
    constexpr float shotgunLoudness = 1.7f;
    constexpr float legacyShotgunRange = 72.0f * 200.0f * shotgunLoudness;

    EXPECT_FLOAT_EQ(GunshotNoiseRangeWorld(72, shotgunLoudness), legacyShotgunRange);
    EXPECT_FLOAT_EQ(GunshotNoiseRangeWorld(196, shotgunLoudness), legacyShotgunRange);
    EXPECT_FLOAT_EQ(GunshotNoiseRangeWorld(230, shotgunLoudness), legacyShotgunRange);
}

TEST(AIBehaviorMathTest, ShotInvestigationTimeIsFinite)
{
    EXPECT_EQ(ShotInvestigationTime(1000.0f, 1000.0f, false),
              kShotInvestigationMinTime);
    EXPECT_EQ(ShotInvestigationTime(0.0f, 1000.0f, false),
              kShotInvestigationMaxTime);
    EXPECT_EQ(ShotInvestigationTime(0.0f, 1000.0f, true),
              kTRexShotInvestigationMaxTime);
}

TEST(AIBehaviorMathTest, ShotInvestigationEndsAtTargetOrTimeout)
{
    EXPECT_TRUE(ShotInvestigationComplete(0, 10000.0f * 10000.0f));
    EXPECT_TRUE(ShotInvestigationComplete(1000,
                                         kShotInvestigationArrivalRadius
                                             * kShotInvestigationArrivalRadius));
    EXPECT_FALSE(ShotInvestigationComplete(1000, 10000.0f * 10000.0f));
}

TEST(AIBehaviorMathTest, AwarenessEventsSelectSpeciesReaction)
{
    EXPECT_EQ(HeardShotReactionState(false),
              HunterAwarenessState::InvestigatingShot);
    EXPECT_EQ(HeardShotReactionState(true),
              HunterAwarenessState::FleeingFromShot);
    EXPECT_EQ(DirectHitReactionState(false),
              HunterAwarenessState::RetaliatingHit);
    EXPECT_EQ(DirectHitReactionState(true),
              HunterAwarenessState::FleeingFromHit);
}

TEST(AIBehaviorMathTest, AwarenessStatesDistinguishFixedReactions)
{
    EXPECT_TRUE(IsFixedHunterPursuitState(
        HunterAwarenessState::InvestigatingShot));
    EXPECT_TRUE(IsFixedHunterPursuitState(
        HunterAwarenessState::RetaliatingHit));
    EXPECT_TRUE(IsFixedHunterFleeState(
        HunterAwarenessState::FleeingFromShot));
    EXPECT_TRUE(IsFixedHunterFleeState(
        HunterAwarenessState::FleeingFromHit));
    EXPECT_FALSE(IsTimedHunterReactionState(
        HunterAwarenessState::TrackingHunter));
    EXPECT_FALSE(IsTimedHunterReactionState(HunterAwarenessState::None));
}

TEST(AIBehaviorMathTest, NormalAwarenessRespectsAggressionRange)
{
    EXPECT_FALSE(OutsideNormalAggressionRange(99.0f, 100.0f, false));
    EXPECT_TRUE(OutsideNormalAggressionRange(101.0f, 100.0f, false));
    EXPECT_TRUE(OutsideNormalAggressionRangeSquared(101.0f * 101.0f,
                                                    100.0f, false));
}

TEST(AIBehaviorMathTest, RecentDamageBypassesNormalAggressionRange)
{
    EXPECT_FALSE(OutsideNormalAggressionRange(1000.0f, 100.0f, true));
    EXPECT_FALSE(OutsideNormalAggressionRangeSquared(1000.0f * 1000.0f,
                                                     100.0f, true));
}
