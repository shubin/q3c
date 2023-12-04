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
// fog volume (AABB) seen from inside


#include "shared.hlsli"
#include "fog.hlsli"


#if PIXEL_SHADER

float4 ps(VOut input) : SV_Target
{
	float zwDepth = depthTexture.Load(int3(input.position.xy, 0)).x;
	float depthBuff = LinearDepth(zwDepth, input.proj2232.x, input.proj2232.y);
	float depthFrag = input.depthVS;
	float depth = min(depthBuff, depthFrag);
	float fogOpacity = saturate(depth / colorDepth.w);

	return float4(colorDepth.rgb, fogOpacity);

	// depth test debugging
	//return depthFrag <= depthBuff ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);

	// depth linearization debugging
	//return float4(0, abs(depthBuff - depthFrag) < 1 ? 1 : 0, 0, 1);
}

#endif
