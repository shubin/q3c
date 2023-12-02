/*
===========================================================================
Copyright (C) 2019-2020 Gian 'myT' Schellenbaum

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
// depth sprite vertex and pixel shaders

#include "shared.hlsli"

cbuffer VertexShaderBuffer
{
    matrix modelViewMatrix;
    matrix projectionMatrix;
	float4 clipPlane;
};

struct VIn
{
	float4 position : POSITION;
	float4 color : COLOR0;
	float2 texCoords : TEXCOORD0;
};

struct VOut
{
	float4 position : SV_Position;
	float4 color : COLOR0;
	float2 texCoords : TEXCOORD0;
	float2 proj2232 : TEXCOORD1;
	float depthVS : DEPTHVS;
	float clipDist : SV_ClipDistance0;
};

VOut vs_main(VIn input)
{
	float4 positionVS = mul(modelViewMatrix, float4(input.position.xyz, 1));

	VOut output;
	output.position = mul(projectionMatrix, positionVS);
	output.color = input.color;
	output.texCoords = input.texCoords;
	output.proj2232 = float2(-projectionMatrix[2][2], projectionMatrix[2][3]);
	output.depthVS = -positionVS.z;
	output.clipDist = dot(positionVS, clipPlane);

	return output;
}

cbuffer PixelShaderBuffer
{
	uint alphaTest;
	float distance;
	float offset;
	float greyscale;
	float4 colorScale;
	float4 colorBias;
};

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

Texture2DMS<float2> textureDepth;

/*
f   = far  clip plane distance
n   = near clip plane distance
exp = exponential depth value (as stored in the Z-buffer)

                     2 * f * n             B
linear(exp) = ----------------------- = -------
              (f + n) - exp * (f - n)   exp - A

            f + n               -2 * f * n
with    A = -----    and    B = ----------
            f - n                  f - n
*/
float LinearDepth(float zwDepth, float proj22, float proj32)
{
	return proj32 / (zwDepth - proj22);
}

float Contrast(float d, float power)
{
	bool aboveHalf = d > 0.5;
	float base = saturate(2.0 * (aboveHalf ? (1.0 - d) : d));
	float r = 0.5 * pow(base, power);

	return aboveHalf ? (1.0 - r) : r;
}

float4 ps_main(VOut input, uint sampleIndex : SV_SampleIndex) : SV_TARGET
{
	float4 r = input.color * texture0.Sample(sampler0, input.texCoords);
	if((alphaTest == 1 && r.a == 0.0) ||
	   (alphaTest == 2 && r.a >= 0.5) ||
	   (alphaTest == 3 && r.a <  0.5))
		discard;

	float zwDepth = textureDepth.Load(input.position.xy, sampleIndex).x;
	float depthS = LinearDepth(zwDepth, input.proj2232.x, input.proj2232.y);
	float depthP = input.depthVS - offset;
	float scale = Contrast((depthS - depthP) * distance, 2.0);
	float4 r2 = lerp(r * colorScale + colorBias, r, scale);

	return MakeGreyscale(r2, greyscale);
}
