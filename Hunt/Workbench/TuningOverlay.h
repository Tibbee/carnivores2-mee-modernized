#pragma once

// The legacy F10 underwater debug overlay, shared by the GL and software
// renderers (GLUI.cpp ShowControlElements and SoftHUD.cpp DrawUnderwaterDebugMenu).
// Rows come from the workbench model (TuningSession.h) and live values from
// the runtime adapter, so the parameter list, order and labels cannot drift
// between the two drawings; each renderer supplies its own text primitive and
// dirty-rect handling, the frame position and colours are identical.

#include <cstddef>
#include <cstdio>
#include "TuningSession.h"
#include "TuningRuntime.h"

namespace workbench {

inline constexpr int kOverlayX = 10;
inline constexpr int kOverlayY = 40;
inline constexpr int kOverlayLineH = 16;
inline constexpr int kOverlayIndent = 10;
inline constexpr int kOverlayMaxRows = 7;

inline constexpr int kOverlayHeaderColor = 0x00FFFFFF;
inline constexpr int kOverlaySelectedColor = 0x0000FFFF;
inline constexpr int kOverlayRowColor = 0x00C0C0C0;
inline constexpr int kOverlayHintColor = 0x00808080;

inline constexpr const char* kOverlayHint = "Arrows: select +/-: adjust";

struct OverlayRow {
	const char* name;
	const char* desc;
	float value;
};

// Tab 0/1/2 are FOG/WAVES/SUN; anything else reads as SUN, matching the
// renderers' old ternary chains.
inline const char* OverlayTabName(int tab)
{
	if (tab == 0) { return "FOG"; }
	if (tab == 1) { return "WAVES"; }
	return "SUN";
}

inline void FormatOverlayHeader(char* out, std::size_t size, int tab)
{
	std::snprintf(out, size, "=== %s DEBUG (F10=close, PgUp/PgDn=tab, D=dump) ===",
	              OverlayTabName(tab));
}

// Fills `rows` with the tab's live rows and returns the count. Values are read
// once per call through the runtime adapter, so the drawing shows the same
// state the arrow / +/- keys edit.
inline int FillOverlayRows(int tab, OverlayRow* rows, int capacity)
{
	// Legacy hint strings, positionally held next to the parameter rows. The
	// per-tab count guards the pairing if a registry row is ever added.
	struct TabHints {
		const char* const* text;
		int count;
	};
	static const char* const kFogHints[] = {
		"(1.0=normal)", "(Beer-Lambert)", "(units)", "(fog units)",
		"(exponent)", "(added to FLimit)", "(camera boost)",
	};
	static const char* const kWaveHints[] = {
		"(primary swell)", "(cross-wave)", "(fine detail)", "(time mult)",
	};
	static const char* const kSunHints[] = {
		"(blinding, 0=off)", "(sun/moon disc)",
	};
	static const TabHints kTabs[] = {
		{ kFogHints, 7 },
		{ kWaveHints, 4 },
		{ kSunHints, 2 },
	};

	const int effectiveTab = (tab == 0 || tab == 1) ? tab : 2;
	const TabHints& hints = kTabs[effectiveTab];
	const Values values = ReadRuntimeValues();

	int count = 0;
	while (count < capacity && count < hints.count) {
		const Parameter parameter = LegacyParameter(effectiveTab, count);
		if (parameter == Parameter::Count) {
			break;
		}
		const Descriptor* descriptor = FindDescriptor(parameter);
		rows[count].name = descriptor != nullptr ? descriptor->legacyLabel : "?";
		rows[count].desc = hints.text[count];
		rows[count].value = values[static_cast<std::size_t>(parameter)];
		++count;
	}
	return count;
}

// "> name = value  desc" for the selected row, "name = value  desc" otherwise.
inline void FormatOverlayRow(char* out, std::size_t size, const OverlayRow& row, bool selected)
{
	std::snprintf(out, size, selected ? "> %s = %.2f  %s" : "%s = %.2f  %s",
	              row.name, static_cast<double>(row.value), row.desc);
}

} // namespace workbench
