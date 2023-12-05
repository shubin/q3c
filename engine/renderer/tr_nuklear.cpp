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
// Nuklear integration


#include "tr_local.h"
#define NK_IMPLEMENTATION
#define NK_PRIVATE
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#include "../nuklear/nuklear.h"


#if defined(offsetof)
#undef offsetof
#endif
#define offsetof(s, m)      ((intptr_t)&(((s*)0)->m))
#define membersize(s, m)    (sizeof(((s*)0)->m))


struct structMember_t
{
	int offset;
	int numBytes;
};

#define X(Member) { (int)offsetof(nk_font, Member), (int)membersize(nk_font, Member) }
static const structMember_t s_fontMembers[] =
{
	X(fallback_codepoint),
	X(handle.height),
	X(handle.texture.id),
	X(info.ascent),
	X(info.descent),
	X(info.glyph_count),
	X(info.glyph_offset),
	X(info.height),
	X(scale),
	X(texture.id)
};
#undef X

#define X(Member) { (int)offsetof(nk_font_glyph, Member), (int)membersize(nk_font_glyph, Member) }
static const structMember_t s_fontGlyphMembers[] =
{
	X(codepoint),
	X(h),
	X(u0),
	X(u1),
	X(v0),
	X(v1),
	X(w),
	X(x0),
	X(x1),
	X(xadvance),
	X(y0),
	X(y1)
};
#undef X

#define X(Member) { (int)offsetof(struct nk_font_config, Member), (int)membersize(struct nk_font_config, Member) }
static const structMember_t s_fontConfigMembers[] =
{
	X(merge_mode),
	X(pixel_snap),
	X(oversample_h),
	X(oversample_v),
	X(size),
	X(coord_type),
	X(fallback_glyph),
	X(spacing.x),
	X(spacing.y)
};
#undef X


#if defined(_DEBUG)
static void ValidateMembers()
{
	for(int i = 0; i < ARRAY_LEN(s_fontMembers); i++)
	{
		Q_assert(s_fontMembers[i].numBytes == 4);
	}

	for(int i = 0; i < ARRAY_LEN(s_fontGlyphMembers); i++)
	{
		Q_assert(s_fontGlyphMembers[i].numBytes == 4);
	}

	for(int i = 0; i < ARRAY_LEN(s_fontConfigMembers); i++)
	{
		Q_assert(
			s_fontConfigMembers[i].numBytes == 4 ||
			s_fontConfigMembers[i].numBytes == 1);
	}
}
#endif


qhandle_t RE_NK_CreateFontAtlas(vmIndex_t vmIndex, byte* outBuf, int outSize, const byte* inBuf, int inSize)
{
#if defined(_DEBUG)
	ValidateMembers();
#endif

	qhandle_t shaderHandle = 0;
	void* fontData = NULL;

	struct nk_font_atlas atlas;
	nk_font_atlas_init_default(&atlas);
	nk_font_atlas_begin(&atlas);

	struct nk_font_config fontConfigs[16];
	struct nk_font* fonts[16];
	int numRangesPerFont[16];

	nk_rune ranges[(16 + 1) * 2 * ARRAY_LEN(fonts)];
	int32_t* in = (int32_t*)inBuf;
	const int numFonts = *in++;
	if(numFonts <= 0 || numFonts > ARRAY_LEN(fonts))
	{
		goto clean_up;
	}

	nk_rune* outRange = ranges;
	for(int i = 0; i < numFonts; i++)
	{
		const int skip = *in++;
		const char* const filePath = (const char*)in;
		in += skip;
		const float height = *(float*)in++;
		const int pixelSnap = *in++;
		const int overSampleH = *in++;
		const int overSampleV = *in++;
		const int numRangePairs = *in++;
		if(height < 1.0f ||
			(pixelSnap != 0 && pixelSnap != 1) ||
			overSampleH <= 0 ||
			overSampleV <= 0 ||
			numRangePairs <= 0)
		{
			goto clean_up;
		}
		numRangesPerFont[i] = (numRangePairs + 1) * 2;

		const int fontSize = FS_ReadFile(filePath, &fontData);
		if(fontSize <= 0 || fontData == NULL)
		{
			goto clean_up;
		}

		const nk_rune* const rangeStart = outRange;
		for(int j = 0; j < numRangePairs; j++)
		{
			*outRange++ = *in++;
			*outRange++ = *in++;
		}
		*outRange++ = 0;
		*outRange++ = 0;

		fontConfigs[i] = nk_font_config(height);
		fontConfigs[i].pixel_snap = pixelSnap;
		fontConfigs[i].oversample_h = overSampleH;
		fontConfigs[i].oversample_v = overSampleV;
		fontConfigs[i].range = rangeStart;
		fonts[i] = nk_font_atlas_add_from_memory(&atlas, fontData, fontSize, height, &fontConfigs[i]);

		FS_FreeFile(fontData);
		fontData = NULL;
	}

	int imgWidth, imgHeight;
	byte* const imgData = (byte*)nk_font_atlas_bake(&atlas, &imgWidth, &imgHeight, NK_FONT_ATLAS_RGBA32);
	if(imgData == NULL)
	{
		goto clean_up;
	}

	const char* const name =
		vmIndex == VM_UI ?
		"$cpma/ui/nuklear_font_atlas" :
		"$cpma/cgame/nuklear_font_atlas";
	image_t* const image = R_CreateImage(name, imgData, imgWidth, imgHeight, TF_RGBA8, IMG_NOMIPMAP, TW_CLAMP_TO_EDGE);
	if(image == NULL)
	{
		goto clean_up;
	}

	shaderHandle = RE_RegisterShaderFromImage(name, image);
	nk_font_atlas_end(&atlas, nk_handle_id(shaderHandle), 0);

	for(int i = 0; i < numFonts; i++)
	{
		if(fonts[i]->fallback < atlas.glyphs ||
			fonts[i]->fallback >= atlas.glyphs + atlas.glyph_count)
		{
			goto clean_up;
		}
	}

	int numRanges = 0;
	for(int i = 0; i < numFonts; i++)
	{
		int r = 0;
		while(fonts[i]->info.ranges[r++] > 0)
		{
			numRanges++;
		}
	}
	if(numRanges <= 0 || numRanges % 2 != 0)
	{
		shaderHandle = 0;
		goto clean_up;
	}
	numRanges += 2;
	
	const int numGlyphs = atlas.glyph_count;
	const int numEntries =
		3 +
		numFonts * (ARRAY_LEN(s_fontMembers) + ARRAY_LEN(s_fontConfigMembers) + 2) +
		numGlyphs * ARRAY_LEN(s_fontGlyphMembers) +
		numRanges;
	if(numEntries * 4 > outSize)
	{
		shaderHandle = 0;
		goto clean_up;
	}

	int firstRange = 0;

	int32_t* out = (int32_t*)outBuf;
	*out++ = numFonts;
	*out++ = numGlyphs;
	*out++ = numRanges;

	for(int f = 0; f < numFonts; f++)
	{
		const nk_font* const font = fonts[f];
		const int fallbackGlyph = font->fallback - atlas.glyphs;
		*out++ = fallbackGlyph;
		*out++ = firstRange;
		for(int i = 0; i < ARRAY_LEN(s_fontMembers); i++)
		{
			const byte* const inField = (const byte*)font + s_fontMembers[i].offset;
			*out++ = *(int*)inField;
		}
		for(int i = 0; i < ARRAY_LEN(s_fontConfigMembers); i++)
		{
			const byte* const inField = (const byte*)font->config + s_fontConfigMembers[i].offset;
			if(s_fontConfigMembers[i].numBytes == 1)
				*out++ = (int)(*(char*)inField);
			else
				*out++ = *(int*)inField;
		}
		firstRange += numRangesPerFont[f];
	}

	for(int g = 0; g < numGlyphs; g++)
	{
		const nk_font_glyph* const glyph = atlas.glyphs + g;
		for(int i = 0; i < ARRAY_LEN(s_fontGlyphMembers); i++)
		{
			const byte* const inField = (const byte*)glyph + s_fontGlyphMembers[i].offset;
			*out++ = *(int*)inField;
		}
	}

	for(int i = 0; i < numRanges; i++)
	{
		*out++ = (int)ranges[i];
	}

	Q_assert(out == (int*)outBuf + numEntries);

clean_up:
	nk_font_atlas_clear(&atlas);
	if(fontData != NULL)
	{
		FS_FreeFile(fontData);
	}

	return shaderHandle;
}
