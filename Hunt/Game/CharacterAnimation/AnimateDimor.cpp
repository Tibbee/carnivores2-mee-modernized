// AnimateDimor.cpp -- split from CharacterAnimation.cpp
// ==========================================================================
// One-time split from CharacterAnimation.cpp; no generator -- edit this file.
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

void AnimateDimor(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;


TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdist = static_cast<float>(sqrt(targetdx * targetdx + targetdz * targetdz));

	float playerdx = PlayerX - cptr->pos.x;
	float playerdz = PlayerZ - cptr->pos.z;
	float pdist = static_cast<float>(sqrt(playerdx * playerdx + playerdz * playerdz));


	//=========== run away =================//

	// Step 4: Extend culling distance by 4 units (~1024 world units)
	// to allow smoothstep fade-out to complete
	if (pdist > (charViewR + 20 + 4) * 256)
		if (ReplaceCharacterForward(cptr)) goto TBEGIN;


	//======== exploring area ===============//




	if (cptr->packId >= 0) {
		float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
		float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
		float leaderdist = static_cast<float>(sqrt(leaderdx * leaderdx + leaderdz * leaderdz));

		if (cptr->followLeader) {
			if (leaderdist < cptr->packDensity * 128 * 0.6)
			{
				cptr->followLeader = false;
				SetNewTargetPlace(cptr, 4048.f);
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
	else if (tdist < 1024)
	{
		SetNewTargetPlace(cptr, 4048.f);
		goto TBEGIN;
	}


	//============================================//


	cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);
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
		if (cptr->Phase == DinoInfo[cptr->CType].flyAnim)
			if (cptr->pos.y > GetLandH(cptr->pos.x, cptr->pos.z) + DinoInfo[cptr->CType].maxDepth)
				cptr->Phase = DinoInfo[cptr->CType].glideAnim;
			else;
		else if (cptr->Phase == DinoInfo[cptr->CType].glideAnim)
			if (cptr->pos.y < GetLandH(cptr->pos.x, cptr->pos.z) + DinoInfo[cptr->CType].minDepth)
				cptr->Phase = DinoInfo[cptr->CType].flyAnim;
	}




	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
		if ((rand() & 1023) > 980)
			ActivateCharacterFx(cptr);

	if (_Phase != cptr->Phase)
	{
		if (!NewPhase) cptr->FTime = 0;
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


	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.6f + drspd * 1.2f;
		else currspeed = -0.6f - drspd * 1.2f;
	else currspeed = 0;

	if (dalpha > pi) currspeed *= -1;
	DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 460.f);

	tgbend = drspd / 2.f;
	if (tgbend > pi / 2) tgbend = pi / 2;

	tgbend *= SGN(currspeed);
	if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
	else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 400.f);


	rspd = cptr->rspeed * TimeDt / 1024.f;
	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim) curspeed = DinoInfo[cptr->CType].flyspd;
	if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) curspeed = DinoInfo[cptr->CType].gldspd;

	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim)
		DeltaFunc(cptr->pos.y, GetLandH(cptr->pos.x, cptr->pos.z) + (DinoInfo[cptr->CType].maxDepth * 1.5), TimeDt / 6.f);
	else
		DeltaFunc(cptr->pos.y, GetLandH(cptr->pos.x, cptr->pos.z), TimeDt / 16.f);


	if (cptr->pos.y < GetLandH(cptr->pos.x, cptr->pos.z) + 236)
		cptr->pos.y = GetLandH(cptr->pos.x, cptr->pos.z) + 256;



	//========== process speed =============//
	curspeed *= cptr->scale;
	DeltaFunc(cptr->vspeed, curspeed, TimeDt / 2024.f);

	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

	cptr->tggamma = cptr->rspeed / 4.0f;
	if (cptr->tggamma > pi / 6.f) cptr->tggamma = pi / 6.f;
	if (cptr->tggamma < -pi / 6.f) cptr->tggamma = -pi / 6.f;
	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
}
