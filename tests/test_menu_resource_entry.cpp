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
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Hunt.h"
#include "Loaders/LoadDiagnostics.h"
#include "test_support.h"

// Production entry points defined in Menu/ScriptParse.cpp.
void ReadCharacters(FILE* stream);
void ReadWeapons(FILE* stream);
void ReadPrices(FILE* stream);
void ReadAccessoryIcons(FILE* stream);
void ReadAccessoryDescriptions(FILE* stream);
void ReadAccessoryPrices(FILE* stream);
bool LoadText(std::vector<std::string>& txt, const std::string& path);

namespace {

class TempMenuDirectory final
{
public:
    TempMenuDirectory()
        : previous(std::filesystem::current_path()),
          root(std::filesystem::temp_directory_path() /
               ("c2-shop-script-" + std::to_string(GetCurrentProcessId()) + "-" +
                std::to_string(GetTickCount64())))
    {
        std::filesystem::create_directories(root / "huntdat");
        std::filesystem::current_path(root);
    }

    ~TempMenuDirectory()
    {
        std::filesystem::current_path(previous);
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

private:
    std::filesystem::path previous;
    std::filesystem::path root;
};

using test_support::TempScript;

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
    g_AccessoryPriceOverrides.clear();
    g_StartCredits = 0;
}

}  // namespace

TEST(MenuResourceEntry, CharacterBlockParsesStockLineShapes)
{
    // ReadCharacters is called after the caller consumed the `characters {`
    // line, so the stream starts with the first dino block opener.
    {
        ResetMenuState();
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

    // _MENU.TXT uses the bare keys plus capital-K variants (Velociraptor),
    // while _RES.TXT uses only the capital-K variants.
    {
        ResetMenuState();
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

TEST(MenuResourceEntry, HuntableThumbnailsFollowTheRoster)
{
    // The stock _MENU.TXT roster: ten huntables whose AI values are not
    // unique (Iguanodon and Carnotaurus both use 17, T-Rex uses 18). Each
    // entry must resolve to its own numbered picture, not its predecessor's.
    {
        ResetMenuState();
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
}

TEST(MenuResourceEntry, PricesBlockAssignsLegacyAndKeyedEntries)
{
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
}

TEST(MenuResourceEntry, ExcessPriceEntriesAreRejected)
{
    // The prices block lists one entry per roster item; a malformed script
    // with more entries than loaded items must fail instead of indexing past
    // the vector.
    {
        ResetMenuState();
        g_DinoInfo.resize(1);
        TempScript script(
            "prices {\n"
            " dino = 10\n"
            " dino = 30\n"
            "}\n");
        ASSERT_NE(script.stream, nullptr);

        EXPECT_THROW(ReadPrices(script.stream), std::exception);
    }

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
}

TEST(MenuResourceEntry, MalformedNumericValuesRecoverOrHaltByMode)
{
    // Exact key matching must not weaken the value validation itself: strict
    // mode (CI / mod authoring) still stops at the first bad value.
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

        EXPECT_THROW(ReadCharacters(script.stream), std::exception);

        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
        LoadDiagnostics::Instance().Clear();
    }

    // The player default accepts the file, falls back to 0 for the bad value
    // and records a diagnostic instead of aborting the menu.
    {
        ResetMenuState();
        LoadDiagnostics::Instance().Clear();
        TempScript script(
            "{\n"
            " ai = ten\n"
            "}\n"
            "}\n");
        ASSERT_NE(script.stream, nullptr);

        EXPECT_NO_THROW(ReadCharacters(script.stream));
        EXPECT_GE(LoadDiagnostics::Instance().Count(), 1u);
        LoadDiagnostics::Instance().Clear();
    }
}

TEST(MenuResourceEntry, OverrideDescriptionFilesSupplyTheLines)
{
    // The rule every optional row goes through: a .nfo in the user's HUNTDAT
    // replaces the built-in blurb outright, so a mod can reword the row.
    {
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

    // The other half of the same branch: when the mod does ship a .nfo its
    // text is what reaches the panel, overriding nothing but replacing the
    // built-in blurb entirely.
    {
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

    // A mod's redirect wins over both the stock .nfo path and the built-in
    // blurb when the file it names exists.
    {
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
}

TEST(MenuResourceEntry, MissingOrEmptyDescriptionFilesFallBack)
{
    {
        const std::vector<std::string> builtin{ "built-in blurb", "second line" };
        std::vector<std::string> lines;
        LoadAccessoryDescription(lines, "huntdat/menu/txt/no_such_accessory.nfo", &builtin);
        EXPECT_EQ(lines, builtin);

        std::vector<std::string> noFallback;
        LoadAccessoryDescription(noFallback, "huntdat/menu/txt/no_such_accessory.nfo", nullptr);
        EXPECT_TRUE(noFallback.empty());
    }

    // Every accessory blurb comes from HUNTDAT via LoadText(); modernization
    // accessories rely on its false branch to fall back to a built-in
    // description, so that path has to stay a real, detectable failure.
    {
        std::vector<std::string> lines{ "untouched" };
        EXPECT_FALSE(LoadText(lines, "huntdat/menu/txt/no_such_accessory.nfo"));
        ASSERT_EQ(lines.size(), 1u);
        EXPECT_EQ(lines[0], "untouched");
    }

    {
        TempMenuDirectory fixture;
        std::ofstream("empty.nfo");
        std::vector<std::string> lines;
        ASSERT_TRUE(LoadText(lines, "empty.nfo"));
        EXPECT_TRUE(lines.empty());
    }

    // A text override must point at an existing file: a missing one is
    // reported and the row keeps its default .nfo. Built-in text covers
    // modernization rows when that default file is also absent.
    {
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

TEST(MenuResourceEntry, ShopScanSkipsCommentsAndNeedsBlockDots)
{
    {
        ResetMenuState();
        TempMenuDirectory fixture;
        std::ofstream menu("huntdat/_menu.txt");
        menu << "common {\n.\n}\n"
                "// accessory_descriptions is only a comment\n"
                "  # accessory_prices = mentioned in a comment\n"
                "  ; accessory_icons is also a comment\n"
                "not_accessory_prices\n"
                "accessory_icons\n{\nflashlight = 'lamp#1.tga' # actual comment\n}\n.\n"
                "accessory_prices\n{\n; flashlight = 900\n// radar = 800\nflashlight = -0\n}\n.\n.\n";
        menu.close();
        ASSERT_NO_THROW(LoadResourcesScript());
        EXPECT_EQ(g_AccessoryIcons["flashlight"], "lamp#1.tga");
        EXPECT_TRUE(g_AccessoryDescriptions.empty());
        ASSERT_EQ(g_AccessoryPriceOverrides.size(), 1u);
        EXPECT_EQ(g_AccessoryPriceOverrides["flashlight"], 0);
    }

    // The shipped accessories block ends at its first dot, not its '}'; a
    // missing score-block dot swallows the next section. Legacy behavior is
    // deliberately preserved; the checker detects this bad insertion and
    // directs authors to add the missing dot.
    {
        ResetMenuState();
        LoadDiagnostics::Instance().Clear();
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
        TempMenuDirectory fixture;
        std::ofstream menu("huntdat/_menu.txt");
        ASSERT_TRUE(menu.is_open());
        menu << "accessories\n{\n camo = 0.85\n}\n"
                "accessory_icons\n{\n scent = 'mod_scent.tga'\n}\n.\n.\n";
        menu.close();
        EXPECT_NO_THROW(LoadResourcesScript());
        EXPECT_TRUE(g_AccessoryIcons.empty());
        EXPECT_EQ(g_AccessoryScoreMods.count("scent"), 1u);
        LoadDiagnostics::Instance().Clear();
    }
}

TEST(MenuResourceEntry, ShopAssignmentsReadWholeLinesAndReportInlineForms)
{
    {
        ResetMenuState();
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
        LoadDiagnostics::Instance().Clear();
        const std::string text = "flashlight =" + std::string(300, ' ') + "'long#path.tga'\n"
            "radar =" + std::string(4100, ' ') + "'ignored.tga'\n"
            "scent = 'after.tga'\n.\n";
        TempScript script(text.c_str());
        ASSERT_NE(script.stream, nullptr);
        ReadAccessoryIcons(script.stream);
        EXPECT_EQ(g_AccessoryIcons["flashlight"], "long#path.tga");
        EXPECT_EQ(g_AccessoryIcons.count("radar"), 0u);
        EXPECT_EQ(g_AccessoryIcons["scent"], "after.tga");
        EXPECT_EQ(LoadDiagnostics::Instance().Count(), 1u);
        TempScript prices(("flashlight =" + std::string(300, ' ') + "7\n.\n").c_str());
        ReadAccessoryPrices(prices.stream);
        EXPECT_EQ(g_AccessoryPriceOverrides["flashlight"], 7);
        LoadDiagnostics::Instance().SetMode(LoadMode::Strict);
        TempScript strict(("radar =" + std::string(4100, ' ') + "9\n.\n").c_str());
        EXPECT_THROW(ReadAccessoryPrices(strict.stream), std::exception);
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
        LoadDiagnostics::Instance().Clear();
    }

    // An inline `key = value }` form cannot be read as an assignment; it must
    // be reported instead of silently succeeding.
    {
        ResetMenuState();
        TempMenuDirectory fixture;
        std::ofstream("huntdat/_menu.txt") << "accessory_prices { flashlight = 7 }\n.\n.\n";
        std::ostringstream log;
        auto* previous = std::cout.rdbuf(log.rdbuf());
        EXPECT_NO_THROW(LoadResourcesScript());
        std::cout.rdbuf(previous);
        EXPECT_TRUE(g_AccessoryPriceOverrides.empty());
        EXPECT_NE(log.str().find("put assignments on separate lines"), std::string::npos);
    }
}

TEST(MenuResourceEntry, BackgroundDimensionsAndTruncationAreValidated)
{
    {
        TempMenuDirectory fixture;
        LoadDiagnostics::Instance().Clear();
        for (unsigned width : {1u, 800u}) {
            const unsigned height = width == 1 ? 1 : 600;
            unsigned char header[18] = {0,0,2};
            header[12] = static_cast<unsigned char>(width);
            header[13] = static_cast<unsigned char>(width >> 8);
            header[14] = static_cast<unsigned char>(height);
            header[15] = static_cast<unsigned char>(height >> 8);
            header[16] = 16;
            std::ofstream out("background.tga", std::ios::binary);
            out.write(reinterpret_cast<char*>(header), sizeof header);
            std::vector<char> pixels(width * height * 2, 0);
            out.write(pixels.data(), pixels.size());
            out.close();
            TargaImage image;
            EXPECT_EQ(LoadMenuBackground("background.tga", image), width == 800);
        }
        EXPECT_EQ(LoadDiagnostics::Instance().Count(), 1u);
        LoadDiagnostics::Instance().Clear();
    }

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
}

TEST(MenuResourceEntry, NamedPricesResolveByKeyAndRejectInvalidValues)
{
    // Keyed entries follow the names, not the entry order.
    {
        ResetMenuState();
        TempScript prices(
            "flashlight = 7\ntranq = 6\nnightvision = 5\n'Double ammo' = 4\n"
            "'Cover scent' = 3\nRadar = 2\nCamouflage = 1\n.\n");
        ASSERT_NE(prices.stream, nullptr);
        ReadAccessoryPrices(prices.stream);
        LoadResources();
        ASSERT_EQ(g_UtilInfo.size(), 7u);
        for (size_t i = 0; i < g_UtilInfo.size(); ++i)
            EXPECT_EQ(g_UtilInfo[i].m_Price, static_cast<int32_t>(i + 1));
    }

    // When a mod fills several spellings for the same row, the probe order
    // decides: canonical score key, then display name, then legacy key.
    {
        ResetMenuState();
        TempScript script("Tranquilizers = 13\ntranq -tranquilizer = 15\ntranq = 17\n.\n");
        ASSERT_NE(script.stream, nullptr);
        ReadAccessoryPrices(script.stream);
        LoadResources();
        ASSERT_EQ(g_UtilInfo.size(), 7u);
        EXPECT_EQ(g_UtilInfo[5].m_Price, 17);
        g_UtilInfo.clear();
        g_AccessoryPriceOverrides.erase("tranq");
        LoadResources();
        EXPECT_EQ(g_UtilInfo[5].m_Price, 13);
        g_UtilInfo.clear();
        g_AccessoryPriceOverrides.erase("tranquilizers");
        LoadResources();
        EXPECT_EQ(g_UtilInfo[5].m_Price, 15);
    }

    // A later invalid value must not erase an earlier valid override.
    {
        ResetMenuState();
        LoadDiagnostics::Instance().Clear();
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
        TempScript prices("camo = 7\ncamo = 999999999999\n.\n");
        ReadAccessoryPrices(prices.stream);
        EXPECT_EQ(g_AccessoryPriceOverrides["camo"], 7);
        EXPECT_EQ(LoadDiagnostics::Instance().Count(), 1u);
        LoadDiagnostics::Instance().Clear();
    }

    // Negative, non-numeric and fractional values are rejected (strict mode
    // halts on them); the display-name alias still normalizes.
    {
        ResetMenuState();
        LoadDiagnostics::Instance().Clear();
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
        TempScript script("'Cover scent' = 12\ntranq = -2\nflashlight = xyz\nradar = 12.5\n.\n");
        ASSERT_NE(script.stream, nullptr);
        EXPECT_NO_THROW(ReadAccessoryPrices(script.stream));
        EXPECT_EQ(g_AccessoryPriceOverrides["coverscent"], 12);
        EXPECT_EQ(g_AccessoryPriceOverrides.count("tranq"), 0u);
        EXPECT_EQ(g_AccessoryPriceOverrides.count("flashlight"), 0u);
        EXPECT_EQ(g_AccessoryPriceOverrides.count("radar"), 0u);
        EXPECT_EQ(LoadDiagnostics::Instance().Count(), 3u);
        LoadDiagnostics::Instance().Clear();

        TempScript strict("camo = -1\n.\n");
        ASSERT_NE(strict.stream, nullptr);
        LoadDiagnostics::Instance().SetMode(LoadMode::Strict);
        EXPECT_THROW(ReadAccessoryPrices(strict.stream), std::exception);
        LoadDiagnostics::Instance().SetMode(LoadMode::Lenient);
        LoadDiagnostics::Instance().Clear();
    }
}

TEST(MenuResourceEntry, NamedPricesDoNotShiftLegacyRows)
{
    ResetMenuState();
    g_AccessoryPrices = { 1, 2, 3, 4, 5 };
    g_AccessoryPriceOverrides["nightvision"] = 77;
    LoadResources();
    ASSERT_EQ(g_UtilInfo.size(), 7u);
    EXPECT_EQ(g_UtilInfo[4].m_Price, 77);
    EXPECT_EQ(g_UtilInfo[5].m_Price, 5);
    EXPECT_EQ(g_UtilInfo[6].m_Price, 30);
    g_UtilInfo.clear();
    g_AccessoryPriceOverrides.clear();
    LoadResources();
    ASSERT_EQ(g_UtilInfo.size(), 7u);
    EXPECT_EQ(g_UtilInfo[4].m_Price, 50);
    EXPECT_EQ(g_UtilInfo[5].m_Price, 5);
    g_UtilInfo.clear();
    g_AccessoryPrices = { 1, 2, 3, 4, 5, 6, 7 };
    LoadResources();
    EXPECT_EQ(g_UtilInfo[4].m_Price, 5);
    EXPECT_EQ(g_UtilInfo[5].m_Price, 6);
    EXPECT_EQ(g_UtilInfo[6].m_Price, 7);
}

TEST(MenuResourceEntry, StockShapedScriptLoadsBlocksAndNamedPrices)
{
    ResetMenuState();
    LoadDiagnostics::Instance().Clear();
    TempMenuDirectory fixture;
    // The shipped accessories block ends at its first dot, not its '}'.
    // Adding a dot after it makes every new block reachable; the second dot
    // after the final new block ends the top-level script.
    std::ofstream menu("huntdat/_menu.txt");
    ASSERT_TRUE(menu.is_open());
    menu << "prices {\n start = 100\n acces = 30\n acces = 40\n acces = 20\n"
            " acces = 50\n acces = 50\n acces = 60\n}\n"
            "accessories\n{\n camo = 0.85\n}\n.\n"
            "accessory_icons\n{\n scent = 'mod_scent.tga'\n}\n.\n"
            "accessory_descriptions\n{\n flashlight = 'mod_flashlight.nfo'\n}\n.\n"
            "accessory_prices\n{\n camo = 9\n nightvision = 70\n"
            " tranq = 15\n flashlight = 0\n}\n.\n.\n";
    menu.close();

    ASSERT_NO_THROW(LoadResourcesScript());
    EXPECT_EQ(g_AccessoryIcons["scent"], "mod_scent.tga");
    EXPECT_EQ(g_AccessoryDescriptions["flashlight"], "mod_flashlight.nfo");
    EXPECT_EQ(g_AccessoryPriceOverrides["nightvision"], 70);
    EXPECT_EQ(g_AccessoryPriceOverrides["flashlight"], 0);
    EXPECT_EQ(g_AccessoryPrices.size(), 6u);
    EXPECT_NO_THROW(LoadResources());
    ASSERT_EQ(g_UtilInfo.size(), 7u);
    EXPECT_EQ(g_UtilInfo[0].m_Price, 9);
    EXPECT_EQ(g_UtilInfo[1].m_Price, 40);
    EXPECT_EQ(g_UtilInfo[2].m_Price, 20);
    EXPECT_EQ(g_UtilInfo[3].m_Price, 50);
    EXPECT_EQ(g_UtilInfo[4].m_Price, 70);
    EXPECT_EQ(g_UtilInfo[5].m_Price, 15);
    EXPECT_EQ(g_UtilInfo[6].m_Price, 0);
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

TEST(MenuResourceEntry, IconBlockNormalizesKeysAndProbeOrder)
{
    // The block accepts the score key, the display name or any spelling that
    // differs only in case/spacing, and the same quoted 'path' form every
    // other script text value uses. The first implementation stored keys
    // verbatim and kept the quote characters inside the path, so both the
    // documented example keys and the documented example values broke.
    {
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

    // When a mod fills several spellings for the same row, the probe order
    // decides: score key first, then the display name, then the legacy
    // command-derived key.
    {
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
}

TEST(MenuResourceEntry, IconLookupFindsEveryRowThroughAnySpelling)
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

    {
        ResetMenuState();
        EXPECT_EQ(LookupAccessoryIcon("tranq", "Tranquilizers", "-tranq -tranquilizer",
                                      "huntdat/menu/pics/equip6.tga"),
                  "huntdat/menu/pics/equip6.tga");
    }

    // Case and spacing do not matter on the block keys either.
    for (const char* spelling : { "Cover scent", "cover  scent", "COVER-SCENT", "cover_scent" }) {
        ResetMenuState();
        TempScript script((std::string(spelling) + " = 'm/scent.tga'\n.\n").c_str());
        ASSERT_NE(script.stream, nullptr);
        EXPECT_NO_THROW(ReadAccessoryIcons(script.stream));

        EXPECT_EQ(LookupAccessoryIcon("scent", "Cover scent", "", "default.tga"),
                  "m/scent.tga") << spelling;
    }

    // The seven shop rows under their score keys. Cover scent (empty flag,
    // so the first implementation derived an empty key) and night vision
    // (whose icon load bypassed the lookup entirely) were unaddressable
    // before the normalized keys.
    {
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
    // A text override must point at an existing file (the fallback cases
    // cover both sides); these paths do not exist, so the row would keep its
    // default .nfo. The icon block carries no such requirement.
}

// The legacy profile blob is a fixed 1660-byte layout (1664 in Ice Age) that
// stock saves and the engine's TrophyLoad share. Pin the round trip before
// the IO moves TUs: every stored field must survive save -> load, and the
// file size must stay exact, so a field reorder or size drift cannot pass.
TEST(MenuProfileIO, TrophySaveLoadRoundTripsThe1660ByteFormat)
{
    TempMenuDirectory dir;

    Profile saved;
    memset(&saved, 0, sizeof(saved));
    strcpy_s(saved.Name, sizeof(saved.Name), "Round Trip");
    saved.RegNumber = 7;
    saved.Score = 350;
    saved.Last.smade = 3;
    saved.Last.success = 2;
    saved.Last.path = 1.5f;
    saved.Last.time = 2.25f;
    saved.Total.smade = 9;
    saved.Total.success = 7;
    saved.Total.path = 42.5f;
    saved.Total.time = 13.75f;
    saved.Body[0].m_CType = 4;
    saved.Body[0].m_Weapon = 2;
    saved.Body[0].m_Phase = 1;
    saved.Body[0].m_Height = 3;
    saved.Body[0].m_Weight = 180;
    saved.Body[0].m_Score = 11;
    saved.Body[0].m_Date = 20261017;
    saved.Body[0].m_Time = 1234;
    saved.Body[0].m_Scale = 0.75f;
    saved.Body[0].m_Range = 64.0f;

    Options savedOptions;
    memset(&savedOptions, 0, sizeof(savedOptions));
    savedOptions.Default();
    savedOptions.Aggression = 3;
    savedOptions.Density = 4;
    savedOptions.Sensitivity = 5;
    savedOptions.Resolution = 0;
    savedOptions.Fog = false;
    savedOptions.Textures = 2;
    savedOptions.ViewRange = 44;
    savedOptions.Shadows = false;
    savedOptions.MouseSensitivity = 6;
    savedOptions.Brightness = 7;
    savedOptions.MouseInvert = true;
    savedOptions.ScentMode = true;
    savedOptions.CamoMode = true;
    savedOptions.RadarMode = true;
    savedOptions.TranqMode = true;
    savedOptions.AlphaColorKey = 0;
    savedOptions.OptSys = 0;
    savedOptions.SoundAPI = AUDIO_OPENALSOFT;
    savedOptions.RenderAPI = 1;
    savedOptions.KeyMap.fkFire = VK_F1;
    savedOptions.KeyMap.fkCCall = 'X';
    g_Options = savedOptions;

    TrophyDelete(7);
    ASSERT_NO_THROW(TrophySave(saved));
    ASSERT_TRUE(std::filesystem::exists("trophy07.sav"));
    EXPECT_EQ(std::filesystem::file_size("trophy07.sav"), 1660u);

    // Wipe both structs so the load must restore every field instead of
    // reading leftover state.
    Profile loaded;
    memset(&loaded, 0, sizeof(loaded));
    memset(&g_Options, 0, sizeof(g_Options));
    ASSERT_NO_THROW(TrophyLoad(loaded, 7));

    EXPECT_STREQ(loaded.Name, "Round Trip");
    EXPECT_EQ(loaded.RegNumber, 7);
    EXPECT_EQ(loaded.Score, 350);
    EXPECT_EQ(loaded.Rank, RANK_MASTER);  // recomputed from Score on save/load
    EXPECT_EQ(loaded.Last.smade, 3);
    EXPECT_EQ(loaded.Last.success, 2);
    EXPECT_FLOAT_EQ(loaded.Last.path, 1.5f);
    EXPECT_FLOAT_EQ(loaded.Last.time, 2.25f);
    EXPECT_EQ(loaded.Total.smade, 9);
    EXPECT_EQ(loaded.Total.success, 7);
    EXPECT_FLOAT_EQ(loaded.Total.path, 42.5f);
    EXPECT_FLOAT_EQ(loaded.Total.time, 13.75f);
    EXPECT_EQ(loaded.Body[0].m_CType, 4);
    EXPECT_EQ(loaded.Body[0].m_Weapon, 2);
    EXPECT_EQ(loaded.Body[0].m_Score, 11);
    EXPECT_FLOAT_EQ(loaded.Body[0].m_Scale, 0.75f);
    EXPECT_FLOAT_EQ(loaded.Body[0].m_Range, 64.0f);

    EXPECT_EQ(g_Options.Aggression, 3);
    EXPECT_EQ(g_Options.Density, 4);
    EXPECT_EQ(g_Options.Sensitivity, 5);
    EXPECT_EQ(g_Options.Resolution, 0);  // old-index 0 falls back with no list
    EXPECT_EQ(g_Options.Fog, false);
    EXPECT_EQ(g_Options.Textures, 2);
    EXPECT_EQ(g_Options.ViewRange, 44);
    EXPECT_EQ(g_Options.Shadows, false);
    EXPECT_EQ(g_Options.MouseSensitivity, 6);
    EXPECT_EQ(g_Options.Brightness, 7);
    EXPECT_EQ(g_Options.MouseInvert, true);
    EXPECT_EQ(g_Options.ScentMode, true);
    EXPECT_EQ(g_Options.CamoMode, true);
    EXPECT_EQ(g_Options.RadarMode, true);
    EXPECT_EQ(g_Options.TranqMode, true);
    EXPECT_EQ(g_Options.AlphaColorKey, 0);
    EXPECT_EQ(g_Options.OptSys, 0);
    EXPECT_EQ(g_Options.SoundAPI, NormalizeAudioBackend(AUDIO_OPENALSOFT));
    EXPECT_EQ(g_Options.RenderAPI, NormalizeMenuRenderAPI(1));
    EXPECT_EQ(g_Options.KeyMap.fkFire, VK_F1);
    EXPECT_EQ(g_Options.KeyMap.fkCCall, 'X');
}
