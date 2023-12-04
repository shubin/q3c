/*
===========================================================================
Copyright (C) 2022-2023 Gian 'myT' Schellenbaum

This file is part of Challenge Quake 3 (CNQ3).

Challenge Quake 3 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Challenge Quake 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Challenge Quake 3. If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/
// Dear ImGui client integration and utility functions


#pragma once


#include "../imgui/imgui.h"
#include "../implot/implot.h"


bool BeginTable(const char* name, int count);
void TableHeader(int count, ...);
void TableRow(int count, ...);
void TableRow2(const char* item0, bool item1);
void TableRow2(const char* item0, int item1);
void TableRow2(const char* item0, float item1, const char* format = "%g");
bool IsShortcutPressed(ImGuiKey key);
void ToggleBooleanWithShortcut(bool& value, ImGuiKey key);

#define MAIN_MENU_LIST(M) \
	M(Tools, "Tools") \
	M(Info, "Information") \
	M(Perf, "Performance")

#define M(Enum, Desc) Enum,
struct GUI_MainMenu
{
	enum Id
	{
		MAIN_MENU_LIST(M)
		Count
	};
};
#undef M

typedef void (*GUI_MainMenuCallback)();
void GUI_AddMainMenuItem(GUI_MainMenu::Id menu, const char* item, const char* shortcut, bool* selected, bool enabled = true);
void GUI_DrawMainMenu();
