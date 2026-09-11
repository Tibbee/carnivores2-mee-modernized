// AnimateHuntable.cpp � split from CharacterAnimation.cpp
// ==========================================================================
// One-time split from CharacterAnimation.cpp; no generator — edit this file.
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

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


	// Step 4: Extend culling distance by 4 units (~1024 world units)
	// to allow smoothstep fade-out to complete
	if (pdistSq > ((charViewR + 20 + 4) * 256) * ((charViewR + 20 + 4) * 256))
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
