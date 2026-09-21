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
