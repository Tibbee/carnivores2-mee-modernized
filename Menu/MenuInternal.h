// MenuInternal.h -- state shared by the split menu translation units
// (Menu.cpp keeps the controller; MenuDraw.cpp owns the drawing surface)
// ==========================================================================
// The standalone menu's UI shell is split across translation units. This
// header owns what they share: the MenuSet list widget, the layout
// namespaces, the option string tables, the ControlsRow mapping and the
// file-local GDI/cursor state that Menu.cpp defines.
// ==========================================================================

#pragma once

#include "Hunt.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class MenuSet {
public:
	int32_t		x0, y0;
	uint32_t	Count;
	uint32_t	Offset;
	std::vector<std::pair<std::string, bool>> Item;
	int32_t		Selected;
	int32_t		Hilite;
	RECT		Rect;
	int32_t		Padding;

	MenuSet() :
		x0(0), y0(0),
		Count(0),
		Offset(0),
		Item(),
		Selected(-1),
		Hilite(-1),
		Rect({ 0,0,0,0 }),
		Padding(0)
	{}

	void AddItem(const std::string& txt);
};

enum MenuSetEnum {
	OPT_GAME = 0,
	OPT_KEYBINDINGS = 1,
	OPT_VIDEO = 2,
	OPT_MAX
};

// ======================================================================= //
// Layout System
// ======================================================================= //
//
// Modern layout system that auto-calculates spacing and supports proper
// text alignment. Replaces hardcoded pixel coordinates for menu sections.
//
namespace Layout {

	// Text alignment options (matches DTA_* values from wingdi.h)
	enum Align {
		LEFT   = 0,
		RIGHT  = 2,
		CENTER = 6
	};

	// A vertical column of items with consistent spacing and alignment
	struct Column {
		int x0;            // X position (left for LEFT, right for RIGHT, center for CENTER)
		int yStart;        // Y position of first item
		int yEnd;          // Y position of last item (bottom of column)
		int itemCount;     // Number of items to display
		Align align;       // Text alignment
		int padding;       // Extra padding from edges

		Column() : x0(0), yStart(0), yEnd(0), itemCount(0), align(LEFT), padding(0) {}

		// Calculate Y position for item i, auto-spacing to fill panel height
		int GetItemY(int i) const {
			if (itemCount <= 0) return yStart;
			int availableHeight = yEnd - yStart;
			int spacing = availableHeight / itemCount;
			return yStart + (spacing * i) + (spacing / 2) - 7; // Center vertically in slot
		}

		// Get the actual spacing being used
		int GetSpacing() const {
			if (itemCount <= 0) return 20;
			return (yEnd - yStart) / itemCount;
		}
	};

	// A rectangular region in the background artwork
	struct Panel {
		int x, y, w, h;

		Panel() : x(0), y(0), w(0), h(0) {}
		Panel(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}

		int GetRight() const { return x + w; }
		int GetBottom() const { return y + h; }
	};

	// Helper: Get X position based on alignment within a panel
	inline int GetAlignedX(int panelLeft, int panelRight, Align align, int padding = 15) {
		switch (align) {
			case LEFT:   return panelLeft + padding;
			case RIGHT:  return panelRight - padding;
			case CENTER: return (panelLeft + panelRight) / 2;
			default:     return panelLeft + padding;
		}
	}

} // namespace Layout

// ======================================================================= //
// Options Screen Layout Definition
// ======================================================================= //
// These values follow the original StartLegacy option screen spacing:
// labels are right-aligned, values sit just to the right of the label column,
// and sliders use the same track positions as the legacy artwork.
//
namespace OptionsLayout {

	const int ROW_HEIGHT = 20;
	const int ROW_HEIGHT_VIDEO = 21; // Fits the extra C2ME video rows inside the legacy panel.

	// Panel regions from the original options artwork.
	const Layout::Panel PANEL_GAME     (40,  75,  340, 175);  // Top-left game settings
	const Layout::Panel PANEL_CONTROLS (422, 75,  338, 425);  // Right side key bindings
	const Layout::Panel PANEL_VIDEO    (40, 285, 340, 255);  // Bottom-left video/settings

	// Legacy-aligned option columns. yStart is biased so Column::GetItemY()
	// returns the exact baseline used by the original menu (100, 353, 71).
	const int GAME_LABEL_RIGHT_X   = 190;
	const int OPTION_VALUE_X       = 205;
	const int OPTION_SLIDER_X      = OPTION_VALUE_X;
	const int OPTION_SLIDER_W      = 123;

	const int CONTROLS_LABEL_RIGHT_X = 600;
	const int CONTROLS_VALUE_X       = 617;
	const int CONTROLS_SLIDER_X      = CONTROLS_VALUE_X + 1;
	const int CONTROLS_SLIDER_W      = 123;

	const int GAME_FIRST_Y   = 80;
	const int VIDEO_FIRST_Y  = 353;
	const int CONTROLS_FIRST_Y = 71;

	inline Layout::Column MakeColumn(int x0, int firstY, int count, Layout::Align align, int rowHeight = ROW_HEIGHT)
	{
		Layout::Column col;
		col.x0 = x0;
		col.yStart = firstY - 4;
		col.yEnd = col.yStart + (count * rowHeight);
		col.itemCount = count;
		col.align = align;
		return col;
	}

	// Labels are right-aligned in their panel, values are left-aligned.
	inline Layout::Column MakeGameLabels(int count)
	{
		return MakeColumn(GAME_LABEL_RIGHT_X, GAME_FIRST_Y, count, Layout::RIGHT);
	}

	inline Layout::Column MakeGameValues(int count)
	{
		return MakeColumn(OPTION_VALUE_X, GAME_FIRST_Y, count, Layout::LEFT);
	}

	inline Layout::Column MakeControlsLabels(int count)
	{
		return MakeColumn(CONTROLS_LABEL_RIGHT_X, CONTROLS_FIRST_Y, count, Layout::RIGHT);
	}

	inline Layout::Column MakeControlsValues(int count)
	{
		return MakeColumn(CONTROLS_VALUE_X, CONTROLS_FIRST_Y, count, Layout::LEFT);
	}

	inline Layout::Column MakeVideoLabels(int count)
	{
		return MakeColumn(GAME_LABEL_RIGHT_X, VIDEO_FIRST_Y, count, Layout::RIGHT, ROW_HEIGHT_VIDEO);
	}

	inline Layout::Column MakeVideoValues(int count)
	{
		return MakeColumn(OPTION_VALUE_X, VIDEO_FIRST_Y, count, Layout::LEFT, ROW_HEIGHT_VIDEO);
	}

} // namespace OptionsLayout

enum DrawTextAlignEnum {
	// Uses the same values as wingdi.h TA_LEFT and so on.
	// TODO: Write a conversion function for Linux if we port it to Linux
	DTA_LEFT = 0,
	DTA_RIGHT = 2,
	DTA_CENTER = 6,
	DTA_BOTTOM = 8
};

// String table
const char g_GitHubURL[] = "https://github.com/carnivores-cpe/Carn2-Menu";
const char st_BoolText[2][4] = { "Off", "On" };
const char st_UnitText[2][10] = { "Metric", "Imperial" };
const char st_TextureText[3][5] = { "Low", "High", "Auto" };
const char st_AlphaKeyText[2][14] = { "Color Key", "Alpha Channel" };
// The new menu exposes only Software and OpenGL. Legacy Direct3D values are
// normalized back to OpenGL so old profiles/configs never launch v_d3d.ren.
const char st_RenText[kRenderAPI_Count][12] = { "Software", "OpenGL" };
const char g_RendererFile[kRenderAPI_Count][8] = { "v_soft", "v_gl" };
const char st_AudText[2][16] = { "DirectSound", "OpenAL Soft" };
const char st_FpsText[kFpsLimitCount][12] = { "Unlimited", "60", "120", "240" };

// Display mode video option. Matches the engine's display_mode config key:
// 0=windowed, 1=exclusive fullscreen, 2=borderless fullscreen.
const int kDisplayModeCount = 3;
const char st_DisplayModeText[kDisplayModeCount][16] = { "Windowed", "Fullscreen", "Borderless" };

// State defined in Menu.cpp.
extern MenuSet MenuOptions[3];
extern MenuSet MenuHunt[4];
extern void* lpVideoBuf;
extern HDC hdcCMain;
extern HBITMAP bmpMain;
extern HBITMAP hbmpOld;
extern HFONT hfntOld;
extern POINT g_CursorPos;
extern int g_WaitKey;
extern int g_LastHoverId;
extern std::pair<unsigned, unsigned> g_HuntInfo;
extern Layout::Column g_GameLabels, g_GameValues;
extern Layout::Column g_ControlsLabels, g_ControlsValues;
extern Layout::Column g_VideoLabels, g_VideoValues;

// Helpers shared by the split translation units (defined in Menu.cpp).
bool IsPointInRect(POINT& p, RECT& rc);
int GetTextW(HDC hdc, const std::string& s);
int GetTextH(HDC hdc, const std::string& s);
bool RegistryRowContains(const POINT& p, unsigned row);

// Rows of MenuOptions[OPT_KEYBINDINGS] that do NOT map onto KeyMap fields.
//
// Everything before FirstControlsRowIndex() is addressed by index into
// Options::KeyMap, so the number of rows added ahead of NightVision must stay
// exactly sizeof(KeyMap)/sizeof(int32_t). The trailing rows are stored
// elsewhere in Options and are resolved by counting back from the end.
//
// They used to be addressed as size()-3, size()-2 and size()-1, which silently
// pointed at the WRONG binding the moment a row was inserted: adding the
// flashlight row shifted size()-3 off "Night vision" and onto another binding,
// with no crash and no log line. Count back from the end instead.
enum class ControlsRow {
	NightVision = 0,
	Flashlight,
	MouseInvert,
	MouseSensitivity,
	Count
};

inline int ControlsRowIndex(ControlsRow row)
{
	return static_cast<int>(MenuOptions[OPT_KEYBINDINGS].Item.size())
		- static_cast<int>(ControlsRow::Count)
		+ static_cast<int>(row);
}

// First row that reads its binding from somewhere other than Options::KeyMap.
inline int FirstControlsRowIndex()
{
	return ControlsRowIndex(ControlsRow::NightVision);
}

// Drawing entry points (defined in MenuDraw.cpp).
void InterfaceClear(WORD Color);
void InterfaceSetFont(HFONT font);
void InterfaceBlt();
void DrawMenuBg(MenuItem& menu);
void DrawTextShadow(int x, int y, const std::string& text, uint32_t color, int align = DTA_LEFT);
void DrawMenuProfile();
void DrawMenuStatistics();
void DrawMenuCredits();
void DrawMenuHunt();
void DrawMenuRegistry();
void DrawMenuOptions();
