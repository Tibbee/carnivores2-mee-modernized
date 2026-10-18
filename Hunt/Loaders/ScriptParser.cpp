// ==========================================================================
// ScriptParser.cpp -- _RES.TXT / _MENU.TXT script file parser
// ==========================================================================

#include "Hunt.h"
#include "Core/CommandLineParse.h"
#include "LoadDiagnostics.h"
#include "LoadValidate.h"
#include "ScriptBlockParse.h"
#include "ScriptValueParse.h"
#include "ScriptParserInternal.h"

void LoadResourcesScript()
{

	SurvivalIndexCh = 0;

	//mosh/dimet waterlevel 50
	//gall waterlevel 100

	//these ai can detect player with sight or scent. Maybe make this a res option at some point.
	AIInfo[AI_PARA].sniffer = true;
	AIInfo[AI_ANKY].sniffer = true;
	AIInfo[AI_STEGO].sniffer = true;
	AIInfo[AI_CHASM].sniffer = true;
	AIInfo[AI_ALLO].sniffer = true;
	AIInfo[AI_VELO].sniffer = true;
	AIInfo[AI_SPINO].sniffer = true;
	AIInfo[AI_CERAT].sniffer = true;
	AIInfo[AI_TREX].sniffer = true;
	AIInfo[AI_PACH].sniffer = true;
	AIInfo[AI_MICRO].sniffer = true;
	AIInfo[AI_TITAN].sniffer = true;
	AIInfo[AI_BRONT].sniffer = true;
	AIInfo[AI_HOG].sniffer = true;
	AIInfo[AI_WOLF].sniffer = true;
	AIInfo[AI_RHINO].sniffer = true;
	AIInfo[AI_DEER].sniffer = true;
	AIInfo[AI_SMILO].sniffer = true;
	AIInfo[AI_MAMM].sniffer = true;
	AIInfo[AI_BEAR].sniffer = true;

	AIInfo[AI_HUNTDOG].targetDistance = 8048.f;
	AIInfo[AI_HUNTDOG].noWayCntMin = 8;
	AIInfo[AI_HUNTDOG].noFindWayMed = 44;
	AIInfo[AI_HUNTDOG].noFindWayRange = 80;
	AIInfo[AI_HUNTDOG].targetBendRotSpd = 3.0f;
	AIInfo[AI_HUNTDOG].targetBendMin = 2.f;
	AIInfo[AI_HUNTDOG].targetBendDelta1 = 1600.f;
	AIInfo[AI_HUNTDOG].targetBendDelta2 = 1200.f;
	AIInfo[AI_HUNTDOG].walkTargetGammaRot = 12.0f;
	AIInfo[AI_HUNTDOG].targetGammaRot = 8.0f;
	AIInfo[AI_HUNTDOG].idleStart = 120;
	AIInfo[AI_HUNTDOG].yBetaGamma4 = 0.4f;
	AIInfo[AI_HUNTDOG].idleStartD = 128;

	AIInfo[AI_PARA].targetDistance = 8048.f;
	AIInfo[AI_PARA].agressMulti = 128;
	AIInfo[AI_PARA].noWayCntMin = 8;
	AIInfo[AI_PARA].noFindWayMed = 44;
	AIInfo[AI_PARA].noFindWayRange = 80;
	AIInfo[AI_PARA].targetBendRotSpd = 3.0f;
	AIInfo[AI_PARA].targetBendMin = 2.f;
	AIInfo[AI_PARA].targetBendDelta1 = 1600.f;
	AIInfo[AI_PARA].targetBendDelta2 = 1200.f;
	AIInfo[AI_PARA].walkTargetGammaRot = 12.0f;
	AIInfo[AI_PARA].targetGammaRot = 8.0f;
	AIInfo[AI_PARA].idleStart = 120;
	AIInfo[AI_PARA].yBetaGamma1 = 128;
	AIInfo[AI_PARA].yBetaGamma2 = 64;
	AIInfo[AI_PARA].yBetaGamma3 = 0.6f;
	AIInfo[AI_PARA].yBetaGamma4 = 0.4f;
	AIInfo[AI_PARA].tGAIncrement = 3.f;
	AIInfo[AI_PARA].rot1 = 0.2f;
	AIInfo[AI_PARA].rot2 = 1.0f;
	//AIInfo[AI_PARA].weaveRange = 0;
	AIInfo[AI_PARA].pWMin = 2048;
	//waterLevel = 160;

	AIInfo[AI_BRONT].iceAge = true;
	AIInfo[AI_BRONT].targetDistance = 8048.f;
	AIInfo[AI_BRONT].agressMulti = 256;
	AIInfo[AI_BRONT].noWayCntMin = 8;
	AIInfo[AI_BRONT].noFindWayMed = 48;
	AIInfo[AI_BRONT].noFindWayRange = 80;
	AIInfo[AI_BRONT].targetBendRotSpd = 3.5f;
	AIInfo[AI_BRONT].targetBendMin = 2.f;
	AIInfo[AI_BRONT].targetBendDelta1 = 1600.f;
	AIInfo[AI_BRONT].targetBendDelta2 = 1200.f;
	AIInfo[AI_BRONT].walkTargetGammaRot = 12.0f;
	AIInfo[AI_BRONT].targetGammaRot = 8.0f;
	AIInfo[AI_BRONT].idleStart = 124;
	AIInfo[AI_BRONT].yBetaGamma1 = 128;
	AIInfo[AI_BRONT].yBetaGamma2 = 64;
	AIInfo[AI_BRONT].yBetaGamma3 = 0.6f;
	AIInfo[AI_BRONT].yBetaGamma4 = 0.3f;
	AIInfo[AI_BRONT].tGAIncrement = 3.f;
	AIInfo[AI_BRONT].rot1 = 0.2f;
	AIInfo[AI_BRONT].rot2 = 1.0f;
	//AIInfo[AI_BRONT].weaveRange = 3072;
	AIInfo[AI_BRONT].pWMin = 2048;

	AIInfo[AI_HOG].iceAge = true;
	AIInfo[AI_HOG].targetDistance = 8048.f;
	AIInfo[AI_HOG].agressMulti = 256;
	AIInfo[AI_HOG].noWayCntMin = 8;
	AIInfo[AI_HOG].noFindWayMed = 48;
	AIInfo[AI_HOG].noFindWayRange = 80;
	AIInfo[AI_HOG].targetBendRotSpd = 3.5f;
	AIInfo[AI_HOG].targetBendMin = 2.f;
	AIInfo[AI_HOG].targetBendDelta1 = 1600.f;
	AIInfo[AI_HOG].targetBendDelta2 = 1200.f;
	AIInfo[AI_HOG].walkTargetGammaRot = 12.0f;
	AIInfo[AI_HOG].targetGammaRot = 8.0f;
	AIInfo[AI_HOG].idleStart = 124;
	AIInfo[AI_HOG].yBetaGamma1 = 128;
	AIInfo[AI_HOG].yBetaGamma2 = 64;
	AIInfo[AI_HOG].yBetaGamma3 = 0.6f;
	AIInfo[AI_HOG].yBetaGamma4 = 0.3f;
	AIInfo[AI_HOG].tGAIncrement = 3.f;
	AIInfo[AI_HOG].rot1 = 0.3f;
	AIInfo[AI_HOG].rot2 = 1.4f;
	//AIInfo[AI_HOG].weaveRange = 3072;
	AIInfo[AI_HOG].pWMin = 2048;

	AIInfo[AI_BEAR].iceAge = true;
	AIInfo[AI_BEAR].targetDistance = 8048.f;
	AIInfo[AI_BEAR].agressMulti = 256;
	AIInfo[AI_BEAR].noWayCntMin = 8;
	AIInfo[AI_BEAR].noFindWayMed = 48;
	AIInfo[AI_BEAR].noFindWayRange = 80;
	AIInfo[AI_BEAR].targetBendRotSpd = 3.5f;
	AIInfo[AI_BEAR].targetBendMin = 2.f;
	AIInfo[AI_BEAR].targetBendDelta1 = 1600.f;
	AIInfo[AI_BEAR].targetBendDelta2 = 1200.f;
	AIInfo[AI_BEAR].walkTargetGammaRot = 12.0f;
	AIInfo[AI_BEAR].targetGammaRot = 8.0f;
	AIInfo[AI_BEAR].idleStart = 124;
	AIInfo[AI_BEAR].yBetaGamma1 = 128;
	AIInfo[AI_BEAR].yBetaGamma2 = 64;
	AIInfo[AI_BEAR].yBetaGamma3 = 0.6f;
	AIInfo[AI_BEAR].yBetaGamma4 = 0.3f;
	AIInfo[AI_BEAR].tGAIncrement = 3.f;
	AIInfo[AI_BEAR].rot1 = 0.2f;
	AIInfo[AI_BEAR].rot2 = 1.0f;
	//AIInfo[AI_BEAR].weaveRange = 0;
	AIInfo[AI_BEAR].pWMin = 2048;
	//AIInfo[AI_BEAR].carnivore = true; //Bear has more in common with herbivore ai

	AIInfo[AI_RHINO].iceAge = true;
	AIInfo[AI_RHINO].targetDistance = 8048.f;
	AIInfo[AI_RHINO].agressMulti = 256;
	AIInfo[AI_RHINO].noWayCntMin = 8;
	AIInfo[AI_RHINO].noFindWayMed = 48;
	AIInfo[AI_RHINO].noFindWayRange = 80;
	AIInfo[AI_RHINO].targetBendRotSpd = 3.5f;
	AIInfo[AI_RHINO].targetBendMin = 2.f;
	AIInfo[AI_RHINO].targetBendDelta1 = 1600.f;
	AIInfo[AI_RHINO].targetBendDelta2 = 1200.f;
	AIInfo[AI_RHINO].walkTargetGammaRot = 12.0f;
	AIInfo[AI_RHINO].targetGammaRot = 8.0f;
	AIInfo[AI_RHINO].idleStart = 124;
	AIInfo[AI_RHINO].yBetaGamma1 = 128;
	AIInfo[AI_RHINO].yBetaGamma2 = 64;
	AIInfo[AI_RHINO].yBetaGamma3 = 0.6f;
	AIInfo[AI_RHINO].yBetaGamma4 = 0.3f;
	AIInfo[AI_RHINO].tGAIncrement = 3.f;
	AIInfo[AI_RHINO].rot1 = 0.3f;
	AIInfo[AI_RHINO].rot2 = 1.2f;
	//AIInfo[AI_RHINO].weaveRange = 3072;
	AIInfo[AI_RHINO].pWMin = 2048;

	AIInfo[AI_DEER].iceAge = true;
	AIInfo[AI_DEER].targetDistance = 8048.f;
	AIInfo[AI_DEER].agressMulti = 256;
	AIInfo[AI_DEER].noWayCntMin = 12;
	AIInfo[AI_DEER].noFindWayMed = 32;
	AIInfo[AI_DEER].noFindWayRange = 60;
	AIInfo[AI_DEER].targetBendRotSpd = 2.f;
	AIInfo[AI_DEER].targetBendMin = 3.f;
	AIInfo[AI_DEER].targetBendDelta1 = 2000.f;
	AIInfo[AI_DEER].targetBendDelta2 = 2000.f;
	AIInfo[AI_DEER].walkTargetGammaRot = 16.0f;
	AIInfo[AI_DEER].targetGammaRot = 10.0f;
	AIInfo[AI_DEER].idleStart = 124;
	AIInfo[AI_DEER].yBetaGamma1 = 128;
	AIInfo[AI_DEER].yBetaGamma2 = 64;
	AIInfo[AI_DEER].yBetaGamma3 = 0.6f;
	AIInfo[AI_DEER].yBetaGamma4 = 0.4f;
	AIInfo[AI_DEER].tGAIncrement = 3.f;
	AIInfo[AI_DEER].rot1 = 0.2f;
	AIInfo[AI_DEER].rot2 = 1.0f;
	//AIInfo[AI_DEER].weaveRange = 0;
	AIInfo[AI_DEER].pWMin = 2048;

	AIInfo[AI_MAMM].iceAge = true;
	AIInfo[AI_MAMM].targetDistance = 8048.f;
	AIInfo[AI_MAMM].agressMulti = 256;
	AIInfo[AI_MAMM].noWayCntMin = 12;
	AIInfo[AI_MAMM].noFindWayMed = 32;
	AIInfo[AI_MAMM].noFindWayRange = 60;
	AIInfo[AI_MAMM].targetBendRotSpd = 2.f;
	AIInfo[AI_MAMM].targetBendMin = 3.f;
	AIInfo[AI_MAMM].targetBendDelta1 = 2000.f;
	AIInfo[AI_MAMM].targetBendDelta2 = 2000.f;
	AIInfo[AI_MAMM].walkTargetGammaRot = 16.0f;
	AIInfo[AI_MAMM].targetGammaRot = 10.0f;
	AIInfo[AI_MAMM].idleStart = 124;
	AIInfo[AI_MAMM].yBetaGamma1 = 128;
	AIInfo[AI_MAMM].yBetaGamma2 = 64;
	AIInfo[AI_MAMM].yBetaGamma3 = 0.6f;
	AIInfo[AI_MAMM].yBetaGamma4 = 0.4f;
	AIInfo[AI_MAMM].tGAIncrement = 3.f;
	AIInfo[AI_MAMM].rot1 = 0.2f;
	AIInfo[AI_MAMM].rot2 = 1.0f;
	//AIInfo[AI_MAMM].weaveRange = 0;
	AIInfo[AI_MAMM].pWMin = 3048;

	AIInfo[AI_SMILO].iceAge = true;
	AIInfo[AI_SMILO].carnivore = true;
	AIInfo[AI_SMILO].targetDistance = 8048.f;
	AIInfo[AI_SMILO].agressMulti = 256;
	AIInfo[AI_SMILO].noWayCntMin = 8;
	AIInfo[AI_SMILO].noFindWayMed = 48;
	AIInfo[AI_SMILO].noFindWayRange = 80;
	AIInfo[AI_SMILO].targetBendRotSpd = 3.5f;
	AIInfo[AI_SMILO].targetBendMin = 3.f;
	AIInfo[AI_SMILO].targetBendDelta1 = 1600.f;
	AIInfo[AI_SMILO].targetBendDelta2 = 1200.f;
	AIInfo[AI_SMILO].walkTargetGammaRot = 12.0f;
	AIInfo[AI_SMILO].targetGammaRot = 8.0f;
	AIInfo[AI_SMILO].idleStartD = 120;
	AIInfo[AI_SMILO].yBetaGamma1 = 128;
	AIInfo[AI_SMILO].yBetaGamma2 = 64;
	AIInfo[AI_SMILO].yBetaGamma3 = 0.6f;
	AIInfo[AI_SMILO].yBetaGamma4 = 0.3f;
	AIInfo[AI_SMILO].tGAIncrement = 3.f;
	AIInfo[AI_SMILO].rot1 = 0.3f;
	AIInfo[AI_SMILO].rot2 = 1.5f;
	//AIInfo[AI_SMILO].weaveRange = 3072;
	AIInfo[AI_SMILO].pWMin = 2048;
	AIInfo[AI_SMILO].jumper = true;

	AIInfo[AI_WOLF].iceAge = true;
	AIInfo[AI_WOLF].carnivore = true;
	AIInfo[AI_WOLF].targetDistance = 8048.f;
	AIInfo[AI_WOLF].agressMulti = 256;
	AIInfo[AI_WOLF].noWayCntMin = 8;
	AIInfo[AI_WOLF].noFindWayMed = 48;
	AIInfo[AI_WOLF].noFindWayRange = 80;
	AIInfo[AI_WOLF].targetBendRotSpd = 3.5f;
	AIInfo[AI_WOLF].targetBendMin = 3.f;
	AIInfo[AI_WOLF].targetBendDelta1 = 1600.f;
	AIInfo[AI_WOLF].targetBendDelta2 = 1200.f;
	AIInfo[AI_WOLF].walkTargetGammaRot = 12.0f;
	AIInfo[AI_WOLF].targetGammaRot = 8.0f;
	AIInfo[AI_WOLF].idleStartD = 120;
	AIInfo[AI_WOLF].yBetaGamma1 = 128;
	AIInfo[AI_WOLF].yBetaGamma2 = 64;
	AIInfo[AI_WOLF].yBetaGamma3 = 0.6f;
	AIInfo[AI_WOLF].yBetaGamma4 = 0.3f;
	AIInfo[AI_WOLF].tGAIncrement = 3.f;
	AIInfo[AI_WOLF].rot1 = 0.4f;
	AIInfo[AI_WOLF].rot2 = 1.5f;
	//AIInfo[AI_WOLF].weaveRange = 3072;
	AIInfo[AI_WOLF].pWMin = 2048;
	AIInfo[AI_WOLF].jumper = true;

	AIInfo[AI_ANKY].targetDistance = 8048.f;
	AIInfo[AI_ANKY].agressMulti = 128;
	AIInfo[AI_ANKY].noWayCntMin = 12;
	AIInfo[AI_ANKY].noFindWayMed = 32;
	AIInfo[AI_ANKY].noFindWayRange = 60;
	AIInfo[AI_ANKY].targetBendRotSpd = 2.f;
	AIInfo[AI_ANKY].targetBendMin = 3.f;
	AIInfo[AI_ANKY].targetBendDelta1 = 2000.f;
	AIInfo[AI_ANKY].targetBendDelta2 = AIInfo[AI_ANKY].targetBendDelta1;
	AIInfo[AI_ANKY].walkTargetGammaRot = 16.0f;
	AIInfo[AI_ANKY].targetGammaRot = 10.0f;
	AIInfo[AI_ANKY].idleStart = 120;
	AIInfo[AI_ANKY].yBetaGamma1 = 128;
	AIInfo[AI_ANKY].yBetaGamma2 = 64;
	AIInfo[AI_ANKY].yBetaGamma3 = 0.6f;
	AIInfo[AI_ANKY].yBetaGamma4 = 0.4f;
	AIInfo[AI_ANKY].tGAIncrement = 3.f;
	AIInfo[AI_ANKY].rot1 = 0.2f;
	AIInfo[AI_ANKY].rot2 = 1.0f;
	//AIInfo[AI_ANKY].weaveRange = 0;
	AIInfo[AI_ANKY].pWMin = 2048;
	//waterLevel = 60;

	AIInfo[AI_PACH].targetDistance = 6048.f;
	AIInfo[AI_PACH].agressMulti = 128;
	AIInfo[AI_PACH].noWayCntMin = 12;
	AIInfo[AI_PACH].noFindWayMed = 32;
	AIInfo[AI_PACH].noFindWayRange = 60;
	AIInfo[AI_PACH].targetBendRotSpd = 3.0f;
	AIInfo[AI_PACH].targetBendMin = 2.f;
	AIInfo[AI_PACH].targetBendDelta1 = 1600.f;
	AIInfo[AI_PACH].targetBendDelta2 = 1200.f;
	AIInfo[AI_PACH].walkTargetGammaRot = 12.0f;
	AIInfo[AI_PACH].targetGammaRot = 8.0f;
	AIInfo[AI_PACH].idleStart = 120;
	AIInfo[AI_PACH].yBetaGamma1 = 128;
	AIInfo[AI_PACH].yBetaGamma2 = 64;
	AIInfo[AI_PACH].yBetaGamma3 = 0.6f;
	AIInfo[AI_PACH].yBetaGamma4 = 0.4f;
	AIInfo[AI_PACH].tGAIncrement = 3.f;
	AIInfo[AI_PACH].rot1 = 0.2f;
	AIInfo[AI_PACH].rot2 = 1.0f;
	//AIInfo[AI_PACH].weaveRange = 0;
	AIInfo[AI_PACH].pWMin = 2048;
	//waterLevel = 140;

	AIInfo[AI_STEGO].targetDistance = 8048.f;
	AIInfo[AI_STEGO].agressMulti = 128;
	AIInfo[AI_STEGO].noWayCntMin = 12;
	AIInfo[AI_STEGO].noFindWayMed = 32;
	AIInfo[AI_STEGO].noFindWayRange = 60;
	AIInfo[AI_STEGO].targetBendRotSpd = 2.f;
	AIInfo[AI_STEGO].targetBendMin = 3.f;
	AIInfo[AI_STEGO].targetBendDelta1 = 2000.f;
	AIInfo[AI_STEGO].targetBendDelta2 = AIInfo[AI_STEGO].targetBendDelta1;
	AIInfo[AI_STEGO].walkTargetGammaRot = 16.0f;
	AIInfo[AI_STEGO].targetGammaRot = 10.0f;
	AIInfo[AI_STEGO].idleStart = 120;
	AIInfo[AI_STEGO].yBetaGamma1 = 128;
	AIInfo[AI_STEGO].yBetaGamma2 = 64;
	AIInfo[AI_STEGO].yBetaGamma3 = 0.6f;
	AIInfo[AI_STEGO].yBetaGamma4 = 0.4f;
	AIInfo[AI_STEGO].tGAIncrement = 3.f;
	AIInfo[AI_STEGO].rot1 = 0.2f;
	AIInfo[AI_STEGO].rot2 = 1.0f;
	//AIInfo[AI_STEGO].weaveRange = 0;
	AIInfo[AI_STEGO].pWMin = 2048;
	//waterLevel = 160;

	AIInfo[AI_CHASM].targetDistance = 8048.f;
	AIInfo[AI_CHASM].agressMulti = 128;
	AIInfo[AI_CHASM].noWayCntMin = 8;
	AIInfo[AI_CHASM].noFindWayMed = 48;
	AIInfo[AI_CHASM].noFindWayRange = 80;
	AIInfo[AI_CHASM].idleStart = 124;
	AIInfo[AI_CHASM].targetBendRotSpd = 3.5f;
	AIInfo[AI_CHASM].targetBendMin = 2.f;
	AIInfo[AI_CHASM].targetBendDelta1 = 1600.f;
	AIInfo[AI_CHASM].targetBendDelta2 = 1200.f;
	AIInfo[AI_CHASM].yBetaGamma1 = 128;
	AIInfo[AI_CHASM].yBetaGamma2 = 64;
	AIInfo[AI_CHASM].yBetaGamma3 = 0.6f;
	AIInfo[AI_CHASM].yBetaGamma4 = 0.3f;
	AIInfo[AI_CHASM].walkTargetGammaRot = 12.0f;
	AIInfo[AI_CHASM].targetGammaRot = 8.0f;
	AIInfo[AI_CHASM].tGAIncrement = 3.f;
	AIInfo[AI_CHASM].rot1 = 0.2f;
	AIInfo[AI_CHASM].rot2 = 1.0f;
	//AIInfo[AI_CHASM].weaveRange = 0;
	AIInfo[AI_CHASM].pWMin = 2048;
	//waterLevel = 120;

	AIInfo[AI_ALLO].agressMulti = 4;
	AIInfo[AI_ALLO].targetBendRotSpd = 2;
	//AIInfo[AI_ALLO].waterLevel = 180;
	AIInfo[AI_ALLO].yBetaGamma1 = 64;
	AIInfo[AI_ALLO].yBetaGamma2 = 32;
	AIInfo[AI_ALLO].yBetaGamma3 = 0.5f;
	AIInfo[AI_ALLO].yBetaGamma4 = 0.4f;
	AIInfo[AI_ALLO].walkTargetGammaRot = 10.0f;
	AIInfo[AI_ALLO].targetGammaRot = 8.0f;
	AIInfo[AI_ALLO].tGAIncrement = 2.f;
	AIInfo[AI_ALLO].idleStartD = 118;
	AIInfo[AI_ALLO].jumper = true;
	AIInfo[AI_ALLO].carnivore = true;
	AIInfo[AI_ALLO].noWayCntMin = 12;
	AIInfo[AI_ALLO].noFindWayMed = 16;
	AIInfo[AI_ALLO].noFindWayRange = 20;
	AIInfo[AI_ALLO].targetDistance = 8048.f;
	//AIInfo[AI_ALLO].weaveRange = 1648;
	AIInfo[AI_ALLO].pWMin = 2048;

	AIInfo[AI_VELO].agressMulti = 8;
	AIInfo[AI_VELO].targetBendRotSpd = 3;
	//AIInfo[AI_VELO].waterLevel = 140;
	AIInfo[AI_VELO].yBetaGamma1 = 48;
	AIInfo[AI_VELO].yBetaGamma2 = 24;
	AIInfo[AI_VELO].yBetaGamma3 = 0.5f;
	AIInfo[AI_VELO].yBetaGamma4 = 0.4f;
	AIInfo[AI_VELO].walkTargetGammaRot = 7.0f;
	AIInfo[AI_VELO].targetGammaRot = 5.0f;
	AIInfo[AI_VELO].tGAIncrement = 2.f;
	AIInfo[AI_VELO].idleStartD = 118;
	AIInfo[AI_VELO].jumper = true;
	AIInfo[AI_VELO].carnivore = true;
	AIInfo[AI_VELO].noWayCntMin = 12;
	AIInfo[AI_VELO].noFindWayMed = 16;
	AIInfo[AI_VELO].noFindWayRange = 20;
	AIInfo[AI_VELO].targetDistance = 8048.f;
	//AIInfo[AI_VELO].weaveRange = 1648;
	AIInfo[AI_VELO].pWMin = 2048;

	AIInfo[AI_SPINO].agressMulti = 8;
	//AIInfo[AI_SPINO].waterLevel = 140;
	AIInfo[AI_SPINO].tGAIncrement = 4.f;
	AIInfo[AI_SPINO].idleStartD = 128;
	AIInfo[AI_SPINO].targetBendRotSpd = 3;
	AIInfo[AI_SPINO].yBetaGamma1 = 98;
	AIInfo[AI_SPINO].yBetaGamma2 = 84;
	AIInfo[AI_SPINO].yBetaGamma3 = 0.4f;
	AIInfo[AI_SPINO].yBetaGamma4 = 0.3f;
	AIInfo[AI_SPINO].walkTargetGammaRot = 9.0f;
	AIInfo[AI_SPINO].targetGammaRot = 6.0f;
	AIInfo[AI_SPINO].jumper = true;
	AIInfo[AI_SPINO].carnivore = true;
	AIInfo[AI_SPINO].noWayCntMin = 12;
	AIInfo[AI_SPINO].noFindWayMed = 16;
	AIInfo[AI_SPINO].noFindWayRange = 20;
	AIInfo[AI_SPINO].targetDistance = 8048.f;
	//AIInfo[AI_SPINO].weaveRange = 1648;
	AIInfo[AI_SPINO].pWMin = 2048;

	AIInfo[AI_CERAT].jumper = false;
	AIInfo[AI_CERAT].agressMulti = 8;
	//AIInfo[AI_CERAT].waterLevel = 140;
	AIInfo[AI_CERAT].tGAIncrement = 4.f;
	AIInfo[AI_CERAT].idleStartD = 128;
	AIInfo[AI_CERAT].targetBendRotSpd = 3;
	AIInfo[AI_CERAT].yBetaGamma1 = 348;
	AIInfo[AI_CERAT].yBetaGamma2 = 324;
	AIInfo[AI_CERAT].yBetaGamma3 = 0.5f;
	AIInfo[AI_CERAT].yBetaGamma4 = 0.4f;
	AIInfo[AI_CERAT].walkTargetGammaRot = 9.0f;
	AIInfo[AI_CERAT].targetGammaRot = 6.0f;
	AIInfo[AI_CERAT].carnivore = true;
	AIInfo[AI_CERAT].noWayCntMin = 12;
	AIInfo[AI_CERAT].noFindWayMed = 16;
	AIInfo[AI_CERAT].noFindWayRange = 20;
	AIInfo[AI_CERAT].targetDistance = 8048.f;
	//AIInfo[AI_CERAT].weaveRange = 1648;
	AIInfo[AI_CERAT].pWMin = 2048;

	AIInfo[AI_TITAN].jumper = false;
	AIInfo[AI_TITAN].agressMulti = 8;
	//AIInfo[AI_TITAN].waterLevel = 140;
	AIInfo[AI_TITAN].tGAIncrement = 4.f;
	AIInfo[AI_TITAN].idleStartD = 128;
	AIInfo[AI_TITAN].targetBendRotSpd = 3;
	AIInfo[AI_TITAN].yBetaGamma1 = 348;
	AIInfo[AI_TITAN].yBetaGamma2 = 324;
	AIInfo[AI_TITAN].yBetaGamma3 = 0.5f;
	AIInfo[AI_TITAN].yBetaGamma4 = 0.4f;
	AIInfo[AI_TITAN].walkTargetGammaRot = 9.0f;
	AIInfo[AI_TITAN].targetGammaRot = 6.0f;
	AIInfo[AI_TITAN].carnivore = true;
	AIInfo[AI_TITAN].noWayCntMin = 12;
	AIInfo[AI_TITAN].noFindWayMed = 16;
	AIInfo[AI_TITAN].noFindWayRange = 20;
	AIInfo[AI_TITAN].targetDistance = 8048.f;
	//AIInfo[AI_TITAN].weaveRange = 6592;
	AIInfo[AI_TITAN].pWMin = 2048;

	AIInfo[AI_MICRO].agressMulti = 8;
	AIInfo[AI_MICRO].targetBendRotSpd = 3;
	//AIInfo[AI_MICRO].waterLevel = 140;
	AIInfo[AI_MICRO].yBetaGamma1 = 48;
	AIInfo[AI_MICRO].yBetaGamma2 = 24;
	AIInfo[AI_MICRO].yBetaGamma3 = 0.5f;
	AIInfo[AI_MICRO].yBetaGamma4 = 0.4f;
	AIInfo[AI_MICRO].walkTargetGammaRot = 7.0f;
	AIInfo[AI_MICRO].targetGammaRot = 5.0f;
	AIInfo[AI_MICRO].tGAIncrement = 2.f;
	AIInfo[AI_MICRO].idleStartD = 118;
	//AIInfo[AI_MICRO].jumper = true;
	AIInfo[AI_MICRO].carnivore = true;
	AIInfo[AI_MICRO].noWayCntMin = 12;
	AIInfo[AI_MICRO].noFindWayMed = 16;
	AIInfo[AI_MICRO].noFindWayRange = 20;
	AIInfo[AI_MICRO].targetDistance = 8048.f;
	//AIInfo[AI_MICRO].weaveRange = 1648;
	AIInfo[AI_MICRO].pWMin = 2048;

	//AIInfo[AI_TREX].waterLevel = 560;

	AIInfo[AI_FISH].jumper = false;
	AIInfo[AI_FISH].agressMulti = 4;
	AIInfo[AI_FISH].idleStart = 126;

	AIInfo[AI_MOSA].jumper = true;
	AIInfo[AI_MOSA].agressMulti = 4;
	AIInfo[AI_MOSA].idleStart = 126; //SET THIS BACK TO 126

	AIInfo[AI_MOSH].idleStart = 76;
	AIInfo[AI_DIMET].idleStart = 76;
	AIInfo[AI_GALL].idleStart = 76;
	AIInfo[AI_PIG].idleStart = 96;

  FILE *stream;
  char line[256];


  stream = fopen("HUNTDAT\\_res.txt", "r");
  if (!stream) DoHalt("Can't open resources file _res.txt");

  TotalC = 0;
  TotalMA = 0;

  char tempProjectName[128];
  int timeOfDay, dinSelect;
  if (HasCommandLineOption("-survival"))
    g_GameMode = GameMode::SurvivalMode;

  ScriptCommonOptions commonOptions;
  rewind(stream);
  ReadCommonOptionsFromScript(stream, commonOptions);
  rewind(stream);
  ApplySurvivalCommonOptions(commonOptions);

  ReadScriptCommandLineOptions(tempProjectName, timeOfDay, dinSelect);

  int areaNumber = -1;
  switch ((char)tempProjectName[18]) {
  case '1':
	  if (tempProjectName[19]) areaNumber = 9;
	  else areaNumber = 0;
	  break;
  case '2':
	  areaNumber = 1;
	  break;
  case '3':
	  areaNumber = 2;
	  break;
  case '4':
	  areaNumber = 3;
	  break;
  case '5':
	  areaNumber = 4;
	  break;
  case '6':
	  areaNumber = 5;
	  break;
  case '7':
	  areaNumber = 6;
	  break;
  case '8':
	  areaNumber = 7;
	  break;
  case '9':
	  areaNumber = 8;
	  break;
  }

  trophyTypeCount = 0;

  while (fgets( line, 255, stream))
  {
    if (line[0] == '.') break;
	if (strstr(line, "spawntable")) ReadSpawnTable(stream);
	if (strstr(line, "packtable")) ReadPackTable(stream);
	if (strstr(line, "trophytable")) ReadTrophyTable(stream);
	if (strstr(line, "areatable")) ReadAreaTable(stream, areaNumber);
    if (strstr(line, "weapons") ) ReadWeapons(stream);
    if (strstr(line, "characters") ) ReadCharacters(stream);

  }

  //default region
  if (g_GameMode != GameMode::SurvivalMode)
  for (int sg = 0; sg < TotalSpawnGroup; sg++) {    
	  if (!spawnGroup[sg].spawnRegionCh) {
		  spawnGroup[sg].spawnRegion[0].XMax = 988;
		  spawnGroup[sg].spawnRegion[0].YMax = 988;
		  spawnGroup[sg].spawnRegion[0].XMin = 12;
		  spawnGroup[sg].spawnRegion[0].YMin = 12;
		  spawnGroup[sg].spawnRegionCh = 1;
	  }
  }

  fclose (stream);

  FlushScriptDiagnostics();
}
