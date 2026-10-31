// ==========================================================================
// CharacterInternal.h -- Internal declarations shared across Character module
// ==========================================================================
// Functions declared here are used by multiple Character/*.cpp files but are
// not exposed to the rest of the engine (not in EngineAPI.h).

#pragma once

#include <cmath>

#include "Core/GameTypes.h"

// Collision / placement checks
int CheckPlaceCollisionP(Vector3d &v, bool aquatic);
int CheckPlaceCollisionFishP(Vector3d &v, int minDepth, int maxDepth);
int CheckPlaceCollisionFish(TCharacter *cptr, Vector3d &v, float mosaDepth, int maxDepth, int minDepth);
int CheckPlaceCollisionMosasaurus(TCharacter *cptr, Vector3d &v, float mosaDepth);
int CheckPlaceCollision(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionMicro(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionLandBrahi(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionBrahi(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionBrahiP(Vector3d &v);
int CheckPlaceCollision2(TCharacter *cptr, Vector3d &v, BOOL wc);
int CheckPossiblePath(TCharacter *cptr, BOOL wc, BOOL mc);

// Movement
void MoveCharacter(TCharacter *cptr, float dx, float dz, BOOL wc, BOOL mc);
void MoveCharacterFish(TCharacter *cptr, float dx, float dz);
void Characters_AddSecondaryOne(TCharacter *cptr);
void LookForAWay(TCharacter *cptr, BOOL wc, BOOL mc);

// AI helpers
// Diagnostic event line for the AI trace (ai_logging 1): one call per
// awareness acquisition or stimulus, with the numbers that decided it.
// Kinds in use: shot, hit, call, contact, sight, scent, startle, proximity.
void TraceHunterEvent(const TCharacter *cptr, const char *kind, float distance,
                      float eventRange, float hearingRange);
void SetNewTargetPlace(TCharacter *cptr, float R);
void SetNewTargetPlaceRegion(TCharacter *cptr, float R);
void SetNewTargetPlace_Icth(TCharacter *cptr, float R);
void SetNewTargetPlace_Brahi(TCharacter *cptr, float R);
void SetNewTargetPlaceFish(TCharacter *cptr, float R);
void SetNewTargetPlaceMosasaurus(TCharacter *cptr, float R);
BOOL ReplaceCharacterForward(TCharacter *cptr);

// Character lifecycle
void ResetCharacter(TCharacter *cptr);
void ProcessPrevPhase(TCharacter *cptr);
void ActivateCharacterFx(TCharacter *cptr);
void ActivateCharacterFxAquatic(TCharacter *cptr);
TCharacter* AddDeadBody(TCharacter *cptr, int phase, bool scream);

// Animation helpers
void ThinkY_Beta_Gamma(TCharacter *cptr, float blook, float glook, float blim, float glim);
float AngleDifference(float a, float b);
float CorrectedAlpha(float a, float b);

// Picks the next phase from an idle-group set when a phase finishes: rolls the
// current group's end chance, then starts a new group by its start chance.
// `groups`/`groupCount` are `DinoInfo[...].idleGroup` with `idleGroupCount`
// (or the idle2 pair); `currentGroup` is the matching
// `cptr->currentIdleGroup`/`currentIdle2Group`. Returns true when the phase
// came from a group -- the caller must then skip the rest of its phase
// selection; false when no group started and `fallbackPhase` was assigned.
// Callers keep their own gate (state, corpse-carried, water).
inline int IdleGroupCount(const TDinoIdleType& group)
{
    const int capacity = static_cast<int>(sizeof(group.anim) / sizeof(group.anim[0]));
    return (group.count > 0 && group.count <= capacity) ? group.count : 0;
}

inline bool SelectIdleGroupPhase(int& phase, const TDinoIdleType* groups,
                                 int groupCount, int& currentGroup,
                                 int fallbackPhase)
{
    if (!groups || groupCount <= 0) {
        currentGroup = -1;
        phase = fallbackPhase;
        return false;
    }
    if (currentGroup >= groupCount) currentGroup = -1;

    if (currentGroup >= 0) {
        const TDinoIdleType& group = groups[currentGroup];
        const int count = IdleGroupCount(group);
        if (count == 0) {
            currentGroup = -1;
        }
        else if (rRand(127) + 1 > (1 - group.end) * 128
            && (group.endOnAny || phase == group.anim[count - 1])) {
            phase = fallbackPhase;
            // The id clears before the instant-repeat case falls through and
            // starts a new group.
            currentGroup = -1;
            if (!group.instantRepeat) {
                return true;
            }
        }
        else {
            phase = group.anim[rRand(count - 1)];
            return true;
        }
    }

    for (int idleGroupNo = 0; idleGroupNo < groupCount; idleGroupNo++) {
        if (IdleGroupCount(groups[idleGroupNo]) == 0) continue;
        if (rRand(127) + 1 > (1 - groups[idleGroupNo].start) * 128) {
            currentGroup = idleGroupNo;
        }
    }
    if (currentGroup >= 0) {
        const TDinoIdleType& group = groups[currentGroup];
        const int count = IdleGroupCount(group);
        if (count == 0) {
            currentGroup = -1;
        }
        else {
            if (group.startOnAny) {
                phase = group.anim[rRand(count - 1)];
            }
            else {
                phase = group.anim[0];
            }
            return true;
        }
    }

    phase = fallbackPhase;
    return false;
}

// Keep normal tracking and fixed event reactions on the same authored range.
inline float GetCharacterAggressionRange(const TCharacter* cptr)
{
    const TDinoInfo& dino = DinoInfo[cptr->CType];
    const TAIInfo& behavior = AIInfo[cptr->Clone];
    // A species-level "agressMulti" override replaces the clone table value;
    // absent (0) keeps the legacy AI-family default. The dangerous Brachiosaurus
    // slots use their legacy formula and are not affected by this override.
    const int aggressMulti = EffectiveAggressMulti(
        DinoAggressMulti[cptr->CType], behavior.agressMulti);
    float aggressionRange;

    if (UsesLegacyBrachiosaurusAggressionRange(cptr->Clone)) {
        aggressionRange = LegacyBrachiosaurusAggressionRange(dino.aggress, OptAgres);
    } else if (dino.Aquatic && cptr->Clone != AI_TREX) {
        const int optionAggression = dino.DangerFish ? OptAgres : 0;
        aggressionRange = GameplayViewRadiusCells(ctViewR) * dino.aggress
            + optionAggression / static_cast<float>(aggressMulti);
    } else if (behavior.carnivore
               && (!behavior.iceAge || cptr->Clone == AI_WOLF)) {
        aggressionRange = GameplayViewRadiusCells(ctViewR) * dino.aggress
            + OptAgres / static_cast<float>(aggressMulti);
    } else {
        aggressionRange = aggressMulti * dino.aggress
            + OptAgres / 8.0f;
    }

    return cptr->gliding ? aggressionRange * 2.0f : aggressionRange;
}

// A hunter event is a stronger stimulus than passive detection, so the event
// reaction uses a scaled copy of the authored aggression range (see
// kHunterEventRangeScale in Constants.h).
inline float GetCharacterHunterEventRange(const TCharacter* cptr)
{
    return GetCharacterAggressionRange(cptr) * kHunterEventRangeScale;
}

inline bool IsFixedHunterPursuit(const TCharacter* cptr)
{
    return IsFixedHunterPursuitState(cptr->hunterAwareness);
}

inline bool IsFixedHunterFlee(const TCharacter* cptr)
{
    return IsFixedHunterFleeState(cptr->hunterAwareness);
}

inline bool IsTimedHunterReaction(const TCharacter* cptr)
{
    return IsTimedHunterReactionState(cptr->hunterAwareness);
}

inline bool TracksHunterExactly(const TCharacter* cptr)
{
    return cptr->hunterAwareness == HunterAwarenessState::TrackingHunter;
}

// The single awareness state: the hunter is relevant to this creature while
// any reaction is active. Replaces the legacy awareHunter boolean, which was
// always written in lockstep with hunterAwareness.
inline bool IsHunterAware(const TCharacter* cptr)
{
    return cptr->hunterAwareness != HunterAwarenessState::None;
}

// Contact-range awareness: while a creature follows a remembered event
// position it still notices a hunter who physically enters its attack reach.
// The horizontal reach is the authored attack distance (scaled, matching the
// aquatic kill check); the vertical tolerance mirrors the animator kill
// branches (at least 256 units of altitude difference, or the species water
// depth, plus the aquatic jumping allowance) so a creature on a cliff does not
// notice a hunter standing far below. This grants no active sight or smell.
inline bool ShouldPromoteFixedPursuitToTracking(const TCharacter* cptr,
                                                const Vector3d& hunterPosition)
{
    const TDinoInfo& dino = DinoInfo[cptr->CType];
    const float attackReach = dino.killDist * cptr->scale;
    if (attackReach <= 0.0f)
        return false;

    float verticalReach = dino.Aquatic
        ? static_cast<float>(cptr->spcDepth)
        : static_cast<float>(dino.waterLevel);
    if (verticalReach < 256.0f)
        verticalReach = 256.0f;
    if (dino.Aquatic && AIInfo[cptr->Clone].jumper
        && cptr->Phase == dino.jumpAnim)
        verticalReach += 80.0f;

    const float dx = hunterPosition.x - cptr->pos.x;
    const float dz = hunterPosition.z - cptr->pos.z;

    return ShouldPromotePursuitToTracking(
        IsFixedHunterPursuit(cptr),
        fabs(hunterPosition.y - cptr->pos.y) <= verticalReach + 20.0f,
        dx * dx + dz * dz, attackReach * attackReach);
}

// A pack member without its own tracking follows the pack's hunt anchor --
// the position of the packmate that is currently tracking the hunter, while
// it is fresh -- and falls back to the leader's live position; when afraid it
// flees radially away from that point. The leader itself only follows a fresh
// anchor: without one it keeps its own event or wander destination instead of
// freezing on its own position. The anchor is a packmate's position, never
// the hunter's.
inline TCharacter* PackLeader(const TCharacter* cptr)
{
    if (!cptr || cptr->packId < 0 || cptr->packId >= PackCount) return nullptr;
    return Packs[cptr->packId].leader;
}

inline void SetPackLeaderTarget(TCharacter* cptr, bool flee)
{
    TCharacter* leader = PackLeader(cptr);
    if (!leader) return;

    const bool anchorFresh = IsPackHuntAnchorFresh(
        RealTime, PackHuntTime[cptr->packId], kPackHuntAnchorTime);
    if (!ShouldFollowPackTarget(anchorFresh, leader == cptr)) return;

    const float targetX = anchorFresh ? PackHuntX[cptr->packId] : leader->pos.x;
    const float targetZ = anchorFresh ? PackHuntZ[cptr->packId] : leader->pos.z;
    if (!flee) {
        cptr->tgx = targetX;
        cptr->tgz = targetZ;
        cptr->tgtime = 0;
        return;
    }

    Vector3d away;
    away.x = cptr->pos.x - targetX;
    away.y = 0.0f;
    away.z = cptr->pos.z - targetZ;
    NormVector(away, 2048.0f);
    cptr->tgx = cptr->pos.x + away.x;
    cptr->tgz = cptr->pos.z + away.z;
    cptr->tgtime = 0;
}

// Character targets must stay inside the playable map; the legacy target
// pickers clamp to these bounds and an unbounded flee/search extension can
// otherwise park a creature against the world edge.
inline float ClampCharacterTargetCoordinate(float value)
{
    constexpr float kTargetMin = 512.0f;
    constexpr float kTargetMax = 1018.0f * 256.0f;
    if (value < kTargetMin) return kTargetMin;
    if (value > kTargetMax) return kTargetMax;
    return value;
}

inline void ClearHunterReaction(TCharacter* cptr)
{
    cptr->hunterAwareness = HunterAwarenessState::None;
    cptr->AfraidTime = 0;
    cptr->State = 0;
    cptr->tgx = cptr->pos.x;
    cptr->tgz = cptr->pos.z;
    cptr->tgtime = 0;
}
