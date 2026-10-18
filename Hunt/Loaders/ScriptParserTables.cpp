// ==========================================================================
// ScriptParserTables.cpp -- _RES.TXT trophy, snow, area, and weapon readers
// ==========================================================================
//
// Extracted mechanically from ScriptParser.cpp (see CarnivoresDoc/working/
// specs/c2-script-parser-split.md, Slice 3). Behavior, diagnostics, recovery
// policy, and parse order are unchanged; only file placement and linkage
// moved. The driver, the character group, and the contract tests reach these
// readers through the declarations in ScriptParserInternal.h.

#include "Hunt.h"
#include "ScriptParserInternal.h"
#include "LoadValidate.h"

void ReadTrophyTypeInfo(FILE *stream, int trophyGroup)
{
	RequireScriptSlot(trophyTypeCount, TROPHY2_COUNT, "trophy types");
	char *value;
	char line[256];

	while (fgets(line, 255, stream)) {
		if (strstr(line, "}")) {

			if (trophyGroup >= 0) {
				trophyType[trophyTypeCount].group = trophyGroup;
			}
			else {
				RequireScriptSlot(trophyType[trophyTypeCount].ctypeCh, TROPHY2_COUNT,
				                  "trophy character list");
				trophyType[trophyTypeCount].ctype[trophyType[trophyTypeCount].ctypeCh] = TotalC;
				trophyType[trophyTypeCount].ctypeCh++;
				DinoInfo[TotalC].trophy = true;
			}
			trophyTypeCount++;
			break;

		}
		value = strstr(line, "=");
		if (!value) {
			char errorBuff[100];
			sprintf(errorBuff, "Script loading error: TrophyType: %i", trophyTypeCount);
			DoHalt(errorBuff);
		}
		value++;

		if (strstr(line, "tropPos")) trophyType[trophyTypeCount].trophyPos = ReadScriptIntField(value, line, "trophy tropPos");
		if (strstr(line, "alpha")) trophyType[trophyTypeCount].alpha = ReadScriptIntField(value, line, "trophy alpha");
		if (strstr(line, "beta")) trophyType[trophyTypeCount].beta = ReadScriptIntField(value, line, "trophy beta");
		if (strstr(line, "gamma")) trophyType[trophyTypeCount].gamma = ReadScriptIntField(value, line, "trophy gamma");
		if (strstr(line, "xoffset")) trophyType[trophyTypeCount].xoffset = ReadScriptIntField(value, line, "trophy xoffset");
		if (strstr(line, "yoffset")) trophyType[trophyTypeCount].yoffset = ReadScriptIntField(value, line, "trophy yoffset");
		if (strstr(line, "zoffset")) trophyType[trophyTypeCount].zoffset = ReadScriptIntField(value, line, "trophy zoffset");
		if (strstr(line, "xscale")) trophyType[trophyTypeCount].xoffsetScale = ReadScriptIntField(value, line, "trophy xscale");
		if (strstr(line, "yscale")) trophyType[trophyTypeCount].yoffsetScale = ReadScriptIntField(value, line, "trophy yscale");
		if (strstr(line, "zscale")) trophyType[trophyTypeCount].zoffsetScale = ReadScriptIntField(value, line, "trophy zscale");
		if (strstr(line, "tropAnim")) trophyType[trophyTypeCount].anim = ReadScriptIntField(value, line, "trophy tropAnim");
		if (strstr(line, "xdata")) trophyType[trophyTypeCount].xdata = ReadScriptIntField(value, line, "trophy xdata");
		if (strstr(line, "ydata")) trophyType[trophyTypeCount].ydata = ReadScriptIntField(value, line, "trophy ydata");
		if (strstr(line, "zdata")) trophyType[trophyTypeCount].zdata = ReadScriptIntField(value, line, "trophy zdata");
		if (strstr(line, "playAnim")) readBool(value, trophyType[trophyTypeCount].playAnim);

	}
}

void ReadTrophyTable(FILE *stream)
{
	trophyGroupCount = 0;

	char line[256], *value;
	while (fgets(line, 255, stream))
	{
		if (strstr(line, "}")) break;

		if (strstr(line, "trophygroup"))
			while (fgets(line, 255, stream))
			{
				if (strstr(line, "}"))
				{
					trophyGroupCount++;
					break;
				}

				if (strstr(line, "tropinfo")) {
					ReadTrophyTypeInfo(stream, trophyGroupCount);
				}

			}

	}
}

void ReadSnowType(FILE *stream)
{
	RequireScriptSlot(SnowCh, 32, "snow types");
	char *value;
	char line[256];

	while (fgets(line, 255, stream)) {
		if (strstr(line, "}")) {
			SnowCh++;
			break;
		}
		value = strstr(line, "=");
		if (!value) {
			char errorBuff[100];
			sprintf(errorBuff, "Script loading error: snowtype: %i", SnowCh);
			DoHalt(errorBuff);
		}
		value++;

		if (strstr(line, "vSpd"))  SnowInfo[SnowCh].snow_vSpd = ReadScriptIntField(value, line, "snow vertical speed");
		if (strstr(line, "hSpd"))  SnowInfo[SnowCh].snow_hSpd = ReadScriptIntField(value, line, "snow horizontal speed");
		if (strstr(line, "dens")) {
			SnowInfo[SnowCh].snow_dens = RecoverScriptRange(
			    ReadScriptIntField(value, line, "snow density"), 0, (1 << 20),
			    "snow density", line);
		}

		if (strstr(line, "red"))  SnowInfo[SnowCh].snow_r = ReadScriptIntField(value, line, "snow red");
		if (strstr(line, "gre"))  SnowInfo[SnowCh].snow_g = ReadScriptIntField(value, line, "snow green");
		if (strstr(line, "blu"))  SnowInfo[SnowCh].snow_b = ReadScriptIntField(value, line, "snow blue");
		if (strstr(line, "alp"))  SnowInfo[SnowCh].snow_a = ReadScriptIntField(value, line, "snow alpha");
		if (strstr(line, "rad"))  SnowInfo[SnowCh].snow_rad = ReadScriptFloatField(value, line, "snow radius");

	}
}

void ReadAreaTable (FILE *stream, int areaNumber)
{
	SnowCh = 0;

	TotalAreaInfo = 0;
	char line[256], *value;
	while (fgets(line, 255, stream))
	{
		if (strstr(line, "}")) break;
		if (strstr(line, "{"))
			while (fgets(line, 255, stream))
			{
				if (strstr(line, "}"))
				{
					if (SnowCh){
						int totalSnowTemp=0;
						for (int st = 0; st < SnowCh; st++) {
							if (SnowInfo[st].snow_dens < 0 ||
							    SnowInfo[st].snow_dens > (1 << 20) - totalSnowTemp)
								DoHalt("Script loading error: total snow density out of range.");
							SnowInfo[st].addr = totalSnowTemp;
							totalSnowTemp += SnowInfo[st].snow_dens;
						}
						// Allocate through the memory grid so MEM_DEBUG can track it.
						// Tag as Global because LoadResourcesScript runs once in
						// InitEngine and Snow is never re-allocated on level loads.
						// Freed in ReleaseGlobalResources.
						// Free-before-realloc: _RES.TXT holds a snow sector per
						// area table, so a second sector orphaned the first
						// block (12,600-byte stock leak on snow maps).
						if (Snow) {
						    (void)_HeapFree(Heap, 0, Snow);
						    Snow = nullptr;
						}
						Snow = totalSnowTemp > 0
						    ? (TSnowElement*)_HeapAlloc(Heap, 0, totalSnowTemp * sizeof(TSnowElement), MemoryTag::Global)
						    : nullptr;
					}
					TotalAreaInfo++;
					break;
				}
				value = strstr(line, "=");
				if (!value && !strstr(line, "snow")) DoHalt("Script loading error: AreaTable");
				value = value ? value + 1 : line;

				char testBuff[100];
				sprintf(testBuff, "\n TEST: %i", TotalAreaInfo);
				PrintLogVerbose(testBuff);
				sprintf(testBuff, "\n TES2: %i", areaNumber);
				PrintLogVerbose(testBuff);

				if (TotalAreaInfo == areaNumber) {

					if (strstr(line, "snow")) {
						ReadSnowType(stream);
					}

					if (strstr(line, "tree"))
						TreeTable[ScriptIndex(value, 255, "tree table")] = true;

					if (strstr(line, "survivalPlayerX")) SurvivalSpawnX = ReadScriptIntField(value, line, "survival player X");
					if (strstr(line, "survivalPlayerY")) SurvivalSpawnZ = ReadScriptIntField(value, line, "survival player Y");
					if (strstr(line, "survivalPlayerA")) SurvivalSpawnA = ReadScriptFloatField(value, line, "survival player angle");
					if (strstr(line, "survivalDinoXMax")) SurvivalDinoSpawn.XMax = ReadScriptIntField(value, line, "survival dino X max");
					if (strstr(line, "survivalDinoYMax")) SurvivalDinoSpawn.YMax = ReadScriptIntField(value, line, "survival dino Y max");
					if (strstr(line, "survivalDinoXMin")) SurvivalDinoSpawn.XMin = ReadScriptIntField(value, line, "survival dino X min");
					if (strstr(line, "survivalDinoYMin")) SurvivalDinoSpawn.YMin = ReadScriptIntField(value, line, "survival dino Y min");

				} else {
					if (strstr(line, "snow")) SkipSector(stream);
				}

			}

	}

}

void ReadWeaponLine(FILE *stream, char *_value, char line[256]) {
	RequireScriptSlot(TotalW, 10, "weapons");
	char *value = _value;

	// Text fields are handled before the numeric/flag dispatch: their values
	// are free-form, and a name or path can contain a numeric field key as a
	// substring (a lowercase 'crossbow', a path like models/separated/...).
	// Once such a line is recognized here it cannot reach a numeric reader.
	if (ScriptKeyIs(line, "name"))
	{
		CopyScriptField(WeapInfo[TotalW].Name, sizeof(WeapInfo[TotalW].Name), value, "Weapons name", line);
		return;
	}

	if (ScriptKeyIs(line, "file"))
	{
		CopyScriptField(WeapInfo[TotalW].FName, sizeof(WeapInfo[TotalW].FName), value, "Weapons file", line);
		return;
	}

	if (ScriptKeyIs(line, "gunshot"))
	{
		CopyScriptField(WeapInfo[TotalW].SFXName, sizeof(WeapInfo[TotalW].SFXName), value, "Weapons gunshot", line);
		WeapInfo[TotalW].MGSSound = true;
		return;
	}

	if (ScriptKeyIs(line, "pic1"))
	{
		CopyScriptField(WeapInfo[TotalW].BFName, sizeof(WeapInfo[TotalW].BFName), value, "Weapons pic", line);
		return;
	}

	if (ScriptKeyIs(line, "picc"))
	{
		CopyScriptField(WeapInfo[TotalW].CFName, sizeof(WeapInfo[TotalW].CFName), value, "Chamber pic", line);
		WeapInfo[TotalW].picch = true;
		return;
	}

	if (ScriptKeyIs(line, "bModel"))
	{
		CopyScriptField(WeapInfo[TotalW].BLName, sizeof(WeapInfo[TotalW].BLName), value, "Weapons bullet", line);
		WeapInfo[TotalW].bullet = true;
		return;
	}

	if (strstr(line, "getAnim"))  WeapInfo[TotalW].getAnim = ReadScriptIntField(value, line, "weapon getAnim");
	if (strstr(line, "putAnim"))  WeapInfo[TotalW].putAnim = ReadScriptIntField(value, line, "weapon putAnim");
	if (strstr(line, "shtAnim"))  WeapInfo[TotalW].shtAnim = ReadScriptIntField(value, line, "weapon shtAnim");
	if (strstr(line, "rldAnimFull"))  WeapInfo[TotalW].rldAnim = ReadScriptIntField(value, line, "weapon rldAnimFull");
	if (strstr(line, "rldAnimPart"))  WeapInfo[TotalW].rldAnimPart = ReadScriptIntField(value, line, "weapon rldAnimPart");
	if (strstr(line, "rckAnim"))  WeapInfo[TotalW].pmpAnim = ReadScriptIntField(value, line, "weapon rckAnim");
	if (strstr(line, "modAnim"))  WeapInfo[TotalW].modAnim = ReadScriptIntField(value, line, "weapon modAnim");

	if (strstr(line, "emptyAnim")) WeapInfo[TotalW].emptyAnim = ReadScriptIntField(value, line, "weapon emptyAnim");
	if (strstr(line, "getEmpAnim")) WeapInfo[TotalW].getEmpAnim = ReadScriptIntField(value, line, "weapon getEmpAnim");
	if (strstr(line, "putEmpAnim")) WeapInfo[TotalW].putEmpAnim = ReadScriptIntField(value, line, "weapon putEmpAnim");

	if (strstr(line, "getAqSnd"))  WeapInfo[TotalW].getAqSnd = ReadScriptIntField(value, line, "weapon getAqSnd");
	if (strstr(line, "putAqSnd"))  WeapInfo[TotalW].putAqSnd = ReadScriptIntField(value, line, "weapon putAqSnd");
	if (strstr(line, "shtAqSnd"))  WeapInfo[TotalW].shtAqSnd = ReadScriptIntField(value, line, "weapon shtAqSnd");
	if (strstr(line, "rldAqSndFull"))  WeapInfo[TotalW].rldAqSnd = ReadScriptIntField(value, line, "weapon rldAqSndFull");
	if (strstr(line, "rldAqSndPart"))  WeapInfo[TotalW].rldAqSndPart = ReadScriptIntField(value, line, "weapon rldAqSndPart");
	if (strstr(line, "rckAqSnd"))  WeapInfo[TotalW].pmpAqSnd = ReadScriptIntField(value, line, "weapon rckAqSnd");
	if (strstr(line, "modAqSnd"))  WeapInfo[TotalW].modAqSnd = ReadScriptIntField(value, line, "weapon modAqSnd");

	if (strstr(line, "mustRack")) readBool(value, WeapInfo[TotalW].mustPump);
	if (strstr(line, "autoRack")) readBool(value, WeapInfo[TotalW].autoPump);
	if (strstr(line, "autoReload")) readBool(value, WeapInfo[TotalW].autoReload);

	if (strstr(line, "canRun")) readBool(value, WeapInfo[TotalW].canRun);
	if (strstr(line, "cannotMortal")) readBool(value, WeapInfo[TotalW].cannotMortal);

	if (strstr(line, "semiAuto")) readBool(value, WeapInfo[TotalW].semiauto);
	if (strstr(line, "fullAuto")) readBool(value, WeapInfo[TotalW].fullauto);

	if (strstr(line, "land_power"))  WeapInfo[TotalW].Power = ReadScriptFloatField(value, line, "weapon land_power");
	if (strstr(line, "land_veloc"))  WeapInfo[TotalW].Veloc = ReadScriptFloatField(value, line, "weapon land_veloc");
	if (strstr(line, "land_prec"))   WeapInfo[TotalW].Prec = ReadScriptFloatField(value, line, "weapon land_prec");
	if (strstr(line, "land_fall"))   WeapInfo[TotalW].Fall = ReadScriptFloatField(value, line, "weapon land_fall");

	if (strstr(line, "aqua_power"))  WeapInfo[TotalW].PowerAq = ReadScriptFloatField(value, line, "weapon aqua_power");
	if (strstr(line, "aqua_veloc"))  WeapInfo[TotalW].VelocAq = ReadScriptFloatField(value, line, "weapon aqua_veloc");
	if (strstr(line, "aqua_prec"))   WeapInfo[TotalW].PrecAq = ReadScriptFloatField(value, line, "weapon aqua_prec");
	if (strstr(line, "aqua_fall"))   WeapInfo[TotalW].FallAq = ReadScriptFloatField(value, line, "weapon aqua_fall");

	if (strstr(line, "loud"))   WeapInfo[TotalW].Loud = ReadScriptFloatField(value, line, "weapon loud");
	if (strstr(line, "rate"))   WeapInfo[TotalW].Rate = ReadScriptFloatField(value, line, "weapon rate");
	if (strstr(line, "shots"))  WeapInfo[TotalW].Shots = ReadScriptIntField(value, line, "weapon shots");
	if (strstr(line, "reload")) WeapInfo[TotalW].Reload = ReadScriptIntField(value, line, "weapon reload");
	if (strstr(line, "trace"))  WeapInfo[TotalW].TraceC = ReadScriptIntField(value, line, "weapon trace") - 1;
	if (strstr(line, "optic"))  WeapInfo[TotalW].Optic = ReadScriptFloatField(value, line, "weapon optic");
	//if (strstr(line, "price")) WeapInfo[TotalW].Price =        atoi(value);

	if (strstr(line, "unzoom")) readBool(value, WeapInfo[TotalW].unzoom);
	if (strstr(line, "breathaim")) readBool(value, WeapInfo[TotalW].breathaim);

	if (strstr(line, "harpoon")) readBool(value, WeapInfo[TotalW].harpoon);

	if (strstr(line, "cross")) readBool(value, WeapInfo[TotalW].cross);
	if (strstr(line, "croR")) WeapInfo[TotalW].crossRed = ReadScriptIntField(value, line, "weapon crosshair red");
	if (strstr(line, "croG")) WeapInfo[TotalW].crossGreen = ReadScriptIntField(value, line, "weapon crosshair green");
	if (strstr(line, "croB")) WeapInfo[TotalW].crossBlue = ReadScriptIntField(value, line, "weapon crosshair blue");

	if (strstr(line, "shake"))   WeapInfo[TotalW].shake = ReadScriptFloatField(value, line, "weapon shake");

	if (strstr(line, "radar")) readBool(value, WeapInfo[TotalW].onRadar);
	if (strstr(line, "radR")) WeapInfo[TotalW].radarRed = ReadScriptIntField(value, line, "weapon radar red");
	if (strstr(line, "radG")) WeapInfo[TotalW].radarGreen = ReadScriptIntField(value, line, "weapon radar green");
	if (strstr(line, "radB")) WeapInfo[TotalW].radarBlue = ReadScriptIntField(value, line, "weapon radar blue");
	if (strstr(line, "radTime"))  WeapInfo[TotalW].radarTime = ReadScriptIntField(value, line, "weapon radar time");

	if (strstr(line, "muzzflash")) readBool(value, WeapInfo[TotalW].MuzzFlash);
	if (strstr(line, "chamflash")) readBool(value, WeapInfo[TotalW].ChamFlash);

	if (strstr(line, "recoil"))  WeapInfo[TotalW].recoil = ReadScriptFloatField(value, line, "weapon recoil");

	if (strstr(line, "retrieve")) readBool(value, WeapInfo[TotalW].retrieve);

}

void ReadWeapons(FILE *stream)
{

	//area
  char tempProjectName[128];
  int timeOfDay, dinSelect;
  ReadScriptCommandLineOptions(tempProjectName, timeOfDay, dinSelect);

  TotalW = 0;

  char line[256], *value;
  while (fgets( line, 255, stream))
  {
    if (strstr(line, "}")) break;
    if (strstr(line, "{"))
      while (fgets( line, 255, stream))
      {
        // Validate before any field or end-of-section calculation indexes
        // WeapInfo; an empty eleventh section must be rejected too.
        RequireScriptSlot(TotalW, 10, "weapons");
        if (strstr(line, "}"))
        {

			WeapInfo[TotalW].radarColour565 = ((WeapInfo[TotalW].radarRed >> 3) << 11) | ((WeapInfo[TotalW].radarGreen >> 2) << 5) | (WeapInfo[TotalW].radarBlue >> 3);
			WeapInfo[TotalW].radarColour555 = ((WeapInfo[TotalW].radarRed >> 3) << 10) | ((WeapInfo[TotalW].radarGreen >> 3) << 5) | (WeapInfo[TotalW].radarBlue >> 3);

			WeapInfo[TotalW].crossColour565 = ((WeapInfo[TotalW].crossRed >> 3) << 11) | ((WeapInfo[TotalW].crossGreen >> 2) << 5) | (WeapInfo[TotalW].crossBlue >> 3);
			WeapInfo[TotalW].crossColour555 = ((WeapInfo[TotalW].crossRed >> 3) << 10) | ((WeapInfo[TotalW].crossGreen >> 3) << 5) | (WeapInfo[TotalW].crossBlue >> 3);

			if (WeapInfo[TotalW].Veloc > WeapInfo[TotalW].VelocAq) WeapInfo[TotalW].aqLow = true;
			else WeapInfo[TotalW].aqLow = false;

          // WeapInfo has 10 entries; an 11th weapon section would overflow.
          if (TotalW >= 10)
            DoHalt("Script loading error: too many weapons (max 10).");
          TotalW++;
          break;
        }
        value = strstr(line, "=");

		if (!value &&
			!strstr(line, "overwrite") &&
			!strstr(line, "addition")) {
			char errorBuff[100];
			sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: Weapons: %s", WeapInfo[TotalW].Name);
			DoHalt(errorBuff);
		}
        value = value ? value + 1 : line;

		ReadWeaponLine(stream, value, line);

		if (strstr(line, "overwrite") || strstr(line, "addition")) {

			if (OverwriteBlockApplies(line, tempProjectName, timeOfDay, dinSelect)) {

				while (fgets(line, 255, stream)) {
					if (strstr(line, "}")) break;

					value = strstr(line, "=");
					if (!value){
						char errorBuff[100];
						sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: Weapons: %s", WeapInfo[TotalW].Name);
						DoHalt(errorBuff);
					}
					value++;

					ReadWeaponLine(stream, value, line);

				}

			}
			else {
				SkipSector(stream);
			}
		}

      }

  }

}
