// ConfigIO.cpp -- menu-side config.cfg read/write (split from Resources.cpp)
// ==========================================================================
// The menu writes the keys it owns and merges them byte-preservingly with
// everything else (Core/ConfigMerge.h); the engine reader lives in
// Hunt/Game/ConfigFile.cpp.
// ==========================================================================

#include "Hunt.h"
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Core/ConfigMerge.h"
#include "Core/ConfigPath.h"
#include "Core/ConfigScan.h"
#include "Core/ConfigText.h"
#include "Core/Flashlight.h"

// ================================================================
// config.cfg -- text-based settings file
// ================================================================
// Format: one setting per line, "key value".
// Lines starting with '#' are comments.  Unknown keys are ignored.
// This file is the single source of truth for settings that are
// not part of the legacy binary trophy format.
// ================================================================

// Write the current settings to config.cfg (EXE-directory based, shared
// with the game engine).
void SaveConfig()
{
	std::string configPath = configpath::ResolveWritePath();

	// Snapshot the existing bytes so everything this build does not own
	// (user comments, game-side envN_* audio keys, future keys) is carried
	// through byte-exact by the merge below. Without this, every save
	// silently deletes hand edits and the render exe's audio tweaks.
	std::string existing;
	{
		std::ifstream in(configPath, std::ios::binary);
		existing.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	// Fresh renderings of every key this build owns, in canonical order.
	// Conditional keys (resolution, hunt_*) are present only when valid --
	// a missing entry means "leave any existing line alone, append nothing".
	std::vector<std::pair<std::string, std::string>> fresh;
	fresh.emplace_back("renderer", std::to_string(g_Options.RenderAPI));
	fresh.emplace_back("fov", std::to_string(g_Options.FOV));
	fresh.emplace_back("object_detail", std::to_string(g_Options.ObjectDetail));
	fresh.emplace_back("fps_limit", std::to_string(g_Options.OptFpsLimit));
	fresh.emplace_back("verbose_logging", g_Options.VerboseLogging ? "1" : "0");
	fresh.emplace_back("nightvision_key", std::to_string(g_Options.NightVisionKey));
	fresh.emplace_back("flashlight_key", std::to_string(g_Options.FlashlightKey));

	// Persist the selected resolution as WxH so the render exe honours it
	// even though the legacy per-profile index (trophy0N.sav) is fragile
	// across differently-ordered mode lists.
	if (g_Options.Resolution >= 0 && g_Options.Resolution < g_ResCount) {
		std::string res = std::to_string(g_ResolutionList[g_Options.Resolution].w)
			+ "x" + std::to_string(g_ResolutionList[g_Options.Resolution].h);
		fresh.emplace_back("resolution", res);
	}

	// Persist the display mode (0=windowed, 1=exclusive fullscreen,
	// 2=borderless fullscreen). The render exe applies it on launch.
	int dm = g_Options.DisplayMode;
	if (dm < 0 || dm > 2) dm = 2;
	fresh.emplace_back("display_mode", std::to_string(dm));

	// Remember the last hunt setup (captured by Menu.cpp from the MenuHunt
	// lists). Guarded: the lists only exist after the first hunt-screen
	// visit, so a fresh boot or options-only session never clobbers the file
	// with empty defaults.
	if (g_HasSavedHunt) {
		fresh.emplace_back("hunt_area", g_SavedHuntArea);
		fresh.emplace_back("hunt_dinos", std::to_string(g_SavedHuntDinos));
		fresh.emplace_back("hunt_weapons", std::to_string(g_SavedHuntWeapons));
		fresh.emplace_back("hunt_utils", std::to_string(g_SavedHuntUtils));
		fresh.emplace_back("hunt_time", std::to_string(g_SavedHuntTime));
	}

	std::string output;
	if (existing.empty()) {
		// First run: canonical file, exactly as before (CRLF like the engine's
		// CreateDefaultConfig template; written in binary mode below).
		output = "# Carnivores 2 Modder's Engine configuration\r\n";
		output += "# Edit by hand if needed — values are validated on load.\r\n";
		output += "\r\n";
		for (const auto& kv : fresh)
			output += kv.first + " " + kv.second + "\r\n";
	} else {
		// Merge with the workbench's byte-preserving policy (Core/ConfigMerge.h):
		// only an owned key's value token is rewritten, comments and unowned
		// keys pass through verbatim, and absent keys append once. An earlier
		// duplicate of an owned key is left alone -- the readers let the last
		// occurrence win, so it is inert.
		const auto writable = [](const std::pair<std::string, std::string>& kv) {
			if (kv.first.empty() || kv.second.empty() || kv.first.front() == '#') return false;
			return kv.first.find_first_of(" \t\r\n") == std::string::npos &&
			       kv.second.find_first_of(" \t\r\n") == std::string::npos;
		};
		std::vector<configmerge::Entry> entries;
		for (const auto& kv : fresh) {
			// A value that cannot round-trip (empty, or a space-containing
			// hunt_area) is skipped with a notice; the rest of the save lands.
			if (!writable(kv)) {
				std::cout << "Config: skipping " << kv.first
				          << " (not a single-token key value)" << std::endl;
				continue;
			}
			entries.push_back({kv.first, kv.second});
		}
		const auto merged = configmerge::Merge(existing, entries);
		if (!merged.ok) {
			std::cout << "Config: merge failed (" << merged.error
			          << "); the file was left unchanged." << std::endl;
			return;
		}
		output = merged.text;
	}

	// Opened only after the new bytes are known, and binary so the merge's
	// terminators are written as-is (text mode would double the CRs); a
	// refusal never leaves a truncated file behind.
	std::ofstream fs(configPath, std::ios::trunc | std::ios::binary);
	if (!fs.is_open()) {
		std::cout << "Config: could not write " << configPath << std::endl;
		return;
	}
	fs << output;

	std::cout << "Config Saved (" << configPath << ")." << std::endl;
}

// Apply one scanned config.cfg field. Keys match case-insensitively, like the
// engine loader, so a hand-written "FOV 62" is not silently ignored here while
// the engine honours it; a missing value leaves the setting unchanged.
// Returns true if the key was recognised.
static bool ParseConfigField(const configscan::Field& field)
{
	std::istringstream iss(std::string(field.value));

	if (configscan::KeyEquals(field.key, "renderer")) {
		int v;
		if (iss >> v) {
			// 0=Software, 1=OpenGL. Legacy Direct3D values are normalized
			// to OpenGL so old configs do not launch v_d3d.ren.
			g_Options.RenderAPI = NormalizeMenuRenderAPI(v);
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "fov")) {
		int v;
		if (iss >> v) {
			if (v < kFovMin) v = kFovMin;
			if (v > kFovMax) v = kFovMax;
			g_Options.FOV = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "object_detail")) {
		int v;
		if (iss >> v) {
			v = ClampObjectDetail(v);
			g_Options.ObjectDetail = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "fps_limit")) {
		int v;
		if (iss >> v) {
			if (v < 0) v = 0;
			if (v >= kFpsLimitCount) v = kFpsLimitDefault;
			g_Options.OptFpsLimit = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "verbose_logging")) {
		int v;
		if (iss >> v) {
			g_Options.VerboseLogging = (v != 0);
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "nightvision_key")) {
		int v;
		if (iss >> v) {
			g_Options.NightVisionKey = v;
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "flashlight_key")) {
		int v;
		if (iss >> v) {
			g_Options.FlashlightKey = ClampFlashlightKey(v);
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "resolution")) {
		int w = 0, h = 0;
		char x = 0;
		if (iss >> w >> x >> h && (x == 'x' || x == 'X') && w > 0 && h > 0) {
			// Translate WxH back to an index in the current mode list.
			for (int r = 0; r < g_ResCount; r++) {
				if (g_ResolutionList[r].w == w && g_ResolutionList[r].h == h) {
					g_Options.Resolution = r;
					break;
				}
			}
		}
		return true;
	}

	if (configscan::KeyEquals(field.key, "display_mode")) {
		int v;
		if (iss >> v) {
			if (v >= 0 && v <= 2)
				g_Options.DisplayMode = v;
		}
		return true;
	}

	// Last hunt setup (see SaveConfig). Everything is validated again
	// against the live lists on restore, so stale/modded values can only
	// fall back to defaults, never select out of range.
	if (configscan::KeyEquals(field.key, "hunt_area")) {
		std::string v;
		if (iss >> v && v.size() < 128) {
			g_SavedHuntArea = v;
			g_HasSavedHunt = true;
		}
		return true;
	}
	if (configscan::KeyEquals(field.key, "hunt_dinos") || configscan::KeyEquals(field.key, "hunt_weapons") || configscan::KeyEquals(field.key, "hunt_utils")) {
		unsigned long long v;
		if (iss >> v) {
			if (configscan::KeyEquals(field.key, "hunt_dinos")) g_SavedHuntDinos = v;
			else if (configscan::KeyEquals(field.key, "hunt_weapons")) g_SavedHuntWeapons = v;
			else g_SavedHuntUtils = v;
			g_HasSavedHunt = true;
		}
		return true;
	}
	if (configscan::KeyEquals(field.key, "hunt_time")) {
		int v;
		if (iss >> v && v >= HUNT_DAWN && v <= HUNT_NIGHT) {
			g_SavedHuntTime = v;
			g_HasSavedHunt = true;
		}
		return true;
	}

	// Unknown key -- ignore gracefully (forward-compat with newer configs)
	return false;
}

// Read config.cfg and override g_Options fields.
// If the file does not exist, write defaults (first-run migration).
void LoadConfig()
{
	std::string configPath = configpath::ResolveReadPath();
	std::ifstream fs(configPath);
	if (!fs.is_open()) {
		std::cout << "Config: " << configPath << " not found — writing defaults." << std::endl;
		SaveConfig();
		return;
	}

	std::string whole;
	size_t nulBytes = 0;
	if (!ReadConfigText(fs, whole, nulBytes)) {
		std::cout << "Config: unreadable or unsupported encoding; using defaults." << std::endl;
		return;
	}
	if (nulBytes) {
		std::cout << "Config: recovered " << nulBytes << " NUL padding bytes; please resave config.cfg." << std::endl;
	}
	for (const configscan::Field& field : configscan::Scan(whole)) {
		ParseConfigField(field);
	}

	std::cout << "Config Loaded (" << configPath << ")." << std::endl;
}
