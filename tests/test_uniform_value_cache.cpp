#include <gtest/gtest.h>
#include "UniformValueCache.h"
#include "NightPresentation.h"
#include <cmath>

namespace {
}

TEST(UniformValueCache, FirstUseRepeatedValuesAndFreshProgram) {
    UniformValueCache<4> first, second;
    const std::array<float, 4> value{};
    EXPECT_TRUE(first.shouldUpload(value));
    for (int draw = 0; draw < 1000; ++draw) EXPECT_FALSE(first.shouldUpload(value));
    EXPECT_TRUE(second.shouldUpload(value));
    UniformValueCache<4> rebuilt;
    EXPECT_TRUE(rebuilt.shouldUpload(value));
}

TEST(UniformValueCache, EveryComponentAndSingleUlpChangeUploads) {
    UniformValueCache<4> cache;
    std::array<float, 4> value{1, 2, 3, 4};
    EXPECT_TRUE(cache.shouldUpload(value));
    for (std::size_t i = 0; i < value.size(); ++i) {
        value[i] = std::nextafter(value[i], 100.0f);
        EXPECT_TRUE(cache.shouldUpload(value));
        EXPECT_FALSE(cache.shouldUpload(value));
    }
}

TEST(UniformValueCache, SignedZeroIsNotSilentlyCoalesced) {
    UniformValueCache<1> cache;
    EXPECT_TRUE(cache.shouldUpload({0.0f}));
    EXPECT_TRUE(cache.shouldUpload({-0.0f}));
    EXPECT_FALSE(cache.shouldUpload({-0.0f}));
    EXPECT_TRUE(cache.shouldUpload({0.0f}));
}

TEST(UniformValueCache, NightWorldHudNvAndConfigTransitionsUseProductionPolicy) {
    UniformValueCache<3> presentation;
    UniformValueCache<4> settings;
    NightLightingSettings options;
    auto world = NightPresentationParameters(2, false, false, options, NightDrawKind::World);
    EXPECT_TRUE(presentation.shouldUpload(world));
    EXPECT_FALSE(presentation.shouldUpload(world));
    auto hud = NightPresentationParameters(2, false, false, options, NightDrawKind::Hud);
    EXPECT_TRUE(presentation.shouldUpload(hud));
    EXPECT_FALSE(presentation.shouldUpload(hud));
    EXPECT_TRUE(presentation.shouldUpload(world));
    EXPECT_TRUE(presentation.shouldUpload(NightPresentationParameters(2, true, true, options, NightDrawKind::Hud)));
    EXPECT_TRUE(presentation.shouldUpload(NightPresentationParameters(2, false, true, options, NightDrawKind::Hud)));
    EXPECT_TRUE(settings.shouldUpload(NightLightingParameters(2, false, options)));
    options.worldBrightnessPct = 15;
    EXPECT_TRUE(settings.shouldUpload(NightLightingParameters(2, false, options)));
    EXPECT_TRUE(settings.shouldUpload(NightLightingParameters(2, true, options)));
    EXPECT_FALSE(settings.shouldUpload(NightLightingParameters(1, false, options)));
}

TEST(UniformValueCache, FlashlightOffNonWorldAndDeathTransitionsCannotRetainBeam) {
    UniformValueCache<4> beam;
    const std::array<float, 4> disabled{0, kFlashlightRangeDefault, 1, 0};
    const std::array<float, 4> enabled{2, 11000, FlashlightCosFromAngleDeg(10), FlashlightCosFromAngleDeg(22)};
    EXPECT_TRUE(beam.shouldUpload(enabled));
    EXPECT_FALSE(beam.shouldUpload(enabled));
    EXPECT_TRUE(beam.shouldUpload(disabled)); // Non-world/off transition.
    EXPECT_FALSE(beam.shouldUpload(disabled));
    EXPECT_TRUE(beam.shouldUpload(enabled));
    ASSERT_FALSE(FlashlightShouldIlluminate(true, true, 200, 0));
    EXPECT_TRUE(beam.shouldUpload(disabled)); // Death transition.
    auto edited = enabled;
    for (std::size_t i = 0; i < edited.size(); ++i) {
        edited[i] = std::nextafter(edited[i], 20000.0f);
        EXPECT_TRUE(beam.shouldUpload(edited));
    }
}

TEST(UniformValueCache, RepeatedFiveUniformRequestsCollapseToFirstUploads) {
    UniformValueCache<1> strength;
    UniformValueCache<4> settings, beam;
    UniformValueCache<3> presentation, color;
    int uploads = 0;
    for (int draw = 0; draw < 1000; ++draw) {
        uploads += strength.shouldUpload({1});
        uploads += settings.shouldUpload({.15f, .85f, 1, 1});
        uploads += presentation.shouldUpload({1, 0, 0});
        uploads += beam.shouldUpload({0, kFlashlightRangeDefault, 1, 0});
        uploads += color.shouldUpload({kFlashlightColorR, kFlashlightColorG, kFlashlightColorB});
    }
    EXPECT_EQ(uploads, 5); // Previously 5000 calls, synthetic unchanged state.
}
