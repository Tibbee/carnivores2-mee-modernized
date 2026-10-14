#include <gtest/gtest.h>
#include "UniformValueCache.h"
#include <cmath>

namespace {
using Packet = std::array<float, 60>;
}

TEST(PerFrameUpload, FirstUseAndExplicitInvalidationAlwaysUpload) {
    UniformValueCache<60> cache;
    Packet data{};
    EXPECT_TRUE(cache.shouldUpload(data));
    for (int draw = 0; draw < 1000; ++draw) EXPECT_FALSE(cache.shouldUpload(data));
    cache.invalidate();
    EXPECT_TRUE(cache.shouldUpload(data));
    EXPECT_FALSE(cache.shouldUpload(data));
    cache.invalidate();
    cache.invalidate();
    EXPECT_TRUE(cache.shouldUpload(data));
}

TEST(PerFrameUpload, EveryFloatIncludingPaddingAndSlackIsComparedExactly) {
    UniformValueCache<60> cache;
    Packet data;
    data.fill(1.0f);
    EXPECT_TRUE(cache.shouldUpload(data));
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = std::nextafter(data[i], 2.0f);
        EXPECT_TRUE(cache.shouldUpload(data)) << i;
        EXPECT_FALSE(cache.shouldUpload(data)) << i;
    }
}

TEST(PerFrameUpload, WorldNearHudAndWaterChangesUploadWithinTheSameFrame) {
    UniformValueCache<60> cache;
    Packet world{};
    world[0] = 1.0f; world[5] = 2.0f; world[10] = 1.0f; world[15] = 1.0f;
    auto near = world;
    near[0] = 3.0f; near[5] = 4.0f;
    auto hud = world;
    hud[5] = 1.0f;
    auto water = world;
    water[44] = 100; water[45] = 200; water[46] = 1; water[47] = 3;
    for (const auto& packet : {world, near, hud, water, world}) {
        EXPECT_TRUE(cache.shouldUpload(packet));
        EXPECT_FALSE(cache.shouldUpload(packet));
    }
    auto fog = world;
    fog[20] = .25f;
    EXPECT_TRUE(cache.shouldUpload(fog));
    fog[48] = .5f;
    EXPECT_TRUE(cache.shouldUpload(fog));
    fog[49] = .8f;
    EXPECT_TRUE(cache.shouldUpload(fog));
}

TEST(PerFrameUpload, PartialWaterWriteMustNotSuppressRestoringTheFullPacket) {
    UniformValueCache<60> cache;
    Packet world{};
    world[47] = 765;
    Packet simulatedBuffer = world;
    ASSERT_TRUE(cache.shouldUpload(world));
    simulatedBuffer[46] = 1;
    simulatedBuffer[47] = 3;
    cache.invalidate(); // Same operation as SetWaterAlphaFade's partial write.
    ASSERT_TRUE(cache.shouldUpload(world));
    simulatedBuffer = world;
    EXPECT_EQ(simulatedBuffer, world);
    EXPECT_FALSE(cache.shouldUpload(world));
}
