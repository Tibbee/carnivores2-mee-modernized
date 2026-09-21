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

inline const char* HunterAwarenessStateName(HunterAwarenessState state)
{
    switch (state)
    {
    case HunterAwarenessState::None: return "None";
    case HunterAwarenessState::InvestigatingShot: return "InvestigatingShot";
    case HunterAwarenessState::FleeingFromShot: return "FleeingFromShot";
    case HunterAwarenessState::RetaliatingHit: return "RetaliatingHit";
    case HunterAwarenessState::FleeingFromHit: return "FleeingFromHit";
    case HunterAwarenessState::FleeingFromCall: return "FleeingFromCall";
    case HunterAwarenessState::TrackingHunter: return "TrackingHunter";
    }
    return "?";
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

// The single owner of the untimed reaction timers (exact tracking and the
// morale timer). Timed fixed reactions keep their dedicated central tick so
// they can clear the awareness state on expiry; everything else ticks exactly
// once per frame here, so no tracking lock can live forever.
inline int TickUntimedReaction(int afraidTime, int elapsed)
{
    return afraidTime > elapsed ? afraidTime - elapsed : 0;
}

// A kill always requires the hunter to be physically inside the attack
// reach; nothing authorizes a kill at a distance. Exact tracking authorizes
// it during a chase. A fixed flee reaction also authorizes it at contact:
// a huge body crossing the hunter's position crushes them (trample) even
// though the creature has no intent to attack. Fixed pursuits are not listed
// because contact-range awareness promotes them to tracking first.
inline bool HunterAwarenessAllowsKill(HunterAwarenessState state)
{
    return state == HunterAwarenessState::TrackingHunter
        || IsFixedHunterFleeState(state);
}

inline bool IsWithinSquaredReach(float distanceSquared, float reach)
{
    return reach > 0.0f && distanceSquared < reach * reach;
}

inline bool IsWithinLinearReach(float distance, float reach)
{
    return reach > 0.0f && distance < reach;
}

inline bool IsWithinKillAltitude(float verticalDifference, float verticalReach)
{
    return verticalDifference < verticalReach;
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

// A fixed flee reaction stores a destination point, not a direction. When the
// creature reaches it the reaction must continue along the direction the
// creature is already running: extending along the bearing from the creature
// to the reached point re-anchors ahead of any creature that overshot or
// swung wide of it, which turns the escape into a local orbit around the
// point. The engine turns and moves a creature along its facing, so the unit
// look vector is its travel direction; extending along it keeps the run
// straight and leaves no steering error to turn on. Returns true when the leg
// was extended.
inline bool ExtendFleeDestination(float positionX, float positionZ,
                                  float lookX, float lookZ,
                                  float arrivalRadius, float legLength,
                                  float& destinationX, float& destinationZ)
{
    const float dx = destinationX - positionX;
    const float dz = destinationZ - positionZ;
    if (dx * dx + dz * dz >= arrivalRadius * arrivalRadius)
        return false;

    destinationX = positionX + lookX * legLength;
    destinationZ = positionZ + lookZ * legLength;
    return true;
}
