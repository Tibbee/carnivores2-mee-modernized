// ==========================================================================
// CharacterAI.cpp -- Character awareness and noise-making logic
// ==========================================================================
// Extracted from Characters.cpp.

#include "Hunt.h"
#include "Game/CharacterInternal.h"

void MakeNoise(Vector3d pos, float range)
{
	for (int c = 0; c < ChCount; c++)
	{
		TCharacter *cptr = &Characters[c];
		// StateF == 0xFF marks static exhibits and carried bodies. Their State
		// is not an AI state and must never be rewritten by awareness events.
		if (cptr->StateF == 0xFF) continue;
		if (!cptr->Health) continue;
		if ((DinoInfo[cptr->CType].Aquatic && cptr->Clone != AI_TREX)
			|| cptr->Clone == AI_HUNTDOG) continue;

		float l = VectorLength(SubVectors(cptr->pos, pos));
		float r = range * (DinoInfo[cptr->CType].HearK * 2);
		if (l > r) continue;

		// Do not replace exact awareness from sight or direct damage with the
		// less precise information supplied by a subsequent gunshot.
		if (cptr->awareHunter
			&& cptr->hunterAwareness != HunterAwarenessState::InvestigatingShot
			&& cptr->hunterAwareness != HunterAwarenessState::FleeingFromShot)
			continue;

		const bool isTRex = cptr->Clone == AI_TREX;
		const int reactionTime = ShotInvestigationTime(l, r, isTRex);
		cptr->AfraidTime = reactionTime;
		cptr->NoFindCnt = 0;
		cptr->awareHunter = true;
		if (!cptr->State) cptr->State = 2;

		const TDinoInfo& dino = DinoInfo[cptr->CType];
		const bool fearsShot = dino.fearHearShot
			|| (dino.defensive && cptr->Health == dino.Health0);
		const float eventDx = cptr->pos.x - pos.x;
		const float eventDz = cptr->pos.z - pos.z;
		const float eventDistance = static_cast<float>(
			sqrt(eventDx * eventDx + eventDz * eventDz));
		// T-Rex has no authored aggression value and its dedicated animator has
		// no flee state. Preserve its legacy behavior: investigate audible shots.
		const bool fleesShot = ShouldFleeFromAwarenessEvent(
			eventDistance, GetCharacterAggressionRange(cptr),
			dino.aggress, fearsShot, isTRex);
		cptr->hunterAwareness = HeardShotReactionState(fleesShot);
		if (fleesShot) {
			Vector3d away = SubVectors(cptr->pos, pos);
			away.y = 0.0f;
			NormVector(away, 2048.0f);
			cptr->tgx = cptr->pos.x + away.x;
			cptr->tgz = cptr->pos.z + away.z;
		} else {
			cptr->tgx = pos.x;
			cptr->tgz = pos.z;
		}
		cptr->tgtime = 0;
	}
}

void ReactToHunterCall(Vector3d pos, int callIndex)
{
	if (callIndex < 0 || callIndex >= 64) return;

	for (int c = 0; c < ChCount; c++)
	{
		TCharacter* cptr = &Characters[c];
		if (cptr->StateF == 0xFF) continue;
		if (!cptr->Health) continue;
		if (!DinoInfo[cptr->CType].fearCall[callIndex]) continue;
		if (cptr->Clone == AI_DIMOR || cptr->Clone == AI_PTERA
			|| cptr->Clone == AI_BRACH) continue;

		const float distance = VectorLength(SubVectors(cptr->pos, pos));
		const float hearingRange = GameplayViewRadiusCells(ctViewR) * 400.0f
			* (DinoInfo[cptr->CType].HearK * 2.0f);
		if (distance > hearingRange) continue;

		// A call can refresh its own flee response, but it must not replace
		// exact sight, scent, or direct-hit information.
		if (cptr->awareHunter
			&& cptr->hunterAwareness != HunterAwarenessState::FleeingFromCall)
			continue;

		Vector3d away = SubVectors(cptr->pos, pos);
		away.y = 0.0f;
		NormVector(away, 2048.0f);
		cptr->tgx = cptr->pos.x + away.x;
		cptr->tgz = cptr->pos.z + away.z;
		cptr->tgtime = 0;
		cptr->State = 2;
		cptr->AfraidTime = (10 + rRand(5)) * 1024;
		cptr->NoFindCnt = 0;
		cptr->awareHunter = true;
		cptr->hunterAwareness = HunterAwarenessState::FleeingFromCall;
	}
}


void CheckAfraid()
{
	if (!MyHealth) return;
	if (g_GameMode == GameMode::TrophyMode) return;

	Vector3d ppos, plook, clook, wlook, rlook;
	ppos = PlayerPos;

	if (DEBUG || IsUnderwater() || ObservMode) return;

	plook.y = 0;
	plook.x = static_cast<float>(sin(CameraAlpha));
	plook.z = static_cast<float>(-cos(CameraAlpha));

	wlook = Wind.nv;

	float kR, kwind, klook, kstand;

	float kmask = 1.0f;
	float kskill = 1.0f;
	float kscent = 1.0f;

	if (CamoMode)  kmask *= 1.5;
	if (ScentMode) kscent *= 1.5;

	for (int c = 0; c < ChCount; c++)
	{
		TCharacter *cptr = &Characters[c];
		// Trophy mounts use State as their persistent exhibit slot. The room
		// normally runs in GameMode::Normal, so CheckAfraid still executes;
		// allowing a mount through perception can rewrite slot 0 to AI state 2
		// and disconnect the first plaque from TrophyRoom2.Body[0].
		if (cptr->StateF == 0xFF) continue;
		if (!cptr->Health) continue;
		if (!AIInfo[cptr->Clone].sniffer) continue;
		//if (cptr->AfraidTime || cptr->State == 1) continue;

		// Preserve the T-Rex's established pursuit lock, but keep checking while
		// it follows a fixed shot or hit position. Actual sight or scent can then
		// upgrade that positional reaction to continuous hunter tracking.
		if (cptr->Clone == AI_TREX
			&& ShouldSkipTRexPerception(cptr->AfraidTime != 0, cptr->State == 1,
				cptr->hunterAwareness)) continue;

		if (g_GameMode == GameMode::SurvivalMode) goto isAfraid;

		rlook = SubVectors(ppos, cptr->pos);
		kR = VectorLength(rlook) / 256.f
			/ (32.f + GameplayViewRadiusCells(ctViewR) / 2.f);
		NormVector(rlook, 1.0f);

		kR *= 2.5f / static_cast<float>((1.5 + OptSens / 128.f));
		if (kR > 3.0f) continue;

		clook.x = cptr->lookx;
		clook.y = 0;
		clook.z = cptr->lookz;

		MulVectorsScal(wlook, rlook, kwind);
		kwind *= Wind.speed / 10;
		MulVectorsScal(clook, rlook, klook);
		klook *= -1.f;

		if (HeadY > 180) kstand = 0.7f;
		else kstand = 1.2f;

		//============= reasons ==============//

		float kALook = kR * ((klook + 3.f) / 3.f) * kstand * kmask;
		if (klook > 0.3) kALook *= 2.0;
		if (klook > 0.8) kALook *= 2.0;
		kALook /= DinoInfo[cptr->CType].LookK;
		if (kALook < 1.0)
			if (TraceLook(cptr->pos.x, cptr->pos.y + 220, cptr->pos.z,
				PlayerX, PlayerY + HeadY / 2.f, PlayerZ)) kALook *= 1.3f;

		if (kALook < 1.0)
			if (TraceLook(cptr->pos.x, cptr->pos.y + 220, cptr->pos.z,
				PlayerX, PlayerY + HeadY, PlayerZ))   kALook = 2.0;
		kALook *= (1.f + static_cast<float>(ObjectsOnLook) / 6.f);

		/*
		  if (kR<1.0f) {
			  char t[32];
		   sprintf_s(t, sizeof(t),"%d", ObjectsOnLook);
		   AddMessage(t);
		   kALook = 20.f;
		  }
		  */

		float kASmell = kR * ((kwind + 2.0f) / 2.0F) * ((klook + 3.f) / 3.f) * kscent;
		if (kwind > 0) kASmell *= 2.0;
		kASmell /= DinoInfo[cptr->CType].SmellK;

		float kRes = MIN(kALook, kASmell);

		if (kRes < 1.0)
		{

			isAfraid:

			//MessageBeep(0xFFFFFFFF);
			char t[128];
			if (kALook < kASmell)
				sprintf(t, "LOOK: KR: %f  Tr: %d  K: %f", kR, ObjectsOnLook, kALook);
			else
				sprintf(t, "SMELL: KR: %f  Tr: %d  K: %f", kR, ObjectsOnLook, kASmell);
			//AddMessage(t);

			//MESSAGE REMOVED

			kRes = MIN(kRes, kR);
			cptr->AfraidTime = static_cast<int>((1.0 / (kRes + 0.1) * 10.f * 1000.f));
			if (cptr->State==0) {
				cptr->State = 2;
			}
			cptr->awareHunter = true;
			cptr->hunterAwareness = HunterAwarenessState::TrackingHunter;
			if (cptr->Clone == AI_TREX) //===== T-Rex
				if (kALook > kASmell) cptr->State = 3;
			cptr->NoFindCnt = 0;
		}
	}
}