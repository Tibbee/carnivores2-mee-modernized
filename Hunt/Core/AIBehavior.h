// ==========================================================================
// AIBehavior.h -- Dependency-free hunter awareness state helpers
// ==========================================================================
#pragma once

#include <cstdint>

#include "Constants.h"

enum class HunterAwarenessState : std::uint8_t
{
    None,
    InvestigatingShot,
    FleeingFromShot,
    RetaliatingHit,
    FleeingFromHit,
    FleeingFromCall,
    TrackingHunter
};

inline HunterAwarenessState HeardShotReactionState(bool flees)
{
    return flees
        ? HunterAwarenessState::FleeingFromShot
        : HunterAwarenessState::InvestigatingShot;
}

inline HunterAwarenessState DirectHitReactionState(bool flees)
{
    return flees
        ? HunterAwarenessState::FleeingFromHit
        : HunterAwarenessState::RetaliatingHit;
}

inline HunterAwarenessState UpdatedDirectHitAwarenessState(
    HunterAwarenessState current, bool flees)
{
    return current == HunterAwarenessState::TrackingHunter && !flees
        ? current
        : DirectHitReactionState(flees);
}

inline bool ShouldInitializeDirectHitAlert(bool survived, bool wasAware)
{
    return survived && !wasAware;
}

// A hunter event (heard shot or direct hit) is a stronger stimulus than
// passive detection: the species' authored aggression range is scaled up for
// the reaction (see GetCharacterHunterEventRange). Authored fear and
// passivity always flee. A species whose authored range does not cover the
// event -- a low-aggression herbivore, for example -- also flees instead of
// charging the source. `alwaysRespondAggressively` is the T-Rex exception
// (its `aggress` value is intentionally omitted).
inline bool ShouldFleeFromHunterEvent(int aggression, bool fearsEvent,
                                      float eventDistance, float eventRange,
                                      bool alwaysRespondAggressively = false)
{
    return !alwaysRespondAggressively
        && (fearsEvent || aggression <= 0 || eventDistance > eventRange);
}

inline bool IsFixedHunterPursuitState(HunterAwarenessState state)
{
    return state == HunterAwarenessState::InvestigatingShot
        || state == HunterAwarenessState::RetaliatingHit;
}

inline bool IsFixedHunterFleeState(HunterAwarenessState state)
{
    return state == HunterAwarenessState::FleeingFromShot
        || state == HunterAwarenessState::FleeingFromHit
        || state == HunterAwarenessState::FleeingFromCall;
}

inline bool IsTimedHunterReactionState(HunterAwarenessState state)
{
    return IsFixedHunterPursuitState(state) || IsFixedHunterFleeState(state);
}

inline bool ShouldSkipTRexPerception(bool hasReactionTime,
                                     bool isStateOne,
                                     HunterAwarenessState awareness)
{
    return (hasReactionTime || isStateOne)
        && !IsTimedHunterReactionState(awareness);
}

// A direct hit must cancel any pending look/roar notice and start the charge.
// Repeated hits during an active retaliation or exact tracking keep the
// current pursuit instead of restarting it.
inline bool ShouldRestartTRexHitPursuit(HunterAwarenessState priorAwareness)
{
    return priorAwareness != HunterAwarenessState::RetaliatingHit
        && priorAwareness != HunterAwarenessState::TrackingHunter;
}

// The look/smell notice animation must not interrupt a timed shot or hit
// reaction; awareness still upgrades to exact tracking independently.
inline bool ShouldScheduleNoticeAnimation(HunterAwarenessState awareness)
{
    return !IsTimedHunterReactionState(awareness);
}

// The authored look offset moves the hunter distance to the creature's
// reaction point (snout or head) for a few families. Kept as a pure table so
// the shared hunter-geometry function and the animators cannot drift. Pass
// AIInfo[clone].carnivore for the fallback rule.
inline float HunterLookOffset(int clone, float scale, bool carnivore)
{
    switch (clone)
    {
    case AI_ALLO:
    case AI_MOSA:
    case AI_FISH:
        return 100.0f * scale;
    case AI_CHASM:
    case AI_HOG:
    case AI_BRONT:
    case AI_BEAR:
    case AI_WOLF:
    case AI_RHINO:
    case AI_SMILO:
        return 300.0f * scale;
    case AI_BRACHDANGER:
    case AI_LANDBRACH:
    case AI_TREX:
        return 108.0f;
    default:
        return carnivore ? 108.0f : 0.0f;
    }
}

// The authored flee/pursue rule shared by the standard predator family
// (AnimateHuntable, AnimateBrahi). The engine wrapper derives the inputs from
// the species flags and the current reaction, so the rule itself stays a pure,
// testable predicate instead of a per-animator if-chain. Any authored fear
// response -- a defensive species at full health, a shot-fearing species that
// is already hurt, or a creature told to flee the shot -- overrides aggression.
inline bool ShouldFleeFromAuthoredThreat(bool outsideAggressionRange,
                                         bool passive,
                                         bool aware,
                                         bool defensiveAtFullHealth,
                                         bool injuredAndFearsShot,
                                         bool fleeingFromShot)
{
    return outsideAggressionRange || passive || !aware
        || defensiveAtFullHealth || injuredAndFearsShot || fleeingFromShot;
}

// A creature following a remembered event position (shot investigation or hit
// retaliation) still notices a hunter who physically enters its attack reach.
// Contact-range presence is stronger evidence than the stored event point, so
// the fixed pursuit upgrades to exact tracking. The stored point alone never
// authorizes a kill at a distance, and flee reactions are not affected. This
// grants no active sight or smell: a species authored not to look or smell for
// the hunter still ignores it while idle.
inline bool ShouldPromotePursuitToTracking(bool isFixedPursuit,
                                           bool verticalInRange,
                                           float hunterDistanceSquared,
                                           float attackReachSquared)
{
    return isFixedPursuit && verticalInRange && attackReachSquared > 0.0f
        && hunterDistanceSquared <= attackReachSquared;
}
