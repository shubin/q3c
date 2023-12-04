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
// SMAA pass #3: neighborhood blending


cbuffer RootConstants
{
	float4 rtMetrics;
};

#include "smaa.hlsli"

struct VOut
{
	float4 position : SV_Position;
	float2 texCoords : TEXCOORD0;
	float4 offsets : TEXCOORD1;
};


#if SMAA_INCLUDE_VS

VOut vs(uint id : SV_VertexID)
{
	VOut output;
	output.position.x = (float)(id / 2) * 4.0 - 1.0;
	output.position.y = (float)(id % 2) * 4.0 - 1.0;
	output.position.z = 0.0;
	output.position.w = 1.0;
	output.texCoords.x = (float)(id / 2) * 2.0;
	output.texCoords.y = 1.0 - (float)(id % 2) * 2.0;
	SMAANeighborhoodBlendingVS(output.texCoords, output.offsets);

	return output;
}

#endif


#if SMAA_INCLUDE_PS

Texture2D inputTex : register(t0);
Texture2D blendTex : register(t4);

float4 ps(VOut input) : SV_Target
{
	return SMAANeighborhoodBlendingPS(input.texCoords, input.offsets, inputTex, blendTex);
}

#endif
