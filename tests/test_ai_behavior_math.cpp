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
    EXPECT_FALSE(OutsideNormalAggressionRange(99.0f, 100.0f, false));
    EXPECT_TRUE(OutsideNormalAggressionRange(101.0f, 100.0f, false));
    EXPECT_TRUE(OutsideNormalAggressionRangeSquared(101.0f * 101.0f,
                                                    100.0f, false));
}

TEST(AIBehaviorMathTest, RecentDamageBypassesNormalAggressionRange)
{
    // A creature that was just shot keeps engaging beyond its authored
    // acquisition range; the pre-622e50b reaction model relied on this.
    EXPECT_FALSE(OutsideNormalAggressionRange(1000.0f, 100.0f, true));
    EXPECT_FALSE(OutsideNormalAggressionRangeSquared(1000.0f * 1000.0f,
                                                     100.0f, true));
    EXPECT_TRUE(OutsideNormalAggressionRange(1000.0f, 100.0f, false));
    EXPECT_TRUE(OutsideNormalAggressionRangeSquared(1000.0f * 1000.0f,
                                                    100.0f, false));
}

TEST(AIBehaviorMathTest, HunterEventsUseTheScaledAggressionRange)
{
    // Event ranges are the authored aggression range scaled by
    // kHunterEventRangeScale. Carnotaurus: 72 * 200 * 2.5.
    constexpr float carnoEventRange =
        72.0f * 200.0f * kHunterEventRangeScale;
    // Pachycephalosaurus: 72 * 60 * 2.5 -- a low-aggression herbivore, even
    // though it reuses the Allosaurus AI clone.
    constexpr float pachyEventRange =
        72.0f * 60.0f * kHunterEventRangeScale;

    // A predator's scaled range covers any shot it can hear.
    EXPECT_FALSE(ShouldFleeFromHunterEvent(200, false, 25000.0f,
                                           carnoEventRange));
    // The same distant event makes a low-aggression herbivore flee...
    EXPECT_TRUE(ShouldFleeFromHunterEvent(60, false, 25000.0f,
                                          pachyEventRange));
    // ...while an event inside its scaled range still provokes a reaction.
    EXPECT_FALSE(ShouldFleeFromHunterEvent(60, false, 5000.0f,
                                           pachyEventRange));
}

TEST(AIBehaviorMathTest, AuthoredFearAndPassivityAlwaysFleeHunterEvents)
{
    constexpr float eventRange = 40000.0f;
    EXPECT_TRUE(ShouldFleeFromHunterEvent(200, true, 10.0f, eventRange));
    EXPECT_TRUE(ShouldFleeFromHunterEvent(0, false, 10.0f, eventRange));
    EXPECT_TRUE(ShouldFleeFromHunterEvent(-1, false, 10.0f, eventRange));
    EXPECT_FALSE(ShouldFleeFromHunterEvent(200, false, 10.0f, eventRange));
}

TEST(AIBehaviorMathTest, DedicatedPredatorWithoutFleeStateRespondsAggressively)
{
    constexpr bool alwaysRespondAggressively = true;
    // The T-Rex has no authored aggress value and no flee path.
    EXPECT_FALSE(ShouldFleeFromHunterEvent(0, true, 25000.0f, 0.0f,
                                           alwaysRespondAggressively));
}

TEST(AIBehaviorMathTest, DistantReactionsGetEnoughTravelTime)
{
    // A slow creature at the edge of hearing gets the travel time plus the
    // minimum search window instead of the proximity-only minimum.
    const int base = ShotInvestigationTime(1000.0f, 1000.0f, false);
    EXPECT_EQ(base, kShotInvestigationMinTime);
    const int extended = ShotInvestigationTimeForTravel(base, 5000.0f, 1.0f);
    EXPECT_EQ(extended, 5000 + kShotInvestigationMinTime);
    // A fast creature only needs the travel floor when it exceeds the base.
    EXPECT_EQ(ShotInvestigationTimeForTravel(base, 5000.0f, 100.0f),
              50 + kShotInvestigationMinTime);
    // A long proximity-based reaction is never shortened.
    const int longBase = ShotInvestigationTime(0.0f, 1000.0f, false);
    EXPECT_EQ(ShotInvestigationTimeForTravel(longBase, 5000.0f, 100.0f),
              longBase);
    // The floor is capped so an event reaction stays finite.
    EXPECT_EQ(ShotInvestigationTimeForTravel(base, 1.0e7f, 1.0f),
              kShotInvestigationTravelCap);
}

TEST(AIBehaviorMathTest, ContactRangePromotesFixedPursuitToTracking)
{
    constexpr float reach = 400.0f;
    constexpr float reachSquared = reach * reach;

    // A hunter inside the reach of an investigating or retaliating creature
    // is treated as detected, so species without sight/scent stay dangerous.
    EXPECT_TRUE(ShouldPromotePursuitToTracking(
        true, true, 399.0f * 399.0f, reachSquared));
    // Exactly at the reach still counts as contact.
    EXPECT_TRUE(ShouldPromotePursuitToTracking(
        true, true, reachSquared, reachSquared));
    // Outside the reach, a remembered event point still never kills.
    EXPECT_FALSE(ShouldPromotePursuitToTracking(
        true, true, 401.0f * 401.0f, reachSquared));
    // Vertical separation (a hunter below a cliff) blocks contact awareness.
    EXPECT_FALSE(ShouldPromotePursuitToTracking(
        true, false, 10.0f, reachSquared));
    // Flee reactions keep their stored direction and stay non-lethal.
    EXPECT_FALSE(ShouldPromotePursuitToTracking(
        false, true, 10.0f, reachSquared));
    // Species without an authored attack reach never promote this way.
    EXPECT_FALSE(ShouldPromotePursuitToTracking(true, true, 10.0f, 0.0f));
}
