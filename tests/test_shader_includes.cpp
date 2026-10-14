// test_shader_includes.cpp -- the loader's #include expansion contract.
//
// GLSL 3.30 has no include mechanism; the shared flashlight/night-lighting
// block relies on shaderinclude::ExpandFile being an exact splice. These pin
// resolution relative to the including file, nesting, text passthrough and the
// error paths that must fail the shader load with a named reason.

#include <gtest/gtest.h>

#include <direct.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Renderer/ShaderIncludes.h"

namespace {

const char* kDir = "__c2_shader_include__";

class ShaderIncludes : public ::testing::Test {
protected:
    void SetUp() override { _mkdir(kDir); }

    void TearDown() override {
        for (const auto& file : files_) std::remove((std::string(kDir) + "/" + file).c_str());
        _rmdir(kDir);
    }

    void Write(const std::string& name, const std::string& text) {
        std::ofstream file(std::string(kDir) + "/" + name, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(file.is_open()) << name;
        file << text;
        files_.push_back(name);
    }

    shaderinclude::Result Expand(const std::string& name) {
        return shaderinclude::ExpandFile(std::string(kDir) + "/" + name);
    }

    std::vector<std::string> files_;
};

} // namespace

TEST_F(ShaderIncludes, SplicesRelativeAndNestedIncludesAndLeavesTextAlone)
{
    Write("nested.glsl", "float nestedValue = 2.0;\r\n");
    Write("piece.glsl", "#include \"nested.glsl\"\r\nfloat pieceValue = 1.0;\r\n");
    Write("main.frag", "void main() {\r\n   #include \"piece.glsl\"\r\n"
                      "   // #include \"comment_only.glsl\"\r\n}\r\n");

    const auto result = Expand("main.frag");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.text,
              "void main() {\r\n"
              "float nestedValue = 2.0;\r\n"
              "float pieceValue = 1.0;\r\n"
              "   // #include \"comment_only.glsl\"\r\n"
              "}\r\n");
}

TEST_F(ShaderIncludes, RejectsMissingMalformedAndCyclicIncludes)
{
    Write("malformed.frag", "#include <angle_brackets.glsl>\r\n");
    const auto malformed = Expand("malformed.frag");
    EXPECT_FALSE(malformed.ok);
    EXPECT_NE(malformed.error.find("malformed #include"), std::string::npos);

    Write("trailing.frag", "#include \"piece.glsl\" trailing\r\n");
    EXPECT_FALSE(Expand("trailing.frag").ok);

    Write("missing.frag", "#include \"absent.glsl\"\r\n");
    const auto missing = Expand("missing.frag");
    EXPECT_FALSE(missing.ok);
    EXPECT_NE(missing.error.find("absent.glsl"), std::string::npos);

    // Two files including each other stop at the depth limit instead of
    // recursing forever.
    Write("cycle_a.frag", "#include \"cycle_b.frag\"\r\n");
    Write("cycle_b.frag", "#include \"cycle_a.frag\"\r\n");
    const auto cycle = Expand("cycle_a.frag");
    EXPECT_FALSE(cycle.ok);
    EXPECT_NE(cycle.error.find("depth"), std::string::npos);

    const auto absent = shaderinclude::ExpandFile(std::string(kDir) + "/no_such_file.frag");
    EXPECT_FALSE(absent.ok);
    EXPECT_NE(absent.error.find("no_such_file.frag"), std::string::npos);
}
