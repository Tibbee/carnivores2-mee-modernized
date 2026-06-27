// ==========================================================================
// CharacterAnimation.cpp — Character animation dispatch and per-dino animators
// ==========================================================================
// Extracted from Characters.cpp.

#include "Hunt.h"
#include "Game/CharacterInternal.h"

void AnimateHuntDead(TCharacter *cptr)
{

	//if (!cptr->FTime) ActivateCharacterFx(cptr);

	ProcessPrevPhase(cptr);
	BOOL NewPhase = false;
	bool loopDone = false;

	if (killerDino) {
		if (DinoInfo[killerDino->CType].killType[killerDino->killType].carryCorpse &&
			DinoInfo[killerDino->CType].Aquatic) {
			cptr->bend = killerDino->bend;
			cptr->bdepth = killerDino->bdepth;
		}
	}



	cptr->FTime += TimeDt;
	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		if (killerDino) {
			if (DinoInfo[killerDino->CType].killType[killerDino->killType].dontloop &&
				cptr->Phase == DinoInfo[killerDino->CType].killType[killerDino->killType].hunteranim) {
				loopDone = true;
			}
		}

		NewPhase = true;
		if (cptr->Phase == 2)
			cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;
		else
			cptr->FTime = 0;

		if (cptr->Phase == 1)
		{
			cptr->FTime = 0;
			cptr->Phase = 2;
		}

		ActivateCharacterFx(cptr);
	}

	bool notAq = false;
	if (!killerDino) {
		notAq = true;
	} else if (!DinoInfo[killerDino->CType].Aquatic) notAq = true;

	if (notAq) {
		float h = GetLandH(cptr->pos.x, cptr->pos.z);
		DeltaFunc(cptr->pos.y, h, TimeDt / 5.f);

		if (cptr->Phase == 2)
			if (cptr->pos.y > h + 3)
			{
				cptr->FTime = 0;
				//MessageBeep(0xFFFFFFFF);
			}


		if (cptr->pos.y < h + 256)
		{
			//=== beta ===//
			float blook = 256;
			float hlook = GetLandH(cptr->pos.x + cptr->lookx * blook, cptr->pos.z + cptr->lookz * blook);
			float hlook2 = GetLandH(cptr->pos.x - cptr->lookx * blook, cptr->pos.z - cptr->lookz * blook);
			DeltaFunc(cptr->beta, (hlook2 - hlook) / (blook * 3.2f), TimeDt / 1800.f);

			if (cptr->beta > 0.4f) cptr->beta = 0.4f;
			if (cptr->beta < -0.4f) cptr->beta = -0.4f;

			//=== gamma ===//
			float glook = 256;
			hlook = GetLandH(cptr->pos.x + cptr->lookz * glook, cptr->pos.z - cptr->lookx*glook);
			hlook2 = GetLandH(cptr->pos.x - cptr->lookz * glook, cptr->pos.z + cptr->lookx*glook);
			cptr->tggamma = (hlook - hlook2) / (glook * 3.2f);
			if (cptr->tggamma > 0.4f) cptr->tggamma = 0.4f;
			if (cptr->tggamma < -0.4f) cptr->tggamma = -0.4f;
			DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1800.f);
		}
	}
	

	if (killerDino) {

		//	if (!(GetLandUpH(killerDino->pos.x, killerDino->pos.z) - GetLandH(killerDino->pos.x, killerDino->pos.z) >
		//		DinoInfo[killerDino->CType].waterLevel * killerDino->scale))
		if (!killedwater || DinoInfo[killerDino->CType].Aquatic) //make exception for aquatic dangerous creatures duh
		{
			if (DinoInfo[killerDino->CType].killTypeCount) {
				if ((DinoInfo[killerDino->CType].killType[killerDino->killType].elevate &&
					killerDino->Phase == DinoInfo[killerDino->CType].killType[killerDino->killType].anim)
					|| DinoInfo[killerDino->CType].killType[killerDino->killType].carryCorpse
					) {

					cptr->pos = killerDino->pos;
					cptr->FTime = killerDino->FTime;
					cptr->beta = killerDino->beta;
					cptr->gamma = killerDino->gamma;
					cptr->scale = killerDino->scale;
					cptr->alpha = killerDino->alpha;
					killerDino->bend = 0;

				}

			}

		}

		if (loopDone) {
			if (DinoInfo[killerDino->CType].killType[killerDino->killType].carryCorpse
				&& (!killedwater || DinoInfo[killerDino->CType].Aquatic)) {
				cptr->Phase = DinoInfo[killerDino->CType].killType[killerDino->killType].hunterswimanim;
			}
			else {
				cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;
			}
		}

	}

}



/*
void AnimateTRexDead(TCharacter *cptr)
{

	if (cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].die)
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
			cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;
	}

	//======= movement ===========//
	DeltaFunc(cptr->vspeed, 0, TimeDt / 800.f);
	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

	ThinkY_Beta_Gamma(cptr, 200, 196, 0.6f, 0.5f);
	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1600.f);
}
*/

/*
void AnimateDimorDead(TCharacter *cptr)
{

	if (cptr->Phase != DinoInfo[cptr->CType].fallAnim && cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].die)
	{
		if (cptr->PPMorphTime > 128)
		{
			cptr->PrevPhase = cptr->Phase;
			cptr->PrevPFTime = cptr->FTime;
			cptr->PPMorphTime = 0;
		}

		cptr->FTime = 0;
		cptr->Phase = DinoInfo[cptr->CType].fallAnim;
		cptr->rspeed = 0;
		ActivateCharacterFx(cptr);
		return;
	}

	ProcessPrevPhase(cptr);

	cptr->FTime += TimeDt;
	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
		if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].die)
			cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;
		else
			cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;


	//======= movement ===========//
	if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].die)
		DeltaFunc(cptr->vspeed, 0, TimeDt / 400.f);
	else
		DeltaFunc(cptr->vspeed, 0, TimeDt / 1200.f);

	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

	if (cptr->Phase == DinoInfo[cptr->CType].fallAnim)
	{
		float wh = GetLandUpH(cptr->pos.x, cptr->pos.z);
		float lh = GetLandH(cptr->pos.x, cptr->pos.z);
		BOOL OnWater = (wh > lh);
		if (OnWater)
			if (cptr->pos.y >= wh && (cptr->pos.y + cptr->rspeed * TimeDt / 1024) < wh)
			{
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 2.0);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 2.5);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 3.0);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 3.5);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 3.0);
			}
		cptr->pos.y += cptr->rspeed * TimeDt / 1024;
		cptr->rspeed -= TimeDt * 2.56;

		if (cptr->pos.y < lh)
		{
			cptr->pos.y = lh;
			if (OnWater)
			{
				AddElements(cptr->pos.x + siRand(128), lh, cptr->pos.z + siRand(128), 4, 10);
				AddElements(cptr->pos.x + siRand(128), lh, cptr->pos.z + siRand(128), 4, 10);
				AddElements(cptr->pos.x + siRand(128), lh, cptr->pos.z + siRand(128), 4, 10);
			}

			if (cptr->PPMorphTime > 128)
			{
				cptr->PrevPhase = cptr->Phase;
				cptr->PrevPFTime = cptr->FTime;
				cptr->PPMorphTime = 0;
			}

			cptr->Phase = DinoInfo[cptr->CType].deathType[cptr->deathType].die;
			cptr->FTime = 0;
			ActivateCharacterFx(cptr);
		}
	}
	else
	{
		ThinkY_Beta_Gamma(cptr, 140, 126, 0.6f, 0.5f);
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1600.f);
	}
}
*/



//universal animate dead proc
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





void AnimateTitan(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;

	bool alertInit = false;
	if (cptr->State == 2) alertInit = true;
	if (cptr->packId >= 0) {
		if (!cptr->State && Packs[cptr->packId]._alert) alertInit = true;
	}



	if (alertInit)
	{
		cptr->State = 1;
		if (cptr->gliding) cptr->Phase = DinoInfo[cptr->CType].flyAnim;
		else cptr->Phase = DinoInfo[cptr->CType].runAnim;
	}


TBEGIN:
	const float landH = GetLandH(cptr->pos.x, cptr->pos.z);
	const float landUpH = GetLandUpH(cptr->pos.x, cptr->pos.z);
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdist = static_cast<float>(sqrt(targetdx * targetdx + targetdz * targetdz));

	float playerdx, playerdz;
	playerdx = PlayerX - cptr->pos.x - cptr->lookx * 108;
	playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 108;
	float pdist = static_cast<float>(sqrt(playerdx * playerdx + playerdz * playerdz));




	if (landUpH - landH > DinoInfo[cptr->CType].waterLevel * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto NOTHINK;

	//============================================//			// (run away)
	if (!MyHealth) cptr->State = 0;
	bool fleeMode = false;
	if (cptr->State)
	{

		cptr->currentIdleGroup = -1;

		float aDist;
		aDist = ctViewR * DinoInfo[cptr->CType].aggress + OptAgres / AIInfo[cptr->Clone].agressMulti;
		if (cptr->gliding) aDist *= 2;

		if (g_GameMode != GameMode::SurvivalMode) {
			if (pdist > aDist || ((PlayerY - cptr->pos.y > pdist) && cptr->gliding) ||
				DinoInfo[cptr->CType].aggress <= 0 || !cptr->awareHunter) {
				fleeMode = true;
			}
			else if (DinoInfo[cptr->CType].defensive && cptr->Health == DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearShot && cptr->Health < DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearHearShot && cptr->heardShot) fleeMode = true;
			else if (cptr->packId >= 0) Packs[cptr->packId].attack = true;
		}

		if (cptr->packId >= 0) {
			if (Packs[cptr->packId]._attack) fleeMode = false;
		}

		if (fleeMode) {
			nv.x = playerdx;
			nv.z = playerdz;
			nv.y = 0;
			NormVector(nv, 2048.f);
			cptr->tgx = cptr->pos.x - nv.x;
			cptr->tgz = cptr->pos.z - nv.z;
			cptr->tgtime = 0;
			cptr->AfraidTime -= TimeDt;

			if (cptr->packId >= 0) {
				if (cptr->AfraidTime <= 0)
				{
					if (!Packs[cptr->packId]._alert) {
						cptr->AfraidTime = 0;
						cptr->State = 0;
					}
				}
				else Packs[cptr->packId].alert = true;
			}
			else if (cptr->AfraidTime <= 0) {
				cptr->AfraidTime = 0;
				cptr->State = 0;
			}

		}
		else
		{
			cptr->tgx = PlayerX;
			cptr->tgz = PlayerZ;
			cptr->tgtime = 0;
			if (cptr->packId >= 0) {
				Packs[cptr->packId].alert = true;
			}
		}

		if (pdist < DinoInfo[cptr->CType].killDist && DinoInfo[cptr->CType].killDist > 0) {
			int killAlt = DinoInfo[cptr->CType].waterLevel;
			if (killAlt < 256) killAlt = 256;
			if (fabs(PlayerY - cptr->pos.y) < killAlt + 20)
			{

				if (DinoInfo[cptr->CType].killTypeCount > 0) {

					if (!(cptr->StateF & csONWATER))
					{
						cptr->vspeed /= 8.0f;
						cptr->State = 1;
						cptr->Phase = DinoInfo[cptr->CType].killType[cptr->killType].anim;
						if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) cptr->FTime = 0;
						AddDeadBody(cptr,
							DinoInfo[cptr->CType].killType[cptr->killType].hunteranim,
							DinoInfo[cptr->CType].killType[cptr->killType].scream);
					}
					else AddDeadBody(cptr, HUNT_EAT, true);

				}
				else {
					AddDeadBody(cptr, HUNT_EAT, true);
					cptr->State = 0;
				}

			}
		}


	}

	if (!cptr->State)
	{
		cptr->AfraidTime = 0;

		if (cptr->packId >= 0) {
			float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
			float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
			float leaderdist = static_cast<float>(sqrt(leaderdx * leaderdx + leaderdz * leaderdz));

			if (cptr->followLeader) {
				if (leaderdist < cptr->packDensity * 128 * 0.6)
				{
					cptr->followLeader = false;
					SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
					goto TBEGIN;
				}
			} else {
				if (leaderdist > cptr->packDensity * 128 * 1.3)
				{
					cptr->followLeader = true;
				}
			}

		}

		float tdst = 456;
		if (cptr->gliding) tdst = 1024;

		if (cptr->followLeader) {
			cptr->tgx = Packs[cptr->packId].leader->pos.x;
			cptr->tgz = Packs[cptr->packId].leader->pos.z;
		}
		else if (tdist < tdst) 
		{
			SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
			goto TBEGIN;
		}



	}

NOTHINK:
	if (pdist < AIInfo[cptr->Clone].pWMin && !cptr->gliding) cptr->NoFindCnt = 0;
	if (cptr->NoFindCnt && !cptr->gliding) cptr->NoFindCnt--;
	else
	{
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);

		if (cptr->State && pdist > DinoInfo[cptr->CType].weaveRange && !DinoInfo[cptr->CType].dontWeave)
		{
			float rTD;
			rTD = 824.f;

			cptr->tgalpha += static_cast<float>(sin(RealTime / rTD)) / AIInfo[cptr->Clone].tGAIncrement;
			if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
			if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
		}
	}

	if (!cptr->gliding) {

		LookForAWay(cptr, !DinoInfo[cptr->CType].canSwim, true);

		if (cptr->NoWayCnt > AIInfo[cptr->Clone].noWayCntMin)
		{
			cptr->NoWayCnt = 0;
			cptr->NoFindCnt = AIInfo[cptr->Clone].noFindWayMed + rRand(AIInfo[cptr->Clone].noFindWayRange);
		}
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

		if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) {
			if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				cptr->State = 0;
			}
		}


		NewPhase = true;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount)  goto ENDPSELECT;

	float FlDst = ctViewR * DinoInfo[cptr->CType].flyDist + OptAgres / AIInfo[cptr->Clone].agressMulti;
	if (!alertInit) FlDst *= 1.5;
	if (!cptr->gliding && cptr->State && pdist > FlDst) cptr->gliding = true;
    else if (cptr->pos.y < landUpH + 50
		&& cptr->Phase != DinoInfo[cptr->CType].takeoffAnim
		&& !(landUpH > landH)) {
		cptr->gliding = false;
	}

	if (NewPhase){

		if (!cptr->State && rRand(50) == 2) cptr->gliding = true;
		
		if (cptr->gliding) {

			if (!cptr->State || fleeMode) {
				//WANDER/FLEE
				if (!cptr->State && cptr->shakeTime) cptr->shakeTime -= 1;

				if (cptr->Phase == DinoInfo[cptr->CType].flyAnim) {
					if (cptr->pos.y > landUpH + 5800) {
						cptr->Phase = DinoInfo[cptr->CType].glideAnim;
					}
				}
				else if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) {
					
					if (!cptr->shakeTime) {
						if (cptr->pos.y < landUpH + 1200) {

							//lander
							if (landUpH > landH) cptr->Phase = DinoInfo[cptr->CType].flyAnim;
							else cptr->Phase = DinoInfo[cptr->CType].landAnim;
						}
					} else {
						if (cptr->pos.y < landUpH + 3800) {
							cptr->Phase = DinoInfo[cptr->CType].flyAnim;
						}
					}

				}
				else if (cptr->Phase == DinoInfo[cptr->CType].takeoffAnim) {
					if (cptr->pos.y > landUpH + 1024) {
						cptr->Phase = DinoInfo[cptr->CType].flyAnim;
					}
				}
				else if (cptr->Phase != DinoInfo[cptr->CType].landAnim){
					cptr->beta = 0;
					cptr->gamma = 0;
					//	//TITAN_SLIDE	cptr->Slide = 0;
					cptr->Phase = DinoInfo[cptr->CType].takeoffAnim;

					cptr->shakeTime = 25 + rRand(150);//lander
				}
				
			} else {

				cptr->shakeTime = 0;//lander

				if (cptr->Phase != DinoInfo[cptr->CType].takeoffAnim &&
					cptr->Phase != DinoInfo[cptr->CType].glideAnim &&
					cptr->Phase != DinoInfo[cptr->CType].flyAnim &&
					cptr->Phase != DinoInfo[cptr->CType].diveAnim) {
					cptr->beta = 0;
					cptr->gamma = 0;
					//	//TITAN_SLIDE	cptr->Slide = 0;
					cptr->Phase = DinoInfo[cptr->CType].takeoffAnim;
				} else {
					float dalph = cptr->alpha - cptr->tgalpha;
					if (dalph < 0) dalph *= -1;
					if (dalph > pi) dalph -= pi;
					if (dalph > pi/2) {
						if(cptr->pos.y - PlayerY < pdist / 2) cptr->Phase = DinoInfo[cptr->CType].takeoffAnim;
						else if (cptr->pos.y > PlayerY + 600) cptr->Phase = DinoInfo[cptr->CType].glideAnim;
						else cptr->Phase = DinoInfo[cptr->CType].flyAnim;
					} else {
						if (cptr->pos.y < PlayerY + 256 && pdist > 2048) cptr->Phase = DinoInfo[cptr->CType].takeoffAnim;
						else if (cptr->pos.y - PlayerY > pdist / (1.4 * DinoInfo[cptr->CType].divspd)) cptr->Phase = DinoInfo[cptr->CType].diveAnim;
						else if (cptr->pos.y > PlayerY + 600) cptr->Phase = DinoInfo[cptr->CType].glideAnim;
						else cptr->Phase = DinoInfo[cptr->CType].flyAnim;
					}

					/*
					if (256 + cptr->pos.y - PlayerY < pdist/2) {
						cptr->Phase = DinoInfo[cptr->CType].takeoffAnim;
					} else if (cptr->pos.y - PlayerY > pdist / 2) {
						cptr->Phase = DinoInfo[cptr->CType].diveAnim;
					} else if (cptr->pos.y > PlayerY + 600) {
						cptr->Phase = DinoInfo[cptr->CType].glideAnim;
					} else cptr->Phase = DinoInfo[cptr->CType].flyAnim;
					*/
				}
				
			}
			
		} else {

			cptr->shakeTime = 0;//lander

			if (!cptr->State) {

				if (DinoInfo[cptr->CType].idleGroupCount
					&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
					&& !(cptr->StateF & csONWATER)) {

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

				} else {
					cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				}

			} else {
				cptr->Phase = DinoInfo[cptr->CType].runAnim;

				if (fabs(cptr->pos.y - PlayerY) > pdist / 2) {
					cptr->beta = 0;
					cptr->gamma = 0;
					cptr->gliding = true;
					cptr->Phase = DinoInfo[cptr->CType].takeoffAnim;
				}

			}

		}
	}

	if (cptr->currentIdleGroup == -1 && !cptr->gliding) {
		if (!cptr->State) cptr->Phase = DinoInfo[cptr->CType].walkAnim;
		else if (fabs(cptr->tgalpha - cptr->alpha) < 1.0 ||
			fabs(cptr->tgalpha - cptr->alpha) > 2 * pi - 1.0)
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
		else cptr->Phase = DinoInfo[cptr->CType].walkAnim;

	}

	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->StateF & csONWATER) cptr->Phase = DinoInfo[cptr->CType].swimAnim;
	}

	/*	//TITAN_SLIDE
	if (cptr->gliding) {
		if (cptr->Slide > 40) cptr->Phase = DinoInfo[cptr->CType].slideAnim;
	}
	*/

ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
		ActivateCharacterFx(cptr);

	if (_Phase != cptr->Phase)
	{
		//==== set proportional FTime for better morphing =//

		if (cptr->gliding) {
			if (!NewPhase) cptr->FTime = 0;
		} else if (MORPHP) {
			if ((_Phase == DinoInfo[cptr->CType].runAnim ||
				_Phase == DinoInfo[cptr->CType].walkAnim) &&
				(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
					cptr->Phase == DinoInfo[cptr->CType].walkAnim))
				cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[_Phase].AniTime + 64;
			else if (!NewPhase) cptr->FTime = 0;
		}

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
	float dalpha = fabs(cptr->tgalpha - cptr->alpha);
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;
	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;
	if (cptr->currentIdleGroup >= 0) goto SKIPROT;




	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.6f + drspd * 1.2f;
		else currspeed = -0.6f - drspd * 1.2f;
	else currspeed = 0;
	if (cptr->AfraidTime && !cptr->gliding) currspeed *= 2.5;
	if (cptr->gliding) currspeed /= 2;

	if (dalpha > pi) currspeed *= -1;
	if (((cptr->StateF & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim ) && !cptr->gliding) currspeed /= 1.4f;

	if (cptr->gliding) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 460.f);
	else if (cptr->AfraidTime) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 160.f);
	else DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 180.f);

	if (cptr->gliding) {
		tgbend = drspd / 2.f;
		if (tgbend > pi / 10) tgbend = pi / 10;
	} else {
		tgbend = drspd / AIInfo[cptr->Clone].targetBendRotSpd;
		if (tgbend > pi / 5) tgbend = pi / 5;
	}

	tgbend *= SGN(currspeed);
	if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
	else if (cptr->gliding) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 400.f);
	else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 600.f);


	rspd = cptr->rspeed * TimeDt / 1024.f;




	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

SKIPROT:

	/*	//TITAN_SLIDE
	if (cptr->gliding) {
		//======= set slide mode ===========//
		if (!cptr->Slide && cptr->vspeed > 0.6)
			if (AngleDifference(cptr->tgalpha, cptr->alpha) > pi * 2 / 3.f)
			{
				cptr->Slide = static_cast<int>((cptr->vspeed*700.f));
				cptr->slidex = cptr->lookx;
				cptr->slidez = cptr->lookz;
				cptr->vspeed = 0;
			}
	}
	*/


	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;
	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim) curspeed = DinoInfo[cptr->CType].flyspd;
	if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) curspeed = DinoInfo[cptr->CType].gldspd;
	if (cptr->Phase == DinoInfo[cptr->CType].takeoffAnim) curspeed = DinoInfo[cptr->CType].tkfspd;
	if (cptr->Phase == DinoInfo[cptr->CType].diveAnim) curspeed = DinoInfo[cptr->CType].divspd;
	if (cptr->Phase == DinoInfo[cptr->CType].landAnim) curspeed = DinoInfo[cptr->CType].lndspd;
	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) curspeed = DinoInfo[cptr->CType].swmspd;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) curspeed = 0.0f;

	/*	//TITAN_SLIDE
	if (cptr->gliding && cptr->Slide)
	{
		curspeed /= 8;
		if (drspd > pi / 2.f) curspeed = 0;
		else if (drspd > pi / 4.f) curspeed *= 2.f - 4.f*drspd / pi;
	}
	else */
	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim) cptr->pos.y += TimeDt / 5.f;
	if (cptr->Phase == DinoInfo[cptr->CType].takeoffAnim) cptr->pos.y += TimeDt / 4.f;
	if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) cptr->pos.y -= TimeDt / 10.f;
	if (cptr->Phase == DinoInfo[cptr->CType].landAnim) cptr->pos.y -= TimeDt;
	if (cptr->Phase == DinoInfo[cptr->CType].diveAnim) cptr->pos.y -= TimeDt;

	//if (cptr->pos.y < landH + 236) cptr->pos.y = landH + 256;

	//========== process speed =============//

	if (cptr->gliding) {
		curspeed *= cptr->scale;
		DeltaFunc(cptr->vspeed, curspeed, TimeDt / 2024.f);

		cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
		cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

		cptr->tggamma = cptr->rspeed / 1.5f;
		if (cptr->tggamma > pi / 3.f) cptr->tggamma = pi / 3.f;
		if (cptr->tggamma < -pi / 3.f) cptr->tggamma = -pi / 3.f;
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 3048.f);

	} else {

		DeltaFunc(cptr->vspeed, curspeed, TimeDt / 500.f);

		MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt * cptr->scale,
			cptr->lookz * cptr->vspeed * TimeDt * cptr->scale, !DinoInfo[cptr->CType].canSwim, true);

		/*	//TITAN_SLIDE
		//========== slide ==============//
		if (cptr->Slide && cptr->gliding)
		{
			MoveCharacter(cptr, cptr->slidex * cptr->Slide / 600.f * TimeDt * cptr->scale,
				cptr->slidez * cptr->Slide / 600.f * TimeDt * cptr->scale, !DinoInfo[cptr->CType].canSwim, true);

			cptr->Slide -= TimeDt;
			if (cptr->Slide < 0) cptr->Slide = 0;
		}
		*/


		//============ Y movement =================//

		if (cptr->pos.y < landH) cptr->pos.y = landH;

		if (cptr->StateF & csONWATER && DinoInfo[cptr->CType].canSwim)
		{
			cptr->pos.y = landUpH - (DinoInfo[cptr->CType].waterLevel + 20) * cptr->scale;
			cptr->beta /= 2;
			cptr->tggamma = 0;
		}
		else
		{
			ThinkY_Beta_Gamma(cptr,
				AIInfo[cptr->Clone].yBetaGamma1,
				AIInfo[cptr->Clone].yBetaGamma2,
				AIInfo[cptr->Clone].yBetaGamma3,
				AIInfo[cptr->Clone].yBetaGamma4);
		}

		//=== process to tggamma ===//
		if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].walkTargetGammaRot;
		else cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].targetGammaRot;

		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1624.f);

		//==================================================//


	}

}

void AnimatePoacher(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;

	cptr->FTime += TimeDt;

	ProcessPrevPhase(cptr);

	//======== select new phase =======================//
	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;

		NewPhase = true;
	}



	if (NewPhase) {
		if (!cptr->ammo) {
			if (DinoInfo[cptr->CType].reloadAnim>=0) cptr->Phase = DinoInfo[cptr->CType].reloadAnim;
			else cptr->ammo = DinoInfo[cptr->CType].Reload;
		} else {
			cptr->Phase = DinoInfo[cptr->CType].fireAnim;
		}
	}

	cptr->alpha += pi/650;
	if (cptr->alpha > 2 * pi) cptr->alpha -= 2 * pi;



ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase) {
		ActivateCharacterFx(cptr);
		if (cptr->Phase == DinoInfo[cptr->CType].reloadAnim) {
		
			cptr->ammo = DinoInfo[cptr->CType].Reload;
		
		} else if (cptr->Phase == DinoInfo[cptr->CType].fireAnim) {
			if (WeapInfo[DinoInfo[cptr->CType].Weapon].MGSSound) {
				Vector3d shotpos = SubVectors(cptr->pos, PlayerPos);
				shotpos.x /= -3.f;
				shotpos.y /= -3.f;
				shotpos.z /= -3.f;
				shotpos = SubVectors(PlayerPos, shotpos);
				AddVoice3d(fxGunShot[DinoInfo[cptr->CType].Weapon].length,
					fxGunShot[DinoInfo[cptr->CType].Weapon].lpData.data(),
					shotpos.x, shotpos.y, shotpos.z);
			}

			cptr->ammo -= 1;

			for (int s = 0; s <= WeapInfo[DinoInfo[cptr->CType].Weapon].TraceC; s++)
			{
				float rA = siRand(128) * 0.00010 * (2.f - WeapInfo[DinoInfo[cptr->CType].Weapon].Prec);
				float rB = siRand(128) * 0.00010 * (2.f - WeapInfo[DinoInfo[cptr->CType].Weapon].Prec);


				float ca = static_cast<float>(cos(cptr->alpha + rA + pi / 2));
				float sa = static_cast<float>(sin(cptr->alpha + rA + pi / 2));
				float cb = static_cast<float>(cos(cptr->beta + rB));
				float sb = static_cast<float>(sin(cptr->beta + rB));

				nv.x = sa;
				nv.y = 0;
				nv.z = -ca;

				nv.x *= cb;
				nv.y = -sb;
				nv.z *= cb;

				float v = WeapInfo[DinoInfo[cptr->CType].Weapon].Veloc;
				if (IsUnderwater()) v = WeapInfo[DinoInfo[cptr->CType].Weapon].VelocAq;
				float l = WeapInfo[DinoInfo[cptr->CType].Weapon].Veloc;
				if (WeapInfo[DinoInfo[cptr->CType].Weapon].aqLow) l = WeapInfo[DinoInfo[cptr->CType].Weapon].VelocAq;

				AddBullet(cptr->pos.x, cptr->pos.y + (170 * cptr->scale), cptr->pos.z,
					nv.x * 64 * v,
					nv.y * 64 * v,
					nv.z * 64 * v,
					nv.x * 64 * l,
					nv.y * 64 * l,
					nv.z * 64 * l,
					DinoInfo[cptr->CType].Weapon,
					true);
			}
		}
	}

	if (_Phase != cptr->Phase)
	{
		//==== set proportional FTime for better morphing =//

		if (MORPHP || !AIInfo[cptr->Clone].carnivore || AIInfo[cptr->Clone].iceAge) {
			if ((_Phase == DinoInfo[cptr->CType].runAnim ||
				_Phase == DinoInfo[cptr->CType].walkAnim) &&
				(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
					cptr->Phase == DinoInfo[cptr->CType].walkAnim))
				cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[_Phase].AniTime + 64;
			else if (!NewPhase) cptr->FTime = 0;
		}

		if (cptr->PPMorphTime > 128)
		{
			cptr->PrevPhase = _Phase;
			cptr->PrevPFTime = _FTime;
			cptr->PPMorphTime = 0;
		}
	}

	cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;

}




void AnimateHuntable(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;
	if ((!AIInfo[cptr->Clone].carnivore || AIInfo[cptr->Clone].iceAge) && cptr->AfraidTime) cptr->AfraidTime = MAX(0, cptr->AfraidTime - TimeDt);

	bool alertInit = false;
	if (cptr->State == 2) alertInit = true;
	if (cptr->packId >= 0) {
		if (!cptr->State && Packs[cptr->packId]._alert) alertInit = true;
	}

	if (alertInit && (MyHealth || AIInfo[cptr->Clone].carnivore))
	{
		if (!AIInfo[cptr->Clone].carnivore) NewPhase = true;

		if (AIInfo[cptr->Clone].jumper) {
			if (cptr->Phase != DinoInfo[cptr->CType].jumpAnim) NewPhase = true;
		}
		cptr->State = 1;

		if (cptr->Clone == AI_SPINO || cptr->Clone == AI_CERAT) cptr->Phase = DinoInfo[cptr->CType].runAnim;
	}

TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdistSq = targetdx * targetdx + targetdz * targetdz;

	float playerdx, playerdz;
	if (cptr->Clone == AI_ALLO) {
		playerdx = PlayerX - cptr->pos.x - cptr->lookx * 100 * cptr->scale;
		playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 100 * cptr->scale;
	} else if (cptr->Clone == AI_CHASM || cptr->Clone == AI_HOG || cptr->Clone == AI_BRONT || cptr->Clone == AI_BEAR ||
		cptr->Clone == AI_WOLF || cptr->Clone == AI_RHINO || cptr->Clone == AI_SMILO) {
		playerdx = PlayerX - cptr->pos.x - cptr->lookx * 300 * cptr->scale;
		playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 300 * cptr->scale;
	} else if (AIInfo[cptr->Clone].carnivore) {
		playerdx = PlayerX - cptr->pos.x - cptr->lookx * 108;
		playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 108;
	} else {
		playerdx = PlayerX - cptr->pos.x;
		playerdz = PlayerZ - cptr->pos.z;
	}

	float pdistSq = playerdx * playerdx + playerdz * playerdz;

	


	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > DinoInfo[cptr->CType].waterLevel * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto NOTHINK;

	//============================================//			// (run away)
	if (!MyHealth) cptr->State = 0;
	if (cptr->State)
	{
		cptr->currentIdleGroup = -1;

		float aDist;
		if (AIInfo[cptr->Clone].carnivore && (!AIInfo[cptr->Clone].iceAge || cptr->Clone == AI_WOLF)) {
			aDist = ctViewR * DinoInfo[cptr->CType].aggress + OptAgres / AIInfo[cptr->Clone].agressMulti;
		} else {
			aDist = AIInfo[cptr->Clone].agressMulti * DinoInfo[cptr->CType].aggress + OptAgres / 8;
			if (pdistSq < 6000 * 6000 && cptr->Clone != AI_DEER) cptr->AfraidTime = 8000;
		}

		bool fleeMode = false;
		if (g_GameMode != GameMode::SurvivalMode) {
			if (pdistSq > aDist * aDist ||
				DinoInfo[cptr->CType].aggress <= 0 || !cptr->awareHunter) {
				fleeMode = true;
			}
			else if (DinoInfo[cptr->CType].defensive && cptr->Health == DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearShot && cptr->Health < DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearHearShot && cptr->heardShot) fleeMode = true;
			else if (cptr->packId >= 0) Packs[cptr->packId].attack = true;
		}

		if (cptr->packId >= 0) {
			if (Packs[cptr->packId]._attack) fleeMode = false;
		}

		if (fleeMode) {
			nv.x = playerdx;
			nv.z = playerdz;
			nv.y = 0;
			NormVector(nv, 2048.f);
			cptr->tgx = cptr->pos.x - nv.x;
			cptr->tgz = cptr->pos.z - nv.z;
			cptr->tgtime = 0;
			if (AIInfo[cptr->Clone].carnivore) cptr->AfraidTime -= TimeDt;

			if (cptr->packId >= 0) {
				if (cptr->AfraidTime <= 0)
				{
					if (!Packs[cptr->packId]._alert) {
						if (AIInfo[cptr->Clone].carnivore)cptr->AfraidTime = 0;
						else SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
						cptr->State = 0;
					}
				}
				else Packs[cptr->packId].alert = true;
			}
			else if (cptr->AfraidTime <= 0) {
				if (AIInfo[cptr->Clone].carnivore)cptr->AfraidTime = 0;
				else SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
				cptr->State = 0;
			}

		}
		else
		{
			cptr->tgx = PlayerX;
			cptr->tgz = PlayerZ;
			cptr->tgtime = 0;
			if (cptr->packId >= 0 && AIInfo[cptr->Clone].carnivore) {
				Packs[cptr->packId].alert = true;
			}
		}

		if (AIInfo[cptr->Clone].jumper) {
			if (!(cptr->StateF & csONWATER))
				if (pdistSq < (1324 * cptr->scale) * (1324 * cptr->scale) && pdistSq > (900 * cptr->scale) * (900 * cptr->scale))
					if (AngleDifference(cptr->alpha, FindVectorAlpha(playerdx, playerdz)) < 0.2f)
						cptr->Phase = DinoInfo[cptr->CType].jumpAnim;
		}

		if (pdistSq < DinoInfo[cptr->CType].killDist * DinoInfo[cptr->CType].killDist && DinoInfo[cptr->CType].killDist > 0) {
			int killAlt = DinoInfo[cptr->CType].waterLevel;
			if (killAlt < 256) killAlt = 256;
			if (fabs(PlayerY - cptr->pos.y) < killAlt + 20)
			{

				if (DinoInfo[cptr->CType].killTypeCount > 0) {

					if (!(cptr->StateF & csONWATER))
					{
						cptr->vspeed /= 8.0f;
						cptr->State = 1;
						cptr->Phase = DinoInfo[cptr->CType].killType[cptr->killType].anim;
						if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) cptr->FTime = 0;
						AddDeadBody(cptr,
							DinoInfo[cptr->CType].killType[cptr->killType].hunteranim,
							DinoInfo[cptr->CType].killType[cptr->killType].scream);
					}
					else AddDeadBody(cptr, HUNT_EAT, true);

				}
				else {
					AddDeadBody(cptr, HUNT_EAT, true);
					cptr->State = 0;
				}

			}
		}


	}


	if (pdistSq > ((ctViewR + 20) * 256) * ((ctViewR + 20) * 256))
		if (ReplaceCharacterForward(cptr)) goto TBEGIN;


	if (!cptr->State)
	{
		if (cptr->Clone == AI_VELO || cptr->Clone == AI_CERAT || !AIInfo[cptr->Clone].carnivore) cptr->AfraidTime = 0;

		if (pdistSq < 1024.f * 1024.f && cptr->Clone == AI_DEER && !ObservMode && !DEBUG) {
			cptr->State = 1;
			cptr->AfraidTime = (6 + rRand(8)) * 1024;
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
			goto TBEGIN;
		}


		if (cptr->packId >= 0) {
			float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
			float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
			float leaderdistSq = leaderdx * leaderdx + leaderdz * leaderdz;

			if (cptr->followLeader) {
				if (leaderdistSq < (cptr->packDensity * 128 * 0.6) * (cptr->packDensity * 128 * 0.6))
				{
					cptr->followLeader = false;
					SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
					goto TBEGIN;
				}
			}
			else {
				if (leaderdistSq > (cptr->packDensity * 128 * 1.3) * (cptr->packDensity * 128 * 1.3))
				{
					cptr->followLeader = true;
				}
			}

		}

		if (cptr->followLeader) {
			cptr->tgx = Packs[cptr->packId].leader->pos.x;
			cptr->tgz = Packs[cptr->packId].leader->pos.z;
		}
		else if (tdistSq < 456 * 456)
		{
			SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
			goto TBEGIN;
		}



	}

NOTHINK:
	if (pdistSq < AIInfo[cptr->Clone].pWMin * AIInfo[cptr->Clone].pWMin && (AIInfo[cptr->Clone].carnivore || AIInfo[cptr->Clone].iceAge)) cptr->NoFindCnt = 0;
	if (cptr->NoFindCnt) cptr->NoFindCnt--;
	else
	{
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);

		//bool weaveCondition = pdist > AIInfo[cptr->Clone].weaveRange;

		/*
		if (AIInfo[cptr->Clone].iceAge) {
			weaveCondition = pdist > 3072 && cptr->AfraidTime; //12*256
		} else {
			if (AIInfo[cptr->Clone].carnivore) {
				weaveCondition = cptr->State && pdist > 1648;
			}
			else {
				weaveCondition = cptr->AfraidTime;
			}
		}
		*/

		//if (!AIInfo[cptr->Clone].carnivore || AIInfo[cptr->Clone].iceAge) weaveCondition = weaveCondition && cptr->AfraidTime;

		if (cptr->State && pdistSq > DinoInfo[cptr->CType].weaveRange * DinoInfo[cptr->CType].weaveRange && !DinoInfo[cptr->CType].dontWeave)
		{
			float rTD;
			if (AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge) {
				rTD = 824.f;
			} else {
				rTD = 1024.f;
			}
			cptr->tgalpha += static_cast<float>(sin(RealTime / rTD)) / AIInfo[cptr->Clone].tGAIncrement;
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

		if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) {
			if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				cptr->State = 0;
			}
		}


		NewPhase = true;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount)  goto ENDPSELECT;

	if (AIInfo[cptr->Clone].jumper) {
		if (NewPhase && _Phase == DinoInfo[cptr->CType].jumpAnim)
		{
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
			goto ENDPSELECT;
		}

		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) goto ENDPSELECT;
	}

	if (NewPhase)
		if (!cptr->State)
		{




			if (DinoInfo[cptr->CType].idleGroupCount
				&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
				&& !(cptr->StateF & csONWATER)) {

					if (cptr->currentIdleGroup >= 0) {
						if (rRand(127) + 1 > (1 - DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].end) * 128
							&& (DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].endOnAny
							|| cptr->Phase == DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].anim[DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].count - 1])) {
							cptr->Phase = DinoInfo[cptr->CType].walkAnim;
							if (DinoInfo[cptr->CType].idleGroup[cptr->currentIdleGroup].instantRepeat){
								cptr->currentIdleGroup = -1; //this must be done inside the if statement
							} else {
								cptr->currentIdleGroup = -1; //this must be done inside the if statement
								goto ENDPSELECT;
							}
						} else {
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
					} else cptr->Phase = DinoInfo[cptr->CType].walkAnim;
					

			}
			else {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
			}

		}
		else cptr->Phase = DinoInfo[cptr->CType].runAnim;

	if (cptr->currentIdleGroup == -1){
	if (AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge) {
		if (!cptr->State) cptr->Phase = DinoInfo[cptr->CType].walkAnim;
		else if (fabs(cptr->tgalpha - cptr->alpha) < 1.0 ||
			fabs(cptr->tgalpha - cptr->alpha) > 2 * pi - 1.0)
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
		else cptr->Phase = DinoInfo[cptr->CType].walkAnim;
	} else {
		//NEEDED FOR SWIMMING STUFF
		if (!cptr->State) cptr->Phase = DinoInfo[cptr->CType].walkAnim;
		else cptr->Phase = DinoInfo[cptr->CType].runAnim;
	}
	}

	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->StateF & csONWATER) cptr->Phase = DinoInfo[cptr->CType].swimAnim;
	}

	if (cptr->Clone != AI_CERAT && AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge) {
		if (cptr->Slide > 40) cptr->Phase = DinoInfo[cptr->CType].slideAnim;
	}


ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
		ActivateCharacterFx(cptr);

	if (_Phase != cptr->Phase)
	{
		//==== set proportional FTime for better morphing =//

		if (MORPHP || !AIInfo[cptr->Clone].carnivore || AIInfo[cptr->Clone].iceAge) {
			if ((_Phase == DinoInfo[cptr->CType].runAnim ||
				_Phase == DinoInfo[cptr->CType].walkAnim) &&
				(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
					cptr->Phase == DinoInfo[cptr->CType].walkAnim))
				cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[_Phase].AniTime + 64;
			else if (!NewPhase) cptr->FTime = 0;
		}

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
    float dalpha = fabs(cptr->tgalpha - cptr->alpha);
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;

	if (AIInfo[cptr->Clone].jumper) {
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) goto SKIPROT;
	}
	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;
	if (cptr->currentIdleGroup >= 0) goto SKIPROT;


	if (AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge) {

		if (drspd > 0.02)
			if (cptr->tgalpha > cptr->alpha) currspeed = 0.6f + drspd * 1.2f;
			else currspeed = -0.6f - drspd * 1.2f;
		else currspeed = 0;
		if (cptr->AfraidTime) currspeed *= 2.5;

		if (dalpha > pi) currspeed *= -1;
		if ((cptr->StateF & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim) currspeed /= 1.4f;

		if (cptr->AfraidTime) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 160.f);
		else DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 180.f);

		tgbend = drspd / AIInfo[cptr->Clone].targetBendRotSpd;
		if (tgbend > pi / 5) tgbend = pi / 5;

		tgbend *= SGN(currspeed);
		if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
		else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 600.f);


		rspd = cptr->rspeed * TimeDt / 1024.f;

	} else {

		if (drspd > 0.02){
			if (cptr->tgalpha > cptr->alpha) currspeed = AIInfo[cptr->Clone].rot1 + drspd * AIInfo[cptr->Clone].rot2;
			else currspeed = -AIInfo[cptr->Clone].rot1 - drspd * AIInfo[cptr->Clone].rot2;
		} else currspeed = 0;

		if (cptr->AfraidTime) currspeed *= 1.5;
		if (dalpha > pi) currspeed *= -1;
		if ((cptr->State & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim) currspeed /= 1.4f;

		DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 400.f);

		tgbend = drspd / AIInfo[cptr->Clone].targetBendRotSpd;
		if (tgbend > pi / AIInfo[cptr->Clone].targetBendMin) tgbend = pi / AIInfo[cptr->Clone].targetBendMin;

		tgbend *= SGN(currspeed);
		if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / AIInfo[cptr->Clone].targetBendDelta1);
		else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / AIInfo[cptr->Clone].targetBendDelta2);


		rspd = cptr->rspeed * TimeDt / 612.f;

	}


	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

SKIPROT:

	if (cptr->Clone != AI_CERAT && AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge) {
		//======= set slide mode ===========//
		if (!cptr->Slide && cptr->vspeed > 0.6 && (cptr->Phase != DinoInfo[cptr->CType].jumpAnim || !AIInfo[cptr->Clone].jumper))
			if (AngleDifference(cptr->tgalpha, cptr->alpha) > pi * 2 / 3.f)
			{
				cptr->Slide = static_cast<int>((cptr->vspeed*700.f));
				cptr->slidex = cptr->lookx;
				cptr->slidez = cptr->lookz;
				cptr->vspeed = 0;
			}
	}

	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;
	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) curspeed = DinoInfo[cptr->CType].swmspd;
	}
	if (AIInfo[cptr->Clone].jumper) {
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) curspeed = DinoInfo[cptr->CType].jmpspd;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) curspeed = 0.0f;

	if (cptr->Phase == DinoInfo[cptr->CType].runAnim && cptr->Slide && cptr->Clone != AI_CERAT && AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge)
	{
		curspeed /= 8;
		if (drspd > pi / 2.f) curspeed = 0;
		else if (drspd > pi / 4.f) curspeed *= 2.f - 4.f*drspd / pi;
	}
	else if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	//========== process speed =============//

	if (AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge) {

		DeltaFunc(cptr->vspeed, curspeed, TimeDt / 500.f);

		if (AIInfo[cptr->Clone].jumper) {
			if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) cptr->vspeed = DinoInfo[cptr->CType].jmpspd;
		}

		MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt * cptr->scale,
			cptr->lookz * cptr->vspeed * TimeDt * cptr->scale, !DinoInfo[cptr->CType].canSwim, true);

		if (cptr->Clone != AI_CERAT) {
			//========== slide ==============//
			if (cptr->Slide)
			{
				MoveCharacter(cptr, cptr->slidex * cptr->Slide / 600.f * TimeDt * cptr->scale,
					cptr->slidez * cptr->Slide / 600.f * TimeDt * cptr->scale, !DinoInfo[cptr->CType].canSwim, true);

				cptr->Slide -= TimeDt;
				if (cptr->Slide < 0) cptr->Slide = 0;
			}
		}

	} else {

		curspeed *= cptr->scale;
		if (curspeed > cptr->vspeed) DeltaFunc(cptr->vspeed, curspeed, TimeDt / 1024.f);
		else DeltaFunc(cptr->vspeed, curspeed, TimeDt / 256.f);

		MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt,
			cptr->lookz * cptr->vspeed * TimeDt, !DinoInfo[cptr->CType].canSwim, true);

	}


	//============ Y movement =================//
	if (cptr->StateF & csONWATER && DinoInfo[cptr->CType].canSwim)
	{
		cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - (DinoInfo[cptr->CType].waterLevel + 20) * cptr->scale;
		cptr->beta /= 2;
		cptr->tggamma = 0;
	}
	else
	{
		ThinkY_Beta_Gamma(cptr,
			AIInfo[cptr->Clone].yBetaGamma1,
			AIInfo[cptr->Clone].yBetaGamma2,
			AIInfo[cptr->Clone].yBetaGamma3,
			AIInfo[cptr->Clone].yBetaGamma4);
	}

	//=== process to tggamma ===//
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].walkTargetGammaRot;
	else cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].targetGammaRot;
	if (AIInfo[cptr->Clone].jumper) {
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) cptr->tggamma = 0;
	}

	if (AIInfo[cptr->Clone].carnivore && !AIInfo[cptr->Clone].iceAge){
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1624.f);
	} else {
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
	}

	//==================================================//

}



Vector3d LookForATree(TCharacter *cptr) {

	float searchAlpha1 = cptr->tgalpha;
	float searchAlpha2 = cptr->tgalpha;
	float dalpha = 15.f;


	for (int i = 0; i < 12; i++)
	{
		searchAlpha1 = cptr->tgalpha + dalpha * pi / 180.f;
		searchAlpha2 = cptr->tgalpha - dalpha * pi / 180.f;
		Vector3d p1 = cptr->pos;
		Vector3d p2 = cptr->pos;
		float lookx1 = static_cast<float>(cos(searchAlpha1));
		float lookz1 = static_cast<float>(sin(searchAlpha1));
		float lookx2 = static_cast<float>(cos(searchAlpha2));
		float lookz2 = static_cast<float>(sin(searchAlpha2));
		for (int t = 0; t < 20; t++) {
			p1.x += lookx1 * 256.f;
			p1.z += lookz1 * 256.f;
			p2.x += lookx2 * 256.f;
			p2.z += lookz2 * 256.f;

			int ccx1 = static_cast<int>(p1.x) / 256;
			int ccz1 = static_cast<int>(p1.z) / 256;
			int ccx2 = static_cast<int>(p2.x) / 256;
			int ccz2 = static_cast<int>(p2.z) / 256;
			for (int z = -2; z <= 2; z++) {
				for (int x = -2; x <= 2; x++) {
					if (TreeTable[OMap[ccz1 + z][ccx1 + x]])
					{
						Vector3d tree;
						tree.x = ccx1 + x;
						tree.z = ccz1 + z;
						return tree;
					}
					if (TreeTable[OMap[ccz2 + z][ccx2 + x]])
					{
						Vector3d tree;
						tree.x = ccx2 + x;
						tree.z = ccz2 + z;
						return tree;
					}
				}
			}
		}
	}
	Vector3d tree;
	tree.x = 0;
	tree.z = 0;
	return tree;
}


Vector3d CheckForATree(TCharacter *cptr) {

	int ccx = static_cast<int>(cptr->pos.x) / 256;
	int ccz = static_cast<int>(cptr->pos.z) / 256;
	/* for (int z = -2; z <= 2; z++) {
		for (int x = -2; x <= 2; x++) {
			if (TreeTable[OMap[ccz + z][ccx + x]])
			{
				Vector3d tree;
				tree.x = ccx + x;
				tree.z = ccz + z;
				return tree;
			}
		}
	}*/

	if (TreeTable[OMap[ccz][ccx]])
	{
		Vector3d tree;
		tree.x = ccx;
		tree.z = ccz;
		return tree;
	}

	Vector3d tree;
	tree.x = 0;
	tree.z = 0;
	return tree;
}


void AnimateMicro(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;

	bool alertInit = false;
	if (cptr->State == 2) alertInit = true;
	if (cptr->packId >= 0) {
		if (!cptr->State && Packs[cptr->packId]._alert) alertInit = true;
	}



	if (alertInit)
	{
		cptr->State = 1;
		if (cptr->gliding) cptr->Phase = DinoInfo[cptr->CType].glideAnim;
		else cptr->Phase = DinoInfo[cptr->CType].runAnim;
	}

TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdistSq = targetdx * targetdx + targetdz * targetdz;

	float playerdx, playerdz;
	playerdx = PlayerX - cptr->pos.x - cptr->lookx * 108;
	playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 108;
	float pdistSq = playerdx * playerdx + playerdz * playerdz;




	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > DinoInfo[cptr->CType].waterLevel * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto NOTHINK;

	//============================================//			// (run away)
	if (!MyHealth) cptr->State = 0;
	if (cptr->State)
	{

		cptr->currentIdleGroup = -1;

		float aDist;
		aDist = ctViewR * DinoInfo[cptr->CType].aggress + OptAgres / AIInfo[cptr->Clone].agressMulti;
		if (cptr->gliding) aDist *= 2;

		bool fleeMode = false;
		if (g_GameMode != GameMode::SurvivalMode) {
			if (pdistSq > aDist * aDist ||
				DinoInfo[cptr->CType].aggress <= 0 || !cptr->awareHunter) {
				fleeMode = true;
			}
			else if (DinoInfo[cptr->CType].defensive && cptr->Health == DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearShot && cptr->Health < DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearHearShot && cptr->heardShot) fleeMode = true;
			else if (cptr->packId >= 0) Packs[cptr->packId].attack = true;
		}

		if (cptr->packId >= 0) {
			if (Packs[cptr->packId]._attack) fleeMode = false;
		}


		Vector3d tree;
		cptr->gottaClimb = false;
		if (pdistSq > 1000 * 1000 && !cptr->gliding) {
			tree = LookForATree(cptr);
			if (tree.x) cptr->gottaClimb = true;
		}

		if (fleeMode) {
			nv.x = playerdx;
			nv.z = playerdz;
			nv.y = 0;
			NormVector(nv, 2048.f);
			cptr->tgx = cptr->pos.x - nv.x;
			cptr->tgz = cptr->pos.z - nv.z;
			cptr->tgtime = 0;
			cptr->AfraidTime -= TimeDt;

			if (cptr->packId >= 0) {
				if (cptr->AfraidTime <= 0)
				{
					if (!Packs[cptr->packId]._alert) {
						cptr->AfraidTime = 0;
						cptr->State = 0;
					}
				}
				else Packs[cptr->packId].alert = true;
			}
			else if (cptr->AfraidTime <= 0) {
				cptr->AfraidTime = 0;
				cptr->State = 0;
			}

		}
		else {
			if (cptr->gottaClimb) {
				cptr->tgx = tree.x * 256.f;
				cptr->tgz = tree.z * 256.f;
			}
			else {
				cptr->tgx = PlayerX;
				cptr->tgz = PlayerZ;
			}
			cptr->tgtime = 0;


			if (cptr->packId >= 0) {
				Packs[cptr->packId].alert = true;
			}

		}



		if (pdistSq < DinoInfo[cptr->CType].killDist * DinoInfo[cptr->CType].killDist && DinoInfo[cptr->CType].killDist > 0) {
			int killAlt = DinoInfo[cptr->CType].waterLevel;
			if (killAlt < 256) killAlt = 256;
			if (fabs(PlayerY - cptr->pos.y) < killAlt + 20)
			{

				if (DinoInfo[cptr->CType].killTypeCount > 0) {

					if (!(cptr->StateF & csONWATER))
					{
						cptr->vspeed /= 8.0f;
						cptr->State = 1;
						cptr->Phase = DinoInfo[cptr->CType].killType[cptr->killType].anim;
						if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) cptr->FTime = 0;
						AddDeadBody(cptr,
							DinoInfo[cptr->CType].killType[cptr->killType].hunteranim,
							DinoInfo[cptr->CType].killType[cptr->killType].scream);
					}
					else AddDeadBody(cptr, HUNT_EAT, true);

				}
				else {
					AddDeadBody(cptr, HUNT_EAT, true);
					cptr->State = 0;
				}

			}
		}


	}

	if (!cptr->State)
	{
		cptr->AfraidTime = 0;

		if (cptr->packId >= 0) {
			float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
			float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
			float leaderdistSq = leaderdx * leaderdx + leaderdz * leaderdz;

			if (cptr->followLeader) {
				if (leaderdistSq < (cptr->packDensity * 128 * 0.6) * (cptr->packDensity * 128 * 0.6))
				{
					cptr->followLeader = false;
					SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
					goto TBEGIN;
				}
			}
			else {
				if (leaderdistSq > (cptr->packDensity * 128 * 1.3) * (cptr->packDensity * 128 * 1.3))
				{
					cptr->followLeader = true;
				}
			}

		}

		float tdst = 456;
		// todo randomly triple target distance

		if (cptr->followLeader) {
			cptr->tgx = Packs[cptr->packId].leader->pos.x;
			cptr->tgz = Packs[cptr->packId].leader->pos.z;
		}
		else if (tdistSq < tdst * tdst)
		{
			SetNewTargetPlace(cptr, AIInfo[cptr->Clone].targetDistance);
			goto TBEGIN;
		}



	}

NOTHINK:
	if (pdistSq < AIInfo[cptr->Clone].pWMin * AIInfo[cptr->Clone].pWMin && !cptr->gliding) cptr->NoFindCnt = 0;
	if (cptr->NoFindCnt && !cptr->gliding) cptr->NoFindCnt--;
	else
	{
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);

		if (cptr->State && (pdistSq > DinoInfo[cptr->CType].weaveRange * DinoInfo[cptr->CType].weaveRange || !cptr->gottaClimb) && !DinoInfo[cptr->CType].dontWeave)
		{
			float rTD;
			rTD = 824.f;

			cptr->tgalpha += static_cast<float>(sin(RealTime / rTD)) / AIInfo[cptr->Clone].tGAIncrement;
			if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
			if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
		}
	}

	if (!cptr->gliding) {

		LookForAWay(cptr, !DinoInfo[cptr->CType].canSwim, true);

		if (cptr->NoWayCnt > AIInfo[cptr->Clone].noWayCntMin)
		{
			cptr->NoWayCnt = 0;
			cptr->NoFindCnt = AIInfo[cptr->Clone].noFindWayMed + rRand(AIInfo[cptr->Clone].noFindWayRange);
		}
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

		if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) {
			if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				cptr->State = 0;
			}
		}


		NewPhase = true;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto ENDPSELECT;

	if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) {
		if (cptr->pos.y <= GetLandUpH(cptr->pos.x, cptr->pos.z)) {
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
			cptr->gliding = false;
		} else goto ENDPSELECT;
	} //temp

	if (cptr->State) {
		if (cptr->Phase != DinoInfo[cptr->CType].climbAnim) {
			Vector3d tree = CheckForATree(cptr);
			if (tree.x) {
				cptr->climbable.x = (tree.x * 256.f) +128.f;
				cptr->climbable.z = (tree.z * 256.f) +128.f;
				cptr->vspeed = 0;
				cptr->climbY = GetLandH(cptr->climbable.x, cptr->climbable.z) + MObjects[OMap[static_cast<int>(tree.z)][static_cast<int>(tree.x)]].info.YHi - 384;
				cptr->Phase = DinoInfo[cptr->CType].climbAnim;
				cptr->gliding = true;
				goto ENDPSELECT;
			}
		} else {
			if (cptr->pos.y >= cptr->climbY) cptr->Phase = DinoInfo[cptr->CType].glideAnim;
			goto ENDPSELECT;
		}
	}

	if (NewPhase)
		if (!cptr->State)
		{

			if (DinoInfo[cptr->CType].idleGroupCount
				&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
				&& !(cptr->StateF & csONWATER)) {

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
			else {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
			}

		}
		else cptr->Phase = DinoInfo[cptr->CType].runAnim;

	if (cptr->currentIdleGroup == -1) {
		if (!cptr->State) cptr->Phase = DinoInfo[cptr->CType].walkAnim;
		else if (fabs(cptr->tgalpha - cptr->alpha) < 1.0 ||
			fabs(cptr->tgalpha - cptr->alpha) > 2 * pi - 1.0)
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
		else cptr->Phase = DinoInfo[cptr->CType].walkAnim;
	}

	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->StateF & csONWATER) cptr->Phase = DinoInfo[cptr->CType].swimAnim;
	}

	if (!cptr->gliding) {
		if (cptr->Slide > 40) cptr->Phase = DinoInfo[cptr->CType].slideAnim;
	}


ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
		ActivateCharacterFx(cptr);

	if (_Phase != cptr->Phase)
	{
		//==== set proportional FTime for better morphing =//

		if (cptr->gliding) {
			if (!NewPhase) cptr->FTime = 0;
		}
		else if (MORPHP) {
			if ((_Phase == DinoInfo[cptr->CType].runAnim ||
				_Phase == DinoInfo[cptr->CType].walkAnim) &&
				(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
					cptr->Phase == DinoInfo[cptr->CType].walkAnim))
				cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[_Phase].AniTime + 64;
			else if (!NewPhase) cptr->FTime = 0;
		}

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
	float dalpha = fabs(cptr->tgalpha - cptr->alpha);
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;
	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;
	if (cptr->currentIdleGroup >= 0) goto SKIPROT;

	if (cptr->Phase == DinoInfo[cptr->CType].climbAnim) {
		cptr->pos.x = cptr->climbable.x - (cptr->lookx * DinoInfo[cptr->CType].climbDist);
		cptr->pos.z = cptr->climbable.z - (cptr->lookz * DinoInfo[cptr->CType].climbDist);
		if (_Phase != DinoInfo[cptr->CType].climbAnim) cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z);
		cptr->gamma = 0;
		cptr->beta = 0;
		goto SKIPROT;
	}


	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.6f + drspd * 1.2f;
		else currspeed = -0.6f - drspd * 1.2f;
	else currspeed = 0;
	if (cptr->AfraidTime && !cptr->gliding) currspeed *= 2.5;
	//if (cptr->gliding) currspeed /= 2;

	if (dalpha > pi) currspeed *= -1;
	if (((cptr->StateF & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim) && !cptr->gliding) currspeed /= 1.4f;

	if (cptr->gliding) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 460.f);
	else if (cptr->AfraidTime) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 160.f);
	else DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 180.f);

	if (cptr->gliding) {
		tgbend = drspd / 2.f;
		if (tgbend > pi / 2) tgbend = pi / 2;
	}
	else {
		tgbend = drspd / AIInfo[cptr->Clone].targetBendRotSpd;
		if (tgbend > pi / 5) tgbend = pi / 5;
	}

	tgbend *= SGN(currspeed);
	if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
	else if (cptr->gliding) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 400.f);
	else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 600.f);


	rspd = cptr->rspeed * TimeDt / 1024.f;




	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

SKIPROT:

	if (!cptr->gliding) {
		//======= set slide mode ===========//
		if (!cptr->Slide && cptr->vspeed > 0.6 && (cptr->Phase != DinoInfo[cptr->CType].jumpAnim || !AIInfo[cptr->Clone].jumper))
			if (AngleDifference(cptr->tgalpha, cptr->alpha) > pi * 2 / 3.f)
			{
				cptr->Slide = static_cast<int>((cptr->vspeed*700.f));
				cptr->slidex = cptr->lookx;
				cptr->slidez = cptr->lookz;
				cptr->vspeed = 0;
			}
	}
	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;
	if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) curspeed = DinoInfo[cptr->CType].gldspd;
	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) curspeed = DinoInfo[cptr->CType].swmspd;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) curspeed = 0.0f;

	if (cptr->Phase == DinoInfo[cptr->CType].runAnim && cptr->Slide && !cptr->gliding)
	{
		curspeed /= 8;
		if (drspd > pi / 2.f) curspeed = 0;
		else if (drspd > pi / 4.f) curspeed *= 2.f - 4.f*drspd / pi;
	}
	else if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;


	if (cptr->Phase == DinoInfo[cptr->CType].climbAnim) cptr->pos.y += TimeDt / 4.f;
	if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) cptr->pos.y -= TimeDt /	8.f;

	//========== process speed =============//

	if (cptr->gliding) {
		curspeed *= cptr->scale;
		DeltaFunc(cptr->vspeed, curspeed, TimeDt / 2024.f);

		cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
		cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

		cptr->tggamma = cptr->rspeed / 4.0f;
		if (cptr->tggamma > pi / 6.f) cptr->tggamma = pi / 6.f;
		if (cptr->tggamma < -pi / 6.f) cptr->tggamma = -pi / 6.f;
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);

	}
	else {

		DeltaFunc(cptr->vspeed, curspeed, TimeDt / 500.f);

		MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt * cptr->scale,
			cptr->lookz * cptr->vspeed * TimeDt * cptr->scale, !DinoInfo[cptr->CType].canSwim, true);

		if (!cptr->gliding) {
			//========== slide ==============//
			if (cptr->Slide)
			{
				MoveCharacter(cptr, cptr->slidex * cptr->Slide / 600.f * TimeDt * cptr->scale,
					cptr->slidez * cptr->Slide / 600.f * TimeDt * cptr->scale, !DinoInfo[cptr->CType].canSwim, true);

				cptr->Slide -= TimeDt;
				if (cptr->Slide < 0) cptr->Slide = 0;
			}
		}
		//============ Y movement =================//

		if (cptr->pos.y < GetLandH(cptr->pos.x, cptr->pos.z)) cptr->pos.y = GetLandH(cptr->pos.x, cptr->pos.z);

		if (cptr->StateF & csONWATER && DinoInfo[cptr->CType].canSwim)
		{
			cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - (DinoInfo[cptr->CType].waterLevel + 20) * cptr->scale;
			cptr->beta /= 2;
			cptr->tggamma = 0;
		}
		else
		{
			ThinkY_Beta_Gamma(cptr,
				AIInfo[cptr->Clone].yBetaGamma1,
				AIInfo[cptr->Clone].yBetaGamma2,
				AIInfo[cptr->Clone].yBetaGamma3,
				AIInfo[cptr->Clone].yBetaGamma4);
		}

		//=== process to tggamma ===//
		if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].walkTargetGammaRot;
		else cptr->tggamma += cptr->rspeed / AIInfo[cptr->Clone].targetGammaRot;

		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1624.f);

		//==================================================//


	}

}






boolean huntDogSearch(TCharacter *cptr)
{
	bool preyFound = false;
	Vector3d preyPos;
	float preyDist;
	int preyNo;

	//if (!MyHealth) return false;
	if (g_GameMode == GameMode::TrophyMode) return false;

	float kR, kwind, klook, kstand;

	float kmask = 1.0f;
	float kscent = 1.5f;

	for (int c = 0; c < ChCount; c++)
	{
		TCharacter *dino = &Characters[c];

		Vector3d ppos, plook, clook, wlook, rlook;
		ppos = dino->pos;

		wlook = Wind.nv;

		plook.y = 0;
		plook.x = static_cast<float>(sin(dino->alpha));
		plook.z = static_cast<float>(-cos(dino->alpha));

		if (!dino->Health) continue;
		if (!DinoInfo[dino->CType].dogSmell) continue;

		rlook = SubVectors(dino->pos, cptr->pos);
		kR = VectorLength(rlook) / 256.f / (32.f + ctViewR / 2);
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
				dino->pos.x, dino->pos.y, dino->pos.z)) kALook *= 1.3f;

		if (kALook < 1.0)
			if (TraceLook(cptr->pos.x, cptr->pos.y + 220, cptr->pos.z,
				dino->pos.x, dino->pos.y, dino->pos.z))   kALook = 2.0;
		kALook *= (1.f + static_cast<float>(ObjectsOnLook) / 6.f);

		float kASmell = kR * ((kwind + 2.0f) / 2.0F) * ((klook + 3.f) / 3.f) * kscent;
		if (kwind > 0) kASmell *= 2.0;
		kASmell /= DinoInfo[cptr->CType].SmellK;

		float kRes = MIN(kALook, kASmell);

		if (kRes < 1.0)
		{
			kRes = MIN(kRes, kR);

			if (preyFound) {
				float dx = dino->pos.x - cptr->pos.x;
				float dz = dino->pos.z - cptr->pos.z;
				float tempDist = static_cast<float>(sqrt(dx * dx + dz * dz));
				if (tempDist < preyDist) {
					preyDist = tempDist;
					preyPos = dino->pos;
					preyNo = c;
				}
			} else {
				float dx = dino->pos.x - cptr->pos.x;
				float dz = dino->pos.z - cptr->pos.z;
				preyDist = static_cast<float>(sqrt(dx * dx + dz * dz));
				preyPos = dino->pos;
				preyNo = c;
				preyFound = true;
			}

		}
	}

	if (preyFound) {
		cptr->tgx = preyPos.x;
		cptr->tgz = preyPos.z;
		cptr->tgtime = 0;
		cptr->dogPrey = preyNo;
		return true;
	}

	return false;
}









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

	/*
	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->StateF & csONWATER) cptr->Phase = DinoInfo[cptr->CType].swimAnim;
	}
	*/

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


void AnimateTRex(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;
	BOOL LookMode = false;



TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdistSq = targetdx * targetdx + targetdz * targetdz;

	float playerdx = PlayerX - cptr->pos.x - cptr->lookx * 108;
	float playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 108;
	float pdistSq = playerdx * playerdx + playerdz * playerdz;
	float palpha = FindVectorAlpha(playerdx, playerdz);
	//if (cptr->State==2) { NewPhase=true; cptr->State=1; }


	bool alertInit = false;
	if (cptr->State == 5) alertInit = true;
	if (cptr->packId >= 0) {
		if (!cptr->State && Packs[cptr->packId]._alert) alertInit = true;
	}

	if (alertInit)
	{
		NewPhase = true;
		cptr->State = 1;
		cptr->Phase = DinoInfo[cptr->CType].walkAnim;
		cptr->FTime = 0;
		cptr->tgx = PlayerX;
		cptr->tgz = PlayerZ;
		goto TBEGIN;
	}

	if (cptr->State) Packs[cptr->packId].alert = true;



	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > DinoInfo[cptr->CType].waterLevel * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto NOTHINK;

	//============================================//
	//if (!MyHealth) cptr->State = 0; //TREX cannot return to state 0!!!

	if (cptr->State)
	{

		cptr->currentIdleGroup = -1;

		cptr->tgx = PlayerX;
		cptr->tgz = PlayerZ;
		cptr->tgtime = 0;
		if (cptr->State > 1)
			if (AngleDifference(cptr->alpha, palpha) < 0.4f)
			{
				if (cptr->State == 2) {
					if (DinoInfo[cptr->CType].lookCount) {
						cptr->Phase = DinoInfo[cptr->CType].lookAnim[rRand(DinoInfo[cptr->CType].lookCount - 1)];
						cptr->rspeed = 0;
					}
					else if (DinoInfo[cptr->CType].roarCount) {
						cptr->Phase = cptr->roarAnim;
						cptr->rspeed = 0;
					} else {
						cptr->Phase = DinoInfo[cptr->CType].runAnim;
					}
					
				}
				else {
					if (DinoInfo[cptr->CType].smellCount) {
						cptr->Phase = DinoInfo[cptr->CType].smellAnim[rRand(DinoInfo[cptr->CType].smellCount - 1)];
						cptr->rspeed = 0;
					}
					else if (DinoInfo[cptr->CType].roarCount) {
						cptr->Phase = cptr->roarAnim;
						cptr->rspeed = 0;
					} else {
						cptr->Phase = DinoInfo[cptr->CType].runAnim;
					}

				}
				cptr->State = 1;
			}




		if (pdistSq < DinoInfo[cptr->CType].killDist * DinoInfo[cptr->CType].killDist && DinoInfo[cptr->CType].killDist > 0 && MyHealth)
		{
			int killAlt = DinoInfo[cptr->CType].waterLevel;
			if (killAlt < 256) killAlt = 256;
			if (fabs(PlayerY - cptr->pos.y) < killAlt + 20)
			{
				if (DinoInfo[cptr->CType].killTypeCount > 0) {

					if (!(cptr->StateF & csONWATER))
					{
						cptr->vspeed /= 8.0f;
						cptr->State = 1;
						cptr->Phase = DinoInfo[cptr->CType].killType[cptr->killType].anim;
						if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) cptr->FTime = 0;
						AddDeadBody(cptr,
							DinoInfo[cptr->CType].killType[cptr->killType].hunteranim,
							DinoInfo[cptr->CType].killType[cptr->killType].scream);
					}
					else AddDeadBody(cptr, HUNT_EAT, true);

				}
				else {
					AddDeadBody(cptr, HUNT_EAT, true);
					cptr->State = 0;
				}

			}
		}



	}

	if (pdistSq > ((ctViewR + 20) * 256) * ((ctViewR + 20) * 256))
		if (ReplaceCharacterForward(cptr)) goto TBEGIN;

	if (!cptr->State) {


		if (cptr->packId >= 0) {
			float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
			float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
			float leaderdistSq = leaderdx * leaderdx + leaderdz * leaderdz;

			if (cptr->followLeader) {
				if (leaderdistSq < (cptr->packDensity * 128 * 0.6) * (cptr->packDensity * 128 * 0.6))
				{
					cptr->followLeader = false;
					SetNewTargetPlace(cptr, 8048.f);
					goto TBEGIN;
				}
			}
			else {
				if (leaderdistSq > (cptr->packDensity * 128 * 1.3) * (cptr->packDensity * 128 * 1.3))
				{
					cptr->followLeader = true;
				}
			}

		}

		if (cptr->followLeader) {
			cptr->tgx = Packs[cptr->packId].leader->pos.x;
			cptr->tgz = Packs[cptr->packId].leader->pos.z;
		}
		else if (tdistSq < 1224 * 1224)
		{
			SetNewTargetPlace(cptr, 8048.f);
			goto TBEGIN;
		}
	}


NOTHINK:
	if (pdistSq < 2048 * 2048) cptr->NoFindCnt = 0;
	if (cptr->NoFindCnt) cptr->NoFindCnt--;
	else
	{
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);

		if (cptr->State && pdistSq > DinoInfo[cptr->CType].weaveRange * DinoInfo[cptr->CType].weaveRange && !DinoInfo[cptr->CType].dontWeave)
		{
			cptr->tgalpha += static_cast<float>(sin(RealTime / 824.f)) / 6.f;
			if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
			if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
		}
	}

	LookForAWay(cptr, !DinoInfo[cptr->CType].canSwim, !cptr->State || DinoInfo[cptr->CType].TRexObjCollide);
	//LookForAWay(cptr, !DinoInfo[cptr->CType].canSwim, true);
	
	if (cptr->NoWayCnt > 12)
	{
		cptr->NoWayCnt = 0;
		cptr->NoFindCnt = 16 + rRand(20);
	}


	if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
	if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;

	//===============================================//

	ProcessPrevPhase(cptr);


	//======== select new phase =======================//

	
	for (int i = 0; i < DinoInfo[cptr->CType].lookCount; i++) {
		if (cptr->Phase == DinoInfo[cptr->CType].lookAnim[i]) LookMode = true;
	}

	for (int i = 0; i < DinoInfo[cptr->CType].smellCount; i++) {
		if (cptr->Phase == DinoInfo[cptr->CType].smellAnim[i]) LookMode = true;
	}
	

	cptr->FTime += TimeDt;

	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;

		if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) {
			if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				cptr->State = 0;
			}
		}


		NewPhase = true;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount)    goto ENDPSELECT;

	if (!NewPhase)
		if (DinoInfo[cptr->CType].roarCount > 0 && cptr->Phase == cptr->roarAnim) goto ENDPSELECT;
		
	if (!cptr->State)
		if (NewPhase)
			/*
			if (rRand(128) > 110
				&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
				&& !(cptr->StateF & csONWATER)
				) {
				if (rRand(128) > 64) {
					if (DinoInfo[cptr->CType].idleCount) {
						cptr->Phase = DinoInfo[cptr->CType].idleAnim[rRand(DinoInfo[cptr->CType].idleCount - 1)];
					}
				}
				else {
					if (DinoInfo[cptr->CType].idle2Count) {
						cptr->Phase = DinoInfo[cptr->CType].idle2Anim[rRand(DinoInfo[cptr->CType].idle2Count - 1)];
					}
				}
				goto ENDPSELECT;
			}
			*/

			if (DinoInfo[cptr->CType].idleGroupCount
				&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
				&& !(cptr->StateF & csONWATER)) {

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
			else {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
			}


	if (!NewPhase) {
		if (LookMode) goto ENDPSELECT;
		if (cptr->currentIdleGroup >= 0) goto ENDPSELECT;
	}

	if (cptr->State)
		if (NewPhase && LookMode)
		{
			if (DinoInfo[cptr->CType].roarCount > 0) {
				cptr->Phase = cptr->roarAnim;
				goto ENDPSELECT;
			}// else cptr->Phase = DinoInfo[cptr->CType].runAnim;
		}

	if (!cptr->State || cptr->State > 1) cptr->Phase = DinoInfo[cptr->CType].walkAnim;
	else if (fabs(cptr->tgalpha - cptr->alpha) < 1.0 ||
		fabs(cptr->tgalpha - cptr->alpha) > 2 * pi - 1.0)
		cptr->Phase = DinoInfo[cptr->CType].runAnim;
	else cptr->Phase = DinoInfo[cptr->CType].walkAnim;

	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->StateF & csONWATER) cptr->Phase = DinoInfo[cptr->CType].swimAnim;
	}

ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
		ActivateCharacterFx(cptr);

	if (_Phase != cptr->Phase)
	{
		//==== set proportional FTime for better morphing =//

		if (MORPHP) {
			if ((_Phase == DinoInfo[cptr->CType].runAnim ||
				_Phase == DinoInfo[cptr->CType].walkAnim) &&
				(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
					cptr->Phase == DinoInfo[cptr->CType].walkAnim))
				cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[_Phase].AniTime + 64;
			else if (!NewPhase) cptr->FTime = 0;
		}

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

	if (DinoInfo[cptr->CType].roarCount > 0 && cptr->Phase == cptr->roarAnim) goto SKIPROT;
	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;
	if (LookMode) goto SKIPROT;
	if (cptr->currentIdleGroup >= 0) goto SKIPROT;

	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.7f + drspd * 1.4f;
		else currspeed = -0.7f - drspd * 1.4f;
	else currspeed = 0;
	if (cptr->AfraidTime) currspeed *= 2.5;

	if (dalpha > pi) currspeed *= -1;

	if (cptr->State) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 440.f);
	else DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 620.f);

	tgbend = drspd / 2;
	if (tgbend > pi / 6.f) tgbend = pi / 6.f;

	tgbend *= SGN(currspeed);
	DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 1800.f);




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
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;
	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) curspeed = DinoInfo[cptr->CType].swmspd;
	}

	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	//========== process speed =============//

	DeltaFunc(cptr->vspeed, curspeed, TimeDt / 200.f);

	MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt * cptr->scale,
		cptr->lookz * cptr->vspeed * TimeDt * cptr->scale, !DinoInfo[cptr->CType].canSwim, true);

	//============ Y movement =================//
	if ((cptr->StateF & csONWATER) && DinoInfo[cptr->CType].canSwim)
	{
		cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - (DinoInfo[cptr->CType].waterLevel - 20) * cptr->scale;
		cptr->beta /= 2;
		cptr->tggamma = 0;
	}
	else
	{
		ThinkY_Beta_Gamma(cptr, 348, 324, 0.5f, 0.4f);
	}



	//=== process to tggamma ===//
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / 16.0f;
	else cptr->tggamma += cptr->rspeed / 12.0f;

	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2024.f);


	//==================================================//

}

//multiplayer
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













void AnimateClassicAmbient(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;
	if (cptr->AfraidTime) cptr->AfraidTime = MAX(0, cptr->AfraidTime - TimeDt);

	bool alertInit = false;
	if (cptr->State == 2) alertInit = true;
	if (cptr->packId >= 0) {
		if (!cptr->State && Packs[cptr->packId]._alert) alertInit = true;
	}

	if (alertInit) {
		NewPhase = true;
		cptr->State = 1;
	}

TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdistSq = targetdx * targetdx + targetdz * targetdz;

	bool pdistMulti = false;
	int pCh = 1;
	float pdistSq[4];
	float playerdx[4];
	float playerdz[4];
	playerdx[0] = PlayerX - cptr->pos.x;
	playerdz[0] = PlayerZ - cptr->pos.z;
	pdistSq[0] = playerdx[0] * playerdx[0] + playerdz[0] * playerdz[0];
	if (Multiplayer) {
		//for loop 1 to hunter count
		playerdx[pCh] = MPlayers[pCh].pos.x - cptr->pos.x;
		playerdz[pCh] = MPlayers[pCh].pos.z - cptr->pos.z;
		pdistSq[pCh] = playerdx[pCh] * playerdx[pCh] + playerdz[pCh] * playerdz[pCh];
		pCh += 1;
		//
	}

	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > DinoInfo[cptr->CType].waterLevel * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);


	//=========== run away =================//

	if (cptr->State)
	{
		cptr->currentIdleGroup = -1;

		if (!cptr->AfraidTime)
		{
			pdistMulti = false;
			for (int pNo = 0; pNo < pCh; pNo++) {
				if (pdistSq[pNo] < 2048.f * 2048.f) pdistMulti = true;
			}
			if (pdistMulti) {
				if (cptr->Clone == AI_GALL) cptr->State = 1;
				cptr->AfraidTime = (5 + rRand(5)) * 1024;
				if (cptr->packId >= 0) {
					Packs[cptr->packId].alert = true;
				}
			}

			pdistMulti = true;
			for (int pNo = 0; pNo < pCh; pNo++) {
				if (!(pdistSq[pNo] > 4096.f * 4096.f)) pdistMulti = false;
			}
			if (pdistMulti)
			{
				if (cptr->packId >= 0) {
					if (!Packs[cptr->packId]._alert) {
						cptr->State = 0;
						SetNewTargetPlace(cptr, 2048.f);
						goto TBEGIN;
					}
				} else {
					cptr->State = 0;
					SetNewTargetPlace(cptr, 2048.f);
					goto TBEGIN;
				}
			}
		} else if (cptr->packId >= 0) Packs[cptr->packId].alert = true;


		nv.x = playerdx[0];
		nv.z = playerdz[0];
		nv.y = 0;
		NormVector(nv, 2048.f);
		cptr->tgx = cptr->pos.x - nv.x;
		cptr->tgz = cptr->pos.z - nv.z;
		cptr->tgtime = 0;
	}

	if (pdistSq[0] > ((ctViewR + 20) * 256) * ((ctViewR + 20) * 256) && cptr->CType)
		if (ReplaceCharacterForward(cptr)) goto TBEGIN;


	//======== exploring area ===============//
	if (!cptr->State)
	{
		cptr->AfraidTime = 0;
		pdistMulti = false;
		for (int pNo = 0; pNo < pCh; pNo++) {
			if (pdistSq[pNo] < 812.f * 812.f) pdistMulti = true;
		}
		if (pdistMulti)
		{
			cptr->State = 1;
			cptr->AfraidTime = (5 + rRand(5)) * 1024;
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
			goto TBEGIN;
		}

		if (cptr->packId >= 0) {
			float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
			float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
			float leaderdistSq = leaderdx * leaderdx + leaderdz * leaderdz;

			if (cptr->followLeader) {
				if (leaderdistSq < (cptr->packDensity * 128 * 0.6) * (cptr->packDensity * 128 * 0.6))
				{
					cptr->followLeader = false;
					SetNewTargetPlace(cptr, 2048.f);
					goto TBEGIN;
				}
			}
			else {
				if (leaderdistSq > (cptr->packDensity * 128 * 1.3) * (cptr->packDensity * 128 * 1.3))
				{
					cptr->followLeader = true;
				}
			}

		}

		if (cptr->followLeader) {
			cptr->tgx = Packs[cptr->packId].leader->pos.x;
			cptr->tgz = Packs[cptr->packId].leader->pos.z;
		}
		else if (tdistSq < 456 * 456)
		{
			SetNewTargetPlace(cptr, 2048.f);
			goto TBEGIN;
		}
	}


	//============================================//

	if (cptr->NoFindCnt) cptr->NoFindCnt--;
	else
	{
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);

		if (cptr->State && pdistSq[0] > DinoInfo[cptr->CType].weaveRange * DinoInfo[cptr->CType].weaveRange && !DinoInfo[cptr->CType].dontWeave)
		{
			cptr->tgalpha += static_cast<float>(sin(RealTime / 824.f)) / 2.f;
			if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
			if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
		}
	}
	
	LookForAWay(cptr, !DinoInfo[cptr->CType].canSwim, true);
	if (cptr->NoWayCnt > 8)
	{
		cptr->NoWayCnt = 0;
		if (cptr->Clone == AI_GALL){
			cptr->NoFindCnt = 8 + rRand(40);
		} else {
			cptr->NoFindCnt = 8 + rRand(80);
		}
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

	if (NewPhase)


		if (!cptr->State)
		{

			//if (DinoInfo[cptr->CType].idleCount) {
			if (DinoInfo[cptr->CType].idleGroupCount
			//	&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
				&& !(cptr->StateF & csONWATER)) {

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

				/*
				bool idlePhase = false;
				for (int i = 0; i < DinoInfo[cptr->CType].idleCount; i++) {
					if (cptr->Phase == DinoInfo[cptr->CType].idleAnim[i]) idlePhase = true;
				}


				if (idlePhase) {
					if (rRand(128) > AIInfo[cptr->Clone].idleStart && cptr->Phase == DinoInfo[cptr->CType].idleAnim[DinoInfo[cptr->CType].idleCount - 1])
						cptr->Phase = DinoInfo[cptr->CType].walkAnim;
					else cptr->Phase = DinoInfo[cptr->CType].idleAnim[rRand(DinoInfo[cptr->CType].idleCount - 1)];
					goto ENDPSELECT;
				}
				if (rRand(128) > 120) cptr->Phase = DinoInfo[cptr->CType].idleAnim[0];
				else cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				*/
			} else cptr->Phase = DinoInfo[cptr->CType].walkAnim;

		}
		else cptr->Phase = DinoInfo[cptr->CType].runAnim;

	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->StateF & csONWATER) cptr->Phase = DinoInfo[cptr->CType].swimAnim;
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

	if (cptr->currentIdleGroup >= 0) goto SKIPROT;

	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.8f + drspd * 1.4f;
		else currspeed = -0.8f - drspd * 1.4f;
	else currspeed = 0;

	if (cptr->AfraidTime) currspeed *= 1.5;
	if (dalpha > pi) currspeed *= -1;
	if ((cptr->State & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim) currspeed /= 1.4f;

	DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 260.f);

	if (cptr->Clone == AI_GALL) {
		tgbend = drspd / 3;
	} else {
		tgbend = drspd / 2;
	}

	if (tgbend > pi / 2) tgbend = pi / 2;

	tgbend *= SGN(currspeed);
	if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
	else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 400.f);


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
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;

	if (DinoInfo[cptr->CType].canSwim) {
		if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) curspeed = DinoInfo[cptr->CType].swmspd;
	}

	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	//========== process speed =============//
	curspeed *= cptr->scale;
	DeltaFunc(cptr->vspeed, curspeed, TimeDt / 1024.f);

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
		ThinkY_Beta_Gamma(cptr, 64, 32, 0.7f, 0.4f);
	}

	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / 12.0f;
	else cptr->tggamma += cptr->rspeed / 8.0f;
	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
}



void AnimateFish(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;
	Vector3d _pos = cptr->pos;
	float _depth = cptr->depth;
	float _beta = cptr->beta;

TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targety = cptr->tdepth;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;
	float targetdy = targety - cptr->depth;

	float tdist2Sq = targetdx * targetdx + targetdz * targetdz;

	//float attackDist = 1024.f;
	//if (DinoInfo[cptr->CType].DangerFish) {
	//	attackDist = DinoInfo[cptr->CType].aggress;
	//}

	float playerdx = PlayerX - cptr->pos.x - cptr->lookx * 100 *cptr->scale;
	float playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 100 *cptr->scale;
	float pdistSq = playerdx * playerdx + playerdz * playerdz;

	if (pdistSq > ((ctViewR + 20) * 256) * ((ctViewR + 20) * 256)) {
		if (ReplaceCharacterForward(cptr)) {
			goto TBEGIN;
		}
	}

	//REMOVED - turny !!!
	//if (cptr->State == 2)
	//{
	//	NewPhase = true;
	//	cptr->State = 1;
	//}

	float tv;
	switch (cptr->Clone) {
	 case AI_FISH: tv = 1024.f;
	 case AI_MOSA: tv = 5024.f;
	}

	// JUMP & IDLE PARTICLES

	//int Scal = ((cptr->scale * 2) - 1);
	if (pdistSq < ((ctViewR + 20) * 256) * ((ctViewR + 20) * 256)) {	//Only create particles within player render distance
		if (DinoInfo[cptr->CType].partCnt[cptr->Phase]) {
			if (cptr->FTime > DinoInfo[cptr->CType].partFrame1[cptr->Phase] / cptr->pinfo->Animation[cptr->Phase].aniKPS
				&& cptr->FTime < DinoInfo[cptr->CType].partFrame2[cptr->Phase] / cptr->pinfo->Animation[cptr->Phase].aniKPS) {
				for (int i = 0; i < static_cast<int>(sqrt(DinoInfo[cptr->CType].partCnt[cptr->Phase]* ((cptr->scale * 3) - 2))); i++) {
					float xo = static_cast<int>(siRand(static_cast<int>(DinoInfo[cptr->CType].partDist[cptr->Phase])* cptr->scale)) + cptr->pos.x +
						((cos(cptr->alpha)  * ((cptr->scale * 1.5) - 0.5) * DinoInfo[cptr->CType].partOffset[cptr->Phase]));
					float zo = static_cast<int>(siRand(static_cast<int>(DinoInfo[cptr->CType].partDist[cptr->Phase]) * cptr->scale)) + cptr->pos.z +
						((sin(cptr->alpha)  * ((cptr->scale * 1.5) - 0.5) * DinoInfo[cptr->CType].partOffset[cptr->Phase]));
					AddElementsA(xo,
						GetLandUpH(xo, zo),
						zo,
						2,
						5,
						DinoInfo[cptr->CType].partMag[cptr->Phase],
						DinoInfo[cptr->CType].partAngled[cptr->Phase],
						cptr->alpha);
					if (DinoInfo[cptr->CType].partCircle[cptr->Phase]) AddWCircle(xo, zo, 1.2);
				}
			}
		}
	}


	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > 180 * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	bool playerInWater = GetLandUpH(PlayerX, PlayerZ) - GetLandH(PlayerX, PlayerZ) > 0;


	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto NOTHINK;

	//============================================//
	if (!MyHealth) cptr->State = 0;

	int ao = 0;
	if (DinoInfo[cptr->CType].DangerFish)ao = OptAgres;
	float attackDist = ctViewR * DinoInfo[cptr->CType].aggress + ao / AIInfo[cptr->Clone].agressMulti;

	if (!cptr->State)
	{

		bool attackmode = pdistSq <= attackDist * attackDist && playerInWater && !DinoInfo[cptr->CType].dontSwimAway
			&& MyHealth && !ObservMode && !DEBUG;
		if (g_GameMode == GameMode::SurvivalMode) attackmode = true;
		if (attackmode)	cptr->AfraidTime = static_cast<int>((10.f)) * 1024;
		if (cptr->packId >= 0 && MyHealth) {
			if (attackmode) Packs[cptr->packId].alert = true;
			if (Packs[cptr->packId]._alert) attackmode = true;
		}

		if (attackmode) {
			cptr->State = 1;
			cptr->turny = 0;
			cptr->lastTBeta = cptr->beta;
			//cptr->AfraidTime = static_cast<int>((10.f)) * 1024;
			//goto TBEGIN;
		} else {

			if (cptr->packId >= 0) {
				float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
				float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
				float leaderdistSq = leaderdx * leaderdx + leaderdz * leaderdz;


				if (cptr->followLeader) {
					if (leaderdistSq < (cptr->packDensity * 128 * 0.6) * (cptr->packDensity * 128 * 0.6))
					{
						cptr->followLeader = false;
						SetNewTargetPlaceFish(cptr, tv);
						goto TBEGIN;
					}
				}
				else {
					if (leaderdistSq > (cptr->packDensity * 128 * 1.3) * (cptr->packDensity * 128 * 1.3))
					{
						cptr->followLeader = true;
						cptr->turny = 0;
						cptr->lastTBeta = cptr->beta;
					}
				}

			}

			if (cptr->followLeader) {
				cptr->tgx = Packs[cptr->packId].leader->pos.x;
				cptr->tgz = Packs[cptr->packId].leader->pos.z;
				cptr->tdepth = Packs[cptr->packId].leader->depth;

			} else if (tdist2Sq < 456 * 456) // Ignore vertical
			{
				SetNewTargetPlaceFish(cptr, tv);
				goto TBEGIN;
			}
		}
	}

	if (cptr->State)
	{
		if (pdistSq > attackDist * attackDist || !playerInWater)
		{
			cptr->AfraidTime -= TimeDt;

			if (cptr->packId >= 0) {
				if (cptr->AfraidTime <= 0) {

					if (!Packs[cptr->packId]._alert) {
						cptr->AfraidTime = 0;
						cptr->State = 0;
						SetNewTargetPlaceFish(cptr, tv);
						goto TBEGIN;
					}

				} else Packs[cptr->packId].alert = true;
			} else if (cptr->AfraidTime <= 0) {
				cptr->AfraidTime = 0;
				cptr->State = 0;
				SetNewTargetPlaceFish(cptr, tv);
				goto TBEGIN;
			}




		}

		if (DinoInfo[cptr->CType].DangerFish || g_GameMode == GameMode::SurvivalMode) {
			cptr->tgx = PlayerX;
			cptr->tgz = PlayerZ;
			cptr->tgtime = 0;
			cptr->tdepth = PlayerY;


			// Mosa Target Depth Failsafes
			if (cptr->tdepth > GetLandUpH(cptr->tgx, cptr->tgz) - (cptr->spcDepth * 0.75)) {
				cptr->tdepth = GetLandUpH(cptr->pos.x, cptr->pos.z) - (cptr->spcDepth * 0.75);
			}

			//Target above the player so it can get to jumping depth in time.
			if (AIInfo[cptr->Clone].jumper) {
				if (cptr->depth < cptr->tdepth) {
					cptr->tdepth += (cptr->tdepth - cptr->depth) * 3;
					//float haw = (GetLandUpH(cptr->tgx, cptr->tgz) - GetLandH(cptr->tgx, cptr->tgz));
					//if (haw) cptr->tdepth *= (cptr->tdepth - GetLandH(cptr->tgx, cptr->tgz)) / haw;
				}
			}

			if (cptr->packId >= 0) {
				Packs[cptr->packId].alert = true;
			}

		}
		else
		{
			nv.x = playerdx;
			nv.z = playerdz;
			nv.y = 0;
			NormVector(nv, 2048.f);
			cptr->tgx = cptr->pos.x - nv.x;
			cptr->tgz = cptr->pos.z - nv.z;

			cptr->tdepth = GetLandH(cptr->pos.x, cptr->pos.z) +
				((GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z)) / 2);
		}

		cptr->tgtime = 0;

		if (cptr->Phase != DinoInfo[cptr->CType].jumpAnim){
			if (AIInfo[cptr->Clone].jumper && DinoInfo[cptr->CType].DangerFish) {
				if (cptr->depth > GetLandUpH(cptr->pos.x, cptr->pos.z) - (cptr->spcDepth * 0.95)){
					float pUp = PlayerY - GetLandUpH(PlayerX, PlayerZ); //jump later if the player is on a low bridge, not at all if too high
					if (pUp < 0) pUp = 0;
					float md = ((DinoInfo[cptr->CType].jumpRange * DinoInfo[cptr->CType].jmpspd) - (pUp * 1.3)) * cptr->scale;
					float jumpMin = md - 200;
					if (pdistSq < md * md && (jumpMin <= 0 || pdistSq > jumpMin * jumpMin))//1200
						if (AngleDifference(cptr->alpha, FindVectorAlpha(playerdx, playerdz)) < 0.2f) {

							Vector3d pv;
							pv.x = PlayerX;
							pv.z = PlayerZ;

							if (!CheckPlaceCollisionFish(cptr, pv, cptr->depth,
								DinoInfo[cptr->CType].maxDepth,
								DinoInfo[cptr->CType].minDepth)) {

								cptr->Phase = DinoInfo[cptr->CType].jumpAnim;
								NewPhase = true;
								cptr->FTime = 0;
								cptr->bend = 0;
								cptr->bdepth = 0;

							}

						}
				}
			}
		}

		if (pdistSq < (DinoInfo[cptr->CType].killDist * cptr->scale) * (DinoInfo[cptr->CType].killDist * cptr->scale) && DinoInfo[cptr->CType].killDist > 0) {
			float killAlt = cptr->spcDepth;
			if (killAlt < 256) killAlt = 256;
			if (AIInfo[cptr->Clone].jumper && cptr->Phase == DinoInfo[cptr->CType].jumpAnim) killAlt += 80;
			if (fabs(PlayerY - cptr->pos.y) < killAlt + 20 * cptr->scale)
			{

				if (DinoInfo[cptr->CType].killTypeCount > 0) {

					cptr->vspeed /= 8.0f;
					cptr->State = 1;
					cptr->Phase = DinoInfo[cptr->CType].killType[cptr->killType].anim;
					if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) cptr->FTime = 0;
					//cptr->FTime = 0;
					AddDeadBody(cptr,
						DinoInfo[cptr->CType].killType[cptr->killType].hunteranim,
						DinoInfo[cptr->CType].killType[cptr->killType].scream);
				}
				else {
					AddDeadBody(cptr, HUNT_EAT, true);
					cptr->State = 0;
				}

				cptr->aquaticIdle = false;

			}
		}
		

	}


NOTHINK:
	if (pdistSq < 2048 * 2048) cptr->NoFindCnt = 0;
	if (cptr->NoFindCnt) cptr->NoFindCnt--;
	else
	{
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);
		
		if (cptr->State && pdistSq > DinoInfo[cptr->CType].weaveRange * DinoInfo[cptr->CType].weaveRange && !DinoInfo[cptr->CType].dontWeave)
		{
			cptr->tgalpha += static_cast<float>(sin(RealTime / 824.f)) / 2.f;
			if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
			if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
		}
	}

	LookForAWay(cptr, false, true);
	if (cptr->NoWayCnt > 12)
	{
		cptr->NoWayCnt = 0;
		cptr->NoFindCnt = 16 + rRand(20);
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

		if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) {
			if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				cptr->State = 0;
			}
		}


		NewPhase = true;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount)  goto ENDPSELECT;

	if (AIInfo[cptr->Clone].jumper) {
		if (NewPhase && _Phase == DinoInfo[cptr->CType].jumpAnim)
		{
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
			goto ENDPSELECT;
		}

		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) goto ENDPSELECT;
	}

	/*
	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;
		NewPhase = true;

	}
	*/

	if (cptr->State) cptr->aquaticIdle = false;
	else if (DinoInfo[cptr->CType].lookCount > 0) {
		for (int i = 0; i < DinoInfo[cptr->CType].lookCount; i++) {
			if (NewPhase && _Phase == DinoInfo[cptr->CType].lookAnim[i]) {
				cptr->aquaticIdle = false;
			}
		}
	}

	if (NewPhase) {
		if (!cptr->State) {
			cptr->Phase = DinoInfo[cptr->CType].walkAnim;
			if (DinoInfo[cptr->CType].lookCount){
				if (!cptr->aquaticIdle && rRand(128) > AIInfo[cptr->Clone].idleStart
					&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
					) { // Don't play idles when carrying hunters corpse
					cptr->aquaticIdle = true;
				}

				if (cptr->aquaticIdle &&
					MyHealth && // Don't play idles when carrying hunters corpse
					cptr->depth > GetLandUpH(cptr->pos.x, cptr->pos.z) - (cptr->spcDepth * 0.8) &&
					fabs(cptr->beta) < pi / 32 &&
					fabs(cptr->gamma) < pi / 32 &&
					fabs(cptr->bend) < pi / 32) {

					cptr->Phase = DinoInfo[cptr->CType].lookAnim[rRand(DinoInfo[cptr->CType].lookCount - 1)];
					NewPhase = true;
					cptr->FTime = 0;
					goto ENDPSELECT;
				}

			}
		} else cptr->Phase = DinoInfo[cptr->CType].runAnim;

	}

	/*
	if (!cptr->State) cptr->Phase = DinoInfo[cptr->CType].walkAnim;
	else if (fabs(cptr->tgalpha - cptr->alpha) < 1.0 ||
		fabs(cptr->tgalpha - cptr->alpha) > 2 * pi - 1.0)
		cptr->Phase = DinoInfo[cptr->CType].runAnim;
	else cptr->Phase = DinoInfo[cptr->CType].walkAnim;
	*/

	//if (cptr->StateF & csONWATER) cptr->Phase = RAP_SWIM;
	//if (cptr->Slide > 40) cptr->Phase = RAP_SLIDE;


ENDPSELECT:

	//====== process phase changing ===========//
	if ((_Phase != cptr->Phase) || NewPhase)
	{

		/*
		bool idp = false;

		for (int i = 0; i < DinoInfo[cptr->CType].idleCount; i++) {
			if (cptr->Phase == DinoInfo[cptr->CType].idleAnim[i]) idp = true;
		}
		
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim || idp) {
			ActivateCharacterFx(cptr);
		} else {
			ActivateCharacterFxAquatic(cptr);
		}
		*/
		ActivateCharacterFxAquatic(cptr);
		if (cptr->Phase != DinoInfo[cptr->CType].walkAnim && cptr->Phase != DinoInfo[cptr->CType].runAnim) {
			ActivateCharacterFx(cptr);
		}



	}

	if (_Phase != cptr->Phase)
	{
		//==== set proportional FTime for better morphing =//
		//if (MORPHP)
		//	if (_Phase <= 3 && cptr->Phase <= 3)
		
		if ((_Phase == DinoInfo[cptr->CType].runAnim ||
			_Phase == DinoInfo[cptr->CType].walkAnim) &&
			(cptr->Phase == DinoInfo[cptr->CType].runAnim ||
				cptr->Phase == DinoInfo[cptr->CType].walkAnim)) {
			cptr->FTime = _FTime * cptr->pinfo->Animation[cptr->Phase].AniTime / cptr->pinfo->Animation[_Phase].AniTime + 64;
		}
		//else if (!NewPhase) cptr->FTime = 0;

		if (cptr->PPMorphTime > 128)
		{
			cptr->PrevPhase = _Phase;
			cptr->PrevPFTime = _FTime;
			cptr->PPMorphTime = 0;
		}
	}

	cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;



	//========== rotation to tgalpha ===================//

	//OLD BACKUP
	/*
		float rspd, currspeed, tgbend;
	float dalpha = static_cast<float>(fabs(cptr->tgalpha - cptr->alpha));
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;


	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;

	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.6f + drspd * 1.2f;
		else currspeed = -0.6f - drspd * 1.2f;
	else currspeed = 0;
	if (cptr->AfraidTime) currspeed *= 2.5;

	if (dalpha > pi) currspeed *= -1;
	if ((cptr->StateF & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim) currspeed /= 1.4f;

	if (cptr->AfraidTime) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 160.f);
	else DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 180.f);

	tgbend = drspd / 2;
	if (tgbend > pi / 5) tgbend = pi / 5;

	tgbend *= SGN(currspeed);
	if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
	else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 600.f);


	rspd = cptr->rspeed * TimeDt / 1024.f;
	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;
	*/

	float rspd, currspeed, tgbend;
	float dalpha = static_cast<float>(fabs(cptr->tgalpha - cptr->alpha));
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;

	if (AIInfo[cptr->Clone].jumper) {
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) goto SKIPROT;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;
		
	for (int i = 0; i < DinoInfo[cptr->CType].lookCount; i++) {
		if (cptr->Phase == DinoInfo[cptr->CType].lookAnim[i]) goto SKIPROT;
	}
	
	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.6f + drspd * 1.2f;
		else currspeed = -0.6f - drspd * 1.2f;
	else currspeed = 0;
	//if (cptr->AfraidTime) currspeed *= 2.5;

	if (dalpha > pi) currspeed *= -1;
	/*if ((cptr->StateF & csONWATER) || cptr->Phase == DinoInfo[cptr->CType].walkAnim) */currspeed /= 1.4f;

	if (cptr->AfraidTime) DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 250.f);
	else DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 460.f);

	tgbend = drspd / 2;
	if (tgbend > pi / 5) tgbend = pi / 5;

	tgbend *= SGN(currspeed);
	if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
	else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 600.f);


	rspd = cptr->rspeed * TimeDt / 1024.f;
	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

SKIPROT:

	/*
	//======= set slide mode ===========//
	if (!cptr->Slide && cptr->vspeed > 0.6 && cptr->Phase != RAP_JUMP)
		if (AngleDifference(cptr->tgalpha, cptr->alpha) > pi * 2 / 3.f)
		{
			cptr->Slide = static_cast<int>((cptr->vspeed*700.f));
			cptr->slidex = cptr->lookx;
			cptr->slidez = cptr->lookz;
			cptr->vspeed = 0;
		}
		*/


		//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;
	if (AIInfo[cptr->Clone].jumper) {
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) curspeed = DinoInfo[cptr->CType].jmpspd;
	}

	if (DinoInfo[cptr->CType].lookCount > 0) {
		for (int i = 0; i < DinoInfo[cptr->CType].lookCount; i++) {
			if (cptr->Phase == DinoInfo[cptr->CType].lookAnim[i]) {
				curspeed = DinoInfo[cptr->CType].wlkspd;
			}
		}
	}


	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) curspeed = 0.0f;

	/*
	if (cptr->Phase == RAP_RUN && cptr->Slide)
	{
		curspeed /= 8;
		if (drspd > pi / 2.f) curspeed = 0;
		else if (drspd > pi / 4.f) curspeed *= 2.f - 4.f*drspd / pi;
	}
	else*/ if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	//========== process speed =============//

	DeltaFunc(cptr->vspeed, curspeed, TimeDt / 500.f);

	if (AIInfo[cptr->Clone].jumper) {
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) cptr->vspeed = DinoInfo[cptr->CType].jmpspd;
	}

	MoveCharacterFish(cptr, cptr->lookx * cptr->vspeed * TimeDt * cptr->scale,
		cptr->lookz * cptr->vspeed * TimeDt * cptr->scale);

	/*
	//========== slide ==============//
	if (cptr->Slide)
	{
		MoveCharacter(cptr, cptr->slidex * cptr->Slide / 600.f * TimeDt * cptr->scale,
			cptr->slidez * cptr->Slide / 600.f * TimeDt * cptr->scale, false, true);

		cptr->Slide -= TimeDt;
		if (cptr->Slide < 0) cptr->Slide = 0;
	}
	*/

	//============ Y movement =================//


	float tdx2 = cptr->tgx - cptr->pos.x;
	float tdz2 = cptr->tgz - cptr->pos.z;
	float tdist22 = static_cast<float>(sqrt(tdx2 * tdx2 + tdz2 * tdz2)); //need this, it's an updated target dist

	float tbeta = -atan((cptr->tdepth - cptr->depth) / tdist22);

	if (cptr->turny < (pi)) {
		tbeta = (((cos(cptr->turny) + 1) / 2) * (cptr->lastTBeta - tbeta)) + tbeta;
		cptr->turny += pi / 100;
	}
	DeltaFunc(cptr->beta,tbeta, cptr->vspeed * TimeDt * cptr->scale*(pi/5000));

	if (cptr->Clone == AI_MOSA && cptr->Phase == DinoInfo[cptr->CType].walkAnim) {
		//cptr->depth -= cptr->beta * 10;
		cptr->depth -= cptr->beta * 25 * curspeed;

	} else {
		cptr->depth -= cptr->beta * 35 * curspeed;
	}

	float newBend = (_beta - cptr->beta) * 25;
	float max = 0.2;
	float maxIt = max / 6;

	if (fabs(cptr->bdepth - newBend) > maxIt) {
		if (newBend > cptr->bdepth) {
			cptr->bdepth += maxIt;
			//if (cptr->bdepth > max) cptr->bdepth = max; - see below
		}
		else {
			cptr->bdepth -= maxIt;
			//if (cptr->bdepth < -max) cptr->bdepth = -max; - see below
		}
	}
	else {
		cptr->bdepth = newBend;
	}
	if (cptr->bdepth > max) cptr->bdepth = max;
	if (cptr->bdepth < -max) cptr->bdepth = -max;

	/*
	if (cptr->beta < 0) cptr->beta += 2 * pi;
	if (cptr->beta > 2 * pi) cptr->beta -= 2 * pi;
	*/

	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) {
		cptr->tggamma = cptr->bend;
	}
	else {
		cptr->tggamma = cptr->bend * 2;	//run anim only
	}

	//=== process to tggamma ===//
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / 10.0f;
	else cptr->tggamma += cptr->rspeed / 8.0f;

	if (AIInfo[cptr->Clone].jumper) {
		if (cptr->Phase == DinoInfo[cptr->CType].jumpAnim) cptr->tggamma = 0;
	}

	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1624.f);


	// Mosa Depth Failsafes
	if (cptr->depth > GetLandUpH(cptr->pos.x, cptr->pos.z) - (cptr->spcDepth / 2)) {
		cptr->depth = GetLandUpH(cptr->pos.x, cptr->pos.z) - (cptr->spcDepth / 2);
		cptr->tdepth = GetLandH(cptr->pos.x, cptr->pos.z) +
			((GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z)) / 2);
		cptr->lastTBeta = cptr->beta;
	}
	if (cptr->depth < GetLandH(cptr->pos.x, cptr->pos.z) + (cptr->spcDepth / 2)) {
		cptr->depth = GetLandH(cptr->pos.x, cptr->pos.z) + (cptr->spcDepth / 2);
		cptr->tdepth = GetLandH(cptr->pos.x, cptr->pos.z) +
			((GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z)) / 2);
		cptr->lastTBeta = cptr->beta;
	}

	//==================================================//

	cptr->pos.y = cptr->depth;

}



void AnimateIcth(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;
	if (cptr->AfraidTime) cptr->AfraidTime = MAX(0, cptr->AfraidTime - TimeDt);


	bool alertInit = false;
	if (cptr->State == 2) alertInit = true;
	if (cptr->packId >= 0) {
		if (!cptr->State && Packs[cptr->packId]._alert) alertInit = true;
	}

	if (alertInit) {
		NewPhase = true;
		cptr->State = 1;
	}

	cptr->FTime += TimeDt;

TBEGIN:
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdistSq = targetdx * targetdx + targetdz * targetdz;

	float playerdx = PlayerX - cptr->pos.x;
	float playerdz = PlayerZ - cptr->pos.z;
	float pdistSq = playerdx * playerdx + playerdz * playerdz;
	float playerdy = PlayerY - cptr->pos.y;
	float pdistUpSq = pdistSq + playerdy * playerdy;

	//	if (cptr->AfraidTime && !(_Phase == ICTH_FLY || _Phase == ICTH_LANDING || _Phase == ICTH_FLY2 || _Phase == ICTH_TAKEOFF || _Phase == ICTH_WINGUP_WATER || _Phase == ICTH_WINGUP_LAND))
	//	{
	//		cptr->wingUp = true;
	//	}

	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > 20)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	//=========== run away =================//
	if (cptr->State)
	{

		cptr->currentIdleGroup = -1;
		cptr->currentIdle2Group = -1;

		bool relax = false;
		if (cptr->packId >= 0) {
			if (!cptr->AfraidTime) {
				if (!Packs[cptr->packId]._alert) relax = true;
			} else Packs[cptr->packId].alert = true;
		} else if (!cptr->AfraidTime) relax = true;

		if (relax)
		{
			if (cptr->pos.y >= GetLandUpH(cptr->pos.x, cptr->pos.z) + 236)
			{
				cptr->gliding = true;
				SetNewTargetPlace_Icth(cptr, 2048.f);
			}
			else
			{
				cptr->Phase = DinoInfo[cptr->CType].landAnim;
				NewPhase = true;
				SetNewTargetPlace_Icth(cptr, 2048.f);
			}
			cptr->State = 0;
			goto TBEGIN;
		}

	}


	if (pdistSq > ((ctViewR + 20) * 256) * ((ctViewR + 20) * 256))
		if (ReplaceCharacterForward(cptr)) goto TBEGIN;

	//======== exploring area ===============//
	if (!cptr->State)
	{
		cptr->AfraidTime = 0;
		if (pdistUpSq < 1050.f * 1050.f)
		{
			cptr->State = 1;
			SetNewTargetPlace_Icth(cptr, 2048.f);
			cptr->AfraidTime = (50 + rRand(8)) * 1024;
			NewPhase = true;
			if (cptr->packId >= 0) Packs[cptr->packId].alert = true;
			goto TBEGIN;
		}

	}

	int targetNear = 456;

	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim) {
		targetNear = 2024;
	}

	
	if (cptr->packId >= 0) {
		float leaderdx = Packs[cptr->packId].leader->pos.x - cptr->pos.x;
		float leaderdz = Packs[cptr->packId].leader->pos.z - cptr->pos.z;
		float leaderdistSq = leaderdx * leaderdx + leaderdz * leaderdz;

		if (cptr->followLeader) {
			if (leaderdistSq < (cptr->packDensity * 128 * 0.6) * (cptr->packDensity * 128 * 0.6))
			{
				cptr->followLeader = false;
				SetNewTargetPlace_Icth(cptr, 4048.f);
				goto TBEGIN;
			}
		}
		else {
			if (leaderdistSq > (cptr->packDensity * 128 * 1.3) * (cptr->packDensity * 128 * 1.3))
			{
				cptr->followLeader = true;
			}
		}

	}

	if (cptr->followLeader) {
		cptr->tgx = Packs[cptr->packId].leader->pos.x;
		cptr->tgz = Packs[cptr->packId].leader->pos.z;
	}
	else if (tdistSq < targetNear * targetNear)
	{
		SetNewTargetPlace_Icth(cptr, 2048.f);
		goto TBEGIN;
	}
	


	//===============================================//

	ProcessPrevPhase(cptr);



	//======== select new phase =======================//


	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;
		NewPhase = true;
	}

	float wy = GetLandUpH(cptr->pos.x,
		cptr->pos.z) - GetLandH(cptr->pos.x,
			cptr->pos.z);
	float swimLevel = DinoInfo[cptr->CType].waterLevel * cptr->scale;// 40;

	if (NewPhase)
	{
		if (!cptr->State)
		{

			if (cptr->gliding == true)
			{
				cptr->Phase = DinoInfo[cptr->CType].glideAnim;
			}
			else if (cptr->Phase != DinoInfo[cptr->CType].landAnim)
			{
				if (wy >= swimLevel) {
					

					if (DinoInfo[cptr->CType].idle2GroupCount) {

						if (cptr->currentIdle2Group >= 0) {
							if (rRand(127) + 1 > (1 - DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].end) * 128
								&& (DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].endOnAny
									|| cptr->Phase == DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].anim[DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].count - 1])) {
								cptr->Phase = DinoInfo[cptr->CType].swimAnim;
								if (DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].instantRepeat) {
									cptr->currentIdle2Group = -1; //this must be done inside the if statement
								}
								else {
									cptr->currentIdle2Group = -1; //this must be done inside the if statement
									goto ENDPSELECT;
								}
							}
							else {
								cptr->Phase = DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].anim[rRand(DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].count - 1)];
								goto ENDPSELECT;
							}
						}

						for (int idle2GroupNo = 0; idle2GroupNo < DinoInfo[cptr->CType].idle2GroupCount; idle2GroupNo++) {
							if (rRand(127) + 1 > (1 - DinoInfo[cptr->CType].idle2Group[idle2GroupNo].start) * 128) cptr->currentIdle2Group = idle2GroupNo;
						}
						if (cptr->currentIdle2Group >= 0) {
							if (DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].startOnAny)
								cptr->Phase = DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].anim[rRand(DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].count - 1)];
							else
								cptr->Phase = DinoInfo[cptr->CType].idle2Group[cptr->currentIdle2Group].anim[0];
							goto ENDPSELECT;
						}
						else cptr->Phase = DinoInfo[cptr->CType].swimAnim;

					}
					else cptr->Phase = DinoInfo[cptr->CType].swimAnim;


				}
				else
				{
					

					if (DinoInfo[cptr->CType].idleGroupCount) {

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
					else {
						cptr->Phase = DinoInfo[cptr->CType].walkAnim;
					}
					
				}
			}

		}
		
		if (cptr->State) {

			bool afraid = false;
			if (cptr->AfraidTime) afraid = true;

			if (cptr->packId >= 0) {
				if (Packs[cptr->packId]._alert) afraid = true;
			}


			if (afraid) {

				if (cptr->Phase == DinoInfo[cptr->CType].flyAnim)
				{
					if (cptr->pos.y > GetLandUpH(cptr->pos.x, cptr->pos.z) + 2100)
					{
						cptr->Phase = DinoInfo[cptr->CType].glideAnim;
						SetNewTargetPlace_Icth(cptr, 2048.f);
					}
				}
				else if (cptr->Phase == DinoInfo[cptr->CType].glideAnim)
				{
					if (cptr->pos.y < GetLandUpH(cptr->pos.x, cptr->pos.z) + 1600)
					{
						cptr->Phase = DinoInfo[cptr->CType].flyAnim;
						SetNewTargetPlace_Icth(cptr, 2048.f);
					}
				}
				else if (cptr->Phase == DinoInfo[cptr->CType].takeoffAnim)
				{
					if (cptr->pos.y > GetLandUpH(cptr->pos.x, cptr->pos.z) + 236)
					{
						cptr->Phase = DinoInfo[cptr->CType].flyAnim;
					}
				}
				else
				{
					cptr->Phase = DinoInfo[cptr->CType].takeoffAnim;
					if (cptr->notFlushed == false)
					{
						ActivateCharacterFx(cptr);
					}
					else
					{
						cptr->notFlushed = false;
					}

					cptr->gamma = 0;
					cptr->beta = 0;
					cptr->bend = 0;//?
				}


			}
			else {
				if (cptr->gliding == true)
				{
					cptr->Phase = DinoInfo[cptr->CType].glideAnim;
				}
				else if (cptr->Phase != DinoInfo[cptr->CType].landAnim)
				{
					if (wy >= swimLevel) cptr->Phase = DinoInfo[cptr->CType].swimAnim;
					else cptr->Phase = DinoInfo[cptr->CType].walkAnim;

				}
			}

			if (cptr->currentIdleGroup >= 0 || cptr->currentIdle2Group >= 0) {
						if (rRand(24) > 23)
						{
							cptr->State = 1;
							SetNewTargetPlace_Icth(cptr, 2048.f);
							cptr->AfraidTime = (50 + rRand(8)) * 1024;
							cptr->notFlushed = true;
							NewPhase = true;
							goto TBEGIN;
						}
			}

		}



	}

	if (cptr->gliding == true) {
		if (cptr->pos.y <= GetLandUpH(cptr->pos.x, cptr->pos.z) + 236)
		{
			cptr->gliding = false;
			cptr->Phase = DinoInfo[cptr->CType].landAnim;
			NewPhase = true;
			goto TBEGIN;
		}
	}

	if (cptr->Phase == DinoInfo[cptr->CType].landAnim) {
		if (cptr->pos.y <= GetLandUpH(cptr->pos.x, cptr->pos.z) + 15)
		{
			if (cptr->StateF & csONWATER)
			{
				cptr->Phase = DinoInfo[cptr->CType].shakeWaterAnim;
			}
			else
			{
				cptr->Phase = DinoInfo[cptr->CType].shakeLandAnim;
			}
			//TODO Set beta/gamma and such on land? - might be better to set it further down?
		}
		else if (cptr->pos.y > GetLandUpH(cptr->pos.x, cptr->pos.z) + 256)
		{
			cptr->gliding = true;
			NewPhase = true;
			goto TBEGIN;
		}
	}

	
	if (wy >= swimLevel)
	{
		if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) {
			NewPhase = true;
			goto TBEGIN;
		}

		if (cptr->currentIdleGroup >= 0) {
					NewPhase = true;
					cptr->currentIdleGroup = -1;
					goto TBEGIN;
		}
	}


	if (!(wy >= swimLevel))
	{
		if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) {
			NewPhase = true;
			goto TBEGIN;
		}

		if (cptr->currentIdle2Group >= 0) {
					NewPhase = true;
					cptr->currentIdle2Group = -1;
					goto TBEGIN;
		}
	}
	


	//LAST


	if (NewPhase)
	{

		if (cptr->Phase == DinoInfo[cptr->CType].walkAnim || cptr->currentIdleGroup >= 0)
		{
			if (cptr->shakeTime < 9)
			{
				cptr->shakeTime = cptr->shakeTime + 1;
			}

			if (cptr->shakeTime == 8)
			{
				cptr->Phase = DinoInfo[cptr->CType].shakeLandAnim;
			}
		}
		else
		{
			cptr->shakeTime = 0;
		}
	}



	//============================================//

	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim
		|| cptr->Phase == DinoInfo[cptr->CType].takeoffAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim) {
		cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);
	}
	else
	{

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


		LookForAWay(cptr, false, true);
		if (cptr->NoWayCnt > 12)
		{
			cptr->NoWayCnt = 0;
			cptr->NoFindCnt = 32 + rRand(60);
		}

	}

	if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
	if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;

ENDPSELECT:

	//====== process phase changing ===========//

	if ((_Phase != cptr->Phase) || NewPhase)
	{
		if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim)
		{

			if ((rand() & 1023) > 880)
			{
				ActivateCharacterFx(cptr);
			}
		}
		else if (cptr->Phase != DinoInfo[cptr->CType].takeoffAnim)
		{
			ActivateCharacterFx(cptr);
		}

	}

	if (_Phase != cptr->Phase)
	{

		if((_Phase == DinoInfo[cptr->CType].walkAnim || _Phase == DinoInfo[cptr->CType].swimAnim || _Phase == DinoInfo[cptr->CType].flyAnim
			|| _Phase == DinoInfo[cptr->CType].glideAnim || _Phase == DinoInfo[cptr->CType].landAnim || _Phase == DinoInfo[cptr->CType].takeoffAnim)
			&&
			(cptr->Phase == DinoInfo[cptr->CType].walkAnim || cptr->Phase == DinoInfo[cptr->CType].swimAnim || cptr->Phase == DinoInfo[cptr->CType].flyAnim
				|| cptr->Phase == DinoInfo[cptr->CType].glideAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim || cptr->Phase == DinoInfo[cptr->CType].takeoffAnim))
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

	if (cptr->currentIdleGroup >= 0) {
				goto SKIPROT;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].shakeLandAnim) goto SKIPROT;

	if (drspd > 0.02)
		if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim
			|| cptr->Phase == DinoInfo[cptr->CType].takeoffAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim)
		{
			if (cptr->tgalpha > cptr->alpha) currspeed = 0.6f + drspd * 1.2f;
			else currspeed = -0.6f - drspd * 1.2f;
		}
		else
		{
			if (cptr->tgalpha > cptr->alpha) currspeed = 0.2f + drspd * 1.0f;
			else currspeed = -0.2f - drspd * 1.0f;
		}
	else currspeed = 0;

	//if (cptr->AfraidTime) currspeed *= 1.5;
	if (dalpha > pi) currspeed *= -1;


	if (cptr->currentIdle2Group >= 0) {
				currspeed /= 1.4f;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].swimAnim || cptr->Phase == DinoInfo[cptr->CType].shakeWaterAnim) currspeed /= 1.4f;

	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim
		|| cptr->Phase == DinoInfo[cptr->CType].takeoffAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim)
	{
		DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 460.f);
	}
	else
	{
		DeltaFunc(cptr->rspeed, currspeed, static_cast<float>(TimeDt) / 400.f);
	}

	tgbend = drspd / 2.f;
	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim
		|| cptr->Phase == DinoInfo[cptr->CType].takeoffAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim)
	{
		if (tgbend > pi / 2) tgbend = pi / 2;
	}
	else
	{
		if (tgbend > pi / 3.f) tgbend = pi / 3.f;
	}

	tgbend *= SGN(currspeed);
	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim
		|| cptr->Phase == DinoInfo[cptr->CType].takeoffAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim)
	{
		if (fabs(tgbend) > fabs(cptr->bend)) DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 800.f);
		else DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 400.f);

		rspd = cptr->rspeed * TimeDt / 1024.f;
	}
	else
	{
		DeltaFunc(cptr->bend, tgbend, static_cast<float>(TimeDt) / 2000.f);

		rspd = cptr->rspeed * TimeDt / 612.f;
	}

	if (drspd < fabs(rspd)) cptr->alpha = cptr->tgalpha;
	else cptr->alpha += rspd;


	if (cptr->alpha > pi * 2) cptr->alpha -= pi * 2;
	if (cptr->alpha < 0) cptr->alpha += pi * 2;

SKIPROT:

	//========== movement ==============================//
	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	float curspeed = 0;
	/*
	if (cptr->Phase == ICTH_FLY) curspeed = cptr->speed_fly;//2.00f;
	if (cptr->Phase == ICTH_FLY2) curspeed = cptr->speed_glide;//1.80f;
	if (cptr->Phase == ICTH_TAKEOFF) curspeed = cptr->speed_takeoff;// 1.50f;
	if (cptr->Phase == ICTH_LANDING) curspeed = cptr->speed_land;// 0.30f;
	if (cptr->Phase == ICTH_WALK) curspeed = cptr->speed_walk;//0.10f;
	if (cptr->Phase == ICTH_SWIM) curspeed = cptr->speed_swim;//0.15f;
	if (cptr->Phase == ICTH_SWIM_IDLE1) curspeed = cptr->speed_swim;//0.15f;
	if (cptr->Phase == ICTH_SWIM_IDLE2) curspeed = cptr->speed_swim;//0.15f;
	if (cptr->Phase == ICTH_WINGDOWN_WATER) curspeed = cptr->speed_swim;//0.15f;
	*/
	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim) curspeed = DinoInfo[cptr->CType].flyspd;
	if (cptr->Phase == DinoInfo[cptr->CType].glideAnim) curspeed = DinoInfo[cptr->CType].gldspd;
	if (cptr->Phase == DinoInfo[cptr->CType].takeoffAnim) curspeed = DinoInfo[cptr->CType].tkfspd;
	if (cptr->Phase == DinoInfo[cptr->CType].landAnim) curspeed = DinoInfo[cptr->CType].lndspd;
	if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) curspeed = DinoInfo[cptr->CType].wlkspd;
	if (cptr->Phase == DinoInfo[cptr->CType].swimAnim) curspeed = DinoInfo[cptr->CType].swmspd;
	if (cptr->Phase == DinoInfo[cptr->CType].shakeWaterAnim) curspeed = DinoInfo[cptr->CType].swmspd;

	if (cptr->currentIdle2Group >= 0) {
				curspeed = DinoInfo[cptr->CType].swmspd;
	}

	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;



	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim
		|| cptr->Phase == DinoInfo[cptr->CType].takeoffAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim)
	{
		if (cptr->Phase == DinoInfo[cptr->CType].flyAnim)
			DeltaFunc(cptr->pos.y, GetLandUpH(cptr->pos.x, cptr->pos.z) + 4048, TimeDt / 6.f);

		if (cptr->Phase == DinoInfo[cptr->CType].glideAnim)
		{
			if (cptr->gliding == true)
			{
				DeltaFunc(cptr->pos.y, GetLandUpH(cptr->pos.x, cptr->pos.z), TimeDt / 8.f);
			}
			else
			{
				DeltaFunc(cptr->pos.y, GetLandUpH(cptr->pos.x, cptr->pos.z), TimeDt / 16.f);
			}
		}

		if (cptr->Phase == DinoInfo[cptr->CType].takeoffAnim)
			DeltaFunc(cptr->pos.y, GetLandUpH(cptr->pos.x, cptr->pos.z) + 4048, TimeDt / 5.f);

		if (cptr->Phase == DinoInfo[cptr->CType].landAnim)
			DeltaFunc(cptr->pos.y, GetLandUpH(cptr->pos.x, cptr->pos.z), TimeDt / 4.f);


		if (cptr->gliding == false)
		{
			if (cptr->Phase != DinoInfo[cptr->CType].landAnim && cptr->Phase != DinoInfo[cptr->CType].takeoffAnim) {
				if (cptr->pos.y < GetLandUpH(cptr->pos.x, cptr->pos.z) + 236)
					cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) + 256;
			}
			else {
				if (cptr->pos.y < GetLandUpH(cptr->pos.x, cptr->pos.z))
					cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z);
			}

		}

	}



	//========== process speed =============//

	bool swimmingAnim = false;
	if (cptr->Phase == DinoInfo[cptr->CType].swimAnim || DinoInfo[cptr->CType].shakeWaterAnim) swimmingAnim = true;
	if (cptr->currentIdle2Group >= 0) {
				swimmingAnim = true;
	}

	curspeed *= cptr->scale;

	if (cptr->Phase == DinoInfo[cptr->CType].flyAnim || cptr->Phase == DinoInfo[cptr->CType].glideAnim
		|| cptr->Phase == DinoInfo[cptr->CType].takeoffAnim || cptr->Phase == DinoInfo[cptr->CType].landAnim)
	{
		DeltaFunc(cptr->vspeed, curspeed, TimeDt / 2024.f);

		cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
		cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

		
		cptr->tggamma = cptr->rspeed / 4.0f;
		if (cptr->tggamma > pi / 6.f) cptr->tggamma = pi / 6.f;
		if (cptr->tggamma < -pi / 6.f) cptr->tggamma = -pi / 6.f;
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
		
	}
	else
	{
		if (curspeed > cptr->vspeed) DeltaFunc(cptr->vspeed, curspeed, TimeDt / 1024.f);
		else DeltaFunc(cptr->vspeed, curspeed, TimeDt / 256.f);

		MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt,
			cptr->lookz * cptr->vspeed * TimeDt, false, true);

		if (!swimmingAnim)
		{
			ThinkY_Beta_Gamma(cptr, 128, 64, 0.6f, 0.4f);
			if (cptr->Phase == DinoInfo[cptr->CType].walkAnim) cptr->tggamma += cptr->rspeed / 16.0f;
			else cptr->tggamma += cptr->rspeed / 10.0f;

			DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
		}
		else {
			cptr->gamma = 0;
		}
	}


	/*
	if (swimmingAnim)
	{
		cptr->gamma = 0;
	}
	else
	{
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 2048.f);
	}
	*/



	//============ Y movement =================//
	if ((wy >= swimLevel) && cptr->Phase != DinoInfo[cptr->CType].flyAnim && cptr->Phase != DinoInfo[cptr->CType].glideAnim
		&& cptr->Phase != DinoInfo[cptr->CType].takeoffAnim && cptr->Phase != DinoInfo[cptr->CType].landAnim)
	{
		cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - (DinoInfo[cptr->CType].waterLevel * cptr->scale);
		//cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - 20;
		cptr->beta /= 2;
		cptr->tggamma = 0;
	}

}



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




void AnimateIcthDead(TCharacter *cptr)
{
	cptr->bend = 0;

	if (cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].fall && cptr->Phase != DinoInfo[cptr->CType].deathType[cptr->deathType].die
		&& !(cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].sleep && cptr->Clone == AI_ICTH)
		&& !(cptr->Phase == cptr->waterDieAnim && DinoInfo[cptr->CType].waterDieCount))
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
		ActivateCharacterFx(cptr);
		return;
	}

	ProcessPrevPhase(cptr);

	float wh = GetLandUpH(cptr->pos.x, cptr->pos.z);
	float lh = GetLandH(cptr->pos.x, cptr->pos.z);
	BOOL OnWaterQ = (wh > lh);
	if (!DinoInfo[cptr->CType].waterDieCount) OnWaterQ = false;

	cptr->FTime += TimeDt;
	if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
	{
		if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].die ||
			(cptr->Phase == cptr->waterDieAnim && DinoInfo[cptr->CType].waterDieCount) ||
			(cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].sleep && cptr->Clone == AI_ICTH))
		{
			if (cptr->canSleep)
			{
				cptr->FTime = 0;
				cptr->Phase = DinoInfo[cptr->CType].deathType[cptr->deathType].sleep;
				ActivateCharacterFx(cptr);
			}
			else
			{
				cptr->FTime = cptr->pinfo->Animation[cptr->Phase].AniTime - 1;
			}
		}
		else
			cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;


	}


	//======= movement ===========//
	if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].die || 
		(cptr->Phase == cptr->waterDieAnim && DinoInfo[cptr->CType].waterDieCount) || 
		(cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].sleep && cptr->Clone == AI_ICTH))
		DeltaFunc(cptr->vspeed, 0, TimeDt / 400.f);
	else
		DeltaFunc(cptr->vspeed, 0, TimeDt / 1200.f);

	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;

	if (cptr->Phase == DinoInfo[cptr->CType].deathType[cptr->deathType].fall)
	{
		if (OnWaterQ)
			if (cptr->pos.y >= wh && (cptr->pos.y + cptr->rspeed * TimeDt / 1024) < wh)
			{
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 2.0);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 2.5);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 3.0);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 3.5);
				AddWCircle(cptr->pos.x + siRand(128), cptr->pos.z + siRand(128), 3.0);
			}
		cptr->pos.y += cptr->rspeed * TimeDt / 1024;
		cptr->rspeed -= TimeDt * 2.56;

		if (cptr->pos.y <= wh)
		{
			cptr->pos.y = wh;

			if (cptr->PPMorphTime > 128)
			{
				cptr->PrevPhase = cptr->Phase;
				cptr->PrevPFTime = cptr->FTime;
				cptr->PPMorphTime = 0;
			}

			if (OnWaterQ)
			{
				//				AddElements(cptr->pos.x + siRand(128), lh, cptr->pos.z + siRand(128), 4, 10);
				//				AddElements(cptr->pos.x + siRand(128), lh, cptr->pos.z + siRand(128), 4, 10);
				//				AddElements(cptr->pos.x + siRand(128), lh, cptr->pos.z + siRand(128), 4, 10);
				cptr->Phase = cptr->waterDieAnim;
			}
			else
			{
				cptr->Phase = DinoInfo[cptr->CType].deathType[cptr->deathType].die;
			}
			cptr->FTime = 0;
			ActivateCharacterFx(cptr);
		}

		cptr->canSleep = (Tranq && !OnWaterQ && cptr->Clone == AI_ICTH &&
			(cptr->deathPhase == DinoInfo[cptr->CType].walkAnim || cptr->deathPhase == DinoInfo[cptr->CType].shakeLandAnim || cptr->currentIdleGroup >= 0));

	}
	else
	{
		ThinkY_Beta_Gamma(cptr, 140, 126, 0.6f, 0.5f);
		DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 1600.f);
	}

	if (DinoInfo[cptr->CType].waterDieCount && cptr->Phase == cptr->waterDieAnim)
	{
		cptr->pos.y = wh;
		cptr->gamma = 0;
		cptr->beta = 0;
		cptr->alpha = 0;
	}
}








 
//NEW BRAHI
void AnimateBrahi(TCharacter *cptr)
{
	NewPhase = false;
	int _Phase = cptr->Phase;
	int _FTime = cptr->FTime;
	float _tgalpha = cptr->tgalpha;

	bool autoCorrect = false;

TBEGIN:
	//cptr->tgtime = 0;
	float targetx = cptr->tgx;
	float targetz = cptr->tgz;
	float targetdx = targetx - cptr->pos.x;
	float targetdz = targetz - cptr->pos.z;

	float tdist = static_cast<float>(sqrt(targetdx * targetdx + targetdz * targetdz));

	float playerdx = PlayerX - cptr->pos.x - cptr->lookx * 108;
	float playerdz = PlayerZ - cptr->pos.z - cptr->lookz * 108;
	float pdist = static_cast<float>(sqrt(playerdx * playerdx + playerdz * playerdz));

	int attackDist = 128 * DinoInfo[cptr->CType].aggress + OptAgres / 8; //agress = 56

	bool playerAttackable = ((GetLandUpH(PlayerX, PlayerZ) - GetLandH(PlayerX, PlayerZ)) <= 550);
	bool attacking = false;

	bool alertInit = false;
	if (cptr->State == 2) alertInit = true;
	if (cptr->packId >= 0) {
		if (!cptr->State && Packs[cptr->packId]._alert) alertInit = true;
	}

	if (alertInit) {
		NewPhase = true;
		cptr->State = 1;
	}

	if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > 140 * cptr->scale)
		cptr->StateF |= csONWATER;
	else
		cptr->StateF &= (!csONWATER);

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto NOTHINK;

	//============================================//
	if (!MyHealth) cptr->State = 0;
	if (cptr->State)
	{

		cptr->currentIdleGroup = -1;

		bool fleeMode = false;
		if (g_GameMode != GameMode::SurvivalMode) {
			if (pdist > attackDist || !playerAttackable || DinoInfo[cptr->CType].aggress <= 0 || !cptr->awareHunter) {
				fleeMode = true;
			}
			else if (DinoInfo[cptr->CType].defensive && cptr->Health == DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearShot && cptr->Health < DinoInfo[cptr->CType].Health0) fleeMode = true;
			else if (DinoInfo[cptr->CType].fearHearShot && cptr->heardShot) fleeMode = true;
			else if (cptr->packId >= 0) Packs[cptr->packId].attack = true;
		}

		if (cptr->packId >= 0) {
			if (Packs[cptr->packId]._attack) fleeMode = false;
		}

		if (!autoCorrect) {
			if (GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z) > 550) {
				autoCorrect = true;
				SetNewTargetPlace_Brahi(cptr, 2048.f);
				goto TBEGIN;
			}
			else if (!fleeMode)
			{
				attacking = true;
				cptr->tgx = PlayerX;
				cptr->tgz = PlayerZ;
				cptr->tgtime = 0;
				if (cptr->packId >= 0) {
					Packs[cptr->packId].alert = true;
				}
			}
			else
			{
				attacking = false;
				nv.x = playerdx;
				nv.z = playerdz;
				nv.y = 0;
				NormVector(nv, 2048.f);
				cptr->tgx = cptr->pos.x - nv.x;
				cptr->tgz = cptr->pos.z - nv.z;
				cptr->tgtime = 0;
				cptr->AfraidTime -= TimeDt;


				if (cptr->packId >= 0) {
					if (cptr->AfraidTime <= 0)
					{
						if (!Packs[cptr->packId]._alert) {
							cptr->AfraidTime = 0;
							cptr->State = 0;
						}
					}
					else Packs[cptr->packId].alert = true;
				}
				else if (cptr->AfraidTime <= 0) {
					cptr->AfraidTime = 0;
					cptr->State = 0;
				}

			}
		}

		if (pdist < DinoInfo[cptr->CType].killDist && DinoInfo[cptr->CType].killDist > 0) //killdist = 600
			if (fabs(PlayerY - cptr->pos.y - 120) < 256)
			{

				if (DinoInfo[cptr->CType].killTypeCount > 0) {

					if (!(cptr->StateF & csONWATER))
					{
						cptr->vspeed /= 8.0f;
						cptr->State = 1;
						cptr->Phase = DinoInfo[cptr->CType].killType[cptr->killType].anim;
						if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) cptr->FTime = 0;
						AddDeadBody(cptr,
							DinoInfo[cptr->CType].killType[cptr->killType].hunteranim,
							DinoInfo[cptr->CType].killType[cptr->killType].scream);
					}
					else AddDeadBody(cptr, HUNT_EAT, true);

				}
				else {
					AddDeadBody(cptr, HUNT_EAT, true);
					cptr->State = 0;
				}


			}
	}

	if (pdist > (ctViewR + 20) * 256)
		if (ReplaceCharacterForward(cptr)) goto TBEGIN;

	if (!cptr->State)
	{
		attacking = false;
		cptr->AfraidTime = 0;


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
	}

NOTHINK:
	
	if ((cptr->Clone == AI_LANDBRACH || cptr->State) && !autoCorrect) {
		if (pdist < 2048) cptr->NoFindCnt = 0;
		if (cptr->NoFindCnt) cptr->NoFindCnt--;
		else
		{
			cptr->tgalpha = CorrectedAlpha(FindVectorAlpha(targetdx, targetdz), cptr->alpha);//FindVectorAlpha(targetdx, targetdz);

			if (cptr->State && pdist > DinoInfo[cptr->CType].weaveRange && !DinoInfo[cptr->CType].dontWeave)
			{
				cptr->tgalpha += static_cast<float>(sin(RealTime / 824.f)) / 4.f;
				if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
				if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
			}


		}

		if (cptr->Clone == AI_LANDBRACH) {
			LookForAWay(cptr, !DinoInfo[cptr->CType].canSwim, true);
		}
		else {
			LookForAWay(cptr, true, true);
		}


		if (cptr->NoWayCnt > 12)
		{
			cptr->NoWayCnt = 0;
			cptr->NoFindCnt = 16 + rRand(20);
		}
	} else {
		cptr->tgalpha = FindVectorAlpha(targetdx, targetdz);

		if (cptr->State && pdist > DinoInfo[cptr->CType].weaveRange && !DinoInfo[cptr->CType].dontWeave)
		{
			cptr->tgalpha += static_cast<float>(sin(RealTime / 824.f)) / 4.f;
			if (cptr->tgalpha < 0) cptr->tgalpha += 2 * pi;
			if (cptr->tgalpha > 2 * pi) cptr->tgalpha -= 2 * pi;
		}
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

		if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) {
			if (DinoInfo[cptr->CType].killType[cptr->killType].dontloop) {
				cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				cptr->State = 0;
			}
		}

		NewPhase = true;
	}

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount)  goto ENDPSELECT;

	if (NewPhase)
	{
		if (!cptr->State)
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



				/*
				if (cptr->Phase != DinoInfo[cptr->CType].walkAnim)
				{
					if (rRand(128) > 90)
					{
						cptr->Phase = DinoInfo[cptr->CType].walkAnim;
					}
					else
					{
						cptr->Phase = DinoInfo[cptr->CType].idleAnim[rRand(DinoInfo[cptr->CType].idleCount - 1)];
					}
					goto ENDPSELECT;
				}
				if (rRand(128) > 0
					&& (MyHealth || !DinoInfo[cptr->CType].killType[cptr->killType].carryCorpse)
					)
				{
					cptr->Phase = DinoInfo[cptr->CType].idleAnim[0];
				}
				else
				{
					cptr->Phase = DinoInfo[cptr->CType].walkAnim;
				}
				*/


			} else cptr->Phase = DinoInfo[cptr->CType].walkAnim;

		}
		else
		{
			cptr->Phase = DinoInfo[cptr->CType].runAnim;
		}
	}

	/*
	if (cptr->Phase != BRA_IDLE1 && cptr->Phase != BRA_IDLE2 && cptr->Phase != BRA_IDLE3)
		if (!cptr->State) cptr->Phase = BRA_WALK;
		else if (fabs(cptr->tgalpha - cptr->alpha) < 1.0 ||
			fabs(cptr->tgalpha - cptr->alpha) > 2 * pi - 1.0)
			cptr->Phase = BRA_RUN;
		else cptr->Phase = BRA_WALK;
	 */ //001 is this needed?

	 //cptr->Phase=BRA_WALK;

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

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) goto SKIPROT;

	if (cptr->currentIdleGroup >= 0) goto SKIPROT;

	if (drspd > 0.02)
		if (cptr->tgalpha > cptr->alpha) currspeed = 0.2f + drspd * 0.2f;
		else currspeed = -0.2f - drspd * 0.2f;
	else currspeed = 0;
	if (cptr->AfraidTime) currspeed *= DinoInfo[cptr->CType].rotspdmulti;

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
	if (cptr->Phase == DinoInfo[cptr->CType].runAnim) curspeed = DinoInfo[cptr->CType].runspd;

	if (cptr->Phase == DinoInfo[cptr->CType].killType[cptr->killType].anim && DinoInfo[cptr->CType].killTypeCount) curspeed = 0.0f;

	if (drspd > pi / 2.f) curspeed *= 2.f - 2.f*drspd / pi;

	//========== process speed =============//
	curspeed *= cptr->scale;
	DeltaFunc(cptr->vspeed, curspeed, TimeDt / 1024.f);
	cptr->pos.x += cptr->lookx * cptr->vspeed * TimeDt;
	cptr->pos.z += cptr->lookz * cptr->vspeed * TimeDt;
	/*
	MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt,
					   cptr->lookz * cptr->vspeed * TimeDt, true, true);
	*/

	ThinkY_Beta_Gamma(cptr, 256, 128, 0.1f, 0.2f);
	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 4048.f);
}







//OLD BRAHI
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
	if (pdist > (ctViewR + 20) * 256)
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
	/*
	MoveCharacter(cptr, cptr->lookx * cptr->vspeed * TimeDt,
					   cptr->lookz * cptr->vspeed * TimeDt, true, true);
	*/

	ThinkY_Beta_Gamma(cptr, 256, 128, 0.1f, 0.2f);
	DeltaFunc(cptr->gamma, cptr->tggamma, TimeDt / 4048.f);
}






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

	if (pdist > (ctViewR + 20) * 256)
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



//multiplayer
void AnimateMHunters() {

	//loop through hunters

	for (int c = 0; c < 1; c++) {//temp 1 player

		Vector3d *pos = &MPlayers[c].pos;

		if (mGunShot[c] != -1) {
			int weapon = mGunShot[c];
			mGunShot[c] = -1;
			if (WeapInfo[weapon].MGSSound) {
				TSFX *shotFx = &fxGunShot[WeapInfo[weapon].SFXIndex];
				AddVoice3d(shotFx->length, shotFx->lpData.data(), pos->x, pos->y, pos->z);//TODO XYZ NEEDS TO BE PLAYER -> SOUND VECTOR
			}
			MakeNoise(*pos, ctViewR * 200 * WeapInfo[weapon].Loud);
		}

		if (mHunterCall[c] != -1) {
			int targetCreature = mHunterCall[c];
			mHunterCall[c] = -1;
			int callType = mHunterCallType[c];
			mHunterCallType[c] = -1;
			TSFX *callFx = &fxCall[targetCreature][callType];
			AddVoice3d(callFx->length, callFx->lpData.data(), pos->x, pos->y, pos->z);
		}

	}
	
}



void AnimateCharacters()
{
	//if (!RunMode) return;
	TCharacter *cptr;

	HitBox.pos.x = PlayerX;
	HitBox.pos.y = PlayerY;
	HitBox.pos.z = PlayerZ;
	HitBox.alpha = PlayerAlpha;

	if (g_GameMode == GameMode::TrophyMode) {

		for (CurDino = 0; CurDino < ChCount; CurDino++)
		{

			//LandingList.list[DinoInfo[Characters[ChCount].CType].trophyType[DinoInfo[Characters[ChCount].CType].tCounter].trophyPos].x

			cptr = &Characters[CurDino];

			if (cptr->animateTrophy) {
				cptr->FTime += TimeDt;
				if (cptr->FTime >= cptr->pinfo->Animation[cptr->Phase].AniTime)
				{
					cptr->FTime %= cptr->pinfo->Animation[cptr->Phase].AniTime;
				}
			}
		}

		return;
	}

	if (Multiplayer && !Host) {
		
		for (CurDino = 0; CurDino < 6/*ChCount*/; CurDino++)
		{
			cptr = &Characters[CurDino];

			AnimateMClientCharacter(cptr);
		}
		

		return;
	}

	if (g_GameMode == GameMode::SurvivalMode) {
		bool waveOver = true;
		for (CurDino = 0; CurDino < ChCount; CurDino++) {
			if (Characters[CurDino].Health) waveOver = false;
		}
		if (waveOver) {
			WaveNoteTime = 2000;
			PlaceCharactersSurvival();
		}
	}


	//packs
	for (int packN = 0; packN < PackCount; packN++) {

		Packs[packN]._alert = Packs[packN].alert;
		Packs[packN]._attack = Packs[packN].attack;
		Packs[packN].alert = false;
		Packs[packN].attack = false;

	}

	TrophyDisplay = false;

	for (CurDino = 0; CurDino < ChCount; CurDino++)
	{
		cptr = &Characters[CurDino];
		if (cptr->StateF == 0xFF) continue;
		cptr->tgtime += TimeDt;

		// tracker bullets
		if (cptr->RTime && WeapInfo[cptr->tracker].radarTime) {
			cptr->RTime -= TimeDt;
			if (cptr->RTime < 0) {
				cptr->RTime = 0;
				cptr->tracker = -1;
			}
		}


		// replace pack leader
		if (cptr->Health && cptr->packId >= 0) {
			if (!Packs[cptr->packId].leader->Health) Packs[cptr->packId].leader = cptr;
		}

		if (cptr->tgtime > 30 * 1000) {

			if (cptr->Clone == AI_BRACH || cptr->Clone == AI_BRACHDANGER || cptr->Clone == AI_LANDBRACH) SetNewTargetPlace_Brahi(cptr, 2048.f);
			else if (cptr->Clone == AI_MOSA) SetNewTargetPlaceFish(cptr, 5048.f);
			else if (cptr->Clone == AI_FISH) SetNewTargetPlaceFish(cptr, 1024.f);
			else SetNewTargetPlace(cptr, 2048);

		}

		if (cptr->tgtime > 50 * 1000 && cptr->Clone == AI_ICTH) {
			if (cptr->Phase != DinoInfo[cptr->CType].flyAnim &&
				cptr->Phase != DinoInfo[cptr->CType].glideAnim &&
				cptr->Phase != DinoInfo[cptr->CType].takeoffAnim &&
				cptr->Phase != DinoInfo[cptr->CType].landAnim)
			{
				cptr->State = 2;
				cptr->AfraidTime = (50 + rRand(8)) * 1024;
				cptr->notFlushed = true;
			}
			else {
				SetNewTargetPlace_Icth(cptr, 2048);
			}
		}



		if (GetLandUpH(cptr->pos.x, cptr->pos.z) == GetLandH(cptr->pos.x, cptr->pos.z))
		  if (cptr->Health)
			if (cptr->BloodTTime)
			{
				cptr->BloodTTime -= TimeDt;
				if (cptr->BloodTTime < 0) cptr->BloodTTime = 0;

				float k = (20000.f + cptr->BloodTTime) / 90000.f;
				if (k > 1.5) k = 1.5;
				cptr->BloodTime += static_cast<int>((static_cast<float>(TimeDt) * k));
				if (cptr->BloodTime > 600)
				{
					cptr->BloodTime = rRand(228);
					AddBloodTrail(cptr);
					if (rRand(128) > 96) AddBloodTrail(cptr);
				}
			}

		if (cptr->AfraidTime <= 0) {
			cptr->awareHunter = false;
			cptr->heardShot = false;
		}

		

		//disp ship info
		if (!cptr->Health && DinoInfo[cptr->CType].trophy && g_GameMode != GameMode::SurvivalMode) {
			if (fabs(VectorLength(SubVectors(PlayerPos, cptr->pos))) < DinoInfo[cptr->CType].Radius) {
				TrophyDisplayBody.ctype = cptr->CType;
				TrophyDisplayBody.scale = cptr->scale;
				TrophyDisplayBody.weapon = CurrentWeapon;
				TrophyDisplayBody.score = cptr->tempScore;
				TrophyDisplayBody.phase = (RealTime & 3);
				TrophyDisplayBody.time = cptr->tempTime;
				TrophyDisplayBody.date = cptr->tempDate;
				TrophyDisplayBody.range = cptr->tempRange;
				TrophyDisplay = true;
				TrophyDisplayC = CurDino;
			}
		}

		switch (cptr->Clone)
		{
		case AI_MOSA:
		case AI_FISH:
			if (cptr->Health) AnimateFish(cptr);
			else AnimateDeadFish(cptr);
			break;
		case AI_BRACH:
			if (cptr->Health) AnimateBrahiOld(cptr);
			else AnimateDeadCommon(cptr);
			break;
		case AI_BRACHDANGER:
		case AI_LANDBRACH:
			if (cptr->Health) AnimateBrahi(cptr);
			else AnimateDeadCommon(cptr);
			break;
		case AI_ICTH:
			if (cptr->Health) AnimateIcth(cptr);
			else AnimateIcthDead(cptr);
			break;
		case AI_MOSH:
		case AI_PIG:
		case AI_GALL:
		case AI_DIMET:
			if (cptr->Health) AnimateClassicAmbient(cptr);
			else AnimateDeadCommon(cptr);
			break;
		case AI_DIMOR:
		case AI_PTERA:
			if (cptr->Health) AnimateDimor(cptr);
			else AnimateIcthDead(cptr);
			break;
		case AI_HUNTDOG:
			//	HUNTDOG TEMPP DISABLED
			/* if (cptr->Health) AnimateHuntdog(cptr);
			else AnimateDeadCommon(cptr);*/
			break;

		case AI_POACHER:
			// TEMP DISABLED
			//if (cptr->Health) AnimatePoacher(cptr);
			//else AnimateDeadCommon(cptr);
			break;

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
			if (cptr->Health) AnimateHuntable(cptr);
			else AnimateDeadCommon(cptr);
			break;
		case AI_TREX:
			if (cptr->Health) AnimateTRex(cptr);
			else AnimateDeadCommon(cptr);
			break;
		case AI_TITAN:
			//TEMP DISABLED
			//if (cptr->Health) AnimateTitan(cptr);
			//else AnimateIcthDead(cptr);
			break;
		case AI_MICRO:
			//TEMP DISABLED
			//if (cptr->Health) AnimateMicro(cptr);
			//else AnimateIcthDead(cptr);
			break;
		case 0:
			AnimateHuntDead(cptr);
			break;
		}

	}
}