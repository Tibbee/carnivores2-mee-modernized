#pragma once

// Distance falloff for the water-surface wave displacement applied in
// PreCashGroundModel (Game/Effects.cpp). Pure header -- no engine globals --
// so the curve is unit-testable without a renderer.
//
// The displacement is shared engine state: PreCashGroundModel writes the
// offset vertices into VMap2 before either renderer draws, so this fade
// applies identically to the OpenGL and software renderers.
//
// Legacy behaviour is a hard cutoff -- full amplitude everywhere except the
// outermost rings -- which leaves a seam where the last displaced ring meets
// flat water. A positive falloff rings the amplitude smoothly down to zero
// before that boundary instead.

inline constexpr int kWaterWaveCutoffRings = 4;

// Accepted range for the config.cfg / workbench fade setting. Kept here, next
// to the curve, so the loader, workbench registry and defaults cannot drift.
inline constexpr int kWaterWaveFadeDefault = 8;
inline constexpr int kWaterWaveFadeMin     = 0;
inline constexpr int kWaterWaveFadeMax     = 64;

// Amplitude multiplier (0..1) for a water cell `ring` cells out from the
// camera.
//
// `falloffRings` == 0 reproduces the legacy hard cutoff exactly: 1.0 for every
// ring strictly inside the cutoff, 0.0 from the cutoff outward. A positive
// value ramps linearly to 0 across that many rings before the cutoff, so the
// transition is continuous instead of stepped.
inline float WaterWaveFade(int ring, int viewRadius, int falloffRings)
{
    // Rings at or beyond this are never displaced.
    const int cutoff = viewRadius - kWaterWaveCutoffRings;
    const int span = (falloffRings > 0 ? falloffRings : 0) + 1;
    const float t = static_cast<float>(cutoff - ring) / static_cast<float>(span);
    if (t >= 1.0f) return 1.0f;
    if (t <= 0.0f) return 0.0f;
    return t;
}
