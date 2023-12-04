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


struct VOut
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};


#if VERTEX_SHADER

cbuffer RootConstants
{
	float4x4 projectionMatrix;
};

struct VIn
{
	float2 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

VOut vs(VIn input)
{
	VOut output;
	output.pos = mul(projectionMatrix, float4(input.pos.xy, 0.0, 1.0));
	output.col = input.col;
	output.uv = input.uv;

	return output;
}

#endif


#if PIXEL_SHADER

cbuffer RootConstants
{
	uint textureIndex;
	uint samplerIndex;
};

SamplerState samplers[96] : register(s0);
Texture2D textures[4096] : register(t0);

float4 ps(VOut input) : SV_Target
{
	return input.col * textures[textureIndex].Sample(samplers[samplerIndex], input.uv);
}

#endif
