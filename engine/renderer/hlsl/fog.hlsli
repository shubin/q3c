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
// fog volume (AABB) rendering - shared code


struct VOut
{
	float4 position : SV_Position;
	float2 proj2232 : TEXCOORD0;
	float depthVS : DEPTHVS;
};


#ifdef VERTEX_SHADER

cbuffer RootConstants
{
	matrix modelViewMatrix;
	matrix projectionMatrix;
	float4 boxMin;
	float4 boxMax;
};

VOut vs(float3 positionOS : POSITION)
{
	float3 positionWS = boxMin.xyz + positionOS * (boxMax.xyz - boxMin.xyz);
	float4 positionVS = mul(modelViewMatrix, float4(positionWS, 1));

	VOut output;
	output.position = mul(projectionMatrix, positionVS);
	output.proj2232 = float2(-projectionMatrix[2][2], projectionMatrix[2][3]);
	output.depthVS = -positionVS.z;

	return output;
}

#endif


#ifdef PIXEL_SHADER

cbuffer RootConstants
{
	float4 colorDepth;
};

Texture2D depthTexture : register(t0);

#endif
