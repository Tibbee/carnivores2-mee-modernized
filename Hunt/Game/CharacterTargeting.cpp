// CharacterTargeting.cpp -- auto-extracted from Characters.cpp
// ==========================================================================
// Auto-extracted from Characters.cpp
// ==========================================================================

#include "Hunt.h"
#include "CharacterInternal.h"

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

	//if (tr < 128)
	if (tr < 100)
	{
		if (!waterNear(p.x, p.z, 50)) goto replace;
		if (cptr->spawnAlt + 400 < GetLandUpH(p.x, p.z)) goto replace;
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

	float tdistTemp = fabs(static_cast<float>(sqrt(
		((p.x - cptr->pos.x)*(p.x - cptr->pos.x)) +
		((p.z - cptr->pos.z) * (p.z - cptr->pos.z)))));
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

	//PREVENT TOO MUCH TURNING/bending
	if (tr < 1024) {
		float tbeta = -atan((targetDepthTemp) / tdistTemp);
		int dbeta = tbeta - cptr->beta;
		if (dbeta < 0) dbeta *= -1;
		if (dbeta > pi / 16) {
			goto replace2;
		}
	}

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
