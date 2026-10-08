#include <gtest/gtest.h>
#include "UnattendedCapture.h"
#include "CaptureBitmap.h"
#include <string>
#include <vector>
#include <utility>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
bool parse(std::initializer_list<std::string> args, unattended_capture::Options& options) {
    std::vector<std::string> strings(args);
    std::vector<char*> argv;
    for (auto& arg : strings) argv.push_back(arg.data());
    return unattended_capture::parse(static_cast<int>(argv.size()), argv.data(), options);
}
std::uint32_t get32(const std::array<unsigned char, 54>& header, int offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(header[offset + i]) << (8 * i);
    return value;
}
}

TEST(UnattendedCapture, OrdinaryLaunchHasNoSideEffects) {
    unattended_capture::Options options;
    EXPECT_TRUE(parse({"game", "-camspin", "-windowed"}, options));
    EXPECT_FALSE(options.enabled);
    EXPECT_EQ(options.warmupMs, 3000);
    EXPECT_EQ(options.output[0], '\0');
}

TEST(UnattendedCapture, AcceptsExplicitOptionsInEitherOrderAndPathsWithSpaces) {
    unattended_capture::Options options;
    ASSERT_TRUE(parse({"game", "-CAPTURE-OUTPUT=C:/test path/frame.bmp", "-capture-warmup-ms=2000", "-CAPTURE-BACKGROUND"}, options));
    EXPECT_TRUE(options.enabled);
    EXPECT_EQ(options.warmupMs, 2000);
    EXPECT_STREQ(options.output, "C:/test path/frame.bmp");
}

TEST(UnattendedCapture, RejectsPartialAndInvalidArgumentsAtomically) {
    unattended_capture::Options options;
    for (const auto& bad : {"", "-1", "120001", "oops", "999999999999", "3x"}) {
        EXPECT_FALSE(parse({"game", "-capture-background", "-capture-output=frame.bmp", std::string("-capture-warmup-ms=") + bad}, options));
        EXPECT_FALSE(options.enabled);
    }
    EXPECT_FALSE(parse({"game", "-capture-background"}, options));
    EXPECT_FALSE(parse({"game", "-capture-output=frame.bmp"}, options));
    EXPECT_FALSE(parse({"game", "-capture-background", "-capture-output="}, options));
    EXPECT_FALSE(parse({"game", "-capture-background", "-capture-output=" + std::string(260, 'x')}, options));
}

TEST(UnattendedCapture, WorkbenchModeIsExplicitValidatedAndCaptureOnly) {
    using Mode = unattended_capture::WorkbenchMode;
    for (const auto& entry : {std::pair<const char*, Mode>{"closed", Mode::Closed}, {"UI", Mode::UI}, {"test", Mode::Test}}) {
        unattended_capture::Options options;
        ASSERT_TRUE(parse({"game", std::string("-capture-workbench=") + entry.first,
                          "-capture-output=frame.bmp", "-capture-background"}, options));
        EXPECT_EQ(options.workbench, entry.second);
    }
    for (const auto& value : {"", "open", "1", "ui trailing", "testx"}) {
        unattended_capture::Options options;
        EXPECT_FALSE(parse({"game", "-capture-background", "-capture-output=frame.bmp",
                            std::string("-capture-workbench=") + value}, options));
        EXPECT_FALSE(options.enabled);
        EXPECT_EQ(options.workbench, Mode::Closed);
    }
    unattended_capture::Options options;
    EXPECT_FALSE(parse({"game", "-capture-workbench=ui"}, options));
    EXPECT_FALSE(parse({"game", "-capture-workbench=closed"}, options));
    EXPECT_FALSE(options.enabled);
}

TEST(UnattendedCapture, ScheduleStartsAtFirstFrameAndTriggersExactlyOnce) {
    unattended_capture::Schedule schedule;
    EXPECT_FALSE(schedule.shouldTrigger(10000, 3000));
    EXPECT_FALSE(schedule.shouldTrigger(12999, 3000));
    EXPECT_FALSE(schedule.shouldTrigger(9999, 3000));
    EXPECT_TRUE(schedule.shouldTrigger(13000, 3000));
    EXPECT_FALSE(schedule.shouldTrigger(16000, 3000));
    unattended_capture::Schedule immediate;
    EXPECT_TRUE(immediate.shouldTrigger(0, 0));
    EXPECT_FALSE(immediate.shouldTrigger(1, 0));
}

TEST(CaptureBitmap, WritesExactPixelPayloadAndReportsFileErrors) {
    struct TemporaryDirectory {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("c2-capture-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ~TemporaryDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    } temp;
    ASSERT_TRUE(std::filesystem::create_directory(temp.path));
    const auto file = (temp.path / "frame with spaces.bmp").string();
    const unsigned char pixels[] = {255, 0, 0, 255, 0, 255, 0, 255,
                                     0, 0, 255, 255, 255, 255, 255, 255};
    ASSERT_TRUE(capture_bitmap::write(file.c_str(), 2, 2, pixels));
    std::ifstream stream(file, std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), {});
    ASSERT_EQ(bytes.size(), 54u + sizeof(pixels));
    EXPECT_EQ(std::vector<unsigned char>(bytes.begin() + 54, bytes.end()),
              std::vector<unsigned char>(std::begin(pixels), std::end(pixels)));
    EXPECT_FALSE(capture_bitmap::write((temp.path / "missing" / "frame.bmp").string().c_str(), 2, 2, pixels));
    EXPECT_FALSE(capture_bitmap::write(file.c_str(), 2, 2, nullptr));
}

TEST(CaptureBitmap, HeaderHasBottomUp32BitLayoutAndCheckedSize) {
    std::array<unsigned char, 54> header{};
    std::uint32_t bytes = 0;
    ASSERT_TRUE(capture_bitmap::makeHeader(2560, 1440, header, bytes));
    EXPECT_EQ(bytes, 2560u * 1440u * 4u);
    EXPECT_EQ(header[0], 'B'); EXPECT_EQ(header[1], 'M');
    EXPECT_EQ(get32(header, 2), bytes + 54);
    EXPECT_EQ(get32(header, 10), 54u);
    EXPECT_EQ(get32(header, 18), 2560u);
    EXPECT_EQ(get32(header, 22), 1440u);
    EXPECT_EQ(header[26], 1); EXPECT_EQ(header[28], 32);
    EXPECT_EQ(get32(header, 30), 0u); // BI_RGB
    EXPECT_FALSE(capture_bitmap::makeHeader(0, 1440, header, bytes));
    EXPECT_FALSE(capture_bitmap::makeHeader(1440, -1, header, bytes));
    EXPECT_FALSE(capture_bitmap::makeHeader(2147483647, 2147483647, header, bytes));
}
