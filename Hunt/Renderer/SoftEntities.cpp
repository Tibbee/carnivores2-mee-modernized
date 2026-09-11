// ==========================================================================
// SoftEntities.cpp -- Software renderer character and entity rendering
//
// Split from the original monolithic RenderSoft.cpp.
// ==========================================================================

#include "Hunt.h"
#include "SoftInternal.h"

#ifdef _soft
namespace
{
void AddChRenderItem(int r, int type, int index)
{
  if (r < 0 || r > kViewDistanceMax) return;

  TCharListLine& line = ChRenderList[r];
  constexpr int kMaxItems = static_cast<int>(sizeof(line.Items) / sizeof(line.Items[0]));
  if (line.ICount >= kMaxItems) return;

  line.Items[line.ICount++] = {type, index};
}
}

void CreateChRenderList()
{
//=========== ship ================//
  Ship.rpos.x = Ship.pos.x - CameraX;
  Ship.rpos.y = Ship.pos.y - CameraY;
  Ship.rpos.z = Ship.pos.z - CameraZ;
  float r = static_cast<float>((std::max)(fabs(Ship.rpos.x), fabs(Ship.rpos.z)));
  int ri = -1 + static_cast<int>((r / 256.f + 1.6f));

  if (Ship.State!=-1)
    if (ri < ctViewR-6)
    {
      int h = static_cast<int>(((Ship.pos.y - GetLandUpH(Ship.pos.x, Ship.pos.z)) / 1.8));
//           AddShadowCircle(static_cast<int>(Ship.pos.x)+h, static_cast<int>(Ship.pos.z)+h, 1200, 24);
    }


  if (HARD3D) return;
  for (int c=0; c<=ctViewR; c++)
    ChRenderList[c].ICount = 0;


//=========== ship ================//

  if (Ship.State==-1) goto NOSHIP;
  if (ri < 0) ri = 0;
  if (ri < ctViewR)
  {
    Ship.rpos = RotateVector(Ship.rpos);
    if (Ship.rpos.z > BackViewR) goto NOSHIP;
    if ( fabs(Ship.rpos.x) > -Ship.rpos.z + BackViewR ) goto NOSHIP;

    AddChRenderItem(ri, 3, 0);
  }
NOSHIP:
  ;





  //=========== sship ================//
  SShip.rpos.x = SShip.pos.x - CameraX;
  SShip.rpos.y = SShip.pos.y - CameraY;
  SShip.rpos.z = SShip.pos.z - CameraZ;
  r = static_cast<float>((std::max)(fabs(SShip.rpos.x), fabs(SShip.rpos.z)));
  ri = -1 + static_cast<int>((r / 256.f + 1.6f));

  if (SShip.State < 1)
	  if (ri < ctViewR - 6)
	  {
		  int h = static_cast<int>(((SShip.pos.y - GetLandUpH(SShip.pos.x, SShip.pos.z)) / 1.8));
		  //           AddShadowCircle(static_cast<int>(Ship.pos.x)+h, static_cast<int>(Ship.pos.z)+h, 1200, 24);
	  }


  if (HARD3D) return;
  //for (int c = 0; c <= ctViewR; c++)
  //  ChRenderList[c].ICount = 0;


  //=========== sship ================//

  if (SShip.State == -1) goto NOSSHIP;
  if (ri < 0) ri = 0;
  if (ri < ctViewR)
  {
	  SShip.rpos = RotateVector(SShip.rpos);
	  if (SShip.rpos.z > BackViewR) goto NOSSHIP;
	  if (fabs(SShip.rpos.x) > -SShip.rpos.z + BackViewR) goto NOSSHIP;

	  AddChRenderItem(ri, 5, 0);
  }
NOSSHIP:
  ;






  //=========== bag ================//
  AmmoBag.rpos.x = AmmoBag.pos.x - CameraX;
  AmmoBag.rpos.y = AmmoBag.pos.y - CameraY;
  AmmoBag.rpos.z = AmmoBag.pos.z - CameraZ;
  r = static_cast<float>((std::max)(fabs(AmmoBag.rpos.x), fabs(AmmoBag.rpos.z)));
  ri = -1 + static_cast<int>((r / 256.f + 1.6f));

  if (AmmoBag.State < 1)
	  if (ri < ctViewR - 6)
	  {
		  int h = static_cast<int>(((AmmoBag.pos.y - GetLandUpH(AmmoBag.pos.x, AmmoBag.pos.z)) / 1.8));
		  //           AddShadowCircle(static_cast<int>(Ship.pos.x)+h, static_cast<int>(Ship.pos.z)+h, 1200, 24);
	  }


  if (HARD3D) return;
  //for (int c = 0; c <= ctViewR; c++)
  //	  ChRenderList[c].ICount = 0;


  //=========== bag ================//

  if (AmmoBag.State < 1) goto NOBAG;
  if (ri < 0) ri = 0;
  if (ri < ctViewR)
  {
	  AmmoBag.rpos = RotateVector(AmmoBag.rpos);
	  if (AmmoBag.rpos.z > BackViewR) goto NOBAG;
	  if (fabs(AmmoBag.rpos.x) > -AmmoBag.rpos.z + BackViewR) goto NOBAG;

	  AddChRenderItem(ri, 6, 0);
  }
NOBAG:
  ;





  for (int b = 0; b < bulletCh; b++) {
	  if (WeapInfo[bullet[b].parent].bullet) {


		  //=========== bullet ================//
		  bullet[b].rpos.x = bullet[b].a.x - CameraX;
		  bullet[b].rpos.y = bullet[b].a.y - CameraY;
		  bullet[b].rpos.z = bullet[b].a.z - CameraZ;
		  r = static_cast<float>((std::max)(fabs(bullet[b].rpos.x), fabs(bullet[b].rpos.z)));
		  ri = -1 + static_cast<int>((r / 256.f + 1.6f));

		  if (HARD3D) return;
		  //for (int c = 0; c <= ctViewR; c++)
		  //  ChRenderList[c].ICount = 0;


		  //=========== sship ================//

		  if (ri < 0) ri = 0;
		  if (ri < ctViewR)
		  {
			  bullet[b].rpos = RotateVector(bullet[b].rpos);
			  if (bullet[b].rpos.z > BackViewR) goto NOBULLET;
			  if (fabs(bullet[b].rpos.x) > -bullet[b].rpos.z + BackViewR) goto NOBULLET;

			  AddChRenderItem(ri, 7, b);
		  }
	  NOBULLET:
		  ;


	  }
  }


//============= Dinosaurs ====================//
  TCharacter *cptr;
  for (int c=0; c<ChCount; c++)
  {
    cptr = &Characters[c];
    cptr->rpos.x = cptr->pos.x - CameraX;
    cptr->rpos.y = cptr->pos.y - CameraY;
    cptr->rpos.z = cptr->pos.z - CameraZ;

    float r = static_cast<float>((std::max)(fabs(cptr->rpos.x), fabs(cptr->rpos.z)));
    int ri = -1 + static_cast<int>((r / 256.f + 0.5f));
    if (ri < 0) ri = 0;
    if (ri > ctViewR) continue;

    cptr->rpos = RotateVector(cptr->rpos);

    float br = BackViewR + DinoInfo[cptr->CType].Radius;
    if (cptr->rpos.z > br) continue;
    if ( fabs(cptr->rpos.x) > -cptr->rpos.z + br ) continue;
    if ( fabs(cptr->rpos.y) > -cptr->rpos.z + br ) continue;

    /*
          if (cptr->rpos.z > BackViewR + ) continue;
          if ( fabs(cptr->rpos.x) > -cptr->rpos.z + BackViewR ) continue;
    */
//      AddShadowCircle(static_cast<int>(cptr->pos.x)+100, static_cast<int>(cptr->pos.z)+100, 360, 16);

    AddChRenderItem(ri, 0, c);
  }






  if (Multiplayer) {

	  //============= Multiplayer ====================//

	  // test - 1 other player

	  TCharacter *cptr;
	  for (int c = 0; c < 1; c++)// 1 player, not playercount
	  {
		  cptr = &MPlayers[0];
		  cptr->rpos.x = cptr->pos.x - CameraX;
		  cptr->rpos.y = cptr->pos.y - CameraY;
		  cptr->rpos.z = cptr->pos.z - CameraZ;

		  float r = static_cast<float>((std::max)(fabs(cptr->rpos.x), fabs(cptr->rpos.z)));
		  int ri = -1 + static_cast<int>((r / 256.f + 0.5f));
		  if (ri < 0) ri = 0;
		  if (ri > ctViewR) continue;

		  cptr->rpos = RotateVector(cptr->rpos);

		  float br = BackViewR + 256;//hunter radius 256?
		  if (cptr->rpos.z > br) continue;
		  if (fabs(cptr->rpos.x) > -cptr->rpos.z + br) continue;
		  if (fabs(cptr->rpos.y) > -cptr->rpos.z + br) continue;

		  /*
				if (cptr->rpos.z > BackViewR + ) continue;
				if ( fabs(cptr->rpos.x) > -cptr->rpos.z + BackViewR ) continue;
		  */
		  //      AddShadowCircle(static_cast<int>(cptr->pos.x)+100, static_cast<int>(cptr->pos.z)+100, 360, 16);

		  AddChRenderItem(ri, 4, 0); //increase for each extra player
	  }


  }






}


void RenderChList(int r)
{
  if (HARD3D || r < 0 || r > kViewDistanceMax) return;
  for (int c=0; c<ChRenderList[r].ICount; c++)
  {
	  if (ChRenderList[r].Items[c].CType == 0) {
		  RenderCharacter(&Characters[ChRenderList[r].Items[c].Index]);
	  } else if (ChRenderList[r].Items[c].CType == 4) {
		  RenderCharacter(&MPlayers[ChRenderList[r].Items[c].Index]);
	  } else if (ChRenderList[r].Items[c].CType == 3) {
		  RenderShip();
	  }else if (ChRenderList[r].Items[c].CType == 5) {
		  RenderSShip();
	  }else if (ChRenderList[r].Items[c].CType == 6) {
		  RenderBag();
	  }else if (ChRenderList[r].Items[c].CType == 7) {
		  RenderBullet(ChRenderList[r].Items[c].Index);
	  }
	
  }
}



void RenderCharacter(TCharacter *cptr)
{

  float zs = static_cast<float>(VectorLength( cptr->rpos ));
  if (zs > ctViewR*256) return;

  GlassL = 0;
  if (zs > 256 * (ctViewR-4))
    GlassL = static_cast<int>((std::min)(255.0f, zs / 4.0f - 64.0f * (ctViewR - 4)));

  CreateChMorphedModel(cptr);


  float wh = GetLandUpH(cptr->pos.x, cptr->pos.z);
  waterclip = false;

  if (!IsUnderwater())
    if (wh > cptr->pos.y + 32*2)
    {
      waterclipbase.x = cptr->pos.x - CameraX;
      waterclipbase.y = wh - CameraY;
      waterclipbase.z = cptr->pos.z - CameraZ;
      waterclipbase = RotateVector(waterclipbase);
      waterclip = true;
    }


  if ( fabs(cptr->rpos.z) + fabs(cptr->rpos.x) <2560)
    RenderModelClip(cptr->pinfo->mptr.get(),
                    cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 240, 0,
                    -cptr->alpha + pi / 2 + CameraAlpha,
                    CameraBeta );
  else if (waterclip)
    RenderModelClipWater(cptr->pinfo->mptr.get(),
                         cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 240, 0,
                         -cptr->alpha + pi / 2 + CameraAlpha,
                         CameraBeta );
  else
    RenderModel(cptr->pinfo->mptr.get(),
                cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 240,  0,
                -cptr->alpha + pi / 2 + CameraAlpha,
                CameraBeta );
}


void RenderBag()
{
	float zs = static_cast<float>(VectorLength(AmmoBag.rpos));
	if (zs > ctViewR * 256) return;

	GlassL = 0;
	if (zs > 256 * (ctViewR - 4))
		GlassL = static_cast<int>((std::min)(255.0f, zs / 4.0f - 64.0f * (ctViewR - 4)));


	CreateMorphedModel(BagModel.mptr.get(), &BagModel.Animation[0], AmmoBag.FTime, 1.0);

	if (fabs(AmmoBag.rpos.z) < 4000)
		RenderModelClip(BagModel.mptr.get(),
			AmmoBag.rpos.x, AmmoBag.rpos.y, AmmoBag.rpos.z, 240, 0, -0 - pi / 2 + CameraAlpha, CameraBeta);
	else
		RenderModel(BagModel.mptr.get(),
			AmmoBag.rpos.x, AmmoBag.rpos.y, AmmoBag.rpos.z, 240, 0, -0 - pi / 2 + CameraAlpha, CameraBeta);
}



void RenderSShip()
{
	float zs = static_cast<float>(VectorLength(SShip.rpos));
	if (zs > ctViewR * 256) return;

	GlassL = 0;
	if (zs > 256 * (ctViewR - 4))
		GlassL = static_cast<int>((std::min)(255.0f, zs / 4.0f - 64.0f * (ctViewR - 4)));


	CreateMorphedModelBetaGamma(SShipModel.mptr.get(), &SShipModel.Animation[0], SShip.FTime, 1.0, SShip.beta, SShip.gamma);

	if (fabs(SShip.rpos.z) < 4000)
		RenderModelClip(SShipModel.mptr.get(),
			SShip.rpos.x, SShip.rpos.y, SShip.rpos.z, 240, 0, -SShip.alpha - pi / 2 + CameraAlpha, CameraBeta);
	else
		RenderModel(SShipModel.mptr.get(),
			SShip.rpos.x, SShip.rpos.y, SShip.rpos.z, 240, 0, -SShip.alpha - pi / 2 + CameraAlpha, CameraBeta);
}




void RenderShip()
{
  float zs = static_cast<float>(VectorLength( Ship.rpos ));
  if (zs > ctViewR*256) return;

  GlassL = 0;
  if (zs > 256 * (ctViewR-4))
    GlassL = static_cast<int>((std::min)(255.0f, zs / 4.0f - 64.0f * (ctViewR - 4)));


  CreateMorphedModel(ShipModel.mptr.get(), &ShipModel.Animation[0], Ship.FTime, 1.0);

  if ( fabs(Ship.rpos.z)  < 4000)
    RenderModelClip(ShipModel.mptr.get(),
                    Ship.rpos.x, Ship.rpos.y, Ship.rpos.z, 240, 0, -Ship.alpha -pi/2 + CameraAlpha, CameraBeta);
  else
    RenderModel(ShipModel.mptr.get(),
                Ship.rpos.x, Ship.rpos.y, Ship.rpos.z, 240, 0, -Ship.alpha -pi/2 + CameraAlpha, CameraBeta);
}


void RenderBullet(int b)
{
	float zs = static_cast<float>(VectorLength(bullet[b].rpos));
	if (zs > ctViewR * 256) return;

	GlassL = 0;
	if (zs > 256 * (ctViewR - 4))
		GlassL = static_cast<int>((std::min)(255.0f, zs / 4.0f - 64.0f * (ctViewR - 4)));


	CreateMorphedModelBetaGamma(Weapon.Bullet[bullet[b].parent].mptr.get(), &Weapon.Bullet[bullet[b].parent].Animation[0], bullet[b].FTime, 1.0, bullet[b].beta, 0);

	// Vanilla formula (ship convention -- see GLRenderer.cpp): the morph above
	// already applied the trajectory pitch; render adds only yaw + camera.
	if (fabs(bullet[b].rpos.z) < 4000)
		RenderModelClip(Weapon.Bullet[bullet[b].parent].mptr.get(),
			bullet[b].rpos.x, bullet[b].rpos.y, bullet[b].rpos.z, 240, 0, -bullet[b].alpha - pi / 2 + CameraAlpha, CameraBeta);
	else
		RenderModel(Weapon.Bullet[bullet[b].parent].mptr.get(),
			bullet[b].rpos.x, bullet[b].rpos.y, bullet[b].rpos.z, 240, 0, -bullet[b].alpha - pi / 2 + CameraAlpha, CameraBeta);
}





#endif // _soft
