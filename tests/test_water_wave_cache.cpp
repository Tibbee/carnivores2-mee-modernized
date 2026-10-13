#include <gtest/gtest.h>
#include "Core/WaterWaveCache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace {
using water_wave::Cache;
using water_wave::Parameters;
using water_wave::Surface;
constexpr Parameters stock{14.58f, 8.10f, 5.00f, 0.50f};

void FillRandom(int (&random)[32][32]) {
    // Deterministic, including both ends of the production 0..1024 range.
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            random[y][x] = (y * 173 + x * 397) % 1025;
    random[31][31] = 1024;
}

// Independent oracle: preserve HEAD's direct expressions and float addition
// ordering. The production cache is included above, not copied into tests.
Surface Direct(int time, const int (&random)[32][32], Parameters p, int x, int y) {
    const float t = static_cast<float>(time) / 200.f * p.speed;
    const float px = static_cast<float>(x) * 0.5f, py = static_cast<float>(y) * 0.5f;
    const int r1 = random[y][x], r2 = random[(y + 11) & 31][(x + 7) & 31];
    Surface s{};
    s.wx[0] = std::sin(px + py + t) * p.amp1;
    s.wy[0] = std::sin(px * 0.8f + py * 0.6f + t * 0.9f) * (p.amp1 * 0.8f);
    s.wz[0] = std::sin(pi / 2.f + px + py + t) * p.amp1;
    s.wx[1] = std::sin(px * 1.5f - py * 0.7f + t * 1.3f + r1 * 0.01f) * p.amp2;
    s.wy[1] = std::sin(px * 1.2f - py * 0.9f + t * 1.1f + r1 * 0.01f) * (p.amp2 * 0.8f);
    s.wz[1] = std::sin(pi / 3.f + px * 1.5f - py * 0.7f + t * 1.3f + r2 * 0.01f) * p.amp2;
    s.wx[2] = std::sin(px * 2.3f + py * 1.2f + t * 2.1f + r2 * 0.01f) * p.amp3;
    s.wy[2] = std::sin(px * 2.0f + py * 1.5f + t * 1.8f + r2 * 0.01f) * (p.amp3 * 0.8f);
    s.wz[2] = std::sin(pi / 4.f + px * 2.3f + py * 1.2f + t * 2.1f + r1 * 0.01f) * p.amp3;
    return s;
}
float DirectLighting(int time, int random) {
    return std::sin(-pi / 2 + random / 128 + static_cast<float>(time) / 200.f);
}

void ExpectDirect(const Cache& cache, int time, const int (&random)[32][32], Parameters p) {
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const auto ref = Direct(time, random, p, x, y);
            const auto& actual = cache.surface[y][x];
            for (int k = 0; k < 3; ++k) {
                SCOPED_TRACE(::testing::Message() << "time=" << time << " x=" << x << " y=" << y << " k=" << k << " speed=" << p.speed);
                EXPECT_FLOAT_EQ(actual.wx[k], ref.wx[k]);
                EXPECT_FLOAT_EQ(actual.wy[k], ref.wy[k]);
                EXPECT_FLOAT_EQ(actual.wz[k], ref.wz[k]);
            }
            EXPECT_FLOAT_EQ(cache.lighting[y][x], DirectLighting(time, random[y][x]));
            EXPECT_EQ(static_cast<int>(cache.lighting[y][x] * 24),
                      static_cast<int>(DirectLighting(time, random[y][x]) * 24));
        }
}

void ExpectSame(const Cache& a, const Cache& b) {
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            EXPECT_FLOAT_EQ(a.lighting[y][x], b.lighting[y][x]);
            for (int k = 0; k < 3; ++k) {
                EXPECT_FLOAT_EQ(a.surface[y][x].wx[k], b.surface[y][x].wx[k]);
                EXPECT_FLOAT_EQ(a.surface[y][x].wy[k], b.surface[y][x].wy[k]);
                EXPECT_FLOAT_EQ(a.surface[y][x].wz[k], b.surface[y][x].wz[k]);
            }
        }
}
}

TEST(WaterWaveCache, FirstUseWithAllZeroRandomPhasesIsInitialized) {
    int random[32][32]{};
    Cache cache;
    cache.update(0, random, stock);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const auto ref = Direct(0, random, stock, x, y);
            EXPECT_FLOAT_EQ(cache.lighting[y][x], DirectLighting(0, 0));
            for (int k = 0; k < 3; ++k) {
                EXPECT_NEAR(cache.surface[y][x].wx[k], ref.wx[k], 0.0001f);
                EXPECT_NEAR(cache.surface[y][x].wy[k], ref.wy[k], 0.0001f);
                EXPECT_NEAR(cache.surface[y][x].wz[k], ref.wz[k], 0.0001f);
            }
        }
}

TEST(WaterWaveCache, SameTimestampEditsInvalidateParameterAndRandomSources) {
    // A parameter or random-table edit at the same uptime must not leave
    // stale samples behind: the reused cache must match a fresh evaluation.
    {
        int random[32][32]; FillRandom(random);
        Cache reused;
        constexpr int time = 86400000;
        reused.update(time, random, stock);
        for (Parameters changed : {Parameters{0, 0, 0, 0}, Parameters{45, 16, 7, 1.3f},
                                   stock, Parameters{-14, 0, 5, -0.5f}}) {
            reused.update(time, random, changed);
            Cache fresh;
            fresh.update(time, random, changed);
            ExpectSame(reused, fresh);
            ExpectDirect(reused, time, random, changed);
        }
    }
    {
        int random[32][32]; FillRandom(random);
        Cache reused;
        constexpr int time = 604800000;
        reused.update(time, random, stock);
        random[0][0] = 1024;
        random[11][7] = 128;
        random[31][31] = 0;
        reused.update(time, random, stock);
        Cache fresh;
        fresh.update(time, random, stock);
        ExpectSame(reused, fresh);
        ExpectDirect(reused, time, random, stock);
    }
}

TEST(WaterWaveCache, ReusedCachesStayIdenticalAcrossAbsoluteAndSignedUptimes) {
    // Reusing one cache across absolute and signed uptimes must not
    // accumulate history drift.
    {
        int random[32][32]; FillRandom(random);
        Cache reused;
        for (int t : {0, 1800000, 86400000, 1814400000,
                      (std::numeric_limits<int>::max)(), (std::numeric_limits<int>::min)(), 0}) {
            reused.update(t, random, stock);
            Cache fresh;
            fresh.update(t, random, stock);
            ExpectSame(reused, fresh);
        }
    }
    {
        int random[32][32]; FillRandom(random);
        Cache cache;
        std::uint32_t state = 0x31415926u;
        for (int sample = 0; sample < 64; ++sample) {
            state = state * 1664525u + 1013904223u;
            int time;
            std::memcpy(&time, &state, sizeof(time));
            const Parameters p = sample % 2 ? stock : Parameters{45.0f, 16.0f, 7.0f, 1.3f};
            random[sample & 31][(sample * 7) & 31] = sample * 13;
            cache.update(time, random, p);
            ExpectDirect(cache, time, random, p);
        }
    }
}

TEST(WaterWaveCache, MatchesDirectLegacyAtAbsoluteUptimes) {
    int random[32][32]; FillRandom(random);
    Cache cache;
    for (int time : {0, 1, 16, 199, 200, 1000, 60000, 1800000, 3600000, 21600000, 86400000, 604800000,
                     1814400000, (std::numeric_limits<int>::max)(),
                     (std::numeric_limits<int>::min)()}) {
        cache.update(time, random, stock);
        ExpectDirect(cache, time, random, stock);
        float maxComponent = 0, maxDisplacement = 0, maxLighting = 0;
        int differentLights = 0;
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                const auto ref = Direct(time, random, stock, x, y);
                const auto& actual = cache.surface[y][x];
                float dx = 0, dy = 0, dz = 0;
                for (int k = 0; k < 3; ++k) {
                    dx += actual.wx[k] - ref.wx[k];
                    dy += actual.wy[k] - ref.wy[k];
                    dz += actual.wz[k] - ref.wz[k];
                    for (float error : {actual.wx[k] - ref.wx[k], actual.wy[k] - ref.wy[k],
                                        actual.wz[k] - ref.wz[k]}) {
                        ASSERT_TRUE(std::isfinite(error));
                        maxComponent = (std::max)(maxComponent, std::fabs(error));
                    }
                }
                maxDisplacement = (std::max)(maxDisplacement, std::sqrt(dx*dx + dy*dy + dz*dz));
                const float refLight = DirectLighting(time, random[y][x]);
                maxLighting = (std::max)(maxLighting, std::fabs(cache.lighting[y][x] - refLight));
                differentLights += static_cast<int>(cache.lighting[y][x] * 24) !=
                                   static_cast<int>(refLight * 24);
            }
        EXPECT_EQ(differentLights, 0);
        std::cout << "wave-diff time_ms=" << time << " component_max=" << maxComponent
                  << " displacement_max=" << maxDisplacement << " lighting_max=" << maxLighting
                  << " light_integer_differences=" << differentLights << "/1024\n";
    }
}
