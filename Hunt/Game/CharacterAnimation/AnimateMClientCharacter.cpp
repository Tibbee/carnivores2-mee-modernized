// AnimateMClientCharacter.cpp — split from CharacterAnimation.cpp
// ==========================================================================
// One-time split from CharacterAnimation.cpp; no generator â€” edit this file.
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

void AnimateMClientCharacter(TCharacter *cptr)
{
	NewPhase = false;
	int _FTime = cptr->FTime;

	if (cptr->_PhaseM != cptr->Phase) cptr->FTime = 0;

	ProcessPrevPhase(cptr);

	//======== select new phase =======================//
	cptr->FTime += TimeDt;

	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].die) cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;
		else {
			cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;
			NewPhase = true;
		}
	}

	//====== process phase changing ===========//
	if ((cptr->_PhaseM != cptr->Phase) || NewPhase) {
		if (cptr->Clone == AI_DIMOR || cptr->Clone == AI_PTERA) {
			if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim) {
				if ((rand() & 1023) > 980) ActivateCharacterFx(cptr);
			} else if (!NewPhase) ActivateCharacterFx(cptr);
		} else ActivateCharacterFx(cptr);
	}

	if (cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].die) {

		if (cptr->_PhaseM != cptr->Phase)
		{
			if ((cptr->_PhaseM == DinoInfo[cptr->CType].runAnim ||
				cptr->_PhaseM == DinoInfo[cptr->CType].walkAnim) &&
				(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
					cptr->Phase == DinoInfo[cptr->CType].walkAnim))
				cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[cptr->_PhaseM].AniTime + 64;
			else if (!NewPhase) cptr->FTime = 0;

			if (cptr->PPMorphTime > 128)
			{
				cptr->PrevPhase = cptr->_PhaseM;
				cptr->PrevPFTime = _FTime;
				cptr->PPMorphTime = 0;
			}
		}

		cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;

	}

	//========== rotation to tgalpha ===================//

	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	//============ Y movement =================//
	if (cptr->Clone != AI_DIMOR && cptr->Clone != AI_PTERA) {
		if (cptr->StateF & csONWATER && DinoInfo[cptr->CType].canSwim)
		{
			cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - (DinoInfo[cptr->CType].waterLevel + 20) * cptr->scale;
			cptr->beta /= 2;
			cptr->tggamma = 0;
		}
		else {
			ThinkY_Beta_Gamma(cptr, 64, 32, 0.7f, 0.4f);
		}

		//if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / 12.0f;
		//else cptr->tggamma += cptr->rspeed / 8.0f;
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
	} else cptr->gamma = cptr->bend;

	cptr->_PhaseM = cptr->Phase;
	if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].die) cptr->Health = 0;
	if (cptr->Clone == AI_DIMOR || cptr->Clone == AI_PTERA)
		if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].fall) cptr->Health = 0;
	if (DinoInfo[cptr->CType].waterDieCount > 0)
		if (cptr->Phase == cptr->waterDieAnim) cptr->Health = 0;
}
