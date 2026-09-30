// test_menu_resource_entry.cpp -- production menu _RES.TXT/_MENU.TXT reader contracts
//
// The menu prefers HUNTDAT/_MENU.TXT and falls back to the legacy _RES.TXT.
// These tests drive the real ReadCharacters()/ReadWeapons()/ReadPrices()
// readers with the line shapes the shipped files use, so a field that is
// strict-parsed can never be reached by a line whose assignment key is
// something else (the stock `file = 'models/main_hunt/...'` line and a
// decimal `health = 13.5` are both represented here).

#include <gtest/gtest.h>

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Hunt.h"
#include "Loaders/LoadDiagnostics.h"

// Production entry points defined in Menu/Resources.cpp.
void ReadCharacters(FILE* stream);
void ReadWeapons(FILE* stream);
void ReadPrices(FILE* stream);
void ReadAccessoryIcons(FILE* stream);
void ReadAccessoryDescriptions(FILE* stream);
bool LoadText(std::vector<std::string>& txt, const std::string& path);

namespace {

class TempScript final
{
public:
    explicit TempScript(const char* text)
        : stream(tmpfile())
    {
        if (stream) {
            fputs(text, stream);
            rewind(stream);
        }
    }

    ~TempScript()
    {
        if (stream)
            fclose(stream);
    }

    FILE* stream;
};

void ResetMenuState()
{
    g_DinoInfo.clear();
    g_WeapInfo.clear();
    g_AreaInfo.clear();
    g_UtilInfo.clear();
    g_AccessoryPrices.clear();
    g_AccessoryScoreMods.clear();
    g_AccessoryIcons.clear();
    g_AccessoryDescriptions.clear();
    g_StartCredits = 0;
}

}  // namespace

TEST(MenuResourceEntry, LegacyResCharactersBlockParsesStockLineShapes)
{
    ResetMenuState();
    // ReadCharacters is called after the caller consumed the `characters {`
    // line, so the stream starts with the first dino block opener.
    TempScript script(
        "{\n"
        " name    = 'Massospondylus'\n"
        " file    = 'models/main_hunt/para.car'\n"
        " ai      = 10\n"
        " health  = 13.5\n"
        " basescore = 9\n"
        " smellK  = 0.8\n"
        " hearK   = 0.75\n"
        " lookK   = 0.5\n"
        " scale0  = 980\n"
        "}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    // The name and path values contain numeric field keys as substrings
    // ("Massospondylus" -> mass, "main_hunt" -> ai); health is a decimal
    // literal. None of them may be read as a numeric field.
    EXPECT_NO_THROW(ReadCharacters(script.stream));

    ASSERT_EQ(g_DinoInfo.size(), 1u);
    const DinoInfo& dino = g_DinoInfo[0];
    EXPECT_EQ(dino.m_Name, "Massospondylus");
    EXPECT_EQ(dino.m_FilePath, "models/main_hunt/para.car");
    EXPECT_EQ(dino.m_AI, 10);
    EXPECT_EQ(dino.m_BaseHealth, 13);  // legacy atoi truncation of 13.5
    EXPECT_EQ(dino.m_BaseScore, 9);
    EXPECT_FLOAT_EQ(dino.m_SmellK, 0.8f);
    EXPECT_FLOAT_EQ(dino.m_HearK, 0.75f);
    EXPECT_FLOAT_EQ(dino.m_LookK, 0.5f);
    EXPECT_EQ(dino.m_BaseScale, 980);
}

TEST(MenuResourceEntry, MenuTxtAcceptsBothSensitivityKeySpellings)
{
    ResetMenuState();
    // _MENU.TXT uses the bare keys plus capital-K variants (Velociraptor),
    // while _RES.TXT uses only the capital-K variants.
    TempScript script(
        "{\n"
        " name = 'Velociraptor'\n"
        " ai = 15\n"
        " smell = 1.0\n"
        " hearK = 0.7\n"
        " lookK = 0.5\n"
        "}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadCharacters(script.stream));

    ASSERT_EQ(g_DinoInfo.size(), 1u);
    const DinoInfo& dino = g_DinoInfo[0];
    EXPECT_EQ(dino.m_AI, 15);
    EXPECT_FLOAT_EQ(dino.m_SmellK, 1.0f);
    EXPECT_FLOAT_EQ(dino.m_HearK, 0.7f);
    EXPECT_FLOAT_EQ(dino.m_LookK, 0.5f);
}

TEST(MenuResourceEntry, LegacyHealthAcceptsLongSuffix)
{
    ResetMenuState();
    TempScript script(
        "{\n"
        " name = 'Dsungaripterus'\n"
        " ai = 3\n"
        " health = 1l\n"
        "}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadCharacters(script.stream));
    EXPECT_TRUE(g_DinoInfo.empty());  // Ambient AI is parsed but not listed.
}

TEST(MenuResourceEntry, WeaponValuesDoNotSelectOtherFields)
{
    ResetMenuState();
    TempScript script(
        "{\n"
        " name = 'Pirate Rifle'\n"
        " power = 5\n"
        " prec = 1.8\n"
        " loud = 1.2\n"
        " rate = 0.45\n"
        " file = 'models/pirateship/rifle_fall.car'\n"
        " pic1 = 'ammo/bullet1.tga'\n"
        "}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    // 'Pirate' contains "rate" and the path contains "fall"; both keys are
    // strict-parsed fields.
    EXPECT_NO_THROW(ReadWeapons(script.stream));

    ASSERT_EQ(g_WeapInfo.size(), 1u);
    const WeapInfo& weapon = g_WeapInfo[0];
    EXPECT_EQ(weapon.m_Name, "Pirate Rifle");
    EXPECT_FLOAT_EQ(weapon.m_Power, 5.0f);
    EXPECT_FLOAT_EQ(weapon.m_Rate, 0.45f);
    EXPECT_EQ(weapon.m_FilePath, "models/pirateship/rifle_fall.car");
    EXPECT_EQ(weapon.m_BulletFilePath, "ammo/bullet1.tga");
}

TEST(MenuResourceEntry, PricesBlockKeepsLegacyKeys)
{
    ResetMenuState();
    TempScript script(
        "prices {\n"
        " start = 100\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadPrices(script.stream));
    EXPECT_EQ(g_StartCredits, 100u);
}

TEST(MenuResourceEntry, PricesWithinCapacityAreAssigned)
{
    ResetMenuState();
    g_DinoInfo.resize(2);
    g_WeapInfo.resize(1);
    g_DinoInfo[0].m_AI = 10;  // CurD starts at the first huntable index
    TempScript script(
        "prices {\n"
        " dino = 10\n"
        " dino = 30\n"
        " weapon = 20\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadPrices(script.stream));
    EXPECT_EQ(g_DinoInfo[0].m_Price, 10);
    EXPECT_EQ(g_DinoInfo[1].m_Price, 30);
    EXPECT_EQ(g_WeapInfo[0].m_Price, 20);
}

TEST(MenuResourceEntry, ExcessPriceEntriesAreRejected)
{
    ResetMenuState();
    g_DinoInfo.resize(1);
    TempScript script(
        "prices {\n"
        " dino = 10\n"
        " dino = 30\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    // The prices block lists one entry per roster item; a malformed script with
    // more entries than loaded dinos must fail instead of indexing past the
    // vector.
    EXPECT_THROW(ReadPrices(script.stream), std::exception);
}

TEST(MenuResourceEntry, ExcessWeaponPricesAreRejected)
{
    ResetMenuState();
    g_WeapInfo.resize(1);
    TempScript script(
        "prices {\n"
        " weapon = 10\n"
        " weapon = 30\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_THROW(ReadPrices(script.stream), std::exception);
}

TEST(MenuResourceEntry, MalformedNumericValuesFailInStrictMode)
{
    ResetMenuState();
    LoadDiagnostics::Instance().Clear();
    LoadDiagnostics::Instance().SetMode(LoadMode::Strict);
    TempScript script(
        "{\n"
        " ai = ten\n"
        "}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    // Exact key matching must not weaken the value validation itself: strict
    // mode (CI / mod authoring) still stops at the first bad value.
    EXPECT_THROW(ReadCharacters(script.stream), std::exception);

    LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
    LoadDiagnostics::Instance().Clear();
}

TEST(MenuResourceEntry, MalformedNumericValuesRecoverInLenientMode)
{
    ResetMenuState();
    LoadDiagnostics::Instance().Clear();
    TempScript script(
        "{\n"
        " ai = ten\n"
        "}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    // The player default accepts the file, falls back to 0 for the bad value
    // and records a diagnostic instead of aborting the menu.
    EXPECT_NO_THROW(ReadCharacters(script.stream));
    EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);
    LoadDiagnostics::Instance().Clear();
}

TEST(MenuResourceEntry, HuntableThumbnailFollowsListPositionNotAiSlot)
{
    ResetMenuState();
    // The stock _MENU.TXT roster: ten huntables whose AI values are not
    // unique (Iguanodon and Carnotaurus both use 17, T-Rex uses 18). Each
    // entry must resolve to its own numbered picture, not its predecessor's.
    TempScript script(
        "{\n name = 'Parasaurolophus'\n ai = 10\n}\n"
        "{\n name = 'Pachycephalosaurus'\n ai = 11\n}\n"
        "{\n name = 'Stegosaurus'\n ai = 12\n}\n"
        "{\n name = 'Allosaurus'\n ai = 13\n}\n"
        "{\n name = 'Triceratops'\n ai = 14\n}\n"
        "{\n name = 'Velociraptor'\n ai = 15\n}\n"
        "{\n name = 'Dilophosaurus'\n ai = 16\n}\n"
        "{\n name = 'Iguanodon'\n ai = 17\n}\n"
        "{\n name = 'Carnotaurus'\n ai = 17\n}\n"
        "{\n name = 'Tyrannosaurus Rex'\n ai = 18\n}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadCharacters(script.stream));

    ASSERT_EQ(g_DinoInfo.size(), 10u);
    EXPECT_EQ(g_DinoInfo[7].m_PicturePath, "huntdat/menu/pics/dino8.tga");
    EXPECT_EQ(g_DinoInfo[8].m_PicturePath, "huntdat/menu/pics/dino9.tga");
    EXPECT_EQ(g_DinoInfo[9].m_PicturePath, "huntdat/menu/pics/dino10.tga");
}

TEST(MenuResourceEntry, AccessoryDescriptionPrefersTheShippedFile)
{
    // The rule every optional row goes through: a .nfo in the user's HUNTDAT
    // replaces the built-in blurb outright, so a mod can reword the row.
    const char* path = "node_such_dir_but_exists.nfo";
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out << "Mod-supplied blurb.\r\n";
    out.close();

    const std::vector<std::string> builtin{ "built-in blurb" };
    std::vector<std::string> lines;
    LoadAccessoryDescription(lines, path, &builtin);
    std::remove(path);

    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "Mod-supplied blurb.");
}

TEST(MenuResourceEntry, AccessoryDescriptionFallsBackOnlyWhenTheFileIsMissing)
{
    const std::vector<std::string> builtin{ "built-in blurb", "second line" };
    std::vector<std::string> lines;
    LoadAccessoryDescription(lines, "huntdat/menu/txt/no_such_accessory.nfo", &builtin);
    EXPECT_EQ(lines, builtin);

    std::vector<std::string> noFallback;
    LoadAccessoryDescription(noFallback, "huntdat/menu/txt/no_such_accessory.nfo", nullptr);
    EXPECT_TRUE(noFallback.empty());
}

TEST(MenuResourceEntry, ExistingDescriptionFileSuppliesTheLines)
{
    // The other half of the same branch: when the mod does ship a .nfo its
    // text is what reaches the panel, overriding nothing but replacing the
    // built-in blurb entirely.
    const char* path = "node_such_dir_but_exists.nfo";
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out << "Accessory : Something Else\r\n"
        << "\r\n"
        << "Mod-supplied text.\r\n";
    out.close();

    std::vector<std::string> lines;
    EXPECT_TRUE(LoadText(lines, path));
    std::remove(path);

    // CRLF terminators are stripped by the text-mode stream, so no line may
    // carry a trailing '\r' into TextOut (which would draw a junk glyph).
    ASSERT_GE(lines.size(), 2u);
    EXPECT_EQ(lines[0], "Accessory : Something Else");
    for (const std::string& line : lines) {
        EXPECT_EQ(line.find('\r'), std::string::npos);
    }
}

TEST(MenuResourceEntry, MissingDescriptionFileIsReportedAndAppendsNothing)
{
    // Every accessory blurb comes from HUNTDAT via LoadText(); modernization
    // accessories rely on its false branch to fall back to a built-in
    // description, so that path has to stay a real, detectable failure.
    std::vector<std::string> lines{ "untouched" };
    EXPECT_FALSE(LoadText(lines, "huntdat/menu/txt/no_such_accessory.nfo"));
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "untouched");
}

TEST(MenuResourceEntry, BuiltInFlashlightBlurbFitsTheShopPanel)
{
    // The accessory panel draws each line unwrapped at x=424 in the 800-wide
    // menu buffer with a ~7px-per-character font, so roughly 48 characters is
    // the safe ceiling -- the widest line in any stock .nfo is 48. Nothing in
    // the draw path measures or wraps, so an over-long line silently runs off
    // the panel edge instead of failing.
    ASSERT_FALSE(kFlashlightDescription.empty());
    EXPECT_EQ(kFlashlightDescription.front(), "Accessory : Flashlight");
    for (const std::string& line : kFlashlightDescription) {
        EXPECT_LE(line.size(), 48u) << "shop panel would clip: " << line;
    }
    // It must also state the two things the row otherwise leaves unknown: how
    // the beam is switched on, and that the accessory is scoring-neutral.
    const std::string joined = [] {
        std::string out;
        for (const std::string& line : kFlashlightDescription) { out += line; out += '\n'; }
        return out;
    }();
    EXPECT_NE(joined.find("default: F"), std::string::npos);
    EXPECT_NE(joined.find("score"), std::string::npos);
}

TEST(MenuResourceEntry, ExplicitPicturePathOverridesTheDefaultThumbnail)
{
    ResetMenuState();
    TempScript script(
        "{\n"
        " name = 'Custom'\n"
        " ai = 10\n"
        " pic = 'huntdat/menu/pics/custom.tga'\n"
        "}\n"
        "}\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadCharacters(script.stream));

    ASSERT_EQ(g_DinoInfo.size(), 1u);
    EXPECT_EQ(g_DinoInfo[0].m_PicturePath, "huntdat/menu/pics/custom.tga");
}

TEST(MenuResourceEntry, AccessoryIconBlockNormalizesKeysAndStripsQuotes)
{
    // The block accepts the score key, the display name or any spelling that
    // differs only in case/spacing, and the same quoted 'path' form every
    // other script text value uses. The first implementation stored keys
    // verbatim and kept the quote characters inside the path, so both the
    // documented example keys and the documented example values broke.
    ResetMenuState();
    TempScript script(
        "Flashlight = 'huntdat/menu/pics/mod_fl.tga'\n"
        "'Cover scent' = huntdat/menu/pics/mod_scent.tga\n"
        "Night_Vision = 'huntdat/menu/pics/mod_nv.tga'\n"
        "# comment = 'not an entry'\n"
        "emptyvalue =\n"
        ".\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadAccessoryIcons(script.stream));

    EXPECT_EQ(g_AccessoryIcons.size(), 3u);
    EXPECT_EQ(g_AccessoryIcons["flashlight"], "huntdat/menu/pics/mod_fl.tga");
    EXPECT_EQ(g_AccessoryIcons["coverscent"], "huntdat/menu/pics/mod_scent.tga");
    EXPECT_EQ(g_AccessoryIcons["nightvision"], "huntdat/menu/pics/mod_nv.tga");
}

TEST(MenuResourceEntry, AccessoryDescriptionBlockMirrorsTheIconBlock)
{
    // Same reader, same grammar, second map. The score key and the display
    // name stay two separate stored keys ('tranq' and 'tranquilizers'); the
    // row's probe order decides which one wins -- the score key first.
    ResetMenuState();
    TempScript script(
        "tranq = 'huntdat/menu/txt/mod_tranq.nfo'\n"
        "Tranquilizers = huntdat/menu/txt/mod_tranq2.nfo\n"
        ".\n");
    ASSERT_NE(script.stream, nullptr);

    EXPECT_NO_THROW(ReadAccessoryDescriptions(script.stream));

    EXPECT_EQ(g_AccessoryDescriptions.size(), 2u);
    EXPECT_EQ(g_AccessoryDescriptions["tranq"], "huntdat/menu/txt/mod_tranq.nfo");
    EXPECT_EQ(g_AccessoryDescriptions["tranquilizers"], "huntdat/menu/txt/mod_tranq2.nfo");
    // A text override must point at an existing file (the policy tests below
    // cover both sides); these paths do not exist, so the row would keep its
    // default .nfo. The icon block carries no such requirement.
}

TEST(MenuResourceEntry, IconProbeOrderIsScoreKeyThenDisplayNameThenLegacy)
{
    // When a mod fills several spellings for the same row, the probe order
    // decides: score key first, then the display name, then the legacy
    // command-derived key.
    ResetMenuState();
    TempScript script(
        "tranq = 'm/score_key.tga'\n"
        "Tranquilizers = 'm/display_name.tga'\n"
        "tranq -tranquilizer = 'm/legacy.tga'\n"
        ".\n");
    ASSERT_NE(script.stream, nullptr);
    EXPECT_NO_THROW(ReadAccessoryIcons(script.stream));

    EXPECT_EQ(LookupAccessoryIcon("tranq", "Tranquilizers", "-tranq -tranquilizer", "default.tga"),
              "m/score_key.tga");

    // Removing the winner promotes the display name, then the legacy key.
    g_AccessoryIcons.erase("tranq");
    EXPECT_EQ(LookupAccessoryIcon("tranq", "Tranquilizers", "-tranq -tranquilizer", "default.tga"),
              "m/display_name.tga");

    g_AccessoryIcons.erase("tranquilizers");
    EXPECT_EQ(LookupAccessoryIcon("tranq", "Tranquilizers", "-tranq -tranquilizer", "default.tga"),
              "m/legacy.tga");
}

TEST(MenuResourceEntry, IconLookupAcceptsEveryKeySpelling)
{
    // One row, every accepted spelling: 'tranq' is the canonical score key,
    // 'Tranquilizers' the display name, and 'tranq -tranquilizer' the legacy
    // compound key the first implementation accidentally required.
    for (const char* key : { "tranq", "Tranquilizers", "tranq -tranquilizer" }) {
        ResetMenuState();
        TempScript script((std::string(key) + " = 'm/mod_tranq.tga'\n.\n").c_str());
        ASSERT_NE(script.stream, nullptr);
        EXPECT_NO_THROW(ReadAccessoryIcons(script.stream));

        EXPECT_EQ(LookupAccessoryIcon("tranq", "Tranquilizers", "-tranq -tranquilizer",
                                      "huntdat/menu/pics/equip6.tga"),
                  "m/mod_tranq.tga") << key;
    }
}

TEST(MenuResourceEntry, IconLookupFallsBackToTheBuiltInPathWithoutOverrides)
{
    ResetMenuState();
    EXPECT_EQ(LookupAccessoryIcon("tranq", "Tranquilizers", "-tranq -tranquilizer",
                                  "huntdat/menu/pics/equip6.tga"),
              "huntdat/menu/pics/equip6.tga");
}

TEST(MenuResourceEntry, EveryAccessoryRowIsIconAddressable)
{
    // The seven shop rows under their score keys. Cover scent (empty flag,
    // so the first implementation derived an empty key) and night vision
    // (whose icon load bypassed the lookup entirely) were unaddressable
    // before the normalized keys.
    ResetMenuState();
    TempScript script(
        "camo = 'm/camo.tga'\n"
        "radar = 'm/radar.tga'\n"
        "scent = 'm/scent.tga'\n"
        "double = 'm/double.tga'\n"
        "nightvision = 'm/nv.tga'\n"
        "tranq = 'm/tranq.tga'\n"
        "flashlight = 'm/fl.tga'\n"
        ".\n");
    ASSERT_NE(script.stream, nullptr);
    EXPECT_NO_THROW(ReadAccessoryIcons(script.stream));

    struct Row { const char* scoreKey; const char* name; const char* cmd; const char* expect; };
    const Row rows[] = {
        { "camo", "Camouflage", "-camo", "m/camo.tga" },
        { "radar", "Radar", "-radar", "m/radar.tga" },
        { "scent", "Cover scent", "", "m/scent.tga" },
        { "double", "Double ammo", "-double", "m/double.tga" },
        { "nightvision", "Night vision", "-nightvision", "m/nv.tga" },
        { "tranq", "Tranquilizers", "-tranq -tranquilizer", "m/tranq.tga" },
        { "flashlight", "Flashlight", "-flashlight", "m/fl.tga" },
    };
    for (const Row& row : rows) {
        EXPECT_EQ(LookupAccessoryIcon(row.scoreKey, row.name, row.cmd, "default.tga"),
                  row.expect) << row.name;
    }
}

TEST(MenuResourceEntry, IconLookupForgivesCaseAndSpacing)
{
    for (const char* spelling : { "Cover scent", "cover  scent", "COVER-SCENT", "cover_scent" }) {
        ResetMenuState();
        TempScript script((std::string(spelling) + " = 'm/scent.tga'\n.\n").c_str());
        ASSERT_NE(script.stream, nullptr);
        EXPECT_NO_THROW(ReadAccessoryIcons(script.stream));

        EXPECT_EQ(LookupAccessoryIcon("scent", "Cover scent", "", "default.tga"),
                  "m/scent.tga") << spelling;
    }
}

TEST(MenuResourceEntry, MissingIconOverrideKeepsTheOverrideAndReports)
{
    // Unchanged original behavior: an override wins even when its file is
    // missing, so the row shows the empty icon box -- but the failure now
    // reaches the load summary instead of dying silently.
    ResetMenuState();
    LoadDiagnostics::Instance().Clear();
    TempScript script("camo = 'no/such/mod_icon.tga'\n.\n");
    ASSERT_NE(script.stream, nullptr);
    EXPECT_NO_THROW(ReadAccessoryIcons(script.stream));

    EXPECT_EQ(LookupAccessoryIcon("camo", "Camouflage", "-camo", "huntdat/menu/pics/equip1.tga"),
              "no/such/mod_icon.tga");
    EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);
    LoadDiagnostics::Instance().Clear();
}

TEST(MenuResourceEntry, MissingTextOverrideFallsBackAndReports)
{
    // A text override must point at an existing file: a missing one is
    // reported and the row keeps its default .nfo. Built-in text covers
    // modernization rows when that default file is also absent.
    ResetMenuState();
    LoadDiagnostics::Instance().Clear();
    TempScript script("flashlight = 'no/such/mod_text.nfo'\n.\n");
    ASSERT_NE(script.stream, nullptr);
    EXPECT_NO_THROW(ReadAccessoryDescriptions(script.stream));

    EXPECT_EQ(LookupAccessoryText("flashlight", "Flashlight", "-flashlight",
                                  "huntdat/menu/txt/flashlight.nfo"),
              "huntdat/menu/txt/flashlight.nfo");
    EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);
    LoadDiagnostics::Instance().Clear();
}

TEST(MenuResourceEntry, TruncatedTgaIsRejectedBeforeReadingPixels)
{
    LoadDiagnostics::Instance().Clear();
    const char* path = "mod_truncated_icon.tga";
    const unsigned char header[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 0, 16, 0
    };
    for (size_t length : {size_t(9), sizeof(header)}) {
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out.write(reinterpret_cast<const char*>(header), length);
        out.close();
        Picture pic;
        EXPECT_FALSE(LoadPicture(pic, path));
        EXPECT_FALSE(pic.IsValid());
    }
    std::remove(path);
    EXPECT_EQ(LoadDiagnostics::Instance().Count(), 2u);
    LoadDiagnostics::Instance().Clear();
}

TEST(MenuResourceEntry, MissingIconNeverInheritsThePreviousRowsPicture)
{
    ResetMenuState();
    LoadDiagnostics::Instance().Clear();
    // Tiny but valid bottom-origin, 16-bit TGA for the night-vision row.
    // The next two rows intentionally have no art, like the deployed install.
    const char* path = "mod_override_icon.tga";
    const unsigned char tga[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 0, 16, 0, 0xff, 0xff
    };
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(tga), sizeof(tga));
    out.close();
    g_AccessoryIcons["nightvision"] = path;
    g_AccessoryIcons["tranq"] = "no/such/tranq.tga";
    g_AccessoryIcons["flashlight"] = "no/such/flashlight.tga";
    // A directory exists but cannot be read as an .nfo; use the built-in
    // blurb rather than leaving the row blank.
    g_AccessoryDescriptions["flashlight"] = ".";

    std::ostringstream log;
    std::streambuf* original = std::cout.rdbuf(log.rdbuf());
    EXPECT_NO_THROW(LoadResources());
    std::cout.rdbuf(original);
    std::remove(path);

    ASSERT_EQ(g_UtilInfo.size(), 7u);
    EXPECT_TRUE(g_UtilInfo[4].m_Thumbnail.IsValid());
    EXPECT_FALSE(g_UtilInfo[5].m_Thumbnail.IsValid());
    EXPECT_FALSE(g_UtilInfo[6].m_Thumbnail.IsValid());
    EXPECT_EQ(g_UtilInfo[6].m_Description, kFlashlightDescription);
    EXPECT_NE(log.str().find("Load diagnostics:"), std::string::npos);
    EXPECT_NE(log.str().find("override file could not be read"), std::string::npos);
    EXPECT_NE(log.str().find("no/such/tranq.tga"), std::string::npos);
    EXPECT_EQ(LoadDiagnostics::Instance().Count(), 0u);
}

TEST(MenuResourceEntry, TextOverrideRedirectsTheDescriptionFile)
{
    // A mod's redirect wins over both the stock .nfo path and the built-in
    // blurb when the file it names exists.
    ResetMenuState();
    const char* path = "mod_override_desc.nfo";
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out << "Mod-supplied flashlight blurb.\r\n";
    out.close();

    TempScript script((std::string("flashlight = '") + path + "'\n.\n").c_str());
    ASSERT_NE(script.stream, nullptr);
    EXPECT_NO_THROW(ReadAccessoryDescriptions(script.stream));

    std::vector<std::string> lines;
    LoadAccessoryDescription(lines,
                              LookupAccessoryText("flashlight", "Flashlight", "-flashlight",
                                                  "huntdat/menu/txt/flashlight.nfo"),
                              &kFlashlightDescription);
    std::remove(path);

    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "Mod-supplied flashlight blurb.");
}
