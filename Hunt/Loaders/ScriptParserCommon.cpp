// ==========================================================================
// ScriptParserCommon.cpp -- shared _RES.TXT script-parser helpers
// ==========================================================================
//
// Extracted mechanically from ScriptParser.cpp (see
// CarnivoresDoc/working/specs/c2-script-parser-split.md, Slice 2).
// Behavior, diagnostics, recovery policy, and parse order are unchanged;
// only file placement and linkage
// moved. The grammar translation units include ScriptParserInternal.h for
// these helpers.

#include "Hunt.h"
#include "ScriptParserInternal.h"
#include "Core/CommandLineParse.h"
#include "LoadDiagnostics.h"
#include "LoadValidate.h"
#include "ScriptBlockParse.h"
#include "ScriptValueParse.h"

// All diagnostics recorded while a script is parsed are reported through one
// policy (LoadDiagnostics.h): lenient recovers and logs, strict halts. Keep
// the group name stable so CI/tests can filter it.
static const char* kScriptLoadGroup = "ScriptParser";

void ReportScriptRecovery(const char* field, const char* reason,
                          const char* line)
{
  LoadDiagnostics::Instance().Report(kScriptLoadGroup, field, reason, line);
}

// Write the recovered-value summary to the hunt log once per full script
// parse and clear it, so repeated parses do not repeat old entries. Strict
// mode never reaches this point with entries: it halts on the first one.
void FlushScriptDiagnostics()
{
  LoadDiagnostics& diagnostics = LoadDiagnostics::Instance();
  if (diagnostics.Count() == 0)
    return;

  std::string summary = diagnostics.Summary();
  PrintLog(const_cast<char*>(summary.c_str()));
  diagnostics.Clear();
}

// _RES.TXT string safety. Name/file fields are fixed char arrays
// (WeapInfo/DinoInfo fixed arrays, GameTypes.h); an overlong modded
// value previously overflowed via strcpy, and value[strlen(value)-2] indexed
// before the buffer when the quoted value was shorter than ''. In lenient
// mode the value is truncated to the field with a diagnostic; strict mode
// halts. The offending line goes into the message as well: the field name
// alone left modders hunting through the whole script for the bad entry.
[[noreturn]] void ScriptFieldFail(const char* what, const char* line,
                                  const char* reason)
{
  char bad[256];
  bad[0] = 0;
  if (line && !CopyCapped(bad, sizeof(bad), line))
    bad[0] = 0;
  size_t len = strlen(bad);
  while (len > 0 && (bad[len - 1] == '\n' || bad[len - 1] == '\r'))
    bad[--len] = 0;

  const bool hasReason = reason && reason[0] != 0;
  char sz[512];
  if (bad[0] && hasReason)
    sprintf_s(sz, sizeof(sz),
              "Script loading error: %s missing, too long, or malformed (%s).\n"
              "Line: %s",
              what, reason, bad);
  else if (bad[0])
    sprintf_s(sz, sizeof(sz),
              "Script loading error: %s missing, too long, or malformed.\n"
              "Line: %s",
              what, bad);
  else if (hasReason)
    sprintf_s(sz, sizeof(sz),
              "Script loading error: %s missing, too long, or malformed (%s).",
              what, reason);
  else
    sprintf_s(sz, sizeof(sz),
              "Script loading error: %s missing, too long, or malformed.", what);
  DoHalt(sz);
}

// Read the quoted value out of `value` (the text after '='). The line is
// never modified, so a second field on the same line still parses, and the
// key is matched by name (ScriptKeyIs) instead of by searching the line for
// a substring. Recovery policy: a missing/unclosed quote leaves the field
// empty (and is fatal in strict mode); an overlong value is truncated in
// lenient mode instead of halting the whole hunt.
void CopyScriptField(char* dst, size_t dstCap, const char* value,
                     const char* what, const char* line)
{
  const char* quoted = nullptr;
  size_t length = 0;
  if (!dst || dstCap == 0)
    ScriptFieldFail(what, line, "invalid destination field");
  if (!FindQuotedValue(value, &quoted, &length))
  {
    ReportScriptRecovery(what, "missing or unclosed quoted value", line);
    if (LoadDiagnostics::Instance().Strict())
      ScriptFieldFail(what, line, "missing or unclosed quoted value");
    dst[0] = 0;
    return;
  }
  if (length >= dstCap)
  {
    char reason[96];
    sprintf_s(reason, sizeof(reason),
              "value longer than the %u-byte field; truncated",
              static_cast<unsigned>(dstCap));
    ReportScriptRecovery(what, reason, line);
    if (LoadDiagnostics::Instance().Strict())
      ScriptFieldFail(what, line, reason);
    length = dstCap - 1;
  }
  memcpy(dst, quoted, length);
  dst[length] = 0;
}

void CopyProjectName(char* dst, const char* src)
{
  if (!CopyCapped(dst, 128, src)) {
    // ProcessCommandLine() rejects the same option without changing the last
    // valid value. Keep this second argv pass consistent so a malformed later
    // token cannot turn an otherwise valid launch into an abnormal halt.
    PrintLog("Script loading: ignoring overlong project path.\n");
    return;
  }
  // The vanilla sixth slot stores assets as external.map/.rsc; every other
  // slot is areaN. The legacy area-filter logic below reads the fixed path
  // offset that holds the area digit, which a bare "external" basename
  // cannot satisfy (reproduced as an 0xC0000005 launch crash). Normalize the
  // parser-local copy to the slot's logical name so the offset logic keeps
  // working; the engine's file-open path (ProjectName) is untouched and still
  // opens external.map/.rsc. See ProjectBasenameIsExternal/RewriteExternalProjectAlias
  // in LoadValidate.h and tests/test_load_validate.cpp.
  RewriteExternalProjectAlias(dst, 128);
}

bool HasCommandLineOption(const char* option)
{
  for (int a = 0; a < __argc; ++a)
  {
    if (_stricmp(__argv[a], option) == 0)
      return true;
  }
  return false;
}

bool HasCommandLineValue(const char* option)
{
  for (int a = 0; a < __argc; ++a)
  {
    const char* value = nullptr;
    if (CommandLineOptionValue(__argv[a], option, &value))
      return true;
  }
  return false;
}

bool IsScriptBlockLine(const char* line, const char* name)
{
  while (*line == ' ' || *line == '\t')
    ++line;

  const size_t nameLength = strlen(name);
  if (strncmp(line, name, nameLength) != 0)
    return false;

  line += nameLength;
  while (*line == ' ' || *line == '\t')
    ++line;
  return *line == '{';
}

void ReadCommonOptions(FILE* stream, ScriptCommonOptions& options)
{
  char line[256];
  while (fgets(line, 255, stream))
  {
    if (strstr(line, "}"))
      return;

    char* value = strstr(line, "=");
    if (!value)
      DoHalt("Script loading error: common");
    ++value;

    if (ScriptKeyIs(line, "survivalArea"))
    {
      options.survivalArea = RecoverScriptRange(
          ReadScriptIntField(value, line, "survival area"), 1, 10,
          "survival area", line);
      options.hasSurvivalArea = true;
    }
    else if (ScriptKeyIs(line, "survivalWeapon"))
    {
      options.survivalWeapon = RecoverScriptRange(
          ReadScriptIntField(value, line, "survival weapon"), 1, 10,
          "survival weapon", line);
      options.hasSurvivalWeapon = true;
    }
    else if (ScriptKeyIs(line, "survivalDTM"))
    {
      options.survivalDayNight = RecoverScriptRange(
          ReadScriptIntField(value, line, "survival day/night"), 0, 2,
          "survival day/night", line);
      options.hasSurvivalDayNight = true;
    }
  }

  DoHalt("Script loading error: unterminated common block.");
}

void ReadCommonOptionsFromScript(FILE* stream, ScriptCommonOptions& options)
{
  char line[256];
  while (fgets(line, 255, stream))
  {
    if (IsScriptBlockLine(line, "common"))
    {
      ReadCommonOptions(stream, options);
      return;
    }
  }
}

void ApplySurvivalCommonOptions(const ScriptCommonOptions& options)
{
  if (g_GameMode != GameMode::SurvivalMode)
    return;

  if (options.hasSurvivalArea && !HasCommandLineValue("prj="))
  {
    char projectName[128];
    sprintf_s(projectName, sizeof(projectName), "huntdat/areas/area%d",
              options.survivalArea);
    CopyProjectName(ProjectName, projectName);
  }

  if (options.hasSurvivalWeapon && !HasCommandLineValue("wep="))
  {
    // _RES.TXT numbers the survival weapon from one, while WeaponPres is a
    // zero-based bitmask. Stock data uses 8 for its eighth weapon.
    WeaponPres = 1 << (options.survivalWeapon - 1);
  }

  if (options.hasSurvivalDayNight && !HasCommandLineValue("dtm="))
    OptDayNight = options.survivalDayNight;
}

void ReadScriptCommandLineOptions(char projectName[128], int& timeOfDay,
                                  int& dinSelect)
{
  memset(projectName, 0, 128);
  // ProcessCommandLine() has already established the current global value.
  // Start the second argv pass from that value so an overlong later token
  // cannot erase an otherwise valid project selection.
  CopyCapped(projectName, 128, ProjectName);
  timeOfDay = OptDayNight;
  dinSelect = 0;

  for (int a = 0; a < __argc; a++)
  {
    const char* value = nullptr;
    LPSTR argument = __argv[a];

    if (CommandLineOptionValue(argument, "prj=", &value))
    {
      CopyProjectName(projectName, value);
      continue;
    }

    if (CommandLineOptionValue(argument, "dtm=", &value))
    {
      int parsed = 0;
      if (ParseCommandLineInt(value, parsed) && parsed >= 0 && parsed <= 2)
        timeOfDay = parsed;
      continue;
    }

    if (CommandLineOptionValue(argument, "din=", &value))
    {
      int parsed = 0;
      if (ParseCommandLineInt(value, parsed) && parsed >= 0 && parsed <= 1023)
        dinSelect = parsed * 1024;
    }
  }
}

// One recovery path for every scalar field. Ok returns immediately; any
// other status records a diagnostic and, in lenient mode, returns the
// memory-safe recovered value (fallback or clamp). Strict mode halts with
// the same reason the log would have carried.
int RecoverScriptIntResult(const ScriptIntResult& parsed,
                           const char* field, const char* line)
{
  if (parsed.status == ScriptScalarStatus::Ok)
    return parsed.value;

  const char* reason = ScriptScalarStatusReason(parsed.status);
  ReportScriptRecovery(field, reason, line);
  if (LoadDiagnostics::Instance().Strict())
    ScriptFieldFail(field, line, reason);
  return parsed.value;
}

float RecoverScriptFloatResult(const ScriptFloatResult& parsed,
                               const char* field, const char* line)
{
  if (parsed.status == ScriptScalarStatus::Ok)
    return parsed.value;

  const char* reason = ScriptScalarStatusReason(parsed.status);
  ReportScriptRecovery(field, reason, line);
  if (LoadDiagnostics::Instance().Strict())
    ScriptFieldFail(field, line, reason);
  return parsed.value;
}

int ReadScriptIntField(const char* value, const char* line,
                       const char* field)
{
  return RecoverScriptIntResult(ParseScriptIntStatus(value, 0), field, line);
}

int ReadScriptLegacyIntField(const char* value, const char* line,
                             const char* field)
{
  return RecoverScriptIntResult(ParseScriptLegacyIntStatus(value, 0), field,
                                line);
}

float ReadScriptFloatField(const char* value, const char* line,
                           const char* field)
{
  return RecoverScriptFloatResult(ParseScriptFloatStatus(value, 0.0f), field,
                                  line);
}

int ReadScriptInt(const char* value)
{
  return ReadScriptIntField(value, value, "script integer");
}

void RequireScriptSlot(int index, int capacity, const char* what)
{
  if (!IsValidIndex(index, capacity))
  {
    char sz[192];
    sprintf_s(sz, sizeof(sz),
              "Script loading error: %s capacity exceeded (index=%d, max=%d).",
              what, index, capacity - 1);
    DoHalt(sz);
  }
}

int ReadScriptIndexField(const char* value, const char* line,
                         const char* field, int capacity)
{
  const int parsed = ReadScriptIntField(value, line, field);
  return RecoverScriptIndex(parsed, capacity, field, line);
}

// Value-derived indices recover by clamping into the fixed array in lenient
// mode (0 or capacity-1), so a bad mod value can neither abort the hunt nor
// touch memory out of bounds. Count guards (RequireScriptSlot at a capacity)
// stay fatal in both modes: there is no value to clamp, the section itself
// does not fit.
int RecoverScriptIndex(int index, int capacity, const char* what,
                       const char* line)
{
  if (IsValidIndex(index, capacity))
    return index;

  char reason[96];
  sprintf_s(reason, sizeof(reason), "index out of range [0..%d]", capacity - 1);
  ReportScriptRecovery(what, reason, line);
  if (LoadDiagnostics::Instance().Strict())
  {
    char bad[192];
    bad[0] = 0;
    if (line && !CopyCapped(bad, sizeof(bad), line))
      bad[0] = 0;
    size_t len = strlen(bad);
    while (len > 0 && (bad[len - 1] == '\n' || bad[len - 1] == '\r'))
      bad[--len] = 0;

    char sz[320];
    if (bad[0])
      sprintf_s(sz, sizeof(sz),
                "Script loading error: %s index out of range (index=%d, max=%d).\n"
                "Line: %s",
                what, index, capacity - 1, bad);
    else
      sprintf_s(sz, sizeof(sz),
                "Script loading error: %s index out of range (index=%d, max=%d).",
                what, index, capacity - 1);
    DoHalt(sz);
  }
  return index < 0 ? 0 : capacity - 1;
}

// Clamp a value into an inclusive range. Used by the survival defaults and
// other settings whose out-of-range handling used to halt a hunt (v1.1.9).
int RecoverScriptRange(int value, int minimum, int maximum,
                       const char* what, const char* line)
{
  if (value >= minimum && value <= maximum)
    return value;

  char reason[96];
  sprintf_s(reason, sizeof(reason), "expected %d..%d, got %d", minimum,
            maximum, value);
  ReportScriptRecovery(what, reason, line);
  if (LoadDiagnostics::Instance().Strict())
    ScriptFieldFail(what, line, reason);
  return value < minimum ? minimum : maximum;
}

// A configured minimum/maximum pair must fit its destination capacity and
// retain the ordering expected by the spawning loops. Lenient mode clamps
// both ends into [0, capacity] and orders the pair; strict mode halts.
void RecoverOrderedScriptRange(int& minimum, int& maximum, int capacity,
                               const char* what, const char* line)
{
  if (IsValidOrderedRange(minimum, maximum, capacity))
    return;

  char reason[128];
  sprintf_s(reason, sizeof(reason),
            "invalid range (min=%d, max=%d, capacity=%d)", minimum, maximum,
            capacity);
  ReportScriptRecovery(what, reason, line);
  if (LoadDiagnostics::Instance().Strict())
    ScriptFieldFail(what, line, reason);

  if (minimum < 0) minimum = 0;
  if (minimum > capacity) minimum = capacity;
  if (maximum < 0) maximum = 0;
  if (maximum > capacity) maximum = capacity;
  if (minimum > maximum)
  {
    const int swap = minimum;
    minimum = maximum;
    maximum = swap;
  }
}

int ScriptIndex(const char* value, int capacity, const char* what)
{
  const int index = value ? ReadScriptInt(value) : -1;
  return RecoverScriptIndex(index, capacity, what, value);
}

void readBool(char *value, BOOL &out) {
	if (strstr(value, "TRUE")) out = true;
	if (strstr(value, "FALSE")) out = false;
}

void readBool(char *value, bool &out) {
	if (strstr(value, "TRUE")) out = true;
	if (strstr(value, "FALSE")) out = false;
}

void SkipSector(FILE *stream)
{
  if (!ConsumeScriptBlockBody(stream))
    DoHalt("Script loading error: unterminated block.");
}
