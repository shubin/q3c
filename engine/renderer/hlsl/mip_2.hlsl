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
// mip-map generation: 8-tap 1D filter


cbuffer RootConstants
{
	float4 weights;
	int2 maxSize;
	int2 scale;
	int2 offset;
	uint clampMode; // 0 = repeat
	uint srcMip;
	uint dstMip;
}

RWTexture2D<float4> mips[2] : register(u0);

uint2 FixCoords(int2 c)
{
	if(clampMode > 0)
	{
		// clamp
		return uint2(clamp(c, int2(0, 0), maxSize));
	}

	// repeat
	return uint2(c & maxSize);
}

[numthreads(8, 8, 1)]
void cs(uint3 id : SV_DispatchThreadID)
{
	RWTexture2D<float4> src = mips[srcMip];
	RWTexture2D<float4> dst = mips[dstMip];

	// @TODO: is this actually required?
	uint w, h;
	dst.GetDimensions(w, h);
	if(any(id.xy >= uint2(w, h)))
	{
		return;
	}

	int2 base = int2(id.xy) * scale;
	float4 r = float4(0, 0, 0, 0);
	r += src[FixCoords(base - offset * 3)] * weights.x;
	r += src[FixCoords(base - offset * 2)] * weights.y;
	r += src[FixCoords(base - offset * 1)] * weights.z;
	r += src[base] * weights.w;
	r += src[base + offset] * weights.w;
	r += src[FixCoords(base + offset * 2)] * weights.z;
	r += src[FixCoords(base + offset * 3)] * weights.y;
	r += src[FixCoords(base + offset * 4)] * weights.x;
	dst[id.xy] = r;
}
