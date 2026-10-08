// Production ofBOUND entry-point tests; no world traversal or renderer.
#include <gtest/gtest.h>
#include "Hunt.h"
#include "Core/BoundFit.h"
#include <cmath>
#include <vector>
#include <new>

// Only external state used by ObjectBounds.cpp. Model allocations use the
// Win32 heap with a matching release seam, not mock fitting/collision code.
float PlayerY = 0.0f;
int OptBoundBoxFit = 0;
int OptBoundBoxFitWin = 20;
int OptBoundBoxSlope = 0;
HANDLE Heap = GetProcessHeap();
BOOL _HeapFree(HANDLE heap, DWORD flags, LPVOID ptr) { return HeapFree(heap, flags, ptr); }

namespace {
constexpr float kRadians = 3.14159265358979323846f / 180.0f;
constexpr float kCentreX = 321.0f, kCentreZ = -123.0f, kBaseY = 800.0f;
constexpr float kThickness = 96.0f;

std::unique_ptr<TModel> ModelOf(const std::vector<TPoint3d>& points) {
    auto model = std::make_unique<TModel>();
    model->VCount = static_cast<int>(points.size());
    auto* storage = static_cast<TPoint3d*>(HeapAlloc(Heap, 0, points.size() * sizeof(TPoint3d)));
    if (!storage && !points.empty()) throw std::bad_alloc();
    model->gVertex.reset(storage);
    std::copy(points.begin(), points.end(), storage);
    return model;
}

std::vector<TPoint3d> SlabPoints(float pitch, float yaw, int subdivisions) {
    const float cp = std::cos(pitch * kRadians), sp = std::sin(pitch * kRadians);
    const float cy = std::cos(yaw * kRadians), sy = std::sin(yaw * kRadians);
    std::vector<TPoint3d> points;
    for (float y : {0.0f, kThickness}) {
        for (int i = 0; i <= subdivisions; ++i) {
            for (float z : {-200.0f, 200.0f}) {
                const float x = -750.0f + 1500.0f * i / subdivisions;
                const float u = x * cp - y * sp;
                points.push_back({kCentreX + u * cy - z * sy,
                                  kBaseY + x * sp + y * cp,
                                  kCentreZ + u * sy + z * cy, 0, 0});
            }
        }
    }
    return points;
}

void WorldPoint(float u, float v, float yaw, int rot, float cellX, float cellZ,
                float& x, float& z) {
    const float c = std::cos(yaw * kRadians), s = std::sin(yaw * kRadians);
    c2bound::MapRotate(kCentreX + u * c - v * s, kCentreZ + u * s + v * c, rot, x, z);
    x += cellX;
    z += cellZ;
}

float BottomAtWorld(const TBound& b, float x, float z, int rot, float cellX, float cellZ) {
    float u, v;
    c2bound::LocalOf(x, z, cellX, cellZ, b.cx, b.cy, b.cosph, b.sinph, rot, u, v);
    return c2bound::BottomAt(b.y1, b.bottomGx, b.bottomGz, u, v);
}

std::vector<TPoint3d> LayerPoints(float bottomGx, float bottomGz,
                                float topGx, float topGz) {
    std::vector<TPoint3d> points;
    for (int layer = 0; layer < 2; ++layer) {
        for (int i = 0; i <= 4; ++i) {
            for (int j = 0; j <= 4; ++j) {
                const float x = -100.0f + i * 50.0f, z = -100.0f + j * 50.0f;
                const float y = layer == 0 ? 800.0f + bottomGx * x + bottomGz * z
                                          : 1000.0f + topGx * x + topGz * z;
                points.push_back({x, y, z, 0, 0});
            }
        }
    }
    return points;
}

class ObjectBoundsTest : public ::testing::Test {
protected:
    void SetUp() override {
        PlayerY = 0.0f;
        OptBoundBoxFit = 1;
        OptBoundBoxFitWin = 20;
        OptBoundBoxSlope = 1;
    }
    void TearDown() override {
        PlayerY = 0.0f;
        OptBoundBoxFit = 0;
        OptBoundBoxSlope = 0;
    }
};
}

TEST_F(ObjectBoundsTest, DerivesBothSurfacesOfTiltedSlabsWithoutNewAssetData) {
    for (float pitch : {-30.0f, 30.0f}) {
        for (float yaw : {0.0f, 30.0f, 45.0f}) {
            for (int subdivisions : {1, 16}) {
                SCOPED_TRACE(::testing::Message() << pitch << "," << yaw << "," << subdivisions);
                auto model = ModelOf(SlabPoints(pitch, yaw, subdivisions));
                TBound bounds[8]{};
                CalcBoundBox(model.get(), bounds);
                const TBound& b = bounds[0];
                ASSERT_GT(std::fabs(b.bottomGx) + std::fabs(b.bottomGz), 0.1f);
                ASSERT_GT(std::fabs(b.gx) + std::fabs(b.gz), 0.1f);
                const float grade = std::tan(pitch * kRadians);
                const float verticalThickness = kThickness / std::cos(pitch * kRadians);
                for (int rot = 0; rot < 4; ++rot) {
                    for (float along : {-400.0f, 0.0f, 400.0f}) {
                        float x, z, u, v;
                        WorldPoint(along, 0.0f, yaw, rot, 1024.0f, 2048.0f, x, z);
                        c2bound::LocalOf(x, z, 1024.0f, 2048.0f, b.cx, b.cy,
                                        b.cosph, b.sinph, rot, u, v);
                        EXPECT_NEAR(c2bound::BottomAt(b.y1, b.bottomGx, b.bottomGz, u, v),
                                    kBaseY + grade * along, 0.2f);
                        EXPECT_NEAR(c2bound::TopAt(b.y2, b.gx, b.gz, u, v),
                                    kBaseY + grade * along + verticalThickness, 0.2f);
                    }
                }
                for (int i = 1; i < 8; ++i) {
                    EXPECT_LT(bounds[i].a, 0.0f);
                    EXPECT_FLOAT_EQ(bounds[i].bottomGx, 0.0f);
                    EXPECT_FLOAT_EQ(bounds[i].bottomGz, 0.0f);
                }
            }
        }
    }
}

TEST_F(ObjectBoundsTest, WallPreservesClearanceUnderRaisedEndButStillBlocksInsideSlab) {
    auto model = ModelOf(SlabPoints(30.0f, 30.0f, 16));
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    for (int rot = 0; rot < 4; ++rot) {
        float x, z;
        WorldPoint(400.0f, 0.0f, 30.0f, rot, 1024.0f, 2048.0f, x, z);
        const float bottom = BottomAtWorld(bounds[0], x, z, rot, 1024.0f, 2048.0f);
        PlayerY = bottom + 160.0f - 300.0f; // 44 units above the player's head
        float px = x, pz = z;
        CheckBoundCollision(px, pz, 1024.0f, 2048.0f, 160.0f, bounds, rot);
        EXPECT_FLOAT_EQ(px, x);
        EXPECT_FLOAT_EQ(pz, z);
        PlayerY = bottom + 160.0f - 200.0f; // head intersects the slab
        CheckBoundCollision(px, pz, 1024.0f, 2048.0f, 160.0f, bounds, rot);
        EXPECT_GT(std::fabs(px - x) + std::fabs(pz - z), 1.0f);
    }
}

TEST_F(ObjectBoundsTest, CeilingReturnsLocalUndersideHeightWithMapRotationAndObjectOffset) {
    auto model = ModelOf(SlabPoints(30.0f, 30.0f, 16));
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    for (int rot = 0; rot < 4; ++rot) {
        for (float along : {-400.0f, 400.0f}) {
            float x, z, ceiling;
            WorldPoint(along, 0.0f, 30.0f, rot, 1024.0f, 2048.0f, x, z);
            const float bottom = BottomAtWorld(bounds[0], x, z, rot, 1024.0f, 2048.0f);
            PlayerY = bottom + 160.0f - 300.0f;
            ASSERT_TRUE(PointUnBound(ceiling, x, z, 1024.0f, 2048.0f, 160.0f, bounds, rot));
            EXPECT_NEAR(ceiling, bottom, 0.01f); // caller adds the object offset
            PlayerY = bottom + 160.0f - 127.0f;
            EXPECT_FALSE(PointUnBound(ceiling, x, z, 1024.0f, 2048.0f, 160.0f, bounds, rot));
        }
    }
}

TEST_F(ObjectBoundsTest, FloorStillFollowsTopInsteadOfUnderside) {
    auto model = ModelOf(SlabPoints(30.0f, 30.0f, 16));
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    for (int rot = 0; rot < 4; ++rot) {
        float x, z, floor;
        WorldPoint(400.0f, 0.0f, 30.0f, rot, 1024.0f, 2048.0f, x, z);
        const float bottom = BottomAtWorld(bounds[0], x, z, rot, 1024.0f, 2048.0f);
        const float top = bottom + kThickness / std::cos(30.0f * kRadians);
        PlayerY = top + 160.0f;
        ASSERT_TRUE(PointOnBound(floor, x, z, 1024.0f, 2048.0f, 160.0f, bounds, rot));
        EXPECT_NEAR(floor, top, 0.2f);
        PlayerY = bottom + 160.0f - 300.0f;
        EXPECT_FALSE(PointOnBound(floor, x, z, 1024.0f, 2048.0f, 160.0f, bounds, rot));
    }
}

TEST_F(ObjectBoundsTest, UndersideWorksWithoutOrientedFootprintFitting) {
    OptBoundBoxFit = 0;
    auto model = ModelOf(SlabPoints(30.0f, 30.0f, 16));
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    EXPECT_FLOAT_EQ(bounds[0].cosph, 1.0f);
    EXPECT_FLOAT_EQ(bounds[0].sinph, 0.0f);
    EXPECT_GT(std::fabs(bounds[0].bottomGx) + std::fabs(bounds[0].bottomGz), 0.1f);
    float x, z;
    WorldPoint(400.0f, 0.0f, 30.0f, 0, 0.0f, 0.0f, x, z);
    EXPECT_NEAR(BottomAtWorld(bounds[0], x, z, 0, 0.0f, 0.0f),
                kBaseY + std::tan(30.0f * kRadians) * 400.0f, 0.2f);
}

TEST_F(ObjectBoundsTest, DisabledSlopeAndFlatSlabsKeepLegacyLimits) {
    for (float pitch : {0.0f, 30.0f}) {
        auto model = ModelOf(SlabPoints(pitch, 0.0f, 16));
        TBound legacy[8]{}, enabled[8]{};
        OptBoundBoxSlope = 0;
        CalcBoundBox(model.get(), legacy);
        for (const auto& b : legacy) {
            EXPECT_FLOAT_EQ(b.gx, 0.0f);
            EXPECT_FLOAT_EQ(b.gz, 0.0f);
            EXPECT_FLOAT_EQ(b.bottomGx, 0.0f);
            EXPECT_FLOAT_EQ(b.bottomGz, 0.0f);
        }
        if (pitch == 0.0f) {
            OptBoundBoxSlope = 1;
            CalcBoundBox(model.get(), enabled);
            EXPECT_FLOAT_EQ(enabled[0].y1, legacy[0].y1);
            EXPECT_FLOAT_EQ(enabled[0].y2, legacy[0].y2);
            EXPECT_FLOAT_EQ(enabled[0].bottomGx, 0.0f);
            EXPECT_FLOAT_EQ(enabled[0].bottomGz, 0.0f);
        }
    }
}

TEST_F(ObjectBoundsTest, RebuildingWithSlopeDisabledClearsOldSurfaceGradients) {
    auto model = ModelOf(SlabPoints(30.0f, 30.0f, 16));
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    ASSERT_GT(std::fabs(bounds[0].bottomGx) + std::fabs(bounds[0].bottomGz), 0.1f);
    OptBoundBoxSlope = 0;
    CalcBoundBox(model.get(), bounds);
    EXPECT_FLOAT_EQ(bounds[0].gx, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].gz, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].bottomGx, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].bottomGz, 0.0f);
}

TEST_F(ObjectBoundsTest, RuntimeSurfaceFieldsDoNotChangeAssetRecordLayouts) {
    EXPECT_EQ(sizeof(TBound), 48u);
    EXPECT_EQ(sizeof(TObject), 520u);
    EXPECT_EQ(sizeof(TObjInfo), 64u);
    EXPECT_EQ(sizeof(TPoint3d), 16u);
}

TEST_F(ObjectBoundsTest, PartsAndHiddenVerticesDoNotContaminateEachOther) {
    auto points = SlabPoints(30.0f, 0.0f, 16);
    const size_t original = points.size();
    for (size_t i = 0; i < original; ++i) {
        TPoint3d p = points[i];
        p.owner = 7;
        p.y += 500.0f;
        points.push_back(p);
    }
    points.push_back({kCentreX, -10000.0f, kCentreZ, 0, 1});
    auto model = ModelOf(points);
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    ASSERT_GT(std::fabs(bounds[0].bottomGx), 0.1f);
    ASSERT_GT(std::fabs(bounds[7].bottomGx), 0.1f);
    EXPECT_NEAR(bounds[7].y1 - bounds[0].y1, 500.0f, 0.1f);
    EXPECT_NEAR(bounds[7].y2 - bounds[0].y2, 500.0f, 0.1f);
    for (int i = 1; i < 7; ++i) EXPECT_LT(bounds[i].a, 0.0f);
}

TEST_F(ObjectBoundsTest, CeilingPreservesStrictFootprintAndEqualityGates) {
    TBound bounds[8]{};
    for (auto& b : bounds) b.a = -1.0f;
    bounds[0] = {0.0f, 0.0f, 100.0f, 100.0f, 500.0f, 700.0f,
                 1.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    float ceiling;
    PlayerY = 500.0f - 128.0f;
    EXPECT_TRUE(PointUnBound(ceiling, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, bounds, 0));
    EXPECT_FLOAT_EQ(ceiling, 500.0f);
    EXPECT_FALSE(PointUnBound(ceiling, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, bounds, 0));
    PlayerY += 1.0f;
    EXPECT_FALSE(PointUnBound(ceiling, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, bounds, 0));
    PlayerY = 500.0f - 256.0f;
    float x = 0.0f, z = 0.0f;
    CheckBoundCollision(x, z, 0.0f, 0.0f, 0.0f, bounds, 0);
    EXPECT_GT(std::fabs(x) + std::fabs(z), 1.0f);
    PlayerY -= 1.0f;
    x = z = 0.0f;
    CheckBoundCollision(x, z, 0.0f, 0.0f, 0.0f, bounds, 0);
    EXPECT_FLOAT_EQ(x, 0.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);
}

TEST_F(ObjectBoundsTest, LowerPlaneCanFitWhenUpperPlaneIsRejected) {
    auto points = LayerPoints(0.5f, 0.0f, 0.5f, 0.0f);
    points.push_back({0.0f, 2000.0f, 0.0f, 0, 0}); // upper spike only
    auto model = ModelOf(points);
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    EXPECT_FLOAT_EQ(bounds[0].gx, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].gz, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].y2, 2000.0f);
    EXPECT_NEAR(bounds[0].bottomGx, 0.5f, 1e-4f);
    EXPECT_NEAR(bounds[0].bottomGz, 0.0f, 1e-4f);
}

TEST_F(ObjectBoundsTest, LowerSpikeRetainsFlatMinimumWithoutRejectingGoodTop) {
    auto points = LayerPoints(0.5f, 0.0f, 0.5f, 0.0f);
    points.push_back({0.0f, 100.0f, 0.0f, 0, 0});
    auto model = ModelOf(points);
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    EXPECT_FLOAT_EQ(bounds[0].bottomGx, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].bottomGz, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].y1, 100.0f);
    EXPECT_NEAR(bounds[0].gx, 0.5f, 1e-4f);
}

TEST_F(ObjectBoundsTest, RejectsUndersideThatCrossesTopOnlyAfterPadding) {
    auto points = LayerPoints(0.75f, 0.0f, 0.0f, 0.0f);
    for (auto& p : points) if (p.y == 1000.0f) p.y = 900.0f;
    auto model = ModelOf(points);
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    // Bottom <= 875 on the real mesh, but exceeds 900 when extrapolated
    // to the padded right edge. The whole-footprint order guard rejects it.
    EXPECT_FLOAT_EQ(bounds[0].bottomGx, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].bottomGz, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].y1, 724.0f); // original first-vertex -1 rule
    EXPECT_FLOAT_EQ(bounds[0].y2, 900.0f);
}

TEST_F(ObjectBoundsTest, FlatUndersideRetainsOriginalMinimumUnderSlopedTop) {
    auto model = ModelOf(LayerPoints(0.0f, 0.0f, 0.5f, 0.0f));
    TBound bounds[8]{};
    CalcBoundBox(model.get(), bounds);
    EXPECT_FLOAT_EQ(bounds[0].bottomGx, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].bottomGz, 0.0f);
    EXPECT_FLOAT_EQ(bounds[0].y1, 799.0f);
    EXPECT_NEAR(bounds[0].gx, 0.5f, 1e-4f);
}

TEST_F(ObjectBoundsTest, SteepOrCollinearSurfacesRetainLegacyLimits) {
    for (bool collinear : {false, true}) {
        auto points = LayerPoints(2.0f, 0.0f, 2.0f, 0.0f);
        if (collinear) for (auto& p : points) p.z = 0.0f;
        auto model = ModelOf(points);
        TBound legacy[8]{}, enabled[8]{};
        OptBoundBoxSlope = 0;
        CalcBoundBox(model.get(), legacy);
        OptBoundBoxSlope = 1;
        CalcBoundBox(model.get(), enabled);
        EXPECT_FLOAT_EQ(enabled[0].y1, legacy[0].y1);
        EXPECT_FLOAT_EQ(enabled[0].y2, legacy[0].y2);
        EXPECT_FLOAT_EQ(enabled[0].gx, 0.0f);
        EXPECT_FLOAT_EQ(enabled[0].gz, 0.0f);
        EXPECT_FLOAT_EQ(enabled[0].bottomGx, 0.0f);
        EXPECT_FLOAT_EQ(enabled[0].bottomGz, 0.0f);
    }
}

TEST_F(ObjectBoundsTest, WallClampsUndersideToFootprintBeforeMarginTest) {
    TBound bounds[8]{};
    for (auto& b : bounds) b.a = -1.0f;
    bounds[0] = {0.0f, 0.0f, 100.0f, 100.0f, 500.0f, 900.0f,
                 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f};
    PlayerY = 294.25f; // head 550.25; clamped underside 550, not 550.5
    float x = 101.0f, z = 0.0f;
    CheckBoundCollision(x, z, 0.0f, 0.0f, 0.0f, bounds, 0);
    EXPECT_FLOAT_EQ(x, 102.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);
}

TEST_F(ObjectBoundsTest, CeilingSelectsLowestValidPartWithoutAnArbitraryHeightCap) {
    TBound bounds[8]{};
    for (auto& b : bounds) b.a = -1.0f;
    bounds[0] = {0.0f, 0.0f, 100.0f, 100.0f, 1600.0f, 1800.0f,
                 1.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    bounds[7] = bounds[0];
    bounds[7].y1 = 1200.0f;
    bounds[7].y2 = 1400.0f;
    PlayerY = 800.0f;
    float ceiling;
    ASSERT_TRUE(PointUnBound(ceiling, 50.0f, 0.0f, 0.0f, 0.0f, 0.0f, bounds, 0));
    EXPECT_FLOAT_EQ(ceiling, 1225.0f);
}

TEST(BoundSurfaces, PlaneOrderingChecksBothGradientsAcrossAllCorners) {
    EXPECT_TRUE(c2bound::PlanesOrdered(200.0f, 0.5f, 0.2f, 100.0f, 0.5f, 0.2f, 100.0f, 50.0f));
    EXPECT_FALSE(c2bound::PlanesOrdered(200.0f, 0.0f, 0.0f, 100.0f, 1.0f, 0.1f, 100.0f, 50.0f));
    EXPECT_TRUE(c2bound::PlanesOrdered(200.0f, 0.0f, 0.0f, 100.0f, 0.5f, 1.0f, 100.0f, 50.0f));
    EXPECT_FALSE(c2bound::PlanesOrdered(99.0f, 0.5f, 0.2f, 100.0f, 0.5f, 0.2f, 100.0f, 50.0f));
}

TEST(BoundSurfaces, BottomEnvelopeRecoversBothGradientSigns) {
    auto points = LayerPoints(0.25f, -0.2f, 0.0f, 0.0f);
    std::vector<float> us, vs, ys;
    for (const auto& p : points) { us.push_back(p.x); vs.push_back(p.z); ys.push_back(p.y); }
    const auto bottom = c2bound::FitBottomPlane(
        us.data(), vs.data(), ys.data(), static_cast<int>(ys.size()),
        c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
    ASSERT_TRUE(bottom.ok);
    EXPECT_NEAR(bottom.c0, 800.0f, 0.01f);
    EXPECT_NEAR(bottom.gx, 0.25f, 1e-4f);
    EXPECT_NEAR(bottom.gz, -0.2f, 1e-4f);
    EXPECT_FLOAT_EQ(c2bound::BottomAt(800.0f, 0.25f, -0.2f, 40.0f, 50.0f), 800.0f);
    EXPECT_FALSE(c2bound::FitBottomPlane(nullptr, nullptr, nullptr, 0,
                 c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade).ok);
}

TEST_F(ObjectBoundsTest, WallUsesUndersideEvenWhenTopIsFlat) {
    TBound bounds[8]{};
    for (auto& b : bounds) b.a = -1.0f;
    bounds[0] = {0.0f, 0.0f, 100.0f, 100.0f, 500.0f, 900.0f,
                 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f};
    PlayerY = 280.0f; // head at 536; underside at x=90 is 545
    float x = 90.0f, z = 0.0f;
    CheckBoundCollision(x, z, 0.0f, 0.0f, 0.0f, bounds, 0);
    EXPECT_FLOAT_EQ(x, 90.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);
}
