// AnimateDeadFish.cpp -- split from CharacterAnimation.cpp
// ==========================================================================
// One-time split from CharacterAnimation.cpp; no generator -- edit this file.
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

void AnimateDeadFish(TCharacter *cptr)
{

	if (cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].fall && cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].die)
	{
		cptr->deathPhase = cptr->Phase;
		if (cptr->PPMorphTime > 128)
		{
			cptr->PrevPhase = cptr->Phase;
			cptr->PrevPFTime = cptr->FTime;
			cptr->PPMorphTime = 0;
		}

		cptr->FTime = 0;
		cptr->Phase = DinoInfo[cptr->CType].deathType[cptr->deathType].fall;
		cptr->rspeed = 0;
		ActivateCharacterFxAquatic(cptr);
		return;
	}

	ProcessPrevPhase(cptr);

	float lh = GetLandH(cptr->pos.x, cptr->pos.z);

	cptr->FTime += TimeDt;
	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime) cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;


	//======= movement ===========//
	if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].die)
		DeltaFunc(cptr->vspeed, 0, TimeDt / 400.f);
	else {
		DeltaFunc(cptr->vspeed, 0, TimeDt / 1200.f);
		DeltaFunc(cptr->beta, 0, TimeDt / 1200.f);
		DeltaFunc(cptr->gamma, 0, TimeDt / 1200.f);
		DeltaFunc(cptr->bend, 0, TimeDt / 1200.f);
	}


	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

	if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].fall)
	{
		cptr->pos.y += cptr->rspeed * TimeDt / 30000;
		cptr->rspeed -= TimeDt * 2.56;

		if (cptr->pos.y <= lh)
		{
			cptr->pos.y = lh;

			//if (cptr->PPMorphTime > 128)
			//{
			//	cptr->PrevPhase = cptr->Phase;
			//	cptr->PrevPFTime = cptr->FTime;
			//	cptr->PPMorphTime = 0;
			//}
			
			cptr->Phase = DinoInfo[cptr->CType].deathType[cptr->deathType].die;
			cptr->FTime = 0;
			ActivateCharacterFxAquatic(cptr);
		}
	}
	else
	{
		ThinkY_Beta_Gamma(cptr, 140, 126, 0.6f, 0.5f);
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1600.f);
	}

}
