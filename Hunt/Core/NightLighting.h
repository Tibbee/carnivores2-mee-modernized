#pragma once

// OpenGL moonlit-night policy. Pure, renderer-independent math for tests;
// settings live outside profile/trophy structures and the legacy light maps.
#include "ConfigParse.h"
#include "Flashlight.h"
#include <algorithm>
#include <array>
#include <cstring>

struct NightLightingSettings {
	int mode = 1; // 0 = Classic, 1 = Moonlit
	int worldBrightnessPct = 22;
	int desaturationPct = 85;
	int skyBrightnessPct = 60;
};

inline NightLightingSettings& NightLightingOptions() {
	static NightLightingSettings settings;
	return settings;
}

enum class NightLightingOption { Mode, WorldBrightness, Desaturation, SkyBrightness };

inline bool ParseNightLightingOption(NightLightingOption option, const char* text,
	NightLightingSettings& settings) {
	const int minimum = option == NightLightingOption::Mode ||
		option == NightLightingOption::Desaturation ? 0 : 5;
	const int maximum = option == NightLightingOption::Mode ? 1 : 100;
	int value = 0;
	if (!ParseConfigInt(text, minimum, maximum, value)) return false;
	switch (option) {
	case NightLightingOption::Mode: settings.mode = value; break;
	case NightLightingOption::WorldBrightness: settings.worldBrightnessPct = value; break;
	case NightLightingOption::Desaturation: settings.desaturationPct = value; break;
	case NightLightingOption::SkyBrightness: settings.skyBrightnessPct = value; break;
	default: return false;
	}
	return true;
}

// LoadConfig's legacy %63s must not turn an oversized token into a valid
// truncated scalar. Validate the full authored value before parsing the copy.
inline bool NightLightingConfigValueFits(const char* line, std::size_t capacity) {
	if (!line) return false;
	const char* value = line + std::strspn(line, " \t\r\n");
	value += std::strcspn(value, " \t\r\n");
	value += std::strspn(value, " \t\r\n");
	const auto length = std::strcspn(value, " \t\r\n");
	return length > 0 && length < capacity;
}

inline bool UseMoonlitNight(int timeOfDay, bool nightVisionOn,
	const NightLightingSettings& settings) {
	return timeOfDay == 2 && !nightVisionOn && settings.mode == 1;
}

// Plain vec4 uniform, not a PerFrame UBO extension: world gain, desaturation,
// sky gain, restoration/new sky treatment enabled. NV retains the old sky.
inline std::array<float, 4> NightLightingParameters(int timeOfDay, bool nightVisionOn,
	const NightLightingSettings& settings) {
	if (!UseMoonlitNight(timeOfDay, nightVisionOn, settings))
		return {0.5f, 0.6f, 1.0f, 0.0f};
	return {settings.worldBrightnessPct / 100.0f, settings.desaturationPct / 100.0f,
		settings.skyBrightnessPct / 100.0f, 1.0f};
}

// Restoration uses the EXISTING beam geometry/intensity before the separate
// highlight roll-off. Zero intensity, cone edge and range endpoint are no-ops.
inline float NightFlashlightRestoration(float intensity, float distance, float range,
	float cosOffAxis, float cosInner, float cosOuter) {
	return std::clamp(intensity * FlashlightConeTerm(cosOffAxis, cosInner, cosOuter) *
		FlashlightDistanceTerm(distance, range), 0.0f, 1.0f);
}

using NightRgb = std::array<float, 3>;

inline float NightLuminance(const NightRgb& color) {
	return color[0] * 0.299f + color[1] * 0.587f + color[2] * 0.114f;
}

// Reference of ApplyNightLighting() in the three world shaders. Input is
// already lit/fogged. The unchanged warm flashlight contribution comes AFTER
// this function; restoring color is not an extra lamp or a changed beam curve.
inline NightRgb GradeNightWorld(const NightRgb& original, int timeOfDay,
	bool nightVisionOn, const NightLightingSettings& settings, float restoration = 0.0f) {
	if (timeOfDay != 2 || nightVisionOn) return original;
	const auto parameters = NightLightingParameters(timeOfDay, nightVisionOn, settings);
	const float gray = NightLuminance(original);
	const float restore = parameters[3] > 0.5f ? std::clamp(restoration, 0.0f, 1.0f) : 0.0f;
	NightRgb result{};
	for (size_t i = 0; i < result.size(); ++i) {
		const float graded = (original[i] * (1.0f - parameters[1]) +
			gray * parameters[1]) * parameters[0];
		result[i] = graded * (1.0f - restore) + original[i] * restore;
	}
	return result;
}

inline NightRgb GradeNightSky(const NightRgb& original, float elevation,
	int timeOfDay, bool nightVisionOn, const NightLightingSettings& settings) {
	const auto parameters = NightLightingParameters(timeOfDay, nightVisionOn, settings);
	const float vert = std::clamp(elevation, 0.0f, 1.0f);
	const float zenith = parameters[3] > 0.5f ? 1.0f - 0.28f * vert : 0.92f + 0.08f * (1.0f - vert);
	NightRgb result = original;
	if (parameters[3] > 0.5f) {
		const float gray = NightLuminance(original);
		for (size_t i = 0; i < result.size(); ++i)
			result[i] = (original[i] * (1.0f - parameters[1]) + gray * parameters[1]) * parameters[2];
	}
	for (float& channel : result) channel *= zenith;
	return result;
}

inline NightRgb GradeNightSkyFog(const NightRgb& original, int timeOfDay,
	bool nightVisionOn, const NightLightingSettings& settings) {
	if (timeOfDay != 2) return original;
	const auto parameters = NightLightingParameters(timeOfDay, nightVisionOn, settings);
	const float desaturation = parameters[3] > 0.5f ? parameters[1] : 0.75f;
	const float brightness = parameters[3] > 0.5f ? parameters[0] : 1.0f;
	const float gray = NightLuminance(original);
	NightRgb result{};
	for (size_t i = 0; i < result.size(); ++i)
		result[i] = (original[i] * (1.0f - desaturation) + gray * desaturation) * brightness;
	return result;
}
