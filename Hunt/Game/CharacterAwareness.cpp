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

// Opt-in diagnostic trace (verbose_logging 1): one line per awareness
// event with the numbers that decided it and the stored destination.
void TraceHunterEvent(const TCharacter* cptr, const char* kind, float distance,
                      float eventRange, float hearingRange)
{
	if (!g_VerboseLogging) return;
	char buf[256];
	sprintf_s(buf, sizeof(buf),
		"[AI] %s clone=%d dist=%.0f eventR=%.0f hearR=%.0f -> %s target=(%.0f,%.0f) pos=(%.0f,%.0f) afraid=%d\n",
		kind, cptr->Clone, distance, eventRange, hearingRange,
		HunterAwarenessStateName(cptr->hunterAwareness),
		cptr->tgx, cptr->tgz, cptr->pos.x, cptr->pos.z, cptr->AfraidTime);
	PrintLogVerbose(buf);
}

// The ideal flee point is directly away from the source, but that ray can
// land in water, on a blocked cell, or on a cliff face. A creature sent to an
// unreachable point presses into the obstacle and runs along it, which reads
// as circling instead of fleeing. Try the away heading first, then rotations
// to either side in 15-degree steps, and take the first point that passes the
// placement check. Falls back to the ideal point when no heading is placeable
// (for example, a creature on a small island).
void SetHunterFleeTargetAway(TCharacter* cptr, float awayDx, float awayDz)
{
	const float length = sqrt(awayDx * awayDx + awayDz * awayDz);
	float baseX;
	float baseZ;
	if (length > 0.0f) {
		baseX = awayDx / length;
		baseZ = awayDz / length;
	} else {
		baseX = cptr->lookx;
		baseZ = cptr->lookz;
	}
	const float baseAngle = static_cast<float>(atan2(baseZ, baseX));
	const float step = static_cast<float>(pi) / 12.0f;

	for (int index = 0; index <= 6; ++index) {
		const float offset = step * index;
		for (int side = 0; side < (index == 0 ? 1 : 2); ++side) {
			const float angle = baseAngle + (side == 0 ? offset : -offset);
			Vector3d p;
			p.x = ClampCharacterTargetCoordinate(
				cptr->pos.x + static_cast<float>(cos(angle)) * 2048.0f);
			p.z = ClampCharacterTargetCoordinate(
				cptr->pos.z + static_cast<float>(sin(angle)) * 2048.0f);
			p.y = 0.0f;
			if (!CheckPlaceCollisionP(p, cptr->cpcpAquatic)) {
				cptr->tgx = p.x;
				cptr->tgz = p.z;
				cptr->tgtime = 0;
				return;
			}
		}
	}

	cptr->tgx = ClampCharacterTargetCoordinate(cptr->pos.x + baseX * 2048.0f);
	cptr->tgz = ClampCharacterTargetCoordinate(cptr->pos.z + baseZ * 2048.0f);
	cptr->tgtime = 0;
}

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
	if (IsHunterAware(cptr)
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
		SetHunterFleeTargetAway(cptr, cptr->pos.x - position.x,
			cptr->pos.z - position.z);
	} else {
		cptr->tgx = ClampCharacterTargetCoordinate(position.x);
		cptr->tgz = ClampCharacterTargetCoordinate(position.z);
	}
	cptr->tgtime = 0;
	TraceHunterEvent(cptr, "shot", distance, GetCharacterHunterEventRange(cptr),
		hearingRange);
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
	if (IsHunterAware(cptr)
		&& cptr->hunterAwareness != HunterAwarenessState::FleeingFromCall)
		return false;

	SetHunterFleeTargetAway(cptr, cptr->pos.x - position.x,
		cptr->pos.z - position.z);
	cptr->State = 2;
	cptr->AfraidTime = (10 + rRand(5)) * 1024;
	cptr->NoFindCnt = 0;
	cptr->hunterAwareness = HunterAwarenessState::FleeingFromCall;
	TraceHunterEvent(cptr, "call", distance, GetCharacterHunterEventRange(cptr),
		hearingRange);
	return true;
}

bool ApplyDirectHit(TCharacter& character, const THunterStimulus& stimulus)
{
	TCharacter* cptr = &character;
	const TDinoInfo& info = DinoInfo[cptr->CType];
	const bool wasAware = IsHunterAware(cptr);
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
			SetHunterFleeTargetAway(cptr, cptr->pos.x - stimulus.position.x,
				cptr->pos.z - stimulus.position.z);
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
	TraceHunterEvent(cptr, "hit", sourceDistance,
		GetCharacterHunterEventRange(cptr), 0.0f);
	return true;
}

bool ApplyContact(TCharacter& character, const THunterStimulus& stimulus)
{
	TCharacter* cptr = &character;

	// A creature following a remembered event position still notices a hunter
	// who physically enters its attack reach. The promotion grants exact
	// tracking, never a kill at the stored point; flee reactions keep their
	// direction because they are not fixed pursuits. The detached observer
	// camera is exempt, debug mode is not.
	if (!MyHealth || !cptr->Health || cptr->StateF == 0xFF)
		return false;
	if (ObservMode)
		return false;
	if (!ShouldPromoteFixedPursuitToTracking(cptr, stimulus.position))
		return false;

	cptr->hunterAwareness = HunterAwarenessState::TrackingHunter;
	cptr->AfraidTime = kCloseRangeAwarenessTime;
	cptr->NoFindCnt = 0;
	const float contactDx = cptr->pos.x - stimulus.position.x;
	const float contactDz = cptr->pos.z - stimulus.position.z;
	TraceHunterEvent(cptr, "contact",
		static_cast<float>(sqrt(contactDx * contactDx + contactDz * contactDz)),
		GetCharacterHunterEventRange(cptr), 0.0f);
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

enum class HunterAIFamily
{
	None,
	Standard,
	Brahi,
	TRex,
	Fish
};

// The animator a species belongs to decides which live-target policy applies.
// The clone lists mirror the animator dispatch in CharacterAnimation.cpp.
HunterAIFamily GetHunterAIFamily(const TCharacter* cptr)
{
	switch (cptr->Clone)
	{
	case AI_PARA:
	case AI_ANKY:
	case AI_PACH:
	case AI_STEGO:
	case AI_ALLO:
	case AI_CHASM:
	case AI_VELO:
	case AI_SPINO:
	case AI_CERAT:
	case AI_BRONT:
	case AI_HOG:
	case AI_WOLF:
	case AI_RHINO:
	case AI_DEER:
	case AI_SMILO:
	case AI_MAMM:
	case AI_BEAR:
		return HunterAIFamily::Standard;
	case AI_BRACHDANGER:
	case AI_LANDBRACH:
		return HunterAIFamily::Brahi;
	case AI_TREX:
		return HunterAIFamily::TRex;
	case AI_MOSA:
	case AI_FISH:
		return HunterAIFamily::Fish;
	default:
		return HunterAIFamily::None;
	}
}

void SetHunterPlayerTarget(TCharacter* cptr)
{
	cptr->tgx = PlayerX;
	cptr->tgz = PlayerZ;
	cptr->tgtime = 0;
}

// Live flee destinations go through the same walkability check as the event
// reactions, so a creature that sees the hunter does not target water or a
// cliff either.
void SetHunterFleeTarget(TCharacter* cptr, float hunterDx, float hunterDz)
{
	// The hunter direction points at the threat; the flee ray points away.
	SetHunterFleeTargetAway(cptr, -hunterDx, -hunterDz);
}

// Live hunter targets belong to an active response; idle and wandering
// creatures keep their own destinations. A pack member can be woken by the
// pack alert before its own State rises (the animators' alertInit block), so
// that frame is included here as well; the guard mirrors the per-family
// alertInit conditions. Fish acquire in their own idle block, so a fish
// acquisition frame still lands its live target on the following frame.
void UpdateLiveHunterNavigation(TCharacter* cptr, HunterAIFamily family)
{
	const bool packWake = cptr->State == 0
		&& (family == HunterAIFamily::Standard
			|| family == HunterAIFamily::Brahi
			|| family == HunterAIFamily::TRex)
		&& cptr->packId >= 0 && Packs[cptr->packId]._alert
		&& (family == HunterAIFamily::Brahi
			|| family == HunterAIFamily::TRex
			|| MyHealth || AIInfo[cptr->Clone].carnivore);
	if (cptr->State == 0 && !packWake)
		return;

	const bool fixedPursuit = IsFixedHunterPursuit(cptr);
	const bool fixedFlee = IsFixedHunterFlee(cptr);
	const bool tracksHunter = TracksHunterExactly(cptr);

	switch (family)
	{
	case HunterAIFamily::Standard:
	case HunterAIFamily::Brahi: {
		const THunterGeometry hunter = GetHunterGeometry(cptr);
		const bool hunterAttackable = family != HunterAIFamily::Brahi
			|| (GetLandUpH(PlayerX, PlayerZ) - GetLandH(PlayerX, PlayerZ)) <= 550.0f;
		if (ShouldFleeHunter(*cptr, hunter.distanceSquared, hunterAttackable)) {
			if (!fixedFlee) {
				if (tracksHunter || cptr->packId < 0)
					SetHunterFleeTarget(cptr, hunter.dx, hunter.dz);
				else
					SetPackLeaderTarget(cptr, true);
			}
		}
		else if (!fixedPursuit) {
			if (tracksHunter || cptr->packId < 0)
				SetHunterPlayerTarget(cptr);
			else
				SetPackLeaderTarget(cptr, false);
		}
		break;
	}
	case HunterAIFamily::TRex:
		if (!fixedPursuit) {
			if (tracksHunter)
				SetHunterPlayerTarget(cptr);
			else
				SetPackLeaderTarget(cptr, false);
		}
		break;
	case HunterAIFamily::Fish: {
		const THunterGeometry hunter = GetHunterGeometry(cptr);
		if (!fixedPursuit && tracksHunter
			&& (DinoInfo[cptr->CType].DangerFish
				|| g_GameMode == GameMode::SurvivalMode)) {
			SetHunterPlayerTarget(cptr);
			cptr->tdepth = PlayerY;

			// Mosa target depth failsafes (kept from AnimateFish).
			if (cptr->tdepth > GetLandUpH(cptr->tgx, cptr->tgz) - (cptr->spcDepth * 0.75)) {
				cptr->tdepth = GetLandUpH(cptr->pos.x, cptr->pos.z) - (cptr->spcDepth * 0.75);
			}

			// Target above the player so the fish can reach jumping depth in time.
			if (AIInfo[cptr->Clone].jumper && cptr->depth < cptr->tdepth) {
				cptr->tdepth += (cptr->tdepth - cptr->depth) * 3;
			}
		}
		else if (!fixedPursuit && !fixedFlee) {
			SetHunterFleeTarget(cptr, hunter.dx, hunter.dz);
			cptr->tdepth = GetLandH(cptr->pos.x, cptr->pos.z) +
				((GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z)) / 2);
		}
		break;
	}
	default:
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
	case HunterStimulusKind::Contact:
		return ApplyContact(character, stimulus);
	}
	return false;
}

THunterGeometry GetHunterGeometry(const TCharacter* cptr)
{
	const float offset = HunterLookOffset(cptr->Clone, cptr->scale,
		AIInfo[cptr->Clone].carnivore);
	THunterGeometry geometry;
	geometry.dx = PlayerX - cptr->pos.x - cptr->lookx * offset;
	geometry.dz = PlayerZ - cptr->pos.z - cptr->lookz * offset;
	geometry.distanceSquared = geometry.dx * geometry.dx + geometry.dz * geometry.dz;
	geometry.distance = static_cast<float>(sqrt(geometry.distanceSquared));
	return geometry;
}

bool CanKillHunter(const TCharacter& character, const THunterGeometry& hunter)
{
	const TCharacter* cptr = &character;
	if (!MyHealth || !cptr->Health || cptr->StateF == 0xFF || ObservMode)
		return false;
	if (!HunterAwarenessAllowsKill(cptr->hunterAwareness))
		return false;

	const TDinoInfo& dino = DinoInfo[cptr->CType];
	const float verticalDifference = static_cast<float>(fabs(PlayerY - cptr->pos.y));

	// Family-specific attack reach: the horizontal metric and the vertical
	// allowance mirror the animator kill checks that were unified here.
	switch (GetHunterAIFamily(cptr))
	{
	case HunterAIFamily::Brahi:
		return IsWithinLinearReach(hunter.distance, dino.killDist)
			&& IsWithinKillAltitude(
				static_cast<float>(fabs(PlayerY - cptr->pos.y - 120.0f)), 256.0f);
	case HunterAIFamily::Fish: {
		float killAlt = static_cast<float>(cptr->spcDepth);
		if (killAlt < 256.0f) killAlt = 256.0f;
		if (AIInfo[cptr->Clone].jumper && cptr->Phase == dino.jumpAnim)
			killAlt += 80.0f;
		return IsWithinSquaredReach(hunter.distanceSquared,
				dino.killDist * cptr->scale)
			&& IsWithinKillAltitude(verticalDifference,
				killAlt + 20.0f * cptr->scale);
	}
	case HunterAIFamily::Standard:
	case HunterAIFamily::TRex: {
		float killAlt = static_cast<float>(dino.waterLevel);
		if (killAlt < 256.0f) killAlt = 256.0f;
		return IsWithinSquaredReach(hunter.distanceSquared, dino.killDist)
			&& IsWithinKillAltitude(verticalDifference, killAlt + 20.0f);
	}
	default:
		return false;
	}
}

bool ShouldFleeHunter(const TCharacter& character, float hunterDistanceSquared,
                      bool hunterAttackable)
{
	const TCharacter* cptr = &character;
	const TDinoInfo& dino = DinoInfo[cptr->CType];
	const bool fixedPursuit = IsFixedHunterPursuit(cptr);
	const bool fixedFlee = IsFixedHunterFlee(cptr);
	const bool fixedReaction = fixedPursuit || fixedFlee;
	const bool tracksHunter = TracksHunterExactly(cptr);
	const bool packAttackOverride = cptr->packId >= 0
		&& Packs[cptr->packId]._attack && !fixedReaction;

	bool flee = false;
	if (g_GameMode != GameMode::SurvivalMode) {
		const bool recentlyDamaged = cptr->BloodTTime > 0;
		const bool outsideRange = !fixedPursuit
			&& (OutsideNormalAggressionRangeSquared(hunterDistanceSquared,
					GetCharacterAggressionRange(cptr), recentlyDamaged)
				|| !hunterAttackable);
		const bool authoredFlee = ShouldFleeFromAuthoredThreat(
			outsideRange, dino.aggress <= 0, IsHunterAware(cptr),
			dino.defensive && cptr->Health == dino.Health0,
			dino.fearShot && cptr->Health < dino.Health0,
			cptr->hunterAwareness == HunterAwarenessState::FleeingFromShot);
		if (authoredFlee) {
			flee = true;
		}
		else if (!fixedReaction && tracksHunter && cptr->packId >= 0) {
			Packs[cptr->packId].attack = true;
		}
	}
	if (fixedFlee)
		flee = true;
	if (packAttackOverride)
		flee = false;
	return flee;
}

void UpdateHunterNavigation(TCharacter& character)
{
	if (!character.Health)
		return;

	TCharacter* cptr = &character;

	if (cptr->StateF == 0xFF)
		return;

	// Single timer owner: the timed fixed reactions were already decremented
	// and cleared by the central tick; every other reaction timer (exact
	// tracking and morale) ticks here, exactly once per frame. This replaces
	// the per-animator decrements, including the special tick that contact
	// promotion used to need, and guarantees that a tracking lock expires.
	if (!IsTimedHunterReaction(cptr))
		cptr->AfraidTime = TickUntimedReaction(cptr->AfraidTime, TimeDt);

	// Fixed flee: once the stored point is reached, extend the leg along the
	// creature's heading so the escape keeps running instead of orbiting the
	// point (extending along the bearing turned overshooting creatures back).
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
			if (g_VerboseLogging) {
				char buf[256];
				sprintf_s(buf, sizeof(buf),
					"[AI] search clone=%d state=%s pos=(%.0f,%.0f) oldTarget=(%.0f,%.0f)\n",
					cptr->Clone, HunterAwarenessStateName(cptr->hunterAwareness),
					cptr->pos.x, cptr->pos.z, cptr->tgx, cptr->tgz);
				PrintLogVerbose(buf);
			}
			SelectHunterSearchTarget(cptr);
		}
	}

	UpdateLiveHunterNavigation(cptr, GetHunterAIFamily(cptr));
}
