#include <gtest/gtest.h>
#include "Core/NightPresentation.h"
#include <limits>

TEST(NightPresentation, EquipmentStatesAgreeForMapCompassWindAndWeapon) {
	const NightLightingSettings settings;
	for (const auto kind : {NightDrawKind::Hud, NightDrawKind::Viewmodel}) {
		const auto dark = NightPresentationParameters(2, false, false, settings, kind);
		EXPECT_FLOAT_EQ(dark[0], 0.22f);
		EXPECT_FLOAT_EQ(dark[1], 0.85f);
		EXPECT_FLOAT_EQ(dark[2], 1.0f);
		EXPECT_EQ(NightPresentationParameters(2, false, true, settings, kind),
			(std::array<float, 3>{1.0f, 0.0f, 1.0f}));
		const auto nv = NightPresentationParameters(2, true, false, settings, kind);
		EXPECT_FLOAT_EQ(nv[0], 1.0f);
		EXPECT_FLOAT_EQ(nv[1], 0.85f);
		EXPECT_FLOAT_EQ(nv[2], 1.0f);
		EXPECT_EQ(NightPresentationParameters(2, true, true, settings, kind),
			NightPresentationParameters(2, true, false, settings, kind));
	}
}

TEST(NightPresentation, DayDawnWorldAndMoonOptOut) {
	NightLightingSettings settings;
	const std::array<float, 3> disabled{1.0f, 0.0f, 0.0f};
	for (const auto kind : {NightDrawKind::World, NightDrawKind::Viewmodel,
			NightDrawKind::Hud, NightDrawKind::EssentialHud, NightDrawKind::Unaffected}) {
		for (int time : {0, 1})
			for (bool nv : {false, true})
				for (bool flashlight : {false, true})
					EXPECT_EQ(NightPresentationParameters(time, nv, flashlight, settings, kind), disabled);
	}
	EXPECT_EQ(NightPresentationParameters(2, false, false, settings, NightDrawKind::World), disabled);
	EXPECT_EQ(NightPresentationParameters(2, true, true, settings, NightDrawKind::Unaffected), disabled);
}

TEST(NightPresentation, DarkGradeMatchesWorldButTorchAddsNoExtraBoost) {
	const NightLightingSettings settings;
	const NightRgb original{0.8f, 0.4f, 0.1f};
	EXPECT_EQ(GradeNightPresentation(original,
		NightPresentationParameters(2, false, false, settings, NightDrawKind::Viewmodel)),
		GradeNightWorld(original, 2, false, settings));
	EXPECT_EQ(GradeNightPresentation(original,
		NightPresentationParameters(2, false, true, settings, NightDrawKind::Viewmodel)), original);
}

TEST(NightPresentation, NightVisionRestoresBrightnessNotNaturalColor) {
	const NightLightingSettings settings;
	const NightRgb original{0.8f, 0.4f, 0.1f};
	const auto lit = GradeNightPresentation(original,
		NightPresentationParameters(2, true, true, settings, NightDrawKind::Hud));
	EXPECT_NEAR(NightLuminance(lit), NightLuminance(original), 1e-6f);
	EXPECT_LT(lit[0] - lit[2], (original[0] - original[2]) * 0.16f);
	EXPECT_NE(lit, original);
}

TEST(NightPresentation, ReadabilityFloorDoesNotRestoreColorOrReduceBrighterSettings) {
	NightLightingSettings settings;
	const NightRgb original{0.8f, 0.4f, 0.1f};
	auto parameters = NightPresentationParameters(2, false, false, settings, NightDrawKind::Hud);
	const auto normal = GradeNightPresentation(original, parameters);
	const auto readable = GradeNightPresentation(original, parameters, true);
	EXPECT_GT(NightLuminance(readable), NightLuminance(normal));
	EXPECT_NE(readable, original);
	EXPECT_FLOAT_EQ(NightPresentationParameters(2, false, false, settings,
		NightDrawKind::EssentialHud)[0], kNightReadabilityFloor);
	settings.worldBrightnessPct = 80;
	parameters = NightPresentationParameters(2, false, false, settings, NightDrawKind::Hud);
	EXPECT_EQ(GradeNightPresentation(original, parameters, true),
		GradeNightPresentation(original, parameters));
	EXPECT_EQ(GradeNightPresentation(original, {1.0f, 0.0f, 0.0f}, true), original);
}

TEST(NightPresentation, ReadabilityRegionsClipAndUseTopDownHalfOpenBounds) {
	NightReadableRegions regions;
	regions.Add(-20, -10, 420, 310, 800, 600);
	ASSERT_EQ(regions.count, 1);
	EXPECT_EQ((std::array<float, 4>{regions.bounds[0], regions.bounds[1],
		regions.bounds[2], regions.bounds[3]}), (std::array<float, 4>{0, 0, 0.5f, 0.5f}));
	EXPECT_TRUE(regions.Contains(0, 0));
	EXPECT_FALSE(regions.Contains(0.5f, 0.5f));
	regions.Add(1, 1, 1, 1, 800, 600); // already covered
	EXPECT_EQ(regions.count, 1);
	regions.Clear();
	EXPECT_FALSE(regions.Contains(0, 0));
	EXPECT_EQ(regions.count, 0);
}

TEST(NightPresentation, InvalidAndExtremeRegionsDoNotOverflow) {
	NightReadableRegions regions;
	regions.Add(0, 0, -1, 1, 800, 600);
	regions.Add(0, 0, 1, 1, 0, 600);
	regions.Add((std::numeric_limits<int>::max)(), 0,
		(std::numeric_limits<int>::max)(), 1, 800, 600);
	regions.Add((std::numeric_limits<int>::min)(), 0,
		(std::numeric_limits<int>::max)(), 1, 800, 600);
	EXPECT_EQ(regions.count, 0);
	regions.Add(-1, -1, (std::numeric_limits<int>::max)(),
		(std::numeric_limits<int>::max)(), 800, 600);
	EXPECT_EQ(regions.count, 1);
	EXPECT_TRUE(regions.Contains(0.99f, 0.99f));
}

TEST(NightPresentation, RegionOverflowFailsReadableRatherThanDroppingControls) {
	NightReadableRegions regions;
	for (int i = 0; i < kNightReadableRectLimit; ++i) regions.Add(i * 2, 0, 1, 1, 100, 100);
	EXPECT_EQ(regions.count, kNightReadableRectLimit);
	regions.Add(0, 10, 1, 1, 100, 100);
	EXPECT_EQ(regions.count, 1);
	EXPECT_TRUE(regions.Contains(0.99f, 0.99f));
	regions.Add(0, 20, 1, 1, 100, 100);
	EXPECT_EQ(regions.count, 1);
}

