/*
===========================================================================
Copyright (C) 2023 Gian 'myT' Schellenbaum

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
// Main renderer GUI tools


#include "tr_local.h"
#include "../client/cl_imgui.h"


qbool CL_IMGUI_IsCustomFontLoaded(const char** customFontName);


#define IMAGE_WINDOW_NAME  "Image Details"
#define SHADER_WINDOW_NAME "Shader Details"
#define EDIT_WINDOW_NAME   "Shader Editor"


struct ImageWindow
{
	const image_t* image;
	bool active;
	int mip;
};

struct ShaderWindow
{
	char formattedCode[4096];
	shader_t* shader;
	bool active;
};

struct ShaderReplacement
{
	shader_t original;
	int index;
};

struct ShaderReplacements
{
	ShaderReplacement shaders[16];
	int count;
};

struct ImageReplacement
{
	image_t original;
	int index;
};

struct ImageReplacements
{
	ImageReplacement images[16];
	int count;
};

struct ShaderEditWindow
{
	char code[4096];
	shader_t originalShader;
	shader_t* shader;
	bool active;
};

static ImageWindow imageWindow;
static ShaderWindow shaderWindow;
static ShaderReplacements shaderReplacements;
static ImageReplacements imageReplacements;
static ShaderEditWindow shaderEditWindow;
static char cvarFilter[256];

static const char* mipNames[16] =
{
	"Mip 0",
	"Mip 1",
	"Mip 2",
	"Mip 3",
	"Mip 4",
	"Mip 5",
	"Mip 6",
	"Mip 7",
	"Mip 8",
	"Mip 9",
	"Mip 10",
	"Mip 11",
	"Mip 12",
	"Mip 13",
	"Mip 14",
	"Mip 15"
};

struct ImageFlag
{
	int flag;
	const char* description;
};

static const ImageFlag imageFlags[] =
{
	{ IMG_NOPICMIP, "'nopicmip'" },
	{ IMG_NOMIPMAP, "'nomipmap'" },
	{ IMG_NOIMANIP, "'noimanip'" },
	{ IMG_LMATLAS, "int. LM" },
	{ IMG_EXTLMATLAS, "ext. LM" },
	{ IMG_NOAF, "no AF" }
};

// RGB triplets for ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
static const float cpmaColorCodes[] =
{
	1.000000f, 0.000000f, 0.000000f,
	1.000000f, 0.267949f, 0.000000f,
	1.000000f, 0.500000f, 0.000000f,
	1.000000f, 0.732051f, 0.000000f,
	1.000000f, 1.000000f, 0.000000f,
	0.732051f, 1.000000f, 0.000000f,
	0.500000f, 1.000000f, 0.000000f,
	0.267949f, 1.000000f, 0.000000f,
	0.000000f, 1.000000f, 0.000000f,
	0.000000f, 1.000000f, 0.267949f,
	0.000000f, 1.000000f, 0.500000f,
	0.000000f, 1.000000f, 0.732051f,
	0.000000f, 1.000000f, 1.000000f,
	0.000000f, 0.732051f, 1.000000f,
	0.000000f, 0.500000f, 1.000000f,
	0.000000f, 0.267949f, 1.000000f,
	0.000000f, 0.000000f, 1.000000f,
	0.267949f, 0.000000f, 1.000000f,
	0.500000f, 0.000000f, 1.000000f,
	0.732051f, 0.000000f, 1.000000f,
	1.000000f, 0.000000f, 1.000000f,
	1.000000f, 0.000000f, 0.732051f,
	1.000000f, 0.000000f, 0.500000f,
	1.000000f, 0.000000f, 0.267949f,
	0.250000f, 0.250000f, 0.250000f,
	0.500000f, 0.500000f, 0.500000f,
	0.000000f, 0.000000f, 0.000000f,
	1.000000f, 0.000000f, 0.000000f,
	0.000000f, 1.000000f, 0.000000f,
	1.000000f, 1.000000f, 0.000000f,
	0.200000f, 0.200000f, 1.000000f,
	0.000000f, 1.000000f, 1.000000f,
	1.000000f, 0.000000f, 1.000000f,
	1.000000f, 1.000000f, 1.000000f,
	0.750000f, 0.750000f, 0.750000f,
	0.600000f, 0.600000f, 1.000000f
};


#if 0
static void SelectableText(const char* text)
{
	char buffer[256];
	Q_strncpyz(buffer, text, sizeof(buffer));

	ImGui::PushID(text);
	ImGui::PushItemWidth(ImGui::GetWindowSize().x);
	ImGui::InputText("", buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();
	ImGui::PopID();
}
#endif

static bool IsNullOrEmpty(const char* string)
{
	return string == NULL || string[0] == '\0';
}

static bool IsNonEmpty(const char* string)
{
	return string != NULL && string[0] != '\0';
}

static void SetTooltipIfValid(const char* text)
{
	if(IsNonEmpty(text) && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
	{
		ImGui::SetTooltip(text);
	}
}

static ImVec4 GetColorFromCPMACode(char colorCode)
{
	int colorIndex = 0;
	if(colorCode >= 'A' && colorCode <= 'Z')
	{
		colorIndex = colorCode - 'A';
	}
	else if(colorCode >= 'a' && colorCode <= 'z')
	{
		colorIndex = colorCode - 'a';
	}
	else if(colorCode >= '0' && colorCode <= '9')
	{
		colorIndex = 26 + (colorCode - '0');
	}

	const float* colorPointer = cpmaColorCodes + colorIndex * 3;
	const ImVec4 color(colorPointer[0], colorPointer[1], colorPointer[2], 1.0f);

	return color;
}

static float InverseToneMap(float x)
{
	return powf(x / r_brightness->value, r_gamma->value);
}

static bool CPMAColorCodeButton(const char* title, const char* id, char& currentColor)
{
	const char* const popupId = va("##color_popup_%s", id);
	const ImGuiColorEditFlags previewFlags = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoAlpha;
	bool clicked = false;

	ImGui::Text(va("%c", currentColor));
	ImGui::SameLine();

	if(ImGui::ColorButton(va("##%s", id), GetColorFromCPMACode(currentColor), previewFlags))
	{
		ImGui::OpenPopup(popupId);
	}

	if(ImGui::BeginPopup(popupId))
	{
		if(title != NULL && title[0] != '\0')
		{
			ImGui::Text(title);
		}

		const char* rows[] =
		{
			"ABCDEF",
			"GHIJKL",
			"MNOPQR",
			"STUVWX",
			"YZ0123",
			"456789"
		};
		for(int y = 0; y < ARRAY_LEN(rows); ++y)
		{
			const char* row = rows[y];
			for(int x = 0; row[x] != '\0'; ++x)
			{
				if(x > 0)
				{
					ImGui::SameLine();
				}

				const char colorCode = row[x];
				const ImGuiColorEditFlags buttonFlags = ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_NoAlpha;
				ImVec4 color = GetColorFromCPMACode(colorCode);
				color.x = InverseToneMap(color.x);
				color.y = InverseToneMap(color.y);
				color.z = InverseToneMap(color.z);
				if(ImGui::ColorButton(va("%c", colorCode), color, buttonFlags))
				{
					currentColor = colorCode;
					clicked = true;
				}
			}
		}

		ImGui::EndPopup();
	}

	return clicked;
}

static const char* RemoveColorCodes(const char* text)
{
	static char buffer[1024];
	Q_strncpyz(buffer, text, sizeof(buffer));

	char* d = buffer;
	char* s = buffer;
	int c;
	while((c = *s) != 0)
	{
		if(Q_IsColorString(s))
		{
			s++;
		}
		else
		{
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return buffer;
}

static void FormatShaderCode(char* dest, int destSize, const shader_t* shader)
{
#define Append(Text) Q_strcat(dest, destSize, Text)

	dest[0] = '\0';
	if(shader->text == NULL)
	{
		return;
	}

	const char* s = shader->text;
	int tabs = 0;
	for(;;)
	{
		const char c0 = s[0];
		const char c1 = s[1];
		if(c0 == '{')
		{
			tabs++;
			Append("{");
		}
		else if(c0 == '\n')
		{
			Append("\n");
			if(c1 == '}')
			{
				tabs--;
				if(tabs == 0)
				{
					Append("}\n");
					return;
				}
			}
			for(int i = 0; i < tabs; i++)
			{
				Append("    ");
			}
		}
		else
		{
			Append(va("%c", c0));
		}
		s++;
	}

#undef Append
}

static void TitleText(const char* text)
{
	ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.0f, 1.0f), text);
}

static void OpenImageDetails(const image_t* image)
{
	ImGui::SetWindowFocus(IMAGE_WINDOW_NAME);

	imageWindow.active = true;
	imageWindow.image = image;
	imageWindow.mip = 0;
}

static void OpenShaderDetails(shader_t* shader)
{
	ImGui::SetWindowFocus(SHADER_WINDOW_NAME);

	shaderWindow.active = true;
	shaderWindow.shader = shader;
	FormatShaderCode(shaderWindow.formattedCode, sizeof(shaderWindow.formattedCode), shader);
}

static void OpenShaderEdit(shader_t* shader)
{
	ImGui::SetWindowFocus(EDIT_WINDOW_NAME);

	if(shaderEditWindow.active)
	{
		R_SetShaderData(shaderEditWindow.shader, &shaderEditWindow.originalShader);
	}

	shaderEditWindow.active = true;
	shaderEditWindow.shader = shader;
	shaderEditWindow.originalShader = *shader;
	FormatShaderCode(shaderEditWindow.code, sizeof(shaderEditWindow.code), shader);
	tr.shaderParseFailed = qfalse;
	tr.shaderParseNumWarnings = 0;
}

static bool IsCheating()
{
	return ri.Cvar_Get("sv_cheats", "0", 0)->integer != 0;
}

static void RemoveShaderReplacement(int shaderIndex)
{
	for(int i = 0; i < shaderReplacements.count; ++i)
	{
		const ShaderReplacement& repl = shaderReplacements.shaders[i];
		if(shaderIndex == repl.index && shaderIndex >= 0 && shaderIndex < tr.numShaders)
		{
			*tr.shaders[repl.index] = repl.original;
			if(i < shaderReplacements.count - 1)
			{
				shaderReplacements.shaders[i] = shaderReplacements.shaders[shaderReplacements.count - 1];
			}
			shaderReplacements.count--;
			break;
		}
	}
}

static void AddShaderReplacement(int shaderIndex)
{
	if(shaderReplacements.count >= ARRAY_LEN(shaderReplacements.shaders))
	{
		return;
	}

	if(shaderIndex < 0 || shaderIndex >= tr.numShaders)
	{
		return;
	}

	for(int i = 0; i < shaderReplacements.count; ++i)
	{
		if(shaderReplacements.shaders[i].index == shaderIndex)
		{
			return;
		}
	}

	ShaderEditWindow& edit = shaderEditWindow;
	if(edit.active && edit.shader->index == shaderIndex)
	{
		RemoveShaderReplacement(shaderIndex);
		edit.active = false;
	}

	ShaderReplacement& repl = shaderReplacements.shaders[shaderReplacements.count++];
	repl.index = shaderIndex;
	repl.original = *tr.shaders[shaderIndex];
	*tr.shaders[shaderIndex] = *tr.defaultShader;
	Q_strncpyz(tr.shaders[shaderIndex]->name, repl.original.name, sizeof(tr.shaders[shaderIndex]->name));
	tr.shaders[shaderIndex]->index = repl.original.index;
	tr.shaders[shaderIndex]->sortedIndex = repl.original.sortedIndex;
}

static bool IsReplacedShader(int shaderIndex)
{
	for(int i = 0; i < shaderReplacements.count; ++i)
	{
		const ShaderReplacement& repl = shaderReplacements.shaders[i];
		if(shaderIndex == repl.index)
		{
			return true;
		}
	}

	return false;
}

static void ClearShaderReplacements()
{
	for(int i = 0; i < shaderReplacements.count; ++i)
	{
		const ShaderReplacement& sr = shaderReplacements.shaders[i];
		if(sr.index >= 0 && sr.index < tr.numShaders)
		{
			*tr.shaders[sr.index] = sr.original;
		}
	}
	shaderReplacements.count = 0;
}

static void AddImageReplacement(int imageIndex)
{
	if(imageReplacements.count >= ARRAY_LEN(imageReplacements.images))
	{
		return;
	}

	if(imageIndex < 0 || imageIndex >= tr.numImages)
	{
		return;
	}

	for(int i = 0; i < imageReplacements.count; ++i)
	{
		if(imageReplacements.images[i].index == imageIndex)
		{
			return;
		}
	}

	ImageReplacement& repl = imageReplacements.images[imageReplacements.count++];
	repl.index = imageIndex;
	repl.original = *tr.images[imageIndex];
	*tr.images[imageIndex] = *tr.defaultImage;
	Q_strncpyz(tr.images[imageIndex]->name, repl.original.name, sizeof(tr.images[imageIndex]->name));
	tr.images[imageIndex]->index = repl.original.index;
}

static void RemoveImageReplacement(int imageIndex)
{
	for(int i = 0; i < imageReplacements.count; ++i)
	{
		const ImageReplacement& repl = imageReplacements.images[i];
		if(imageIndex == repl.index && imageIndex >= 0 && imageIndex < tr.numImages)
		{
			*tr.images[repl.index] = repl.original;
			if(i < imageReplacements.count - 1)
			{
				imageReplacements.images[i] = imageReplacements.images[imageReplacements.count - 1];
			}
			imageReplacements.count--;
			break;
		}
	}
}

static bool IsReplacedImage(int imageIndex)
{
	for(int i = 0; i < imageReplacements.count; ++i)
	{
		const ImageReplacement& repl = imageReplacements.images[i];
		if(imageIndex == repl.index)
		{
			return true;
		}
	}

	return false;
}

static void ClearImageReplacements()
{
	for(int i = 0; i < imageReplacements.count; ++i)
	{
		const ImageReplacement& repl = imageReplacements.images[i];
		if(repl.index >= 0 && repl.index < tr.numImages)
		{
			*tr.images[repl.index] = repl.original;
		}
	}
	imageReplacements.count = 0;
}

static void DrawFilter(char* filter, size_t filterSize)
{
	if(ImGui::Button("Clear filter"))
	{
		filter[0] = '\0';
	}
	ImGui::SameLine();
	if(ImGui::IsWindowAppearing())
	{
		ImGui::SetKeyboardFocusHere();
	}
	ImGui::InputText(" ", filter, filterSize);
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
	{
		ImGui::SetTooltip("Use * to match any character any amount of times.");
	}
}

static void DrawImageToolTip(const image_t* image)
{
	const float scaleX = 128.0f / image->width;
	const float scaleY = 128.0f / image->height;
	const float scale = min(scaleX, scaleY);
	const float w = max(1.0f, scale * (float)image->width);
	const float h = max(1.0f, scale * (float)image->height);
	ImGui::BeginTooltip();
	ImGui::Image((ImTextureID)image->textureIndex, ImVec2(w, h));
	ImGui::EndTooltip();
}

static void DrawImageList()
{
	static bool listActive = false;
	ToggleBooleanWithShortcut(listActive, ImGuiKey_I);
	GUI_AddMainMenuItem(GUI_MainMenu::Tools, "Image Explorer", "Ctrl+I", &listActive);
	if(listActive)
	{
		if(ImGui::Begin("Image Explorer", &listActive))
		{
			if(imageReplacements.count > 0 && ImGui::Button("Restore Images"))
			{
				ClearImageReplacements();
			}

			static char rawFilter[256];
			DrawFilter(rawFilter, sizeof(rawFilter));

			if(BeginTable("Images", 1))
			{
				for(int i = 0; i < tr.numImages; ++i)
				{
					const image_t* image = tr.images[i];

					char filter[256];
					Com_sprintf(filter, sizeof(filter), rawFilter[0] == '*' ? "%s" : "*%s", rawFilter);
					if(filter[0] != '\0' && !Com_Filter(filter, image->name))
					{
						continue;
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if(ImGui::Selectable(va("%s##%d", image->name, i), false))
					{
						OpenImageDetails(image);
					}
					else if(ImGui::IsItemHovered())
					{
						DrawImageToolTip(image);
					}
				}

				ImGui::EndTable();
			}
		}

		ImGui::End();
	}
}

static void DrawImageWindow()
{
	ImageWindow& window = imageWindow;
	if(window.active)
	{
		if(ImGui::Begin(IMAGE_WINDOW_NAME, &window.active, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const image_t* const image = window.image;

			TitleText(image->name);

			char pakName[256];
			if(FS_GetPakPath(pakName, sizeof(pakName), image->pakChecksum))
			{
				ImGui::Text(pakName);
			}

			ImGui::Text("%dx%d", image->width, image->height);
			if(image->wrapClampMode == TW_CLAMP_TO_EDGE)
			{
				ImGui::SameLine();
				ImGui::Text("'clampMap'");
			}
			for(int f = 0; f < ARRAY_LEN(imageFlags); ++f)
			{
				if(image->flags & imageFlags[f].flag)
				{
					ImGui::SameLine();
					ImGui::Text(imageFlags[f].description);
				}
			}

			if(IsCheating())
			{
				if(IsReplacedImage(image->index))
				{
					if(ImGui::Button("Restore Image"))
					{
						RemoveImageReplacement(image->index);
					}
				}
				else
				{
					if(ImGui::Button("Replace Image"))
					{
						AddImageReplacement(image->index);
					}
				}
			}

			ImGui::NewLine();
			ImGui::Text("Shaders:");
			for(int is = 0; is < ARRAY_LEN(tr.imageShaders); ++is)
			{
				const int i = tr.imageShaders[is] & 0xFFFF;
				if(i != image->index)
				{
					continue;
				}

				const int s = (tr.imageShaders[is] >> 16) & 0xFFFF;
				const shader_t* const shader = tr.shaders[s];
				if(ImGui::Selectable(va("%s##%d", shader->name, is), false))
				{
					OpenShaderDetails((shader_t*)shader);
				}
				else if(ImGui::IsItemHovered())
				{
					const char* const shaderPath = R_GetShaderPath(shader);
					if(shaderPath != NULL)
					{
						ImGui::BeginTooltip();
						ImGui::Text(shaderPath);
						ImGui::EndTooltip();
					}
				}
			}
			ImGui::NewLine();

			int width = image->width;
			int height = image->height;
			if((image->flags & IMG_NOMIPMAP) == 0)
			{
				ImGui::Combo("Mip", &window.mip, mipNames, R_ComputeMipCount(width, height));
			}
			for(int m = 0; m < window.mip; ++m)
			{
				width = max(width / 2, 1);
				height = max(height / 2, 1);
			}

			const ImTextureID textureId =
				(ImTextureID)image->textureIndex |
				(ImTextureID)(window.mip << 16);
			ImGui::Image(textureId, ImVec2(width, height));
		}

		ImGui::End();
	}
}

static void DrawShaderList()
{
	static bool listActive = false;
	ToggleBooleanWithShortcut(listActive, ImGuiKey_S);
	GUI_AddMainMenuItem(GUI_MainMenu::Tools, "Shader Explorer", "Ctrl+S", &listActive);
	if(listActive)
	{
		if(ImGui::Begin("Shader Explorer", &listActive))
		{
			if(shaderReplacements.count > 0 && ImGui::Button("Restore Shaders"))
			{
				ClearShaderReplacements();
			}

			if(tr.world != NULL)
			{
				if(tr.traceWorldShader)
				{
					if(ImGui::Button("Disable world shader tracing"))
					{
						tr.traceWorldShader = qfalse;
					}
					if((uint32_t)tr.tracedWorldShaderIndex < (uint32_t)tr.numShaders)
					{
						shader_t* shader = tr.shaders[tr.tracedWorldShaderIndex];
						if(ImGui::Selectable(va("%s##world_shader_trace", shader->name), false))
						{
							OpenShaderDetails(shader);
						}
					}
				}
				else
				{
					if(ImGui::Button("Enable world shader tracing"))
					{
						tr.traceWorldShader = qtrue;
					}
				}
			}

			static char rawFilter[256];
			DrawFilter(rawFilter, sizeof(rawFilter));

			if(BeginTable("Shaders", 1))
			{
				for(int s = 0; s < tr.numShaders; ++s)
				{
					shader_t* shader = tr.shaders[s];

					char filter[256];
					Com_sprintf(filter, sizeof(filter), rawFilter[0] == '*' ? "%s" : "*%s", rawFilter);
					if(filter[0] != '\0' && !Com_Filter(filter, shader->name))
					{
						continue;
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if(ImGui::Selectable(va("%s##%d", shader->name, s), false))
					{
						OpenShaderDetails(shader);
					}
				}

				ImGui::EndTable();
			}
		}

		ImGui::End();
	}
}

static void DrawShaderWindow()
{
	ShaderWindow& window = shaderWindow;
	if(window.active)
	{
		if(ImGui::Begin(SHADER_WINDOW_NAME, &window.active, ImGuiWindowFlags_AlwaysAutoResize))
		{
			shader_t* shader = window.shader;
			TitleText(shader->name);

			const char* const shaderPath = R_GetShaderPath(shader);
			if(shaderPath != NULL)
			{
				ImGui::Text(shaderPath);
			}

			const bool isReplaced = IsReplacedShader(shader->index);
			const bool isCheating = IsCheating();

			if(isCheating)
			{
				if(isReplaced)
				{
					if(ImGui::Button("Restore Shader"))
					{
						RemoveShaderReplacement(shader->index);
					}
				}
				else
				{
					if(ImGui::Button("Replace Shader"))
					{
						AddShaderReplacement(shader->index);
					}
				}
			}

			ImGui::NewLine();
			ImGui::Text("Images:");
			if(shader->isSky)
			{
				for(int i = 0; i < 6; ++i)
				{
					const image_t* image = shader->sky.outerbox[i];
					if(image == NULL)
					{
						continue;
					}

					if(ImGui::Selectable(va("%s##skybox_%d", image->name, i), false))
					{
						OpenImageDetails(image);
					}
					else if(ImGui::IsItemHovered())
					{
						DrawImageToolTip(image);
					}
				}
			}

			const image_t* displayImages[MAX_IMAGE_ANIMATIONS * MAX_SHADER_STAGES];
			int displayImageCount = 0;
			for(int s = 0; s < shader->numStages; ++s)
			{
				const textureBundle_t& bundle = shader->stages[s]->bundle;
				const int imageCount = max(bundle.numImageAnimations, 1);
				for(int i = 0; i < imageCount; ++i)
				{
					const image_t* image = bundle.image[i];

					bool found = false;
					for(int di = 0; di < displayImageCount; ++di)
					{
						if(displayImages[di] == image)
						{
							found = true;
							break;
						}
					}
					if(found)
					{
						continue;
					}
					if(displayImageCount < ARRAY_LEN(displayImages))
					{
						displayImages[displayImageCount++] = image;
					}

					if(ImGui::Selectable(va("%s##%d_%d", image->name, s, i), false))
					{
						OpenImageDetails(image);
					}
					else if(ImGui::IsItemHovered())
					{
						DrawImageToolTip(image);
					}
				}
			}
			ImGui::NewLine();

			if(isReplaced)
			{
				ImGui::Text("No code available (using default shader)");
			}
			else if(shaderEditWindow.active && window.shader == shaderEditWindow.shader)
			{
				ImGui::Text("Shader code is being edited");
				if(shaderEditWindow.code[0] != '\0')
				{
					ImGui::Text("This is the current edited code, not the last successful compile");
					ImGui::TextUnformatted(shaderEditWindow.code);
				}
			}
			else if(window.formattedCode[0] != '\0')
			{
				ImGui::TextUnformatted(window.formattedCode);
				ImGui::NewLine();
				if(ImGui::Button("Copy Code"))
				{
					ImGui::SetClipboardText(window.formattedCode);
				}
			}
			else
			{
				ImGui::Text("No code available");
			}

			if(isCheating)
			{
				if(!shaderEditWindow.active || window.shader != shaderEditWindow.shader)
				{
					ImGui::NewLine();
					if(ImGui::Button("Edit Shader"))
					{
						if(isReplaced)
						{
							RemoveShaderReplacement(shader->index);
						}
						OpenShaderEdit(shader);
					}
				}
			}
		}

		ImGui::End();
	}
}

static int ShaderEditCallback(ImGuiInputTextCallbackData* data)
{
	data->InsertChars(data->CursorPos, "    ");
	return 1;
}

static void DrawShaderEdit()
{
	ShaderEditWindow& window = shaderEditWindow;
	const bool wasActive = window.active;
	if(window.active)
	{
		if(ImGui::Begin(EDIT_WINDOW_NAME, &window.active, ImGuiWindowFlags_AlwaysAutoResize))
		{
			shader_t* shader = window.shader;
			TitleText(shader->name);

			if(IsCheating())
			{
				ImGui::NewLine();

				if(ImGui::IsWindowAppearing())
				{
					ImGui::SetKeyboardFocusHere();
				}
				const ImVec2 xSize = ImGui::CalcTextSize("X");
				const ImVec2 inputSize = ImVec2(xSize.x * 60.0f, xSize.y * 30.0f);
				ImGui::InputTextMultiline(
					"##Shader Code", window.code, sizeof(window.code), inputSize,
					ImGuiInputTextFlags_CallbackCompletion, &ShaderEditCallback);

				ImGui::NewLine();
				if(ImGui::Button("Apply Code"))
				{
					R_EditShader(window.shader, &window.originalShader, window.code);
				}
				ImGui::SameLine(inputSize.x - ImGui::CalcTextSize("Close and Discard").x);
				if(ImGui::Button("Close and Discard"))
				{
					window.active = false;
				}

				if(tr.shaderParseFailed || tr.shaderParseNumWarnings > 0)
				{
					ImGui::NewLine();
				}
				if(tr.shaderParseFailed)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.0f, 1.0f), "Error: %s", tr.shaderParseError);
				}
				for(int i = 0; i < tr.shaderParseNumWarnings; ++i)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: %s", tr.shaderParseWarnings[i]);
				}
			}
			else
			{
				ImGui::Text("Enable cheats to use");
			}
		}

		ImGui::End();
	}

	if(wasActive && !window.active)
	{
		R_SetShaderData(window.shader, &window.originalShader);
	}
}

static void DrawFrontEndStats()
{
	static bool active = false;
	GUI_AddMainMenuItem(GUI_MainMenu::Perf, "Front-end stats", "", &active);
	if(active)
	{
		if(ImGui::Begin("Front-end stats", &active, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if(BeginTable("General stats", 2))
			{
				struct Item
				{
					int index;
					const char* title;
				};
				const Item items[] =
				{
					{ RF_LEAF_CLUSTER, "PVS cluster" },
					{ RF_LEAF_AREA, "PVS area" },
					{ RF_LEAFS, "Leaves" },
					{ RF_LIT_LEAFS, "Lit leaves" },
					{ RF_LIT_SURFS, "Lit surfaces" },
					{ RF_LIT_CULLS, "Lit surfaces culled" },
					{ RF_LIGHT_CULL_IN, "Lights kept" },
					{ RF_LIGHT_CULL_OUT, "Lights rejected" }
				};

				for(int i = 0; i < ARRAY_LEN(items); ++i)
				{
					const Item& item = items[i];
					TableRow(2, item.title, va("%d", tr.pc[item.index]));
				}

				ImGui::EndTable();
			}

			if(BeginTable("Culling stats", 4))
			{
				struct Item
				{
					int indexIn;
					int indexClip;
					int indexOut;
					const char* title;
				};
				const Item items[] =
				{
					{ RF_MD3_CULL_S_IN, RF_MD3_CULL_S_CLIP, RF_MD3_CULL_S_OUT, "MD3 vs sphere" },
					{ RF_MD3_CULL_B_IN, RF_MD3_CULL_B_CLIP, RF_MD3_CULL_B_OUT, "MD3 vs box" },
					{ RF_BEZ_CULL_S_IN, RF_BEZ_CULL_S_CLIP, RF_BEZ_CULL_S_OUT, "Grid vs sphere" },
					{ RF_BEZ_CULL_B_IN, RF_BEZ_CULL_B_CLIP, RF_BEZ_CULL_B_OUT, "Grid vs box" }
				};

				ImGui::TableSetupColumn("Surface");
				ImGui::TableSetupColumn("In", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFont()->FontSize * 4.0f);
				ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFont()->FontSize * 4.0f);
				ImGui::TableSetupColumn("Out", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFont()->FontSize * 4.0f);
				ImGui::TableHeadersRow();
				for(int i = 0; i < ARRAY_LEN(items); ++i)
				{
					const Item& item = items[i];
					TableRow(4, item.title,
						va("%d", tr.pc[item.indexIn]),
						va("%d", tr.pc[item.indexClip]),
						va("%d", tr.pc[item.indexOut]));
				}

				ImGui::EndTable();
			}
		}

		ImGui::End();
	}
}

static ImVec4 ParseHexRGB(const char* text)
{
	float c[4];
	Com_ParseHexColor(c, text, qfalse);

	return ImVec4(c[0], c[1], c[2], 1.0f);
}

static ImVec4 ParseHexRGBA(const char* text)
{
	float c[4];
	Com_ParseHexColor(c, text, qtrue);

	return ImVec4(c[0], c[1], c[2], c[3]);
}

static float GetVerticalFov(float hfov)
{
	const float w = 1.0f;
	const float h = (float)glConfig.vidHeight / (float)glConfig.vidWidth;
	const float adj = w / tanf(DEG2RAD(0.5f * hfov));
	const float vfov = 2.0f * RAD2DEG(atanf(h / adj));

	return vfov;
}

static float GetHorizontalFov(float vfov)
{
	const float w = 1.0f;
	const float h = (float)glConfig.vidHeight / (float)glConfig.vidWidth;
	const float adj = h / tanf(DEG2RAD(0.5f * vfov));
	const float hfov = 2.0f * RAD2DEG(atanf(w / adj));

	return hfov;
}

static void DrawCVarValue(cvar_t* cvar)
{
	if(cvar->flags & (CVAR_ROM | CVAR_INIT))
	{
		ImGui::Text(cvar->string);
		return;
	}

	if(cvar->type == CVART_COLOR_RGB)
	{
		const char* const id = cvar->name;
		const char* const oldValue = cvar->latchedString != NULL ? cvar->latchedString : cvar->string;
		const ImVec4 color = ParseHexRGB(oldValue);
		const ImGuiColorEditFlags flags =
			ImGuiColorEditFlags_NoLabel |
			ImGuiColorEditFlags_NoInputs |
			ImGuiColorEditFlags_InputRGB |
			ImGuiColorEditFlags_NoAlpha |
			ImGuiColorEditFlags_PickerHueWheel;
		float outColor[4] = { color.x, color.y, color.z, 1.0f };

		if(ImGui::ColorEdit3(va("%s##picker_%s", cvar->gui.title, id), outColor, flags))
		{
			const unsigned int r = outColor[0] * 255.0f;
			const unsigned int g = outColor[1] * 255.0f;
			const unsigned int b = outColor[2] * 255.0f;
			Cvar_Set2(cvar->name, va("%02X%02X%02X", r, g, b), 0);
		}
	}
	else if(cvar->type == CVART_COLOR_RGBA)
	{
		const char* const id = cvar->name;
		const char* const oldValue = cvar->latchedString != NULL ? cvar->latchedString : cvar->string;
		const ImVec4 color = ParseHexRGBA(oldValue);
		const ImGuiColorEditFlags flags =
			ImGuiColorEditFlags_NoLabel |
			ImGuiColorEditFlags_NoInputs |
			ImGuiColorEditFlags_InputRGB |
			ImGuiColorEditFlags_AlphaPreview |
			ImGuiColorEditFlags_AlphaBar |
			ImGuiColorEditFlags_PickerHueWheel;
		float outColor[4] = { color.x, color.y, color.z, color.w };

		if(ImGui::ColorEdit4(va("%s##picker_%s", cvar->gui.title, id), outColor, flags))
		{
			const unsigned int r = outColor[0] * 255.0f;
			const unsigned int g = outColor[1] * 255.0f;
			const unsigned int b = outColor[2] * 255.0f;
			const unsigned int a = outColor[3] * 255.0f;
			Cvar_Set2(cvar->name, va("%02X%02X%02X%02X", r, g, b, a), 0);
		}
	}
	else if(cvar->type == CVART_COLOR_CPMA || !Q_stricmp(cvar->name, "ch_eventOwnColor") || !Q_stricmp(cvar->name, "ch_eventEnemyColor"))
	{
		const char* const oldValue = cvar->latchedString != NULL ? cvar->latchedString : cvar->string;
		char colorCode = oldValue[0];
		if(CPMAColorCodeButton(NULL, cvar->name, colorCode))
		{
			Cvar_Set2(cvar->name, va("%c", colorCode), 0);
		}
	}
	else if(cvar->type == CVART_COLOR_CHBLS || !Q_stricmp(cvar->name, "color"))
	{
		const char* titles[5] =
		{
			"Rail core",
			"Head / helmet / visor",
			"Body / shirt",
			"Legs",
			"Rail spiral"
		};

		const char* const oldValue = cvar->latchedString != NULL ? cvar->latchedString : cvar->string;
		char colorCodes[6] = { '7', '7', '7', '7', '7', '\0' };
		for(int i = 0; i < 5; ++i)
		{
			if(oldValue[i] == '\0')
			{
				break;
			}
			colorCodes[i] = (char)toupper(oldValue[i]);
		}

		bool clicked = false;
		for(int i = 0; i < 5; ++i)
		{
			if(CPMAColorCodeButton(titles[i], va("%s_%d", cvar->name, i), colorCodes[i]))
			{
				clicked = true;
			}
			ImGui::SameLine();
			ImGui::Text(titles[i]);
		}
		Cvar_Set2(cvar->name, colorCodes, 0);
	}
	else if(cvar->gui.numValues > 0 && cvar->type == CVART_BITMASK)
	{
		const int oldValue = cvar->latchedString != NULL ? atoi(cvar->latchedString) : cvar->integer;
		for(int i = 0; i < cvar->gui.numValues; ++i)
		{
			cvarGuiValue_t* const value = &cvar->gui.values[i];
			const int bitIndex = atoi(value->value);
			const int bitMask = 1 << bitIndex;
			bool bitValue = (oldValue & bitMask) != 0;
			if(ImGui::Checkbox(value->title, &bitValue))
			{
				int newValue = oldValue;
				if(bitValue)
				{
					newValue |= bitMask;
				}
				else
				{
					newValue &= ~bitMask;
				}
				Cvar_Set2(cvar->name, va("%d", newValue), 0);
			}
			SetTooltipIfValid(value->desc);
		}
	}
	else if(cvar->gui.numValues == 2 && cvar->type == CVART_BOOL)
	{
		// keep the value order: the value for 1 can come before the value for 0
		// this is on purpose because there are CVars phrased as a negative
		const bool oldValue = cvar->latchedString ? (atoi(cvar->latchedString) != 0) : (cvar->integer != 0);
		const cvarGuiValue_t* const values = cvar->gui.values;
		const int i0 = atoi(values[0].value);
		const int i1 = atoi(values[1].value);
		const cvarGuiValue_t* const value0 = &values[0];
		const cvarGuiValue_t* const value1 = &values[1];
		int currValue = oldValue ? 1 : 0;

		if(ImGui::RadioButton(value0->title, &currValue, i0))
		{
			Cvar_Set2(cvar->name, values[0].value, 0);
		}
		SetTooltipIfValid(value0->desc);

		ImGui::SameLine();

		if(ImGui::RadioButton(value1->title, &currValue, i1))
		{
			Cvar_Set2(cvar->name, values[1].value, 0);
		}
		SetTooltipIfValid(value1->desc);
	}
	else if(cvar->gui.numValues == 2 && cvar->type == CVART_INTEGER &&
		cvar->validator.i.max == cvar->validator.i.min + 1)
	{
		const int oldValue = cvar->latchedString ? atoi(cvar->latchedString) : cvar->integer;
		const cvarGuiValue_t* const values = cvar->gui.values;
		const cvarGuiValue_t* const value0 = &values[0];
		const cvarGuiValue_t* const value1 = &values[1];
		const int i0 = atoi(value0->value);
		const int i1 = atoi(value1->value);
		int currValue = oldValue;

		if(ImGui::RadioButton(value0->title, &currValue, i0))
		{
			Cvar_Set2(cvar->name, value0->value, 0);
		}
		SetTooltipIfValid(value0->desc);

		ImGui::SameLine();

		if(ImGui::RadioButton(value1->title, &currValue, i1))
		{
			Cvar_Set2(cvar->name, value1->value, 0);
		}
		SetTooltipIfValid(value1->desc);
	}
	else if(cvar->gui.numValues > 0)
	{
		const char* const oldValue = cvar->latchedString != NULL ? cvar->latchedString : cvar->string;
		const char* previewString = NULL;
		for(int i = 0; i < cvar->gui.numValues; ++i)
		{
			cvarGuiValue_t* const value = &cvar->gui.values[i];
			if(strcmp(oldValue, value->value) == 0)
			{
				previewString = value->title;
				break;
			}
		}
		if(previewString == NULL)
		{
			previewString = oldValue;
		}

		ImGui::SetNextItemWidth(ImGui::GetIO().FontDefault->FontSize * 15.0f);
		if(ImGui::BeginCombo(va("##%s", cvar->name), previewString, ImGuiComboFlags_None))
		{
			for(int i = 0; i < cvar->gui.numValues; ++i)
			{
				cvarGuiValue_t* const value = &cvar->gui.values[i];
				bool selected = false;
				if(ImGui::Selectable(value->title, &selected, ImGuiSelectableFlags_None))
				{
					Cvar_Set2(cvar->name, value->value, 0);
				}
				SetTooltipIfValid(value->desc);
			}

			ImGui::EndCombo();
		}
	}
	else if(cvar->type == CVART_STRING)
	{
		if(!Q_stricmp(cvar->name, "mvw_DM") ||
			!Q_stricmp(cvar->name, "mvw_TDM1") ||
			!Q_stricmp(cvar->name, "mvw_TDM2") ||
			!Q_stricmp(cvar->name, "mvw_TDM3") ||
			!Q_stricmp(cvar->name, "mvw_TDM4"))
		{
			int v[4] = {};
			const char* const oldValue = cvar->latchedString != NULL ? cvar->latchedString : cvar->string;
			sscanf(oldValue, "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]);
			if(ImGui::SliderInt4(va("##%s", cvar->name), v, 0, 640, "%d", ImGuiSliderFlags_AlwaysClamp))
			{
				Cvar_Set2(cvar->name, va("%d %d %d %d", v[0], v[1], v[2], v[3]), 0);
			}
		}
		else
		{
			static char text[256];
			Q_strncpyz(text, cvar->string, sizeof(text));
			if(ImGui::InputText(va("##%s", cvar->name), text, sizeof(text)))
			{
				Cvar_Set2(cvar->name, text, 0);
			}
		}
	}
	else if(cvar->type == CVART_BOOL)
	{
		const bool oldValue = cvar->latchedString ? (atoi(cvar->latchedString) != 0) : (cvar->integer != 0);
		bool curValue = oldValue;
		ImGui::Checkbox(va("##%s", cvar->name), &curValue);
		if(curValue != oldValue)
		{
			Cvar_Set2(cvar->name, va("%d", (int)curValue), 0);
		}
	}
	else if(cvar->type == CVART_INTEGER || cvar->type == CVART_BITMASK)
	{
		const int oldValue = cvar->latchedString ? atoi(cvar->latchedString) : cvar->integer;
		int curValue = oldValue;
		int min = cvar->validator.i.min;
		int max = cvar->validator.i.max;
		min = (min == INT32_MIN) ? -(1 << 20) : min;
		max = (max == INT32_MAX) ?  (1 << 20) : max;
		ImGui::SetNextItemWidth(ImGui::GetIO().FontDefault->FontSize * 15.0f);
		ImGui::SliderInt(va("##%s", cvar->name), &curValue, min, max,
			"%d", ImGuiSliderFlags_AlwaysClamp);
		if(curValue != oldValue)
		{
			Cvar_Set2(cvar->name, va("%d", curValue), 0);
		}
	}
	else if(cvar->type == CVART_FLOAT && (!Q_stricmp(cvar->name, "cg_fov") || !Q_stricmp(cvar->name, "cg_zoomFov")))
	{
		const float oldValue = cvar->latchedString ? atof(cvar->latchedString) : cvar->value;
		const float hMin = cvar->validator.f.min;
		const float hMax = cvar->validator.f.max;
		const float vMin = GetVerticalFov(hMin);
		const float vMax = GetVerticalFov(hMax);
		float curValue = oldValue;
		ImGui::SetNextItemWidth(ImGui::GetIO().FontDefault->FontSize * 10.0f);
		const bool hChange = ImGui::SliderFloat(va("Horizontal##%s", cvar->name), &curValue, hMin, hMax,
			"%g", ImGuiSliderFlags_AlwaysClamp);
		curValue = GetVerticalFov(curValue);
		ImGui::SetNextItemWidth(ImGui::GetIO().FontDefault->FontSize * 10.0f);
		const bool vChange = ImGui::SliderFloat(va("Vertical##%s", cvar->name), &curValue, vMin, vMax,
			"%g", ImGuiSliderFlags_AlwaysClamp);
		curValue = GetHorizontalFov(curValue);
		if(hChange || vChange)
		{
			Cvar_Set2(cvar->name, va("%g", curValue), 0);
		}
	}
	else if(cvar->type == CVART_FLOAT)
	{
		const float oldValue = cvar->latchedString ? atof(cvar->latchedString) : cvar->value;
		float curValue = oldValue;
		float min = cvar->validator.f.min;
		float max = cvar->validator.f.max;
		min = (min == -FLT_MAX) ? -1048576.0f : min;
		max = (max ==  FLT_MAX) ?  1048576.0f : max;
		ImGui::SetNextItemWidth(ImGui::GetIO().FontDefault->FontSize * 15.0f);
		ImGui::SliderFloat(va("##%s", cvar->name), &curValue, min, max,
			"%g", ImGuiSliderFlags_AlwaysClamp);
		if(curValue != oldValue)
		{
			Cvar_Set2(cvar->name, va("%g", curValue), 0);
		}
	}
}

static void DrawCVarToolTip(cvar_t* cvar)
{
	if(!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
	{
		return;
	}

	const char* const tooltipTextRaw = IsNonEmpty(cvar->gui.help) ? cvar->gui.help : "";
	const char* const tooltipTextClean = RemoveColorCodes(tooltipTextRaw);
	char help[1024];
	if(IsNullOrEmpty(tooltipTextClean))
	{
		Com_sprintf(help, sizeof(help), "CVar: %s", cvar->name);
	}
	else
	{
		Com_sprintf(help, sizeof(help), "CVar: %s\n\n%s", cvar->name, tooltipTextClean);
	}

	ImGui::SetTooltip(help);
}

static void DrawCVarNoValue(cvar_t* cvar)
{
	Q_assert(cvar != NULL);
	Q_assert(IsNonEmpty(cvar->gui.title));

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text(cvar->gui.title);
	DrawCVarToolTip(cvar);
	if((cvar->flags & (CVAR_ROM | CVAR_INIT)) == 0)
	{
		ImGui::TableSetColumnIndex(2);
		if(ImGui::Button(va("R##%s", cvar->name)))
		{
			Cvar_Set2(cvar->name, cvar->resetString, 0);
		}
	}
	ImGui::TableSetColumnIndex(3);
	ImGui::Text(cvar->latchedString != NULL ? cvar->latchedString : "");
	ImGui::TableSetColumnIndex(4);
	const char* const desc = IsNonEmpty(cvar->gui.desc) ? cvar->gui.desc : "";
	ImGui::TextWrapped(RemoveColorCodes(desc));
	DrawCVarToolTip(cvar);
}

static void DrawCVar(const char* cvarName)
{
	cvar_t* cvar = Cvar_FindVar(cvarName);
	if(cvar == NULL)
	{
		return;
	}

	DrawCVarNoValue(cvar);
	ImGui::TableSetColumnIndex(1);
	DrawCVarValue(cvar);
}

static void DrawCVarValueCombo(const char* cvarName, int count, ...)
{
	cvar_t* cvar = Cvar_FindVar(cvarName);
	if(cvar == NULL)
	{
		return;
	}

#if defined(_DEBUG)
	if(Q_stricmp(cvarName, "r_guiFont"))
	{
		Q_assert(count == cvar->validator.i.max - cvar->validator.i.min + 1);
	}
#endif

	const int oldValue = cvar->latchedString != NULL ? atoi(cvar->latchedString) : cvar->integer;

	const char* previewString = NULL;
	va_list args;
	va_start(args, count);
	for(int i = 0; i < count; ++i)
	{
		const char* value = va_arg(args, const char*);
		if(oldValue == cvar->validator.i.min + i)
		{
			previewString = value;
			break;
		}
	}
	va_end(args);
	Q_assert(previewString != NULL);

	ImGui::SetNextItemWidth(ImGui::GetIO().FontDefault->FontSize * 15.0f);
	if(ImGui::BeginCombo(va("##%s", cvar->name), previewString, ImGuiComboFlags_None))
	{
		va_start(args, count);
		for(int i = 0; i < count; ++i)
		{
			const char* value = va_arg(args, const char*);
			bool selected;
			if(ImGui::Selectable(value, &selected, ImGuiSelectableFlags_None))
			{
				Cvar_Set2(cvar->name, va("%d", cvar->validator.i.min + i), 0);
			}
		}
		va_end(args);

		ImGui::EndCombo();
	}
}

#if 0
static void DrawCVarValueStringCombo(const char* cvarName, int count, ...)
{
	cvar_t* cvar = Cvar_FindVar(cvarName);
	if(cvar == NULL)
	{
		return;
	}

	const char* const currValue = cvar->latchedString != NULL ? cvar->latchedString : cvar->string;

	const char* previewString = NULL;
	va_list args;
	va_start(args, count);
	for(int i = 0; i < count; ++i)
	{
		const char* value = va_arg(args, const char*);
		const char* title = va_arg(args, const char*);
		if(strcmp(currValue, value) == 0)
		{
			previewString = title;
			break;
		}
	}
	va_end(args);

	if(previewString == NULL)
	{
		previewString = "???";
	}

	ImGui::SetNextItemWidth(ImGui::GetIO().FontDefault->FontSize * 15.0f);
	if(ImGui::BeginCombo(va("##%s", cvar->name), previewString, ImGuiComboFlags_None))
	{
		va_start(args, count);
		for(int i = 0; i < count; ++i)
		{
			const char* value = va_arg(args, const char*);
			const char* title = va_arg(args, const char*);
			bool selected;
			if(ImGui::Selectable(title, &selected, ImGuiSelectableFlags_None))
			{
				Cvar_Set2(cvar->name, value, 0);
			}
		}
		va_end(args);

		ImGui::EndCombo();
	}
}

static void DrawCVarValueBitmask(const char* cvarName, int count, ...)
{
	cvar_t* cvar = Cvar_FindVar(cvarName);
	if(cvar == NULL)
	{
		return;
	}

	Q_assert(cvar->validator.i.min == 0);
	Q_assert(IsPowerOfTwo(cvar->validator.i.max + 1));
	Q_assert(count == log2(cvar->validator.i.max + 1));

	ImGui::TableSetColumnIndex(1);

	const int oldValue = cvar->latchedString != NULL ? atoi(cvar->latchedString) : cvar->integer;

	va_list args;
	va_start(args, count);
	for(int i = 0; i < count; ++i)
	{
		const char* title = va_arg(args, const char*);
		bool bitValue = (oldValue & (1 << i)) != 0;
		if(ImGui::Checkbox(title, &bitValue))
		{
			int newValue = oldValue;
			if(bitValue)
			{
				newValue |= (1 << i);
			}
			else
			{
				newValue &= ~(1 << i);
			}
			Cvar_Set2(cvar->name, va("%d", newValue), 0);
		}
	}
	va_end(args);
}
#endif

static void DrawFontSelection()
{
#if 0
	ImGuiIO& io = ImGui::GetIO();
	ImFont* currentFont = ImGui::GetFont();
	if(ImGui::BeginCombo("Font Selection", currentFont->GetDebugName()))
	{
		for(int i = 0; i < io.Fonts->Fonts.Size; ++i)
		{
			ImFont* font = io.Fonts->Fonts[i];
			ImGui::PushID((void*)font);
			if(ImGui::Selectable(font->GetDebugName(), font == currentFont))
			{
				io.FontDefault = font;
			}
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}
#endif

	ImGui::AlignTextToFramePadding();
	ImGui::Text("Font");
	ImGui::SameLine();
	const char* customFontName = NULL;
	if(CL_IMGUI_IsCustomFontLoaded(&customFontName))
	{
		DrawCVarValueCombo("r_guiFont", 3,
			"Proggy Clean (13px)",
			"Sweet16 Mono (16px)",
			customFontName);
	}
	else
	{
		DrawCVarValueCombo("r_guiFont", 2,
			"Proggy Clean (13px)",
			"Sweet16 Mono (16px)");
	}
	const int fontIndex = Cvar_VariableIntegerValue("r_guiFont");
	ImGuiIO& io = ImGui::GetIO();
	if(fontIndex >= 0 && fontIndex < io.Fonts->Fonts.Size)
	{
		io.FontDefault = io.Fonts->Fonts[fontIndex];
	}
}

static void SetTableColumns()
{
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFont()->FontSize * 16.0f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFont()->FontSize * 20.0f);
	ImGui::TableSetupColumn("Reset");
	ImGui::TableSetupColumn("Latched");
	ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFont()->FontSize * 32.0f);
	ImGui::TableHeadersRow();
}

static void DrawVideoRestart(bool restartNeeded)
{
	if(ImGui::Button("Video restart"))
	{
		Cbuf_AddText("vid_restart\n");
	}
	if(restartNeeded)
	{
		ImGui::SameLine();
		ImGui::Text("Some pending changes need a restart to take effect.");
	}
}

static bool DrawCVarTable(bool* restartNeeded, const char* title, int categoryMask)
{
	if(BeginTable(title, 5))
	{
		SetTableColumns();

		for(cvar_t* var = Cvar_GetFirst(); var != NULL; var = var->next)
		{
			if(var->gui.categories & CVARCAT_DEBUGGING)
			{
				continue;
			}

			if(var->gui.categories & categoryMask)
			{
				if(cvarFilter[0] != '\0')
				{
					char filter[256];
					Com_sprintf(filter, sizeof(filter), cvarFilter[0] == '*' ? "%s" : "*%s", cvarFilter);
					const cvarGui_t* const gui = &var->gui;
					const bool matchName = !!Com_Filter(filter, var->name);
					const bool matchTitle = gui->title != NULL && !!Com_Filter(filter, gui->title);
					const bool matchDesc = gui->desc != NULL && !!Com_Filter(filter, gui->desc);
					const bool matchHelp = gui->help != NULL && !!Com_Filter(filter, gui->help);
					bool matchValues = false;
					for(int v = 0; v < gui->numValues; ++v)
					{
						const cvarGuiValue_t* const value = &gui->values[v];
						if(!!Com_Filter(filter, value->title) ||
							!!Com_Filter(filter, value->desc))
						{
							matchValues = true;
							break;
						}
					}
					if(!matchName && !matchTitle && !matchDesc && !matchHelp && !matchValues)
					{
						continue;
					}
				}

				DrawCVar(var->name);
				if(var->latchedString != NULL)
				{
					*restartNeeded = true;
				}
			}
		}

		ImGui::EndTable();
	}

	return restartNeeded;
}

static void DrawSettings()
{
	static float opacity = 1.0f;
	ImVec4 bgColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
	bgColor.w *= opacity;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);

	static bool active = false;
	static bool restartNeeded = false;
	ToggleBooleanWithShortcut(active, ImGuiKey_O);
	GUI_AddMainMenuItem(GUI_MainMenu::Tools, "Client Settings", "Ctrl+O", &active);
	if(active)
	{
		const ImGuiWindowFlags flags =
			ImGuiWindowFlags_AlwaysAutoResize |
			(restartNeeded ? ImGuiWindowFlags_UnsavedDocument : 0);
		restartNeeded = false;
		if(ImGui::Begin("Client Settings", &active, flags))
		{
			DrawFontSelection();
			ImGui::SliderFloat("Dialog opacity", &opacity, 0.5f, 1.0f, "%g", ImGuiSliderFlags_AlwaysClamp);

			ImGui::NewLine();
			DrawFilter(cvarFilter, sizeof(cvarFilter));
			ImGui::NewLine();

			ImGui::BeginTabBar("Tabs#ClientSettings");
			if(ImGui::BeginTabItem("All"))
			{
				DrawCVarTable(&restartNeeded, "All settings", -1 & (~CVARCAT_DEBUGGING));
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("General"))
			{
				DrawCVarTable(&restartNeeded, "General settings", CVARCAT_GENERAL);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Gameplay"))
			{
				DrawCVarTable(&restartNeeded, "Gameplay settings", CVARCAT_GAMEPLAY);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("HUD"))
			{
				DrawCVarTable(&restartNeeded, "HUD settings", CVARCAT_HUD);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Video"))
			{
				DrawCVarTable(&restartNeeded, "Display settings", CVARCAT_DISPLAY);
				DrawCVarTable(&restartNeeded, "Graphics settings", CVARCAT_GRAPHICS);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Audio"))
			{
				DrawCVarTable(&restartNeeded, "Sound settings", CVARCAT_SOUND);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Input"))
			{
				DrawCVarTable(&restartNeeded, "Input settings", CVARCAT_INPUT);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Network"))
			{
				DrawCVarTable(&restartNeeded, "Network and client-side prediction settings", CVARCAT_NETWORK);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Console"))
			{
				DrawCVarTable(&restartNeeded, "Console settings", CVARCAT_CONSOLE);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Demo"))
			{
				DrawCVarTable(&restartNeeded, "Demo playback settings", CVARCAT_DEMO);
				ImGui::EndTabItem();
			}
			if(ImGui::BeginTabItem("Performance"))
			{
				DrawCVarTable(&restartNeeded, "Performance settings", CVARCAT_PERFORMANCE);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();

			DrawVideoRestart(restartNeeded);
		}

		ImGui::End();
	}

	ImGui::PopStyleColor(1);
}

void R_DrawGUI()
{
	DrawImageList();
	DrawImageWindow();
	DrawShaderList();
	DrawShaderWindow();
	DrawShaderEdit();
	DrawFrontEndStats();
	DrawSettings();
}

void R_ShutDownGUI()
{
	// this is necessary to avoid crashes in the detail windows
	// following a map change or video restart:
	// the renderer is shut down and the pointers become stale
	if(shaderEditWindow.active)
	{
		R_SetShaderData(shaderEditWindow.shader, &shaderEditWindow.originalShader);
	}
	memset(&imageWindow, 0, sizeof(imageWindow));
	memset(&shaderWindow, 0, sizeof(shaderWindow));
	memset(&shaderEditWindow, 0, sizeof(shaderEditWindow));
	ClearShaderReplacements();
	ClearImageReplacements();
}
