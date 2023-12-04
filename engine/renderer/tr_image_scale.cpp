/*
===========================================================================
Copyright (C) 2022-2023 Gian 'myT' Schellenbaum

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
// high-quality image resampling at an affordable price

#include "tr_local.h"

#include <emmintrin.h>


#if !idSSE2
#error Unsupported architecture
#endif


#define SIMD_ALIGNMENT       32
#define SIMD_ALIGNMENT_MASK  (SIMD_ALIGNMENT - 1)
#define IMAGE_SIZE_F32       (MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE * sizeof(float) * 4)

#define FatalError(Fmt, ...) ri.Error(ERR_FATAL, Fmt "\n", ##__VA_ARGS__)


struct allocator_t {
	byte* buffer;
	int byteCount;
	int topBytes;
	int bottomBytes;
	qbool fromTop;
};

struct filter_t {
	const char* cvarName;
	float (*function)(float x);
	float support;
};

struct imageU8_t {
	byte* data;
	int width;
	int height;
};

struct imageF32_t {
	float* data;
	int width;
	int height;
};

struct contrib_t {
	int byteOffset;
	float weight;
};

struct contribList_t {
	int firstIndex;
};

struct imageContribs_t {
	contrib_t* contribs;
	contribList_t* contribLists;
	int contribsPerPixel;
};


// store enough space for 2 full images and some extra for contribution lists
static byte bigBuffer[(2 * IMAGE_SIZE_F32) + (2 << 20)];
static allocator_t allocator;
static textureWrap_t wrapMode = TW_REPEAT;


static void MEM_Init()
{
	if (allocator.buffer != NULL) {
		return;
	}

	allocator.buffer = (byte*)(size_t(bigBuffer + SIMD_ALIGNMENT_MASK) & size_t(~SIMD_ALIGNMENT_MASK));
	allocator.byteCount = (int)((bigBuffer + sizeof(bigBuffer)) - allocator.buffer) & (~SIMD_ALIGNMENT_MASK);
	allocator.topBytes = 0;
	allocator.bottomBytes = 0;
	allocator.fromTop = qfalse;

	assert(allocator.byteCount % SIMD_ALIGNMENT == 0);
}


static void* MEM_Alloc( int byteCount )
{
	byteCount = (byteCount + SIMD_ALIGNMENT_MASK) & (~SIMD_ALIGNMENT_MASK);
	const int freeCount = allocator.byteCount - allocator.topBytes - allocator.bottomBytes;
	if (byteCount > freeCount) {
		FatalError("Couldn't allocate %d bytes, only %d bytes free!", byteCount, freeCount);
	}

	if (allocator.fromTop) {
		allocator.topBytes += byteCount;
		return allocator.buffer + allocator.byteCount - allocator.topBytes;
	}

	void* result = allocator.buffer + allocator.bottomBytes;
	allocator.bottomBytes += byteCount;
	return result;
}


static void MEM_ClearAll()
{
	allocator.bottomBytes = 0;
	allocator.topBytes = 0;
}


static void MEM_FlipAndClearSide()
{
	allocator.fromTop = !allocator.fromTop;
	if (allocator.fromTop) {
		allocator.topBytes = 0;
	} else {
		allocator.bottomBytes = 0;
	}
}


static int clamp( int val, int min, int max )
{
	return val < min ? min : (val > max ? max : val);
}


static float sinc( float x )
{
	x *= M_PI;

	if ((x < 0.01f) && (x > -0.01f)) {
		return 1.0f + x * x * (-1.0f / 6.0f + x * x * 1.0f / 120.0f);
	}

	return sin(x) / x;
}


static float clean( float x )
{
	if (fabsf(x) < 0.0000125f) {
		return 0.0f;
	}

	return x;
}


static float Tent1( float x )
{
	x = fabs(x);
	if (x <= 1.0f) {
		return 1.0f - x;
	}

	return 0.0f;
}


// Mitchell-Netravali with B = 1/3 and C = 1/3
static float MitchellNetravali2( float x )
{
	x = fabs(x);

	if (x < 1.0f) {
		return (16.0f + x * x * (21.0f * x - 36.0f)) / 18.0f;
	} else if (x < 2.0f) {
		return (32.0f + x * (-60.0f + x * (36.0f - 7.0f * x))) / 18.0f;
	}

	return 0.0f;
}


template <int S>
static float Lanczos( float x )
{
	x = fabs(x);

	if (x < (float)S) {
		return clean(sinc(x) * sinc(x / (float)S));
	}

	return 0.0f;
}


template <int S>
static float BlackmanHarris( float x )
{
	const float a0 = 0.35875f;
	const float a1 = 0.48829f;
	const float a2 = 0.14128f;
	const float a3 = 0.01168f;
	const float N = (float)S;
	x = 2.0f * M_PI * (x / N + 0.5f);

	return a0 - a1 * cosf(x) + a2 * cosf(2.0f * x) - a3 * cosf(3.0f * x);
}


// matches id's original filter
static float idTent2( float x )
{
	x = fabs(x);
	if (x <= 1.25f) {
		return 1.25f - x;
	}

	return 0.0f;
}


static const filter_t filters[] =
{
	{ "L4", Lanczos<4>, 4.0f },
	{ "L3", Lanczos<3>, 3.0f },
	{ "MN2", MitchellNetravali2, 2.0f },
	{ "BH4", BlackmanHarris<4>, 4.0f },
	{ "BH3", BlackmanHarris<3>, 3.0f },
	{ "BH2", BlackmanHarris<2>, 2.0f },
	{ "BL", Tent1, 1.0f },
	{ "T2", idTent2, 2.0f }
};


static void IMG_U8_Allocate( imageU8_t* output, int width, int height )
{
	output->data = (byte*)MEM_Alloc(width * height * 4 * sizeof(byte));
	output->width = width;
	output->height = height;
}


static void IMG_F32_Allocate( imageF32_t* output, int width, int height )
{
	output->data = (float*)MEM_Alloc(width * height * 4 * sizeof(float));
	output->width = width;
	output->height = height;
}


#if 0

// transform from and into pre-multiplied alpha form

static void IMG_U8toF32_InvGamma_PreMul( imageF32_t* output, const imageU8_t* input )
{
	assert((size_t)output->data % 16 == 0);

	float* out = output->data;
	const byte* in = input->data;
	const byte* inEnd = input->data + input->width * input->height * 4;

	const float scaleArray[4] = { 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f };
	const __m128 scale = _mm_loadu_ps(scaleArray);
	const __m128i zero = _mm_setzero_si128();
	const __m128 zero3one = _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f);
	const __m128 alphaMask = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1));

	while (in < inEnd) {
		const __m128i inputu8 = _mm_cvtsi32_si128(*(const int*)in);
		const __m128i inputu16 = _mm_unpacklo_epi8(inputu8, zero);
		const __m128i inputu32 = _mm_unpacklo_epi8(inputu16, zero);
		const __m128 inputf32 = _mm_cvtepi32_ps(inputu32);
		const __m128 nonLinear = _mm_mul_ps(inputf32, scale);            // [A Bs Gs Rs] non-linear space
		const __m128 linearX = _mm_mul_ps(nonLinear, nonLinear);         // [? B G R]
		const __m128 alpha = _mm_shuffle_ps(nonLinear, nonLinear, 0xFF); // [A A A A]
		const __m128 linear0 = _mm_and_ps(linearX, alphaMask);           // [0 B G R]
		const __m128 linear1 = _mm_or_ps(linear0, zero3one);             // [1 B G R]
		const __m128 outputf32 = _mm_mul_ps(linear1, alpha);             // [A B*A G*A R*A] pre-multiplied alpha in linear space
		_mm_stream_ps(out, outputf32);

		out += 4;
		in += 4;
	}

	_mm_sfence();
}


static void IMG_DeMul_Gamma_F32toU8( imageU8_t* output, const imageF32_t* input )
{
	assert((size_t)input->data % 16 == 0);
	assert((size_t)output->data % 16 == 0);

	byte* out = output->data;
	const float* in = input->data;
	const float* inEnd = input->data + input->width * input->height * 4;

	const float scaleArray[4] = { 255.0f, 255.0f, 255.0f, 255.0f };
	const __m128 scale = _mm_loadu_ps(scaleArray);
	const __m128 alphaMask = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1)); // [0 -1 -1 -1]
	const __m128 alphaScale = _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f);            // [1 0 0 0]

	while (in < inEnd) {
		const __m128 inputf32 = _mm_load_ps(in);                       // [A B*A G*A R*A] pre-multiplied alpha in linear space
		const __m128 alpha = _mm_shuffle_ps(inputf32, inputf32, 0xFF); // [A A A A]
		const __m128 linear1 = _mm_div_ps(inputf32, alpha);            // [1 B G R]
		const __m128 zero3alpha = _mm_mul_ps(alpha, alphaScale);       // [A 0 0 0]
		const __m128 nonLinear1 = _mm_sqrt_ps(linear1);                // [1 Bs Gs Rs]
		const __m128 nonLinear0 = _mm_and_ps(nonLinear1, alphaMask);   // [0 Bs Gs Rs]
		const __m128 nonLinear = _mm_or_ps(nonLinear0, zero3alpha);    // [A Bs Gs Rs] non-linear space
		const __m128 outputf32 = _mm_mul_ps(nonLinear, scale);
		const __m128i outputu32 = _mm_cvtps_epi32(outputf32);
		const __m128i outputu16 = _mm_packs_epi32(outputu32, outputu32);
		const __m128i outputu8 = _mm_packus_epi16(outputu16, outputu16);
		*(int*)out = _mm_cvtsi128_si32(outputu8);

		out += 4;
		in += 4;
	}
}

#endif


static void IMG_U8toF32_InvGamma( imageF32_t* output, const imageU8_t* input )
{
	assert((size_t)output->data % 16 == 0);

	float* out = output->data;
	const byte* in = input->data;
	const byte* inEnd = input->data + input->width * input->height * 4;

	const float scaleArray[4] = { 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f };
	const __m128 scale = _mm_loadu_ps(scaleArray);
	const __m128i zero = _mm_setzero_si128();
	const __m128 colorMask = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1));
	const __m128 alphaMask = _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0));

	while (in < inEnd) {
		const __m128i inputu8 = _mm_cvtsi32_si128(*(const int*)in);
		const __m128i inputu16 = _mm_unpacklo_epi8(inputu8, zero);
		const __m128i inputu32 = _mm_unpacklo_epi8(inputu16, zero);
		const __m128 inputf32 = _mm_cvtepi32_ps(inputu32);
		const __m128 nonLinear = _mm_mul_ps(inputf32, scale);    // [A Bs Gs Rs] non-linear space
		const __m128 alpha0 = _mm_and_ps(nonLinear, alphaMask);  // [A 0 0 0]
		const __m128 linearX = _mm_mul_ps(nonLinear, nonLinear); // [? B G R]
		const __m128 linear0 = _mm_and_ps(linearX, colorMask);   // [0 B G R]
		const __m128 outputf32 = _mm_or_ps(alpha0, linear0);     // [A B G R] linear space
		_mm_stream_ps(out, outputf32);

		out += 4;
		in += 4;
	}

	_mm_sfence();
}


static void IMG_Gamma_F32toU8( imageU8_t* output, const imageF32_t* input )
{
	assert((size_t)input->data % 16 == 0);
	assert((size_t)output->data % 16 == 0);

	byte* out = output->data;
	const float* in = input->data;
	const float* inEnd = input->data + input->width * input->height * 4;

	const float scaleArray[4] = { 255.0f, 255.0f, 255.0f, 255.0f };
	const __m128 scale = _mm_loadu_ps(scaleArray);
	const __m128 colorMask = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1));
	const __m128 alphaMask = _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0));

	while (in < inEnd) {
		const __m128 linear = _mm_load_ps(in);                       // [A B G R] linear space
		const __m128 alpha0 = _mm_and_ps(linear, alphaMask);         // [A 0 0 0]
		const __m128 nonLinearX = _mm_sqrt_ps(linear);               // [? Bs Gs Rs]
		const __m128 nonLinear0 = _mm_and_ps(nonLinearX, colorMask); // [0 Bs Gs Rs]
		const __m128 nonLinear = _mm_or_ps(nonLinear0, alpha0);      // [A Bs Gs Rs] non-linear space
		const __m128 outputf32 = _mm_mul_ps(nonLinear, scale);
		const __m128i outputu32 = _mm_cvtps_epi32(outputf32);
		const __m128i outputu16 = _mm_packs_epi32(outputu32, outputu32);
		const __m128i outputu8 = _mm_packus_epi16(outputu16, outputu16);
		*(int*)out = _mm_cvtsi128_si32(outputu8);

		out += 4;
		in += 4;
	}
}


static int IMG_WrapPixel( int p, int size, textureWrap_t textureWrap )
{
	if (textureWrap == TW_CLAMP_TO_EDGE) {
		return clamp(p, 0, size - 1);
	}

	return (p + size * 1024) % size;
}


static void IMG_CreateContribs( imageContribs_t* contribs, int srcSize, int dstSize, int byteScale, const filter_t* filter )
{
	const float scale = (float)srcSize / (float)dstSize;
	const float recScale = 1.0f / (float)scale;
	const int filterHalfWidthI = (int)ceilf(filter->support * (float)scale / 2.0f);
	const int maxContribsPerPixel = filterHalfWidthI * 2;

	contribs->contribLists = (contribList_t*)MEM_Alloc(2 * dstSize * sizeof(contribList_t));
	contribs->contribs = (contrib_t*)MEM_Alloc(2 * dstSize * maxContribsPerPixel * sizeof(contrib_t));
	contribs->contribsPerPixel = 0;

	contribList_t* contribList = contribs->contribLists;
	contrib_t* contrib = contribs->contribs;

	for (int dstPos = 0; dstPos < dstSize; ++dstPos) {
		const int centerI = (int)(dstPos * scale); // the "real" center is 0.5 higher
		const float centerF = (float)centerI + 0.5f;
		const int start = centerI - filterHalfWidthI + 1;
		const int end = centerI + filterHalfWidthI;
		const int firstIndex = (int)(contrib - contribs->contribs);

		float totalWeight = 0.0f;
		for (int k = start; k <= end; ++k) {
			const float delta = ((float)k - centerF) * recScale;
			totalWeight += filter->function(delta) * recScale;
		}
		const float recWeight = 1.0f / totalWeight;

		int contribCount = 0;
		for (int k = start; k <= end; ++k) {
			const int srcPos = IMG_WrapPixel(k, srcSize, wrapMode);
			const float delta = ((float)k - centerF) * recScale;
			const float weight = filter->function(delta) * recScale * recWeight;

			if (weight == 0.0f) {
				continue;
			}

			contrib->byteOffset = srcPos * byteScale;
			contrib->weight = weight;
			contrib++;
			contribCount++;
		}

		const int onePastLastIndex = (int)(contrib - contribs->contribs);
		contribList->firstIndex = firstIndex;
		if (contribs->contribsPerPixel != 0 && contribs->contribsPerPixel != onePastLastIndex - firstIndex) {
			FatalError("Couldn't create a valid contribution list!");
		}
		contribs->contribsPerPixel = onePastLastIndex - firstIndex;
		contribList++;
	}
}


static void IMG_F32_DownScaleX( imageF32_t* output, const imageF32_t* input, const filter_t* filter )
{
	assert((size_t)input->data % 16 == 0);
	assert((size_t)output->data % 16 == 0);

	imageContribs_t contribs;
	IMG_CreateContribs(&contribs, input->width, output->width, 4 * sizeof(float), filter);

	float* outFloat = output->data;
	const int height = output->height;
	const int sw = input->width;
	const int contribsPerPixel = contribs.contribsPerPixel;

	for (int y = 0; y < height; ++y) {
		const char* inChar = (const char*)(input->data + (y * sw * 4));
		const contribList_t* contribList = contribs.contribLists;
		const contribList_t* contribListEnd = contribList + output->width;

		while (contribList < contribListEnd) {
			__m128 sum = _mm_setzero_ps();
			const contrib_t* contrib = contribs.contribs + contribList->firstIndex;
			const contrib_t* contribEnd = contrib + contribsPerPixel;

			while (contrib < contribEnd) {
				const float* in = (const float*)(inChar + contrib->byteOffset);
				const __m128 pixel = _mm_load_ps(in);
				const __m128 weights = _mm_set_ps1(contrib->weight);
				const __m128 weighted = _mm_mul_ps(pixel, weights);
				sum = _mm_add_ps(sum, weighted);

				contrib++;
			}

			_mm_stream_ps(outFloat, sum);

			contribList++;
			outFloat += 4;
		}
	}

	_mm_sfence();
}


static void IMG_F32_DownScaleY( imageF32_t* output, const imageF32_t* input, const filter_t* filter )
{
	assert((size_t)input->data % 16 == 0);
	assert((size_t)output->data % 16 == 0);

	imageContribs_t contribs;
	IMG_CreateContribs(&contribs, input->height, output->height, input->width * 4 * sizeof(float), filter);

	const int width = output->width;

	float* outFloat = output->data;
	const contribList_t* contribList = contribs.contribLists;
	const contribList_t* contribListEnd = contribList + output->height;
	const int contribsPerPixel = contribs.contribsPerPixel;

	while (contribList < contribListEnd) {
		for (int x = 0; x < width; ++x) {
			const char* inChar = (const char*)(input->data + (x * 4));

			__m128 sum = _mm_setzero_ps();
			const contrib_t* contrib = contribs.contribs + contribList->firstIndex;
			const contrib_t* contribEnd = contrib + contribsPerPixel;

			while (contrib < contribEnd) {
				const float* inFloat = (const float*)(inChar + contrib->byteOffset);
				const __m128 pixel = _mm_load_ps(inFloat);
				const __m128 weights = _mm_set_ps1(contrib->weight);
				const __m128 weighted = _mm_mul_ps(pixel, weights);
				sum = _mm_add_ps(sum, weighted);

				contrib++;
			}

			_mm_stream_ps(outFloat, sum);

			outFloat += 4;
		}

		contribList++;
	}

	_mm_sfence();
}


void IMG_U8_BilinearDownsample( imageU8_t* output, const imageU8_t* input )
{
	assert((size_t)output->data % 16 == 0);
	assert(output->width == input->width / 2);
	assert(output->height == input->height / 2);
	assert(output->height % 2 == 0);
	assert(output->width >= 4);
	assert(output->width % 4 == 0);

	const int xs = output->width / 4;
	const int ys = output->height;
	const int srcPitch = input->width * 4;
	byte* src = input->data;
	byte* dst = output->data;

	for (int y = 0; y < ys; ++y) {
		for (int x = 0; x < xs; ++x) {
			const __m128i inputTop0 = _mm_loadu_si128((const __m128i*)src);
			const __m128i inputTop1 = _mm_loadu_si128((const __m128i*)(src + 16));
			const __m128i inputBot0 = _mm_loadu_si128((const __m128i*)(src + srcPitch));
			const __m128i inputBot1 = _mm_loadu_si128((const __m128i*)(src + srcPitch + 16));
			const __m128i avg0 = _mm_avg_epu8(inputTop0, inputBot0);
			const __m128i avg1 = _mm_avg_epu8(inputTop1, inputBot1);
			const __m128i shuf00 = _mm_shuffle_epi32(avg0, (2 << 2) | 0);
			const __m128i shuf01 = _mm_shuffle_epi32(avg0, (3 << 2) | 1);
			const __m128i shuf10 = _mm_shuffle_epi32(avg1, (2 << 2) | 0);
			const __m128i shuf11 = _mm_shuffle_epi32(avg1, (3 << 2) | 1);
			const __m128i shuf0 = _mm_unpacklo_epi64(shuf00, shuf10);
			const __m128i shuf1 = _mm_unpacklo_epi64(shuf01, shuf11);
			const __m128i final = _mm_avg_epu8(shuf0, shuf1);
			_mm_stream_si128((__m128i*)dst, final);

			src += 32;
			dst += 16;
		}

		src += srcPitch;
	}

	_mm_sfence();
}


static void SelectFilter( filter_t* filter, const char* name )
{
	for (int i = 0; i < ARRAY_LEN(filters); ++i) {
		if (!Q_stricmp(name, filters[i].cvarName)) {
			*filter = filters[i];
			return;
		}
	}

	filter->cvarName = "L4";
	filter->function = Lanczos<4>;
	filter->support = 4.0f;
}


static void SelectFilter( filter_t* filter )
{
	return SelectFilter(filter, r_mipGenFilter->string);
}


#if 0
// this is the code that computes the weights for the mip-mapping compute shaders
static void PrintWeights()
{
	for (int f = 0; f < ARRAY_LEN(filters); ++f) {
		Filter filter = filters[f];

		float contribs[8];
		int numContribs = 0;
		float totalWeight = 0.0f;

		float support = filter.support;
		const int count = (int)ceilf(support * 2.0f);

		float position = -support + 0.5f;
		for (int i = 0; i < count; ++i) {
			const float v = filter.function(position / 2.0f);
			position += 1.0f;
			totalWeight += v;
			contribs[numContribs++] = v;
		}

		for (int c = 0; c < numContribs; ++c) {
			contribs[c] /= totalWeight;
		}

		Sys_DebugPrintf("%s: ", filter.cvarName);
		numContribs /= 2;
		for (int c = 0; c < numContribs; ++c) {
			Sys_DebugPrintf("%f ", contribs[c]);
		}
		Sys_DebugPrintf("\n");
	}
}
#endif


void R_ResampleImage( byte** outD, int outW, int outH, const byte* inD, int inW, int inH, textureWrap_t tw )
{
	MEM_Init();
	MEM_ClearAll();

	imageU8_t inputU8;
	inputU8.width = inW;
	inputU8.height = inH;
	inputU8.data = (byte*)inD;
	wrapMode = tw;

	filter_t filter;
	SelectFilter(&filter);

	if (filter.function == Tent1 &&
	    outW == inW / 2 &&
	    outW >= 4 &&
	    outW % 4 == 0 &&
	    outH == inH / 2 &&
	    outH % 2 == 0) {
		imageU8_t outputU8;
		IMG_U8_Allocate(&outputU8, outW, outH);
		MEM_FlipAndClearSide();
		IMG_U8_BilinearDownsample(&outputU8, &inputU8);
		*outD = outputU8.data;
		return;
	}

	imageF32_t inputF32;
	IMG_F32_Allocate(&inputF32, inputU8.width, inputU8.height);
	MEM_FlipAndClearSide();
	IMG_U8toF32_InvGamma(&inputF32, &inputU8);

	// X-axis
	imageF32_t tempF32;
	if (outW != inputU8.width) {
		IMG_F32_Allocate(&tempF32, outW, inputU8.height);
		IMG_F32_DownScaleX(&tempF32, &inputF32, &filter);
		MEM_FlipAndClearSide();
	} else {
		tempF32 = inputF32;
	}

	// Y-axis
	imageF32_t outputF32;
	if (outH != inputU8.height) {
		IMG_F32_Allocate(&outputF32, outW, outH);
		IMG_F32_DownScaleY(&outputF32, &tempF32, &filter);
		MEM_FlipAndClearSide();
	} else {
		outputF32 = tempF32;
	}

	imageU8_t outputU8;
	IMG_U8_Allocate(&outputU8, outputF32.width, outputF32.height);
	MEM_FlipAndClearSide();
	IMG_Gamma_F32toU8(&outputU8, &outputF32);
	*outD = outputU8.data;
}


void R_MipMap( byte** outD, const byte* inD, int inW, int inH, textureWrap_t tw )
{
	const int outW = max(inW >> 1, 1);
	const int outH = max(inH >> 1, 1);
	R_ResampleImage(outD, outW, outH, inD, inW, inH, tw);
}
