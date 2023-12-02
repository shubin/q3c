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
// dynamic light vertex and pixel shaders

#include "shared.hlsli"

cbuffer VertexShaderBuffer
{
    matrix modelViewMatrix;
    matrix projectionMatrix;
	float4 clipPlane;
	float4 osLightPos;
	float4 osEyePos;
};

struct VIn
{
	float4 position : POSITION;
	float4 normal : NORMAL;
	float2 texCoords : TEXCOORD0;
};

struct VOut
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texCoords : TEXCOORD0;
	float3 osLightVec : TEXCOORD1;
	float3 osEyeVec : TEXCOORD2;
	float clipDist : SV_ClipDistance0;
};

VOut vs_main(VIn input)
{
	float4 positionVS = mul(modelViewMatrix, float4(input.position.xyz, 1));

	VOut output;
	output.position = mul(projectionMatrix, positionVS);
	output.normal = input.normal.xyz;
	output.texCoords = input.texCoords;
	output.osLightVec = osLightPos.xyz - input.position.xyz;
	output.osEyeVec = osEyePos.xyz - input.position.xyz;
	output.clipDist = dot(positionVS, clipPlane);

	return output;
}

cbuffer PixelShaderBuffer
{
	float3 lightColor;
	float lightRadius;
	float opaque;
	float intensity;
	float greyscale;
	float dummy;
};

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

float BezierEase(float t)
{
	return t * t * (3.0 - 2.0 * t);
}

float4 ps_main(VOut input) : SV_TARGET
{
	float4 base = MakeGreyscale(texture0.Sample(sampler0, input.texCoords), greyscale);
	float3 nL = normalize(input.osLightVec); // normalized object-space light vector
	float3 nV = normalize(input.osEyeVec); // normalized object-space view vector
	float3 nN = input.normal; // normalized object-space normal vector

	// light intensity
	float intensFactor = min(dot(input.osLightVec, input.osLightVec) * lightRadius, 1.0);
	float3 intens = lightColor * BezierEase(1.0 - sqrt(intensFactor));

	// specular reflection term (N.H)
	float specFactor = min(abs(dot(nN, normalize(nL + nV))), 1.0);
	float spec = pow(specFactor, 16.0) * 0.25;

	// Lambertian diffuse reflection term (N.L)
	float diffuse = min(abs(dot(nN, nL)), 1.0);
	float3 color = (base.rgb * diffuse.rrr + spec.rrr) * intens * intensity;
	float alpha = lerp(opaque, 1.0, base.a);
	float4 final = float4(color.rgb * alpha, alpha);

	return final;
}
