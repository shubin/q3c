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
// gamma-space UINT8 to linear-space FLOAT compute shader

Texture2D<uint4>    src : register(t0);
RWTexture2D<float4> dst : register(u0);

cbuffer Constants : register(b0)
{
	float gamma;
	float dummy0;
	float dummy1;
	float dummy2;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 id : SV_DispatchThreadID)
{
	float4 v = src[id.xy] / 255.0;
	dst[id.xy] = float4(pow(v.xyz, gamma), v.a);
}
