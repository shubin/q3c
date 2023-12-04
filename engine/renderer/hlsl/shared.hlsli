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
// helper functions used by multiple shader files


float4 MakeGreyscale(float4 input, float amount)
{
	float grey = dot(input.rgb, float3(0.299, 0.587, 0.114));
	float4 result = lerp(input, float4(grey, grey, grey, input.a), amount);

	return result;
}

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

// this is from Morgan McGuire's "Hashed Alpha Testing" paper
float Hash(float2 input)
{
	return frac(1.0e4 * sin(17.0 * input.x + 0.1 * input.y) + (0.1 + abs(sin(13.0 * input.y + input.x))));
}

float LinearColor(float color, float invBrightness, float invGamma)
{
	return pow(abs(color * invBrightness), invGamma) * sign(color);
}

float4 Dither(float4 color, float3 position, float seed, float noiseScale, float invBrightness, float invGamma)
{
	float2 newSeed = position.xy + float2(0.6849, 0.6849) * seed + float2(position.z, position.z);
	float noise = (noiseScale / 255.0) * LinearColor(Hash(newSeed) - 0.5, invBrightness, invGamma);

	return color + float4(noise, noise, noise, 0.0);
}

// from NVIDIA's 2007 "Soft Particles" whitepaper by Tristan Lorach
float Contrast(float d, float power)
{
	bool aboveHalf = d > 0.5;
	float base = saturate(2.0 * (aboveHalf ? (1.0 - d) : d));
	float r = 0.5 * pow(base, power);

	return aboveHalf ? (1.0 - r) : r;
}

float GetBitAsFloat(uint bits, uint bitIndex)
{
	return (bits & (1u << bitIndex)) ? 1.0 : 0.0;
}

float2 UnpackHalf2(uint data)
{
	return float2(f16tof32(data), f16tof32(data >> 16));
}
