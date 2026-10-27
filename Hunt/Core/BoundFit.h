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
// cos(45°)/sin(45°) for the diagonal fit-orientation candidate.
inline constexpr float kSqrt1_2 = 0.70710678f;

// Top-plane (slope) fit limits: envelope sampling bucket (model units), the
// near-planar tolerance, the steepest accepted grade (tan 60°) and the grade
// below which a fit collapses back to the legacy flat top.
inline constexpr float kSlopeBucket = 48.0f;
inline constexpr float kSlopeMaxResidual = 16.0f;
inline constexpr float kSlopeMaxGrade = 1.7320508f;
inline constexpr float kSlopeMinGrade = 0.01f;

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

// Smallest padded rectangle among 0°/45° and convex-hull-edge directions.
// Recovers any baked rectangle angle. Seed the search with 0°/45° so its
// padded area is never worse than either candidate, including degenerate
// hulls. Ties retain the seed (0° first), preserving axis-aligned boxes.
inline FitBox FitMinArea(const float* xs, const float* zs, int n, float pad) {
    if (n <= 0) return FitBox{};
    FitBox best = FitBoxAlong(xs, zs, n, 1.0f, 0.0f, pad);
    const FitBox diag = FitBoxAlong(xs, zs, n, kSqrt1_2, kSqrt1_2, pad);
    if (diag.Area() < best.Area()) best = diag;
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
    // A two-point hull is a segment: test its direction just like a polygon
    // edge instead of restricting collinear footprints to the 0°/45° seeds.
    // Single points and zero-length duplicate edges retain the safe seed.
    if (hull.size() < 2) return best;
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
        if (box.Area() < best.Area() - 1e-9f) {
            best = box;
        }
    }
    return best;
}

// Accept a fit only when its padded area beats the padded AABB area by at
// least minWinPercent; keeps axis-aligned and organic parts on the legacy box.
inline bool FitAccepts(float fitArea, float baseArea, int minWinPercent) {
    if (baseArea <= 0.0f) return false;
    const float win = (baseArea - fitArea) / baseArea;
    return win >= static_cast<float>(minWinPercent) * 0.01f;
}

// A part's top surface, fitted as the plane y = c0 + gx*u + gz*v in the
// footprint box's local frame (u = cosph/sinph axis, v = perpendicular).
struct TopPlane {
    float c0 = 0.0f;  // plane height at local (0, 0)
    float gx = 0.0f;  // rise per unit along u
    float gz = 0.0f;  // rise per unit along v
    bool ok = false;  // false = keep the legacy flat top at max y
};

// Fit the top envelope of a part. us/vs are the part's vertices in the box's
// local frame, ys the model heights. Every (u, v) grid cell contributes only
// its highest vertex, so under-surfaces cannot drag the plane down. Cells
// that sit clearly below the current plane are then dropped and the fit is
// repeated: those are cells covered only by an under-surface (a slab's
// bottom face fringes past the top in projection) and they must not poison
// the plane. Above-plane samples are never dropped, so spikes still fail the
// residual gate. Accepted only when a majority of the envelope stays within
// maxResidual of the plane and the grade is within maxGrade; degenerate
// (collinear) samples are rejected, keeping organic and vertical parts flat.
inline TopPlane FitTopPlane(const float* us, const float* vs, const float* ys, int n,
                            float bucket, float maxResidual, float maxGrade) {
    TopPlane r;
    if (n < 3) return r;

    float u1 = FLT_MAX, u2 = -FLT_MAX, v1 = FLT_MAX, v2 = -FLT_MAX;
    for (int i = 0; i < n; ++i) {
        if (us[i] < u1) u1 = us[i];
        if (us[i] > u2) u2 = us[i];
        if (vs[i] < v1) v1 = vs[i];
        if (vs[i] > v2) v2 = vs[i];
    }
    const float uSpan = u2 - u1;
    const float vSpan = v2 - v1;

    const int nbU = std::max(1, std::min(32, static_cast<int>(uSpan / bucket + 0.5f) + 1));
    const int nbV = std::max(1, std::min(32, static_cast<int>(vSpan / bucket + 0.5f) + 1));
    std::vector<float> topY(static_cast<size_t>(nbU) * nbV, -FLT_MAX);
    std::vector<float> topU(static_cast<size_t>(nbU) * nbV, 0.0f);
    std::vector<float> topV(static_cast<size_t>(nbU) * nbV, 0.0f);
    for (int i = 0; i < n; ++i) {
        const int iu = (uSpan > 0.0f) ? std::min(nbU - 1, static_cast<int>((us[i] - u1) / uSpan * nbU)) : 0;
        const int iv = (vSpan > 0.0f) ? std::min(nbV - 1, static_cast<int>((vs[i] - v1) / vSpan * nbV)) : 0;
        const size_t k = static_cast<size_t>(iu) * nbV + iv;
        if (ys[i] > topY[k]) {
            topY[k] = ys[i];
            topU[k] = us[i];
            topV[k] = vs[i];
        }
    }

    std::vector<unsigned char> keep(topY.size(), 0);
    int kept = 0;
    for (size_t k = 0; k < topY.size(); ++k) {
        if (topY[k] != -FLT_MAX) {
            keep[k] = 1;
            ++kept;
        }
    }
    const int total = kept;
    if (kept < 3) return r;

    double c0 = 0.0, gx = 0.0, gz = 0.0;
    for (int pass = 0; ; ++pass) {
        double sumN = 0.0, su = 0.0, sv = 0.0, sy = 0.0;
        for (size_t k = 0; k < topY.size(); ++k) {
            if (!keep[k]) continue;
            sumN += 1.0;
            su += topU[k];
            sv += topV[k];
            sy += topY[k];
        }
        if (sumN < 3.0) return r;

        const double mu = su / sumN;
        const double mv = sv / sumN;
        const double my = sy / sumN;
        double cuu = 0.0, cuv = 0.0, cvv = 0.0, cuy = 0.0, cvy = 0.0;
        for (size_t k = 0; k < topY.size(); ++k) {
            if (!keep[k]) continue;
            const double du = topU[k] - mu;
            const double dv = topV[k] - mv;
            const double dy = topY[k] - my;
            cuu += du * du;
            cuv += du * dv;
            cvv += dv * dv;
            cuy += du * dy;
            cvy += dv * dy;
        }
        const double det = cuu * cvv - cuv * cuv;
        // Collinear samples leave the gradient underdetermined; the relative
        // guard keeps large decks solvable and degenerate slivers flat.
        if (det < 1e-9 * (cuu + cvv + 1.0)) return r;
        gx = (cuy * cvv - cvy * cuv) / det;
        gz = (cvy * cuu - cuy * cuv) / det;
        c0 = my - gx * mu - gz * mv;

        if (pass >= 3) break;
        bool dropped = false;
        for (size_t k = 0; k < topY.size(); ++k) {
            if (!keep[k]) continue;
            const double resid = topY[k] - (c0 + gx * topU[k] + gz * topV[k]);
            if (resid < -maxResidual) {
                keep[k] = 0;
                --kept;
                dropped = true;
            }
        }
        if (!dropped) break;
    }

    if (kept < 3) return r;
    // A real top surface stays majority-represented after trimming; if most
    // of the envelope had to go, the top is not planar enough to trust.
    if (kept * 2 < total) return r;

    const double grade = std::sqrt(gx * gx + gz * gz);
    if (grade > maxGrade) return r;

    for (size_t k = 0; k < topY.size(); ++k) {
        if (!keep[k]) continue;
        const double fit = c0 + gx * topU[k] + gz * topV[k];
        if (std::fabs(topY[k] - fit) > maxResidual) return r;
    }

    r.c0 = static_cast<float>(c0);
    r.gx = static_cast<float>(gx);
    r.gz = static_cast<float>(gz);
    r.ok = true;
    return r;
}

// Fit the lower envelope using the identical rules reflected in Y. Highest
// reflected vertices are the original lowest vertices; trimmed top-face
// fringes cannot pull the underside up, while downward protrusions still
// fail the residual gate. No new model data is needed.
inline TopPlane FitBottomPlane(const float* us, const float* vs, const float* ys, int n,
                               float bucket, float maxResidual, float maxGrade) {
    if (n < 3) return TopPlane{};
    std::vector<float> reflected(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) reflected[static_cast<size_t>(i)] = -ys[i];
    TopPlane r = FitTopPlane(us, vs, reflected.data(), n, bucket, maxResidual, maxGrade);
    r.c0 = -r.c0;
    r.gx = -r.gx;
    r.gz = -r.gz;
    return r;
}

// Heights at the footprint centre plus independent gradients along u/v.
// Zero gradients retain the original flat limits, including failed fits.
struct SlopedBounds {
    float y1, y2;
    float gx = 0.0f, gz = 0.0f;
    float bottomGx = 0.0f, bottomGz = 0.0f;
};

// A linear top-minus-bottom difference reaches its minimum at a rectangle
// corner. Check the entire padded footprint, not just its centre, before
// accepting an underside; independently fitted planes must never cross.
inline bool PlanesOrdered(float top, float topGx, float topGz,
                          float bottom, float bottomGx, float bottomGz,
                          float a, float b) {
    const double gap = static_cast<double>(top) - bottom;
    const double variation = std::fabs(static_cast<double>(topGx) - bottomGx) * a
                           + std::fabs(static_cast<double>(topGz) - bottomGz) * b;
    return gap >= variation;
}

// Producer shared by CalcBoundBox and tests. us/vs are absolute model
// coordinates in the chosen box frame; centreU/V recenter each fitted plane.
// Keep the existing top acceptance unchanged. Accept a lower plane
// independently only when gentle/non-flat and below the retained top over
// the padded footprint; otherwise keep the legacy minimum y1.
inline SlopedBounds FitSlopedBounds(const float* us, const float* vs, const float* ys, int n,
                                    float centreU, float centreV, float a, float b,
                                    float legacyBottom, float legacyTop) {
    SlopedBounds r{legacyBottom, legacyTop};
    const TopPlane top = FitTopPlane(us, vs, ys, n, kSlopeBucket, kSlopeMaxResidual, kSlopeMaxGrade);
    if (top.ok && std::sqrt(top.gx * top.gx + top.gz * top.gz) >= kSlopeMinGrade) {
        r.y2 = top.c0 + top.gx * centreU + top.gz * centreV;
        r.gx = top.gx;
        r.gz = top.gz;
    }
    const TopPlane bottom = FitBottomPlane(us, vs, ys, n, kSlopeBucket, kSlopeMaxResidual, kSlopeMaxGrade);
    if (bottom.ok && std::sqrt(bottom.gx * bottom.gx + bottom.gz * bottom.gz) >= kSlopeMinGrade) {
        const float y1 = bottom.c0 + bottom.gx * centreU + bottom.gz * centreV;
        if (PlanesOrdered(r.y2, r.gx, r.gz, y1, bottom.gx, bottom.gz, a, b)) {
            r.y1 = y1;
            r.bottomGx = bottom.gx;
            r.bottomGz = bottom.gz;
        }
    }
    return r;
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

// Signed local coordinates of a world point in the box's frame (u along
// (cos, sin), v perpendicular), composed with the map rotation: the same
// coordinates the oriented wall/containment code uses. Sloped boxes evaluate
// their top plane through this so the surface lives in the fit's frame.
inline void LocalOf(float px, float pz, float cellX, float cellZ,
                    float boxX, float boxZ, float cosA, float sinA, int rotIdx,
                    float& qx, float& qy) {
    float uwx, uwz, vwx, vwz, ccx, ccz;
    MapRotate(cosA, sinA, rotIdx, uwx, uwz);
    MapRotate(-sinA, cosA, rotIdx, vwx, vwz);
    MapRotate(boxX, boxZ, rotIdx, ccx, ccz);
    const float dx = (px - cellX) - ccx;
    const float dz = (pz - cellZ) - ccz;
    qx = dx * uwx + dz * uwz;
    qy = dx * vwx + dz * vwz;
}

// Height of a (possibly sloped) top at local (qx, qy). y2 is the surface
// height at the box centre; gx/gz the gradient along u/v ((0, 0) = flat top).
inline float TopAt(float y2, float gx, float gz, float qx, float qy) {
    return y2 + gx * qx + gz * qy;
}

// Underside height at the same local coordinates as the top. y1 is the
// bottom's centre height when fitted, otherwise the legacy minimum.
inline float BottomAt(float y1, float gx, float gz, float qx, float qy) {
    return y1 + gx * qx + gz * qy;
}

// Existing player step/head gates, now supplied with position-dependent
// heights on sloped boxes. Equality continues to count as overlap.
inline bool WallHeightOverlaps(float top, float bottom, float objectY, float playerY) {
    return !(top + objectY < playerY + 128.0f || bottom + objectY > playerY + 256.0f);
}

inline float ClampF(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
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
