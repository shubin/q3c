/*
===========================================================================
Copyright (C) 2019-2023 Gian 'myT' Schellenbaum

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
// dynamic light shader


#if VERTEX_SHADER

struct VIn
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoords : TEXCOORD0;
	float4 color : COLOR0;
};

#endif

struct VOut
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texCoords : TEXCOORD0;
	float4 color : COLOR0;
	float3 osLightVec : TEXCOORD1;
	float3 osEyeVec : TEXCOORD2;
};


#if VERTEX_SHADER

cbuffer RootConstants
{
	matrix modelViewMatrix;
	matrix projectionMatrix;
	float4 osLightPos;
	float4 osEyePos;
};

VOut vs(VIn input)
{
	float4 positionVS = mul(modelViewMatrix, float4(input.position.xyz, 1));

	VOut output;
	output.position = mul(projectionMatrix, positionVS);
	output.normal = input.normal;
	output.texCoords = input.texCoords;
	output.color = input.color;
	output.osLightVec = osLightPos.xyz - input.position.xyz;
	output.osEyeVec = osEyePos.xyz - input.position.xyz;

	return output;
}

#endif


#if PIXEL_SHADER

#include "shared.hlsli"

cbuffer RootConstants
{
	uint4 stageIndices; // low 16 = texture, high 16 = sampler
	float3 color;
	float recSqrRadius; // 1 / (r * r)
	float greyscale;
	float intensity;
	float opaque;
};

Texture2D textures2D[4096] : register(t0);
SamplerState samplers[96] : register(s0);

float BezierEase(float t)
{
	return t * t * (3.0 - 2.0 * t);
}

[earlydepthstencil]
float4 ps(VOut input) : SV_Target
{
	// fetch data
	uint textureIndex = stageIndices.x & 0xFFFF;
	uint samplerIndex = stageIndices.x >> 16;
	float lightRecSqrRadius = recSqrRadius;
	float3 lightColor = color;
	float4 baseColored = input.color * textures2D[textureIndex].Sample(samplers[samplerIndex], input.texCoords);
	float4 base = MakeGreyscale(baseColored, greyscale);
	float3 nL = normalize(input.osLightVec); // normalized object-space light vector
	float3 nV = normalize(input.osEyeVec); // normalized object-space view vector
	float3 nN = input.normal; // normalized object-space normal vector

	// light intensity
	float intensFactor = min(dot(input.osLightVec, input.osLightVec) * lightRecSqrRadius, 1.0);
	float3 intens = lightColor * BezierEase(1.0 - sqrt(intensFactor));

	// specular reflection term (N.H)
	float specFactor = min(abs(dot(nN, normalize(nL + nV))), 1.0);
	float spec = pow(specFactor, 16.0) * 0.25;

	// Lambertian diffuse reflection term (N.L)
	float diffuse = max(dot(nN, nL), 0);
	float3 color = (base.rgb * diffuse.rrr + spec.rrr) * intens * intensity;
	float alpha = lerp(opaque, 1.0, base.a);
	float4 final = float4(color.rgb * alpha, alpha);

	return final;
}

#endif
