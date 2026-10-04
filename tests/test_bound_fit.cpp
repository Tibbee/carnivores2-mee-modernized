// ==========================================================================
// test_bound_fit.cpp -- Unit tests for Hunt/Core/BoundFit.h
// ==========================================================================
// BoundFit.h is pure, so no engine linkage is needed. The equivalence cases
// pin the oriented wall helper against a local re-implementation of
// CheckBoundCollision's legacy formula for axis-aligned boxes (using sample
// points that avoid exact ties, where the two resolutions are equivalent but
// pick different axes). Fitted-box cases pin the rotated-frame behavior.

#include <gtest/gtest.h>

#include "Core/BoundFit.h"

#include <cmath>
#include <vector>

namespace {

constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;

// Rectangle footprint corners (model space) rotated by deg.
void RectPoints(float L, float W, float deg, float* xs, float* zs) {
    const float c = std::cos(deg * kDeg2Rad);
    const float s = std::sin(deg * kDeg2Rad);
    const float hx = L * 0.5f;
    const float hz = W * 0.5f;
    const float corners[4][2] = {{-hx, -hz}, {hx, -hz}, {hx, hz}, {-hx, hz}};
    for (int i = 0; i < 4; ++i) {
        xs[i] = corners[i][0] * c - corners[i][1] * s;
        zs[i] = corners[i][0] * s + corners[i][1] * c;
    }
}

struct WallResult {
    bool hit = false;
    float nx = 0.0f, nz = 0.0f;
};

// Local port of the legacy CheckBoundCollision result (axis-aligned boxes).
WallResult LegacyWall(float px, float pz, float cellX, float cellZ, float cx, float cz,
                      float a, float b, int rotIdx, float margin) {
    const float rc[4] = {1.0f, 0.0f, -1.0f, 0.0f};
    const float rs[4] = {0.0f, 1.0f, 0.0f, -1.0f};
    const float ca = rc[rotIdx & 3];
    const float sa = rs[rotIdx & 3];
    const float ppx = px - cellX, ppy = pz - cellZ;
    const float ccx = cx * ca + cz * sa;
    const float ccy = cz * ca - cx * sa;
    float w, h;
    if (rotIdx & 1) { w = b + margin; h = a + margin; }
    else            { w = a + margin; h = b + margin; }
    WallResult r;
    r.nx = px;
    r.nz = pz;
    const float dw = std::fabs(ppx - ccx) - w;
    const float dh = std::fabs(ppy - ccy) - h;
    if (dw <= 0.0f && dh <= 0.0f) {
        r.hit = true;
        if (dw > dh) {
            r.nx = cellX + ccx + w * ((ppx - ccx) < 0.0f ? -1.0f : 1.0f);
        } else {
            r.nz = cellZ + ccy + h * ((ppy - ccy) < 0.0f ? -1.0f : 1.0f);
        }
    }
    return r;
}

c2bound::FitBox MakeBox(float a, float b, float cosAng, float sinAng) {
    c2bound::FitBox box;
    box.a = a;
    box.b = b;
    box.cos = cosAng;
    box.sin = sinAng;
    return box;
}

}  // namespace

TEST(BoundFit, CandidatesPick45ForDiagonalRectangle) {
    float xs[4], zs[4];
    RectPoints(2048.0f, 256.0f, 45.0f, xs, zs);
    const c2bound::FitBox f = c2bound::FitCandidates(xs, zs, 4, c2bound::kPad);
    EXPECT_FLOAT_EQ(f.cos, c2bound::kSqrt1_2);
    EXPECT_FLOAT_EQ(f.sin, c2bound::kSqrt1_2);
    EXPECT_NEAR(f.a, 1024.0f + c2bound::kPad, 0.05f);
    EXPECT_NEAR(f.b, 128.0f + c2bound::kPad, 0.05f);
    EXPECT_NEAR(f.cx, 0.0f, 0.05f);
    EXPECT_NEAR(f.cz, 0.0f, 0.05f);
}

TEST(BoundFit, CandidatesKeepLegacyBoxWhenAxisAligned) {
    float xs[4], zs[4];
    RectPoints(2048.0f, 256.0f, 0.0f, xs, zs);
    const c2bound::FitBox f = c2bound::FitCandidates(xs, zs, 4, c2bound::kPad);
    EXPECT_FLOAT_EQ(f.cos, 1.0f);
    EXPECT_FLOAT_EQ(f.sin, 0.0f);
    EXPECT_NEAR(f.a, 1024.0f + c2bound::kPad, 0.01f);
    EXPECT_NEAR(f.b, 128.0f + c2bound::kPad, 0.01f);
    const float aabbArea = (2048.0f + 2.0f * c2bound::kPad) * (256.0f + 2.0f * c2bound::kPad);
    EXPECT_FALSE(c2bound::FitAccepts(f.Area(), aabbArea, 20));
}

TEST(BoundFit, MinAreaFollowsTheBakedAngle) {
    float xs[4], zs[4];
    RectPoints(2048.0f, 256.0f, 30.0f, xs, zs);
    const c2bound::FitBox f = c2bound::FitMinArea(xs, zs, 4, c2bound::kPad);
    EXPECT_NEAR(f.Area(), (2048.0f + 2.0f * c2bound::kPad) * (256.0f + 2.0f * c2bound::kPad), 100.0f);
    const float c30 = std::cos(30.0f * kDeg2Rad);
    const float s30 = std::sin(30.0f * kDeg2Rad);
    if (f.a > f.b) {
        EXPECT_NEAR(std::fabs(f.cos), c30, 1e-3f);
        EXPECT_NEAR(std::fabs(f.sin), s30, 1e-3f);
        EXPECT_NEAR(f.a, 1024.0f + c2bound::kPad, 0.5f);
        EXPECT_NEAR(f.b, 128.0f + c2bound::kPad, 0.5f);
    } else {
        EXPECT_NEAR(std::fabs(f.cos), s30, 1e-3f);
        EXPECT_NEAR(std::fabs(f.sin), c30, 1e-3f);
        EXPECT_NEAR(f.a, 128.0f + c2bound::kPad, 0.5f);
        EXPECT_NEAR(f.b, 1024.0f + c2bound::kPad, 0.5f);
    }
}

TEST(BoundFit, MinAreaAgreesWithCandidatesAt45) {
    float xs[4], zs[4];
    RectPoints(2048.0f, 256.0f, 45.0f, xs, zs);
    const c2bound::FitBox c = c2bound::FitCandidates(xs, zs, 4, c2bound::kPad);
    const c2bound::FitBox m = c2bound::FitMinArea(xs, zs, 4, c2bound::kPad);
    EXPECT_NEAR(m.Area(), c.Area(), 1.0f);
}

TEST(BoundFit, AcceptThresholdBoundaries) {
    EXPECT_TRUE(c2bound::FitAccepts(75.0f, 100.0f, 25));    // win = 0.25 exactly
    EXPECT_FALSE(c2bound::FitAccepts(75.1f, 100.0f, 25));
    EXPECT_TRUE(c2bound::FitAccepts(74.9f, 100.0f, 25));
    EXPECT_TRUE(c2bound::FitAccepts(80.0f, 100.0f, 20));    // win = 0.20 exactly
    EXPECT_FALSE(c2bound::FitAccepts(100.0f, 100.0f, 20));  // no win
    EXPECT_TRUE(c2bound::FitAccepts(100.0f, 100.0f, 0));    // 0% threshold accepts identity
    EXPECT_FALSE(c2bound::FitAccepts(150.0f, 100.0f, 20));  // worse fits never win
    EXPECT_FALSE(c2bound::FitAccepts(10.0f, 0.0f, 20));     // no base area
}

TEST(BoundFit, WallPushFacePushAndMiss) {
    const c2bound::FitBox box = MakeBox(100.0f, 50.0f, 1.0f, 0.0f);
    float px = 95.0f, pz = 0.0f;
    EXPECT_TRUE(c2bound::WallPushOriented(px, pz, 0.0f, 0.0f, box, 0, 2.0f));
    EXPECT_FLOAT_EQ(px, 102.0f);
    EXPECT_FLOAT_EQ(pz, 0.0f);

    float nx = 105.0f, nz = 0.0f;
    EXPECT_FALSE(c2bound::WallPushOriented(nx, nz, 0.0f, 0.0f, box, 0, 2.0f));
    EXPECT_FLOAT_EQ(nx, 105.0f);
    EXPECT_FLOAT_EQ(nz, 0.0f);
}

TEST(BoundFit, WallPushLeastPenetrationAndSgnZero) {
    const c2bound::FitBox box = MakeBox(100.0f, 50.0f, 1.0f, 0.0f);
    // Near the centre both faces are far; the least-penetration axis is the
    // near one (z), and qy == 0 must push to +h, never NaN (SGN never 0).
    float px = -3.0f, pz = 0.0f;
    EXPECT_TRUE(c2bound::WallPushOriented(px, pz, 0.0f, 0.0f, box, 0, 2.0f));
    EXPECT_FLOAT_EQ(px, -3.0f);
    EXPECT_FLOAT_EQ(pz, 52.0f);
}

TEST(BoundFit, WallPushMatchesLegacyForAxisAlignedBoxes) {
    const float a = 100.0f, b = 50.0f;
    // Points whose pushed coordinate would be exactly 0 are excluded: both
    // faces are equally close there, and the helper's sign convention may
    // resolve toward the opposite (equivalent) face than the legacy code.
    const float samples[7][2] = {
        {95.0f, 0.0f}, {7.0f, 45.0f}, {10.0f, 10.0f}, {7.0f, -47.0f},
        {-90.0f, 30.0f}, {40.0f, -30.0f}, {0.0f, -90.0f},
    };
    const c2bound::FitBox box = MakeBox(a, b, 1.0f, 0.0f);
    for (int rot = 0; rot < 4; ++rot) {
        for (const auto& s : samples) {
            float hx = s[0], hz = s[1];
            const bool hit = c2bound::WallPushOriented(hx, hz, 0.0f, 0.0f, box, rot, 2.0f);
            const WallResult leg = LegacyWall(s[0], s[1], 0.0f, 0.0f, 0.0f, 0.0f, a, b, rot, 2.0f);
            EXPECT_EQ(hit, leg.hit) << "rot=" << rot << " sample=" << s[0] << "," << s[1];
            if (hit) {
                EXPECT_FLOAT_EQ(hx, leg.nx) << "rot=" << rot << " sample=" << s[0] << "," << s[1];
                EXPECT_FLOAT_EQ(hz, leg.nz) << "rot=" << rot << " sample=" << s[0] << "," << s[1];
            }
        }
    }
}

TEST(BoundFit, MapRotationComposesWithFitAngle) {
    // φ = 45° at map rotation 90° is the same box as φ = -45° unrotated.
    const c2bound::FitBox f45 = MakeBox(1096.0f, 200.0f, c2bound::kSqrt1_2, c2bound::kSqrt1_2);
    const c2bound::FitBox fm45 = MakeBox(1096.0f, 200.0f, c2bound::kSqrt1_2, -c2bound::kSqrt1_2);
    float ax = 636.4f, az = -636.4f;
    float bx = ax, bz = az;
    const bool ha = c2bound::WallPushOriented(ax, az, 0.0f, 0.0f, f45, 1, 2.0f);
    const bool hb = c2bound::WallPushOriented(bx, bz, 0.0f, 0.0f, fm45, 0, 2.0f);
    ASSERT_TRUE(ha);
    ASSERT_TRUE(hb);
    EXPECT_NEAR(ax, bx, 1e-3f);
    EXPECT_NEAR(az, bz, 1e-3f);
}

TEST(BoundFit, OrientedContainsRotatedFrame) {
    const c2bound::FitBox f = MakeBox(1096.0f, 200.0f, c2bound::kSqrt1_2, c2bound::kSqrt1_2);
    // On the u axis: inside at 1000, outside at 1200.
    EXPECT_TRUE(c2bound::OrientedContains(1000.0f * c2bound::kSqrt1_2,
                                          1000.0f * c2bound::kSqrt1_2, 0.0f, 0.0f, f, 0));
    EXPECT_FALSE(c2bound::OrientedContains(1200.0f * c2bound::kSqrt1_2,
                                           1200.0f * c2bound::kSqrt1_2, 0.0f, 0.0f, f, 0));
    // On the v axis: inside at 150, outside at 250.
    EXPECT_TRUE(c2bound::OrientedContains(-150.0f * c2bound::kSqrt1_2,
                                          150.0f * c2bound::kSqrt1_2, 0.0f, 0.0f, f, 0));
    EXPECT_FALSE(c2bound::OrientedContains(-250.0f * c2bound::kSqrt1_2,
                                           250.0f * c2bound::kSqrt1_2, 0.0f, 0.0f, f, 0));
    // Rotating the query with the map rotation is the same containment.
    float rx = 0.0f, rz = 0.0f;
    c2bound::MapRotate(1000.0f * c2bound::kSqrt1_2, 1000.0f * c2bound::kSqrt1_2, 1, rx, rz);
    EXPECT_TRUE(c2bound::OrientedContains(rx, rz, 0.0f, 0.0f, f, 1));
}

TEST(BoundFit, DegenerateInputsAreSafe) {
    // Single point: both fit entry points must stay finite.
    const float oneX[1] = {123.0f}, oneZ[1] = {456.0f};
    const c2bound::FitBox p = c2bound::FitCandidates(oneX, oneZ, 1, c2bound::kPad);
    EXPECT_NEAR(p.cx, 123.0f, 0.01f);
    EXPECT_NEAR(p.cz, 456.0f, 0.01f);

    // Collinear points: the hull degenerates and FitMinArea falls back.
    const float lineX[3] = {0.0f, 10.0f, 20.0f};
    const float lineZ[3] = {0.0f, 10.0f, 20.0f};
    const c2bound::FitBox l = c2bound::FitMinArea(lineX, lineZ, 3, c2bound::kPad);
    EXPECT_TRUE(std::isfinite(l.cx) && std::isfinite(l.cz));
    EXPECT_GT(l.Area(), 0.0f);
}

// ===================== top-plane (slope) fit =====================

TEST(BoundFit, TopPlaneFitsSlopedDeckAt30) {
    // 30° planar top y = 100 + tan30°·u over a 2048 x 256 footprint.
    const float grade = std::tan(30.0f * kDeg2Rad);
    std::vector<float> us, vs, ys;
    for (int i = 0; i < 33; ++i) {
        for (int j = 0; j < 5; ++j) {
            const float u = -1024.0f + 2048.0f * static_cast<float>(i) / 32.0f;
            const float v = -128.0f + 256.0f * static_cast<float>(j) / 4.0f;
            us.push_back(u);
            vs.push_back(v);
            ys.push_back(100.0f + grade * u);
        }
    }
    const c2bound::TopPlane p = c2bound::FitTopPlane(
        us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
        c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
    ASSERT_TRUE(p.ok);
    EXPECT_NEAR(p.gx, grade, 1e-4f);
    EXPECT_NEAR(p.gz, 0.0f, 1e-4f);
    EXPECT_NEAR(p.c0, 100.0f, 0.1f);
}

TEST(BoundFit, TopPlaneAcceptsFlatTopAtZeroGrade) {
    std::vector<float> us, vs, ys;
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 4; ++j) {
            us.push_back(-500.0f + 50.0f * static_cast<float>(i));
            vs.push_back(-100.0f + 50.0f * static_cast<float>(j));
            ys.push_back(64.0f);
        }
    }
    const c2bound::TopPlane p = c2bound::FitTopPlane(
        us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
        c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
    ASSERT_TRUE(p.ok);
    EXPECT_NEAR(p.gx, 0.0f, 1e-6f);
    EXPECT_NEAR(p.gz, 0.0f, 1e-6f);
    // Below kSlopeMinGrade the producer collapses back to the legacy flat box.
    EXPECT_LT(std::sqrt(p.gx * p.gx + p.gz * p.gz), c2bound::kSlopeMinGrade);
}

TEST(BoundFit, TopPlaneRejectsSteepNonPlanarAndDegenerateTops) {
    // 65° from horizontal exceeds the tan-60° grade cap (planar, 2D spread).
    {
        const float steep = std::tan(65.0f * kDeg2Rad);
        std::vector<float> us, vs, ys;
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 3; ++j) {
                const float u = -512.0f + 64.0f * static_cast<float>(i);
                us.push_back(u);
                vs.push_back(-64.0f + 64.0f * static_cast<float>(j));
                ys.push_back(steep * u);
            }
        }
        const c2bound::TopPlane p = c2bound::FitTopPlane(
            us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
            c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
        EXPECT_FALSE(p.ok);
    }
    // A single spike above an otherwise flat top breaks near-planarity.
    {
        std::vector<float> us, vs, ys;
        for (int i = 0; i < 12; ++i) {
            for (int j = 0; j < 4; ++j) {
                us.push_back(-400.0f + 80.0f * static_cast<float>(i));
                vs.push_back(-96.0f + 64.0f * static_cast<float>(j));
                ys.push_back(50.0f + ((i == 6 && j == 2) ? 40.0f : 0.0f));
            }
        }
        const c2bound::TopPlane p = c2bound::FitTopPlane(
            us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
            c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
        EXPECT_FALSE(p.ok);
    }
    // Collinear samples leave the gradient underdetermined.
    {
        std::vector<float> us, vs, ys;
        for (int i = 0; i < 10; ++i) {
            us.push_back(-450.0f + 100.0f * static_cast<float>(i));
            vs.push_back(0.0f);
            ys.push_back(0.3f * us.back());
        }
        const c2bound::TopPlane p = c2bound::FitTopPlane(
            us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
            c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
        EXPECT_FALSE(p.ok);
    }
}

TEST(BoundFit, TopAtEvaluatesTheSurfacePlane) {
    EXPECT_FLOAT_EQ(c2bound::TopAt(100.0f, 0.5f, -0.25f, 8.0f, 4.0f), 103.0f);
}

TEST(BoundFit, LocalOfMatchesTheBoxFrameUnderMapRotation) {
    // A 45°-fitted box viewed at map rotation 1: the model-space point
    // (1000·c, 1000·s) lands at R1(p) = (1000·s, -1000·c) and must read back
    // as the pure u coordinate.
    const float c = c2bound::kSqrt1_2;
    const float s = c2bound::kSqrt1_2;
    float qx, qy;
    c2bound::LocalOf(1000.0f * s, -1000.0f * c, 0.0f, 0.0f, 0.0f, 0.0f, c, s, 1, qx, qy);
    EXPECT_NEAR(qx, 1000.0f, 0.01f);
    EXPECT_NEAR(qy, 0.0f, 0.01f);

    // Local coordinates are relative to the box centre.
    c2bound::LocalOf(300.0f, 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 1.0f, 0.0f, 0, qx, qy);
    EXPECT_FLOAT_EQ(qx, 200.0f);
    EXPECT_FLOAT_EQ(qy, 0.0f);
}

TEST(BoundFit, TopPlaneIgnoresUnderSurfaceFringeOfATiltedSlab) {
    // A 30°-pitched slab with top AND bottom faces. The bottom face's
    // projection fringes past the top at the high end; without the trimmed
    // fit those cells rejected every slab that has a bottom face.
    const float c30 = std::cos(30.0f * kDeg2Rad);
    const float s30 = std::sin(30.0f * kDeg2Rad);
    const float L = 1500.0f, W = 400.0f, T = 96.0f;
    std::vector<float> us, vs, ys;
    const float yRows[2] = {0.0f, T};
    for (float yLocal : yRows) {
        for (float sx : {-1.0f, 1.0f}) {
            for (float sz : {-1.0f, 1.0f}) {
                const float x = sx * L * 0.5f;
                us.push_back(x * c30 - yLocal * s30);
                vs.push_back(sz * W * 0.5f);
                ys.push_back(x * s30 + yLocal * c30);
            }
        }
    }
    const c2bound::TopPlane p = c2bound::FitTopPlane(
        us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
        c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
    ASSERT_TRUE(p.ok);
    EXPECT_NEAR(p.gx, std::tan(30.0f * kDeg2Rad), 1e-3f);
    EXPECT_NEAR(p.gz, 0.0f, 1e-3f);
}

TEST(BoundFit, TopPlaneFitsTessellatedTiltedSlab) {
    // Same slab tessellated on both faces: the under-surface fringe is a
    // full bucket row here; the trimmed fit must still recover the plane.
    const float c30 = std::cos(30.0f * kDeg2Rad);
    const float s30 = std::sin(30.0f * kDeg2Rad);
    const float L = 1500.0f, W = 400.0f, T = 96.0f;
    std::vector<float> us, vs, ys;
    const float yRows[2] = {0.0f, T};
    for (float yLocal : yRows) {
        for (int i = 0; i <= 16; ++i) {
            for (int j = 0; j <= 4; ++j) {
                const float x = -L * 0.5f + L * static_cast<float>(i) / 16.0f;
                us.push_back(x * c30 - yLocal * s30);
                vs.push_back(-W * 0.5f + W * static_cast<float>(j) / 4.0f);
                ys.push_back(x * s30 + yLocal * c30);
            }
        }
    }
    const c2bound::TopPlane p = c2bound::FitTopPlane(
        us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
        c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
    ASSERT_TRUE(p.ok);
    EXPECT_NEAR(p.gx, std::tan(30.0f * kDeg2Rad), 1e-3f);
    EXPECT_NEAR(p.gz, 0.0f, 1e-3f);
}

TEST(BoundFit, TopPlaneRejectsCurvedDome) {
    // A hemisphere-like top: trimming cannot rescue it, and the majority
    // guard / residual gate must reject it so organic parts stay flat.
    const float R = 300.0f, span = 1200.0f;
    std::vector<float> us, vs, ys;
    for (int i = 0; i <= 24; ++i) {
        for (int j = 0; j <= 24; ++j) {
            const float nx = static_cast<float>(i) / 24.0f - 0.5f;
            const float nz = static_cast<float>(j) / 24.0f - 0.5f;
            const float rn = std::min(1.0f, std::sqrt(nx * nx + nz * nz) * 2.0f / 0.9f);
            us.push_back(nx * span);
            vs.push_back(nz * span);
            ys.push_back(std::sqrt(std::max(0.0f, 1.0f - rn * rn)) * R);
        }
    }
    const c2bound::TopPlane p = c2bound::FitTopPlane(
        us.data(), vs.data(), ys.data(), static_cast<int>(us.size()),
        c2bound::kSlopeBucket, c2bound::kSlopeMaxResidual, c2bound::kSlopeMaxGrade);
    EXPECT_FALSE(p.ok);
}
