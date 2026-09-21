// ==========================================================================
// CharacterAwareness.cpp -- Single hunter-stimulus resolver
// ==========================================================================
// Extracted from MakeNoise(), ReactToHunterCall() and registerDamage() as
// migration step 1 of the hunter-awareness redesign. Behavior is intentionally
// unchanged: the per-creature decision code now lives in one owner, and the
// event handlers are thin callers.

#include "Hunt.h"
#include "Game/CharacterAwareness.h"
#include "Game/CharacterInternal.h"

namespace
{

bool ApplyGunshotHeard(TCharacter& character, const THunterStimulus& stimulus)
{
	TCharacter* cptr = &character;

	// StateF == 0xFF marks static exhibits and carried bodies. Their State
	// is not an AI state and must never be rewritten by awareness events.
	if (cptr->StateF == 0xFF) return false;
	if (!cptr->Health) return false;
	if ((DinoInfo[cptr->CType].Aquatic && cptr->Clone != AI_TREX)
		|| cptr->Clone == AI_HUNTDOG) return false;

	Vector3d position = stimulus.position;
	const float distance = VectorLength(SubVectors(cptr->pos, position));
	const float hearingRange = stimulus.soundRange
		* (DinoInfo[cptr->CType].HearK * 2);
	if (distance > hearingRange) return false;

	// Do not replace exact awareness from sight or direct damage with the
	// less precise information supplied by a subsequent gunshot.
	if (cptr->awareHunter
		&& cptr->hunterAwareness != HunterAwarenessState::InvestigatingShot
		&& cptr->hunterAwareness != HunterAwarenessState::FleeingFromShot)
		return false;

	const bool isTRex = cptr->Clone == AI_TREX;
	// Give a slow creature enough time to reach a distant event; the
	// remaining time then funds the local area search around it.
	const float travelSpeed = DinoInfo[cptr->CType].runspd * cptr->scale;
	const int reactionTime = ShotInvestigationTimeForTravel(
		ShotInvestigationTime(distance, hearingRange, isTRex), distance,
		travelSpeed);
	cptr->AfraidTime = reactionTime;
	cptr->NoFindCnt = 0;
	cptr->awareHunter = true;
	if (!cptr->State) cptr->State = 2;

	const TDinoInfo& dino = DinoInfo[cptr->CType];
	const bool fearsShot = dino.fearHearShot
		|| (dino.defensive && cptr->Health == dino.Health0);
	// A gunshot is a sound: species whose authored (event-scaled) range
	// covers it investigate the position, while low-aggression species
	// flee from it. Authored fear and passivity always flee, and the
	// T-Rex has no flee state and always investigates audible shots.
	const bool fleesShot = ShouldFleeFromHunterEvent(
		dino.aggress, fearsShot, distance,
		GetCharacterHunterEventRange(cptr), isTRex);
	cptr->hunterAwareness = HeardShotReactionState(fleesShot);
	if (fleesShot) {
		Vector3d away = SubVectors(cptr->pos, position);
		away.y = 0.0f;
		NormVector(away, 2048.0f);
		cptr->tgx = ClampCharacterTargetCoordinate(cptr->pos.x + away.x);
		cptr->tgz = ClampCharacterTargetCoordinate(cptr->pos.z + away.z);
	} else {
		cptr->tgx = ClampCharacterTargetCoordinate(position.x);
		cptr->tgz = ClampCharacterTargetCoordinate(position.z);
	}
	cptr->tgtime = 0;
	return true;
}

bool ApplyHunterCall(TCharacter& character, const THunterStimulus& stimulus)
{
	if (stimulus.callIndex < 0 || stimulus.callIndex >= 64) return false;

	TCharacter* cptr = &character;
	if (cptr->StateF == 0xFF) return false;
	if (!cptr->Health) return false;
	if (!DinoInfo[cptr->CType].fearCall[stimulus.callIndex]) return false;
	if (cptr->Clone == AI_DIMOR || cptr->Clone == AI_PTERA
		|| cptr->Clone == AI_BRACH) return false;

	Vector3d position = stimulus.position;
	const float distance = VectorLength(SubVectors(cptr->pos, position));
	const float hearingRange = GameplayViewRadiusCells(ctViewR) * 400.0f
		* (DinoInfo[cptr->CType].HearK * 2.0f);
	if (distance > hearingRange) return false;

	// A call can refresh its own flee response, but it must not replace
	// exact sight, scent, or direct-hit information.
	if (cptr->awareHunter
		&& cptr->hunterAwareness != HunterAwarenessState::FleeingFromCall)
		return false;

	Vector3d away = SubVectors(cptr->pos, position);
	away.y = 0.0f;
	NormVector(away, 2048.0f);
	cptr->tgx = ClampCharacterTargetCoordinate(cptr->pos.x + away.x);
	cptr->tgz = ClampCharacterTargetCoordinate(cptr->pos.z + away.z);
	cptr->tgtime = 0;
	cptr->State = 2;
	cptr->AfraidTime = (10 + rRand(5)) * 1024;
	cptr->NoFindCnt = 0;
	cptr->awareHunter = true;
	cptr->hunterAwareness = HunterAwarenessState::FleeingFromCall;
	return true;
}

bool ApplyDirectHit(TCharacter& character, const THunterStimulus& stimulus)
{
	TCharacter* cptr = &character;
	const TDinoInfo& info = DinoInfo[cptr->CType];
	const bool wasAware = cptr->awareHunter;
	const bool wasTrackingHunter = TracksHunterExactly(cptr);
	const HunterAwarenessState previousAwareness = cptr->hunterAwareness;

	const float sourceDx = cptr->pos.x - stimulus.position.x;
	const float sourceDz = cptr->pos.z - stimulus.position.z;
	const float sourceDistance = static_cast<float>(
		sqrt(sourceDx * sourceDx + sourceDz * sourceDz));
	const bool fearsHit = cptr->Clone != AI_TREX
		&& ((info.defensive && cptr->Health == info.Health0)
			|| (info.fearShot && cptr->Health < info.Health0));
	// A direct hit is a stronger stimulus than passive detection: species
	// whose authored (event-scaled) range covers the source retaliate, while
	// low-aggression species flee from it. Authored fear and passivity always
	// flee; the T-Rex has no flee path.
	const bool fleesHit = ShouldFleeFromHunterEvent(
		info.aggress, fearsHit, sourceDistance,
		GetCharacterHunterEventRange(cptr), cptr->Clone == AI_TREX);
	const bool preservesExactTracking = wasTrackingHunter && !fleesHit;

	cptr->awareHunter = true;
	cptr->hunterAwareness = UpdatedDirectHitAwarenessState(
		cptr->hunterAwareness, fleesHit);
	cptr->AfraidTime = 60 * 1000;

	// Exact sight or scent awareness is stronger than another aggressive hit.
	// Keep tracking the moving hunter instead of restarting an alert animation
	// or downgrading to a fixed retaliation target on every bullet. Authored
	// fear responses can still replace tracking with a flee reaction.
	if (!preservesExactTracking) {
		if (ShouldInitializeDirectHitAlert(true, wasAware))
			cptr->State = 2;

		if (fleesHit) {
			Vector3d away;
			away.x = cptr->pos.x - stimulus.position.x;
			away.y = 0.0f;
			away.z = cptr->pos.z - stimulus.position.z;
			NormVector(away, 2048.0f);
			cptr->tgx = ClampCharacterTargetCoordinate(cptr->pos.x + away.x);
			cptr->tgz = ClampCharacterTargetCoordinate(cptr->pos.z + away.z);
		} else {
			cptr->tgx = ClampCharacterTargetCoordinate(stimulus.position.x);
			cptr->tgz = ClampCharacterTargetCoordinate(stimulus.position.z);
			if (info.Aquatic) cptr->tdepth = stimulus.position.y;
		}
		cptr->tgtime = 0;
	}
	cptr->BloodTTime += 90000;

	// A T-Rex that heard the shot may already be playing its look/roar
	// notice. A direct hit cancels that and charges immediately; repeated
	// hits during an active retaliation or exact tracking keep the current
	// pursuit instead of restarting it.
	if (cptr->Clone == AI_TREX && cptr->Health
		&& ShouldRestartTRexHitPursuit(previousAwareness))
		cptr->State = cptr->State ? 5 : 1;
	return true;
}

void SelectHunterSearchTarget(TCharacter* cptr)
{
	// The local area search after a fixed pursuit reaches its stored event
	// position uses the same family-specific target picker as ordinary
	// wandering.
	switch (cptr->Clone)
	{
	case AI_BRACH:
	case AI_BRACHDANGER:
	case AI_LANDBRACH:
		SetNewTargetPlace_Brahi(cptr, kShotSearchRadius);
		break;
	case AI_MOSA:
	case AI_FISH:
		SetNewTargetPlaceFish(cptr, kShotSearchRadius);
		break;
	default:
		SetNewTargetPlace(cptr, kShotSearchRadius);
		break;
	}
}

} // namespace

bool ApplyHunterStimulus(TCharacter& character, const THunterStimulus& stimulus)
{
	switch (stimulus.kind)
	{
	case HunterStimulusKind::GunshotHeard:
		return ApplyGunshotHeard(character, stimulus);
	case HunterStimulusKind::DirectHit:
		return ApplyDirectHit(character, stimulus);
	case HunterStimulusKind::HunterCall:
		return ApplyHunterCall(character, stimulus);
	}
	return false;
}

void UpdateHunterNavigation(TCharacter& character)
{
	if (!character.Health)
		return;

	TCharacter* cptr = &character;

	// Fixed flee: the stored point is behind the creature once reached, so the
	// flee direction is extended and the creature keeps running instead of
	// turning back and circling.
	if (IsFixedHunterFlee(cptr)) {
		ExtendFixedFleeTarget(cptr);
		return;
	}

	// Fixed pursuit: once the creature reaches the stored event position with
	// time left on the reaction, it searches the area around it instead of
	// running past it or dropping straight to normal wander. An expired
	// reaction was already cleared by the central tick, so only the remaining
	// time case reaches this check.
	if (IsFixedHunterPursuit(cptr) && cptr->AfraidTime > 0) {
		const float dx = cptr->tgx - cptr->pos.x;
		const float dz = cptr->tgz - cptr->pos.z;
		if (dx * dx + dz * dz
			<= kShotInvestigationArrivalRadius
				* kShotInvestigationArrivalRadius) {
			SelectHunterSearchTarget(cptr);
		}
	}
}
