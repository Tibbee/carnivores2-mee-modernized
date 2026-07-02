#include "Hunt.h"
#include "stdio.h"

#define fx_DIE    0

/*
#define RAP_RUN    0
#define RAP_WALK   1
#define RAP_SWIM   2
#define RAP_SLIDE  3
#define RAP_JUMP   4
#define RAP_DIE    5
#define RAP_EAT    6
#define RAP_SLP    7
#define RAP_IDLE1  8
#define RAP_IDLE2  9

#define SPN_RUN    0
#define SPN_WALK   1
#define SPN_SLIDE  2
#define SPN_SWIM   1
#define SPN_IDLE1  3
#define SPN_IDLE2  4
#define SPN_JUMP   5
#define SPN_DIE    6
#define SPN_EAT    7
#define SPN_SLP    8

#define VEL_RUN    0
#define VEL_WALK   1
#define VEL_SWIM   3
#define VEL_SLIDE  2
#define VEL_JUMP   4
#define VEL_DIE    5
#define VEL_EAT    6
#define VEL_SLP    7
#define VEL_IDLE1  8
#define VEL_IDLE2  9

#define CER_WALK   0
#define CER_RUN    1
#define CER_IDLE1  2
#define CER_IDLE2  3
#define CER_IDLE3  4
#define CER_DIE    5
#define CER_SLP    6
#define CER_EAT    7
#define CER_SWIM   0

*/

#define MOSA_RUN    0
#define MOSA_WALK   1
#define MOSA_JUMP   2
#define MOSA_DIE    3
#define MOSA_EAT    4
#define MOSA_SLP    5

/*
#define FISH_WALK   0
#define FISH_RUN    1

#define REX_RUN    0
#define REX_WALK   1
#define REX_SCREAM 2
#define REX_SWIM   3
#define REX_SEE    4
#define REX_SEE1   5
#define REX_SMEL   6
#define REX_SMEL1  7
#define REX_DIE    8
#define REX_EAT    9
#define REX_SLP    10


#define ICTH_WALK           0
#define ICTH_WALK_IDLE1     1
#define ICTH_WALK_IDLE2     2
#define ICTH_WINGDOWN_LAND  3
#define ICTH_SWIM           4
#define ICTH_SWIM_IDLE1     5
#define ICTH_SWIM_IDLE2     6
#define ICTH_WINGDOWN_WATER 7
#define ICTH_FLY            8
#define ICTH_FLY2           9
#define ICTH_TAKEOFF        10
#define ICTH_LANDING        11
#define ICTH_FALL           12
#define ICTH_LAND_DIE       13
#define ICTH_WATER_DIE      14
#define ICTH_SLEEP          15


#define MOS_RUN    0
#define MOS_WALK   1
#define MOS_DIE    2
#define MOS_IDLE1  3
#define MOS_IDLE2  4
#define MOS_SLP    5


#define DMT_WALK   0
#define DMT_RUN    1
#define DMT_IDLE1  2
#define DMT_IDLE2  3
#define DMT_DIE    4
#define DMT_SLP    5


#define ANK_RUN    0
#define ANK_WALK   1
#define ANK_IDLE1  2
#define ANK_IDLE2  3
#define ANK_DIE    4
#define ANK_SLP    5


#define STG_RUN    0
#define STG_WALK   1
#define STG_DIE    2
#define STG_IDLE1  3
#define STG_IDLE2  4
#define STG_SLP    5

#define GAL_RUN    0
#define GAL_WALK   1
#define GAL_SLIDE  2
#define GAL_DIE    3
#define GAL_IDLE1  4
#define GAL_IDLE2  5
#define GAL_SLP    6


#define TRI_RUN    0
#define TRI_WALK   1
#define TRI_IDLE1  2
#define TRI_IDLE2  3
#define TRI_IDLE3  4
#define TRI_DIE    5
#define TRI_SLP    6


#define PAC_WALK   0
#define PAC_RUN    1
#define PAC_SLIDE  2
#define PAC_DIE    3
#define PAC_IDLE1  4
#define PAC_IDLE2  5
#define PAC_SLP    6

#define PAR_WALK   0
#define PAR_RUN    1
#define PAR_IDLE1  2
#define PAR_IDLE2  3
#define PAR_DIE    4
#define PAR_SLP    5

#define DIM_FLY    0
#define DIM_FLYP   1
#define DIM_FALL   2
#define DIM_DIE    3

#define BRA_WALK   0
#define BRA_IDLE1  1
#define BRA_IDLE2  2
#define BRA_IDLE3  3
#define BRA_DIE    4
#define BRA_SLP    5
#define BRA_RUN    6
#define BRA_EAT    10
*/



void SetNewTargetPlace(TCharacter *cptr, float R);
void SetNewTargetPlaceRegion(TCharacter *cptr, float R);
void SetNewTargetPlaceVanilla(TCharacter *cptr, float R);


void ProcessPrevPhase(TCharacter *cptr)
{
/*	char buff[100];
	sprintf(buff, "\n AI= %i", DinoInfo[cptr->CType].Clone);
	PrintLog(buff);

	char buff2[100];
	sprintf(buff2, " Ph= %i", cptr->Phase);
	PrintLog(buff2);*/

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

	cptr->awareHunter = false;
	cptr->heardShot = false;

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
		/*
		if (DinoInfo[cptr->CType].Aquatic) {
			Characters[ChCount].pos.x = cptr->pos.x + cptr->lookx * pl * cptr->scale * static_cast<float>(cos(cptr->beta));
			Characters[ChCount].pos.z = cptr->pos.z + cptr->lookz * pl * cptr->scale * static_cast<float>(cos(cptr->beta));
			Characters[ChCount].pos.y = cptr->pos.y - static_cast<float>(sin(cptr->beta)) * pl * cptr->scale;
			float ply = DinoInfo[cptr->CType].killType[cptr->killType].yoffset;
			Characters[ChCount].pos.y += ply * static_cast<float>(cos(cptr->beta));
			ply *= static_cast<float>(sin(cptr->beta));
			Characters[ChCount].pos.z += ply * static_cast<float>(sin(cptr->alpha));
			Characters[ChCount].pos.x += ply * static_cast<float>(cos(cptr->alpha));
			Characters[ChCount].alpha = cptr->alpha;
			Characters[ChCount].beta = cptr->beta;
			Characters[ChCount].gamma = cptr->gamma;

		}
		*/
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

	/*
	//if (phase > 0) {
	if (DinoInfo[cptr->CType].killType[cptr->killType].elevate) {
		Characters[ChCount].scale = cptr->scale;
		Characters[ChCount].alpha = cptr->alpha;
		cptr->bend = 0;
		DemoPoint.CIndex = CurDino;
	}
	*/

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
	/*
	  if (DEBUG) cptr->tggamma = 0;
	  if (DEBUG) cptr->beta    = 0;
	  */
}




int CheckPlaceCollisionP(Vector3d &v, bool aquatic)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1008 || ccz>1008) return 1;

	int F = (FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
		FMap[ccz][ccx] |
		FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]);

	if (aquatic) {
		if (F & fmNOWAY) return 1;
	}
	else if (F & (fmWater + fmNOWAY)) return 1;


	


	float h = GetLandH(v.x, v.z);
	v.y = h;

	float hh = GetLandH(v.x - 164, v.z - 164);
	if (fabs(hh - h) > 160) return 1;
	hh = GetLandH(v.x + 164, v.z - 164);
	if (fabs(hh - h) > 160) return 1;
	hh = GetLandH(v.x - 164, v.z + 164);
	if (fabs(hh - h) > 160) return 1;
	hh = GetLandH(v.x + 164, v.z + 164);
	if (fabs(hh - h) > 160) return 1;

	for (int z = -2; z <= 2; z++)
		for (int x = -2; x <= 2; x++)
			if (OMap[ccz + z][ccx + x] != 255)
			{
				int ob = OMap[ccz + z][ccx + x];
				if (MObjects[ob].info.Radius < 10) continue;
				float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

				float oz = (ccz + z) * 256.f + 128.f;
				float ox = (ccx + x) * 256.f + 128.f;

				float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
				if (r < CR) return 1;
			}

	return 0;
}


int CheckPlaceCollisionFishP(Vector3d &v, int minDepth, int maxDepth)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1008 || ccz>1008) return 1;
	
	if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) < minDepth ||
		(GetLandUpH(v.x + 256, v.z) - GetLandH(v.x + 256, v.z)) < minDepth ||
		(GetLandUpH(v.x, v.z + 256) - GetLandH(v.x, v.z + 256)) < minDepth ||
		(GetLandUpH(v.x + 256, v.z + 256) - GetLandH(v.x + 256, v.z + 256)) < minDepth ||
		(GetLandUpH(v.x - 256, v.z) - GetLandH(v.x - 256, v.z)) < minDepth ||
		(GetLandUpH(v.x, v.z - 256) - GetLandH(v.x, v.z - 256)) < minDepth ||
		(GetLandUpH(v.x - 256, v.z - 256) - GetLandH(v.x - 256, v.z - 256)) < minDepth ||
		(GetLandUpH(v.x + 256, v.z - 256) - GetLandH(v.x + 256, v.z - 256)) < minDepth ||
		(GetLandUpH(v.x - 256, v.z + 256) - GetLandH(v.x - 256, v.z + 256)) < minDepth) return 1;
		
	if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) > maxDepth ||
		(GetLandUpH(v.x + 256, v.z) - GetLandH(v.x + 256, v.z)) > maxDepth ||
		(GetLandUpH(v.x, v.z + 256) - GetLandH(v.x, v.z + 256)) > maxDepth ||
		(GetLandUpH(v.x + 256, v.z + 256) - GetLandH(v.x + 256, v.z + 256)) > maxDepth ||
		(GetLandUpH(v.x - 256, v.z) - GetLandH(v.x - 256, v.z)) > maxDepth ||
		(GetLandUpH(v.x, v.z - 256) - GetLandH(v.x, v.z - 256)) > maxDepth ||
		(GetLandUpH(v.x - 256, v.z - 256) - GetLandH(v.x - 256, v.z - 256)) > maxDepth ||
		(GetLandUpH(v.x + 256, v.z - 256) - GetLandH(v.x + 256, v.z - 256)) > maxDepth ||
		(GetLandUpH(v.x - 256, v.z + 256) - GetLandH(v.x - 256, v.z + 256)) > maxDepth) return 1;
		
	return 0;
}


int CheckPlaceCollisionFish(TCharacter *cptr, Vector3d &v, float mosaDepth, int maxDepth, int minDepth)
{

	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1018 || ccz>1018) return 1;

	/*if (wc)
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater)
			return 1;
	 */

	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (ccx > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin &&
				ccx < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax &&
				ccz > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin &&
				ccz < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax) return 1;
		}
	}

	// #C1 KEEP THIS
	if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) < minDepth ||
		(GetLandUpH(v.x + 256, v.z) - GetLandH(v.x + 256, v.z)) < minDepth ||
		(GetLandUpH(v.x, v.z + 256) - GetLandH(v.x, v.z + 256)) < minDepth ||
		(GetLandUpH(v.x + 256, v.z + 256) - GetLandH(v.x + 256, v.z + 256)) < minDepth ||
		(GetLandUpH(v.x - 256, v.z) - GetLandH(v.x - 256, v.z)) < minDepth ||
		(GetLandUpH(v.x, v.z - 256) - GetLandH(v.x, v.z - 256)) < minDepth ||
		(GetLandUpH(v.x - 256, v.z - 256) - GetLandH(v.x - 256, v.z - 256)) < minDepth ||
		(GetLandUpH(v.x + 256, v.z - 256) - GetLandH(v.x + 256, v.z - 256)) < minDepth ||
		(GetLandUpH(v.x - 256, v.z + 256) - GetLandH(v.x - 256, v.z + 256)) < minDepth) return 1;

	if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) > maxDepth ||
		(GetLandUpH(v.x + 256, v.z) - GetLandH(v.x + 256, v.z)) > maxDepth ||
		(GetLandUpH(v.x, v.z + 256) - GetLandH(v.x, v.z + 256)) > maxDepth ||
		(GetLandUpH(v.x + 256, v.z + 256) - GetLandH(v.x + 256, v.z + 256)) > maxDepth ||
		(GetLandUpH(v.x - 256, v.z) - GetLandH(v.x - 256, v.z)) > maxDepth ||
		(GetLandUpH(v.x, v.z - 256) - GetLandH(v.x, v.z - 256)) > maxDepth ||
		(GetLandUpH(v.x - 256, v.z - 256) - GetLandH(v.x - 256, v.z - 256)) > maxDepth ||
		(GetLandUpH(v.x + 256, v.z - 256) - GetLandH(v.x + 256, v.z - 256)) > maxDepth ||
		(GetLandUpH(v.x - 256, v.z + 256) - GetLandH(v.x - 256, v.z + 256)) > maxDepth) return 1;



	// #C1 REMOVE THESE
	//if (mosaDepth > GetLandUpH(v.x, v.z) - 700) return 1;
	//if (mosaDepth < GetLandH(v.x, v.z) + 500) return 1;

	/*
	float h = GetLandH(v.x, v.z);
	if (fabs(h - v.y) > 64) return 1;

	v.y = h;

	float hh = GetLandH(v.x - 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x - 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;

	if (mc)
		for (int z = -2; z <= 2; z++)
			for (int x = -2; x <= 2; x++)
				if (OMap[ccz + z][ccx + x] != 255)
				{
					int ob = OMap[ccz + z][ccx + x];
					if (MObjects[ob].info.Radius < 10) continue;
					float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

					float oz = (ccz + z) * 256.f + 128.f;
					float ox = (ccx + x) * 256.f + 128.f;

					float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
					if (r < CR) return 1;
				}

				*/
	return 0;
}


//OLD
int CheckPlaceCollisionMosasaurus(TCharacter *cptr, Vector3d &v, float mosaDepth)
{

	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1018 || ccz>1018) return 1;

	/*if (wc)
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater)
			return 1;
	 */

	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (ccx > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin &&
				ccx < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax &&
				ccz > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin &&
				ccz < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax) return 1;
		}
	}

	// #C1 KEEP THIS
	if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) < 1500 ||
		(GetLandUpH(v.x + 256, v.z) - GetLandH(v.x + 256, v.z)) < 1500 ||
		(GetLandUpH(v.x, v.z + 256) - GetLandH(v.x, v.z + 256)) < 1500 ||
		(GetLandUpH(v.x + 256, v.z + 256) - GetLandH(v.x + 256, v.z + 256)) < 1500 ||
		(GetLandUpH(v.x - 256, v.z) - GetLandH(v.x - 256, v.z)) < 1500 ||
		(GetLandUpH(v.x, v.z - 256) - GetLandH(v.x, v.z - 256)) < 1500 ||
		(GetLandUpH(v.x - 256, v.z - 256) - GetLandH(v.x - 256, v.z - 256)) < 1500 ||
		(GetLandUpH(v.x + 256, v.z - 256) - GetLandH(v.x + 256, v.z - 256)) < 1500 ||
		(GetLandUpH(v.x - 256, v.z + 256) - GetLandH(v.x - 256, v.z + 256)) < 1500) return 1;

	// #C1 REMOVE THESE
	//if (mosaDepth > GetLandUpH(v.x, v.z) - 700) return 1;
	//if (mosaDepth < GetLandH(v.x, v.z) + 500) return 1;

	/*
	float h = GetLandH(v.x, v.z);
	if (fabs(h - v.y) > 64) return 1;

	v.y = h;

	float hh = GetLandH(v.x - 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x - 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;

	if (mc)
		for (int z = -2; z <= 2; z++)
			for (int x = -2; x <= 2; x++)
				if (OMap[ccz + z][ccx + x] != 255)
				{
					int ob = OMap[ccz + z][ccx + x];
					if (MObjects[ob].info.Radius < 10) continue;
					float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

					float oz = (ccz + z) * 256.f + 128.f;
					float ox = (ccx + x) * 256.f + 128.f;

					float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
					if (r < CR) return 1;
				}

				*/
	return 0;
}

/*
bool jumpCollision(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc)
{
	Vector3d p = cptr->pos;
	float lookx = static_cast<float>(cos(cptr->tgalpha));
	float lookz = static_cast<float>(sin(cptr->tgalpha));
	for (int i = 0; i < 10; i++) {

		p.x += lookx * 64.f;
		p.z += lookz * 64.f;
		if (CheckPlaceCollision(cptr, p, wc, mc)) {
			return false;
		};
	}
	return true;
}
*/


int CheckPlaceCollision(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1018 || ccz>1018) return 1;

	if (wc)
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater)
			return 1;


	float h = GetLandH(v.x, v.z);
	if (!(FMap[ccz][ccx] & fmWater))
		if (fabs(h - v.y) > 64) return 1;

	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (ccx > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin &&
				ccx < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax &&
				ccz > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin &&
				ccz < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax) return 1;
		}
	}

	v.y = h;

	float hh = GetLandH(v.x - 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x - 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;

	if (mc)
		for (int z = -2; z <= 2; z++)
			for (int x = -2; x <= 2; x++)
				if (OMap[ccz + z][ccx + x] != 255)
				{
					int ob = OMap[ccz + z][ccx + x];
					if (MObjects[ob].info.Radius < 10) continue;
					float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

					float oz = (ccz + z) * 256.f + 128.f;
					float ox = (ccx + x) * 256.f + 128.f;

					float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
					if (r < CR) return 1;
				}

	return 0;
}

int CheckPlaceCollisionMicro(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1018 || ccz>1018) return 1;

	if (wc)
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater)
			return 1;


	float h = GetLandH(v.x, v.z);
	if (!(FMap[ccz][ccx] & fmWater))
		if (fabs(h - v.y) > 64) return 1;

	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (ccx > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin &&
				ccx < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax &&
				ccz > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin &&
				ccz < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax) return 1;
		}
	}

	v.y = h;

	float hh = GetLandH(v.x - 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x - 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;

	if (mc)
		for (int z = -2; z <= 2; z++)
			for (int x = -2; x <= 2; x++)
				if (OMap[ccz + z][ccx + x] != 255)
				{
					int ob = OMap[ccz + z][ccx + x];
					if (MObjects[ob].info.Radius < 10) continue;
					float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

					float oz = (ccz + z) * 256.f + 128.f;
					float ox = (ccx + x) * 256.f + 128.f;

					float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
					if (r < CR && (!TreeTable[ob] || !cptr->gottaClimb)) return 1;
				}

	return 0;
}

int CheckPlaceCollisionLandBrahi(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1018 || ccz>1018) return 1;

	/*if (wc)
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater)
			return 1;
	 */

	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (ccx > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin &&
				ccx < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax &&
				ccz > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin &&
				ccz < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax) return 1;
		}
	}

	if (wc){
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater){ 
			return 1;
		}
	} else {
		if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x + 256, v.z) - GetLandH(v.x + 256, v.z)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x, v.z + 256) - GetLandH(v.x, v.z + 256)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x + 256, v.z + 256) - GetLandH(v.x + 256, v.z + 256)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x - 256, v.z) - GetLandH(v.x - 256, v.z)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x, v.z - 256) - GetLandH(v.x, v.z - 256)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x - 256, v.z - 256) - GetLandH(v.x - 256, v.z - 256)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x + 256, v.z - 256) - GetLandH(v.x + 256, v.z - 256)) > DinoInfo[cptr->CType].waterLevel ||
			(GetLandUpH(v.x - 256, v.z + 256) - GetLandH(v.x - 256, v.z + 256)) > DinoInfo[cptr->CType].waterLevel) return 1;
	}

	float h = GetLandH(v.x, v.z);
	if (fabs(h - v.y) > 64) return 1;

	v.y = h;

	float hh = GetLandH(v.x - 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x - 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;

	if (mc)
		for (int z = -2; z <= 2; z++)
			for (int x = -2; x <= 2; x++)
				if (OMap[ccz + z][ccx + x] != 255)
				{
					int ob = OMap[ccz + z][ccx + x];
					if (MObjects[ob].info.Radius < 10) continue;
					float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

					float oz = (ccz + z) * 256.f + 128.f;
					float ox = (ccx + x) * 256.f + 128.f;

					float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
					if (r < CR) return 1;
				}

	return 0;
}

int CheckPlaceCollisionBrahi(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1018 || ccz>1018) return 1;

	/*if (wc)
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater)
			return 1;
	 */

	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (ccx > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin &&
				ccx < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax &&
				ccz > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin &&
				ccz < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax) return 1;
		}
	}

	int limit = 550;//550
	int range = 256;//256bluz
	if (wc) {
		if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) > limit ||
			(GetLandUpH(v.x + range, v.z) - GetLandH(v.x + range, v.z)) > limit ||
			(GetLandUpH(v.x, v.z + range) - GetLandH(v.x, v.z + range)) > limit ||
			(GetLandUpH(v.x + range, v.z + range) - GetLandH(v.x + range, v.z + range)) > limit ||
			(GetLandUpH(v.x - range, v.z) - GetLandH(v.x - range, v.z)) > limit ||
			(GetLandUpH(v.x, v.z - range) - GetLandH(v.x, v.z - range)) > limit ||
			(GetLandUpH(v.x - range, v.z - range) - GetLandH(v.x - range, v.z - range)) > limit ||
			(GetLandUpH(v.x + range, v.z - range) - GetLandH(v.x + range, v.z - range)) > limit ||
			(GetLandUpH(v.x - range, v.z + range) - GetLandH(v.x - range, v.z + range)) > limit) return 1;
	}

	float h = GetLandH(v.x, v.z);
	if (fabs(h - v.y) > 64) return 1;

	v.y = h;

	float hh = GetLandH(v.x - 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x - 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;

	if (mc)
		for (int z = -2; z <= 2; z++)
			for (int x = -2; x <= 2; x++)
				if (OMap[ccz + z][ccx + x] != 255)
				{
					int ob = OMap[ccz + z][ccx + x];
					if (MObjects[ob].info.Radius < 10) continue;
					float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

					float oz = (ccz + z) * 256.f + 128.f;
					float ox = (ccx + x) * 256.f + 128.f;

					float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
					if (r < CR) return 1;
				}

	return 0;
}

int CheckPlaceCollisionBrahiP(Vector3d &v)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1008 || ccz>1008) return 1;

	int F = (FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
		FMap[ccz][ccx] |
		FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]);

	if (!(GetLandUpH(v.x, v.z) > GetLandH(v.x, v.z))) return 1;

	int limit = 550;//550
	int range = 256;//256bluz
	if ((GetLandUpH(v.x, v.z) - GetLandH(v.x, v.z)) > limit ||
		(GetLandUpH(v.x + range, v.z) - GetLandH(v.x + range, v.z)) > limit ||
		(GetLandUpH(v.x, v.z + range) - GetLandH(v.x, v.z + range)) > limit ||
		(GetLandUpH(v.x + range, v.z + range) - GetLandH(v.x + range, v.z + range)) > limit ||
		(GetLandUpH(v.x - range, v.z) - GetLandH(v.x - range, v.z)) > limit ||
		(GetLandUpH(v.x, v.z - range) - GetLandH(v.x, v.z - range)) > limit ||
		(GetLandUpH(v.x - range, v.z - range) - GetLandH(v.x - range, v.z - range)) > limit ||
		(GetLandUpH(v.x + range, v.z - range) - GetLandH(v.x + range, v.z - range)) > limit ||
		(GetLandUpH(v.x - range, v.z + range) - GetLandH(v.x - range, v.z + range)) > limit) return 1;

	float h = GetLandH(v.x, v.z);
	v.y = h;

	float hh = GetLandH(v.x - 164, v.z - 164);
	if (fabs(hh - h) > 160) return 1;
	hh = GetLandH(v.x + 164, v.z - 164);
	if (fabs(hh - h) > 160) return 1;
	hh = GetLandH(v.x - 164, v.z + 164);
	if (fabs(hh - h) > 160) return 1;
	hh = GetLandH(v.x + 164, v.z + 164);
	if (fabs(hh - h) > 160) return 1;

	for (int z = -2; z <= 2; z++)
		for (int x = -2; x <= 2; x++)
			if (OMap[ccz + z][ccx + x] != 255)
			{
				int ob = OMap[ccz + z][ccx + x];
				if (MObjects[ob].info.Radius < 10) continue;
				float CR = static_cast<float>(MObjects[ob].info.Radius) + 64;

				float oz = (ccz + z) * 256.f + 128.f;
				float ox = (ccx + x) * 256.f + 128.f;

				float r = static_cast<float>(sqrt((ox - v.x)*(ox - v.x) + (oz - v.z)*(oz - v.z)));
				if (r < CR) return 1;
			}

	return 0;
}




int CheckPlaceCollision2(TCharacter *cptr, Vector3d &v, BOOL wc)
{
	int ccx = static_cast<int>(v.x) / 256;
	int ccz = static_cast<int>(v.z) / 256;

	if (ccx < 4 || ccz < 4 || ccx>1018 || ccz>1018) return 1;

	if (wc)
		if ((FMap[ccz][ccx - 1] | FMap[ccz - 1][ccx] | FMap[ccz - 1][ccx - 1] |
			FMap[ccz][ccx] |
			FMap[ccz + 1][ccx] | FMap[ccz][ccx + 1] | FMap[ccz + 1][ccx + 1]) & fmWater)
			return 1;

	if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
			if (ccx > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin &&
				ccx < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax &&
				ccz > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin &&
				ccz < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax) return 1;
		}
	}

	float h = GetLandH(v.x, v.z);
	/*if (! (FMap[ccz][ccx] & fmWater) )
	  if (fabs(h - v.y) > 64) return 1;*/
	v.y = h;

	float hh = GetLandH(v.x - 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z - 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x - 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;
	hh = GetLandH(v.x + 64, v.z + 64);
	if (fabs(hh - h) > 100) return 1;

	return 0;
}



int CheckPossiblePath(TCharacter *cptr, BOOL wc, BOOL mc)
{
	Vector3d p = cptr->pos;
	float lookx = static_cast<float>(cos(cptr->tgalpha));
	float lookz = static_cast<float>(sin(cptr->tgalpha));
	int c = 0;
	for (int t = 0; t < 20; t++)
	{

		if (cptr->Clone == AI_BRACH) {
			p.x += lookx * 256.f;
			p.z += lookz * 256.f;
			if (CheckPlaceCollisionBrahi(cptr, p, wc, mc)) c++;
		}
		else if (cptr->Clone == AI_BRACHDANGER) {
			p.x += lookx * DinoInfo[cptr->CType].maxGrad;//128
			p.z += lookz * DinoInfo[cptr->CType].maxGrad;//128
			if (CheckPlaceCollisionBrahi(cptr, p, wc, mc)) c++;
		} if (cptr->Clone == AI_LANDBRACH) {
			p.x += lookx * DinoInfo[cptr->CType].maxGrad;//128
			p.z += lookz * DinoInfo[cptr->CType].maxGrad;//128
			if (CheckPlaceCollisionLandBrahi(cptr, p, wc, mc)) c++;
		}
		else if (cptr->Clone == AI_FISH ||
			cptr->Clone == AI_MOSA) {
			p.x += lookx * 64.f;
			p.z += lookz * 64.f;
			if (CheckPlaceCollisionFish(cptr, p, cptr->depth,
				DinoInfo[cptr->CType].maxDepth,
				DinoInfo[cptr->CType].minDepth)) c++;

		}
		else if (cptr->Clone == AI_MICRO) {
			p.x += lookx * 64.f;
			p.z += lookz * 64.f;
			if (CheckPlaceCollisionMicro(cptr, p, wc, mc)) c++;
		}
		else {
			p.x += lookx * 64.f;
			p.z += lookz * 64.f;
			if (CheckPlaceCollision(cptr, p, wc, mc)) c++;
		}
	}
	return c;
}


void LookForAWay(TCharacter *cptr, BOOL wc, BOOL mc)
{
	float alpha = cptr->tgalpha;
	float dalpha = 15.f;
	float afound = alpha;
	int maxp = 16;
	int curp;

	if (!CheckPossiblePath(cptr, wc, mc))
	{
		cptr->NoWayCnt = 0;
		return;
	}

	cptr->NoWayCnt++;
	for (int i = 0; i < 12; i++)
	{
		cptr->tgalpha = alpha + dalpha * pi / 180.f;
		curp = CheckPossiblePath(cptr, wc, mc) + (i >> 1);
		if (!curp) return;
		if (curp < maxp)
		{
			maxp = curp;
			afound = cptr->tgalpha;
		}

		cptr->tgalpha = alpha - dalpha * pi / 180.f;
		curp = CheckPossiblePath(cptr, wc, mc) + (i >> 1);
		if (!curp) return;
		if (curp < maxp)
		{
			maxp = curp;
			afound = cptr->tgalpha;
		}

		dalpha += 15.f;
	}

	cptr->tgalpha = afound;
}


void SetNewTargetPlace_Icth(TCharacter *cptr, float R)
{
	Vector3d p;
	int tr = 0;


	//PrintLog("iT");//TEST20200412
replace:
	//PrintLog("-");//TEST20200412
	p.x = cptr->pos.x + siRand(static_cast<int>(R));
	p.z = cptr->pos.z + siRand(static_cast<int>(R));

	if (p.x < 512) p.x = 512;
	if (p.x > 1018 * 256) p.x = 1018 * 256;
	if (p.z < 512) p.z = 512;
	if (p.z > 1018 * 256) p.z = 1018 * 256;
	tr++;
	if (tr < 16)
		if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) < R / 2.f) goto replace;

	if (tr < 1024) {
		if (spawnGroup[cptr->SpawnGroupType].stayInRegion) {
			BOOL outside = true;
			for (int sr = 0; sr < spawnGroup[cptr->SpawnGroupType].spawnRegionCh; sr++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMax * 256) outside = false;
			}
			if (outside) goto replace;
		}
		if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
			for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax * 256) goto replace;
			}
		}
	}
	/*
	if (stayRegion && outsideRegion && tr > 64) {
		if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) > R * 25.f) {
			stayRegion = false;
			goto replace;
		}
	}
	*/

	//if (tr < 128)
	if (tr < 100)
	{
		if (!waterNear(p.x, p.z, 50)) goto replace;
		if (cptr->spawnAlt + 400 < GetLandUpH(p.x, p.z)) goto replace;
	}
	/*
	else if (R < 10240 && !(tr % 20)) {
		if (!waterNear(p.x, p.z, 50)) {
			R *= 2;
			goto replace;
		}
	}
	*/

	if (tr < 256)
		if (CheckPlaceCollisionP(p, true)) goto replace;

	cptr->tgtime = 0;
	cptr->tgx = p.x;
	cptr->tgz = p.z;
}

void SetNewTargetPlace_IcthOld(TCharacter *cptr, float R)
{
	Vector3d p;
	int tr = 0;
replace:
	p.x = cptr->pos.x + siRand(static_cast<int>(R));
	if (p.x < 512) p.x = 512;
	if (p.x > 1018 * 256) p.x = 1018 * 256;
	p.z = cptr->pos.z + siRand(static_cast<int>(R));
	if (p.z < 512) p.z = 512;
	if (p.z > 1018 * 256) p.z = 1018 * 256;
	tr++;
	if (tr < 16)
		if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) < R / 2.f) goto replace;

	if (tr < 128)
	{
		if (!waterNear(p.x, p.z, 50)) goto replace;
	}

	if (tr < 256)
		if (CheckPlaceCollisionP(p, true)) goto replace;

	cptr->tgtime = 0;
	cptr->tgx = p.x;
	cptr->tgz = p.z;
}

void SetNewTargetPlace(TCharacter *cptr, float R)
{

	//if (cptr->AI < 0) {//STILL NEED THIS FOR CLASSIC AMBIENTS
	SetNewTargetPlaceRegion(cptr, R);
	//}
	//else {
	//	SetNewTargetPlaceVanilla(cptr, R);
	//}


}

void SetNewTargetPlaceVanilla(TCharacter *cptr, float R)
{
	Vector3d p;
	int tr = 0;
	//PrintLog("PAR_START--");
replace:
	//PrintLog("PAR_IT--");
	p.x = cptr->pos.x + siRand(static_cast<int>(R));
	if (p.x < 512) p.x = 512;
	if (p.x > 1018 * 256) p.x = 1018 * 256;
	p.z = cptr->pos.z + siRand(static_cast<int>(R));
	if (p.z < 512) p.z = 512;
	if (p.z > 1018 * 256) p.z = 1018 * 256;
	p.y = GetLandH(p.x, p.z);
	tr++;
	if (tr < 128)
		if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) < R / 2.f) goto replace;

	R += 512;

	if (tr < 256)
		if (CheckPlaceCollisionP(p, cptr->cpcpAquatic)) goto replace;

	cptr->tgtime = 0;
	cptr->tgx = p.x;
	cptr->tgz = p.z;
}

void SetNewTargetPlaceRegion(TCharacter *cptr, float R)
{
	Vector3d p;
	int tr = 0;
replace:
	//PrintLog("-");//TEST20200415
	p.x = cptr->pos.x + siRand(static_cast<int>(R));
	p.z = cptr->pos.z + siRand(static_cast<int>(R));

	if (p.x < 512) p.x = 512;
	if (p.x > 1018 * 256) p.x = 1018 * 256;
	if (p.z < 512) p.z = 512;
	if (p.z > 1018 * 256) p.z = 1018 * 256;
	p.y = GetLandH(p.x, p.z);
	tr++;
	if (tr < 128) {
		if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) < R / 2.f) goto replace;

		if (spawnGroup[cptr->SpawnGroupType].stayInRegion) {
			BOOL outside = true;
			for (int sr = 0; sr < spawnGroup[cptr->SpawnGroupType].spawnRegionCh; sr++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMax * 256) outside = false;
			}
			if (outside) goto replace;
		}
		if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
			for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax * 256) goto replace;
			}
		}

	}

	R += 512;

	if (tr < 256)
		if (CheckPlaceCollisionP(p, cptr->cpcpAquatic)) goto replace;

	cptr->tgtime = 0;
	cptr->tgx = p.x;
	cptr->tgz = p.z;
}

void SetNewTargetPlace_Brahi(TCharacter *cptr, float R)
{
	Vector3d p;
	int tr = 0;
	//PrintLog("bT");//TEST202004111501
replace:
	//PrintLog("-");//TEST202004111501
	p.x = cptr->pos.x + siRand(static_cast<int>(R));
	if (p.x < 512) p.x = 512;
	if (p.x > 1018 * 256) p.x = 1018 * 256;
	p.z = cptr->pos.z + siRand(static_cast<int>(R));
	if (p.z < 512) p.z = 512;
	if (p.z > 1018 * 256) p.z = 1018 * 256;
	tr++;
	if (tr < 16)
		if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) < R / 2.f) goto replace;

	p.y = GetLandH(p.x, p.z);
	float wy = GetLandUpH(p.x, p.z) - p.y;

	if (tr < 128)
	{
		if (cptr->Clone == AI_LANDBRACH) {
			if (DinoInfo[cptr->CType].canSwim) {
				if (wy > 400) goto replace;
			}
			else {
				if (wy > 0) goto replace;
			}
		}
		else {
			if (wy > 400) goto replace;
			if (wy < 200) goto replace;
		}
		
		if (spawnGroup[cptr->SpawnGroupType].stayInRegion) {
			BOOL outside = true;
			for (int sr = 0; sr < spawnGroup[cptr->SpawnGroupType].spawnRegionCh; sr++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMax * 256) outside = false;
			}
			if (outside) goto replace;
		}
		if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
			for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax * 256) goto replace;
			}
		}

	}

	cptr->tgtime = 0;
	cptr->tgx = p.x;
	cptr->tgz = p.z;
}

void SetNewTargetPlaceFish(TCharacter *cptr, float R)
{
	Vector3d p;
	int tr = 0;
replace:
	//PrintLog("-");//TEST202004091129

	/*
	p.x = cptr->pos.x + siRand(static_cast<int>((R/3)));
	p.z = cptr->pos.z + siRand(static_cast<int>((R/3)));
	if (p.x > cptr->pos.x) p.x += R * (2 / 3); else p.x -= R * (2 / 3);
	if (p.z > cptr->pos.z) p.z += R * (2 / 3); else p.z -= R * (2 / 3);
	*/

	p.x = cptr->pos.x + siRand(static_cast<int>((R)));
	p.z = cptr->pos.z + siRand(static_cast<int>((R)));

	if (p.x < 512) p.x = 512;
	if (p.x > 1018 * 256) p.x = 1018 * 256;
	if (p.z < 512) p.z = 512;
	if (p.z > 1018 * 256) p.z = 1018 * 256;

	tr++;
	if (tr < 128) {
		if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) < R * 0.7) goto replace;

		if (spawnGroup[cptr->SpawnGroupType].stayInRegion) {
			BOOL outside = true;
			for (int sr = 0; sr < spawnGroup[cptr->SpawnGroupType].spawnRegionCh; sr++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMax * 256) outside = false;
			}
			if (outside) goto replace;
		}
		if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
			for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax * 256) goto replace;
			}
		}

	}

	p.y = GetLandH(p.x, p.z);
	float wy = GetLandUpH(p.x, p.z) - GetLandH(p.x, p.z);


	int spcdm = 1;
	if (cptr->aquaticIdle) spcdm = 0.75;

	float targetDepthTemp;

	/*
	if (cptr->aquaticIdle) {
		targetDepthTemp = GetLandUpH(p.x, p.z) - (cptr->spcDepth * 0.75);
		goto skipY;
	}
	*/

	float tdistTemp = fabs(static_cast<float>(sqrt(
		((p.x - cptr->pos.x)*(p.x - cptr->pos.x)) +
		((p.z - cptr->pos.z) * (p.z - cptr->pos.z)))));
	/*
	float tdistTemp = fabs(static_cast<float>(sqrt(
		((p.x - cptr->pos.x)*(p.x - cptr->pos.x)) +
		((p.z - cptr->pos.z) * (p.z - cptr->pos.z)))) / 3);
	*/
	tr = 0;

	//PrintLog("fTY");//TEST202004091129

replace2:
	//PrintLog("-");//TEST202004091129
	//targetDepthTemp = siRand(static_cast<int>((R/3)));

	if (cptr->aquaticIdle) {
		targetDepthTemp = rRand(static_cast<int>((GetLandUpH(p.x, p.z) - (cptr->spcDepth * 0.68) - cptr->depth))); //target slightly higher so it doesn't take forever - correct to 0.75 later
	}
	else {
		targetDepthTemp = siRand(static_cast<int>((tdistTemp)));
	}

	tr++;

	/*
	if (cptr->aquaticIdle) {
		if (targetDepthTemp < 0) targetDepthTemp *= -1;
		if (cptr->depth > GetLandUpH(p.x, p.z) - (cptr->spcDepth * 1.1)) {
			targetDepthTemp = GetLandUpH(p.x, p.z) - (cptr->spcDepth * 0.75);
			goto skipY;
		}
	}
	*/

	//PREVENT TOO MUCH TURNING/bending
	if (tr < 1024) {
		float tbeta = -atan((targetDepthTemp) / tdistTemp);
		int dbeta = tbeta - cptr->beta;
		if (dbeta < 0) dbeta *= -1;
		if (dbeta > pi / 16) {
			goto replace2;
		}
	}

	/*
	if (tr < 1024) {
		if (fabs(targetDepthTemp) > tdistTemp) {
			if (targetDepthTemp > 0) {
				targetDepthTemp = tdistTemp;
			}
			else {
				targetDepthTemp = -tdistTemp;
			}
		}
	}
	*/

	targetDepthTemp = cptr->depth + targetDepthTemp;


	if (targetDepthTemp < GetLandH(p.x, p.z) + (cptr->spcDepth * spcdm)) {
		if (tr < 3024) {
			goto replace2;
		}
		else {
			targetDepthTemp = GetLandH(p.x, p.z) + (cptr->spcDepth * spcdm);
		}
	}
	if (targetDepthTemp > GetLandUpH(p.x, p.z) - (cptr->spcDepth * spcdm)) {
		if (tr < 3024) {
			goto replace2;
		}
		else {
			targetDepthTemp = GetLandUpH(p.x, p.z) - (cptr->spcDepth * spcdm);
		}
	}

	//skipY:

	cptr->tgtime = 0;
	cptr->tgx = p.x;
	cptr->tgz = p.z;
	cptr->tdepth = targetDepthTemp;
	cptr->lastTBeta = cptr->beta;
	cptr->turny = 0;
}

//OLD
void SetNewTargetPlaceMosasaurus(TCharacter *cptr, float R)
{
	Vector3d p;
	int tr = 0;
replace:
	p.x = cptr->pos.x + siRand(static_cast<int>(R));
	if (p.x < 512) p.x = 512;
	if (p.x > 1018 * 256) p.x = 1018 * 256;
	p.z = cptr->pos.z + siRand(static_cast<int>(R));
	if (p.z < 512) p.z = 512;
	if (p.z > 1018 * 256) p.z = 1018 * 256;

	tr++;
	//if (tr < 128)
	if (fabs(p.x - cptr->pos.x) + fabs(p.z - cptr->pos.z) < R / 2.f) goto replace;

	p.y = GetLandH(p.x, p.z);
	float wy = GetLandUpH(p.x, p.z) - GetLandH(p.x, p.z);

	if (tr < 8024)
	{
		if (wy < 1500) goto replace;
	}

	if (tr < 128)
	{
		
		if (spawnGroup[cptr->SpawnGroupType].stayInRegion) {
			BOOL outside = true;
			for (int sr = 0; sr < spawnGroup[cptr->SpawnGroupType].spawnRegionCh; sr++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].spawnRegion[sr].YMax * 256) outside = false;
			}
			if (outside) goto replace;
		}
		if (spawnGroup[cptr->SpawnGroupType].avoidRegionCh) {
			for (int ar = 0; ar < spawnGroup[cptr->SpawnGroupType].avoidRegionCh; ar++) {
				if (p.x > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMin * 256 &&
					p.x < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].XMax * 256 &&
					p.z > spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMin * 256 &&
					p.z < spawnGroup[cptr->SpawnGroupType].avoidRegion[ar].YMax * 256) goto replace;
			}
		}

	}

	float targetDepthTemp;
	float tdistTemp = fabs(static_cast<float>(sqrt(
		((p.x - cptr->pos.x)*(p.x - cptr->pos.x)) +
		((p.z - cptr->pos.z) * (p.z - cptr->pos.z)))) / 3);
	/*
	float tdistTemp = fabs(static_cast<float>(sqrt(
		((p.x - cptr->pos.x)*(p.x - cptr->pos.x)) +
		((p.z - cptr->pos.z) * (p.z - cptr->pos.z)))) / 3);
	*/
replace2:
	targetDepthTemp = siRand(static_cast<int>((R / 3)));
	//targetDepthTemp = siRand(static_cast<int>((R/3)));

	tr++;

	//PREVENT TOO MUCH TURNING
	if (tr < 1024) {
		float tbeta = -atan((targetDepthTemp) / tdistTemp);
		int dbeta = tbeta - cptr->beta;
		if (dbeta < 0) dbeta *= -1;
		if (dbeta > pi / 8) {
			goto replace2;
		}
	}


	if (tr < 1024) {
		if (fabs(targetDepthTemp) > tdistTemp) {
			if (targetDepthTemp > 0) {
				targetDepthTemp = tdistTemp;
			}
			else {
				targetDepthTemp = -tdistTemp;
			}
		}
	}

	targetDepthTemp = cptr->depth + targetDepthTemp;
	if (targetDepthTemp < GetLandH(p.x, p.z) + 400) {
		if (tr < 8024) {
			goto replace2;
		}
		else {
			targetDepthTemp = GetLandH(p.x, p.z) + 400;
		}
	}
	if (targetDepthTemp > GetLandUpH(p.x, p.z) - 700) {
		if (tr < 8024) {
			goto replace2;
		}
		else {
			targetDepthTemp = GetLandUpH(p.x, p.z) - 700;
		}
	}

	cptr->tgtime = 0;
	cptr->tgx = p.x;
	cptr->tgz = p.z;
	cptr->tdepth = targetDepthTemp;
	cptr->lastTBeta = cptr->beta;
	cptr->turny = 0;
}


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



void Characters_AddSecondaryOne(TCharacter *cptr)
{

	if (!spawnGroup[cptr->SpawnGroupType].moveForward) return;

	if (ChCount > 64) return;
	Characters[ChCount].CType = cptr->CType;
	Characters[ChCount].SpawnGroupType = cptr->SpawnGroupType;
	Characters[ChCount].Clone = cptr->Clone;
	Characters[ChCount].cpcpAquatic = cptr->cpcpAquatic;
	int tr = 0;
replace1:
	tr++;
	if (tr > 128) return;
	Characters[ChCount].pos.x = PlayerX + siRand(20040);
	Characters[ChCount].pos.z = PlayerZ + siRand(20040);
	Characters[ChCount].pos.y = GetLandH(Characters[ChCount].pos.x,
		Characters[ChCount].pos.z);

	
	BOOL outside = true;
	for (int sr = 0; sr < spawnGroup[Characters[ChCount].SpawnGroupType].spawnRegionCh; sr++) {
		if (Characters[ChCount].pos.x > spawnGroup[Characters[ChCount].SpawnGroupType].spawnRegion[sr].XMin * 256 &&
			Characters[ChCount].pos.x < spawnGroup[Characters[ChCount].SpawnGroupType].spawnRegion[sr].XMax * 256 &&
			Characters[ChCount].pos.z > spawnGroup[Characters[ChCount].SpawnGroupType].spawnRegion[sr].YMin * 256 &&
			Characters[ChCount].pos.z < spawnGroup[Characters[ChCount].SpawnGroupType].spawnRegion[sr].YMax * 256) outside = false;
	}
	if (outside) goto replace1;
	if (spawnGroup[Characters[ChCount].SpawnGroupType].avoidRegionCh) {
		for (int ar = 0; ar < spawnGroup[Characters[ChCount].SpawnGroupType].avoidRegionCh; ar++) {
			if (Characters[ChCount].pos.x > spawnGroup[Characters[ChCount].SpawnGroupType].avoidRegion[ar].XMin * 256 &&
				Characters[ChCount].pos.x < spawnGroup[Characters[ChCount].SpawnGroupType].avoidRegion[ar].XMax * 256 &&
				Characters[ChCount].pos.z > spawnGroup[Characters[ChCount].SpawnGroupType].avoidRegion[ar].YMin * 256 &&
				Characters[ChCount].pos.z < spawnGroup[Characters[ChCount].SpawnGroupType].avoidRegion[ar].YMax * 256) goto replace1;
		}
	}

	if (Characters[ChCount].Clone == AI_BRACH || Characters[ChCount].Clone == AI_BRACHDANGER || Characters[ChCount].Clone == AI_ICTH) {
		if (CheckPlaceCollisionBrahiP(Characters[ChCount].pos))goto replace1;
	} else if (Characters[ChCount].Clone == AI_FISH || Characters[ChCount].Clone == AI_MOSA) {
		if (CheckPlaceCollisionFishP(Characters[ChCount].pos,
			DinoInfo[Characters[ChCount].CType].minDepth,
			DinoInfo[Characters[ChCount].CType].maxDepth)) goto replace1;
	} else if (CheckPlaceCollisionP(Characters[ChCount].pos, Characters[ChCount].cpcpAquatic)) goto replace1;
	

	if (fabs(Characters[ChCount].pos.x - PlayerX) +
		fabs(Characters[ChCount].pos.z - PlayerZ) < 256 * 40)
		goto replace1;

	Characters[ChCount].tgx = Characters[ChCount].pos.x;
	Characters[ChCount].tgz = Characters[ChCount].pos.z;
	Characters[ChCount].tgtime = 0;

	if (Characters[ChCount].Clone == AI_FISH || Characters[ChCount].Clone == AI_MOSA) {
		Characters[ChCount].pos.y = GetLandUpH(Characters[ChCount].pos.x, Characters[ChCount].pos.z) -
			((GetLandUpH(Characters[ChCount].pos.x, Characters[ChCount].pos.z) - GetLandH(Characters[ChCount].pos.x, Characters[ChCount].pos.z)) / 2);
	}

	Characters[ChCount].packId = -1;	//Classic ambient- no pack hunting
	ResetCharacter(&Characters[ChCount]);
	ChCount++;
}



void MoveCharacterFish(TCharacter *cptr, float dx, float dz)
{
	//return;
	Vector3d p = cptr->pos;

	if (CheckPlaceCollisionFish(cptr, p, cptr->depth,
		DinoInfo[cptr->CType].maxDepth,
		DinoInfo[cptr->CType].minDepth))
	{
		cptr->pos.x += dx / 2;
		cptr->pos.z += dz / 2;
		return;
	}

	p.x += dx;
	p.z += dz;

	if (!CheckPlaceCollisionFish(cptr, p, cptr->depth,
		DinoInfo[cptr->CType].maxDepth,
		DinoInfo[cptr->CType].minDepth))
	{
		cptr->pos = p;
		return;
	}

	p = cptr->pos;
	p.x += dx / 2;
	p.z += dz / 2;
	if (!CheckPlaceCollisionFish(cptr, p, cptr->depth,
		DinoInfo[cptr->CType].maxDepth,
		DinoInfo[cptr->CType].minDepth)) cptr->pos = p;
	p = cptr->pos;

	p.x += dx / 4;
	//if (!CheckPlaceCollision2(p)) cptr->pos = p;
	p.z += dz / 4;
	//if (!CheckPlaceCollision2(p)) cptr->pos = p;
	cptr->pos = p;
}


//old
void MoveCharacterMosasaurus(TCharacter *cptr, float dx, float dz)
{
	//return;
	Vector3d p = cptr->pos;

	if (CheckPlaceCollisionMosasaurus(cptr, p, cptr->depth))
	{
		cptr->pos.x += dx / 2;
		cptr->pos.z += dz / 2;
		return;
	}

	p.x += dx;
	p.z += dz;

	if (!CheckPlaceCollisionMosasaurus(cptr, p, cptr->depth))
	{
		cptr->pos = p;
		return;
	}

	p = cptr->pos;
	p.x += dx / 2;
	p.z += dz / 2;
	if (!CheckPlaceCollisionMosasaurus(cptr, p, cptr->depth)) cptr->pos = p;
	p = cptr->pos;

	p.x += dx / 4;
	//if (!CheckPlaceCollision2(p)) cptr->pos = p;
	p.z += dz / 4;
	//if (!CheckPlaceCollision2(p)) cptr->pos = p;
	cptr->pos = p;
}



void MoveCharacter(TCharacter *cptr, float dx, float dz, BOOL wc, BOOL mc)
{
	//return;
	Vector3d p = cptr->pos;

	if (CheckPlaceCollision2(cptr, p, wc))
	{
		cptr->pos.x += dx / 2;
		cptr->pos.z += dz / 2;
		return;
	}

	p.x += dx;
	p.z += dz;

	if (!CheckPlaceCollision2(cptr, p, wc))
	{
		cptr->pos = p;
		return;
	}

	p = cptr->pos;
	p.x += dx / 2;
	p.z += dz / 2;
	if (!CheckPlaceCollision2(cptr, p, wc)) cptr->pos = p;
	p = cptr->pos;

	p.x += dx / 4;
	//if (!CheckPlaceCollision2(p)) cptr->pos = p;
	p.z += dz / 4;
	//if (!CheckPlaceCollision2(p)) cptr->pos = p;
	cptr->pos = p;
}




void MoveCharacter2(TCharacter *cptr, float dx, float dz)
{
	cptr->pos.x += dx;
	cptr->pos.z += dz;
}
















