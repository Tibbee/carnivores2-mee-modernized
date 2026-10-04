#include <gtest/gtest.h>
#include "TerrainSweep.h"
#include <array>

namespace {
constexpr int kSize = 512;
constexpr int kCenter = 256;
using Rows = std::array<terrain_sweep::RowSpan, kSize>;

// Independently enumerate the original RenderGround paired-row loop, not
// visitSpans, to guard the extraction as well as the footprint projection.
void checkReads(const terrain_sweep::Bounds& b, const Rows& rows) {
    const int yLo = (std::max)(0, static_cast<int>(std::ceil(b.ccy + b.residualY - b.radius)));
    const int yHi = (std::min)(b.mapSize - 1, static_cast<int>(std::floor(b.ccy + b.residualY + b.radius)));
    int y = yLo;
    auto check = [&](int left, int right, int width) {
        for (int x = left; x <= right; x += width) {
            // Includes the map-edge 1x1 fallbacks. Invalid map vertices are
            // never read by either collector.
            for (int j = 0; j <= width && y + j < b.mapSize; ++j) {
                for (int i = 0; i <= width && x + i < b.mapSize; ++i) {
                    const int gy = y + j - b.ccy + kCenter;
                    ASSERT_GE(gy, 0); ASSERT_LT(gy, kSize);
                    ASSERT_TRUE(rows[gy].contains(x + i - b.ccx))
                        << "vertex " << x + i - b.ccx << "," << y + j - b.ccy;
                }
            }
        }
    };
    for (; y + 1 <= yHi; y += 2) {
        float lo0, hi0, lo1, hi1;
        const bool ok0 = b.rowRange(y, lo0, hi0);
        const bool ok1 = b.rowRange(y + 1, lo1, hi1);
        if (!ok0 && !ok1) continue;
        float lo = ok0 ? lo0 : lo1, hi = ok0 ? hi0 : hi1;
        if (ok0 && ok1) { lo = (std::min)(lo, lo1); hi = (std::max)(hi, hi1); }
        check((std::max)(0, b.ccx + static_cast<int>(std::floor(lo))),
              (std::min)(b.mapSize - 1, b.ccx + static_cast<int>(std::ceil(hi))), 2);
    }
    if (y <= yHi) {
        float lo, hi;
        if (b.rowRange(y, lo, hi)) {
            check((std::max)(0, b.ccx + static_cast<int>(std::floor(lo))),
                  (std::min)(b.mapSize - 1, b.ccx + static_cast<int>(std::ceil(hi))), 1);
        }
    }
}
}

TEST(TerrainSweep, CoversPairedRowsAndCornersDuringCameraMotion) {
    // All requested radii, cardinal/near-parallel directions, steep pitch,
    // wide/narrow FOV, map edges, and both sides of the even-cell CCX snap.
    for (int r : {73, 196, 230}) {
        for (int origin : {0, 512, 1022}) {
            for (float residual : {-0.5f, 0.0f, 1.499f}) {
                for (float yaw : {0.0f, 0.7853982f, 1.5707963f, 2.3561945f, 3.1415927f, 4.7123890f}) {
                    for (float pitch : {-1.4f, -0.2f, 0.0f, 0.2f, 1.4f}) {
                        for (float fov : {0.5f, 1.0f, 2.0f}) {
                            terrain_sweep::Bounds b(origin, origin, 1024, r,
                                (origin + 0.5f + residual) * 256.0f,
                                (origin + 0.5f + residual) * 256.0f, 2048.0f, 32,
                                std::cos(yaw), std::sin(yaw), std::cos(pitch), std::sin(pitch), fov, 1800.0f);
                            Rows rows{};
                            terrain_sweep::includeReadFootprints(b, rows, kCenter);
                            checkReads(b, rows);
                        }
                    }
                }
            }
        }
    }
}

TEST(TerrainSweep, RejectedVertexRowStillNeedsCorners) {
    terrain_sweep::Bounds b(512, 512, 1024, 230,
        512.5f * 256.0f, 512.5f * 256.0f, 2048.0f, 32,
        -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 320.0f);
    Rows rows{};
    terrain_sweep::includeReadFootprints(b, rows, kCenter);
    float lo, hi;
    ASSERT_FALSE(b.rowRange(743, lo, hi)); // outside the disk
    EXPECT_FALSE(rows[kCenter + 231].empty()); // corner of the last chunk
    // Old sweep's same-row disk term was empty here regardless of +2.5 X
    // slack. The read footprint must be propagated vertically instead.
}

TEST(TerrainSweep, MovingAcrossSnapCanReadPastOldSquare) {
    terrain_sweep::Bounds b(512, 512, 1024, 230,
        513.999f * 256.0f, 513.999f * 256.0f, 2048.0f, 32,
        0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 320.0f);
    Rows rows{};
    terrain_sweep::includeReadFootprints(b, rows, kCenter);
    bool pastOldSquare = false;
    for (const auto& row : rows) {
        if (!row.empty() && row.hi > 232) pastOldSquare = true;
    }
    EXPECT_TRUE(pastOldSquare);
    checkReads(b, rows);
}

TEST(TerrainSweep, RowSpanUnionsEmptyAndDisjointRanges) {
    terrain_sweep::RowSpan span;
    span.include(4, 3);
    EXPECT_TRUE(span.empty());
    span.include(-4, -2);
    span.include(1, 9);
    EXPECT_EQ(span.lo, -4);
    EXPECT_EQ(span.hi, 9);
}
