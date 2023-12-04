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


#include "client.h"
#include "cl_imgui.h"
#include "../imgui/font_proggy_clean.h"
#include "../imgui/font_sweet16_mono.h"


static int keyMap[256];


bool BeginTable(const char* name, int count)
{
	ImGui::Text(name);

	const int flags =
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_SizingStretchProp;
	return ImGui::BeginTable(name, count, flags);
}

void TableHeader(int count, ...)
{
	va_list args;
	va_start(args, count);
	for(int i = 0; i < count; ++i)
	{
		const char* header = va_arg(args, const char*);
		ImGui::TableSetupColumn(header);
	}
	va_end(args);

	ImGui::TableHeadersRow();
}

void TableRow(int count, ...)
{
	ImGui::TableNextRow();

	va_list args;
	va_start(args, count);
	for(int i = 0; i < count; ++i)
	{
		const char* item = va_arg(args, const char*);
		ImGui::TableSetColumnIndex(i);
		ImGui::Text(item);
	}
	va_end(args);
}

void TableRow2(const char* item0, bool item1)
{
	TableRow(2, item0, item1 ? "YES" : "NO");
}

void TableRow2(const char* item0, int item1)
{
	TableRow(2, item0, va("%d", item1));
}

void TableRow2(const char* item0, float item1, const char* format)
{
	TableRow(2, item0, va(format, item1));
}

bool IsShortcutPressed(ImGuiKey key)
{
	return ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(key, false);
}

void ToggleBooleanWithShortcut(bool& value, ImGuiKey key)
{
	if(IsShortcutPressed(key))
	{
		value = !value;
	}
}

struct MainMenuItem
{
	GUI_MainMenu::Id menu;
	const char* name;
	const char* shortcut;
	bool* selected;
	bool enabled;
};

struct MainMenu
{
	MainMenuItem items[64];
	int itemCount;
	int itemCountPerMenu[GUI_MainMenu::Count]; // effectively a histogram
};

static MainMenu mm;

#define M(Enum, Desc) Desc,
static const char* mainMenuNames[GUI_MainMenu::Count + 1] =
{
	MAIN_MENU_LIST(M)
	""
};
#undef M

void GUI_AddMainMenuItem(GUI_MainMenu::Id menu, const char* name, const char* shortcut, bool* selected, bool enabled)
{
	if(mm.itemCount >= ARRAY_LEN(mm.items) ||
		(unsigned int)menu >= GUI_MainMenu::Count)
	{
		Q_assert(!"GUI_AddMainMenuItem: can't add menu entry");
		return;
	}

	MainMenuItem& item = mm.items[mm.itemCount++];
	item.menu = menu;
	item.name = name;
	item.shortcut = shortcut;
	item.selected = selected;
	item.enabled = enabled;

	mm.itemCountPerMenu[menu]++;
}

void GUI_DrawMainMenu()
{
	if(ImGui::BeginMainMenuBar())
	{
		for(int m = 0; m < GUI_MainMenu::Count; ++m)
		{
			if(mm.itemCountPerMenu[m] <= 0)
			{
				continue;
			}

			if(ImGui::BeginMenu(mainMenuNames[m]))
			{
				for(int i = 0; i < mm.itemCount; ++i)
				{
					const MainMenuItem& item = mm.items[i];
					if(item.menu == m)
					{
						ImGui::MenuItem(item.name, item.shortcut, item.selected, item.enabled);
					}
				}

				ImGui::EndMenu();
			}
		}

		ImGui::EndMainMenuBar();
	}

	mm.itemCount = 0;
	memset(mm.itemCountPerMenu, 0, sizeof(mm.itemCountPerMenu));
}

// applies a modified version of Jan Bielak's Deep Dark theme
static void ImGUI_ApplyTheme()
{
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
	colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
	colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

	const ImVec4 hover(0.26f, 0.59f, 0.98f, 0.4f);
	const ImVec4 active(0.2f, 0.41f, 0.68f, 0.5f);
	colors[ImGuiCol_HeaderHovered] = hover;
	colors[ImGuiCol_HeaderActive] = active;
	colors[ImGuiCol_TabHovered] = hover;
	colors[ImGuiCol_TabActive] = active;
	colors[ImGuiCol_NavHighlight] = hover;

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(8.00f, 8.00f);
	style.FramePadding = ImVec2(5.00f, 2.00f);
	style.CellPadding = ImVec2(6.00f, 6.00f);
	style.ItemSpacing = ImVec2(6.00f, 6.00f);
	style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 25;
	style.ScrollbarSize = 15;
	style.GrabMinSize = 10;
	style.WindowBorderSize = 1;
	style.ChildBorderSize = 1;
	style.PopupBorderSize = 1;
	style.FrameBorderSize = 1;
	style.TabBorderSize = 1;
	style.WindowRounding = 7;
	style.ChildRounding = 4;
	style.FrameRounding = 3;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 9;
	style.GrabRounding = 3;
	style.LogSliderDeadzone = 4;
	style.TabRounding = 4;
}

static void ToggleGui_f()
{
	const bool guiActive = Cvar_VariableIntegerValue("r_debugUI") != 0;
	const char* const newValue = guiActive ? "0" : "1";
	Cvar_Set("r_debugUI", newValue);
	Cvar_Set("r_debugInput", newValue);
}

static void ToggleGuiInput_f()
{
	Cvar_Set("r_debugInput", Cvar_VariableIntegerValue("r_debugInput") ? "0" : "1");
}

static const cmdTableItem_t imgui_cmds[] =
{
	{ "togglegui", &ToggleGui_f, NULL, "toggles the CNQ3 GUI" },
	{ "toggleguiinput", &ToggleGuiInput_f, NULL, "toggles CNQ3 GUI input capture" }
};

static const char* GetClipboardText(void*)
{
	return Sys_GetClipboardData();
}

static void SetClipboardText(void*, const char* text)
{
	Sys_SetClipboardData(text);
}

static const ImWchar codepointRanges[] =
{
	32, 126,
	0
};

static void AddProggyCleanFont()
{
	ImFontConfig config;
	config.FontDataOwnedByAtlas = false;
	config.OversampleH = 1;
	config.OversampleV = 1;
	config.PixelSnapH = true;
	config.SizePixels = 13.0f;
	Q_strncpyz(config.Name, "Proggy Clean (13px)", sizeof(config.Name));

	ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
		ProggyClean_compressed_data, ProggyClean_compressed_size, config.SizePixels, &config, codepointRanges);
}

static void AddSweet16MonoFont()
{
	ImFontConfig config;
	config.FontDataOwnedByAtlas = false;
	config.OversampleH = 1;
	config.OversampleV = 1;
	config.PixelSnapH = true;
	config.SizePixels = 16.0f;
	Q_strncpyz(config.Name, "Sweet16 Mono (16px)", sizeof(config.Name));

	ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(
		Sweet16mono_compressed_data_base85, config.SizePixels, &config, codepointRanges);
}

static void AddCustomFont()
{
	const char* const filePath = Cvar_VariableString("r_guiFontFile");
	if(filePath == NULL || filePath[0] == '\0')
	{
		return;
	}

	const int height = Cvar_VariableIntegerValue("r_guiFontHeight");

	const char* name = filePath;
	for(int i = strlen(filePath) - 2; i > 0; i--)
	{
		if(filePath[i] == '/' || filePath[i] == '\\')
		{
			name = filePath + i + 1;
			break;
		}
	}

	ImFontConfig config;
	config.FontDataOwnedByAtlas = false;
	config.OversampleH = 1;
	config.OversampleV = 1;
	config.PixelSnapH = true;
	config.SizePixels = height;
	Com_sprintf(config.Name, sizeof(config.Name), "%s (%dpx)", name, height);

	void* data = NULL;
	const int dataSize = FS_ReadFile(filePath, &data);
	if(data == NULL || dataSize <= 0)
	{
		Com_Printf("^3WARNING: failed to open font file: %s\n", filePath);
		return;
	}

	ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, dataSize, config.SizePixels, &config, codepointRanges);
	FS_FreeFile(data);
}

void CL_IMGUI_Init()
{
	Cmd_RegisterArray(imgui_cmds, MODULE_CLIENT);

	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.IniFilename = "cnq3/imgui.ini";
	io.GetClipboardTextFn = &GetClipboardText;
	io.SetClipboardTextFn = &SetClipboardText;
	io.MouseDrawCursor = false; // just use the operating system's

	AddProggyCleanFont();
	AddSweet16MonoFont();
	AddCustomFont();
	const int fontIndex = Cvar_VariableIntegerValue("r_guiFont");
	if(fontIndex >= 0 && fontIndex < io.Fonts->Fonts.Size)
	{
		io.FontDefault = io.Fonts->Fonts[fontIndex];
	}
	else
	{
		io.FontDefault = io.Fonts->Fonts[0];
		Cvar_Set("r_guiFont", "0");
	}

	ImGUI_ApplyTheme();

	memset(keyMap, 0xFF, sizeof(keyMap));
	keyMap[K_CTRL] = ImGuiMod_Ctrl;
	keyMap[K_ALT] = ImGuiMod_Alt;
	keyMap[K_SHIFT] = ImGuiMod_Shift;
	keyMap[K_TAB] = ImGuiKey_Tab;
	keyMap[K_LEFTARROW] = ImGuiKey_LeftArrow;
	keyMap[K_RIGHTARROW] = ImGuiKey_RightArrow;
	keyMap[K_UPARROW] = ImGuiKey_UpArrow;
	keyMap[K_DOWNARROW] = ImGuiKey_DownArrow;
	keyMap[K_PGUP] = ImGuiKey_PageUp;
	keyMap[K_PGDN] = ImGuiKey_PageDown;
	keyMap[K_HOME] = ImGuiKey_Home;
	keyMap[K_END] = ImGuiKey_End;
	keyMap[K_INS] = ImGuiKey_Insert;
	keyMap[K_DEL] = ImGuiKey_Delete;
	keyMap[K_BACKSPACE] = ImGuiKey_Backspace;
	keyMap[K_SPACE] = ImGuiKey_Space;
	keyMap[K_ENTER] = ImGuiKey_Enter;
	keyMap[K_ESCAPE] = ImGuiKey_Escape;
	keyMap[K_CAPSLOCK] = ImGuiKey_CapsLock;
	keyMap[K_PAUSE] = ImGuiKey_Pause;
	keyMap[K_BACKSLASH] = ImGuiKey_Backslash;
	keyMap[K_KP_INS] = ImGuiKey_Keypad0;
	keyMap[K_KP_END] = ImGuiKey_Keypad1;
	keyMap[K_KP_DOWNARROW] = ImGuiKey_Keypad2;
	keyMap[K_KP_PGDN] = ImGuiKey_Keypad3;
	keyMap[K_KP_LEFTARROW] = ImGuiKey_Keypad4;
	keyMap[K_KP_5] = ImGuiKey_Keypad5;
	keyMap[K_KP_RIGHTARROW] = ImGuiKey_Keypad6;
	keyMap[K_KP_HOME] = ImGuiKey_Keypad7;
	keyMap[K_KP_UPARROW] = ImGuiKey_Keypad8;
	keyMap[K_KP_PGUP] = ImGuiKey_Keypad9;
	keyMap[K_KP_ENTER] = ImGuiKey_KeyPadEnter;
	keyMap[K_KP_SLASH] = ImGuiKey_KeypadDivide;
	keyMap[K_KP_MINUS] = ImGuiKey_KeypadSubtract;
	keyMap[K_KP_PLUS] = ImGuiKey_KeypadAdd;
	keyMap[K_KP_STAR] = ImGuiKey_KeypadMultiply;
	keyMap[K_KP_EQUALS] = ImGuiKey_KeypadEqual;
	for(int i = 0; i < 26; ++i)
	{
		keyMap['a' + i] = ImGuiKey_A + i;
	}
	for(int i = 0; i < 10; ++i)
	{
		keyMap['0' + i] = ImGuiKey_0 + i;
	}
	for(int i = 0; i < 12; ++i)
	{
		keyMap[K_F1 + i] = ImGuiKey_F1 + i;
	}
}

void CL_IMGUI_Frame()
{
	if(Cvar_VariableIntegerValue("r_debugInput"))
	{
		cls.keyCatchers |= KEYCATCH_IMGUI;
	}
	else
	{
		cls.keyCatchers &= ~KEYCATCH_IMGUI;
	}

	static int64_t prevUS = 0;
	const int64_t currUS = Sys_Microseconds();
	const int64_t elapsedUS = currUS - prevUS;
	prevUS = currUS;

	int x, y;
	Sys_GetCursorPosition(&x, &y);
	re.ComputeCursorPosition(&x, &y);

	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = (float)((double)elapsedUS / 1000000.0);
	io.MousePos[0] = x;
	io.MousePos[1] = y;
}

void CL_IMGUI_MouseEvent(int dx, int dy)
{
	ImGui::GetIO().AddMousePosEvent(dx, dy);
}

qbool CL_IMGUI_KeyEvent(int key, qbool down, const char* cmd)
{
	if(down)
	{
		if(cmd != NULL)
		{
			const char* const prefix = "keycatchgui";
			if(Q_stristr(cmd, prefix) == cmd)
			{
				Cbuf_AddText(cmd + strlen(prefix));
				Cbuf_AddText("\n");
				return qtrue;
			}
		}
	}

	if(cls.keyCatchers & KEYCATCH_IMGUI)
	{
		if(down && (key == '`' || key == '~'))
		{
			// continue displaying the GUI but route input to the console
			Cvar_Set("r_debugInput", "0");
			return qfalse;
		}

		unsigned int imguiKey;
		ImGuiIO& io = ImGui::GetIO();
		switch(key)
		{
			case K_MOUSE1:
			case K_MOUSE2:
			case K_MOUSE3:
			case K_MOUSE4:
			case K_MOUSE5:
				io.AddMouseButtonEvent(key - K_MOUSE1, !!down);
				break;
			case K_MWHEELDOWN:
			case K_MWHEELUP:
				io.AddMouseWheelEvent(0.0f, key == K_MWHEELDOWN ? -1.0f : 1.0f);
				break;
			default:
				imguiKey = (unsigned int)keyMap[key];
				if(imguiKey != 0xFFFFFFFF)
				{
					io.AddKeyEvent((ImGuiKey)imguiKey, !!down);
				}
				break;
		}
		
		return qtrue;
	}

	return qfalse;
}

void CL_IMGUI_CharEvent(char key)
{
	ImGui::GetIO().AddInputCharacter(key);
}

void CL_IMGUI_Shutdown()
{
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	Cmd_UnregisterArray(imgui_cmds);
}

qbool CL_IMGUI_IsCustomFontLoaded(const char** debugName)
{
	if(ImGui::GetIO().Fonts->Fonts.Size != 3)
	{
		return qfalse;
	}

	if(debugName != NULL)
	{
		*debugName = ImGui::GetIO().Fonts->Fonts[2]->GetDebugName();
	}
	
	return qtrue;
}
