// ==========================================================================
// CharacterAwareness.h -- Single hunter-stimulus resolver
// ==========================================================================
// Migration step 1 of the hunter-awareness redesign (design review:
// CarnivoresDoc/architecture/hunter-awareness-review.md). The three event
// channels -- heard gunshot, direct hit and hunter call -- share one resolver
// that owns eligibility, awareness priority, the flee/pursue classification,
// the reaction timer and the stored event position.
//
// Later steps move the per-frame navigation, flee and kill decisions here as
// well, after which the animators become consumers of a single intent instead
// of re-deciding against live player coordinates.

#pragma once

#include "Core/GameTypes.h"

enum class HunterStimulusKind
{
    GunshotHeard,
    DirectHit,
    HunterCall
};

struct THunterStimulus
{
    HunterStimulusKind kind = HunterStimulusKind::GunshotHeard;
    // World position of the stimulus: shot origin, damage source or call origin.
    Vector3d position = {};
    // GunshotHeard only: authored weapon noise range before the species HearK.
    float soundRange = 0.0f;
    // HunterCall only: index into the species fearCall table.
    int callIndex = -1;
};

// Resolves one stimulus for one creature. Returns true when the creature was
// eligible for this stimulus type, whether or not its awareness changed.
bool ApplyHunterStimulus(TCharacter& character, const THunterStimulus& stimulus);

// Hunter position relative to the creature's reaction point. The look-offset
// table lives in HunterLookOffset, so every animator and the navigator measure
// the same distance and flee direction for the same species.
struct THunterGeometry
{
    float dx = 0.0f;
    float dz = 0.0f;
    float distanceSquared = 0.0f;
    float distance = 0.0f;
};

THunterGeometry GetHunterGeometry(const TCharacter* cptr);

// The single kill gate for the hunter-directed animators. The player must be
// alive, the creature must be tracking the hunter exactly (a fixed reaction
// or an expired lock never authorizes a kill) and the hunter must be inside
// the family's attack reach. Each animator only plays its own kill animation.
bool CanKillHunter(const TCharacter& character, const THunterGeometry& hunter);

// The authored flee/pursue decision for the standard predator family
// (AnimateHuntable, AnimateBrahi and their disabled copies). hunterDistanceSquared
// is the family's hunter distance; hunterAttackable carries the Brahi altitude
// rule (always true for the Huntable family). A fixed flee always wins, and the
// pack attack flag can cancel a live flee but never a fixed reaction.
bool ShouldFleeHunter(const TCharacter& character, float hunterDistanceSquared,
                      bool hunterAttackable);

// The single owner of hunter-directed navigation. Called once per creature per
// frame after the reaction timers have been applied. It owns the fixed flee
// direction, the local search after a fixed pursuit, and the live tracking /
// live flee destinations for the reacting families. Wandering and pack
// movement stay with the animators; they only read the response.
void UpdateHunterNavigation(TCharacter& character);
