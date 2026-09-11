// AnimateHuntdog.cpp — split from CharacterAnimation.cpp
// ==========================================================================
// One-time split from CharacterAnimation.cpp; no generator â€” edit this file.
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

void AnimateHuntdog(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;
//	if (cptr->AfraidTime) {
//		cptr->AfraidTime = MAX(0, cptr->AfraidTime - TimeDt);
//	}

TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdistSq = targetdx * targetdx + targetdz * targetdz;

	float playerdx = PlayerX - cptr->pos.x;
	float playerdz = PlayerZ - cptr->pos.z;

	float pdistSq = playerdx * playerdx + playerdz * playerdz;


	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > DinoInfo[cptr->CType].waterLevel * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto NOTHINK;


	//======== exploring area ===============//

	if (cptr->State == 1){
		cptr->tgx = PlayerX;
		cptr->tgz = PlayerZ;
		cptr->tgtime = 0;

		if (pdistSq < (3 * 256) * (3 * 256)) {
			cptr->State = 0;
			SetNewTargetPlace(cptr, 8048.f);
		}

	}

	if (cptr->State == 0)
	{

		if (huntDogSearch(cptr)) {
			cptr->State = 2;
		} else {

			if (tdistSq < 456 * 456)
			{
				SetNewTargetPlace(cptr, 8048.f);
				goto TBEGIN;
			}

			if (pdistSq > (4 * 256) * (4 * 256)) {
				cptr->tgx = PlayerX;
				cptr->tgz = PlayerZ;
				cptr->tgtime = 0;
			}

			if (pdistSq > (8 * 256) * (8 * 256)) {
				cptr->State = 1;
			}

		}

	}

	if (cptr->State == 2) {

		if (pdistSq > (17 * 256) * (17 * 256)) {
			cptr->State = 1;
		}

		if (tdistSq < 456 * 456 || !Characters[cptr->dogPrey].Health)
		{
			if (!huntDogSearch(cptr)) {
				cptr->State = 0;
				SetNewTargetPlace(cptr, 8048.f);
				goto TBEGIN;
			}
		}

	}



	//============================================//


NOTHINK:

	if (cptr->NoFindCnt) cptr->NoFindCnt--;
	else
	{
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);
		if (cptr->AfraidTime)
		{
			cptr->tgalpha += static_cast<float>(sin(RealTime / 1024.f)) / 3.f;
			if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
			if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
		}
	}


	LookForAWay(cptr, !DinoInfo[cptr->CType].canSwim, true);
	if (cptr->NoWayCnt > AIInfo[cptr->Clone].noWayCntMin)
	{
		cptr->NoWayCnt = 0;
		cptr->NoFindCnt = AIInfo[cptr->Clone].noFindWayMed + rRand(AIInfo[cptr->Clone].noFindWayRange);
	}

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

	//even if not newphase...
	if (cptr->Phase == 2 && _Phase != 2) {

		if (DinoInfo[cptr->CType].smellCount > 0) {
			cptr->Phase = DinoInfo[cptr->CType].smellAnim[rRand(DinoInfo[cptr->CType].smellCount - 1)];
		}

		//cptr->Phase == cptr->roarAnim;
		cptr->FTime = 0;
		goto ENDPSELECT;
	}

	if (NewPhase){

		if (cptr->State == 1) {
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
		
		} else if (cptr->State == 0){
			if (DinoInfo[cptr->CType].lookCount) {

				if (rRand(AIInfo[cptr->Clone].idleStartD) > 110) {
					cptr->Phase = DinoInfo[cptr->CType].lookAnim[rRand(DinoInfo[cptr->CType].lookCount - 1)];
					goto ENDPSELECT;
				}
				else {
					cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				}

			}
			else {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
			}

		} else if (cptr->State == 2) {

			if (rRand(AIInfo[cptr->Clone].idleStartD) > 120) {

				if (DinoInfo[cptr->CType].smellCount > 0) {
					cptr->Phase = DinoInfo[cptr->CType].smellAnim[rRand(DinoInfo[cptr->CType].smellCount - 1)];
				}

				//cptr->Phase = cptr->roarAnim;
				goto ENDPSELECT;
			}
			else {
				cptr->Phase = DinoInfo[cptr->CType].runAnim;
			}

		}

	}

	

ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
		ActivateCharacterFx(cptr);

	if (_Phase != cptr->Phase)
	{
		if ((_Phase == DinoInfo[cptr->CType].runAnim ||
			_Phase == DinoInfo[cptr->CType].walkAnim) &&
			(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
				cptr->Phase == DinoInfo[cptr->CType].walkAnim))
			cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[_Phase].AniTime + 64;
		else if (!NewPhase) cptr->FTime = 0;

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

	//if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;

	//if (cptr->Phase == cptr->roarAnim) goto SKIPROT;

	for (int i = 0; i < DinoInfo[cptr->CType].smellCount; i++) {
		if (cptr->Phase == DinoInfo[cptr->CType].smellAnim[i]) goto SKIPROT;
	}

	for (int i = 0; i < DinoInfo[cptr->CType].lookCount; i++) {
		if (cptr->Phase == DinoInfo[cptr->CType].lookAnim[i]) goto SKIPROT;
	}

	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.2f + drspd * 1.0f;
		else currspeed = -0.2f - drspd * 1.0f;
	else currspeed = 0;

	//only if fleeing?
	//if (cptr->AfraidTime) currspeed *= 1.5;
	if (dalpha > pi) currspeed *= -1;
	if ((cptr->State & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim) currspeed /= 1.4f;

	DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 400.f);

	tgbend = drspd / AIInfo[cptr->Clone].targetBendRotSpd;
	if (tgbend > pi / AIInfo[cptr->Clone].targetBendMin) tgbend = pi / AIInfo[cptr->Clone].targetBendMin;

	tgbend *= SGN(currspeed);
	if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / AIInfo[cptr->Clone].targetBendDelta1);
	else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / AIInfo[cptr->Clone].targetBendDelta2);


	rspd = cptr->rspeed * TimeDt / 612.f;
	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

SKIPROT:

	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;

	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) curspeed = DinoInfo[cptr->CType].swmspd;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) curspeed = 0.0f;


	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	//========== process speed =============//
	curspeed *= cptr->scale;
	if (curspeed > cptr->vspeed) DeltaFunc(cptr->vspeed, curspeed, TimeDt / 1024.f);
	else DeltaFunc(cptr->vspeed, curspeed, TimeDt / 256.f);

	MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt,
		cptr->lookz * cptr->vspeed * TimeDt, !DinoInfo[cptr->CType].canSwim, true);


	//============ Y movement =================//
	if (cptr->StateF & csONWATER && DinoInfo[cptr->CType].canSwim)
	{
		cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - (DinoInfo[cptr->CType].waterLevel + 20) * cptr->scale;
		cptr->beta /= 2;
		cptr->tggamma = 0;
	}
	else {
		ThinkY_Beta_Gamma(cptr, 128, 64, 0.6f, AIInfo[cptr->Clone].yBetaGamma4);
	}

	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].walkTargetGammaRot;
	else cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].targetGammaRot;
	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
}
