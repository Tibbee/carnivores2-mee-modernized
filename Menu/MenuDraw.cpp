// MenuDraw.cpp -- menu drawing: widgets, text helpers and per-screen draws
// (split from Menu.cpp)
// ==========================================================================
// Owns the GDI drawing surface (background compositing, widget primitives,
// text helpers, the frame blit) and every per-screen draw function that the
// ProcessMenu controller dispatches to.
// ==========================================================================

#include "Hunt.h"
#include "MenuInternal.h"
#include "ListMath.h"
#include "ShopSafety.h"
#include "../Hunt/Core/KeyBindings.h"
#include "../Hunt/Core/ShadowText.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

void _Line(HDC hdc, int x1, int y1, int x2, int y2)
{
	MoveToEx(hdc, x1, y1, nullptr);
	LineTo(hdc, x2, y2);
}

void DrawProgressBar(int x, int y, float l)
{
	int W = 120; y += 13;

	HPEN wp = CreatePen(PS_SOLID, 0, 0x009F9F9F);
	HBRUSH wb = CreateSolidBrush(0x003FAF3F);

	HPEN oldpen = (HPEN)SelectObject(hdcCMain, GetStockObject(BLACK_PEN));
	HBRUSH oldbrs = (HBRUSH)SelectObject(hdcCMain, GetStockObject(BLACK_BRUSH));

	x += 1; y += 1;
	_Line(hdcCMain, x, y - 9, x + W + 1, y - 9);
	_Line(hdcCMain, x, y, x + W + 1, y);
	_Line(hdcCMain, x, y - 8, x, y);
	_Line(hdcCMain, x + W, y - 8, x + W, y);
	_Line(hdcCMain, x + W / 2, y - 8, x + W / 2, y);
	_Line(hdcCMain, x + W / 4, y - 8, x + W / 4, y);
	_Line(hdcCMain, x + W * 3 / 4, y - 8, x + W * 3 / 4, y);

	x -= 1; y -= 1;
	SelectObject(hdcCMain, wp);
	_Line(hdcCMain, x, y - 9, x + W + 1, y - 9);
	_Line(hdcCMain, x, y, x + W + 1, y);
	_Line(hdcCMain, x, y - 8, x, y);
	_Line(hdcCMain, x + W, y - 8, x + W, y);
	_Line(hdcCMain, x + W / 2, y - 8, x + W / 2, y);
	_Line(hdcCMain, x + W / 4, y - 8, x + W / 4, y);
	_Line(hdcCMain, x + W * 3 / 4, y - 8, x + W * 3 / 4, y);

	W -= 2;
	PatBlt(hdcCMain, x + 2, y - 5, static_cast<int>((W * l / 2.f)), 4, PATCOPY);

	SelectObject(hdcCMain, wb);
	PatBlt(hdcCMain, x + 1, y - 6, static_cast<int>((W * l / 2.f)), 4, PATCOPY);

	SelectObject(hdcCMain, oldpen);
	SelectObject(hdcCMain, oldbrs);
	DeleteObject(wp);
	DeleteObject(wb);
}

void DrawSliderBar(int x, int y, int w, float v, int slider_rgb = RGB(239, 228, 176))
{
	if (v < 0.0f)
		v = 0.0f;
	if (v > 1.0f)
		v = 1.0f;

	int xs = static_cast<int>((x + ((w - 2) * v)));

	//HPEN wp = CreatePen(PS_SOLID, 0, 0x009F9F9F);

	HBRUSH wb = CreateSolidBrush(slider_rgb);// RGB(239, 228, 176));

	HPEN oldpen = (HPEN)SelectObject(hdcCMain, GetStockObject(BLACK_PEN));
	HBRUSH oldbrs = (HBRUSH)SelectObject(hdcCMain, GetStockObject(BLACK_BRUSH));

	// Draw track
	_Line(hdcCMain, x, y - 4, x + w + 1, y - 4);
	_Line(hdcCMain, x, y - 3, x + w + 1, y - 3);
	_Line(hdcCMain, x, y - 2, x + w + 1, y - 2);
	_Line(hdcCMain, x, y - 1, x + w + 1, y - 1);
	_Line(hdcCMain, x, y + 0, x + w + 1, y + 0);
	_Line(hdcCMain, x, y + 1, x + w + 1, y + 1);
	_Line(hdcCMain, x, y + 2, x + w + 1, y + 2);
	_Line(hdcCMain, x, y + 3, x + w + 1, y + 3);

	SelectObject(hdcCMain, wb);
	PatBlt(hdcCMain, xs, y - 4, 6, 8, PATCOPY);

	SelectObject(hdcCMain, oldpen);
	SelectObject(hdcCMain, oldbrs);
	//DeleteObject(wp);
	DeleteObject(wb);
}

void DrawPicture(int x, int y, Picture& pic)
{
	BlitMenuPicture(static_cast<uint16_t*>(lpVideoBuf), 800, 600,
		x, y, pic.m_Data, pic.m_Width, pic.m_Height);
}

/*
Draw the menu background and overlay the 'on' layer
*/
void DrawMenuBg(MenuItem& menu)
{
	POINT& p = g_CursorPos;
	uint8_t cursor_id = 0;

	cursor_id = menu.GetID((p.x / 2), (p.y / 2));

	if (g_LastHoverId == -1) {
		g_LastHoverId = cursor_id;
	}
	else if (cursor_id != g_LastHoverId) {
		if (cursor_id != 0) {
			MenuAudioPlayHover();
		}
		g_LastHoverId = cursor_id;
	}

	// Render the base background from the on/off states
	for (int yy = 0; yy < 300; yy++) {
		for (int xx = 0; xx < 400; xx++)
		{
			int x = xx * 2;
			int y = yy * 2;
			int on = false;
			uint8_t id2 = menu.GetID((xx), (yy));

			if (cursor_id == id2) on |= true;

			if (g_MenuState == MENU_HUNT && (id2 >= 1 && id2 <= 6)) {
				on |= (int)g_MenuItem.GetIsElementSet(id2);
			}
			else if (g_MenuState == MENU_OPTIONS && (id2 >= 1 && id2 <= 3)) {
				on |= (int)g_MenuItem.GetIsElementSet(id2);
			}

			if (id2 == 0) on = false;

			if (on) {
				*((uint16_t*)lpVideoBuf + ((x + 0L) + (y + 0L) * 800L)) = menu.m_Image_On[(x + 0) + (600 - (y + 0) - 1) * 800];
				*((uint16_t*)lpVideoBuf + ((x + 1L) + (y + 0L) * 800L)) = menu.m_Image_On[(x + 1) + (600 - (y + 0) - 1) * 800];
				*((uint16_t*)lpVideoBuf + ((x + 0L) + (y + 1L) * 800L)) = menu.m_Image_On[(x + 0) + (600 - (y + 1) - 1) * 800];
				*((uint16_t*)lpVideoBuf + ((x + 1L) + (y + 1L) * 800L)) = menu.m_Image_On[(x + 1) + (600 - (y + 1) - 1) * 800];
			}
			else {
				*((uint16_t*)lpVideoBuf + ((x + 0L) + (y + 0L) * 800L)) = menu.m_Image[(x + 0) + (600 - (y + 0) - 1) * 800];
				*((uint16_t*)lpVideoBuf + ((x + 1L) + (y + 0L) * 800L)) = menu.m_Image[(x + 1) + (600 - (y + 0) - 1) * 800];
				*((uint16_t*)lpVideoBuf + ((x + 0L) + (y + 1L) * 800L)) = menu.m_Image[(x + 0) + (600 - (y + 1) - 1) * 800];
				*((uint16_t*)lpVideoBuf + ((x + 1L) + (y + 1L) * 800L)) = menu.m_Image[(x + 1) + (600 - (y + 1) - 1) * 800];
			}
		}
	}
}

void DrawTextColor(int x, int y, const std::string& text, uint32_t color, int align = DTA_LEFT)
{
	if (align == DTA_RIGHT)
	{
		x -= GetTextW(hdcCMain, text);
	}

	SetBkMode(hdcCMain, TRANSPARENT);
	SetTextColor(hdcCMain, color);
	TextOut(hdcCMain, x, y, text.c_str(), (int)text.size());
}

void DrawTextShadow(int x, int y, const std::string& text, uint32_t color, int align)
{
	if (align == DTA_RIGHT)
	{
		x -= GetTextW(hdcCMain, text);
	}

	shadowtext::Draw(hdcCMain, x, y, text.c_str(), (int)text.size(), color);
}

void DrawURLShadow(int x, int y, const std::string& text, uint32_t color, int align = DTA_LEFT)
{
	int W = GetTextW(hdcCMain, text);
	int H = GetTextH(hdcCMain, text);

	if (align == DTA_RIGHT)
	{
		x -= W;
	}

	RECT rc = { x, y, x + W, y + H };

	if (IsPointInRect(g_CursorPos, rc))
	{
		color = RGB(244, 10, 10);
	}

	HPEN wp = CreatePen(PS_SOLID, 0, color);
	HPEN oldpen = (HPEN)SelectObject(hdcCMain, GetStockObject(BLACK_PEN));

	DrawTextColor(x + 1, y + 1, text, 0x000000);
	_Line(hdcCMain, x + 1, y + H + 1, x + W + 1, y + H + 1);

	SelectObject(hdcCMain, wp);

	DrawTextColor(x, y, text, color);
	_Line(hdcCMain, x, y + H, x + W, y + H);

	SelectObject(hdcCMain, oldpen);
}

void InterfaceSetFont(HFONT font)
{
	if (font == nullptr) {
		SelectObject(hdcCMain, hfntOld);
	}
	else {
		hfntOld = reinterpret_cast<HFONT>(SelectObject(hdcCMain, font));
	}
}

void InterfaceClear(WORD Color)
{
	memset(lpVideoBuf, 0, (800 * 2) * 600);
	hbmpOld = reinterpret_cast<HBITMAP>(SelectObject(hdcCMain, bmpMain));
	hfntOld = reinterpret_cast<HFONT>(SelectObject(hdcCMain, fnt_Small));
}

void InterfaceBlt()
{
	SetStretchBltMode(hdcMain, HALFTONE);
	StretchBlt(hdcMain, 0, 0, g_ClientWidth, g_ClientHeight,
	           hdcCMain, 0, 0, MENU_BASE_WIDTH, MENU_BASE_HEIGHT, SRCCOPY);
	SelectObject(hdcCMain, hfntOld);
	SelectObject(hdcCMain, hbmpOld);
}

/*
Draw the profile details that appear at the top of the main menu,
such as Profile Name, Score, Rank.
*/
void DrawMenuProfile()
{
	std::stringstream ss;

	int c = RGB(239, 228, 176);

	InterfaceSetFont(fnt_Big);
	DrawTextShadow(90, 9, g_UserProfile.Name, c);

	ss << g_UserProfile.Score;
	DrawTextShadow(592, 9, ss.str().c_str(), c, DTA_RIGHT);

#ifdef _carnivores1
	switch (g_UserProfile.Rank)
	{
	case 0: DrawTextShadow(344, 9, "Novice", c); break;
	case 1: DrawTextShadow(344, 9, "Advanced", c); break;
	case 2: DrawTextShadow(344, 9, "Expert", c); break;
	}
#endif

	InterfaceSetFont(nullptr);
}

/*
Draw this profile's last hunt statistics and total hunt statistics, also calls DrawProfileMenu()
to draw the profile name and score/rank as an overlay.
*/
void DrawMenuStatistics()
{
	// X 602 - 792
	RECT rc = { 602, 70, 792, 300 };
	std::stringstream ss;
	int c = RGB(239, 228, 176);

	InterfaceSetFont(fnt_Midd);
	int  ttm = static_cast<int>(g_UserProfile.Total.time);
	int  ltm = static_cast<int>(g_UserProfile.Last.time);

	DrawTextShadow(rc.left + 4, 78, "Path travelled  ", c);

	if (g_Options.OptSys)
		ss << std::setprecision(4) << (g_UserProfile.Last.path / 0.3f) << " ft.";
	else
		ss << std::setprecision(4) << (g_UserProfile.Last.path) << " m.";

	DrawTextShadow(rc.right - 4, 78, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	DrawTextShadow(rc.left + 4, 98, "Time hunted  ", c);
	ss << std::dec << (ltm / 3600) << ":"; // Hours
	ss << std::setfill('0') << std::setw(2) << ((ltm % 3600) / 60) << ":"; // Minutes
	ss << std::setfill('0') << std::setw(2) << (ltm % 60); // Seconds
	DrawTextShadow(rc.right - 4, 98, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	DrawTextShadow(rc.left + 4, 118, "Shots made  ", c);
	ss << g_UserProfile.Last.smade;
	DrawTextShadow(rc.right - 4, 118, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	int accuracy = 0;
	if (g_UserProfile.Last.success > 0)
		accuracy = ((g_UserProfile.Last.success / g_UserProfile.Last.smade) * 100);

	DrawTextShadow(rc.left + 4, 138, "Accuracy  ", c);
	ss << accuracy << "%";
	DrawTextShadow(rc.right - 4, 138, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	/************************** TOTAL STATS **************************/

	DrawTextShadow(rc.left + 4, 208, "Path travelled  ", c);

	if (g_UserProfile.Total.path < 1000)
	{
		if (g_Options.OptSys) ss << std::setprecision(4) << (g_UserProfile.Total.path / 0.3f) << " ft.";
		else                  ss << std::setprecision(4) << (g_UserProfile.Total.path) << " m.";
	}
	else
	{
		if (g_Options.OptSys) ss << std::setprecision(4) << (g_UserProfile.Total.path / 1667.f) << " miles.";
		else                  ss << std::setprecision(4) << (g_UserProfile.Total.path / 1000.f) << " km.";
	}

	DrawTextShadow(rc.right - 4, 208, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	DrawTextShadow(rc.left + 4, 228, "Time hunted  ", c);
	ss << std::dec << (ttm / 3600) << ":"; // Hours
	ss << std::setfill('0') << std::setw(2) << ((ttm % 3600) / 60) << ":"; // Minutes
	ss << std::setfill('0') << std::setw(2) << (ttm % 60); // Seconds
	DrawTextShadow(rc.right - 4, 228, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	DrawTextShadow(rc.left + 4, 248, "Shots made  ", c);
	ss << g_UserProfile.Total.smade;
	DrawTextShadow(rc.right - 4, 248, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	accuracy = 0;
	if (g_UserProfile.Total.success > 0 && g_UserProfile.Total.smade > 0)
		accuracy = static_cast<int>(static_cast<float>((static_cast<float>(g_UserProfile.Total.success) / static_cast<float>(g_UserProfile.Total.smade))) * 100.f);

	DrawTextShadow(rc.left + 4, 268, "Accuracy  ", c);
	ss << accuracy << "%";
	DrawTextShadow(rc.right - 4, 268, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	DrawTextShadow(rc.left + 4, 288, "Rank:", c);

	switch (g_UserProfile.Rank)
	{
	case 0: DrawTextShadow(rc.right - 4, 288, "Novice", c, DTA_RIGHT); break;
	case 1: DrawTextShadow(rc.right - 4, 288, "Advanced", c, DTA_RIGHT); break;
	case 2: DrawTextShadow(rc.right - 4, 288, "Expert", c, DTA_RIGHT); break;
	default: DrawTextShadow(rc.right - 4, 288, "Eldritch", c, DTA_RIGHT); break; // Easter egg/invalid rank
	}

	DrawMenuProfile();
}

/*
*/
void DrawMenuCredits()
{
	uint32_t color = RGB(239, 228, 176);
	std::vector<std::string> contributor_list = {
		/* Please do not remove this name */ "Rexhunter99"
	};

	InterfaceSetFont(fnt_Small);

	DrawURLShadow(550, 42, g_GitHubURL, RGB(126, 178, 239));

	DrawTextShadow(550, 60, "Launcher Code:", color);
	
	auto i = 0U;
	for (auto contributor : contributor_list)
	{
		DrawTextShadow(650, 60 + ((i++) * 15), contributor, color);
	}
}

void DrawMenuHunt()
{
	uint32_t c = RGB(239, 228, 176);
	std::stringstream ss;

	if (g_HuntSelectPic != nullptr && g_HuntSelectPic->IsValid())
	{
		DrawPicture(38, 73, *g_HuntSelectPic);
	}

	InterfaceSetFont(fnt_Big);

	// Draw the profile credits within range of 0 - 9999
	ss << (std::min(9999, std::max(0, g_UserProfile.Score)));
	DrawTextShadow(328 + 8, 38, ss.str(), c, DTA_LEFT);
	ss.str(""); ss.clear();

	// Display the remaining setup budget, not a persistent credit deduction.
	ss << std::clamp<HuntBudget>(RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit), -999, 9999);
	DrawTextShadow(472 - 8, 38, ss.str(), c, DTA_RIGHT);
	ss.str(""); ss.clear();

	InterfaceSetFont(fnt_Midd);

	if (g_HuntInfo.first == 0 && !g_AreaInfo.empty()) // Areas
	{
		for (auto i = 1U; i < g_AreaInfo[g_HuntInfo.second].m_Description.size(); i++)
		{
			DrawTextShadow(424, 96 + ((i-1) * 16), g_AreaInfo[g_HuntInfo.second].m_Description[i], c);
		}
	}
	else if (g_HuntInfo.first == 1 && !g_DinoInfo.empty()) // Dinosaurs
	{
		unsigned d = g_DinoList[g_HuntInfo.second];
		if (!(g_DinoInfo[d].m_Price >= 1000 && g_UserProfile.Score < 1000))
		{
			for (auto i = 0U; i < g_DinoInfo[d].m_Description.size(); i++)
			{
				DrawTextShadow(424, 96 + ((i) * 16), g_DinoInfo[d].m_Description[i], c);
			}

			DrawTextShadow(424, 210 + (0 * 16), "Sight:", c);
			DrawTextShadow(424, 210 + (1 * 16), "Hearing:", c);
			DrawTextShadow(424, 210 + (2 * 16), "Scents:", c);

			DrawProgressBar(424 + 80, 210 + (0 * 16), std::min(1.0f, std::max(0.0f, g_DinoInfo[d].m_LookK)) * 2.f);
			DrawProgressBar(424 + 80, 210 + (1 * 16), std::min(1.0f, std::max(0.0f, g_DinoInfo[d].m_HearK)) * 2.f);
			DrawProgressBar(424 + 80, 210 + (2 * 16), std::min(1.0f, std::max(0.0f, g_DinoInfo[d].m_SmellK)) * 2.f);
		}
	}
	else if (g_HuntInfo.first == 2 && !g_WeapInfo.empty()) // Weapons
	{
		for (auto i = 0U; i < g_WeapInfo[g_HuntInfo.second].m_Description.size(); i++)
		{
			DrawTextShadow(424, 96 + ((i) * 16), g_WeapInfo[g_HuntInfo.second].m_Description[i], c);
		}

		DrawTextShadow(424, 210 + (0 * 16), "Power:", c);
		DrawTextShadow(424, 210 + (1 * 16), "Accuracy:", c);
		DrawTextShadow(424, 210 + (2 * 16), "Volume:", c);

		DrawProgressBar(424 + 80, 210 + (0 * 16), std::min(2.0f, std::max(0.0f, g_WeapInfo[g_HuntInfo.second].m_Power)));
		DrawProgressBar(424 + 80, 210 + (1 * 16), std::min(2.0f, std::max(0.0f, g_WeapInfo[g_HuntInfo.second].m_Prec)));
		DrawProgressBar(424 + 80, 210 + (2 * 16), std::min(2.0f, std::max(0.0f, g_WeapInfo[g_HuntInfo.second].m_Loud)));
	}
	else if (g_HuntInfo.first == 3 && !g_UtilInfo.empty()) // Accessories
	{
		if (g_HuntInfo.second < 9000) {
			for (auto i = 0U; i < g_UtilInfo[g_HuntInfo.second].m_Description.size(); i++)
			{
				DrawTextShadow(424, 96 + ((i) * 16), g_UtilInfo[g_HuntInfo.second].m_Description[i], c);
			}
		}
		else {
			// TODO: Don't hardcode this...
			if (g_HuntInfo.second == 9998)
				for (auto i = 0U; i < g_TranqInfo.m_Description.size(); i++)
				{
					DrawTextShadow(424, 96 + ((i) * 16), g_TranqInfo.m_Description[i], c);
				}
			else if (g_HuntInfo.second == 9999)
				for (auto i = 0U; i < g_ObserverInfo.m_Description.size(); i++)
				{
					DrawTextShadow(424, 96 + ((i) * 16), g_ObserverInfo.m_Description[i], c);
				}
		}
	}

	InterfaceSetFont(fnt_Small);

	HuntBudget score = RemainingHuntBudget(g_UserProfile.Score, g_ScoreDebit);

	for (unsigned ii = MenuHunt[0].Offset;
		ii < HuntListVisibleEnd(MenuHunt[0].Offset, g_AreaInfo.size()); ii++) {
		int i = ii - MenuHunt[0].Offset;
		c = 0xB0B070;

		std::stringstream sc;
		sc << g_AreaInfo[ii].m_Price;

		if (score < g_AreaInfo[ii].m_Price)
			c = 0x707070;

		if (MenuHunt[0].Selected == ii)
		{
			c = RGB(255, 255, 10);
		}

		DrawTextShadow(MenuHunt[0].Rect.left + 4, MenuHunt[0].Rect.top + (16 * i), g_AreaInfo[ii].m_Name, c);
		DrawTextShadow(MenuHunt[0].Rect.right - 4, MenuHunt[0].Rect.top + (16 * i), sc.str(), c, DTA_RIGHT);
	}

	for (unsigned ii = MenuHunt[1].Offset;
		ii < HuntListVisibleEnd(MenuHunt[1].Offset, MenuHunt[1].Item.size()); ii++)
	{
		int i = ii - MenuHunt[1].Offset;
		uint32_t c = 0xB0B070;
		try {
			DinoInfo& di = g_DinoInfo.at(g_DinoList[ii]);
			std::string s = di.m_Name;
			std::stringstream sc;

			sc << di.m_Price;

			// (IceAge)
			// If the dinosaur is worth more than 1000 score and the player has less than, we want to hide what it is
			if (di.m_Price >= 1000 && g_UserProfile.Score < 1000)
			{
				s = "???";
				c = 0x707070;
			}

			if (score < di.m_Price)
			{
				c = 0x707070;
			}

			if (MenuHunt[1].Item[ii].second)
			{
				c = RGB(255, 255, 10);
			}

			DrawTextShadow(MenuHunt[1].Rect.left + 4, MenuHunt[1].Rect.top + (16 * i), s, c);
			DrawTextShadow(MenuHunt[1].Rect.right - 4, MenuHunt[1].Rect.top + (16 * i), sc.str(), c, DTA_RIGHT);
		}
		catch (std::out_of_range& e) {
			throw std::runtime_error(e.what());
		}
	}

	for (unsigned ii = MenuHunt[2].Offset;
		ii < HuntListVisibleEnd(MenuHunt[2].Offset, MenuHunt[2].Item.size()); ii++)
	{
		uint32_t c = 0xB0B070;
		int i = ii - MenuHunt[2].Offset;
		WeapInfo& wi = g_WeapInfo[ii];
		std::stringstream sc;
		sc << wi.m_Price;
		
		if (score < wi.m_Price)
		{
			c = 0x707070;
		}

		if (MenuHunt[2].Item[ii].second)
		{
			c = RGB(255, 255, 10);
		}

		DrawTextShadow(MenuHunt[2].Rect.left + 4, MenuHunt[2].Rect.top + (16 * i), g_WeapInfo[ii].m_Name, c);
		DrawTextShadow(MenuHunt[2].Rect.right - 4, MenuHunt[2].Rect.top + (16 * i), sc.str(), c, DTA_RIGHT);
	}

	for (unsigned ii = MenuHunt[3].Offset;
		ii < HuntListVisibleEnd(MenuHunt[3].Offset, MenuHunt[3].Item.size()); ii++)
	{
		if (ii >= g_UtilInfo.size())
			break;

		uint32_t c = 0xB0B070;
		int i = (int)(ii - MenuHunt[3].Offset);

		std::stringstream sc;
		sc << g_UtilInfo[ii].m_Price;

		// Same affordability grey-out as the other lists: accessories the
		// account cannot cover show dimmed, so their cost is readable even
		// before trying to select them.
		if (score < g_UtilInfo[ii].m_Price)
		{
			c = 0x707070;
		}

		if (MenuHunt[3].Item[ii].second)
		{
			c = RGB(255, 255, 10);
		}

		//DrawTextShadow(MenuHunt[3].Rect.left + 4, MenuHunt[3].Rect.top + (16 * i), MenuHunt[3].Item[ii].first, c);
		DrawTextShadow(MenuHunt[3].Rect.left + 4, MenuHunt[3].Rect.top + (16 * i), g_UtilInfo[ii].m_Name, c);
		DrawTextShadow(MenuHunt[3].Rect.right - 4, MenuHunt[3].Rect.top + (16 * i), sc.str(), c, DTA_RIGHT);
	}
}

/*
Draw the save file 'registry' menu
*/
void DrawMenuRegistry()
{
	POINT& p = g_CursorPos;
	std::stringstream ss;
	uint32_t color = RGB(239, 228, 176);

	if (g_MenuState == MENU_REGISTRY_DELETE)
	{
		InterfaceSetFont(fnt_Midd);
		DrawTextShadow(290, 370, "Do you want to delete player", 0x00B08030);
		ss << "\'" << g_Profiles[g_ProfileIndex].m_Name << "\' ?";
		DrawTextShadow(300, 394, ss.str(), 0x00B08030);
		InterfaceSetFont(0);
	}
	else {
		InterfaceSetFont(fnt_Small);

		if ((timeGetTime() % 800) > 300)
			ss << g_TypingBuffer << "_";
		else
			ss << g_TypingBuffer;

		DrawTextShadow(315, 326, ss.str(), color);

		g_HiliteProfileIndex = g_ProfileIndex;

		// 320, 370
		for (auto i = 0U; i < 7U; i++)
		{
			color = 0xB0B070; // Base colour

			if (i == g_HiliteProfileIndex)
				color = RGB(255, 170, 10);

			if (RegistryRowContains(p, i)) {
				g_HiliteProfileIndex = i; //temporary, move to another function for user input
#ifdef _DEBUG
				color = RGB(42, 255, 42);
#endif
			}

			std::string tname = g_Profiles[i].m_Name;

			if (g_Profiles[i].m_Name.empty()) {
				tname = "...";
			}

			DrawTextShadow(320, 370 + (16 * i), tname, color);

			if (!g_Profiles[i].m_Name.empty())
			{
				std::stringstream score_ss;
				score_ss << g_Profiles[i].m_Score;
				DrawTextShadow(480, 370 + (16 * i), score_ss.str(), color, DTA_RIGHT);
			}
		}

		InterfaceSetFont(0);
	}
}

void DrawMenuOptions()
{
	const int label_c = RGB(181, 134, 82);  // StartLegacy option label color
	const int value_c = RGB(165, 181, 181); // StartLegacy option value color
	const int on_c = RGB(255, 210, 80);

	InterfaceSetFont(g_FontOptions);

	// Game options
	for (auto i = 0U; i < MenuOptions[OPT_GAME].Item.size(); i++) {
		int y0 = g_GameLabels.GetItemY(i);
		int c = (MenuOptions[OPT_GAME].Hilite == i) ? on_c : label_c;

		DrawTextShadow(g_GameLabels.x0, y0, MenuOptions[OPT_GAME].Item[i].first, c, DTA_RIGHT);

		if (i == 0) DrawSliderBar(OptionsLayout::OPTION_SLIDER_X, y0 + 12, OptionsLayout::OPTION_SLIDER_W, static_cast<float>(g_Options.Aggression) / 255.0f, label_c);
		else if (i == 1) DrawSliderBar(OptionsLayout::OPTION_SLIDER_X, y0 + 12, OptionsLayout::OPTION_SLIDER_W, static_cast<float>(g_Options.Density) / 255.0f, label_c);
		else if (i == 2) DrawSliderBar(OptionsLayout::OPTION_SLIDER_X, y0 + 12, OptionsLayout::OPTION_SLIDER_W, static_cast<float>(g_Options.Sensitivity) / 255.0f, label_c);
		else if (i == 3) DrawSliderBar(OptionsLayout::OPTION_SLIDER_X, y0 + 12, OptionsLayout::OPTION_SLIDER_W, static_cast<float>((g_Options.ViewRange - kViewOptMin)) / static_cast<float>((kViewOptMax - kViewOptMin)), label_c);
		else if (i == 4) DrawSliderBar(OptionsLayout::OPTION_SLIDER_X, y0 + 12, OptionsLayout::OPTION_SLIDER_W, static_cast<float>((g_Options.ObjectDetail - kObjectDetailMin)) / static_cast<float>((kObjectDetailMax - kObjectDetailMin)), label_c);
		else DrawTextShadow(g_GameValues.x0, y0, i == 5 ? st_UnitText[g_Options.OptSys] : (i == 6 ? st_AudText[NormalizeAudioBackend(g_Options.SoundAPI)] : st_FpsText[g_Options.OptFpsLimit]), value_c);
	}

	// Control key bindings
	for (auto i = 0U; i < MenuOptions[OPT_KEYBINDINGS].Item.size(); i++) {
		const int row = static_cast<int>(i);

		// Key name string: the two equipment-toggle rows read their own Options
		// members, everything else reads KeyMap.
		std::stringstream ss;
		if (row == ControlsRowIndex(ControlsRow::NightVision)) {
			ss << g_KeyNames[MapVKKey(g_Options.NightVisionKey)];
		} else if (row == ControlsRowIndex(ControlsRow::Flashlight)) {
			ss << g_KeyNames[MapVKKey(g_Options.FlashlightKey)];
		} else {
			ss << g_KeyNames[MapVKKey(*((int32_t*)&g_Options.KeyMap + i))];
		}

		int y0 = g_ControlsLabels.GetItemY(i);
		int c = (MenuOptions[OPT_KEYBINDINGS].Hilite == i) ? on_c : label_c;

		DrawTextShadow(g_ControlsLabels.x0, y0, MenuOptions[OPT_KEYBINDINGS].Item[i].first, c, DTA_RIGHT);

		if (row < FirstControlsRowIndex())
		{
			// KeyMap-bound keys
			if (g_WaitKey == row)
				DrawTextShadow(g_ControlsValues.x0, y0, "<?>", value_c);
			else
				DrawTextShadow(g_ControlsValues.x0, y0, ss.str(), value_c);
		}
		else if (row == ControlsRowIndex(ControlsRow::NightVision) ||
		         row == ControlsRowIndex(ControlsRow::Flashlight))
		{
			// Equipment toggles (stored outside KeyMap: see Options in Hunt.h)
			if (g_WaitKey == row)
				DrawTextShadow(g_ControlsValues.x0, y0, "<?>", value_c);
			else
				DrawTextShadow(g_ControlsValues.x0, y0, ss.str(), value_c);
		}
		else if (row == ControlsRowIndex(ControlsRow::MouseInvert))
			DrawTextShadow(g_ControlsValues.x0, y0, st_BoolText[static_cast<int>(g_Options.MouseInvert)], value_c);
		else if (row == ControlsRowIndex(ControlsRow::MouseSensitivity))
			DrawSliderBar(OptionsLayout::CONTROLS_SLIDER_X, y0 + 12, OptionsLayout::CONTROLS_SLIDER_W, static_cast<float>(g_Options.MouseSensitivity) / 255.0f, label_c);
	}

	// Video/Graphics options
	for (auto i = 0U; i < MenuOptions[OPT_VIDEO].Item.size(); i++) {
		int y0 = g_VideoLabels.GetItemY(i);
		int c = (MenuOptions[OPT_VIDEO].Hilite == i) ? on_c : label_c;

		DrawTextShadow(g_VideoLabels.x0, y0, MenuOptions[OPT_VIDEO].Item[i].first, c, DTA_RIGHT);

		if (i == 0) DrawTextShadow(g_VideoValues.x0, y0, st_RenText[g_Options.RenderAPI], value_c);
		else if (i == 1) {
			// Render the selected resolution from g_ResolutionList[]. The list
			// is built by EnumerateResolutions() at menu startup, so any mode
			// the display supports can be shown. Clamp the index defensively
			// in case the saved value is out of range (e.g., a profile from a
			// 1920x1080 display loaded on a 1366x768 one).
			int idx = g_Options.Resolution;
			if (idx < 0 || idx >= g_ResCount) idx = 0;
			static char resStr[32];
			sprintf(resStr, "%d x %d", g_ResolutionList[idx].w, g_ResolutionList[idx].h);
			DrawTextShadow(g_VideoValues.x0, y0, resStr, value_c);
		}
		else if (i == 2) {
			int dm = g_Options.DisplayMode;
			if (dm < 0 || dm >= kDisplayModeCount) dm = 0;
			DrawTextShadow(g_VideoValues.x0, y0, st_DisplayModeText[dm], value_c);
		}
		else if (i == 3) DrawTextShadow(g_VideoValues.x0, y0, st_BoolText[g_Options.Shadows], value_c);
		else if (i == 4) DrawTextShadow(g_VideoValues.x0, y0, st_BoolText[g_Options.Fog], value_c);
		else if (i == 5) DrawTextShadow(g_VideoValues.x0, y0, st_TextureText[g_Options.Textures], value_c);
		else if (i == 6) DrawTextShadow(g_VideoValues.x0, y0, st_AlphaKeyText[g_Options.AlphaColorKey], value_c);
		else if (i == 7) DrawSliderBar(OptionsLayout::OPTION_SLIDER_X, y0 + 12, OptionsLayout::OPTION_SLIDER_W, static_cast<float>(g_Options.Brightness) / 255.0f, label_c);
		else if (i == 8) {
			float t = static_cast<float>((g_Options.FOV - kFovMin)) / static_cast<float>((kFovMax - kFovMin));
			DrawSliderBar(OptionsLayout::OPTION_SLIDER_X, y0 + 12, OptionsLayout::OPTION_SLIDER_W, t, label_c);
		}
	}

	InterfaceSetFont(nullptr);
}

