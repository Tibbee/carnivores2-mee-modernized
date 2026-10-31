// test_script_parser_entry.cpp -- production _RES.TXT line-reader contracts
//
// ReadWeaponLine()/ReadCharacterLine() dispatch free-form text fields
// (name/file/gunshot/pic1/picc/bModel) before the numeric/flag dispatch and
// before the block openers. A modded name or path can contain a field key or
// a block name as a substring; these tests drive the real readers with such
// values so a text line can neither be strict-parsed as a number nor open a
// block that consumes the rest of the stream.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "Hunt.h"
#include "Game/CharacterInternal.h"
#include "Loaders/LoadDiagnostics.h"
#include "test_support.h"

// Production entry points defined in Hunt/Loaders/ScriptParser.cpp.
void ReadWeaponLine(FILE* stream, char* _value, char line[256]);
void ReadCharacterLine(FILE* stream, char* _value, char line[256],
                       bool& spawnInfoOverwrite, bool& spawnGroupOverwrite,
                       bool& idleOverwrite, bool& idle2Overwrite,
                       bool& roarOverwrite, bool& killOverwrite,
                       bool& waterDieOverwrite, bool& deathTypeOverwrite,
                       bool& trophyTypeOverwrite, bool& idleGroupOverwrite,
                       bool& idle2GroupOverwrite, bool& memberOverwrite);

// Section/common entry points exercised by the split characterization tests.
void ReadWeapons(FILE* stream);
void ReadCharacters(FILE* stream);
void readBool(char* value, BOOL& out);
void readBool(char* value, bool& out);
void SkipSector(FILE* stream);

// Dedup slice: the shared overwrite filter, the paired readers that are
// folded into shared helpers, and one wipe helper that exercises the shared
// slot-wipe template.
bool OverwriteBlockApplies(const char* line, const char* projectName,
                           int timeOfDay, int dinSelect);
void ReadAvoid(FILE* stream);
void ReadRegion(FILE* stream);
void ReadSpawnInfo(FILE* stream);
void ReadSpawnInfoPack(FILE* stream);
void ReadIdleGroupInfo(FILE* stream);
void ReadIdle2GroupInfo(FILE* stream);
void WipeKillTypes();

namespace {

// Keeps a line and its post-'=' value pointer in stable mutable storage; the
// production signatures take char* even though they only read the text.
class ScriptLine final
{
public:
    explicit ScriptLine(const char* text)
        : m_text(text, text + std::strlen(text) + 1)
    {}

    char* data() { return m_text.data(); }

    char* value()
    {
        char* equals = std::strchr(m_text.data(), '=');
        return equals ? equals + 1 : m_text.data();
    }

private:
    std::vector<char> m_text;
};

using test_support::TempScript;

void CallWeaponLine(FILE* stream, const char* text)
{
    ScriptLine line(text);
    ReadWeaponLine(stream, line.value(), line.data());
}

void CallCharacterLine(FILE* stream, const char* text)
{
    ScriptLine line(text);
    bool spawnInfoFlag = false, spawnGroupFlag = false, idleFlag = false,
         idle2Flag = false, roarFlag = false, killFlag = false,
         waterDieFlag = false, deathTypeFlag = false, trophyTypeFlag = false,
         idleGroupFlag = false, idle2GroupFlag = false, memberFlag = false;
    ReadCharacterLine(stream, line.value(), line.data(),
                      spawnInfoFlag, spawnGroupFlag, idleFlag, idle2Flag,
                      roarFlag, killFlag, waterDieFlag, deathTypeFlag,
                      trophyTypeFlag, idleGroupFlag, idle2GroupFlag, memberFlag);
}

void ResetWeapon()
{
    TotalW = 0;
    WeapInfo[0] = TWeapInfo{};
}

void ResetCharacter()
{
    g_CharacterRoster.Reset();
    g_CharacterRoster.BeginDefinition(0);
    TotalC = 0;
    DinoInfo[0] = TDinoInfo{};
}

TEST(RosterIntegration, InstalledResourceScript)
{
    if (!std::getenv("C2_ROSTER_PROBE"))
        GTEST_SKIP() << "Opt-in: run from a backed-up deployed game directory";
    EXPECT_NO_THROW(LoadResourcesScript());
    EXPECT_GT(TotalC, 128);
    EXPECT_EQ(TotalC, DinoInfo.size());
    EXPECT_EQ(TotalC, ChInfo.size());
    EXPECT_TRUE(g_CharacterRoster.Frozen());
    std::cout << "Loaded and finalized " << TotalC << " character definitions\n";
}

void ResetSpawnGroup()
{
    TotalSpawnGroup = 0;
    spawnGroup[0] = TSpawnGroup{};
}

void ResetPackType()
{
    packTypeCount = 0;
    packType[0] = TPackType{};
}

}  // namespace

void PrintLog(LPSTR)
{
}

void PrintLogVerbose(LPSTR message)
{
    PrintLog(message);
}

LPVOID _HeapAlloc(HANDLE, DWORD, DWORD)
{
    return nullptr;
}

LPVOID _HeapAlloc(HANDLE, DWORD, DWORD, MemoryTag)
{
    return nullptr;
}

LPVOID _HeapAllocImpl(HANDLE, DWORD, DWORD, MemoryTag, const char*, int)
{
    return nullptr;
}

BOOL _HeapFree(HANDLE, DWORD, LPVOID)
{
    return TRUE;
}

[[noreturn]] void DoHalt(LPSTR message)
{
    throw std::runtime_error(message ? message : "halt");
}

TEST(ScriptParserEntry, WeaponTextValuesReachOnlyTextFields)
{
    ResetWeapon();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    // Every value below contains a numeric/flag key as a lowercase substring
    // ('crossbow' -> cross, 'separated' -> rate, 'reload_click' -> reload).
    // None of them may consume the text as a number.
    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "name = 'crossbow'"));
    EXPECT_STREQ(WeapInfo[0].Name, "crossbow");
    EXPECT_FALSE(WeapInfo[0].cross);
    EXPECT_EQ(WeapInfo[0].crossRed, 0);

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "file = 'models/separated/rifle.car'"));
    EXPECT_STREQ(WeapInfo[0].FName, "models/separated/rifle.car");
    EXPECT_FLOAT_EQ(WeapInfo[0].Rate, 0.0f);

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "gunshot = 'sounds/reload_click.wav'"));
    EXPECT_STREQ(WeapInfo[0].SFXName, "sounds/reload_click.wav");
    EXPECT_TRUE(WeapInfo[0].MGSSound);
    EXPECT_EQ(WeapInfo[0].Reload, 0);

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "bModel = 'proj/rate_thing.car'"));
    EXPECT_STREQ(WeapInfo[0].BLName, "proj/rate_thing.car");
    EXPECT_TRUE(WeapInfo[0].bullet);
    EXPECT_FLOAT_EQ(WeapInfo[0].Rate, 0.0f);

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "pic1 = 'ammo/bullet1.tga'"));
    EXPECT_STREQ(WeapInfo[0].BFName, "ammo/bullet1.tga");

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "picc = 'ammo/chamb1.tga'"));
    EXPECT_STREQ(WeapInfo[0].CFName, "ammo/chamb1.tga");
    EXPECT_TRUE(WeapInfo[0].picch);
}

TEST(ScriptParserEntry, WeaponNumericDispatchIsUnchanged)
{
    ResetWeapon();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "rate = 0.45"));
    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "land_power = 5"));
    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "reload = 3"));
    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "cross = TRUE"));
    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "recoil = 0.35"));

    EXPECT_FLOAT_EQ(WeapInfo[0].Rate, 0.45f);
    EXPECT_FLOAT_EQ(WeapInfo[0].Power, 5.0f);
    EXPECT_EQ(WeapInfo[0].Reload, 3);
    EXPECT_TRUE(WeapInfo[0].cross);
    EXPECT_FLOAT_EQ(WeapInfo[0].recoil, 0.35f);
}

TEST(ScriptParserEntry, CharacterTextValuesReachOnlyTextFields)
{
    ResetCharacter();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    // 'massive' contains the mass key; the name below contains it too.
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "file = 'models/massive/trex.car'"));
    EXPECT_STREQ(DinoInfo[0].FName, "models/massive/trex.car");
    EXPECT_FLOAT_EQ(DinoInfo[0].Mass, 0.0f);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "name = 'the mass'"));
    EXPECT_STREQ(DinoInfo[0].Name, "the mass");
    EXPECT_FLOAT_EQ(DinoInfo[0].Mass, 0.0f);
}

TEST(ScriptParserEntry, CharacterTextValuesDoNotOpenBlocks)
{
    ResetCharacter();
    // A block opener in a file path used to consume everything up to the next
    // '}'. The stream contains one such body per opener so the old behavior
    // would have advanced the corresponding count.
    TempScript stream(
        "spawnratio = 1\n"
        "}\n"
        "hunterAnim = 3\n"
        "}\n"
        "startChance = 0.5\n"
        "}\n"
        "sentinel = 7\n");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "file = 'models/spawninfo/trex.car'"));
    EXPECT_STREQ(DinoInfo[0].FName, "models/spawninfo/trex.car");
    EXPECT_EQ(DinoInfo[0].SpawnInfoCh, 0);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "file = 'models/killtype/trex.car'"));
    EXPECT_EQ(DinoInfo[0].killTypeCount, 0);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "file = 'models/idlegroup/trex.car'"));
    EXPECT_EQ(DinoInfo[0].idleGroupCount, 0);

    char line[256];
    ASSERT_NE(fgets(line, sizeof(line), stream.stream), nullptr);
    // The stream must still be at its first line: no text line may consume a
    // block body before the outer loop resumes.
    EXPECT_STREQ(line, "spawnratio = 1\n");
}

TEST(ScriptParserEntry, CharacterNumericDispatchIsUnchanged)
{
    ResetCharacter();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "health = 13.5"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "ai = 10"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "mass = 120.5"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "smellK = 0.8"));

    EXPECT_EQ(DinoInfo[0].Health0, 13);  // legacy truncation of 13.5
    EXPECT_EQ(DinoInfo[0].Clone, 10);
    EXPECT_FLOAT_EQ(DinoInfo[0].Mass, 120.5f);
    EXPECT_FLOAT_EQ(DinoInfo[0].SmellK, 0.8f);
}

namespace {

// Policy tests: the same legacy corpus that must keep loading in lenient mode
// (the player default) must be rejected by strict mode (CI / mod authoring).
class ScriptPolicyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LoadDiagnostics::Instance().Clear();
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
    }

    void TearDown() override
    {
        LoadDiagnostics::Instance().Clear();
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
    }
};

}  // namespace

TEST(BrachiosaurusRangeContractTest, ProductionRangeUsesDangerousSlotsWithoutCloneDefaults)
{
    const TDinoInfo savedDino = DinoInfo[0];
    const int savedOverride = DinoAggressMulti[0];
    const int savedOption = OptAgres;
    OptAgres = 128;
    DinoInfo[0] = {};
    TCharacter character = {};
    character.CType = 0;

    for (int clone : {AI_BRACHDANGER, AI_LANDBRACH}) {
        const TAIInfo savedBehavior = AIInfo[clone];
        AIInfo[clone] = {};
        character.Clone = clone;
        DinoInfo[0].aggress = clone == AI_LANDBRACH ? 64 : 85;
        const float expected = clone == AI_LANDBRACH ? 8208.0f : 10896.0f;
        for (int overrideValue : {0, 7}) {
            DinoAggressMulti[0] = overrideValue;
            EXPECT_FLOAT_EQ(GetCharacterAggressionRange(&character), expected);
            const float eventRange = GetCharacterHunterEventRange(&character);
            EXPECT_FLOAT_EQ(eventRange, expected * kHunterEventRangeScale);
            EXPECT_FALSE(ShouldFleeFromHunterEvent(DinoInfo[0].aggress, false,
                                                   5000.0f, eventRange));
        }
        AIInfo[clone] = savedBehavior;
    }

    DinoInfo[0] = savedDino;
    DinoAggressMulti[0] = savedOverride;
    OptAgres = savedOption;
}

TEST_F(ScriptPolicyTest, LegacyScalarCorpusLoadsWithoutDiagnostics)
{
    ResetCharacter();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "health = 13.5"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "scale0 = 1000.0"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "mass = 120.5"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "smellK = 0.8"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "ai = 10"));

    EXPECT_EQ(DinoInfo[0].Health0, 13);
    EXPECT_EQ(DinoInfo[0].Scale0, 1000);
    EXPECT_FLOAT_EQ(DinoInfo[0].Mass, 120.5f);
    EXPECT_FLOAT_EQ(DinoInfo[0].SmellK, 0.8f);
    EXPECT_EQ(DinoInfo[0].Clone, 10);
    EXPECT_EQ(LoadDiagnostics::Instance().Count(), 0u);
}

TEST_F(ScriptPolicyTest, MalformedScalarsRecoverWithDiagnostics)
{
    ResetCharacter();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "health = oops"));
    EXPECT_EQ(DinoInfo[0].Health0, 0);
    EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);

    const std::size_t afterFirst = LoadDiagnostics::Instance().Count();
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "mass = nan"));
    EXPECT_FLOAT_EQ(DinoInfo[0].Mass, 0.0f);
    EXPECT_GT(LoadDiagnostics::Instance().Count(), afterFirst);

    EXPECT_NO_THROW(CallCharacterLine(
        stream.stream, "ai = 99999999999999999999"));
    EXPECT_EQ(DinoInfo[0].Clone, (std::numeric_limits<int>::max)());
}

TEST_F(ScriptPolicyTest, OverlongTextFieldTruncatesWithDiagnostic)
{
    ResetWeapon();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    const std::string longPath(120, 'a');
    const std::string line = "file = '" + longPath + "'";

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, line.c_str()));
    EXPECT_EQ(std::strlen(WeapInfo[0].FName), sizeof(WeapInfo[0].FName) - 1);
    EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);
}

TEST_F(ScriptPolicyTest, MissingQuotedValueClearsFieldWithDiagnostic)
{
    ResetWeapon();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallWeaponLine(stream.stream, "name = unquoted"));
    EXPECT_STREQ(WeapInfo[0].Name, "");
    EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);
}

TEST_F(ScriptPolicyTest, OutOfRangeIndexClampsWithDiagnostic)
{
    ResetCharacter();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "fearCall = 999"));
    EXPECT_TRUE(DinoInfo[0].fearCall[63]);
    EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);
}

TEST_F(ScriptPolicyTest, StrictModeHaltsOnRecoverableValues)
{
    LoadDiagnostics::Instance().SetMode(LoadMode::Strict);
    ResetCharacter();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_THROW(CallCharacterLine(stream.stream, "health = oops"),
                 std::runtime_error);
    EXPECT_THROW(CallCharacterLine(stream.stream, "mass = nan"),
                 std::runtime_error);
    EXPECT_THROW(CallCharacterLine(
                     stream.stream, "ai = 99999999999999999999"),
                 std::runtime_error);
    EXPECT_THROW(CallCharacterLine(stream.stream, "fearCall = 999"),
                 std::runtime_error);

    ResetWeapon();
    EXPECT_THROW(CallWeaponLine(stream.stream, "file = unquoted"),
                 std::runtime_error);
}

TEST_F(ScriptPolicyTest, StrictModeStillAcceptsLegacyForms)
{
    LoadDiagnostics::Instance().SetMode(LoadMode::Strict);
    ResetCharacter();
    TempScript stream("");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "health = 13.5"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "scale0 = 1000.0"));
    EXPECT_NO_THROW(CallCharacterLine(stream.stream, "mass = 120.5"));
    EXPECT_EQ(DinoInfo[0].Health0, 13);
    EXPECT_EQ(DinoInfo[0].Scale0, 1000);
    EXPECT_EQ(LoadDiagnostics::Instance().Count(), 0u);
}

// ---------------------------------------------------------------------------
// Section-level and shared-helper characterization for the ScriptParser split.
// These pin the behavior of the entry points the split moves between TUs:
// ReadWeapons/ReadCharacters section scanning and capacity, readBool's exact
// token semantics, and SkipSector's nested-block/marker/EOF policy.
// ---------------------------------------------------------------------------

TEST(ScriptParserSections, WeaponsSectionReadsMinimalEntry)
{
    ResetWeapon();
    TempScript stream(
        "{\n"
        " name = 'Test Gun'\n"
        " land_veloc = 100\n"
        "}\n"
        "}\n");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(ReadWeapons(stream.stream));
    EXPECT_EQ(TotalW, 1);
    EXPECT_STREQ(WeapInfo[0].Name, "Test Gun");
    EXPECT_FLOAT_EQ(WeapInfo[0].Veloc, 100.0f);
    EXPECT_EQ(WeapInfo[0].aqLow, WeapInfo[0].Veloc > WeapInfo[0].VelocAq);
}

TEST(ScriptParserSections, WeaponsSectionRejectsEleventhEntry)
{
    ResetWeapon();
    std::string fixture;
    for (int i = 0; i < 11; i++)
    {
        fixture += "{\n name = 'w";
        fixture += std::to_string(i);
        fixture += "'\n}\n";
    }
    fixture += "}\n";
    TempScript stream(fixture.c_str());
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_THROW(ReadWeapons(stream.stream), std::runtime_error);
}

TEST(ScriptParserSections, CharactersSectionReadsMinimalEntry)
{
    ResetCharacter();
    TempScript stream(
        "{\n"
        " name = 'Test Dino'\n"
        " file = 'models/test.car'\n"
        " ai = 1\n"
        " health = 3\n"
        "}\n"
        "}\n");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(ReadCharacters(stream.stream));
    EXPECT_EQ(TotalC, 1);
    EXPECT_STREQ(DinoInfo[0].Name, "Test Dino");
    EXPECT_EQ(DinoInfo[0].Health0, 3);
}

TEST(DynamicRoster, ParsesBeyondOldAndPatchLimits)
{
    for (int count : {127, 128, 129, 257, 513}) {
        g_CharacterRoster.Reset();
        TotalC = 0;
        std::string text;
        for (int i = 0; i < count; ++i)
            text += "{\n name = 'Duplicate name'\n ai = 1\n}\n";
        text += "}\n";
        TempScript script(text.c_str());
        ASSERT_NO_THROW(ReadCharacters(script.stream));
        EXPECT_EQ(TotalC, count);
        EXPECT_EQ(DinoInfo.size(), count);
        EXPECT_EQ(DinoInfo[count - 1].Scale0, 800);
        EXPECT_EQ(DinoInfo[count - 1].ScaleA, 600);
        ASSERT_NO_THROW(g_CharacterRoster.Finalize());
        EXPECT_EQ(ChInfo.size(), count);
        auto* lastModel = &ChInfo[count - 1];
        EXPECT_THROW(g_CharacterRoster.BeginDefinition(count), std::logic_error);
        EXPECT_EQ(&ChInfo[count - 1], lastModel);
        EXPECT_THROW(DinoInfo[-1], std::out_of_range);
        EXPECT_THROW(DinoInfo[count], std::out_of_range);
    }
    ResetCharacter();
}

TEST(DynamicRoster, EmptyEntriesAndRepeatedSectionsKeepIds)
{
    g_CharacterRoster.Reset();
    TotalC = 0;
    TempScript first("{\n}\n}\n");
    ASSERT_NO_THROW(ReadCharacters(first.stream));
    TempScript second("{\n name = 'Second'\n}\n}\n");
    ASSERT_NO_THROW(ReadCharacters(second.stream));
    EXPECT_EQ(TotalC, 2);
    EXPECT_STREQ(DinoInfo[1].Name, "Second");
    ResetCharacter();
}

TEST(DynamicRoster, RejectsEmptyFinalRosterAndUnterminatedEntry)
{
    g_CharacterRoster.Reset();
    EXPECT_THROW(g_CharacterRoster.Finalize(), std::runtime_error);
    TotalC = 0;
    TempScript bad("{\n name = 'Unterminated'\n");
    EXPECT_THROW(ReadCharacters(bad.stream), std::runtime_error);
    ResetCharacter();
}

TEST(DynamicRoster, MetadataBudgetIsFailClosed)
{
    CharacterRoster roster;
    EXPECT_THROW({
        while (true) roster.BeginDefinition(roster.size());
    }, std::length_error);
    EXPECT_GT(roster.size(), 513);
    EXPECT_FALSE(roster.Frozen());
    EXPECT_EQ(roster.Definitions().size(), roster.Aggression().size());
    EXPECT_EQ(roster.Models().size(), 0);
}

TEST(DynamicRoster, SpawnLinksGrowAsPairedRecords)
{
    TSpawnGroup group{};
    for (int i = 0; i < 513; ++i) group.links.push_back({i, i % 32});
    ASSERT_EQ(group.links.size(), 513u);
    for (int i = 0; i < 513; ++i) {
        EXPECT_EQ(group.links[i].packType, i);
        EXPECT_EQ(group.links[i].spawnInfo, i % 32);
    }
    group = {};
    EXPECT_TRUE(group.links.empty());
}

TEST(DynamicRoster, TrophyMembershipGrowsWithoutChangingSaveLayout)
{
    ResetCharacter();
    trophyTypeCount = 1;
    trophyType[0] = {};
    trophyType[0].group = 42;
    for (int i = 0; i < 513; ++i) {
        g_CharacterRoster.BeginDefinition(i);
        TotalC = i;
        TempScript unused("");
        CallCharacterLine(unused.stream, "tropgroup = 42");
    }
    ASSERT_EQ(trophyType[0].ctype.size(), 513u);
    EXPECT_EQ(trophyType[0].ctype.back(), 512);
    EXPECT_EQ(sizeof(TTrophyRoom2), 7176u);
    trophyType[0] = {};
    trophyTypeCount = 0;
    ResetCharacter();
}

TEST(ScriptParserCommon, ReadBoolOverloadsParseTrueFalse)
{
    char trueText[] = "TRUE";
    char falseText[] = "FALSE";
    char otherText[] = "maybe";

    BOOL boolResult = false;
    readBool(trueText, boolResult);
    EXPECT_TRUE(boolResult);

    readBool(falseText, boolResult);
    EXPECT_FALSE(boolResult);

    bool cppResult = true;
    readBool(otherText, cppResult);
    EXPECT_TRUE(cppResult);  // unchanged when neither token is present

    readBool(falseText, cppResult);
    EXPECT_FALSE(cppResult);
}

TEST(ScriptParserCommon, SkipSectorConsumesNestedBlock)
{
    TempScript stream(" nested { x }\n}\nafter\n");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(SkipSector(stream.stream));
    char next[64];
    ASSERT_NE(fgets(next, sizeof next, stream.stream), nullptr);
    EXPECT_STREQ(next, "after\n");
}

TEST(ScriptParserCommon, SkipSectorHonoursSectionMarker)
{
    TempScript stream(" data\n//==== end of section\nignored\n");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_NO_THROW(SkipSector(stream.stream));
    char next[64];
    ASSERT_NE(fgets(next, sizeof next, stream.stream), nullptr);
    EXPECT_STREQ(next, "ignored\n");
}

TEST(ScriptParserCommon, SkipSectorHaltsOnUnterminatedBlock)
{
    TempScript stream(" no closing brace\n");
    ASSERT_NE(stream.stream, nullptr);

    EXPECT_THROW(SkipSector(stream.stream), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Duplicate-reader characterization for the parser dedup slice. These pin the
// overwrite/addition filter truth table and the behavior of the reader pairs
// that are folded into shared helpers.
// ---------------------------------------------------------------------------

TEST(ScriptParserOverwriteFilter, TrophyRoomAppliesEveryBlock)
{
    EXPECT_TRUE(OverwriteBlockApplies("overwrite area1", "huntdat/areas/trophy", 0, 0));
    EXPECT_TRUE(OverwriteBlockApplies("addition time2", "huntdat/areas/trophy", 2, 0));
}

TEST(ScriptParserOverwriteFilter, AreaQualifierMatchesSelectedArea)
{
    EXPECT_TRUE(OverwriteBlockApplies("overwrite area1", "huntdat/areas/area1", 0, 0));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite area2", "huntdat/areas/area1", 0, 0));
    EXPECT_TRUE(OverwriteBlockApplies("overwrite area2", "huntdat/areas/area2", 0, 0));
}

TEST(ScriptParserOverwriteFilter, Area10UsesTheArea0Token)
{
    // Legacy grammar: projectName[18] is the area digit; area10 has a
    // non-zero projectName[19] and matches the "area0" token, not "area10".
    EXPECT_TRUE(OverwriteBlockApplies("overwrite area0", "huntdat/areas/area10", 0, 0));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite area10", "huntdat/areas/area10", 0, 0));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite area0", "huntdat/areas/area1", 0, 0));
}

TEST(ScriptParserOverwriteFilter, TimeQualifierMatchesSelectedTime)
{
    EXPECT_TRUE(OverwriteBlockApplies("overwrite time0", "huntdat/areas/area1", 0, 0));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite time0", "huntdat/areas/area1", 1, 0));
    EXPECT_TRUE(OverwriteBlockApplies("overwrite time2", "huntdat/areas/area1", 2, 0));
}

TEST(ScriptParserOverwriteFilter, CharacterQualifierMatchesSelectionMask)
{
    EXPECT_TRUE(OverwriteBlockApplies("overwrite char0", "huntdat/areas/area1", 0, 1 << 10));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite char0", "huntdat/areas/area1", 0, 0));
    EXPECT_TRUE(OverwriteBlockApplies("overwrite char9", "huntdat/areas/area1", 0, 1 << 19));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite char9", "huntdat/areas/area1", 0, 1 << 18));
}

TEST(ScriptParserOverwriteFilter, UnqualifiedBlockAlwaysApplies)
{
    EXPECT_TRUE(OverwriteBlockApplies("overwrite", "huntdat/areas/area1", 1, 0));
    EXPECT_TRUE(OverwriteBlockApplies("addition", "huntdat/areas/area2", 2, 0));
}

TEST(ScriptParserOverwriteFilter, PresentQualifiersAreCombined)
{
    const char* project = "huntdat/areas/area3";
    EXPECT_TRUE(OverwriteBlockApplies("overwrite area3 time1 char2",
                                      project, 1, 1 << 12));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite area3 time1 char2",
                                       project, 0, 1 << 12));
    EXPECT_FALSE(OverwriteBlockApplies("overwrite area3 time1 char2",
                                       project, 1, 0));
}

TEST(ScriptParserReaders, RegionReadersPopulateAndCount)
{
    ResetSpawnGroup();
    TempScript region(" xmin = 10\n xmax = 20\n ymin = 30\n ymax = 40\n}\n");
    ASSERT_NE(region.stream, nullptr);
    EXPECT_NO_THROW(ReadRegion(region.stream));
    EXPECT_EQ(spawnGroup[0].spawnRegionCh, 1);
    EXPECT_EQ(spawnGroup[0].spawnRegion[0].XMin, 10);
    EXPECT_EQ(spawnGroup[0].spawnRegion[0].XMax, 20);
    EXPECT_EQ(spawnGroup[0].spawnRegion[0].YMin, 30);
    EXPECT_EQ(spawnGroup[0].spawnRegion[0].YMax, 40);

    ResetSpawnGroup();
    TempScript avoid(" xmin = 1\n xmax = 2\n ymin = 3\n ymax = 4\n}\n");
    ASSERT_NE(avoid.stream, nullptr);
    EXPECT_NO_THROW(ReadAvoid(avoid.stream));
    EXPECT_EQ(spawnGroup[0].avoidRegionCh, 1);
    EXPECT_EQ(spawnGroup[0].avoidRegion[0].XMin, 1);
    EXPECT_EQ(spawnGroup[0].avoidRegion[0].XMax, 2);
    EXPECT_EQ(spawnGroup[0].avoidRegion[0].YMin, 3);
    EXPECT_EQ(spawnGroup[0].avoidRegion[0].YMax, 4);
}

TEST(ScriptParserReaders, RegionReadersHaltOnMalformedLine)
{
    ResetSpawnGroup();
    TempScript region(" garbage\n}\n");
    ASSERT_NE(region.stream, nullptr);
    EXPECT_THROW(ReadRegion(region.stream), std::runtime_error);
}

TEST(ScriptParserReaders, SpawnInfoReadersPopulateAndCount)
{
    ResetCharacter();
    TempScript character(" spawnratio = 0.5\n spawngroup = 3\n}\n");
    ASSERT_NE(character.stream, nullptr);
    EXPECT_NO_THROW(ReadSpawnInfo(character.stream));
    EXPECT_EQ(DinoInfo[0].SpawnInfoCh, 1);
    EXPECT_FLOAT_EQ(DinoInfo[0].SpawnInfo[0].spawnRatio, 0.5f);
    EXPECT_EQ(DinoInfo[0].SpawnInfo[0].spawnGroup, 3);

    ResetPackType();
    TempScript pack(" spawnratio = 0.25\n spawngroup = 7\n}\n");
    ASSERT_NE(pack.stream, nullptr);
    EXPECT_NO_THROW(ReadSpawnInfoPack(pack.stream));
    EXPECT_EQ(packType[0].SpawnInfoCh, 1);
    EXPECT_FLOAT_EQ(packType[0].SpawnInfo[0].spawnRatio, 0.25f);
    EXPECT_EQ(packType[0].SpawnInfo[0].spawnGroup, 7);
}

TEST(ScriptParserReaders, SpawnInfoReadersHaltOnMalformedLine)
{
    ResetCharacter();
    TempScript character(" garbage\n}\n");
    ASSERT_NE(character.stream, nullptr);
    EXPECT_THROW(ReadSpawnInfo(character.stream), std::runtime_error);

    ResetPackType();
    TempScript pack(" garbage\n}\n");
    ASSERT_NE(pack.stream, nullptr);
    EXPECT_THROW(ReadSpawnInfoPack(pack.stream), std::runtime_error);
}

TEST(ScriptParserReaders, IdleGroupReadersPopulateAndCount)
{
    ResetCharacter();
    TempScript idle(
        " startChance = 0.25\n"
        " endChance = 0.75\n"
        " randomStarting = TRUE\n"
        " randomEnding = FALSE\n"
        " instantRepeat = TRUE\n"
        " anim = 5\n"
        "}\n");
    ASSERT_NE(idle.stream, nullptr);
    EXPECT_NO_THROW(ReadIdleGroupInfo(idle.stream));
    EXPECT_EQ(DinoInfo[0].idleGroupCount, 1);
    EXPECT_FLOAT_EQ(DinoInfo[0].idleGroup[0].start, 0.25f);
    EXPECT_FLOAT_EQ(DinoInfo[0].idleGroup[0].end, 0.75f);
    EXPECT_TRUE(DinoInfo[0].idleGroup[0].startOnAny);
    EXPECT_FALSE(DinoInfo[0].idleGroup[0].endOnAny);
    EXPECT_TRUE(DinoInfo[0].idleGroup[0].instantRepeat);
    EXPECT_EQ(DinoInfo[0].idleGroup[0].count, 1);
    EXPECT_EQ(DinoInfo[0].idleGroup[0].anim[0], 5);

    ResetCharacter();
    TempScript idle2(" startChance = 0.1\n anim = 9\n}\n");
    ASSERT_NE(idle2.stream, nullptr);
    EXPECT_NO_THROW(ReadIdle2GroupInfo(idle2.stream));
    EXPECT_EQ(DinoInfo[0].idle2GroupCount, 1);
    EXPECT_FLOAT_EQ(DinoInfo[0].idle2Group[0].start, 0.1f);
    EXPECT_EQ(DinoInfo[0].idle2Group[0].anim[0], 9);
}

TEST(ScriptParserReaders, IdleGroupReadersHaltOnMalformedLine)
{
    ResetCharacter();
    TempScript idle(" garbage\n}\n");
    ASSERT_NE(idle.stream, nullptr);
    EXPECT_THROW(ReadIdleGroupInfo(idle.stream), std::runtime_error);
}

TEST(ScriptParserReaders, WipeHelpersClearAndResetCounts)
{
    ResetCharacter();
    DinoInfo[0].killTypeCount = 2;
    DinoInfo[0].killType[0].hunteranim = 7;
    DinoInfo[0].killType[1].offset = 3;

    EXPECT_NO_THROW(WipeKillTypes());
    EXPECT_EQ(DinoInfo[0].killTypeCount, 0);
    EXPECT_EQ(DinoInfo[0].killType[0].hunteranim, 0);
    EXPECT_EQ(DinoInfo[0].killType[1].offset, 0);
}
