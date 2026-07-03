// AnimateBrahiOld.cpp — auto-extracted from CharacterAnimation.cpp
// ==========================================================================
// Auto-generated from CharacterAnimation.cpp
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

void AnimateBrahiOld(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;

TBEGIN:
	cptr->tgtime = 0;
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdist = static_cast<float>(sqrt(targetdx * targetdx + targetdz * targetdz));

	float playerdx = PlayerX - cptr->pos.x - cptr->lookx * 108;
	float playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 108;
	float pdist = static_cast<float>(sqrt(playerdx * playerdx + playerdz * playerdz));
	if (pdist > (charViewR + 20) * 256)
		if (ReplaceCharacterForward(cptr)) goto TBEGIN;

	if (cptr->packId >= 0) {
		float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
		float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
		float leaderdist = static_cast<float>(sqrt(leaderdx * leaderdx + leaderdz * leaderdz));

		if (cptr->followLeader) {
			if (leaderdist < cptr->packDensity * 128 * 0.6)
			{
				cptr->followLeader = false;
				SetNewTargetPlace_Brahi(cptr, 2048.f);
				goto TBEGIN;
			}
		}
		else {
			if (leaderdist > cptr->packDensity * 128 * 1.3)
			{
				cptr->followLeader = true;
			}
		}

	}

	if (cptr->followLeader) {
		cptr->tgx = Packs[cptr->packId].leader->pos.x;
		cptr->tgz = Packs[cptr->packId].leader->pos.z;
	}
	else if (tdist < 256)
	{
		SetNewTargetPlace_Brahi(cptr, 2048.f);
		goto TBEGIN;
	}

	//============================================//
	cptr->tgalpha = FindVectorAlpha(targetdx, targetdz);

	//============================================//

	if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
	if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;

	//===============================================//

	ProcessPrevPhase(cptr);

	//======== select new phase =======================//
	cptr->FTime += TimeDt;

	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;
		NewPhase = true;
	}

	if (NewPhase)
	{
		
		if (DinoInfo[cptr->CType].idleGroupCount
			&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)) {


			if (cptr->currentIdleGroup >= 0) {
				if (rRand(127) + 1 > (1 - DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].end) * 128
					&& (DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].endOnAny
						|| cptr->Phase == DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].anim[DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].count - 1])) {
					cptr->Phase = DinoInfo[cptr->CType].walkAnim;
					if (DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].instantRepeat) {
						cptr->currentIdleGroup = -1; //this must be done inside the if statement
					}
					else {
						cptr->currentIdleGroup = -1; //this must be done inside the if statement
						goto ENDPSELECT;
					}
				}
				else {
					cptr->Phase = DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].anim[rRand(DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].count - 1)];
					goto ENDPSELECT;
				}
			}

			for (int idleGroupNo = 0; idleGroupNo < DinoInfo[cptr->CType].idleGroupCount; idleGroupNo++) {
				if (rRand(127) + 1 > (1 - DinoInfo[cptr->CType].idleGroup[idleGroupNo].start) * 128) cptr->currentIdleGroup = idleGroupNo;
			}
			if (cptr->currentIdleGroup >= 0) {
				if (DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].startOnAny)
					cptr->Phase = DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].anim[rRand(DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].count - 1)];
				else
					cptr->Phase = DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].anim[0];
				goto ENDPSELECT;
			}
			else cptr->Phase = DinoInfo[cptr->CType].walkAnim;


		}
		else cptr->Phase = DinoInfo[cptr->CType].walkAnim;
	}


ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
		ActivateCharacterFx(cptr);

	if (_Phase != cptr->Phase)
	{

		if (cptr->PPMorphTime > 128)
		{
			cptr->PrevPhase = _Phase;
			cptr->PrevPFTime = _FTime;
			cptr->PPMorphTime = 0;
		}
	}

	cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;



	//========== rotation to tgalpha ===================//

	float rspd, currspeed, tgbend;
	float dalpha = static_cast<float>(fabs(cptr->tgalpha - cptr->alpha));
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;

	if (cptr->currentIdleGroup >= 0) goto SKIPROT;

	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.2f + drspd * 0.2f;
		else currspeed = -0.2f - drspd * 0.2f;
	else currspeed = 0;

	if (dalpha > pi) currspeed *= -1;

	DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 600.f);

	tgbend = drspd / 4;
	if (tgbend > pi / 4) tgbend = pi / 4;

	tgbend *= SGN(currspeed);
	DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 3200.f);

	rspd = cptr->rspeed * TimeDt / 1024.f;
	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;

	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

SKIPROT:

	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;

	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	//========== process speed =============//
	curspeed *= cptr->scale;
	DeltaFunc(cptr->vspeed, curspeed, TimeDt / 1024.f);
	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;
	

	ThinkY_Beta_Gamma(cptr, 256, 128, 0.1f, 0.2f);
	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 4048.f);
}
