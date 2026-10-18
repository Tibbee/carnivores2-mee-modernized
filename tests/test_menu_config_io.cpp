// test_menu_config_io.cpp -- production menu config.cfg read/write contracts
//
// ConfigIO.cpp writes next to the test executable (ConfigPath::ResolveWritePath),
// so ConfigFileGuard backs up and restores that file. The deployed game config
// is never touched.

#include <gtest/gtest.h>

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "Hunt.h"
#include "Core/Flashlight.h"

namespace {

std::filesystem::path TestConfigPath()
{
    char module[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, module, sizeof(module));
    if (length == 0 || length >= sizeof(module))
        return std::filesystem::path("config.cfg");
    return std::filesystem::path(module).parent_path() / "config.cfg";
}

std::string ReadAll(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

void WriteAll(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

class ConfigFileGuard
{
public:
    ConfigFileGuard() : path_(TestConfigPath())
    {
        std::ifstream in(path_, std::ios::binary);
        if (in.is_open()) {
            hadFile_ = true;
            original_ = std::string(std::istreambuf_iterator<char>(in),
                                    std::istreambuf_iterator<char>());
        }
    }

    ~ConfigFileGuard()
    {
        std::error_code ignored;
        if (hadFile_) {
            std::ofstream out(path_, std::ios::binary | std::ios::trunc);
            if (out.is_open())
                out.write(original_.data(), static_cast<std::streamsize>(original_.size()));
        } else {
            std::filesystem::remove(path_, ignored);
        }
    }

    ConfigFileGuard(const ConfigFileGuard&) = delete;
    ConfigFileGuard& operator=(const ConfigFileGuard&) = delete;

    void Write(const std::string& text) const { WriteAll(path_, text); }
    std::string Read() const { return ReadAll(path_); }
    void Remove() const
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

private:
    std::filesystem::path path_;
    std::string original_;
    bool hadFile_ = false;
};

void ResetConfigState()
{
    g_Options.Default();
    g_ResCount = 0;
    g_SavedHunt.Reset();
}

}  // namespace

TEST(MenuConfigIO, LoadConfigParsesOwnedKeysAndLastDuplicateWins)
{
    ConfigFileGuard file;
    ResetConfigState();
    file.Write(
        "# comment\r\n"
        "unknown_key keep\r\n"
        "renderer 2\r\n"
        "FOV 999\r\n"
        "object_detail 1\r\n"
        "fps_limit 99\r\n"
        "verbose_logging 1\r\n"
        "nightvision_key 78\r\n"
        "flashlight_key 999\r\n"
        "display_mode 2\r\n"
        "fov 60\r\n"
        "hunt_area area1\r\n"
        "hunt_dinos 5\r\n"
        "hunt_weapons 3\r\n"
        "hunt_utils 2\r\n"
        "hunt_time 2\r\n");

    LoadConfig();

    EXPECT_EQ(g_Options.RenderAPI, kRenderAPI_OpenGL);
    EXPECT_EQ(g_Options.FOV, 60);
    EXPECT_EQ(g_Options.ObjectDetail, kObjectDetailMin);
    EXPECT_EQ(g_Options.OptFpsLimit, kFpsLimitDefault);
    EXPECT_TRUE(g_Options.VerboseLogging);
    EXPECT_EQ(g_Options.NightVisionKey, 78);
    EXPECT_EQ(g_Options.FlashlightKey, kFlashlightKeyMax);
    EXPECT_EQ(g_Options.DisplayMode, 2);
    EXPECT_TRUE(g_SavedHunt.has);
    EXPECT_EQ(g_SavedHunt.area, "area1");
    EXPECT_EQ(g_SavedHunt.dinos, 5ull);
    EXPECT_EQ(g_SavedHunt.weapons, 3ull);
    EXPECT_EQ(g_SavedHunt.utils, 2ull);
    EXPECT_EQ(g_SavedHunt.time, 2);
}

TEST(MenuConfigIO, SaveConfigPreservesUnownedTextAndRewritesOwnedKeys)
{
    ConfigFileGuard file;
    ResetConfigState();
    file.Write(
        "# keep this comment\r\n"
        "unknown_key keep\r\n"
        "FOV 80\r\n"
        "renderer 0\r\n"
        "fov 70\r\n"
        "custom_no_value\r\n");

    g_Options.FOV = 62;
    g_Options.RenderAPI = kRenderAPI_OpenGL;
    g_Options.DisplayMode = 1;
    g_Options.OptFpsLimit = kFpsLimit60;
    g_Options.VerboseLogging = false;
    g_Options.NightVisionKey = 0x4E;
    g_Options.FlashlightKey = 0x46;
    g_Options.Resolution = 0;
    g_ResCount = 0;
    g_SavedHunt.has = false;

    SaveConfig();
    const std::string saved = file.Read();

    EXPECT_EQ(saved.find("# keep this comment\r\n"), 0u);
    EXPECT_NE(saved.find("unknown_key keep\r\n"), std::string::npos);
    EXPECT_NE(saved.find("custom_no_value\r\n"), std::string::npos);

    // The first duplicate is inert and must stay byte-identical; the last
    // occurrence is the one the loader honours and the writer updates.
    EXPECT_NE(saved.find("FOV 80\r\n"), std::string::npos);
    EXPECT_NE(saved.find("fov 62\r\n"), std::string::npos);
    EXPECT_EQ(saved.find("fov 70\r\n"), std::string::npos);

    EXPECT_NE(saved.find("renderer 1\r\n"), std::string::npos);
    EXPECT_EQ(saved.find("renderer 0\r\n"), std::string::npos);
    EXPECT_NE(saved.find("display_mode 1\r\n"), std::string::npos);
    EXPECT_NE(saved.find("fps_limit " + std::to_string(kFpsLimit60) + "\r\n"),
              std::string::npos);
    EXPECT_NE(saved.find("verbose_logging 0\r\n"), std::string::npos);
    EXPECT_NE(saved.find("object_detail " + std::to_string(kObjectDetailDefault) + "\r\n"),
              std::string::npos);
    EXPECT_NE(saved.find("nightvision_key 78\r\n"), std::string::npos);
    EXPECT_NE(saved.find("flashlight_key 70\r\n"), std::string::npos);

    // Conditional keys stay absent when their inputs are absent.
    EXPECT_EQ(saved.find("resolution "), std::string::npos);
    EXPECT_EQ(saved.find("hunt_"), std::string::npos);

    SaveConfig();
    EXPECT_EQ(file.Read(), saved);
}

TEST(MenuConfigIO, SaveConfigAppendsConditionalKeysWhenValid)
{
    ConfigFileGuard file;
    ResetConfigState();
    file.Write("# existing\r\n");

    g_ResCount = 2;
    g_ResolutionList[0].w = 800;
    g_ResolutionList[0].h = 600;
    g_ResolutionList[1].w = 1024;
    g_ResolutionList[1].h = 768;
    g_Options.Resolution = 1;
    g_SavedHunt.has = true;
    g_SavedHunt.area = "area1";
    g_SavedHunt.dinos = 5;
    g_SavedHunt.weapons = 3;
    g_SavedHunt.utils = 2;
    g_SavedHunt.time = 1;

    SaveConfig();
    const std::string saved = file.Read();

    EXPECT_NE(saved.find("resolution 1024x768\r\n"), std::string::npos);
    EXPECT_NE(saved.find("hunt_area area1\r\n"), std::string::npos);
    EXPECT_NE(saved.find("hunt_dinos 5\r\n"), std::string::npos);
    EXPECT_NE(saved.find("hunt_weapons 3\r\n"), std::string::npos);
    EXPECT_NE(saved.find("hunt_utils 2\r\n"), std::string::npos);
    EXPECT_NE(saved.find("hunt_time 1\r\n"), std::string::npos);
}

TEST(MenuConfigIO, FirstRunWritesCanonicalTemplateWithoutConditionalKeys)
{
    ConfigFileGuard file;
    ResetConfigState();
    file.Remove();

    SaveConfig();
    const std::string saved = file.Read();

    EXPECT_NE(saved.find("# Carnivores 2 Modder's Engine configuration\r\n"),
              std::string::npos);
    EXPECT_NE(saved.find("renderer "), std::string::npos);
    EXPECT_NE(saved.find("fov "), std::string::npos);
    EXPECT_NE(saved.find("object_detail "), std::string::npos);
    EXPECT_NE(saved.find("fps_limit "), std::string::npos);
    EXPECT_NE(saved.find("verbose_logging "), std::string::npos);
    EXPECT_NE(saved.find("nightvision_key "), std::string::npos);
    EXPECT_NE(saved.find("flashlight_key "), std::string::npos);
    EXPECT_NE(saved.find("display_mode "), std::string::npos);
    EXPECT_EQ(saved.find("resolution "), std::string::npos);
    EXPECT_EQ(saved.find("hunt_"), std::string::npos);
}

TEST(MenuConfigIO, LoadConfigMissingFileWritesCanonicalDefaults)
{
    ConfigFileGuard file;
    ResetConfigState();
    file.Remove();

    LoadConfig();

    const std::string saved = file.Read();
    EXPECT_NE(saved.find("# Carnivores 2 Modder's Engine configuration\r\n"),
              std::string::npos);
    EXPECT_NE(saved.find("display_mode "), std::string::npos);
}

TEST(MenuConfigIO, LoadConfigMapsResolutionByWidthAndHeight)
{
    ConfigFileGuard file;
    ResetConfigState();
    g_ResCount = 2;
    g_ResolutionList[0].w = 800;
    g_ResolutionList[0].h = 600;
    g_ResolutionList[1].w = 1024;
    g_ResolutionList[1].h = 768;
    file.Write("resolution 1024x768\r\n");

    LoadConfig();

    EXPECT_EQ(g_Options.Resolution, 1);
}
