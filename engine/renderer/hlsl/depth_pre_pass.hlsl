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
// depth pre-pass


#if VERTEX_SHADER

cbuffer RootConstants
{
	matrix modelViewMatrix;
	matrix projectionMatrix;
};

float4 vs(float4 position : POSITION) : SV_Position
{
	float4 positionVS = mul(modelViewMatrix, float4(position.xyz, 1));

	return mul(projectionMatrix, positionVS);
}

#endif


#if PIXEL_SHADER

void ps()
{
}

#endif
