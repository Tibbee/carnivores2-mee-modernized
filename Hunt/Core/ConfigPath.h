#pragma once
#include <cstring>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Shared by the menu and the engine (like Core/ConfigText.h): both EXEs must
// resolve the same config.cfg no matter which directory each was launched
// from. The module path is what makes that possible, so this header is
// Windows-only rather than pure.
//
// Read: the copy next to the EXE when it exists, else the working-directory
// copy, so a bare launch from elsewhere still finds a config.cfg.
// Write: the EXE directory is authoritative when the module path can be
// resolved -- that is the copy the other EXE reads; the relative name is only
// the fallback.

namespace configpath {

inline constexpr const char* kFileName = "config.cfg";

// Given the module path from GetModuleFileNameA, the config path in the same
// directory. An empty path or one without a separator (neither can come from
// a real module) yields the bare file name.
inline std::string InModuleDirectory(const char* modulePath)
{
	if (modulePath == nullptr) return kFileName;
	const char* separator = strrchr(modulePath, '\\');
	if (separator == nullptr) return kFileName;
	return std::string(modulePath, separator + 1) + kFileName;
}

inline std::string ResolveReadPath()
{
	char module[MAX_PATH];
	const DWORD length = GetModuleFileNameA(nullptr, module, sizeof(module));
	if (length > 0 && length < sizeof(module)) {
		const std::string path = InModuleDirectory(module);
		if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;
	}
	return kFileName;
}

inline std::string ResolveWritePath()
{
	char module[MAX_PATH];
	const DWORD length = GetModuleFileNameA(nullptr, module, sizeof(module));
	if (length > 0 && length < sizeof(module)) return InModuleDirectory(module);
	return kFileName;
}

} // namespace configpath
