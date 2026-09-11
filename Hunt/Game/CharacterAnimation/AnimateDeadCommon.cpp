// AnimateDeadCommon.cpp -- split from CharacterAnimation.cpp
// ==========================================================================
// One-time split from CharacterAnimation.cpp; no generator -- edit this file.
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

void AnimateDeadCommon(TCharacter *cptr)
{
	bool stge = cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].die;
	if (!DinoInfo[cptr->CType].deathType[cptr->deathType].nosleep) stge = stge && cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].sleep;

	if (stge)
	{
		if (cptr->PPMorphTime > 128)
		{
			cptr->PrevPhase = cptr->Phase;
			cptr->PrevPFTime = cptr->FTime;
			cptr->PPMorphTime = 0;
		}

		cptr->FTime = 0;
		cptr->Phase = DinoInfo[cptr->CType].deathType[cptr->deathType].die;
		ActivateCharacterFx(cptr);
	}
	else
	{
		ProcessPrevPhase(cptr);

		cptr->FTime += TimeDt;
		if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
			if (Tranq && !DinoInfo[cptr->CType].deathType[cptr->deathType].nosleep)
			{
				cptr->FTime = 0;
				cptr->Phase = DinoInfo[cptr->CType].deathType[cptr->deathType].sleep;
				ActivateCharacterFx(cptr);
			}
			else
				cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;
	}

	//======= movement ===========//
	DeltaFunc(cptr->vspeed, 0, TimeDt / 800.f);
	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

	if (cptr->Clone == AI_TREX)ThinkY_Beta_Gamma(cptr, 200, 196, 0.6f, 0.5f);
	else ThinkY_Beta_Gamma(cptr, 100, 96, 0.6f, 0.5f);

	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1600.f);
}
