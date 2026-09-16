// test_script_value_parse.cpp -- strict _RES.TXT scalar parsing

#include <gtest/gtest.h>

#include "Loaders/ScriptValueParse.h"

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
