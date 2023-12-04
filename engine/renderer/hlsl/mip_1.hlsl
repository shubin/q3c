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
// mip-map generation: gamma-space to linear-space transform


cbuffer RootConstants
{
	float gamma;
}

RWTexture2D<float4> src : register(u2);
RWTexture2D<float4> dst : register(u0);

[numthreads(8, 8, 1)]
void cs(uint3 id : SV_DispatchThreadID)
{
	// @TODO: is this actually required?
	uint w, h;
	dst.GetDimensions(w, h);
	if(any(id.xy >= uint2(w, h)))
	{
		return;
	}

	float4 v = src[id.xy];
	dst[id.xy] = float4(pow(v.xyz, gamma), v.a);
}
