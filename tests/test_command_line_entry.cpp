// test_command_line_entry.cpp -- production ProcessCommandLine contracts

#include <gtest/gtest.h>

#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "Hunt.h"
#include "Network/NetworkManager.h"

extern int __argc;
extern char** __argv;

NetworkManager g_Network;

namespace {

std::vector<std::string> g_logMessages;

class CommandLineArguments final
{
public:
    explicit CommandLineArguments(std::initializer_list<std::string> values)
    {
        for (const std::string& value : values)
            m_values.emplace_back(value);
        for (std::string& value : m_values)
            m_argv.push_back(value.data());
    }

    void Install()
    {
        __argc = static_cast<int>(m_argv.size());
        __argv = m_argv.data();
    }

private:
    std::vector<std::string> m_values;
    std::vector<char*> m_argv;
};

class RestoreProcessArguments final
{
public:
    RestoreProcessArguments()
        : m_argc(__argc), m_argv(__argv)
    {}

    ~RestoreProcessArguments()
    {
        __argc = m_argc;
        __argv = m_argv;
    }

private:
    int m_argc;
    char** m_argv;
};

void ResetCommandLineState()
{
    std::strcpy(ProjectName, "old-project");
    std::strcpy(g_Network.m_serverAddress, "old-server");
    TargetDino = 0;
    WeaponPres = 0;
    OptDayNight = 0;
    PlayerX = 0.0f;
    PlayerZ = 0.0f;
    LockLanding = false;
    FULLSCREEN = true;
    BORDERLESS = true;
    WinW = 640;
    WinH = 480;
    ResCount = 1;
    ResolutionList[0].w = 800;
    ResolutionList[0].h = 600;
    CurRes = -1;
    OptRes = -1;
    g_GameMode = GameMode::Normal;
    RadarMode = false;
    NightVisionMode = false;
    ScoreMod_Camo = 0.0f;
    ScoreMod_Radar = 0.0f;
    ScoreMod_Scent = 0.0f;
    ScoreMod_Double = 0.0f;
    ScoreMod_Tranq = 0.0f;
    ScoreMod_Observer = 0.0f;
    g_logMessages.clear();
}

}  // namespace

void PrintLog(LPSTR message)
{
    g_logMessages.emplace_back(message ? message : "");
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

BOOL _HeapFree(HANDLE, DWORD, LPVOID)
{
    return TRUE;
}

[[noreturn]] void DoHalt(LPSTR message)
{
    throw std::runtime_error(message ? message : "halt");
}

TEST(CommandLineEntry, ProductionPathAppliesMenuArguments)
{
    RestoreProcessArguments restoreArguments;
    ResetCommandLineState();
    CommandLineArguments arguments{
        "Carnivores1",
        "prj=huntdat/areas/area1",
        "server=localhost",
        "din=5",
        "wep=3",
        "dtm=2",
        "x=4.5",
        "y=6.5",
        "/res=800x600",
        "/windowed",
        "-nightvision",
        "-radar",
        "smod=1.1,1.2,1.3,1.4,1.5,1.6"};
    arguments.Install();

    ProcessCommandLine();

    EXPECT_STREQ(ProjectName, "huntdat/areas/area1");
    EXPECT_STREQ(g_Network.m_serverAddress, "localhost");
    EXPECT_EQ(TargetDino, 5 * 1024);
    EXPECT_EQ(WeaponPres, 3);
    EXPECT_EQ(OptDayNight, 2);
    EXPECT_FLOAT_EQ(PlayerX, 4.5f * 256.0f);
    EXPECT_FLOAT_EQ(PlayerZ, 6.5f * 256.0f);
    EXPECT_TRUE(LockLanding);
    EXPECT_FALSE(FULLSCREEN);
    EXPECT_FALSE(BORDERLESS);
    EXPECT_EQ(WinW, 800);
    EXPECT_EQ(WinH, 600);
    EXPECT_EQ(CurRes, 0);
    EXPECT_EQ(OptRes, 0);
    EXPECT_EQ(g_GameMode, GameMode::NightVision);
    EXPECT_TRUE(NightVisionMode);
    EXPECT_TRUE(RadarMode);
    EXPECT_FLOAT_EQ(ScoreMod_Camo, 1.1f);
    EXPECT_FLOAT_EQ(ScoreMod_Observer, 1.6f);
    EXPECT_TRUE(g_logMessages.empty());
}

TEST(CommandLineEntry, ProductionPathRejectsUnsafeStringsWithoutMutation)
{
    RestoreProcessArguments restoreArguments;
    ResetCommandLineState();
    CommandLineArguments arguments{
        "Carnivores1",
        std::string("prj=huntdat/areas/") + std::string(160, 'p'),
        std::string("server=") + std::string(160, 's')};
    arguments.Install();

    ProcessCommandLine();

    EXPECT_STREQ(ProjectName, "old-project");
    EXPECT_STREQ(g_Network.m_serverAddress, "old-server");
    ASSERT_EQ(g_logMessages.size(), 2u);
    EXPECT_NE(g_logMessages[0].find("prj="), std::string::npos);
    EXPECT_NE(g_logMessages[1].find("server="), std::string::npos);
}

TEST(CommandLineEntry, ProductionPathAcceptsExactCapacityAndMixedCaseOptions)
{
    RestoreProcessArguments restoreArguments;
    ResetCommandLineState();
    const std::string project(127, 'p');
    const std::string server(127, 's');
    CommandLineArguments arguments{
        "Carnivores1",
        "PrJ=" + project,
        "SeRvEr=" + server,
        "/ReS=800X600"};
    arguments.Install();

    ProcessCommandLine();

    EXPECT_STREQ(ProjectName, project.c_str());
    EXPECT_STREQ(g_Network.m_serverAddress, server.c_str());
    EXPECT_EQ(WinW, 800);
    EXPECT_EQ(WinH, 600);
    EXPECT_EQ(CurRes, 0);
    EXPECT_EQ(OptRes, 0);
    EXPECT_TRUE(g_logMessages.empty());
}

TEST(CommandLineEntry, ProductionPathRejectsEmptyAndMalformedValues)
{
    RestoreProcessArguments restoreArguments;
    ResetCommandLineState();
    CommandLineArguments arguments{
        "Carnivores1",
        "prj=",
        "server=",
        "/res=800x600junk"};
    arguments.Install();

    ProcessCommandLine();

    EXPECT_STREQ(ProjectName, "old-project");
    EXPECT_STREQ(g_Network.m_serverAddress, "old-server");
    EXPECT_EQ(WinW, 640);
    EXPECT_EQ(WinH, 480);
    ASSERT_EQ(g_logMessages.size(), 3u);
    EXPECT_NE(g_logMessages[0].find("prj="), std::string::npos);
    EXPECT_NE(g_logMessages[1].find("server="), std::string::npos);
    EXPECT_NE(g_logMessages[2].find("res="), std::string::npos);
}

TEST(CommandLineEntry, ProductionPathIgnoresUnrelatedPrefixes)
{
    RestoreProcessArguments restoreArguments;
    ResetCommandLineState();
    CommandLineArguments arguments{"Carnivores1", "xprj=not-an-option", "xserver=not-an-option"};
    arguments.Install();

    ProcessCommandLine();

    EXPECT_STREQ(ProjectName, "old-project");
    EXPECT_STREQ(g_Network.m_serverAddress, "old-server");
    EXPECT_TRUE(g_logMessages.empty());
}
