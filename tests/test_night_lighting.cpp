#include <gtest/gtest.h>
#include "Core/NightLighting.h"
#include <fstream>
#include <iterator>
#include <string>

namespace {
std::string ReadSource(const char* relative) {
	std::ifstream file(std::string(C2_SOURCE_ROOT) + "/" + relative, std::ios::binary);
	EXPECT_TRUE(file.is_open()) << relative;
	std::string text((std::istreambuf_iterator<char>(file)), {});
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return text;
}
std::string ShaderBlock(const std::string& text, const char* begin, const char* end) {
	const auto start = text.find(begin);
	if (start == std::string::npos) return {};
	const auto finish = text.find(end, start);
	if (finish == std::string::npos) return {};
	return text.substr(start, finish - start);
}
}

TEST(NightLighting, DefaultsAndActivation) {
	const NightLightingSettings settings;
	EXPECT_EQ(settings.mode, 1);
	EXPECT_EQ(settings.worldBrightnessPct, 22);
	EXPECT_EQ(settings.desaturationPct, 85);
	EXPECT_EQ(settings.skyBrightnessPct, 60);
	EXPECT_TRUE(UseMoonlitNight(2, false, settings));
	EXPECT_FALSE(UseMoonlitNight(0, false, settings));
	EXPECT_FALSE(UseMoonlitNight(1, false, settings));
	EXPECT_FALSE(UseMoonlitNight(2, true, settings));
}

TEST(NightLighting, StrictSettingsAndInvalidDuplicatesPreserveValues) {
	NightLightingSettings settings;
	for (const auto option : {NightLightingOption::WorldBrightness, NightLightingOption::SkyBrightness}) {
		EXPECT_TRUE(ParseNightLightingOption(option, "5", settings));
		EXPECT_TRUE(ParseNightLightingOption(option, "100", settings));
		const auto before = settings;
		for (const char* invalid : {"4", "101", "-1", "nan", "22junk", "", "99999999999999999999"}) {
			EXPECT_FALSE(ParseNightLightingOption(option, invalid, settings)) << invalid;
			EXPECT_EQ(settings.worldBrightnessPct, before.worldBrightnessPct);
			EXPECT_EQ(settings.skyBrightnessPct, before.skyBrightnessPct);
		}
	}
	EXPECT_TRUE(ParseNightLightingOption(NightLightingOption::Desaturation, "0", settings));
	EXPECT_TRUE(ParseNightLightingOption(NightLightingOption::Desaturation, "100", settings));
	EXPECT_FALSE(ParseNightLightingOption(NightLightingOption::Desaturation, "101", settings));
	EXPECT_EQ(settings.desaturationPct, 100);
	EXPECT_TRUE(ParseNightLightingOption(NightLightingOption::Mode, "0", settings));
	EXPECT_FALSE(ParseNightLightingOption(NightLightingOption::Mode, "2", settings));
	EXPECT_FALSE(ParseNightLightingOption(NightLightingOption::Mode, nullptr, settings));
	EXPECT_EQ(settings.mode, 0);
}

TEST(NightLighting, OversizedConfigTokensCannotBecomeValidPrefixes) {
	EXPECT_TRUE(NightLightingConfigValueFits(" night_lighting 1 # comment", 64));
	EXPECT_FALSE(NightLightingConfigValueFits("night_lighting", 64));
	EXPECT_FALSE(NightLightingConfigValueFits(nullptr, 64));
	const std::string line = "night_lighting " + std::string(63, '0') + "junk";
	EXPECT_FALSE(NightLightingConfigValueFits(line.c_str(), 64));
	NightLightingSettings settings;
	EXPECT_FALSE(ParseNightLightingOption(static_cast<NightLightingOption>(99), "22", settings));
}

TEST(NightLighting, DayDawnAndNightVisionAreUnchanged) {
	const NightRgb color = {0.8f, 0.4f, 0.1f};
	const NightLightingSettings settings;
	EXPECT_EQ(GradeNightWorld(color, 0, false, settings, 1.0f), color);
	EXPECT_EQ(GradeNightWorld(color, 1, false, settings, 1.0f), color);
	EXPECT_EQ(GradeNightWorld(color, 2, true, settings, 1.0f), color);
	EXPECT_EQ(NightLightingParameters(2, true, settings), (std::array<float, 4>{0.5f, 0.6f, 1.0f, 0.0f}));
	EXPECT_EQ(GradeNightSky(color, 0.6f, 2, true, settings),
		GradeNightSky(color, 0.6f, 1, false, settings));
}

TEST(NightLighting, ClassicMatchesPreviousGradeAndIgnoresRestoration) {
	NightLightingSettings settings;
	settings.mode = 0;
	const NightRgb color = {0.8f, 0.4f, 0.1f};
	const auto result = GradeNightWorld(color, 2, false, settings, 1.0f);
	const float gray = NightLuminance(color);
	for (std::size_t i = 0; i < color.size(); ++i)
		EXPECT_FLOAT_EQ(result[i], (0.4f * color[i] + 0.6f * gray) * 0.5f);
	EXPECT_EQ(NightLightingParameters(2, false, settings),
		(std::array<float, 4>{0.5f, 0.6f, 1.0f, 0.0f}));
}

TEST(NightLighting, MoonlitIsDarkerAndLessSaturatedButNotBlack) {
	const NightLightingSettings settings;
	const NightRgb color = {0.8f, 0.4f, 0.1f};
	const auto result = GradeNightWorld(color, 2, false, settings);
	EXPECT_NEAR(NightLuminance(result), NightLuminance(color) * 0.22f, 1e-6f);
	EXPECT_NEAR(result[0] - result[2], (color[0] - color[2]) * 0.15f * 0.22f, 1e-6f);
	for (const float channel : result) EXPECT_GT(channel, 0.0f);
	EXPECT_EQ(GradeNightWorld(NightRgb{}, 2, false, settings), NightRgb{});
}

TEST(NightLighting, BeamRestoresBothBrightnessAndColorSmoothly) {
	const NightLightingSettings settings;
	const NightRgb color = {0.8f, 0.4f, 0.1f};
	const auto dark = GradeNightWorld(color, 2, false, settings);
	EXPECT_EQ(GradeNightWorld(color, 2, false, settings, 1.0f), color);
	const auto partial = GradeNightWorld(color, 2, false, settings, 0.5f);
	for (std::size_t i = 0; i < color.size(); ++i)
		EXPECT_FLOAT_EQ(partial[i], (dark[i] + color[i]) * 0.5f);
	EXPECT_EQ(GradeNightWorld(color, 2, false, settings, -1.0f), dark);
	EXPECT_EQ(GradeNightWorld(color, 2, false, settings, 10.0f), color);
}

TEST(NightLighting, RestorationRespectsExistingConeRangeAndIntensity) {
	const float inner = FlashlightCosFromAngleDeg(14.0f);
	const float outer = FlashlightCosFromAngleDeg(26.0f);
	EXPECT_FLOAT_EQ(NightFlashlightRestoration(0.0f, 0.0f, 6000.0f, 1.0f, inner, outer), 0.0f);
	EXPECT_FLOAT_EQ(NightFlashlightRestoration(5.0f, 6000.0f, 6000.0f, 1.0f, inner, outer), 0.0f);
	EXPECT_FLOAT_EQ(NightFlashlightRestoration(5.0f, 9000.0f, 6000.0f, 1.0f, inner, outer), 0.0f);
	EXPECT_FLOAT_EQ(NightFlashlightRestoration(5.0f, 0.0f, 6000.0f, outer, inner, outer), 0.0f);
	EXPECT_FLOAT_EQ(NightFlashlightRestoration(5.0f, 0.0f, 6000.0f, 1.0f, inner, outer), 1.0f);
	EXPECT_FLOAT_EQ(NightFlashlightRestoration(1.0f, 3000.0f, 6000.0f, 1.0f, inner, outer), 0.25f);
	EXPECT_NEAR(NightFlashlightRestoration(1.0f, 0.0f, 6000.0f, (inner + outer) * 0.5f, inner, outer), 0.5f, 1e-6f);
}

TEST(NightLighting, SkyIsReadableWithDarkerZenithAndMatchingFogGrade) {
	const NightLightingSettings settings;
	const NightRgb gray = {0.4f, 0.4f, 0.4f};
	const auto horizon = GradeNightSky(gray, 0.0f, 2, false, settings);
	const auto zenith = GradeNightSky(gray, 1.0f, 2, false, settings);
	EXPECT_NEAR(horizon[0], 0.4f * 0.6f, 1e-6f);
	EXPECT_NEAR(zenith[0], horizon[0] * 0.72f, 1e-6f);
	EXPECT_GT(zenith[0], GradeNightWorld(gray, 2, false, settings)[0]);
	const NightRgb fog = {0.2f, 0.8f, 0.1f};
	EXPECT_EQ(GradeNightSkyFog(fog, 2, false, settings), GradeNightWorld(fog, 2, false, settings));
	EXPECT_EQ(GradeNightSkyFog(NightRgb{}, 2, false, settings), NightRgb{});
	EXPECT_EQ(GradeNightSkyFog(fog, 1, false, settings), fog);
}

TEST(NightLighting, WorldShaderCopiesAndCompositionOrderStayInSync) {
	const auto terrain = ReadSource("shaders/terrain.frag");
	const auto model = ReadSource("shaders/model.frag");
	const auto instanced = ReadSource("shaders/instanced_model.frag");
	const auto night = ShaderBlock(terrain, "// BEGIN NIGHT LIGHTING", "// END NIGHT LIGHTING");
	const auto beam = ShaderBlock(terrain, "void ApplyFlashlight(", "// Classic initializer");
	ASSERT_FALSE(night.empty());
	ASSERT_FALSE(beam.empty());
	EXPECT_NE(night.find("uNightSettings.y * uNightStrength"), std::string::npos);
	EXPECT_NE(night.find("mix(1.0, uNightSettings.x, uNightStrength)"), std::string::npos);
	EXPECT_NE(night.find("clamp(uFlashlight.x * cone * atten, 0.0, 1.0)"), std::string::npos);
	EXPECT_NE(night.find("mix(color, originalColor, restoration)"), std::string::npos);
	for (const auto* shader : {&terrain, &model, &instanced}) {
		EXPECT_EQ(ShaderBlock(*shader, "// BEGIN NIGHT LIGHTING", "// END NIGHT LIGHTING"), night);
		EXPECT_EQ(ShaderBlock(*shader, "void ApplyFlashlight(", "// Classic initializer"), beam);
		EXPECT_NE(shader->find("uNightSettings = vec4(0.5, 0.6, 1.0, 0.0)"), std::string::npos);
		const auto restore = shader->find("ApplyNightLighting(finalColor, beamDistance, beamCos);");
		const auto illuminate = shader->find("ApplyFlashlight(finalColor, beamDistance, beamCos);");
		ASSERT_NE(restore, std::string::npos);
		ASSERT_NE(illuminate, std::string::npos);
		EXPECT_LT(restore, illuminate);
	}
}

TEST(NightLighting, EveryActiveDrawPushesUniformsAndMoonStaysExcluded) {
	EXPECT_NE(ReadSource("Hunt/Renderer/GLTerrain.cpp").find("ApplyNightLightingUniforms(m_terrainShader.GetProgramID());"), std::string::npos);
	EXPECT_NE(ReadSource("Hunt/Renderer/GLWater.cpp").find("ApplyNightLightingUniforms(m_terrainShader.GetProgramID());"), std::string::npos);
	const auto models = ReadSource("Hunt/Renderer/GLModel.cpp");
	EXPECT_NE(models.find("ApplyNightLightingUniforms(m_instancedModelShader.GetProgramID());"), std::string::npos);
	EXPECT_NE(models.find("ApplyNightLightingUniforms(m_modelShader.GetProgramID());"), std::string::npos);
	const auto sky = ReadSource("Hunt/Renderer/GLSky.cpp");
	EXPECT_NE(sky.find("ApplyNightLightingUniforms(m_skyShader.GetProgramID());"), std::string::npos);
	EXPECT_NE(sky.find("if (uNight >= 0) glUniform1f(uNight, 0.0f);"), std::string::npos);
	EXPECT_NE(sky.find("ApplyFlashlightUniforms(m_modelShader.GetProgramID(), false);"), std::string::npos);
}
