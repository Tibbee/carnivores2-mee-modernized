#pragma once

// Equipment-dependent visibility for camera-held models and the in-game HUD.
// Separate from the geometric world beam and from saved profile structures.
#include "NightLighting.h"
#include <cstdint>

enum class NightDrawKind { World, Viewmodel, Hud, EssentialHud, Unaffected };
inline constexpr float kNightReadabilityFloor = 0.55f;
inline constexpr int kNightReadableRectLimit = 16;

// vec3: brightness, desaturation, enabled. NV wins over the flashlight.
// The renderer supplies the effective purchased/on/nonzero-intensity state.
inline std::array<float, 3> NightPresentationParameters(int timeOfDay,
	bool nightVisionOn, bool flashlightEnabled, const NightLightingSettings& settings,
	NightDrawKind kind) {
	if (timeOfDay != 2 || kind == NightDrawKind::World ||
		kind == NightDrawKind::Unaffected) return {1.0f, 0.0f, 0.0f};
	const float desaturation = settings.desaturationPct / 100.0f;
	if (nightVisionOn) return {1.0f, desaturation, 1.0f};
	if (flashlightEnabled) return {1.0f, 0.0f, 1.0f};
	float brightness = settings.worldBrightnessPct / 100.0f;
	if (kind == NightDrawKind::EssentialHud)
		brightness = (std::max)(brightness, kNightReadabilityFloor);
	return {brightness, desaturation, 1.0f};
}

inline NightRgb GradeNightPresentation(const NightRgb& original,
	const std::array<float, 3>& parameters, bool readable = false) {
	if (parameters[2] < 0.5f) return original;
	const float gray = NightLuminance(original);
	const float brightness = readable ? (std::max)(parameters[0], kNightReadabilityFloor) : parameters[0];
	NightRgb result{};
	for (std::size_t i = 0; i < result.size(); ++i)
		result[i] = (original[i] * (1.0f - parameters[1]) + gray * parameters[1]) * brightness;
	return result;
}

// Normalized top-down HUD rectangles. Saturating overflow protects controls
// rather than dropping them. Independent of dirty upload/clear rectangles.
struct NightReadableRegions {
	std::array<float, kNightReadableRectLimit * 4> bounds{};
	int count = 0;
	void Clear() { count = 0; }
	void Add(int x, int y, int width, int height, int screenWidth, int screenHeight) {
		if (width <= 0 || height <= 0 || screenWidth <= 0 || screenHeight <= 0) return;
		const auto left = std::clamp<std::int64_t>(x, 0, screenWidth);
		const auto top = std::clamp<std::int64_t>(y, 0, screenHeight);
		const auto right = std::clamp<std::int64_t>(static_cast<std::int64_t>(x) + width, 0, screenWidth);
		const auto bottom = std::clamp<std::int64_t>(static_cast<std::int64_t>(y) + height, 0, screenHeight);
		if (right <= left || bottom <= top) return;
		const std::array<float, 4> rect = {static_cast<float>(left) / screenWidth,
			static_cast<float>(top) / screenHeight, static_cast<float>(right) / screenWidth,
			static_cast<float>(bottom) / screenHeight};
		for (int i = 0; i < count; ++i) {
			const auto offset = static_cast<std::size_t>(i) * 4;
			if (rect[0] >= bounds[offset] && rect[1] >= bounds[offset + 1] &&
				rect[2] <= bounds[offset + 2] && rect[3] <= bounds[offset + 3]) return;
		}
		if (count == kNightReadableRectLimit) {
			count = 1;
			bounds[0] = 0.0f; bounds[1] = 0.0f; bounds[2] = 1.0f; bounds[3] = 1.0f;
			return;
		}
		const auto offset = static_cast<std::size_t>(count++) * 4;
		std::copy(rect.begin(), rect.end(), bounds.begin() + offset);
	}
	bool Contains(float x, float y) const {
		for (int i = 0; i < count; ++i) {
			const auto offset = static_cast<std::size_t>(i) * 4;
			if (x >= bounds[offset] && y >= bounds[offset + 1] &&
				x < bounds[offset + 2] && y < bounds[offset + 3]) return true;
		}
		return false;
	}
};
