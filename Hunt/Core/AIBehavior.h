// ==========================================================================
// AIBehavior.h -- Dependency-free hunter awareness state helpers
// ==========================================================================
#pragma once

#include <cstdint>

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
