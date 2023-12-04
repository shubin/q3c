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
// post-processing: moves from gamma to linear space


struct VOut
{
	float4 position : SV_Position;
	float2 texCoords : TEXCOORD0;
};


#if VERTEX_SHADER

VOut vs(uint id : SV_VertexID)
{
	VOut output;
	output.position.x = (float)(id / 2) * 4.0 - 1.0;
	output.position.y = (float)(id % 2) * 4.0 - 1.0;
	output.position.z = 0.0;
	output.position.w = 1.0;
	output.texCoords.x = (float)(id / 2) * 2.0;
	output.texCoords.y = 1.0 - (float)(id % 2) * 2.0;

	return output;
}

#endif


#if PIXEL_SHADER

// X3571: pow(f, e) won't work if f is negative
#pragma warning(disable : 3571)

cbuffer RootConstants
{
	float gamma;
	float invBrightness;
};

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

float4 ps(VOut input) : SV_Target
{
	float3 base = texture0.Sample(sampler0, input.texCoords).rgb;
	float3 linearSpace = pow(base * invBrightness, gamma);

	return float4(linearSpace, 1.0f);
}

#endif
