// AnimatePoacher.cpp — auto-extracted from CharacterAnimation.cpp
// ==========================================================================
// Auto-generated from CharacterAnimation.cpp
// ==========================================================================

#include "Hunt.h"
#include "../CharacterInternal.h"

// Global state imported from StateDefs.cpp
extern int CurDino;
extern int NewPhase;

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
