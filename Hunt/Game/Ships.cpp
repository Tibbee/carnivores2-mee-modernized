// Ships.cpp — auto-extracted from Game.cpp
// ==========================================================================
// Auto-extracted from Game.cpp
// ==========================================================================

#include "Hunt.h"

// Imported from Projectiles.cpp
extern void RemoveCharacter(int index);

void AnimateBag() {
	if (AmmoBag.State == 1) {
		AmmoBag.pos.y -= 4 * TimeDt;
		if (GetLandH(AmmoBag.pos.x, AmmoBag.pos.z) > AmmoBag.pos.y) {
			AmmoBag.pos.y = GetLandH(AmmoBag.pos.x, AmmoBag.pos.z);
			AmmoBag.State = 2;
			AddVoice3d(BagModel.SoundFX[0].length, BagModel.SoundFX[0].lpData.data(),
				AmmoBag.pos.x, AmmoBag.pos.y, AmmoBag.pos.z);
		}
	}
	if (AmmoBag.State == 2) {
		AmmoBag.FTime += TimeDt;
		if (AmmoBag.FTime >= BagModel.Animation[0].AniTime) {
			AmmoBag.State = 3;
			AmmoBag.FTime = 0;
		}
	}

	if (VectorLength(SubVectors(PlayerPos, AmmoBag.pos)) < 200.f && AmmoBag.State > 0) {
		AddVoicev(BagModel.SoundFX[1].length,
			BagModel.SoundFX[1].lpData.data(), 256);
		AmmoBag.State = -1;
		refillWeapons(false);
	}

}
void AnimateSShip() {
	if (SShip.State < 1) return;

	SetAmbient3d(SShipModel.SoundFX[0].length,
		SShipModel.SoundFX[0].lpData.data(),
		SShip.pos.x, SShip.pos.y, SShip.pos.z);
	
	int _TimeDt = TimeDt;

	TBEGIN:

	float L = VectorLength(SubVectors2d(SShip.tgpos, SShip.pos));
	float L2 = sqrt((SShip.tgpos.x - SShip.pos.x) * (SShip.tgpos.x - SShip.pos.x) +
		(SShip.tgpos.x - SShip.pos.x) * (SShip.tgpos.x - SShip.pos.x));

	if (L < 256.f && SShip.State == 1) {
		SShip.tgpos = SShip.retpos;
		SShip.State = 2;
		AmmoBag.pos = SShip.pos;
		AmmoBag.State = 1;
		goto TBEGIN;
	}
	if (SShip.State == 2 && VectorLength(SubVectors(PlayerPos, SShip.pos)) > (ctViewR + 2) * 256){
		SShip.State = -1;
		return;
	}

	SShip.pos.y += 0.3f*static_cast<float>(cos(RealTime / 256.f));

	SShip.tgalpha = FindVectorAlpha(SShip.tgpos.x - SShip.pos.x, SShip.tgpos.z - SShip.pos.z);
	float currspeed;
	float dalpha = static_cast<float>(fabs(SShip.tgalpha - SShip.alpha));
	float drspd = dalpha;
	if (drspd > pi) drspd = 2 * pi - drspd;

	if (VectorLength(SubVectors(PlayerPos, SShip.pos)) < (ctViewR + 2) * 256 && SShip.State != 1 && dalpha < 1)
	{
		SShip.tgpos.x += static_cast<float>(cos(SShip.alpha)) * 256 * 6.f;
		SShip.tgpos.z += static_cast<float>(sin(SShip.alpha)) * 256 * 6.f;
	}


	//====== speed ===============//
	float vspeed = 64;
	//if (fabs(dalpha) > 0.4) vspeed = 0.f;
	float _s = SShip.speed;
	if (vspeed > SShip.speed) DeltaFunc(SShip.speed, vspeed, TimeDt / 200.f);
	else SShip.speed = vspeed;

	//====== fly ===========//
	float l = TimeDt * SShip.speed / 16.f;
	Vector3d _pos = SShip.pos;
	SShip.pos.x += static_cast<float>(cos(SShip.alpha))*l;
	SShip.pos.z += static_cast<float>(sin(SShip.alpha))*l;

	//======= y movement ============//
	float h = GetLandUpH(SShip.pos.x, SShip.pos.z);
	DeltaFunc(SShip.pos.y, SShip.tgpos.y, TimeDt / 4.f);
	if (SShip.pos.y < h + 1024)
	{
		SShip.pos.y = h + 1024;
	}



	//======= rotation ============//

	float tggamma = SShip.alpha;

	if (SShip.tgalpha > SShip.alpha) currspeed = 0.1f + static_cast<float>(fabs(drspd)) / 2.f;
	else currspeed = -0.1f - static_cast<float>(fabs(drspd)) / 2.f;

	if (fabs(dalpha) > pi) currspeed = -currspeed;


	DeltaFunc(SShip.rspeed, currspeed, static_cast<float>(TimeDt) / 420.f);

	float rspd = SShip.rspeed * TimeDt / 2024.f;
	if (fabs(drspd) < fabs(rspd))
	{
		SShip.alpha = SShip.tgalpha;
		SShip.rspeed /= 2;
	}
	else
	{
		SShip.alpha += rspd;
	}

	tggamma -= SShip.alpha;
	tggamma *= 100;
	if (SShip.alpha < 0) SShip.alpha += pi * 2;
	if (SShip.alpha > pi * 2) SShip.alpha -= pi * 2;

	float curgspeed;
	float dgamma = static_cast<float>(fabs(tggamma - SShip.gamma));
	float dgspd = dgamma;
	if (dgspd > pi) dgspd = 2 * pi - dgspd;
	if (tggamma > SShip.gamma) curgspeed = 0.1f + static_cast<float>(fabs(dgspd)) / 2.f;
	else curgspeed = -0.1f - static_cast<float>(fabs(dgspd)) / 2.f;
	curgspeed *= 2;

	if (fabs(dgamma) > pi) curgspeed = -curgspeed;

	DeltaFunc(SShip.gspeed, curgspeed, static_cast<float>(TimeDt) / 420.f);

	float gspd = SShip.gspeed * TimeDt / 2024.f;
	if (fabs(dgspd) < fabs(gspd))
	{
		SShip.gamma = tggamma;
		SShip.gspeed /= 2;
	}
	else
	{
		SShip.gamma += gspd;
	}
//	if (SShip.gamma < 0) SShip.gamma += pi * 2;
//	if (SShip.gamma > pi * 2) SShip.gamma -= pi * 2;

	//beta

	//bullet[b].beta = FindVectorAlpha(
	//sqrt(bullet[b].dif.z*bullet[b].dif.z +
	//	bullet[b].dif.x*bullet[b].dif.x), bullet[b].dif.y);

	
}
void AnimateShip()
{
  if (Ship.State==-1)
  {
    SetAmbient3d(0,0, 0,0,0);
    if (!ShipTask.tcount) return;
    InitShip(ShipTask.clist[0]);
    memcpy(&ShipTask.clist[0], &ShipTask.clist[1], 250*4);
    ShipTask.tcount--;
    return;
  }

  SetAmbient3d(ShipModel.SoundFX[0].length,
               ShipModel.SoundFX[0].lpData.data(),
               Ship.pos.x, Ship.pos.y, Ship.pos.z);

  int _TimeDt = TimeDt;

//====== get up/down time acceleration ===========//
  if (Ship.FTime)
  {
    int am = ShipModel.Animation[0].AniTime;
    if (Ship.FTime < 500) _TimeDt = TimeDt * (Ship.FTime + 48) / 548;
    if (am-Ship.FTime < 500) _TimeDt = TimeDt * (am-Ship.FTime + 48) / 548;
    if (_TimeDt<2) _TimeDt=2;
  }
//===================================

  float L  = VectorLength( SubVectors(Ship.tgpos, Ship.pos) );
  float L2 = sqrt ( (Ship.tgpos.x - Ship.pos.x) * (Ship.tgpos.x - Ship.pos.x) +
                    (Ship.tgpos.x - Ship.pos.x) * (Ship.tgpos.x - Ship.pos.x) );

  Ship.pos.y+=0.3f*static_cast<float>(cos(RealTime / 256.f));



  Ship.tgalpha    = FindVectorAlpha(Ship.tgpos.x - Ship.pos.x, Ship.tgpos.z - Ship.pos.z);
  float currspeed;
  float dalpha = static_cast<float>(fabs(Ship.tgalpha - Ship.alpha));
  float drspd = dalpha;
  if (drspd>pi) drspd = 2*pi - drspd;


//====== fly more away if I near =============//
  if (Ship.State)
    if (Ship.speed>1)
      if (L<4000)
        if (VectorLength(SubVectors(PlayerPos, Ship.pos))<(ctViewR+2)*256)
        {
          Ship.tgpos.x += static_cast<float>(cos(Ship.alpha)) * 256*6.f;
          Ship.tgpos.z += static_cast<float>(sin(Ship.alpha)) * 256*6.f;
          Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + Ship.DeltaY;
          Ship.tgpos.y = MAX(Ship.tgpos.y, GetLandUpH(Ship.pos.x, Ship.pos.z) + Ship.DeltaY);
        }
//==============================//



//========= animate down ==========//
  if (Ship.State==3)
  {
    Ship.FTime+=_TimeDt;
    if (Ship.FTime>=ShipModel.Animation[0].AniTime)
    {
      Ship.FTime=ShipModel.Animation[0].AniTime-1;
      Ship.State=2;
      AddVoicev(ShipModel.SoundFX[4].length,
                ShipModel.SoundFX[4].lpData.data(), 256);
      AddVoice3d(ShipModel.SoundFX[1].length, ShipModel.SoundFX[1].lpData.data(),
                 Ship.pos.x, Ship.pos.y, Ship.pos.z);
    }
    return;
  }


//========= get body on board ==========//
  if (Ship.State)
  {
    if (Ship.cindex!=-1)
    {
      DeltaFunc(Characters[Ship.cindex].pos.y, Ship.pos.y-650 - (Ship.DeltaY-2048), _TimeDt / 3.f);
      DeltaFunc(Characters[Ship.cindex].beta,  0, TimeDt / 4048.f);
      DeltaFunc(Characters[Ship.cindex].gamma, 0, TimeDt / 4048.f);
    }

    if (Ship.State==2)
    {
      Ship.FTime-=_TimeDt;
      if (Ship.FTime<0) Ship.FTime=0;

      if (Ship.FTime==0)
        if (fabs(Characters[Ship.cindex].pos.y - (Ship.pos.y-650 - (Ship.DeltaY-2048))) < 1.f)
        {
          Ship.State = 1;
          AddVoicev(ShipModel.SoundFX[5].length,
                    ShipModel.SoundFX[5].lpData.data(), 256);
          AddVoice3d(ShipModel.SoundFX[2].length, ShipModel.SoundFX[2].lpData.data(),
                     Ship.pos.x, Ship.pos.y, Ship.pos.z);
        }
      return;
    }
  }
//=====================================//


//====== speed ===============//
  float vspeed = 1.f + L / 128.f;
  if (vspeed > 24) vspeed = 24;
  if (Ship.State) vspeed = 24;
  if (fabs(dalpha) > 0.4) vspeed = 0.f;
  float _s = Ship.speed;
  if (vspeed>Ship.speed) DeltaFunc(Ship.speed, vspeed, TimeDt / 200.f);
  else Ship.speed = vspeed;

  if (Ship.speed>0 && _s==0)
    AddVoice3d(ShipModel.SoundFX[2].length, ShipModel.SoundFX[2].lpData.data(),
               Ship.pos.x, Ship.pos.y, Ship.pos.z);

//====== fly ===========//
  float l = TimeDt * Ship.speed / 16.f;

  if (fabs(dalpha) < 0.4)
    if (l<L)
    {
      if (l>L2) l = L2 * 0.5f;
      if (L2<0.1) l = 0;
      Ship.pos.x += static_cast<float>(cos(Ship.alpha))*l;
      Ship.pos.z += static_cast<float>(sin(Ship.alpha))*l;
    }
    else
    {
      if (Ship.State)
      {
        Ship.State = -1;
        RemoveCharacter(Ship.cindex);
        return;
      }
      else
      {
        Ship.pos = Ship.tgpos;
        Ship.State = 3;
        Ship.FTime = 1;
        Ship.tgpos = Ship.retpos;
        Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + Ship.DeltaY;
        Ship.tgpos.y = MAX(Ship.tgpos.y, GetLandUpH(Ship.pos.x, Ship.pos.z) + Ship.DeltaY);
        Characters[Ship.cindex].StateF = 0xFF;
        AddVoice3d(ShipModel.SoundFX[1].length, ShipModel.SoundFX[1].lpData.data(),
                   Ship.pos.x, Ship.pos.y, Ship.pos.z);
      }
    }

//======= y movement ============//
  float h = GetLandUpH(Ship.pos.x, Ship.pos.z);
  DeltaFunc(Ship.pos.y, Ship.tgpos.y, TimeDt / 4.f);
  if (Ship.pos.y < h + 1024)
  {
    if (Ship.State)
      if (Ship.cindex!=-1)
        Characters[Ship.cindex].pos.y+= h + 1024 - Ship.pos.y;
    Ship.pos.y = h + 1024;
  }



//======= rotation ============//

  if (Ship.tgalpha > Ship.alpha) currspeed = 0.1f + static_cast<float>(fabs(drspd))/2.f;
  else currspeed =-0.1f - static_cast<float>(fabs(drspd))/2.f;

  if (fabs(dalpha) > pi) currspeed=-currspeed;


  DeltaFunc(Ship.rspeed, currspeed, static_cast<float>(TimeDt) / 420.f);

  float rspd=Ship.rspeed * TimeDt / 1024.f;
  if (fabs(drspd) < fabs(rspd))
  {
    Ship.alpha = Ship.tgalpha;
    Ship.rspeed/=2;
  }
  else
  {
    Ship.alpha+=rspd;
    if (Ship.State)
      if (Ship.cindex!=-1)
        Characters[Ship.cindex].alpha+=rspd;
  }

  if (Ship.alpha<0) Ship.alpha+=pi*2;
  if (Ship.alpha>pi*2) Ship.alpha-=pi*2;

//======== move body ===========//
  if (Ship.State)
  {
    if (Ship.cindex!=-1)
    {
      Characters[Ship.cindex].pos.x = Ship.pos.x;
      Characters[Ship.cindex].pos.z = Ship.pos.z;
    }
    if (L>1000) Ship.tgpos.y+=TimeDt / 12.f;
  }
  else
  {
    Ship.tgpos.x = Characters[Ship.cindex].pos.x;
    Ship.tgpos.z = Characters[Ship.cindex].pos.z;
    Ship.tgpos.y = GetLandUpH(Ship.tgpos.x, Ship.tgpos.z) + Ship.DeltaY;
    Ship.tgpos.y = MAX(Ship.tgpos.y, GetLandUpH(Ship.pos.x, Ship.pos.z) + Ship.DeltaY);
  }



}
