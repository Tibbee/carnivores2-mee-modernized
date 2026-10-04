#pragma once

// Preserve the legacy sun-sample margin. Depth is sampled at frame start
// using the previous projection, but glare must also respect the CURRENT
// projection when it is applied. Neither check changes the smoothed cache.
inline bool SunTracePositionOnScreen(int x, int y, int width, int height)
{
    return x >= 10 && y >= 10 && x <= width - 10 && y <= height - 10;
}

inline float ApplySunDepthFactor(float light, float cachedFactor,
                                int x, int y, int width, int height)
{
    return SunTracePositionOnScreen(x, y, width, height) ?
        light * cachedFactor : 0.0f;
}
