// test_config_scan.cpp
// Parity corpus for Hunt/Core/ConfigScan.h -- the config.cfg field scanner
// shared by the readers. Every expectation mirrors the behavior of the code
// the scanner will replace, so each migration commit can prove it changed
// nothing:
//   engine reader  Hunt/Game/EngineInit.cpp LoadConfig()
//                  (sscanf_s "%63s %63s"; only column 0 starts a comment;
//                   a missing value is logged and ignored)
//   menu reader    Menu/Resources.cpp ParseConfigField()
//                  ("iss >> key", then a per-key value read; a '#'-prefixed
//                   token is treated as a comment)
//   pre-reader     Hunt/Loaders/LoadDiagnostics.h InitLoadPolicyFromConfigText()
//                  (space/tab blanks, '=' accepted as a separator)
// Both grammars are intentional (owner call, 2026-10-14); the differential
// test at the bottom runs the real pre-reader against the scanner over the
// same corpus.

#include <gtest/gtest.h>

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Core/ConfigScan.h"
#include "Loaders/LoadDiagnostics.h"

namespace {

using configscan::Field;
using configscan::Scan;
using configscan::Separator;

struct Expected {
    std::string_view key;
    std::string_view value;
    bool hasValue;
    std::size_t line;
};

void ExpectFields(const char* text, Separator separator,
                  const std::vector<Expected>& expected) {
    const std::vector<Field> fields = Scan(text, separator);
    ASSERT_EQ(fields.size(), expected.size()) << "text: " << text;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(fields[i].key, expected[i].key) << "field " << i << " of: " << text;
        EXPECT_EQ(fields[i].value, expected[i].value) << "field " << i << " of: " << text;
        EXPECT_EQ(fields[i].hasValue, expected[i].hasValue) << "field " << i << " of: " << text;
        EXPECT_EQ(fields[i].line, expected[i].line) << "field " << i << " of: " << text;
    }
}

bool KeyEqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char x = static_cast<unsigned char>(a[i]);
        const unsigned char y = static_cast<unsigned char>(b[i]);
        if (std::tolower(x) != std::tolower(y)) return false;
    }
    return true;
}

// The texts the differential runs over. They cover both grammars' accepted
// spellings, the comment rules, a refused value and last-occurrence-wins.
const char* const kCorpus[] = {
    "load_mode strict\n",
    "load_mode=strict\n",
    "load_mode = strict\n",
    "load_mode\t=\tstrict\n",
    "LOAD_MODE STRICT\n",
    "load_mode\n",
    "load_mode bogus\n",
    "load_mode strict trailing\n",
    "load_mode strict\nload_mode lenient\n",
    "load_mode lenient\nload_mode strict\n",
    "# load_mode strict\n",
    "  # load_mode=strict\n",
    "\nload_mode strict",
    "\vload_mode strict\n",
    "fov 62\nload_mode strict\n",
};

}  // namespace

TEST(ConfigScan, PinsTheWhitespaceGrammarOfTheReaders) {
    ExpectFields("fov 62\r\nobject_detail 48\n", Separator::Whitespace,
                 {{"fov", "62", true, 0}, {"object_detail", "48", true, 1}});
    ExpectFields("# comment\n\n   \nfov\t62\n", Separator::Whitespace,
                 {{"fov", "62", true, 3}});
    ExpectFields("key\n", Separator::Whitespace, {{"key", "", false, 0}});
    ExpectFields("fov=62\n", Separator::Whitespace, {{"fov=62", "", false, 0}});
    ExpectFields("fov\v62\nfov\f63\n", Separator::Whitespace,
                 {{"fov", "62", true, 0}, {"fov", "63", true, 1}});
    ExpectFields("\vload_mode strict\n", Separator::Whitespace,
                 {{"load_mode", "strict", true, 0}});
    ExpectFields("key = value\n", Separator::Whitespace, {{"key", "=", true, 0}});
    ExpectFields("a\tb\tc\n", Separator::Whitespace, {{"a", "b", true, 0}});
    ExpectFields("  # note\n  #\n", Separator::Whitespace,
                 {{"#", "note", true, 0}, {"#", "", false, 1}});
    ExpectFields("a 1\rb 2\r", Separator::Whitespace,
                 {{"a", "1", true, 0}, {"b", "2", true, 1}});
    ExpectFields("", Separator::Whitespace, {});
}

TEST(ConfigScan, PinsTheEqualsGrammarOfTheLoadPolicyPreRead) {
    ExpectFields("load_mode=strict\n", Separator::WhitespaceOrEquals,
                 {{"load_mode", "strict", true, 0}});
    ExpectFields("load_mode = strict\n", Separator::WhitespaceOrEquals,
                 {{"load_mode", "strict", true, 0}});
    ExpectFields("load_mode\t=\tstrict\n", Separator::WhitespaceOrEquals,
                 {{"load_mode", "strict", true, 0}});
    ExpectFields("load_mode==strict\n", Separator::WhitespaceOrEquals,
                 {{"load_mode", "strict", true, 0}});
    ExpectFields("load_mode=strict trailing\n", Separator::WhitespaceOrEquals,
                 {{"load_mode", "strict", true, 0}});
    ExpectFields("stream=loose\n", Separator::WhitespaceOrEquals,
                 {{"stream", "loose", true, 0}});
    ExpectFields("load_mode\n", Separator::WhitespaceOrEquals,
                 {{"load_mode", "", false, 0}});
    ExpectFields("# load_mode=strict\n", Separator::WhitespaceOrEquals, {});
    ExpectFields("  # load_mode=strict\n", Separator::WhitespaceOrEquals, {});
    ExpectFields("=load_mode strict\n", Separator::WhitespaceOrEquals, {});
}

TEST(ConfigScan, KeyEqualsIsAsciiCaseInsensitive) {
    EXPECT_TRUE(configscan::KeyEquals("FOV", "fov"));
    EXPECT_TRUE(configscan::KeyEquals("env0_decay", "ENV0_DECAY"));
    EXPECT_FALSE(configscan::KeyEquals("fov", "fovv"));
    EXPECT_FALSE(configscan::KeyEquals("fo", "fov"));
    EXPECT_FALSE(configscan::KeyEquals("f0v", "fov"));
}

TEST(ConfigScan, PreReaderMatchesTheEqualsScanOverTheCorpus) {
    for (const char* text : kCorpus) {
        std::string value;
        bool found = false;
        for (const Field& field : Scan(text, Separator::WhitespaceOrEquals)) {
            if (!field.hasValue || !KeyEqualsIgnoreCase(field.key, "load_mode")) continue;
            found = true;
            value.assign(field.value);
        }
        // Set Strict first so "not understood" is distinguishable from a
        // recognized lenient: a refused parse leaves the mode alone.
        LoadMode expected = LoadMode::Strict;
        const bool understood = found && ParseLoadMode(value.c_str(), expected);

        LoadDiagnostics::Instance().SetMode(LoadMode::Strict);
        const bool reported = InitLoadPolicyFromConfigText(text);
        EXPECT_EQ(reported, understood) << text;
        EXPECT_EQ(LoadDiagnostics::Instance().Mode(), expected) << text;
    }
}
