#include "Hunt.h"
#include "Game/CharacterInternal.h"
#include "stdio.h"

void SetNewTargetPlace(TCharacter *cptr, float R);
void SetNewTargetPlaceRegion(TCharacter *cptr, float R);

void ProcessPrevPhase(TCharacter *cptr)
{

	cptr->PPMorphTime += TimeDt;
	if (cptr->PPMorphTime > PMORPHTIME) cptr->PrevPhase = cptr->Phase;

	cptr->PrevPFTime += TimeDt;
 	cptr->PrevPFTime %= cptr->pinfo->Animation[cptr->PrevPhase].AniTime;
	cptr->PrevPFTime %= cptr->pinfo->Animation[cptr->PrevPhase].AniTime;
}

void ActivateCharacterFxAquatic(TCharacter *cptr)
{
	if (cptr->CType) //== not hunter ==//
		if (!IsUnderwater()) return;
	int fx = cptr->pinfo->Anifx[cptr->Phase];
	if (fx == -1) return;

	if (VectorLengthSq(SubVectors(PlayerPos, cptr->pos)) > (68 * 256) * (68 * 256)) return;

	AddVoice3d(cptr->pinfo->SoundFX[fx].length,
		cptr->pinfo->SoundFX[fx].lpData.data(),
		cptr->pos.x, cptr->pos.y, cptr->pos.z);
}

void ActivateCharacterFx(TCharacter *cptr)
{
	
	//char buff[100];
	//sprintf(buff, "\n AI= %i", DinoInfo[cptr->CType].Clone);
	//PrintLog(buff);

	//char buff2[100];
	//sprintf(buff2, " Ph= %i", cptr->Phase);
	//PrintLog(buff2);

	if (cptr->CType) //== not hunter ==//
		if (IsUnderwater()) return;
	int fx = cptr->pinfo->Anifx[cptr->Phase];
	if (fx == -1) return;

	if (VectorLengthSq(SubVectors(PlayerPos, cptr->pos)) > (68 * 256) * (68 * 256)) return;

	AddVoice3d(cptr->pinfo->SoundFX[fx].length,
		cptr->pinfo->SoundFX[fx].lpData.data(),
		cptr->pos.x, cptr->pos.y, cptr->pos.z);
		
}

void ResetCharacter(TCharacter *cptr)
{
	//cptr->AI = DinoInfo[cptr->CType].AI;
	cptr->pinfo = &ChInfo[cptr->CType];
	cptr->Clone = DinoInfo[cptr->CType].Clone;
	cptr->State = 0;
	cptr->StateF = 0;
	cptr->Phase = 0;
	cptr->FTime = 0;
	cptr->PrevPhase = 0;
	cptr->PrevPFTime = 0;
	cptr->PPMorphTime = 0;
	cptr->beta = 0;
	cptr->gamma = 0;
	cptr->tggamma = 0;
	cptr->bend = 0;
	cptr->rspeed = 0;
	cptr->AfraidTime = 0;
	cptr->BloodTTime = 0;
	cptr->BloodTime = 0;

	cptr->claimed = false;

	cptr->tracker = -1;
	cptr->RTime = 0;

	if (cptr->Clone == AI_BRACH ||
		cptr->Clone == AI_BRACHDANGER ||
		cptr->Clone == AI_ICTH ||
		cptr->Clone == AI_FISH ||
		cptr->Clone == AI_MOSA) {
		cptr->cpcpAquatic = true;
	}
	else cptr->cpcpAquatic = false;

	cptr->currentIdleGroup = -1;
	cptr->currentIdle2Group = -1;

	cptr->hunterAwareness = HunterAwarenessState::None;

	if (DinoInfo[cptr->CType].killTypeCount > 1) {
		cptr->killType = rRand(DinoInfo[cptr->CType].killTypeCount - 1);
	}

	if (DinoInfo[cptr->CType].roarCount > 0) {
		cptr->roarAnim = DinoInfo[cptr->CType].roarAnim[rRand(DinoInfo[cptr->CType].roarCount - 1)];
	}

	if (DinoInfo[cptr->CType].deathTypeCount > 1) {
		cptr->deathType = rRand(DinoInfo[cptr->CType].deathTypeCount - 1);
	}

	if (DinoInfo[cptr->CType].waterDieCount > 0) {
		cptr->waterDieAnim = DinoInfo[cptr->CType].waterDieAnim[rRand(DinoInfo[cptr->CType].waterDieCount - 1)];
	}

	cptr->lastTBeta = 0;
	cptr->turny = 0;
	cptr->bdepth = static_cast<float>(0);

	cptr->lookx = static_cast<float>(cos(cptr->alpha));
	cptr->lookz = static_cast<float>(sin(cptr->alpha));

	cptr->Health = DinoInfo[cptr->CType].Health0;
	if (OptAgres > 128) cptr->Health = (cptr->Health*OptAgres) / 128;

	cptr->scale = static_cast<float>((DinoInfo[cptr->CType].Scale0 + rRand(DinoInfo[cptr->CType].ScaleA))) / 1000.f;

	//When does need to get set? not here huh?
	//cptr->RType = spawnGroup[cptr->SpawnGroupType].spawnRegionCh;

	cptr->followLeader = false;

	cptr->aquaticIdle = false;

	cptr->spcDepth = DinoInfo[cptr->CType].spacingDepth + (cptr->scale * 500) - 500;

	cptr->showSonar = false;

	//poacher
	cptr->ammo = DinoInfo[cptr->CType].Reload;

}

void AddDeadBody(TCharacter *cptr, int phase, bool scream)
{
	if (!MyHealth) return;

	if (ExitTime)
		AddMessage("Transportation cancelled.");
	ExitTime = 0;

	g_GameMode = GameMode::Normal;
	g_GameMode = GameMode::Normal;
	Characters[ChCount].CType = 0;
	Characters[ChCount].alpha = CameraAlpha;
	ResetCharacter(&Characters[ChCount]);

	int v = rRand(3);
	if (phase != HUNT_BREATH && scream){
		AddVoicev(fxScream[r].length, fxScream[r].lpData.data(), 256);
	}

	Characters[ChCount].Health = 0;
	MyHealth = 0;
	FlashlightOn = false; // Switch off before the death/kill camera takes over.
	if (cptr)
	{
		killerDino = cptr;

		if (GetLandUpH(killerDino->pos.x, killerDino->pos.z) -
			GetLandH(killerDino->pos.x, killerDino->pos.z) >
			DinoInfo[killerDino->CType].waterLevel * killerDino->scale) {
			killedwater = true;
		}
		else {
			killedwater = false;
		}

		float pl = DinoInfo[cptr->CType].killType[cptr->killType].offset;
		Characters[ChCount].pos.x = cptr->pos.x + cptr->lookx * pl * cptr->scale;
		Characters[ChCount].pos.z = cptr->pos.z + cptr->lookz * pl * cptr->scale;
		Characters[ChCount].pos.y = GetLandQH(Characters[ChCount].pos.x, Characters[ChCount].pos.z);
	}
	else
	{
		Characters[ChCount].pos.x = PlayerX;
		Characters[ChCount].pos.z = PlayerZ;
		Characters[ChCount].pos.y = PlayerY;
	}

	Characters[ChCount].Phase = phase;
	Characters[ChCount].PrevPhase = phase;

	ActivateCharacterFx(&Characters[ChCount]);

	DemoPoint.pos = Characters[ChCount].pos;
	DemoPoint.DemoTime = 1;
	DemoPoint.CIndex = ChCount;

	ChCount++;
}

float AngleDifference(float a, float b)
{
	a -= b;
	a = static_cast<float>(fabs(a));
	if (a > pi) a = 2 * pi - a;
	return a;
}

float CorrectedAlpha(float a, float b)
{
	float d = static_cast<float>(fabs(a - b));
	if (d < pi) return (a + b) / 2;
	else d = (a + pi * 2 - b);

	if (d < 0) d += 2 * pi;
	if (d > 2 * pi) d -= 2 * pi;
	return d;
}

void ThinkY_Beta_Gamma(TCharacter *cptr, float blook, float glook, float blim, float glim)
{
	cptr->pos.y = GetLandH(cptr->pos.x, cptr->pos.z);

	//=== beta ===//
	float hlook = GetLandH(cptr->pos.x + cptr->lookx * blook, cptr->pos.z + cptr->lookz * blook);
	float hlook2 = GetLandH(cptr->pos.x - cptr->lookx * blook, cptr->pos.z - cptr->lookz * blook);
	DeltaFunc(cptr->beta, (hlook2 - hlook) / (blook * 3.2f), TimeDt / 800.f);

	if (cptr->beta > blim) cptr->beta = blim;
	if (cptr->beta < -blim) cptr->beta = -blim;

	//=== gamma ===//
	hlook = GetLandH(cptr->pos.x + cptr->lookz * glook, cptr->pos.z - cptr->lookx*glook);
	hlook2 = GetLandH(cptr->pos.x - cptr->lookz * glook, cptr->pos.z + cptr->lookx*glook);
	cptr->tggamma = (hlook - hlook2) / (glook * 3.2f);
	if (cptr->tggamma > glim) cptr->tggamma = glim;
	if (cptr->tggamma < -glim) cptr->tggamma = -glim;
}

//OLD

/*
*/

//OLD

BOOL ReplaceCharacterForward(TCharacter *cptr)
{

	if (!spawnGroup[cptr->SpawnGroupType].moveForward) return false;

	float al = CameraAlpha + static_cast<float>(siRand(2048)) / 2048.f;
	float sa = static_cast<float>(sin(al));
	float ca = static_cast<float>(cos(al));
	Vector3d p;
	p.x = PlayerX + sa * (charViewR + rRand(10)) * 256;
	p.z = PlayerZ - ca * (charViewR + rRand(10)) * 256;
	p.y = GetLandH(p.x, p.z);

	if (p.x < 16 * 256) return false;
	if (p.z < 16 * 256) return false;
	if (p.x > 1000 * 256) return false;
	if (p.z > 1000 * 256) return false;

	BOOL outside = true;
	for (int sr = 0; sr < spawnGroup[cptr->SpawnGroupType].spawnRegionCh; sr++) {
		if (p.x > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMin * 256 &&
			p.x < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMax * 256 &&
			p.z > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMin * 256 &&
			p.z < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMax * 256) outside = false;
	}
	if (outside) return false;
	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (p.x > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin * 256 &&
				p.x < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax * 256 &&
				p.z > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin * 256 &&
				p.z < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax * 256) return false;
		}
	}

	if (cptr->Clone == AI_BRACH || cptr->Clone == AI_BRACHDANGER || cptr->Clone == AI_ICTH) {
		if (CheckPlaceCollisionBrahiP(p)) return false;
	} else if (cptr->Clone == AI_FISH || cptr->Clone == AI_MOSA) {
			if (CheckPlaceCollisionFishP(p,
				DinoInfo[cptr->CType].minDepth,
				DinoInfo[cptr->CType].maxDepth)) return false;
	} else if (CheckPlaceCollisionP(p, cptr->cpcpAquatic)) return false;

//	cptr->State = 0;
	cptr->pos = p;
	ResetCharacter(cptr);
	//cptr->tgx = cptr->pos.x + siRand(2048);
	//cptr->tgz = cptr->pos.z + siRand(2048);
	if (cptr->Clone == AI_BRACH || cptr->Clone == AI_BRACHDANGER || cptr->Clone == AI_LANDBRACH) SetNewTargetPlace_Brahi(cptr, 2048.f);
	else if (cptr->Clone == AI_MOSA) SetNewTargetPlaceFish(cptr, 5048.f);
	else if (cptr->Clone == AI_FISH) SetNewTargetPlaceFish(cptr, 1024.f);
	else SetNewTargetPlace(cptr, 2048);

	if (cptr->Clone == AI_FISH || cptr->Clone == AI_MOSA) {
		cptr->pos.y = GetLandUpH(cptr->pos.x, cptr->pos.z) - ((GetLandUpH(cptr->pos.x, cptr->pos.z) - GetLandH(cptr->pos.x, cptr->pos.z)) / 2);
	}

	if (cptr->Clone == AI_DIMOR || cptr->Clone == AI_PTERA) //===== dimor ========//
		cptr->pos.y += DinoInfo[cptr->CType].minDepth;
	return true;
}

//old

