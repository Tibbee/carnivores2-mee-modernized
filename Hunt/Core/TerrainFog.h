#pragma once

#include <algorithm>

// Match CalcFogLevel's volume selection. A clear destination still sees fog
// between it and a camera inside a pocket. Use this same index for its colour;
// retaining the amount with the clear cell's sky colour would give the wrong haze.
inline constexpr int ResolveTerrainFogIndex(int mapFogIndex, bool cameraInFog, int cameraFogIndex)
{
	return mapFogIndex == 0 && cameraInFog ? cameraFogIndex : mapFogIndex;
}

// Resolve the camera's depth term for CalcFogLevel. The camera's in-fog
// envelope may only reach vertices in the camera's own pocket -- or a clear
// cell that falls back to that pocket. A foreign pocket keeps its own authored
// density and distance term; lending the camera term to it saturated every
// other volume the moment the player stepped into any pocket, producing flat,
// fully fogged cross-volume silhouettes.
inline constexpr float ResolvePocketCameraDepth(float rawCameraDepth, bool cameraInFog,
	int destinationFogIndex, int cameraFogIndex)
{
	const bool cameraOwnsVolume = cameraInFog && destinationFogIndex == cameraFogIndex;
	return !cameraOwnsVolume && rawCameraDepth > 0.0f ? 0.0f : rawCameraDepth;
}

// Resolve only the visibility gate, not density: legacyFog already contains the
// camera-to-point calculation. In particular, do not add a second fog amount.
inline float ResolveTerrainFogAmount(int mapFogIndex, int legacyFog, bool fogOn,
	bool isUnderwater, bool cameraInFog, int cameraFogIndex)
{
	// Underwater uses its own soft cap, which can exceed the pocket-fog range.
	if (isUnderwater) {
		return static_cast<float>(legacyFog);
	}
	if (!fogOn || ResolveTerrainFogIndex(mapFogIndex, cameraInFog, cameraFogIndex) <= 0) {
		return 0.0f;
	}
	return static_cast<float>(std::clamp(legacyFog, 0, 255));
}
