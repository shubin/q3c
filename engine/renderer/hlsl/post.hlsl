/*
===========================================================================
Copyright (C) 2019 Gian 'myT' Schellenbaum

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
// post-processing vertex and pixel shaders

#include "shared.hlsli"

// X3571: pow(f, e) won't work if f is negative
#pragma warning(disable : 3571)

cbuffer VertexShaderBuffer
{
	float scaleX;
	float scaleY;
	float dummyvs0;
	float dummyvs1;
};

struct VOut
{
	float4 position : SV_Position;
	float2 texCoords : TEXCOORD0;
};

VOut vs_main(uint id : SV_VertexID)
{
	VOut output;
	output.position.x = scaleX * ((float)(id / 2) * 4.0 - 1.0);
	output.position.y = scaleY * ((float)(id % 2) * 4.0 - 1.0);
	output.position.z = 0.0;
	output.position.w = 1.0;
	output.texCoords.x = (float)(id / 2) * 2.0;
	output.texCoords.y = 1.0 - (float)(id % 2) * 2.0;

	return output;
}

cbuffer PixelShaderBuffer
{
	float gamma;
	float brightness;
	float greyscale;
	float dummyps0;
};

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

float4 ps_main(VOut input) : SV_TARGET
{
	float3 base = texture0.Sample(sampler0, input.texCoords).rgb;
	float3 gc = pow(base, gamma) * brightness;

	return MakeGreyscale(float4(gc.rgb, 1.0), greyscale);
}
