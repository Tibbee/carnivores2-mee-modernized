#pragma once
// ==========================================================================
// BoundFit.h -- pure 2D oriented-footprint math for ofBOUND collision boxes
// ==========================================================================
// Used by CalcBoundBox (fit at load) and by the three bound consumers for
// fitted boxes. Header-only and engine-state free so the rules stay
// unit-testable (same pattern as Core/ConfigParse.h).
//
// Coordinates: model space (x, z). The engine's map rotation for quarter-turn
// index r = (FMap >> 2) & 3 maps model -> world with the exact {1,0,-1,0}
// LUT Math.cpp's kRotCos/kRotSin use:
//   x' = x*ca + z*sa,   z' = z*ca - x*sa.
// A fitted box stores its u axis (cos, sin) in model space; v is the
// perpendicular (-sin, cos). The wall test keeps CheckBoundCollision's
// semantics: +2 margin on both extents, least-penetration push, SGN never 0.

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace c2bound {

// CalcBoundBox pads X/Z extents by 72 per side; the fit pads the same way.
inline constexpr float kPad = 72.0f;
// cos(45°)/sin(45°) with the same literal Math.cpp's kOctCos/kOctSin use.
inline constexpr float kSqrt1_2 = 0.70710678f;

// A part's oriented footprint box in model space. Half extents include the
// load-time padding; the wall test adds its own +2 margin.
struct FitBox {
    float cx = 0.0f, cz = 0.0f;    // centre (TBound cx / cy)
    float a = 0.0f, b = 0.0f;      // half extents along u / v
    float cos = 1.0f, sin = 0.0f;  // u axis; v = (-sin, cos); (1,0) = legacy box
    float Area() const { return 4.0f * a * b; }
};

// Smallest enclosing rectangle of the point set at the orientation given by
// the unit axis (c, s); both padded extents inflated by pad.
inline FitBox FitBoxAlong(const float* xs, const float* zs, int n, float c, float s, float pad) {
    if (n <= 0) return FitBox{};
    float u1 = FLT_MAX, u2 = -FLT_MAX, v1 = FLT_MAX, v2 = -FLT_MAX;
    for (int i = 0; i < n; ++i) {
        const float u = xs[i] * c + zs[i] * s;
        const float v = -xs[i] * s + zs[i] * c;
        if (u < u1) u1 = u;
        if (u > u2) u2 = u;
        if (v < v1) v1 = v;
        if (v > v2) v2 = v;
    }
    const float cu = 0.5f * (u1 + u2);
    const float cv = 0.5f * (v1 + v2);
    FitBox box;
    box.cx = cu * c - cv * s;
    box.cz = cu * s + cv * c;
    box.a = 0.5f * (u2 - u1) + pad;
    box.b = 0.5f * (v2 - v1) + pad;
    box.cos = c;
    box.sin = s;
    return box;
}

// Best of the axis-aligned (0°) and diagonal (45°) candidates; a rectangle's
// orientation is defined mod 90°, so those two cover the exact baked cases.
// Ties keep 0°, keeping unrotated parts identical to the legacy box.
inline FitBox FitCandidates(const float* xs, const float* zs, int n, float pad) {
    if (n <= 0) return FitBox{};
    const FitBox axis = FitBoxAlong(xs, zs, n, 1.0f, 0.0f, pad);
    const FitBox diag = FitBoxAlong(xs, zs, n, kSqrt1_2, kSqrt1_2, pad);
    return (diag.Area() < axis.Area()) ? diag : axis;
}

// Min-area enclosing rectangle (convex hull + hull-edge directions). Exact
// for any convex footprint at any baked angle; falls back to the candidates
// when the hull is degenerate (fewer than three distinct points).
inline FitBox FitMinArea(const float* xs, const float* zs, int n, float pad) {
    if (n <= 0) return FitBox{};
    std::vector<int> idx(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) idx[static_cast<size_t>(i)] = i;
    std::sort(idx.begin(), idx.end(), [&](int A, int B) {
        if (xs[A] != xs[B]) return xs[A] < xs[B];
        return zs[A] < zs[B];
    });
    const auto cross = [&](int O, int A, int B) {
        return (xs[A] - xs[O]) * (zs[B] - zs[O]) - (zs[A] - zs[O]) * (xs[B] - xs[O]);
    };
    std::vector<int> lower;
    for (int i = 0; i < n; ++i) {
        const int pt = idx[static_cast<size_t>(i)];
        while (lower.size() >= 2 && cross(lower[lower.size() - 2], lower[lower.size() - 1], pt) <= 0.0f)
            lower.pop_back();
        lower.push_back(pt);
    }
    std::vector<int> upper;
    for (int i = n - 1; i >= 0; --i) {
        const int pt = idx[static_cast<size_t>(i)];
        while (upper.size() >= 2 && cross(upper[upper.size() - 2], upper[upper.size() - 1], pt) <= 0.0f)
            upper.pop_back();
        upper.push_back(pt);
    }
    lower.pop_back();
    upper.pop_back();
    std::vector<int> hull = lower;
    hull.insert(hull.end(), upper.begin(), upper.end());
    if (hull.size() < 3) return FitCandidates(xs, zs, n, pad);

    FitBox best;
    bool have = false;
    for (size_t i = 0; i < hull.size(); ++i) {
        const int A = hull[i];
        const int B = hull[(i + 1) % hull.size()];
        float dx = xs[B] - xs[A];
        float dz = zs[B] - zs[A];
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-6f) continue;
        dx /= len;
        dz /= len;
        const FitBox box = FitBoxAlong(xs, zs, n, dx, dz, pad);
        if (!have || box.Area() < best.Area() - 1e-9f) {
            best = box;
            have = true;
        }
    }
    return have ? best : FitCandidates(xs, zs, n, pad);
}

// Accept a fit only when its padded area beats the padded AABB area by at
// least minWinPercent; keeps axis-aligned and organic parts on the legacy box.
inline bool FitAccepts(float fitArea, float baseArea, int minWinPercent) {
    if (baseArea <= 0.0f) return false;
    const float win = (baseArea - fitArea) / baseArea;
    return win >= static_cast<float>(minWinPercent) * 0.01f;
}

// ===================== runtime tests for fitted boxes =====================

inline void MapRotate(float x, float z, int rotIdx, float& outX, float& outZ) {
    const float rc[4] = {1.0f, 0.0f, -1.0f, 0.0f};
    const float rs[4] = {0.0f, 1.0f, 0.0f, -1.0f};
    const float ca = rc[rotIdx & 3];
    const float sa = rs[rotIdx & 3];
    outX = x * ca + z * sa;
    outZ = z * ca - x * sa;
}

// CheckBoundCollision's ofBOUND branch for a fitted box: push px/pz out of
// the box along the least-penetration axis, all computed in the box's frame.
// Returns whether the point was inside (and pushed).
inline bool WallPushOriented(float& px, float& pz, float cellX, float cellZ,
                             const FitBox& box, int rotIdx, float margin) {
    float uwx, uwz, vwx, vwz, ccx, ccz;
    MapRotate(box.cos, box.sin, rotIdx, uwx, uwz);
    MapRotate(-box.sin, box.cos, rotIdx, vwx, vwz);
    MapRotate(box.cx, box.cz, rotIdx, ccx, ccz);

    const float dx = (px - cellX) - ccx;
    const float dz = (pz - cellZ) - ccz;
    const float qx = dx * uwx + dz * uwz;
    const float qy = dx * vwx + dz * vwz;
    const float w = box.a + margin;
    const float h = box.b + margin;
    const float dw = std::fabs(qx) - w;
    const float dh = std::fabs(qy) - h;
    if (dw > 0.0f || dh > 0.0f) return false;

    if (dw > dh) {
        const float t = (qx < 0.0f ? -w : w);
        px = cellX + ccx + t * uwx + qy * vwx;
        pz = cellZ + ccz + t * uwz + qy * vwz;
    } else {
        const float t = (qy < 0.0f ? -h : h);
        px = cellX + ccx + qx * uwx + t * vwx;
        pz = cellZ + ccz + qx * uwz + t * vwz;
    }
    return true;
}

// PointOnBound / PointUnBound containment for a fitted box: the query is
// tested in the box's frame against the raw half extents (no +2 margin),
// matching those functions' legacy |dx| < a tests.
inline bool OrientedContains(float px, float pz, float cellX, float cellZ,
                             const FitBox& box, int rotIdx) {
    float uwx, uwz, vwx, vwz, ccx, ccz;
    MapRotate(box.cos, box.sin, rotIdx, uwx, uwz);
    MapRotate(-box.sin, box.cos, rotIdx, vwx, vwz);
    MapRotate(box.cx, box.cz, rotIdx, ccx, ccz);
    const float dx = (px - cellX) - ccx;
    const float dz = (pz - cellZ) - ccz;
    const float qx = dx * uwx + dz * uwz;
    const float qy = dx * vwx + dz * vwz;
    return std::fabs(qx) < box.a && std::fabs(qy) < box.b;
}

}  // namespace c2bound
