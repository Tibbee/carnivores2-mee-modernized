// ==========================================================================
// CharacterInternal.h -- Internal declarations shared across Character module
// ==========================================================================
// Functions declared here are used by multiple Character/*.cpp files but are
// not exposed to the rest of the engine (not in EngineAPI.h).

#pragma once

#include "Core/GameTypes.h"

// Collision / placement checks
int CheckPlaceCollisionP(Vector3d &v, bool aquatic);
int CheckPlaceCollisionFishP(Vector3d &v, int minDepth, int maxDepth);
int CheckPlaceCollisionFish(TCharacter *cptr, Vector3d &v, float mosaDepth, int maxDepth, int minDepth);
int CheckPlaceCollisionMosasaurus(TCharacter *cptr, Vector3d &v, float mosaDepth);
bool jumpCollision(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollision(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionMicro(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionLandBrahi(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionBrahi(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);
int CheckPlaceCollisionBrahiP(Vector3d &v);
int CheckPlaceCollision2(TCharacter *cptr, Vector3d &v, BOOL wc);
int CheckPossiblePath(TCharacter *cptr, BOOL wc, BOOL mc);

// Movement
void MoveCharacter(TCharacter *cptr, float dx, float dz, BOOL wc, BOOL mc);
void MoveCharacter2(TCharacter *cptr, float dx, float dz);
void MoveCharacterFish(TCharacter *cptr, float dx, float dz);
void MoveCharacterMosasaurus(TCharacter *cptr, float dx, float dz);
void Characters_AddSecondaryOne(TCharacter *cptr);
void LookForAWay(TCharacter *cptr, BOOL wc, BOOL mc);

// AI helpers
void SetNewTargetPlace(TCharacter *cptr, float R);
void SetNewTargetPlaceVanilla(TCharacter *cptr, float R);
void SetNewTargetPlaceRegion(TCharacter *cptr, float R);
void SetNewTargetPlace_Icth(TCharacter *cptr, float R);
void SetNewTargetPlace_IcthOld(TCharacter *cptr, float R);
void SetNewTargetPlace_Brahi(TCharacter *cptr, float R);
void SetNewTargetPlaceFish(TCharacter *cptr, float R);
void SetNewTargetPlaceMosasaurus(TCharacter *cptr, float R);
BOOL ReplaceCharacterForward(TCharacter *cptr);
boolean huntDogSearch(TCharacter *cptr);

// Character lifecycle
void ResetCharacter(TCharacter *cptr);
void ProcessPrevPhase(TCharacter *cptr);
void ActivateCharacterFx(TCharacter *cptr);
void ActivateCharacterFxAquatic(TCharacter *cptr);
void AddDeadBody(TCharacter *cptr, int phase, bool scream);

// Animation helpers
void ThinkY_Beta_Gamma(TCharacter *cptr, float blook, float glook, float blim, float glim);
Vector3d LookForATree(TCharacter *cptr);
Vector3d CheckForATree(TCharacter *cptr);
float AngleDifference(float a, float b);
float CorrectedAlpha(float a, float b);

// Inline helpers
inline float GetAngleDifference(float a, float b) { return AngleDifference(a, b); }

inline bool IsInvestigatingShot(const TCharacter* cptr)
{
    return cptr->hunterAwareness == HunterAwarenessState::InvestigatingShot;
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

inline void SetPackLeaderTarget(TCharacter* cptr, bool flee)
{
    if (cptr->packId < 0 || !Packs[cptr->packId].leader) return;

    TCharacter* leader = Packs[cptr->packId].leader;
    if (!flee) {
        cptr->tgx = leader->pos.x;
        cptr->tgz = leader->pos.z;
        cptr->tgtime = 0;
        return;
    }

    Vector3d away = SubVectors(cptr->pos, leader->pos);
    away.y = 0.0f;
    NormVector(away, 2048.0f);
    cptr->tgx = cptr->pos.x + away.x;
    cptr->tgz = cptr->pos.z + away.z;
    cptr->tgtime = 0;
}

inline void ClearHunterReaction(TCharacter* cptr)
{
    cptr->awareHunter = false;
    cptr->hunterAwareness = HunterAwarenessState::None;
    cptr->AfraidTime = 0;
    cptr->State = 0;
    cptr->tgx = cptr->pos.x;
    cptr->tgz = cptr->pos.z;
    cptr->tgtime = 0;
}

// Pack following helpers
// Called from animation functions to maintain pack formation
inline void AnimatePackFollow(TCharacter* cptr, float leaderDist)
{
    if (cptr->followLeader) {
        if (leaderDist < cptr->packDensity * 128.0f * 0.6f)
            cptr->followLeader = false;
    } else {
        if (leaderDist > cptr->packDensity * 128.0f * 1.3f)
            cptr->followLeader = true;
    }
}

inline void AnimatePackFollowSq(TCharacter* cptr, float leaderDistSq)
{
    if (cptr->followLeader) {
        float minDist = cptr->packDensity * 128.0f * 0.6f;
        if (leaderDistSq < minDist * minDist)
            cptr->followLeader = false;
    } else {
        float maxDist = cptr->packDensity * 128.0f * 1.3f;
        if (leaderDistSq > maxDist * maxDist)
            cptr->followLeader = true;
    }
}
