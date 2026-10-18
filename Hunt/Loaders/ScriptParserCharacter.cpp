// ==========================================================================
// ScriptParserCharacter.cpp -- _RES.TXT character-section readers
// ==========================================================================
//
// Extracted mechanically from ScriptParser.cpp (see CarnivoresDoc/working/
// specs/c2-script-parser-split.md, Slice 5). Behavior, diagnostics, recovery
// policy, and parse order are unchanged; only file placement and linkage
// moved. The driver and the contract tests reach ReadCharacterLine and
// ReadCharacters through the declarations in ScriptParserInternal.h; the
// remaining readers are internal to this translation unit.

#include "Hunt.h"
#include "ScriptParserInternal.h"
#include "LoadDiagnostics.h"
#include "LoadValidate.h"
#include <cstring>

static int CurrentJumpPartIndex()
{
  const int index = DinoInfo[TotalC].jumpAnim;
  RequireScriptSlot(index, 50, "jump particle animation");
  return index;
}

static int CurrentIdlePartIndex()
{
  if (DinoInfo[TotalC].lookCount <= 0)
  {
    ReportScriptRecovery("idle particle animation",
                         "no preceding look animation", nullptr);
    if (LoadDiagnostics::Instance().Strict())
      DoHalt("Script loading error: idle particle data has no preceding look animation.");
    // Attach the particle data to animation slot 0 instead of aborting the
    // whole hunt; the diagnostic names the missing look animation.
    return 0;
  }
  const int index = DinoInfo[TotalC].lookAnim[DinoInfo[TotalC].lookCount - 1];
  RequireScriptSlot(index, 50, "idle particle animation");
  return index;
}

void WipeKillTypes() {
	WipeScriptSlots(DinoInfo[TotalC].killType, DinoInfo[TotalC].killTypeCount);
}

void WipeDeathTypes() {
	WipeScriptSlots(DinoInfo[TotalC].deathType, DinoInfo[TotalC].deathTypeCount);
}

void WipeIdleGroups() {
	WipeScriptSlots(DinoInfo[TotalC].idleGroup, DinoInfo[TotalC].idleGroupCount);
}

void WipeIdle2Groups() {
	WipeScriptSlots(DinoInfo[TotalC].idle2Group, DinoInfo[TotalC].idle2GroupCount);
}

// Shared body for the primary and secondary idle-group blocks: both fill a
// TDinoIdleType slot list with the same grammar; only the container and the
// diagnostics differ.
static void ReadIdleGroupBlock(FILE* stream, TDinoIdleType* groups, int& count,
                               const char* what, const char* errorLabel,
                               const char* startLabel, const char* endLabel,
                               const char* animWhat, const char* animLabel)
{
	RequireScriptSlot(count, 32, what);
	char* value;
	char line[256];

	while (fgets(line, 255, stream)) {
		if (strstr(line, "}")) {
			count++;
			break;
		}
		value = strstr(line, "=");
		if (!value) {
			char errorBuff[100];
			sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: %s: %s", errorLabel, DinoInfo[TotalC].Name);
			DoHalt(errorBuff);
		}
		value++;

		if (strstr(line, "startChance")) groups[count].start = ReadScriptFloatField(value, line, startLabel);
		if (strstr(line, "endChance")) groups[count].end = ReadScriptFloatField(value, line, endLabel);
		if (strstr(line, "randomStarting")) readBool(value, groups[count].startOnAny);
		if (strstr(line, "randomEnding")) readBool(value, groups[count].endOnAny);
		if (strstr(line, "instantRepeat")) readBool(value, groups[count].instantRepeat);

		if (strstr(line, "anim")) {
			RequireScriptSlot(groups[count].count, 32, animWhat);
			groups[count].anim[groups[count].count] = ReadScriptIntField(value, line, animLabel);
			groups[count].count++;
		}
	}
}

void ReadIdleGroupInfo(FILE *stream)
{
	ReadIdleGroupBlock(stream, DinoInfo[TotalC].idleGroup, DinoInfo[TotalC].idleGroupCount,
	                   "idle groups", "IdleGroup",
	                   "idle start chance", "idle end chance",
	                   "idle-group animations", "idle-group animation");
}

void ReadIdle2GroupInfo(FILE *stream)
{
	ReadIdleGroupBlock(stream, DinoInfo[TotalC].idle2Group, DinoInfo[TotalC].idle2GroupCount,
	                   "secondary idle groups", "IdleGroup2",
	                   "secondary idle start chance", "secondary idle end chance",
	                   "secondary idle-group animations", "secondary idle-group animation");
}

void ReadDeathTypeInfo(FILE *stream)
{
	RequireScriptSlot(DinoInfo[TotalC].deathTypeCount, 32, "death types");
	char *value;
	char line[256];

	while (fgets(line, 255, stream)) {
		if (strstr(line, "}")) {
			DinoInfo[TotalC].deathTypeCount++;
			break;
		}
		value = strstr(line, "=");
		if (!value) {
			char errorBuff[100];
			sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: DeathType: %s", DinoInfo[TotalC].Name);
			DoHalt(errorBuff);
		}
		value++;

		if (strstr(line, "dieAnim")) DinoInfo[TotalC].deathType[DinoInfo[TotalC].deathTypeCount].die = ReadScriptIntField(value, line, "death die animation");
		if (strstr(line, "sleepAnim")) DinoInfo[TotalC].deathType[DinoInfo[TotalC].deathTypeCount].sleep = ReadScriptIntField(value, line, "death sleep animation");
		if (strstr(line, "fallAnim")) DinoInfo[TotalC].deathType[DinoInfo[TotalC].deathTypeCount].fall = ReadScriptIntField(value, line, "death fall animation");
		if (strstr(line, "noSleep")) readBool(value, DinoInfo[TotalC].deathType[DinoInfo[TotalC].deathTypeCount].nosleep);

	}
}

void ReadKillTypeInfo(FILE *stream)
{
	RequireScriptSlot(DinoInfo[TotalC].killTypeCount, 32, "kill types");
	char *value;
	char line[256];

	while (fgets(line, 255, stream)) {
		if (strstr(line, "}")) {
			DinoInfo[TotalC].killTypeCount++;
			break;
		}
		value = strstr(line, "=");
		if (!value) {
			char errorBuff[100];
			sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: KillInfo: %s", DinoInfo[TotalC].Name);
			DoHalt(errorBuff);
		}
		value++;

		if (strstr(line, "hunterAnim")) DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].hunteranim = ReadScriptIntField(value, line, "kill hunter animation");
		if (strstr(line, "hunterCarryAnim")) DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].hunterswimanim = ReadScriptIntField(value, line, "kill hunter-carry animation");
		if (strstr(line, "hunterOffset")) DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].offset = ReadScriptIntField(value, line, "kill hunter offset");
		if (strstr(line, "eatAnim")) DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].anim = ReadScriptIntField(value, line, "kill eat animation");
		if (strstr(line, "hunterSync")) readBool(value, DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].elevate);
		if (strstr(line, "carryCorpse")) readBool(value, DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].carryCorpse);
		if (strstr(line, "dontloop")) readBool(value, DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].dontloop);
		if (strstr(line, "scream")) readBool(value, DinoInfo[TotalC].killType[DinoInfo[TotalC].killTypeCount].scream);

	}
}

void ReadCharacterLine(FILE *stream, char *_value, char line[256], bool &spawnInfoOverwrite, bool &spawnGroupOverwrite,
	bool &idleOverwrite, bool &idle2Overwrite, bool &roarOverwrite, bool &killOverwrite, bool &waterDieOverwrite,
	bool &deathTypeOverwrite, bool &trophyTypeOverwrite, bool &idleGroupOverwrite, bool &idle2GroupOverwrite,
	bool &memberOverwrite) {

	RequireScriptSlot(TotalC, DINOINFO_MAX, "characters");
	char *value = _value;

	// Text fields are handled before the numeric/flag dispatch and before the
	// block openers: their values are free-form, and a name or path can contain
	// a field key or a block name as a substring (a path like
	// models/massive/... or models/spawninfo/...). Once such a line is
	// recognized here it cannot select a numeric field or open a block.
	if (ScriptKeyIs(line, "name"))
	{
		CopyScriptField(DinoInfo[TotalC].Name, sizeof(DinoInfo[TotalC].Name), value, "Characters name", line);
		return;
	}

	if (ScriptKeyIs(line, "file"))
	{
		CopyScriptField(DinoInfo[TotalC].FName, sizeof(DinoInfo[TotalC].FName), value, "Characters file", line);
		return;
	}

	if (strstr(line, "packinfo")) {
		if (memberOverwrite) {
			WipePackMembers2();
			memberOverwrite = false;
		}
		ReadPackMember2(stream);
	}

	if (strstr(line, "packgroup")) {
		if (memberOverwrite) {//reuse same overwrite
			WipePackMembers2();
			memberOverwrite = false;
		}
		ReadPackGroup(stream,line,1);
	}

	if (strstr(line, "tropgroup")) {						
		int gr = ReadScriptIntField(value, line, "trophy group");
		for (int i = 0; i < trophyTypeCount; i++){
			if (trophyType[i].group == gr) {
				RequireScriptSlot(trophyType[i].ctypeCh, TROPHY2_COUNT,
				                  "trophy character list");
				trophyType[i].ctype[trophyType[i].ctypeCh] = TotalC;
				trophyType[i].ctypeCh++;
				DinoInfo[TotalC].trophy = true;
			}
		}
	}

	if (strstr(line, "callNo")) DinoInfo[TotalC].menuDino = ReadScriptIntField(value, line, "call number");

	if (strstr(line, "mass")) DinoInfo[TotalC].Mass = ReadScriptFloatField(value, line, "character mass");
	if (strstr(line, "length")) DinoInfo[TotalC].Length = ReadScriptFloatField(value, line, "character length");
	if (strstr(line, "radius")) DinoInfo[TotalC].Radius = ReadScriptFloatField(value, line, "character radius");
	if (strstr(line, "health")) DinoInfo[TotalC].Health0 = ReadScriptLegacyIntField(value, line, "character health");
	if (strstr(line, "basescore")) DinoInfo[TotalC].BaseScore = ReadScriptFloatField(value, line, "character base score");

	if (ScriptKeyIs(line, "ai")) DinoInfo[TotalC].Clone = ReadScriptIntField(value, line, "character AI");

	if (strstr(line, "smellK")) DinoInfo[TotalC].SmellK = ReadScriptFloatField(value, line, "character smell factor");
	if (strstr(line, "hearK")) DinoInfo[TotalC].HearK = ReadScriptFloatField(value, line, "character hearing factor");
	if (strstr(line, "lookK")) DinoInfo[TotalC].LookK = ReadScriptFloatField(value, line, "character sight factor");
	if (strstr(line, "shipdelta")) DinoInfo[TotalC].ShDelta = ReadScriptFloatField(value, line, "character ship delta");
	if (strstr(line, "scale0")) DinoInfo[TotalC].Scale0 = ReadScriptIntField(value, line, "character scale0");
	if (strstr(line, "scaleA")) DinoInfo[TotalC].ScaleA = ReadScriptIntField(value, line, "character scaleA");
	if (strstr(line, "fearCall")) DinoInfo[TotalC].fearCall[ScriptIndex(value, 64, "fear-call table")] = true;
	if (strstr(line, "dontFear")) DinoInfo[TotalC].fearCall[ScriptIndex(value, 64, "fear-call table")] = false;
	if (strstr(line, "maxdepth")) DinoInfo[TotalC].maxDepth = ReadScriptIntField(value, line, "character max depth");
	if (strstr(line, "maxalt")) DinoInfo[TotalC].maxDepth = ReadScriptIntField(value, line, "character max altitude");
	if (strstr(line, "mindepth")) DinoInfo[TotalC].minDepth = ReadScriptIntField(value, line, "character min depth");
	if (strstr(line, "minalt")) DinoInfo[TotalC].minDepth = ReadScriptIntField(value, line, "character min altitude");
	if (strstr(line, "spcdepth")) DinoInfo[TotalC].spacingDepth = ReadScriptIntField(value, line, "character depth spacing");
	if (strstr(line, "runspd")) DinoInfo[TotalC].runspd = ReadScriptFloatField(value, line, "character run speed");
	if (strstr(line, "jmpspd")) DinoInfo[TotalC].jmpspd = ReadScriptFloatField(value, line, "character jump speed");
	if (strstr(line, "wlkspd")) DinoInfo[TotalC].wlkspd = ReadScriptFloatField(value, line, "character walk speed");
	if (strstr(line, "swmspd")) DinoInfo[TotalC].swmspd = ReadScriptFloatField(value, line, "character swim speed");
	if (strstr(line, "flyspd")) DinoInfo[TotalC].flyspd = ReadScriptFloatField(value, line, "character fly speed");
	if (strstr(line, "gldspd")) DinoInfo[TotalC].gldspd = ReadScriptFloatField(value, line, "character glide speed");
	if (strstr(line, "tkfspd")) DinoInfo[TotalC].tkfspd = ReadScriptFloatField(value, line, "character takeoff speed");
	if (strstr(line, "lndspd")) DinoInfo[TotalC].lndspd = ReadScriptFloatField(value, line, "character land speed");
	if (strstr(line, "divspd")) DinoInfo[TotalC].divspd = ReadScriptFloatField(value, line, "character dive speed");
	if (strstr(line, "aggress")) DinoInfo[TotalC].aggress = ReadScriptIntField(value, line, "character aggression");
	// Per-species override of the AI-family aggression multiplier; absent or
	// non-positive keeps the clone table default (AIInfo[].agressMulti).
	if (strstr(line, "agressMulti"))
		DinoAggressMulti[TotalC] = ReadScriptIntField(value, line, "character aggression multiplier");
	if (strstr(line, "flydist")) DinoInfo[TotalC].flyDist = ReadScriptIntField(value, line, "character fly distance");
	if (strstr(line, "killdist")) DinoInfo[TotalC].killDist = ReadScriptLegacyIntField(value, line, "character kill distance");
	if (strstr(line, "radar")) readBool(value, DinoInfo[TotalC].onRadar);
	if (strstr(line, "dontswimaway")) readBool(value, DinoInfo[TotalC].dontSwimAway);

	if (strstr(line, "survivalIndex")) {
		DinoInfo[TotalC].survivalDino = true;
		RequireScriptSlot(SurvivalIndexCh, 128, "survival character list");
		SurvivalIndex[ScriptIndex(value, 128, "survival index")] = TotalC;
		SurvivalIndexCh++;
	}

	if (strstr(line, "dontBend")) readBool(value, DinoInfo[TotalC].dontBend);

	if (strstr(line, "defensive")) readBool(value, DinoInfo[TotalC].defensive);
	if (strstr(line, "fearShotHear")) readBool(value, DinoInfo[TotalC].fearHearShot);
	if (strstr(line, "fearShotHit")) readBool(value, DinoInfo[TotalC].fearShot);

	if (strstr(line, "weaveRange")) DinoInfo[TotalC].weaveRange = ReadScriptFloatField(value, line, "character weave range");
	if (strstr(line, "dontWeave")) readBool(value, DinoInfo[TotalC].dontWeave);

	if (strstr(line, "radR")) DinoInfo[TotalC].radarRed = ReadScriptIntField(value, line, "character radar red");
	if (strstr(line, "radG")) DinoInfo[TotalC].radarGreen = ReadScriptIntField(value, line, "character radar green");
	if (strstr(line, "radB")) DinoInfo[TotalC].radarBlue = ReadScriptIntField(value, line, "character radar blue");
	
	if (strstr(line, "bloR")) DinoInfo[TotalC].bloodRed = ReadScriptIntField(value, line, "character blood red");
	if (strstr(line, "bloG")) DinoInfo[TotalC].bloodGreen = ReadScriptIntField(value, line, "character blood green");
	if (strstr(line, "bloB")) DinoInfo[TotalC].bloodBlue = ReadScriptIntField(value, line, "character blood blue");

	if (strstr(line, "collisiondist")) DinoInfo[TotalC].maxGrad = ReadScriptIntField(value, line, "character collision distance");
	if (strstr(line, "runrotatespeed")) DinoInfo[TotalC].rotspdmulti = ReadScriptFloatField(value, line, "character rotation speed");

	if (strstr(line, "waterLevel")) DinoInfo[TotalC].waterLevel = ReadScriptIntField(value, line, "character water level");

	if (strstr(line, "CamYLand")) DinoInfo[TotalC].camDemoPoint = ReadScriptFloatField(value, line, "character land camera height");
	if (strstr(line, "CamYWater")) DinoInfo[TotalC].camDemoPointWater = ReadScriptFloatField(value, line, "character water camera height");
	if (strstr(line, "CamBaseLand")) DinoInfo[TotalC].camBase = ReadScriptFloatField(value, line, "character land camera base");
	if (strstr(line, "CamBaseWater")) DinoInfo[TotalC].camBaseWater = ReadScriptFloatField(value, line, "character water camera base");

	if (strstr(line, "dogSmell")) readBool(value, DinoInfo[TotalC].dogSmell);

	if (strstr(line, "climbDist")) DinoInfo[TotalC].climbDist = ReadScriptFloatField(value, line, "character climb distance");

	if (strstr(line, "canswim")) readBool(value, DinoInfo[TotalC].canSwim); //check animate subroutines for what this includes. LandBrach needs this attribute, but maybe rename to wade? (and default to off for landbrach ai? maybe?)

	if (strstr(line, "jumpPartFrame1")) DinoInfo[TotalC].partFrame1[CurrentJumpPartIndex()] = 1000 * ReadScriptIntField(value, line, "jump particle frame 1"); // x1000
	if (strstr(line, "jumpPartFrame2")) DinoInfo[TotalC].partFrame2[CurrentJumpPartIndex()] = 1000 * ReadScriptIntField(value, line, "jump particle frame 2"); // x1000
	if (strstr(line, "jumpPartDist")) DinoInfo[TotalC].partDist[CurrentJumpPartIndex()] = ReadScriptIntField(value, line, "jump particle distance");
	if (strstr(line, "jumpPartCnt")) DinoInfo[TotalC].partCnt[CurrentJumpPartIndex()] = ReadScriptIntField(value, line, "jump particle count");
	if (strstr(line, "jumpPartMag")) DinoInfo[TotalC].partMag[CurrentJumpPartIndex()] = ReadScriptIntField(value, line, "jump particle magnitude");
	if (strstr(line, "jumpPartOffset")) DinoInfo[TotalC].partOffset[CurrentJumpPartIndex()] = ReadScriptIntField(value, line, "jump particle offset");
	if (strstr(line, "jumpPartAngled")) readBool(value, DinoInfo[TotalC].partAngled[CurrentJumpPartIndex()]);
	if (strstr(line, "jumpPartCircle")) readBool(value, DinoInfo[TotalC].partCircle[CurrentJumpPartIndex()]);

	if (strstr(line, "idlePartFrame1")) DinoInfo[TotalC].partFrame1[CurrentIdlePartIndex()] = 1000 * ReadScriptIntField(value, line, "idle particle frame 1"); // x1000
	if (strstr(line, "idlePartFrame2")) DinoInfo[TotalC].partFrame2[CurrentIdlePartIndex()] = 1000 * ReadScriptIntField(value, line, "idle particle frame 2"); // x1000
	if (strstr(line, "idlePartDist")) DinoInfo[TotalC].partDist[CurrentIdlePartIndex()] = ReadScriptIntField(value, line, "idle particle distance");
	if (strstr(line, "idlePartCnt")) DinoInfo[TotalC].partCnt[CurrentIdlePartIndex()] = ReadScriptIntField(value, line, "idle particle count");
	if (strstr(line, "idlePartMag")) DinoInfo[TotalC].partMag[CurrentIdlePartIndex()] = ReadScriptIntField(value, line, "idle particle magnitude");
	if (strstr(line, "idlePartOffset")) DinoInfo[TotalC].partOffset[CurrentIdlePartIndex()] = ReadScriptIntField(value, line, "idle particle offset");
	if (strstr(line, "idlePartAngled")) readBool(value, DinoInfo[TotalC].partAngled[CurrentIdlePartIndex()]);
	if (strstr(line, "idlePartCircle")) readBool(value, DinoInfo[TotalC].partCircle[CurrentIdlePartIndex()]);

	if (strstr(line, "DangerFish")) readBool(value, DinoInfo[TotalC].DangerFish);

	if (strstr(line, "TRexObjCollide")) readBool(value, DinoInfo[TotalC].TRexObjCollide);

	if (strstr(line, "Mystery")) readBool(value, DinoInfo[TotalC].Mystery);
	if (strstr(line, "HideBinoc")) readBool(value, DinoInfo[TotalC].HideBinoc);

	if (strstr(line, "JumpRange")) DinoInfo[TotalC].jumpRange = ReadScriptIntField(value, line, "character jump range");

	if (strstr(line, "Weapon")) {
		DinoInfo[TotalC].Weapon = ScriptIndex(value, TotalW, "character weapon");
		DinoInfo[TotalC].Reload = WeapInfo[DinoInfo[TotalC].Weapon].Shots;
		if (WeapInfo[DinoInfo[TotalC].Weapon].Reload)
			DinoInfo[TotalC].Reload = WeapInfo[DinoInfo[TotalC].Weapon].Reload;
	}

	if (strstr(line, "runAnim")) DinoInfo[TotalC].runAnim = ReadScriptIntField(value, line, "run animation");
	if (strstr(line, "jumpAnim")) DinoInfo[TotalC].jumpAnim = ReadScriptIntField(value, line, "jump animation");
	if (strstr(line, "walkAnim")) DinoInfo[TotalC].walkAnim = ReadScriptIntField(value, line, "walk animation");
	if (strstr(line, "swimAnim")) DinoInfo[TotalC].swimAnim = ReadScriptIntField(value, line, "swim animation");
	if (strstr(line, "flyAnim")) DinoInfo[TotalC].flyAnim = ReadScriptIntField(value, line, "fly animation");
	if (strstr(line, "diveAnim")) DinoInfo[TotalC].diveAnim = ReadScriptIntField(value, line, "dive animation");
	if (strstr(line, "glideAnim")) DinoInfo[TotalC].glideAnim = ReadScriptIntField(value, line, "glide animation");
	if (strstr(line, "takeoffAnim")) DinoInfo[TotalC].takeoffAnim = ReadScriptIntField(value, line, "takeoff animation");
	if (strstr(line, "landAnim")) DinoInfo[TotalC].landAnim = ReadScriptIntField(value, line, "land animation");
	if (strstr(line, "slideAnim")) DinoInfo[TotalC].slideAnim = ReadScriptIntField(value, line, "slide animation");
	if (strstr(line, "shakeLAnim")) DinoInfo[TotalC].shakeLandAnim = ReadScriptIntField(value, line, "shake-land animation");
	if (strstr(line, "shakeWAnim")) DinoInfo[TotalC].shakeWaterAnim = ReadScriptIntField(value, line, "shake-water animation");
	if (strstr(line, "climbAnim")) DinoInfo[TotalC].climbAnim = ReadScriptIntField(value, line, "climb animation");
	if (strstr(line, "fireAnim")) DinoInfo[TotalC].fireAnim = ReadScriptIntField(value, line, "fire animation");
	if (strstr(line, "reloadAnim")) DinoInfo[TotalC].reloadAnim = ReadScriptIntField(value, line, "reload animation");

	if (strstr(line, "lookAnim") || strstr(line, "fishIdleAnim")) {
		if (idleOverwrite) {
			DinoInfo[TotalC].lookCount = 0;
			idleOverwrite = false;
		}
		RequireScriptSlot(DinoInfo[TotalC].lookCount, 32, "look animations");
		DinoInfo[TotalC].lookAnim[DinoInfo[TotalC].lookCount] = ReadScriptIntField(value, line, "look animation");
		DinoInfo[TotalC].lookCount++;
	}

	if (strstr(line, "smellAnim")) {
		if (idle2Overwrite) {
			DinoInfo[TotalC].smellCount = 0;
			idle2Overwrite = false;
		}
		RequireScriptSlot(DinoInfo[TotalC].smellCount, 32, "smell animations");
		DinoInfo[TotalC].smellAnim[DinoInfo[TotalC].smellCount] = ReadScriptIntField(value, line, "smell animation");
		DinoInfo[TotalC].smellCount++;
	}

	if (strstr(line, "roarAnim")) {
		if (roarOverwrite) {
			DinoInfo[TotalC].roarCount = 0;
			roarOverwrite = false;
		}
		RequireScriptSlot(DinoInfo[TotalC].roarCount, 32, "roar animations");
		DinoInfo[TotalC].roarAnim[DinoInfo[TotalC].roarCount] = ReadScriptIntField(value, line, "roar animation");
		DinoInfo[TotalC].roarCount++;
	}

	if (strstr(line, "waterDAnim")) {
		if (waterDieOverwrite) {
			DinoInfo[TotalC].waterDieCount = 0;
			waterDieOverwrite = false;
		}
		RequireScriptSlot(DinoInfo[TotalC].waterDieCount, 32, "water-death animations");
		DinoInfo[TotalC].waterDieAnim[DinoInfo[TotalC].waterDieCount] = ReadScriptIntField(value, line, "water-death animation");
		DinoInfo[TotalC].waterDieCount++;
	}

	/*
	if (strstr(line, "trophy")) {
		if (!DinoInfo[TotalC].trophyCode) {
			bool temp = false;
			readBool(value, temp);
			if (temp) {
				TotalTrophy++;
				DinoInfo[TotalC].trophyCode = TotalTrophy;
				TrophyIndex[TotalTrophy] = TotalC;
			}
		}
		readBool(value, DinoInfo[TotalC].trophySession);
	}
	*/

	if (strstr(line, "tropinfo")) {
		ReadTrophyTypeInfo(stream, -1);
	}

	if (strstr(line, "killtype")) {

		if (killOverwrite) {
			WipeKillTypes();
			killOverwrite = false;
		}

		ReadKillTypeInfo(stream);
	}

	if (strstr(line, "deathtype")) {

		if (deathTypeOverwrite) {
			WipeDeathTypes();
			deathTypeOverwrite = false;
		}

		ReadDeathTypeInfo(stream);
	}

	if (strstr(line, "idlegroup")) {

		if (idleGroupOverwrite) {
			WipeIdleGroups();
			idleGroupOverwrite = false;
		}

		ReadIdleGroupInfo(stream);
	}

	if (strstr(line, "waterIgroup")) {

		if (idle2GroupOverwrite) {
			WipeIdle2Groups();
			idle2GroupOverwrite = false;
		}

		ReadIdle2GroupInfo(stream);
	}

	if (strstr(line, "spawninfo")) {
		if (g_GameMode == GameMode::SurvivalMode) {
			SkipSector(stream);
		} else {
			if (spawnInfoOverwrite) {
				WipeSpawnInfo();
				spawnInfoOverwrite = false;
			}
			ReadSpawnInfo(stream);
		}
	}

	if (strstr(line, "spawngroup")) {
		if (g_GameMode == GameMode::SurvivalMode) {
			SkipSector(stream);
		} else {
			if (spawnGroupOverwrite) {
				WipeSpawnInfo();
				spawnGroupOverwrite = false;
			}
			ReadSpawnGroup(stream, line, 1);
		}
	}

}

void ReadCharacters(FILE *stream)
{

	//area
	char tempProjectName[128];
	int timeOfDay, dinSelect;
	ReadScriptCommandLineOptions(tempProjectName, timeOfDay, dinSelect);

  char line[256], *value;
  while (fgets( line, 255, stream))
  {
    if (strstr(line, "}")) break;
    if (strstr(line, "{"))
      while (fgets( line, 255, stream))
      {
        // Validate before any field or end-of-section calculation indexes
        // DinoInfo; an empty 129th section must be rejected too.
        RequireScriptSlot(TotalC, DINOINFO_MAX, "characters");

        if (strstr(line, "}"))
        {

			if (g_GameMode == GameMode::SurvivalMode && DinoInfo[TotalC].survivalDino)
			{
				DinoInfo[TotalC].SpawnInfoCh = 1;
				DinoInfo[TotalC].SpawnInfo[0].spawnGroup = 0;
			}

		  if (DinoInfo[TotalC].Clone == AI_MOSA ||
			  DinoInfo[TotalC].Clone == AI_FISH) {
			  DinoInfo[TotalC].Aquatic = true;
		  } else {
			  DinoInfo[TotalC].Aquatic = false;
		  }
	
		  DinoInfo[TotalC].radarColour565 = ((DinoInfo[TotalC].radarRed>>3) << 11) | ((DinoInfo[TotalC].radarGreen>>2) << 5) | (DinoInfo[TotalC].radarBlue>>3);
		  DinoInfo[TotalC].radarColour555 = ((DinoInfo[TotalC].radarRed >> 3) << 10) | ((DinoInfo[TotalC].radarGreen >> 3) << 5) | (DinoInfo[TotalC].radarBlue >> 3);

		  // DinoInfo has DINOINFO_MAX (128) entries.
		  if (TotalC >= DINOINFO_MAX)
		    DoHalt("Script loading error: too many characters.");
		  TotalC++;

          break;

        }

			value = strstr(line, "=");
			if (!value &&
				!strstr(line, "overwrite") &&
				!strstr(line, "addition") &&
				!strstr(line, "spawninfo") &&
				!strstr(line, "spawngroup") &&
				!strstr(line, "killtype") &&
				!strstr(line, "tropinfo") &&
				!strstr(line, "deathtype") &&
				!strstr(line, "idlegroup") &&
				!strstr(line, "packinfo") &&
				!strstr(line, "packgroup") &&
				!strstr(line, "waterIgroup"))
				DoHalt("Script loading error: Characters");
			value = value ? value + 1 : line;

			bool temp, temp3, temp4, temp5, temp6, temp7, temp8, temp9, temp10, temp11, temp12, temp13;
			temp = false;
			temp3 = false;
			temp4 = false;
			temp5 = false;
			temp6 = false;
			temp7 = false;
			temp8 = false;
			temp9 = false;
			temp10 = false;
			temp11 = false;
			temp12 = false;
			temp13 = false;
			ReadCharacterLine(stream, value, line, temp, temp3, temp4, temp5, temp6, temp7, temp8, temp9, temp12, temp10, temp11, temp13);

			if (strstr(line, "overwrite") || strstr(line, "addition")) {

				if (OverwriteBlockApplies(line, tempProjectName, timeOfDay, dinSelect)) {

					bool spawnInfoOverwrite = strstr(line, "overwrite");
					bool spawnGroupOverwrite = strstr(line, "overwrite");
					bool idleOverwrite = strstr(line, "overwrite");
					bool killOverwrite = strstr(line, "overwrite");
					bool idle2Overwrite = strstr(line, "overwrite");
					bool roarOverwrite = strstr(line, "overwrite");
					bool waterDieOverwrite = strstr(line, "overwrite");
					bool deathTypeOverwrite = strstr(line, "overwrite");
					bool trophyTypeOverwrite = strstr(line, "overwrite");
					bool idleGroupOverwrite = strstr(line, "overwrite");
					bool idle2GroupOverwrite = strstr(line, "overwrite");
					bool memberOverwrite = strstr(line, "overwrite");

					while (fgets(line, 255, stream)) {
						if (strstr(line, "}")) break;

						value = strstr(line, "=");
						if (!value
							&& !strstr(line, "spawninfo")
							&& !strstr(line, "spawngroup")
							&& !strstr(line, "deathtype")
							&& !strstr(line, "idlegroup")
							&& !strstr(line, "waterIgroup")
							&& !strstr(line, "tropinfo")
							&& !strstr(line, "packinfo")
							&& !strstr(line, "packgroup")
							&& !strstr(line, "killtype"))
							DoHalt("Script loading error: Characters");
						value = value ? value + 1 : line;

						ReadCharacterLine(stream, value, line, spawnInfoOverwrite, spawnGroupOverwrite,
							idleOverwrite, idle2Overwrite, roarOverwrite, killOverwrite,
							waterDieOverwrite, deathTypeOverwrite, trophyTypeOverwrite,
							idleGroupOverwrite, idle2GroupOverwrite,memberOverwrite);

					}

				} else {
					SkipSector(stream);
				}
			}

      }

  }
}
