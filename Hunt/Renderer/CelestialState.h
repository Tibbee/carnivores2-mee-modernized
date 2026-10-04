#pragma once

#include "Core/MathTypes.h"
#include <cmath>
#include <cstdint>
#include <limits>

struct CelestialProjection {
    int x = -1;
    int y = -1;
    bool valid = false;
};

// Preserve the legacy forward-depth threshold and offset truncation. A valid
// projection may be off-screen: its halo can naturally fade at the screen edge.
// Invalid/behind-camera directions must not reuse a previous screen position.
inline CelestialProjection ProjectCelestialBody(const Vector3d& direction,
    int centerX, int centerY, float cameraW, float cameraH) {
    if (!(direction.z < -2024.0f) || !std::isfinite(direction.z) ||
        !(cameraW > 0.0f) || !(cameraH > 0.0f)) return {};
    // Reject malformed/extreme inputs before float-to-int conversion. The
    // double check is a coarse safety bound, not the projection calculation.
    constexpr double limit = 1073741824.0; // Safe int offset with rounding headroom.
    const double dx = static_cast<double>(direction.x) / -direction.z * cameraW;
    const double dy = static_cast<double>(direction.y) / -direction.z * cameraH;
    if (!std::isfinite(dx) || !std::isfinite(dy) ||
        std::fabs(dx) > limit || std::fabs(dy) > limit) return {};
    const std::int64_t x = static_cast<std::int64_t>(centerX) +
        static_cast<int>(direction.x / (-direction.z) * cameraW);
    const std::int64_t y = static_cast<std::int64_t>(centerY) -
        static_cast<int>(direction.y / (-direction.z) * cameraH);
    if (x < (std::numeric_limits<int>::min)() || x > (std::numeric_limits<int>::max)() ||
        y < (std::numeric_limits<int>::min)() || y > (std::numeric_limits<int>::max)()) return {};
    return {static_cast<int>(x), static_cast<int>(y), true};
}

inline float CelestialVisibilityFromSkySample(int timeOfDay, float rawVisibility) {
    // Original night remap, also used to derive a consistent clear-sky seed.
    return timeOfDay == 2 ? 0.12f + rawVisibility / 5.0f : rawVisibility;
}

inline float CelestialInitialVisibility(int timeOfDay) {
    return CelestialVisibilityFromSkySample(timeOfDay, 1.0f);
}

inline float CelestialHaloVisibility(bool projectionValid, float sampledVisibility) {
    return projectionValid ? sampledVisibility : 0.0f;
}
