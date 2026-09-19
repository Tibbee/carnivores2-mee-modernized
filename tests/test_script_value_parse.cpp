// test_script_value_parse.cpp -- strict _RES.TXT scalar parsing

#include <cstdio>

#include <gtest/gtest.h>

#include "Loaders/ScriptBlockParse.h"
#include "Loaders/ScriptValueParse.h"

namespace {

class TempScript final
{
public:
    TempScript()
        : stream(tmpfile())
    {}

    ~TempScript()
    {
        if (stream)
            fclose(stream);
    }

    FILE* stream;
};

}  // namespace

TEST(ScriptValueParse, AcceptsLineEndingAndCommentTails)
{
    int integer = 0;
    EXPECT_TRUE(ParseScriptInt(" 42\r\n", integer));
    EXPECT_EQ(integer, 42);
    EXPECT_TRUE(ParseScriptInt("-7 // comment", integer));
    EXPECT_EQ(integer, -7);

    float decimal = 0.0f;
    EXPECT_TRUE(ParseScriptFloat(" 1.25\n", decimal));
    EXPECT_FLOAT_EQ(decimal, 1.25f);
}

TEST(ScriptValueParse, RejectsPartialAndNonFiniteValues)
{
    int integer = 99;
    EXPECT_FALSE(ParseScriptInt("4oops", integer));
    EXPECT_FALSE(ParseScriptInt("99999999999999999999", integer));
    EXPECT_EQ(integer, 99);

    float decimal = 2.0f;
    EXPECT_FALSE(ParseScriptFloat("1.5oops", decimal));
    EXPECT_FALSE(ParseScriptFloat("nan", decimal));
    EXPECT_FALSE(ParseScriptFloat("inf", decimal));
    EXPECT_FLOAT_EQ(decimal, 2.0f);
}

TEST(ScriptValueParse, PreservesLegacyDecimalToIntegerConversion)
{
    int integer = 0;
    EXPECT_TRUE(ParseScriptLegacyInt("5.5\r\n", integer));
    EXPECT_EQ(integer, 5);
    EXPECT_TRUE(ParseScriptLegacyInt("13 // comment", integer));
    EXPECT_EQ(integer, 13);
    EXPECT_FALSE(ParseScriptLegacyInt("5.5oops", integer));
}

TEST(ScriptBlockParse, ConsumesNestedBodyAndLeavesFollowingField)
{
    TempScript script;
    ASSERT_NE(script.stream, nullptr);
    fputs(" xmin = 10\n nested { value = '}' }\n }\n spawnrate = 0.5\n",
          script.stream);
    rewind(script.stream);

    ASSERT_TRUE(ConsumeScriptBlockBody(script.stream));

    char line[64] = {};
    ASSERT_NE(fgets(line, sizeof(line), script.stream), nullptr);
    EXPECT_STREQ(line, " spawnrate = 0.5\n");
}

TEST(ScriptBlockParse, IgnoresBracesInCommentsAndQuotedValues)
{
    TempScript script;
    ASSERT_NE(script.stream, nullptr);
    fputs(" value = '}' // { comment }\n }\n next = 1\n", script.stream);
    rewind(script.stream);

    ASSERT_TRUE(ConsumeScriptBlockBody(script.stream));

    char line[32] = {};
    ASSERT_NE(fgets(line, sizeof(line), script.stream), nullptr);
    EXPECT_STREQ(line, " next = 1\n");
}

TEST(ScriptBlockParse, RejectsUnterminatedBody)
{
    TempScript script;
    ASSERT_NE(script.stream, nullptr);
    fputs(" value = 1\n", script.stream);
    rewind(script.stream);

    EXPECT_FALSE(ConsumeScriptBlockBody(script.stream));
}
