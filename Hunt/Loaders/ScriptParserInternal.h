// ==========================================================================
// ScriptParserInternal.h -- cross-TU declarations for the _RES.TXT parser
// ==========================================================================
//
// The parser is split into grammar-scoped translation units (see
// CarnivoresDoc/working/specs/c2-script-parser-split.md). This header carries
// the shared common-block option state and the recovery/reading helpers the
// grammar groups and the driver call across TU boundaries. The split is
// mechanical: bodies, diagnostics, recovery policy, and parse order are
// unchanged; only file placement and linkage moved.

#pragma once

#include <cstddef>
#include <cstdio>

#include "Hunt.h"
#include "ScriptValueParse.h"

// Common-block survival options, read by ReadCommonOptionsFromScript and
// applied to the engine globals by ApplySurvivalCommonOptions.
struct ScriptCommonOptions
{
  bool hasSurvivalArea = false;
  bool hasSurvivalWeapon = false;
  bool hasSurvivalDayNight = false;
  int survivalArea = 0;
  int survivalWeapon = 0;
  int survivalDayNight = 0;
};

// All recoveries are reported through one policy (LoadDiagnostics.h):
// lenient recovers and logs, strict halts.
void ReportScriptRecovery(const char* field, const char* reason,
                          const char* line);

// Write the recovered-value summary to the hunt log once per full script
// parse and clear it, so repeated parses do not repeat old entries.
void FlushScriptDiagnostics();

// Fatal malformed-field report (strict mode and structural failures).
[[noreturn]] void ScriptFieldFail(const char* what, const char* line,
                                  const char* reason = nullptr);

// _RES.TXT string safety: bounded copy of a quoted value into a fixed char
// field (lenient truncates with a diagnostic, strict halts).
void CopyScriptField(char* dst, size_t dstCap, const char* value,
                     const char* what, const char* line);

// Bounded project-path copy that normalizes the vanilla "external"
// sixth-slot alias for the area-filter logic (LoadValidate.h).
void CopyProjectName(char* dst, const char* src);

bool HasCommandLineOption(const char* option);
bool HasCommandLineValue(const char* option);

// True when the line opens the named script block.
bool IsScriptBlockLine(const char* line, const char* name);

void ReadCommonOptions(FILE* stream, ScriptCommonOptions& options);
void ReadCommonOptionsFromScript(FILE* stream, ScriptCommonOptions& options);
void ApplySurvivalCommonOptions(const ScriptCommonOptions& options);

// Second argv pass over prj=/dtm=/din=, keeping the current globals as the
// starting values.
void ReadScriptCommandLineOptions(char projectName[128], int& timeOfDay,
                                  int& dinSelect);

// One recovery path for every scalar field: Ok returns immediately; any
// other status records a diagnostic and, in lenient mode, returns the
// memory-safe recovered value. Strict mode halts.
int RecoverScriptIntResult(const ScriptIntResult& parsed, const char* field,
                           const char* line);
float RecoverScriptFloatResult(const ScriptFloatResult& parsed,
                               const char* field, const char* line);

int ReadScriptIntField(const char* value, const char* line,
                       const char* field);
int ReadScriptLegacyIntField(const char* value, const char* line,
                             const char* field);
float ReadScriptFloatField(const char* value, const char* line,
                           const char* field);
int ReadScriptInt(const char* value);

// Count guards stay fatal in both modes: there is no value to clamp.
void RequireScriptSlot(int index, int capacity, const char* what);

int ReadScriptIndexField(const char* value, const char* line,
                         const char* field, int capacity);
int RecoverScriptIndex(int index, int capacity, const char* what,
                       const char* line);
int RecoverScriptRange(int value, int minimum, int maximum, const char* what,
                       const char* line);
void RecoverOrderedScriptRange(int& minimum, int& maximum, int capacity,
                               const char* what, const char* line);
int ScriptIndex(const char* value, int capacity, const char* what);

// Legacy _RES.TXT boolean tokens: TRUE/FALSE substrings, unchanged when
// neither token is present.
void readBool(char* value, BOOL& out);
void readBool(char* value, bool& out);

// Shared slot-wipe body for the parser's fixed-capacity lists: clears the
// active slots and resets the count. Callers keep their capacity guards and
// diagnostics.
template <typename T, typename Count>
inline void WipeScriptSlots(T* slots, Count& count)
{
    if (count) {
        for (Count i = 0; i < count; i++)
            slots[i] = {};
    }
    count = 0;
}

// Consume one script sector/block body after its opening line
// (ScriptBlockParse.h).
void SkipSector(FILE* stream);

// Tables group (ScriptParserTables.cpp): trophy, snow, area, and weapon
// tables. ReadTrophyTypeInfo is called by the character group; ReadWeaponLine
// by the contract tests; the driver calls the remaining readers.
void ReadTrophyTypeInfo(FILE* stream, int trophyGroup);
void ReadTrophyTable(FILE* stream);
void ReadSnowType(FILE* stream);
void ReadAreaTable(FILE* stream, int areaNumber);
void ReadWeaponLine(FILE* stream, char* value, char line[256]);
void ReadWeapons(FILE* stream);

// Spawn group (ScriptParserSpawn.cpp): per-character spawn/pack info, the
// spawn-group region/avoid readers, and the spawn/pack tables. The character
// group calls the per-character readers; the driver calls the top-level
// tables. The remaining spawn readers are internal to that translation unit.
void WipePackMembers2();
void WipeSpawnInfo();
void ReadSpawnInfo(FILE* stream);
void ReadPackMember2(FILE* stream);
void ReadSpawnGroup(FILE* stream, char line[256], int mode);
void ReadPackGroup(FILE* stream, char line[256], int mode);
void ReadSpawnTable(FILE* stream);
void ReadPackTable(FILE* stream);

// Shared overwrite/addition filter used by the spawn/pack, weapons and
// characters readers. Trophy-room projects (projectName[18] == 'h') apply
// every block; otherwise area/time/character qualifiers must match the
// active selection.
bool OverwriteBlockApplies(const char* line, const char* projectName,
                           int timeOfDay, int dinSelect);

// Character group (ScriptParserCharacter.cpp): per-character line dispatch and
// the characters section reader. The driver calls ReadCharacters; the contract
// tests call ReadCharacterLine. The remaining character readers and wipe
// helpers are internal to that translation unit.
void ReadCharacterLine(FILE* stream, char* value, char line[256],
                       bool& spawnInfoOverwrite, bool& spawnGroupOverwrite,
                       bool& idleOverwrite, bool& idle2Overwrite,
                       bool& roarOverwrite, bool& killOverwrite,
                       bool& waterDieOverwrite, bool& deathTypeOverwrite,
                       bool& trophyTypeOverwrite, bool& idleGroupOverwrite,
                       bool& idle2GroupOverwrite, bool& memberOverwrite);
void ReadCharacters(FILE* stream);
