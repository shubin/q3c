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
// UI rendering


struct VOut
{
	float4 position : SV_Position;
	float2 texCoords : TEXCOORD0;
	float4 color : COLOR0;
};


#if VERTEX_SHADER

cbuffer RootConstants
{
	float2 scale;
};

struct VIn
{
	float2 position : POSITION;
	float2 texCoords : TEXCOORD0;
	float4 color : COLOR0;
};

VOut vs(VIn input)
{
	const float2 position = input.position * scale;
	VOut output;
	output.position = float4(position.x - 1.0, 1.0 - position.y, 0.0, 1.0);
	output.texCoords = input.texCoords;
	output.color = input.color;

	return output;
}

#endif


#if PIXEL_SHADER

cbuffer RootConstants
{
	uint textureIndex;
	uint samplerIndex;
};

Texture2D textures2D[4096] : register(t0);
SamplerState samplers[12] : register(s0);

float4 ps(VOut input) : SV_Target
{
	return textures2D[textureIndex].Sample(samplers[samplerIndex], input.texCoords) * input.color;
}

#endif
