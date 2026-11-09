// ==========================================================================
// ScriptParserSpawn.cpp -- _RES.TXT spawn and pack table readers
// ==========================================================================
//
// Extracted mechanically from ScriptParser.cpp (see
// CarnivoresDoc/working/specs/c2-script-parser-split.md, Slice 4).
// Behavior, diagnostics, recovery policy, and parse order are unchanged;
// only file placement and linkage
// moved. The driver and the character group reach these readers through the
// declarations in ScriptParserInternal.h.

#include "Hunt.h"
#include "ScriptParserInternal.h"
#include <cstring>

void WipePackMembers2() {
	WipeScriptSlots(DinoInfo[TotalC].packMember2, DinoInfo[TotalC].packMember2Ch);
}

void WipeSpawnInfo() {
	WipeScriptSlots(DinoInfo[TotalC].SpawnInfo, DinoInfo[TotalC].SpawnInfoCh);
}

// Shared body for the character and pack spawn-info blocks: both fill a
// TSpawnInfo slot list with the same grammar; only the container, the guard
// label and the diagnostics differ.
static void ReadSpawnInfoBlock(FILE* stream, TSpawnInfo* info, int& count,
                               const char* what, const char* errorContext,
                               const char* ratioLabel, const char* groupLabel)
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
			sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: %s", errorContext);
			DoHalt(errorBuff);
		}
		value++;

		if (strstr(line, "spawnratio")) info[count].spawnRatio = ReadScriptFloatField(value, line, ratioLabel);
		if (strstr(line, "spawngroup")) info[count].spawnGroup = ReadScriptIndexField(value, line, groupLabel, 256);
	}
}

void ReadSpawnInfo(FILE *stream)
{
	char errorContext[100];
	sprintf_s(errorContext, sizeof(errorContext), "SpawnInfo: %s", DinoInfo[TotalC].Name);
	ReadSpawnInfoBlock(stream, DinoInfo[TotalC].SpawnInfo,
	                   DinoInfo[TotalC].SpawnInfoCh,
	                   "character spawn-info list", errorContext,
	                   "spawn ratio", "spawn group");
}

void WipeSpawnInfoPack() {
	RequireScriptSlot(packTypeCount, 1024, "pack types");
	WipeScriptSlots(packType[packTypeCount].SpawnInfo, packType[packTypeCount].SpawnInfoCh);
}

void ReadSpawnInfoPack(FILE *stream)
{
	RequireScriptSlot(packTypeCount, 1024, "pack types");
	char errorContext[100];
	sprintf_s(errorContext, sizeof(errorContext), "SpawnInfo: Pack%i", packTypeCount);
	ReadSpawnInfoBlock(stream, packType[packTypeCount].SpawnInfo,
	                   packType[packTypeCount].SpawnInfoCh,
	                   "pack spawn-info list", errorContext,
	                   "pack spawn ratio", "pack spawn group");
}

void ReadPackMember2(FILE *stream) {
	RequireScriptSlot(DinoInfo[TotalC].packMember2Ch, 32, "character pack-member list");
	char *value;
	char line[256];

	while (fgets(line, 255, stream)) {
		if (strstr(line, "}")) {

			DinoInfo[TotalC].packMember2Ch++;
			break;
		}
		value = strstr(line, "=");
		if (!value) {
			char errorBuff[100];
			sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: PackInfo: %s", DinoInfo[TotalC].Name);
			DoHalt(errorBuff);
		}
		value++;

		if (strstr(line, "group")) DinoInfo[TotalC].packMember2[DinoInfo[TotalC].packMember2Ch].packGroup = ReadScriptIndexField(value, line, "pack member group", 1024);
		if (strstr(line, "ratio")) DinoInfo[TotalC].packMember2[DinoInfo[TotalC].packMember2Ch].ratio = ReadScriptFloatField(value, line, "pack member ratio");

	}
}

// Shared body for the spawn-group region and avoid-region blocks: the C2
// grammar is identical apart from the target array and the diagnostic labels.
static void ReadSpawnRegionBlock(FILE* stream, TSpawnRegion* regions, int& count,
                                 const char* what, const char* label,
                                 const char* fieldPrefix)
{
	RequireScriptSlot(TotalSpawnGroup, 256, "spawn groups");
	RequireScriptSlot(count, 16, what);
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
			sprintf_s(errorBuff, sizeof(errorBuff), "Script loading error: %s: SpawnGroup %i", label, TotalSpawnGroup);
			DoHalt(errorBuff);
		}
		value++;

		char xmaxLabel[32], xminLabel[32], ymaxLabel[32], yminLabel[32];
		sprintf_s(xmaxLabel, sizeof(xmaxLabel), "%s xmax", fieldPrefix);
		sprintf_s(xminLabel, sizeof(xminLabel), "%s xmin", fieldPrefix);
		sprintf_s(ymaxLabel, sizeof(ymaxLabel), "%s ymax", fieldPrefix);
		sprintf_s(yminLabel, sizeof(yminLabel), "%s ymin", fieldPrefix);
		if (strstr(line, "xmax")) regions[count].XMax = ReadScriptIntField(value, line, xmaxLabel);
		if (strstr(line, "xmin")) regions[count].XMin = ReadScriptIntField(value, line, xminLabel);
		if (strstr(line, "ymax")) regions[count].YMax = ReadScriptIntField(value, line, ymaxLabel);
		if (strstr(line, "ymin")) regions[count].YMin = ReadScriptIntField(value, line, yminLabel);
	}
}

void ReadAvoid(FILE *stream)
{
	ReadSpawnRegionBlock(stream, spawnGroup[TotalSpawnGroup].avoidRegion,
	                     spawnGroup[TotalSpawnGroup].avoidRegionCh,
	                     "spawn-group avoid regions", "Avoid", "avoid");
}

void ReadRegion(FILE *stream)
{
	ReadSpawnRegionBlock(stream, spawnGroup[TotalSpawnGroup].spawnRegion,
	                     spawnGroup[TotalSpawnGroup].spawnRegionCh,
	                     "spawn-group regions", "Region", "region");
}

void WipeAvoidReg() {
	RequireScriptSlot(TotalSpawnGroup, 256, "spawn groups");
	WipeScriptSlots(spawnGroup[TotalSpawnGroup].avoidRegion, spawnGroup[TotalSpawnGroup].avoidRegionCh);
}

void WipeSpawnReg() {
	RequireScriptSlot(TotalSpawnGroup, 256, "spawn groups");
	WipeScriptSlots(spawnGroup[TotalSpawnGroup].spawnRegion, spawnGroup[TotalSpawnGroup].spawnRegionCh);
}

void ReadSpawnTableLine(FILE *stream, char *_value, char line[256], bool &spawnOverwrite, bool &avoidOverwrite) {
	char *value = _value;

	if (strstr(line, "region")) {

		if (spawnOverwrite) {
			WipeSpawnReg();
			spawnOverwrite = false;
		}

		ReadRegion(stream);
	}

	if (strstr(line, "avoid")) {

		if (avoidOverwrite) {
			WipeAvoidReg();
			avoidOverwrite = false;
		}

		ReadAvoid(stream);
	}

	if (strstr(line, "spawnrate")) spawnGroup[TotalSpawnGroup].SpawnRate = ReadScriptFloatField(value, line, "spawn rate");
	if (strstr(line, "spawnmax")) spawnGroup[TotalSpawnGroup].SpawnMax = ReadScriptIntField(value, line, "spawn max");
	if (strstr(line, "spawnmin")) spawnGroup[TotalSpawnGroup].SpawnMin = ReadScriptIntField(value, line, "spawn min");

	if (strstr(line, "densityMulti")) spawnGroup[TotalSpawnGroup].densityMulti = ReadScriptIntField(value, line, "spawn density multiplier");
	if (strstr(line, "moveForward")) readBool(value, spawnGroup[TotalSpawnGroup].moveForward);
	if (strstr(line, "randomise")) readBool(value, spawnGroup[TotalSpawnGroup].Randomised);
	if (strstr(line, "onlyActiveNearby")) readBool(value, spawnGroup[TotalSpawnGroup].OnlyActiveNearby);
	if (strstr(line, "styInRgn")) readBool(value, spawnGroup[TotalSpawnGroup].stayInRegion);

}

//mode 0 = spawntable
//mode 1 = characters
//mode 2 = packtable
	// Decide whether an overwrite/addition block applies to the active map,
	// time of day and dino selection. Shared by the spawn-table and pack-group
	// readers (the C2 grammar is identical for both); the trophy room applies
	// every block without filtering, as it always has.
	bool OverwriteBlockApplies(const char* line, const char* projectName,
	                                  int timeOfDay, int dinSelect)
	{
		if (projectName[18] == 'h') return true;

		bool readThis = true;
		if (strstr(line, "area")) {
			switch ((char)projectName[18]) {
			case '1':
				if (projectName[19]) {
					if (!strstr(line, "area0")) readThis = false; // area10
				}
				else if (!strstr(line, "area1")) readThis = false;
				break;
			case '2':
				if (!strstr(line, "area2")) readThis = false;
				break;
			case '3':
				if (!strstr(line, "area3")) readThis = false;
				break;
			case '4':
				if (!strstr(line, "area4")) readThis = false;
				break;
			case '5':
				if (!strstr(line, "area5")) readThis = false;
				break;
			case '6':
				if (!strstr(line, "area6")) readThis = false;
				break;
			case '7':
				if (!strstr(line, "area7")) readThis = false;
				break;
			case '8':
				if (!strstr(line, "area8")) readThis = false;
				break;
			case '9':
				if (!strstr(line, "area9")) readThis = false;
				break;
			}
		}

		if (strstr(line, "time")) {
			switch (timeOfDay) {
			case 0:
				if (!strstr(line, "time0")) readThis = false;
				break;
			case 1:
				if (!strstr(line, "time1")) readThis = false;
				break;
			case 2:
				if (!strstr(line, "time2")) readThis = false;
				break;
			}
		}

		if (strstr(line, "char")) {
			bool readChar = false;
			if (strstr(line, "char0") && (dinSelect & (1 << 10))) readChar = true;
			if (strstr(line, "char1") && (dinSelect & (1 << 11))) readChar = true;
			if (strstr(line, "char2") && (dinSelect & (1 << 12))) readChar = true;
			if (strstr(line, "char3") && (dinSelect & (1 << 13))) readChar = true;
			if (strstr(line, "char4") && (dinSelect & (1 << 14))) readChar = true;
			if (strstr(line, "char5") && (dinSelect & (1 << 15))) readChar = true;
			if (strstr(line, "char6") && (dinSelect & (1 << 16))) readChar = true;
			if (strstr(line, "char7") && (dinSelect & (1 << 17))) readChar = true;
			if (strstr(line, "char8") && (dinSelect & (1 << 18))) readChar = true;
			if (strstr(line, "char9") && (dinSelect & (1 << 19))) readChar = true;
			if (!readChar) readThis = false;
		}

		return readThis;
	}

void ReadSpawnGroup(FILE *stream, char line[256], int mode) {
	RequireScriptSlot(TotalSpawnGroup, 256, "spawn groups");

	//area
  char tempProjectName[128];
  int timeOfDay, dinSelect;
  ReadScriptCommandLineOptions(tempProjectName, timeOfDay, dinSelect);

	char *value;

	while (fgets(line, 255, stream))
	{
		if (strstr(line, "}"))
		{
			if (mode == 2) {
				RequireScriptSlot(packTypeCount, 1024, "pack types");
				RequireScriptSlot(packType[packTypeCount].SpawnInfoCh, 32, "pack spawn-info list");
				packType[packTypeCount].SpawnInfo[packType[packTypeCount].SpawnInfoCh].spawnRatio = 1;
				packType[packTypeCount].SpawnInfo[packType[packTypeCount].SpawnInfoCh].spawnGroup = TotalSpawnGroup;
				packType[packTypeCount].SpawnInfoCh++;
			}
			if (mode == 1) {
				RequireScriptSlot(DinoInfo[TotalC].SpawnInfoCh, 32, "character spawn-info list");
				DinoInfo[TotalC].SpawnInfo[DinoInfo[TotalC].SpawnInfoCh].spawnRatio = 1;
				DinoInfo[TotalC].SpawnInfo[DinoInfo[TotalC].SpawnInfoCh].spawnGroup = TotalSpawnGroup;
				DinoInfo[TotalC].SpawnInfoCh++;
			}
			RecoverOrderedScriptRange(spawnGroup[TotalSpawnGroup].SpawnMin,
			                          spawnGroup[TotalSpawnGroup].SpawnMax,
			                          256, "spawn group limits", line);
			TotalSpawnGroup++;
			break;
		}
		value = strstr(line, "=");

		if (strstr(line, "overwrite") || strstr(line, "addition")) {

			if (OverwriteBlockApplies(line, tempProjectName, timeOfDay, dinSelect)) {

				bool regionOverwrite = strstr(line, "overwrite");
				bool avoidOverwrite = strstr(line, "overwrite");

				while (fgets(line, 255, stream)) {
					if (strstr(line, "}")) break;

					value = strstr(line, "=");
					if (!value
						&& !strstr(line, "region")
						&& !strstr(line, "avoid"))
						DoHalt("Script loading error: SpawnTable");
					value = value ? value + 1 : line;

					ReadSpawnTableLine(stream, value, line, regionOverwrite, avoidOverwrite);

				}

			}
			else {
				SkipSector(stream);
			}
		}
		else {
			bool temp, temp2;
			temp = false;
			temp2 = false;
			value = value ? value + 1 : line;
			ReadSpawnTableLine(stream, value, line, temp, temp2);

		}

	}
}

void ReadSpawnTable(FILE *stream)
{

	if (g_GameMode == GameMode::SurvivalMode)
	{
		spawnGroup[0].spawnRegion[0].XMax = SurvivalDinoSpawn.XMax;
		spawnGroup[0].spawnRegion[0].XMin = SurvivalDinoSpawn.XMin;
		spawnGroup[0].spawnRegion[0].YMax = SurvivalDinoSpawn.YMax;
		spawnGroup[0].spawnRegion[0].YMin = SurvivalDinoSpawn.YMin;
		spawnGroup[0].spawnRegionCh = 1;
		TotalSpawnGroup = 1;
		SkipSector(stream);
		return;
	}

	TotalSpawnGroup = 0;
	char line[256];
	while (fgets(line, 255, stream))
	{
		if (strstr(line, "}")) break;

		if (strstr(line, "spawngroup")) ReadSpawnGroup(stream, line, 0);
	}
}

void ReadPackTableLine(FILE *stream, char *_value, char line[256], bool &spawnIOverwrite) {
	RequireScriptSlot(packTypeCount, 1024, "pack types");

	char *value = _value;

	if (strstr(line, "spawninfo")) {
		if (spawnIOverwrite) {
			WipeSpawnInfoPack();
			spawnIOverwrite = false;
		}
		ReadSpawnInfoPack(stream);
	}

	if (strstr(line, "spawngroup")) {
		if (spawnIOverwrite) {//use same overwrite
			WipeSpawnInfoPack();
			spawnIOverwrite = false;
		}
		ReadSpawnGroup(stream, line, 2);
	}

	// Older _RES.TXT files may put region blocks directly in a pack override
	// instead of wrapping them in spawngroup. They are not represented in the
	// pack table, but their contents still need to be consumed as one sector.
	if (IsScriptBlockLine(line, "region")) {
		SkipSector(stream);
		return;
	}

	if (strstr(line, "packMax")) packType[packTypeCount].packMax = ReadScriptIntField(value, line, "pack max");
	if (strstr(line, "packMin")) packType[packTypeCount].packMin = ReadScriptIntField(value, line, "pack min");
	if (strstr(line, "packDensity")) packType[packTypeCount].packDensity = ReadScriptFloatField(value, line, "pack density");
	
}

//mode 0 = packtable
//mode 1 = character
void ReadPackGroup(FILE *stream, char line[256], int mode) {
	RequireScriptSlot(packTypeCount, 1024, "pack types");

	//area
  char tempProjectName[128];
  int timeOfDay, dinSelect;
  ReadScriptCommandLineOptions(tempProjectName, timeOfDay, dinSelect);

	char *value;

	while (fgets(line, 255, stream))
	{
		if (strstr(line, "}"))
		{
			if (mode == 1) {
				RequireScriptSlot(DinoInfo[TotalC].packMember2Ch, 32, "character pack-member list");
				DinoInfo[TotalC].packMember2[DinoInfo[TotalC].packMember2Ch].packGroup = packTypeCount;
				DinoInfo[TotalC].packMember2[DinoInfo[TotalC].packMember2Ch].ratio = 1;
				DinoInfo[TotalC].packMember2Ch++;
			}
			RecoverOrderedScriptRange(packType[packTypeCount].packMin,
			                          packType[packTypeCount].packMax,
			                          256, "pack group limits", line);
			packTypeCount++;
			break;
		}

		value = strstr(line, "=");
		value = value ? value + 1 : line;
		bool temp1;
		temp1 = false;
		ReadPackTableLine(stream, value, line, temp1);

		if (strstr(line, "overwrite") || strstr(line, "addition")) {

			if (OverwriteBlockApplies(line, tempProjectName, timeOfDay, dinSelect)) {

				bool spawnInfoOverwrite = strstr(line, "overwrite");

				while (fgets(line, 255, stream)) {
					if (strstr(line, "}")) break;

					value = strstr(line, "=");
					value = value ? value + 1 : line;

					ReadPackTableLine(stream, value, line, spawnInfoOverwrite);

				}

			}
			else {
				SkipSector(stream);
			}
		}

	}
}

void ReadPackTable(FILE *stream)
{
	packTypeCount = 0;

	if (g_GameMode == GameMode::SurvivalMode) {
		SkipSector(stream);
		return;
	}

	char line[256];
	while (fgets(line, 255, stream))
	{
		if (strstr(line, "}")) break;

		if (strstr(line, "packgroup"))ReadPackGroup(stream, line, 0);

	}

}
