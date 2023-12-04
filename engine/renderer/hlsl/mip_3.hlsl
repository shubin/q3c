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
// mip-map generation: linear-space to gamma-space transform


cbuffer RootConstants
{
	float4 blendColor;
	float intensity;
	float invGamma; // 1.0 / gamma
	uint srcMip;
	uint dstMip;
}

RWTexture2D<float4> mips[2 + 16] : register(u0);

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

	// yes, intensity *should* be done in light-linear space
	// but we keep the old behavior for consistency...
	float4 in0 = src[id.xy];
	float3 in1 = 0.5 * (in0.rgb + blendColor.rgb);
	float3 inV = lerp(in0.rgb, in1.rgb, blendColor.a);
	float3 out0 = pow(max(inV, 0.0), invGamma);
	float3 out1 = out0 * intensity;
	float4 outV = saturate(float4(out1, in0.a));
	dst[id.xy] = outV;
}
