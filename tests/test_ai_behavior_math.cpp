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

TEST(AIBehaviorMathTest, DirectHitDoesNotDowngradeExactTracking)
{
    EXPECT_EQ(UpdatedDirectHitAwarenessState(
                  HunterAwarenessState::TrackingHunter, false),
              HunterAwarenessState::TrackingHunter);
    EXPECT_EQ(UpdatedDirectHitAwarenessState(
                  HunterAwarenessState::TrackingHunter, true),
              HunterAwarenessState::FleeingFromHit);
    EXPECT_EQ(UpdatedDirectHitAwarenessState(
                  HunterAwarenessState::InvestigatingShot, false),
              HunterAwarenessState::RetaliatingHit);
}

TEST(AIBehaviorMathTest, TRexHitRestartsPursuitOnlyWhenNotAlreadyEngaged)
{
    EXPECT_TRUE(ShouldRestartTRexHitPursuit(HunterAwarenessState::None));
    EXPECT_TRUE(ShouldRestartTRexHitPursuit(
        HunterAwarenessState::InvestigatingShot));
    EXPECT_FALSE(ShouldRestartTRexHitPursuit(
        HunterAwarenessState::RetaliatingHit));
    EXPECT_FALSE(ShouldRestartTRexHitPursuit(
        HunterAwarenessState::TrackingHunter));
}

TEST(AIBehaviorMathTest, NoticeAnimationIsSuppressedDuringShotReactions)
{
    EXPECT_FALSE(ShouldScheduleNoticeAnimation(
        HunterAwarenessState::InvestigatingShot));
    EXPECT_FALSE(ShouldScheduleNoticeAnimation(
        HunterAwarenessState::RetaliatingHit));
    EXPECT_TRUE(ShouldScheduleNoticeAnimation(HunterAwarenessState::None));
    EXPECT_TRUE(ShouldScheduleNoticeAnimation(
        HunterAwarenessState::TrackingHunter));
}

TEST(AIBehaviorMathTest, DirectHitAlertOnlyStartsOnFirstAwareness)
{
    EXPECT_TRUE(ShouldInitializeDirectHitAlert(true, false));
    EXPECT_FALSE(ShouldInitializeDirectHitAlert(true, true));
    EXPECT_FALSE(ShouldInitializeDirectHitAlert(false, false));
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
    EXPECT_TRUE(IsFixedHunterFleeState(
        HunterAwarenessState::FleeingFromCall));
    EXPECT_FALSE(IsTimedHunterReactionState(
        HunterAwarenessState::TrackingHunter));
    EXPECT_FALSE(IsTimedHunterReactionState(HunterAwarenessState::None));
}

TEST(AIBehaviorMathTest, TRexCanUpgradeFixedReactionToExactTracking)
{
    EXPECT_FALSE(ShouldSkipTRexPerception(
        true, true, HunterAwarenessState::InvestigatingShot));
    EXPECT_FALSE(ShouldSkipTRexPerception(
        true, true, HunterAwarenessState::RetaliatingHit));
    EXPECT_TRUE(ShouldSkipTRexPerception(
        true, true, HunterAwarenessState::TrackingHunter));
    EXPECT_FALSE(ShouldSkipTRexPerception(
        false, false, HunterAwarenessState::TrackingHunter));
}

TEST(AIBehaviorMathTest, NormalAwarenessRespectsAggressionRange)
{
    EXPECT_FALSE(OutsideNormalAggressionRange(99.0f, 100.0f));
    EXPECT_TRUE(OutsideNormalAggressionRange(101.0f, 100.0f));
    EXPECT_TRUE(OutsideNormalAggressionRangeSquared(101.0f * 101.0f,
                                                    100.0f));
}

TEST(AIBehaviorMathTest, AwarenessEventsRespectAuthoredAggressionRange)
{
    EXPECT_FALSE(ShouldFleeFromAwarenessEvent(99.0f, 100.0f, 1, false));
    EXPECT_FALSE(ShouldFleeFromAwarenessEvent(100.0f, 100.0f, 1, false));
    EXPECT_TRUE(ShouldFleeFromAwarenessEvent(101.0f, 100.0f, 1, false));
}

TEST(AIBehaviorMathTest, LowAndHighAggressionProduceDifferentEventReactions)
{
    constexpr float eventDistance = 1000.0f;
    EXPECT_TRUE(ShouldFleeFromAwarenessEvent(
        eventDistance, 72.0f * 1.0f, 1, false));
    EXPECT_FALSE(ShouldFleeFromAwarenessEvent(
        eventDistance, 72.0f * 200.0f, 200, false));
}

TEST(AIBehaviorMathTest, PassiveAndFearfulSpeciesFleeAwarenessEvents)
{
    EXPECT_TRUE(ShouldFleeFromAwarenessEvent(10.0f, 100.0f, 0, false));
    EXPECT_TRUE(ShouldFleeFromAwarenessEvent(10.0f, 100.0f, -1, false));
    EXPECT_TRUE(ShouldFleeFromAwarenessEvent(10.0f, 100.0f, 100, true));
}

TEST(AIBehaviorMathTest, DedicatedPredatorWithoutFleeStateRespondsAggressively)
{
    constexpr bool alwaysRespondAggressively = true;
    EXPECT_FALSE(ShouldFleeFromAwarenessEvent(
        1000.0f, 0.0f, 0, true, alwaysRespondAggressively));
}

TEST(AIBehaviorMathTest, RecentDamageDoesNotBypassAggressionRange)
{
    EXPECT_TRUE(OutsideNormalAggressionRange(1000.0f, 100.0f));
    EXPECT_TRUE(OutsideNormalAggressionRangeSquared(1000.0f * 1000.0f,
                                                    100.0f));
}
