#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include "Constants.h"

// Shared by the GL terrain walk and ground precache. Bounds describe tile
// CENTERS; the caller must expand visited chunks to their vertex footprints.
namespace terrain_sweep {
struct RowSpan {
    int lo = 1;
    int hi = 0;

    bool empty() const { return lo > hi; }
    bool contains(int x) const { return x >= lo && x <= hi; }
    void include(int left, int right) {
        if (left > right) return;
        if (empty()) { lo = left; hi = right; }
        else { lo = (std::min)(lo, left); hi = (std::max)(hi, right); }
    }
};

struct Bounds {
    int ccx, ccy, mapSize, radius;
    float residualX, residualY;
    float ar, br, al, bl, cr;

    Bounds(int x, int y, int size, int r, float cameraX, float cameraZ,
           float cameraY, int heightScale, float ca, float sa, float cb,
           float sb, float fovk, float backRadius)
        : ccx(x), ccy(y), mapSize(size), radius(r),
          residualX(cameraX / 256.0f - (static_cast<float>(x) + 0.5f)),
          residualY(cameraZ / 256.0f - (static_cast<float>(y) + 0.5f)),
          ar(ca * fovk - sa * cb), br(sa * fovk + ca * cb),
          al(ca * fovk + sa * cb), bl(sa * fovk - ca * cb)
    {
        const float wyMin = -cameraY;
        const float wyMax = 255.0f * static_cast<float>(heightScale) - cameraY;
        const float wyEff = sb > 0.0f ? wyMin : (sb < 0.0f ? wyMax : 0.0f);
        cr = (backRadius - wyEff * sb) / 256.0f;
    }

    bool rowRange(int mapY, float& lo, float& hi) const {
        const float dy = static_cast<float>(mapY - ccy) - residualY;
        const float d2 = static_cast<float>(radius) * static_cast<float>(radius) - dy * dy;
        if (d2 < 0.0f) return false;
        const float d = std::sqrt((std::max)(0.0f, d2));
        lo = -d; hi = d;
        const float rightSlack = 3.0f * (std::fabs(ar) + std::fabs(br));
        const float leftSlack = 3.0f * (std::fabs(al) + std::fabs(bl));
        const float expandedCr = cr + rightSlack;
        const float rhsR = expandedCr - br * dy;
        if (ar > 1e-12f) hi = (std::min)(hi, rhsR / ar);
        else if (ar < -1e-12f) lo = (std::max)(lo, rhsR / ar);
        else if (br * dy > expandedCr) return false;
        const float expandedCl = -cr - leftSlack;
        const float rhsL = expandedCl - bl * dy;
        if (al > 1e-12f) lo = (std::max)(lo, rhsL / al);
        else if (al < -1e-12f) hi = (std::min)(hi, rhsL / al);
        else if (bl * dy < expandedCl) return false;
        if (lo > hi) return false;
        lo += residualX;
        hi += residualX;
        return true;
    }

    // Exactly the renderer's visited spans, including paired-row unions,
    // rounding, map clipping and the odd-row fallback. rows is 1 or 2.
    template<class Visitor>
    void visitSpans(Visitor visit) const {
        const float viewRadius = static_cast<float>(radius);
        const int yLo = (std::max)(0, static_cast<int>(std::ceil(
            static_cast<float>(ccy) + residualY - viewRadius)));
        const int yHi = (std::min)(mapSize - 1, static_cast<int>(std::floor(
            static_cast<float>(ccy) + residualY + viewRadius)));
        for (int y = yLo; y <= yHi; y += 2) {
            const int rows = y + 1 <= yHi ? 2 : 1;
            float lo0 = 0.f, hi0 = 0.f, lo1 = 0.f, hi1 = 0.f;
            const bool ok0 = rowRange(y, lo0, hi0);
            const bool ok1 = rows == 2 && rowRange(y + 1, lo1, hi1);
            if (!ok0 && !ok1) continue;
            float lo = ok0 ? lo0 : lo1;
            float hi = ok0 ? hi0 : hi1;
            if (ok0 && ok1) {
                lo = (std::min)(lo, lo1);
                hi = (std::max)(hi, hi1);
            }
            const int left = (std::max)(0, ccx + static_cast<int>(std::floor(lo)));
            const int right = (std::min)(mapSize - 1, ccx + static_cast<int>(std::ceil(hi)));
            if (left <= right) visit(y, left, right, rows);
        }
    }
};

// A 2x2 chunk reads a 3x3 vertex grid. Its last origin can equal right,
// so right+2 (not right+1) is needed when the span length is odd. Project
// the complete footprint onto ALL affected rows, including rows which
// have no rowRange of their own. The 1x1 fallback needs two vertex rows.
template<class Rows>
void includeReadFootprints(const Bounds& bounds, Rows& out, int gridCenter) {
    bounds.visitSpans([&](int y, int left, int right, int rows) {
        const int lastOrigin = left + ((right - left) / rows) * rows;
        const int vertexRight = (std::min)(bounds.mapSize - 1, lastOrigin + rows);
        for (int j = 0; j <= rows && y + j < bounds.mapSize; ++j) {
            const int gridY = y + j - bounds.ccy + gridCenter;
            if (gridY >= 0 && gridY < static_cast<int>(out.size())) {
                out[gridY].include(left - bounds.ccx, vertexRight - bounds.ccx);
            }
        }
    });
}
#ifdef GL_PERF_HOOKS
// The moving-camera capture gate checks the actual refreshed rows before
// every camera-space grid read. Disabled in Shipping and fixed-camera perf
// runs, so it cannot hide staleness behind visually plausible screenshots.
inline std::array<RowSpan, kViewGridSize> refreshedRows{};
inline bool footprintRefreshed(int gridX, int gridY, int width) {
    for (int j = 0; j <= width; ++j) {
        if (!refreshedRows[gridY + j].contains(gridX - kViewGridCenter) ||
            !refreshedRows[gridY + j].contains(gridX + width - kViewGridCenter)) return false;
    }
    return true;
}
#endif
} // namespace terrain_sweep
