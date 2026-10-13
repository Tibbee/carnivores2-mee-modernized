#include <gtest/gtest.h>
#include "Core/NightLighting.h"
#include <string>

TEST(NightLighting, MoonBrightnessBoostLeavesDawnAndDaySunUnchanged) {
	for (int time : {0, 1}) {
		EXPECT_FLOAT_EQ(NightCelestialDiscAlpha(time), 200.0f);
		EXPECT_FLOAT_EQ(NightCelestialHaloStrength(time), 0.18f);
	}
	EXPECT_FLOAT_EQ(NightCelestialDiscAlpha(2), 200.0f);
	EXPECT_FLOAT_EQ(NightCelestialHaloStrength(2), 0.10f);
	const NightLightingSettings settings;
	EXPECT_FLOAT_EQ(NightMoonBrightnessGain(2, true, settings), 1.28f);
	for (int time : {0, 1})
		EXPECT_FLOAT_EQ(NightMoonBrightnessGain(time, true, settings), 1.0f);
}


TEST(NightLighting, MoonGainSupportsHighValuesButDoesNotLeakToOtherDraws) {
	NightLightingSettings settings;
	for (int percent : {0, 50, 100, 128, 200, 1000}) {
		settings.moonBrightnessPct = percent;
		EXPECT_FLOAT_EQ(NightMoonBrightnessGain(2, true, settings), percent / 100.0f);
		for (int time : {0, 1, 2})
			EXPECT_FLOAT_EQ(NightMoonBrightnessGain(time, false, settings), 1.0f);
	}
	settings.moonBrightnessPct = 10000;
	EXPECT_FLOAT_EQ(NightMoonBrightnessGain(2, true, settings), 10.0f);
	settings.moonBrightnessPct = -1;
	EXPECT_FLOAT_EQ(NightMoonBrightnessGain(2, true, settings), 0.0f);
}

TEST(NightLighting, MoonSettingIsStrictAndPreservesOtherNightControls) {
	NightLightingSettings settings;
	for (const char* valid : {"0", "100", "128", "200", "1000"})
		EXPECT_TRUE(ParseNightLightingOption(NightLightingOption::MoonBrightness, valid, settings));
	for (const char* invalid : {"-1", "1001", "nan", "200junk", "", "99999999999999999999"}) {
		EXPECT_FALSE(ParseNightLightingOption(NightLightingOption::MoonBrightness, invalid, settings));
		EXPECT_EQ(settings.moonBrightnessPct, 1000);
	}
	EXPECT_EQ(settings.worldBrightnessPct, 22);
	EXPECT_EQ(settings.skyBrightnessPct, 60);
	EXPECT_EQ(settings.desaturationPct, 85);
}


TEST(NightLighting, MoonUniformCacheUploadsFirstUseEvenAtNeutralGain) {
	MoonBrightnessUniformCache cache;
	EXPECT_TRUE(cache.shouldUpload(1.0f));
	for (int draw = 0; draw < 1000; ++draw)
		EXPECT_FALSE(cache.shouldUpload(1.0f));
}

TEST(NightLighting, MoonUniformCacheUploadsOnlyMoonAndResetTransitions) {
	MoonBrightnessUniformCache cache;
	NightLightingSettings settings;
	int uploads = 0;
	for (int frame = 0; frame < 120; ++frame) {
		uploads += cache.shouldUpload(NightMoonBrightnessGain(2, true, settings));
		for (int draw = 0; draw < 1000; ++draw)
			uploads += cache.shouldUpload(NightMoonBrightnessGain(2, false, settings));
	}
	EXPECT_EQ(uploads, 240); // Previously 120120 unconditional uploads.
	EXPECT_FALSE(cache.shouldUpload(1.0f));
}

TEST(NightLighting, MoonUniformCacheHandlesEditsSunAndIndependentPrograms) {
	MoonBrightnessUniformCache first, second;
	NightLightingSettings settings;
	EXPECT_TRUE(first.shouldUpload(NightMoonBrightnessGain(2, true, settings)));
	EXPECT_FALSE(first.shouldUpload(NightMoonBrightnessGain(2, true, settings)));
	EXPECT_TRUE(second.shouldUpload(NightMoonBrightnessGain(2, true, settings)));
	settings.moonBrightnessPct = 200;
	EXPECT_TRUE(first.shouldUpload(NightMoonBrightnessGain(2, true, settings)));
	EXPECT_FALSE(first.shouldUpload(NightMoonBrightnessGain(2, true, settings)));
	EXPECT_TRUE(first.shouldUpload(NightMoonBrightnessGain(1, true, settings)));
	EXPECT_FALSE(first.shouldUpload(NightMoonBrightnessGain(0, true, settings)));
	settings.moonBrightnessPct = 0;
	EXPECT_TRUE(first.shouldUpload(NightMoonBrightnessGain(2, true, settings)));
	EXPECT_TRUE(first.shouldUpload(NightMoonBrightnessGain(2, false, settings)));
	EXPECT_FALSE(first.shouldUpload(NightMoonBrightnessGain(2, false, settings)));
	MoonBrightnessUniformCache rebuilt;
	EXPECT_TRUE(rebuilt.shouldUpload(1.0f));
}

TEST(NightLighting, MoonGlowScalesIndependentlyAndLeavesTheSunUnchanged) {
	NightLightingSettings settings;
	EXPECT_FLOAT_EQ(NightMoonGlowStrength(2, settings), 0.10f);
	const float discGain = NightMoonBrightnessGain(2, true, settings);
	const auto grade = NightLightingParameters(2, false, settings);
	for (int percent : {0, 50, 100, 200, 1000}) {
		settings.moonGlowStrengthPct = percent;
		EXPECT_FLOAT_EQ(NightMoonGlowStrength(2, settings), 0.10f * (percent / 100.0f));
		for (int time : {0, 1})
			EXPECT_FLOAT_EQ(NightMoonGlowStrength(time, settings), 0.18f);
		EXPECT_FLOAT_EQ(NightMoonBrightnessGain(2, true, settings), discGain);
		EXPECT_EQ(NightLightingParameters(2, false, settings), grade);
		EXPECT_FLOAT_EQ(NightCelestialDiscAlpha(2), 200.0f);
	}
	settings.moonGlowStrengthPct = -1;
	EXPECT_FLOAT_EQ(NightMoonGlowStrength(2, settings), 0.0f);
	settings.moonGlowStrengthPct = 10000;
	EXPECT_FLOAT_EQ(NightMoonGlowStrength(2, settings), 1.0f);
}

TEST(NightLighting, MoonGlowConfigRejectsInvalidValuesWithoutResettingCustomValues) {
	NightLightingSettings settings;
	for (const char* valid : {"0", "100", "200", "1000"})
		EXPECT_TRUE(ParseNightLightingOption(NightLightingOption::MoonGlowStrength, valid, settings));
	for (const char* invalid : {"-1", "1001", "nan", "200junk", "", "99999999999999999999"}) {
		EXPECT_FALSE(ParseNightLightingOption(NightLightingOption::MoonGlowStrength, invalid, settings));
		EXPECT_EQ(settings.moonGlowStrengthPct, 1000);
	}
	EXPECT_EQ(settings.moonBrightnessPct, 128);
	EXPECT_EQ(settings.worldBrightnessPct, 22);
	EXPECT_EQ(settings.skyBrightnessPct, 60);
}


TEST(NightLighting, DefaultsAndActivation) {
	const NightLightingSettings settings;
	EXPECT_EQ(settings.worldBrightnessPct, 22);
	EXPECT_EQ(settings.desaturationPct, 85);
	EXPECT_EQ(settings.skyBrightnessPct, 60);
	EXPECT_TRUE(UseMoonlitNight(2, false));
	EXPECT_FALSE(UseMoonlitNight(0, false));
	EXPECT_FALSE(UseMoonlitNight(1, false));
	EXPECT_FALSE(UseMoonlitNight(2, true));
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
	EXPECT_FALSE(ParseNightLightingOption(NightLightingOption::WorldBrightness, nullptr, settings));
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
	EXPECT_EQ(NightLightingParameters(2, true, settings), (std::array<float, 4>{1.0f, 0.0f, 1.0f, 0.0f}));
	EXPECT_EQ(GradeNightSky(color, 0.6f, 2, true, settings),
		GradeNightSky(color, 0.6f, 1, false, settings));
}

TEST(NightLighting, SingleNightLookUsesOnlyBrightnessAndDesaturationSettings) {
	NightLightingSettings settings;
	settings.worldBrightnessPct = 15;
	settings.desaturationPct = 85;
	settings.skyBrightnessPct = 100;
	const auto parameters = NightLightingParameters(2, false, settings);
	EXPECT_FLOAT_EQ(parameters[0], 0.15f);
	EXPECT_FLOAT_EQ(parameters[1], 0.85f);
	EXPECT_FLOAT_EQ(parameters[2], 1.0f);
	EXPECT_FLOAT_EQ(parameters[3], 1.0f);
	const NightRgb color = {0.8f, 0.4f, 0.1f};
	EXPECT_EQ(GradeNightWorld(color, 2, false, settings, 1.0f), color);
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

