/*
===========================================================================
Copyright (C) 2019-2022 Gian 'myT' Schellenbaum

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
// OpenGL 3.2+ rendering back-end

#include "tr_local.h"
#include "GL/glew.h"

#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif


/*
Current info:
- OpenGL 3.2 minimum
- GLSL 1.40 minimum
- fancy mip-map generation requires:
	- OpenGL 4.3 (or equivalent extensions)
	- GLSL 4.30
- alpha to coverage    requires GLSL 4.00
- depth fade with MSAA requires GLSL 4.00

Vertex and index data streaming notes:
- everyone: persistent coherent buffer mapping is the best option whenever available
- nVidia  : unsynchronized mapping is very slow, even without threaded driver optimization
- Intel   : glBufferSubData is painfully slow, even with immutable storage guarantees and full-range updates
- AMD     : if persistent coherent buffer mapping isn't available, AMD_pinned_memory is the best option
- AMD     : if neither persistent coherent buffer mapping nor AMD_pinned_memory, then pick glBufferSubData to be safe
- AMD     : glBufferSubData is slower than unsynchronized mapping with modern drivers
- AMD     : unsynchronized mapping drops off the performance cliff with old drivers

Known issues:
- nVidia GeForce GTX 1070 - Windows 7 - drivers 430.64
	once the GL2 back-end is used, performance crashes when switching to the GL3 back-end
- AMD Radeon HD 6950 - Windows 10 Pro version 10.0.16299 build 16299 - drivers 15.201.1151.1008
	with r_gpuMipGen 1, performance collapses big time (confirmed: whenever glTexStorage2D is called)
- AMD Radeon R7 360 - Windows 7 - drivers 14.502.0.0
	with r_gpuMipGen 1, the GPU-generated mips are corrupted (not confirmed: broken barrier implementation?)
*/


// @NOTE: MAX_VERTEXES and MAX_INDEXES are *per frame*
#define LARGEBUFFER_MAX_FRAMES      4
#define LARGEBUFFER_MAX_VERTEXES    131072
#define LARGEBUFFER_MAX_INDEXES     (LARGEBUFFER_MAX_VERTEXES * 8)

// this is the highest maximum we'll ever report
#define MAX_GPU_TEXTURE_SIZE        2048


enum PipelineId
{
	PID_GENERIC,
	PID_DYNAMIC_LIGHT,
	PID_SOFT_SPRITE,
	PID_POST_PROCESS,
	PID_COUNT
};

enum ErrorMode
{
	EM_FATAL,
	EM_PRINT,
	EM_SILENT
};

enum VertexBufferId
{
	VB_POSITION,
	VB_NORMAL,
	VB_TEXCOORD,
	VB_TEXCOORD2,
	VB_COLOR,
	VB_COUNT
};

enum AlphaTest
{
	AT_ALWAYS,
	AT_GREATER_THAN_0,
	AT_LESS_THAN_HALF,
	AT_GREATER_OR_EQUAL_TO_HALF
};

struct Program
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint computeShader;
	GLuint program;
};

struct ArrayBuffer
{
	GLuint buffer;
	GLint componentCount;
	GLenum dataType;
	GLboolean normalized;
	int capacity;
	int itemSize;
	int writeIndex;
	int readIndex;
	qbool indexBuffer;
	// persistent mapping:
	byte* mappedData;
	int pinnedByteCount; // when using AMD_pinned_memory
	GLsync fences[LARGEBUFFER_MAX_FRAMES]; // NULL means uninitialized / invalid
	int writeRangeIndex;
};

struct PipelineArrayBuffer
{
	const char* attribName;
	qbool enabled;
};

struct FrameBuffer
{
	GLuint fbo;
	GLuint color;
	GLuint depthStencil;
	qbool multiSampled;
	qbool hasDepthStencil;
	qbool hasColor;
};

enum GenericUniform
{
	GU_MODELVIEW,
	GU_PROJECTION,
	GU_CLIP_PLANE,
	GU_ALPHA_TEX,
	GU_GREYSCALE,
	GU_GAMMA_BRIGHT_NOISE_SEED, // only defined when dithering is enabled
	GU_A2C_ALPHA_BOOST,         // only defined when alpha to coverage is enabled
	GU_COUNT
};

enum DynamicLightUniform
{
	DU_MODELVIEW,
	DU_PROJECTION,
	DU_CLIP_PLANE,
	DU_LIGHT_POS,
	DU_EYE_POS,
	DU_LIGHT_COLOR_RADIUS,
	DU_OPAQUE,
	DU_INTENSITY,
	DU_GREYSCALE,
	DU_COUNT
};

enum SoftSpriteUniform
{
	SU_MODELVIEW,
	SU_PROJECTION,
	SU_CLIP_PLANE,
	SU_ALPHA_TEST,
	SU_DIST_OFFSET,
	SU_COLOR_SCALE,
	SU_COLOR_BIAS,
	SU_GREYSCALE,
	SU_COUNT
};

enum PostUniform
{
	PU_BRIGHT_GAMMA_GREY,
	PU_COUNT
};

// yes, one could use some template meta-programming horror for this...
#define MAX_UNIFORM_COUNT DU_COUNT
static const char UniformCountLargeEnoughG[(int)MAX_UNIFORM_COUNT >= (int)GU_COUNT ? 1 : -1] = { '\0' };
static const char UniformCountLargeEnoughD[(int)MAX_UNIFORM_COUNT >= (int)DU_COUNT ? 1 : -1] = { '\0' };
static const char UniformCountLargeEnoughS[(int)MAX_UNIFORM_COUNT >= (int)SU_COUNT ? 1 : -1] = { '\0' };
static const char UniformCountLargeEnoughU[(int)MAX_UNIFORM_COUNT >= (int)PU_COUNT ? 1 : -1] = { '\0' };

struct Pipeline
{
	Program program;
	const char* uniformNames[MAX_UNIFORM_COUNT];
	GLint uniformLocations[MAX_UNIFORM_COUNT];
	qbool uniformsDirty[MAX_UNIFORM_COUNT];
	GLint textureLocations[2];
	PipelineArrayBuffer arrayBuffers[VB_COUNT];
};

enum ComputePipelineId
{
	CPID_GAMMA_TO_LINEAR,
	CPID_LINEAR_TO_GAMMA,
	CPID_DOWN_SAMPLE,
	CPID_COUNT
};

struct MipMapGenerator
{
	Program programs[CPID_COUNT];
	GLuint textures[3]; // 0,1=float16 2=uint8
};

enum MappingType
{
	MT_SUBDATA,		// glBufferSubData
	MT_UNSYNC,		// glMapBufferRange with GL_MAP_UNSYNCHRONIZED_BIT
	MT_PERSISTENT,	// glMapBufferRange with GL_MAP_PERSISTENT_BIT and GL_MAP_COHERENT_BIT
	MT_AMDPIN		// glBufferData with GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD
};

struct OpenGL3
{
	char log[8192];

	int maxTextureSize;

	float modelViewMatrix[16];
	float projectionMatrix[16];
	float clipPlane[4];
	qbool enableClipPlane;
	qbool prevEnableClipPlane;
	AlphaTest alphaTest;
	qbool dlOpaque;
	float dlIntensity;
	float depthFadeScale[4];
	float depthFadeBias[4];
	float depthFadeDist;
	float depthFadeOffset;
	float greyscale;

	ArrayBuffer arrayBuffers[VB_COUNT];
	ArrayBuffer indexBuffer;

	GLuint boundTextures[2];
	int activeTextureSlot;
	cullType_t cullType;
	unsigned int srcBlendBits;
	unsigned int dstBlendBits;
	qbool enableDepthTest;
	GLenum depthFunc;
	GLboolean enableDepthWrite;
	GLenum polygonMode;
	qbool enablePolygonOffset;
	texEnv_t texEnv;
	qbool enableAlphaToCoverage;

	FrameBuffer fbMS;
	FrameBuffer fbSS[2];
	unsigned int fbReadIndex; // indexes fbSS
	qbool fbMSEnabled;

	Pipeline pipelines[PID_COUNT];
	PipelineId pipelineId;

	MappingType mappingType;

	ErrorMode errorMode;

	MipMapGenerator mipGen;

	GLuint timerQueries[8];
	qbool queryStarted[8];
	int queryWriteIndex;
	int queryReadIndex;
};

static OpenGL3 gl;


static const char* shared_fs =
"vec4 MakeGreyscale(vec4 color, float amount)\n"
"{\n"
"	float grey = dot(color.rgb, vec3(0.299, 0.587, 0.114));\n"
"	vec4 result = mix(color, vec4(grey, grey, grey, color.a), amount);\n"
"	return result;\n"
"}\n"
"\n";

static const char* generic_vs =
// a good way to test warning reports with r_verbose 1
//"#extension DOESNTEXISTLOL:warn\n"
//----------------------------------
"uniform mat4 modelView;\n"
"uniform mat4 projection;\n"
"uniform vec4 clipPlane;\n"
"\n"
"in vec4 position;\n"
"in vec2 texCoords1;\n"
"in vec2 texCoords2;\n"
"in vec4 color;\n"
"\n"
"centroid out vec2 texCoords1FS;\n"
"centroid out vec2 texCoords2FS;\n"
"centroid out vec4 colorFS;\n"
"\n"
"void main()\n"
"{\n"
"	vec4 positionVS = modelView * vec4(position.xyz, 1);\n"
"	gl_Position = projection * positionVS;\n"
"	gl_ClipDistance[0] = dot(positionVS, clipPlane);\n"
"	texCoords1FS = texCoords1;\n"
"	texCoords2FS = texCoords2;\n"
"	colorFS = color;\n"
"}\n";

static const char* generic_fs =
"uniform sampler2D texture1;\n"
"uniform sampler2D texture2;\n"
"\n"
"uniform uvec2 alphaTex;\n"
"#define alphaTest alphaTex.x\n"
"#define texEnv alphaTex.y\n"
"uniform float greyscale;\n"
"#if CNQ3_DITHER\n"
"uniform vec4 gammaBrightNoiseSeed;\n"
"#define invGamma gammaBrightNoiseSeed.x\n"
"#define invBrightness gammaBrightNoiseSeed.y\n"
"#define noiseScale gammaBrightNoiseSeed.z\n"
"#define seed gammaBrightNoiseSeed.w\n"
"#endif\n"
"#ifdef CNQ3_A2C\n"
"uniform float alphaBoost;\n"
"#endif\n"
"\n"
"centroid in vec2 texCoords1FS;\n"
"centroid in vec2 texCoords2FS;\n"
"centroid in vec4 colorFS;\n"
"\n"
"out vec4 fragColor;\n"
"\n"
"#if CNQ3_DITHER\n"
"float Hash(vec2 v)\n"
"{\n"
"	// this is from Morgan McGuire's 'Hashed Alpha Testing' paper\n"
"	return fract(1.0e4 * sin(17.0 * v.x + 0.1 * v.y) + (0.1 + abs(sin(13.0 * v.y + v.x))));\n"
"}\n"
"\n"
"float Linearize(float color)\n"
"{\n"
"	return pow(abs(color * invBrightness), invGamma) * sign(color);\n"
"}\n"
"\n"
"vec4 Dither(vec4 color, vec3 position)\n"
"{\n"
"	vec2 newSeed = position.xy + vec2(0.6849, 0.6849) * seed + vec2(position.z, position.z);\n"
"	float noise = (noiseScale / 255.0) * Linearize(Hash(newSeed) - 0.5);\n"
"\n"
"	return color + vec4(noise, noise, noise, 0.0);\n"
"}\n"
"#endif\n"
"\n"
"#if CNQ3_A2C\n"
"float CorrectAlpha(float threshold, float alpha, vec2 tc)\n"
"{\n"
"	vec2 size = vec2(textureSize(texture1, 0));\n"
"	if(min(size.x, size.y) <= 8.0)\n"
"		return alpha >= threshold ? 1.0 : 0.0;\n"
"	alpha *= 1.0 + alphaBoost * textureQueryLod(texture1, tc).x;"
"	vec2 dtc = fwidth(tc * size);\n"
"	float recScale = max(0.25 * (dtc.x + dtc.y), 1.0 / 16384.0);\n"
"	float scale = max(1.0 / recScale, 1.0);\n"
"	float ac = threshold + (alpha - threshold) * scale;\n"
"\n"
"	return ac;\n"
"}\n"
"#endif\n"
"\n"
"void main()\n"
"{\n"
"	vec4 p = texture(texture1, texCoords1FS);\n"
"	vec4 s = texture(texture2, texCoords2FS);\n"
"	vec4 r;\n"
"	if(texEnv == uint(1))\n"
"		r = colorFS * s * p;\n"
"	else if(texEnv == uint(2))\n"
"		r = s; // use input.color or not?\n"
"	else if(texEnv == uint(3))\n"
"		r = colorFS * vec4(p.rgb * (1 - s.a) + s.rgb * s.a, p.a);\n"
"	else if(texEnv == uint(4))\n"
"		r = colorFS * vec4(p.rgb + s.rgb, p.a * s.a);\n"
"	else // texEnv == 0\n"
"		r = colorFS * p;\n"
"\n"
"#if CNQ3_DITHER\n"
"	r = Dither(r, gl_FragCoord.xyz);\n"
"#endif\n"
"\n"
"#if CNQ3_A2C\n"
"	if(alphaTest == uint(1))\n"
"		r.a = r.a > 0.0 ? 1.0 : 0.0;\n"
"	if(alphaTest == uint(2))\n"
"		r.a = CorrectAlpha(uintBitsToFloat(0x3F000001), 1.0 - r.a, texCoords1FS);\n"
"	else if(alphaTest == uint(3))\n"
"		r.a = CorrectAlpha(0.5, r.a, texCoords1FS);\n"
"#else\n"
"	if(	(alphaTest == uint(1) && r.a == 0.0) ||\n"
"		(alphaTest == uint(2) && r.a >= 0.5) ||\n"
"		(alphaTest == uint(3) && r.a <  0.5))\n"
"		discard;\n"
"#endif\n"
"\n"
"	fragColor = MakeGreyscale(r, greyscale);\n"
"}\n";

static const char* dl_vs =
"uniform mat4 modelView;\n"
"uniform mat4 projection;\n"
"uniform vec4 clipPlane;\n"
"uniform vec3 osLightPos;\n"
"uniform vec3 osEyePos;\n"
"\n"
"in vec4 position;\n"
"in vec4 normal;\n"
"in vec2 texCoords1;\n"
"\n"
"out vec3 normalFS;\n"
"out vec2 texCoords1FS;\n"
"out vec3 L;\n"
"out vec3 V;\n"
"\n"
"void main()\n"
"{\n"
"	vec4 positionVS = modelView * vec4(position.xyz, 1);\n"
"	gl_Position = projection * positionVS;\n"
"	gl_ClipDistance[0] = dot(positionVS, clipPlane);\n"
"	normalFS = normal.xyz;\n"
"	texCoords1FS = texCoords1;\n"
"	L = osLightPos - position.xyz;\n"
"	V = osEyePos - position.xyz;\n"
"}\n";

static const char* dl_fs =
"uniform sampler2D texture1;\n"
"\n"
"uniform vec4 lightColorRadius;\n"
"uniform float opaque;\n"
"uniform float intensity;\n"
"uniform float greyscale;\n"
"\n"
"in vec3 normalFS;\n"
"in vec2 texCoords1FS;\n"
"in vec3 L;\n"
"in vec3 V;\n"
"\n"
"out vec4 fragColor;\n"
"\n"
"float BezierEase(float t)\n"
"{\n"
"	return t * t * (3.0 - 2.0 * t);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	vec4 base = MakeGreyscale(texture2D(texture1, texCoords1FS), greyscale);\n"
"	vec3 nL = normalize(L);\n"
"	vec3 nV = normalize(V);\n"
"\n"
"	// light intensity\n"
"	float intensFactor = min(dot(L, L) * lightColorRadius.w, 1.0);\n"
"	vec3 intens = lightColorRadius.rgb * BezierEase(1.0 - sqrt(intensFactor));\n"
"\n"
"	// specular reflection term (N.H)\n"
"	float specFactor = min(abs(dot(normalFS, normalize(nL + nV))), 1.0);\n"
"	float spec = pow(specFactor, 16.0) * 0.25;\n"
"\n"
"	// Lambertian diffuse reflection term (N.L)\n"
"	float diffuse = min(abs(dot(normalFS, nL)), 1.0);\n"
"	vec3 color = (base.rgb * vec3(diffuse) + vec3(spec)) * intens * intensity;\n"
"	float alpha = mix(opaque, 1.0, base.a);\n"
"\n"
"	vec4 r = vec4(color.rgb * alpha, alpha);\n"
"	fragColor = r;\n"
"}\n";

static const char* sprite_vs =
"uniform mat4 modelView;\n"
"uniform mat4 projection;\n"
"uniform vec4 clipPlane;\n"
"\n"
"in vec4 position;\n"
"in vec2 texCoords1;\n"
"in vec4 color;\n"
"\n"
"out vec2 texCoords1FS;\n"
"out vec4 colorFS;\n"
"out float depthVS;\n"
"out vec2 proj22_32;\n"
"\n"
"void main()\n"
"{\n"
"	vec4 positionVS = modelView * vec4(position.xyz, 1);\n"
"	gl_Position = projection * positionVS;\n"
"	gl_ClipDistance[0] = dot(positionVS, clipPlane);\n"
"	texCoords1FS = texCoords1;\n"
"	colorFS = color;\n"
"	depthVS = -positionVS.z;\n"
"	proj22_32 = vec2(-projection[2][2], projection[3][2]);\n"
"}\n";

static const char* sprite_fs =
"uniform sampler2D texture1; // diffuse texture\n"
"#if CNQ3_MSAA\n"
"uniform sampler2DMS texture2; // depth texture\n"
"#else\n"
"uniform sampler2D texture2; // depth texture\n"
"#endif\n"
"\n"
"uniform uint alphaTest;\n"
"uniform vec2 distOffset;\n"
"uniform vec4 colorScale;\n"
"uniform vec4 colorBias;\n"
"uniform float greyscale;\n"
"#define distance distOffset.x\n"
"#define offset distOffset.y\n"
"\n"
"in vec2 texCoords1FS;\n"
"in vec4 colorFS;\n"
"in float depthVS;\n"
"in vec2 proj22_32;\n"
"#define proj22 proj22_32.x\n"
"#define proj32 proj22_32.y\n"
"\n"
"out vec4 fragColor;\n"
"\n"
"float LinearDepth(float zwDepth)\n"
"{\n"
"	return proj32 / (zwDepth - proj22);\n"
"}\n"
"\n"
"float Contrast(float d, float power)\n"
"{\n"
"	bool aboveHalf = d > 0.5;\n"
"	float base = clamp(2.0 * (aboveHalf ? (1.0 - d) : d), 0.0, 1.0);\n"
"	float r = 0.5 * pow(base, power);\n"
"\n"
"	return aboveHalf ? (1.0 - r) : r;\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	vec4 r = colorFS * texture(texture1, texCoords1FS);\n"
"	if(	(alphaTest == uint(1) && r.a == 0.0) ||\n"
"		(alphaTest == uint(2) && r.a >= 0.5) ||\n"
"		(alphaTest == uint(3) && r.a <  0.5))\n"
"		discard;\n"
"\n"
"#if CNQ3_MSAA\n"
"	float depthSRaw = texelFetch(texture2, ivec2(gl_FragCoord.xy), gl_SampleID).r;\n"
"#else\n"
"	float depthSRaw = texelFetch(texture2, ivec2(gl_FragCoord.xy), 0).r;\n"
"#endif\n"
"	float depthS = LinearDepth(depthSRaw * 2.0 - 1.0);\n"
"	float depthP = depthVS - offset;\n"
"	float scale = Contrast((depthS - depthP) * distance, 2.0);\n"
"	vec4 r2 = mix(r * colorScale + colorBias, r, scale);\n"
"	fragColor = MakeGreyscale(r2, greyscale);\n"
"}\n";

static const char* post_vs =
"out vec2 texCoords1FS;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position = vec4(\n"
"		float(gl_VertexID / 2) * 4.0 - 1.0,\n"
"		float(gl_VertexID % 2) * 4.0 - 1.0,\n"
"		0.0,\n"
"		1.0);\n"
"	texCoords1FS = vec2(\n"
"		float(gl_VertexID / 2) * 2.0,\n"
"		float(gl_VertexID % 2) * 2.0);\n"
"}\n";

static const char* post_fs =
"uniform sampler2D texture1;\n"
"\n"
"uniform vec3 brightGammaGrey;\n"
"#define brightness brightGammaGrey.x\n"
"#define gamma brightGammaGrey.y\n"
"#define greyscale brightGammaGrey.z\n"
"\n"
"in vec2 texCoords1FS;\n"
"\n"
"out vec4 fragColor;\n"
"\n"
"void main()\n"
"{\n"
"	vec3 base = texture(texture1, texCoords1FS).rgb;\n"
"	vec3 gc = pow(base, vec3(gamma)) * brightness;\n"
"	fragColor = MakeGreyscale(vec4(gc.rgb, 1.0), greyscale);\n"
"}\n";

static const char* gammaToLinear_cs =
"layout (binding = 0, rgba8)   readonly  uniform image2D srcTex;\n"
"layout (binding = 1, rgba16f) writeonly uniform image2D dstTex;\n"
"\n"
"layout (location = 0) uniform float gamma;\n"
"\n"
"layout (local_size_x = 8, local_size_y = 8) in;\n"
"\n"
"void main()\n"
"{\n"
"	ivec2 coords = ivec2(gl_GlobalInvocationID);\n"
"	vec4 inV = imageLoad(srcTex, coords);\n"
"	vec4 outV = vec4(pow(inV.x, gamma), pow(inV.y, gamma), pow(inV.z, gamma), inV.a);\n"
"	imageStore(dstTex, coords, outV);\n"
"}\n";

static const char* linearToGamma_cs =
// yes, intensity *should* be done in light-linear space
// but we keep the old behavior for consistency...
"layout (binding = 0, rgba16f) readonly  uniform image2D srcTex;\n"
"layout (binding = 1, rgba8)   writeonly uniform image2D dstTex;\n"
"\n"
"layout (location = 0) uniform float intensity;\n"
"layout (location = 1) uniform vec4  blendColor;\n"
"layout (location = 2) uniform float invGamma;\n"
"\n"
"layout (local_size_x = 8, local_size_y = 8) in;\n"
"\n"
"void main()\n"
"{\n"
"	ivec2 coords = ivec2(gl_GlobalInvocationID);\n"
"	vec4 in0 = imageLoad(srcTex, coords);\n"
"	vec3 in1 = 0.5 * (in0.rgb + blendColor.rgb);\n"
"	vec3 inV = mix(in0.rgb, in1.rgb, blendColor.a);\n"
"	vec3 out0 = vec3(pow(inV.r, invGamma), pow(inV.g, invGamma), pow(inV.b, invGamma));\n"
"	vec3 out1 = out0 * intensity;\n"
"	vec4 outV = vec4(out1, in0.a);\n"
"	imageStore(dstTex, coords, outV);\n"
"}\n";

static const char* downSample_cs =
"layout (binding = 0, rgba16f) readonly  uniform image2D srcTex;\n"
"layout (binding = 1, rgba16f) writeonly uniform image2D dstTex;\n"
"\n"
"layout (location = 0) uniform vec4  weights;\n"
"layout (location = 1) uniform ivec2 maxSize;\n"
"layout (location = 2) uniform ivec2 scale;\n"
"layout (location = 3) uniform ivec2 offset;\n"
"layout (location = 4) uniform uint  clampMode; // 0 = repeat\n"
"\n"
"layout (local_size_x = 8, local_size_y = 8) in;\n"
"\n"
"ivec2 FixCoords(ivec2 c)\n"
"{\n"
	"if(clampMode > 0)\n"
"	{\n"
"		// clamp\n"
"		return clamp(c, ivec2(0, 0), maxSize);\n"
"	}\n"
"\n"
"	// repeat\n"
"	return c & maxSize;\n"
"}\n"
"\n"
"void main()\n"
"{\n"
	"ivec2 dstTC = ivec2(gl_GlobalInvocationID);\n"
"	ivec2 base  = ivec2(gl_GlobalInvocationID) * scale;\n"
"	vec4 r = vec4(0, 0, 0, 0);\n"
"	r += imageLoad(srcTex, FixCoords(base - offset * 3)) * weights.x;\n"
"	r += imageLoad(srcTex, FixCoords(base - offset * 2)) * weights.y;\n"
"	r += imageLoad(srcTex, FixCoords(base - offset    )) * weights.z;\n"
"	r += imageLoad(srcTex,           base              ) * weights.w;\n"
"	r += imageLoad(srcTex,           base + offset     ) * weights.w;\n"
"	r += imageLoad(srcTex, FixCoords(base + offset * 2)) * weights.z;\n"
"	r += imageLoad(srcTex, FixCoords(base + offset * 3)) * weights.y;\n"
"	r += imageLoad(srcTex, FixCoords(base + offset * 4)) * weights.x;\n"
"	imageStore(dstTex, dstTC, r);\n"
"}\n";


void GL_GetRenderTargetFormat(GLenum* internalFormat, GLenum* format, GLenum* type, int cnq3Format)
{
	switch(cnq3Format)
	{
		case RTCF_R10G10B10A2:
			*internalFormat = GL_RGB10_A2;
			*format = GL_BGRA;
			*type = GL_UNSIGNED_INT_2_10_10_10_REV;
			break;

		case RTCF_R16G16B16A16:
			*internalFormat = GL_RGBA16;
			*format = GL_BGRA;
			*type = GL_UNSIGNED_SHORT;
			break;

		case RTCF_R8G8B8A8:
		default:
			*internalFormat = GL_RGBA8;
			*format = GL_BGRA;
			*type = GL_UNSIGNED_BYTE;
			break;
	}
}

#if defined(_WIN32)

static void AllocatePinnedMemory(ArrayBuffer* buffer)
{
	const int byteCount = PAD(buffer->capacity * buffer->itemSize, 4096);
	buffer->mappedData = (byte*)VirtualAlloc(NULL, byteCount, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	buffer->pinnedByteCount = byteCount;
}

static void FreePinnedMemory(ArrayBuffer* buffer)
{
	VirtualFree(buffer->mappedData, 0, MEM_RELEASE);
	buffer->mappedData = NULL;
	buffer->pinnedByteCount = 0;
}

#else

static void AllocatePinnedMemory(ArrayBuffer* buffer)
{
	const int pageSizeSC = (int)sysconf(_SC_PAGE_SIZE);
	const int pageSize = pageSizeSC > 0 ? pageSizeSC : 4096;
	const int byteCount = PAD(buffer->capacity * buffer->itemSize, pageSize);
	buffer->mappedData = (byte*)mmap(NULL, byteCount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	buffer->pinnedByteCount = byteCount;
}

static void FreePinnedMemory(ArrayBuffer* buffer)
{
	munmap(buffer->mappedData, buffer->pinnedByteCount);
	buffer->mappedData = NULL;
	buffer->pinnedByteCount = 0;
}

#endif

static void HandleError(const char* message)
{
	if(gl.errorMode == EM_FATAL)
	{
		ri.Error(ERR_FATAL, message);
	}
	else if(gl.errorMode == EM_PRINT)
	{
		ri.Printf(PRINT_ERROR, message);
	}
}

// identifier must be one of:
// GL_BUFFER, GL_SHADER, GL_PROGRAM, GL_VERTEX_ARRAY, GL_QUERY, GL_SAMPLER, GL_TEXTURE,
// GL_RENDERBUFFER, GL_FRAMEBUFFER, GL_PROGRAM_PIPELINE, GL_TRANSFORM_FEEDBACK
static void SetDebugName(GLenum identifier, GLuint name, const char* string)
{
	if(GLEW_VERSION_4_3 || GLEW_KHR_debug)
	{
		glObjectLabel(identifier, name, -1, string);
	}
}

static const char* GetShaderTypeName(GLenum shaderType)
{
	switch(shaderType)
	{
		case GL_VERTEX_SHADER: return "vertex";
		case GL_FRAGMENT_SHADER: return "fragment";
		case GL_COMPUTE_SHADER: return "compute";
		default: return "???";
	}
}

static qbool CreateShader(GLuint* shaderPtr, PipelineId pipelineId, GLenum shaderType, const char* shaderSource, const char* debugName)
{
	// alpha to coverage    now requires GLSL 4.00 for textureQueryLod
	// depth fade with MSAA now requires GLSL 4.00 for gl_SampleID
	const qbool enableA2C =
		pipelineId == PID_GENERIC &&
		shaderType == GL_FRAGMENT_SHADER &&
		glInfo.alphaToCoverageSupport;
	const qbool enableDithering =
		pipelineId == PID_GENERIC &&
		shaderType == GL_FRAGMENT_SHADER &&
		r_dither->integer;
	const qbool depthFadeWithMSAA =
		pipelineId == PID_SOFT_SPRITE &&
		shaderType == GL_FRAGMENT_SHADER &&
		glInfo.depthFadeSupport &&
		gl.fbMSEnabled;
	const char* const sourceArray[] =
	{
		shaderType == GL_COMPUTE_SHADER ? "#version 430\n" : (enableA2C || depthFadeWithMSAA ? "#version 400\n" : "#version 140\n"),
		"\n",
		enableA2C ? "#define CNQ3_A2C 1\n" : "#define CNQ3_A2C 0\n",
		enableDithering ? "#define CNQ3_DITHER 1\n" : "#define CNQ3_DITHER 0\n",
		depthFadeWithMSAA ? "#define CNQ3_MSAA 1\n" : "#define CNQ3_MSAA 0\n",
		shaderType == GL_FRAGMENT_SHADER ? shared_fs : "",
		shaderSource
	};

	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, ARRAY_LEN(sourceArray), sourceArray, NULL);
	glCompileShader(shader);

	GLint result = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	const qbool success = result == GL_TRUE;
	if(success)
	{
		*shaderPtr = shader;
		SetDebugName(GL_SHADER, shader, va("%s %s shader", debugName, GetShaderTypeName(shaderType)));
	}

	if(!success || r_verbose->integer)
	{
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if(logLength > 0)
		{
			glGetShaderInfoLog(shader, sizeof(gl.log), NULL, gl.log);
			const ErrorMode em = gl.errorMode;
			gl.errorMode = success ? EM_PRINT : EM_FATAL;
			HandleError(va("'%s' %s shader compilation failed: %s\n", debugName, GetShaderTypeName(shaderType), gl.log));
			gl.errorMode = em;
		}
		else if(!success)
		{
			HandleError(va("'%s' %s shader compilation failed\n", debugName, GetShaderTypeName(shaderType)));
		}
	}

	return success;
}

static qbool FinalizeProgram(Program* prog, const char* debugName)
{
	GLint result = GL_FALSE;
	glGetProgramiv(prog->program, GL_LINK_STATUS, &result);
	const qbool success = result == GL_TRUE;
	if(success)
	{
		SetDebugName(GL_PROGRAM, prog->program, va("%s program", debugName));
	}

	if(!success || r_verbose->integer)
	{
		GLint logLength = 0;
		glGetProgramiv(prog->program, GL_INFO_LOG_LENGTH, &logLength);
		if(logLength > 0)
		{
			glGetProgramInfoLog(prog->program, sizeof(gl.log), NULL, gl.log);
			const ErrorMode em = gl.errorMode;
			gl.errorMode = success ? EM_PRINT : EM_FATAL;
			HandleError(va("'%s' program link failed: %s\n", debugName, gl.log));
			gl.errorMode = em;
		}
		else if(!success)
		{
			HandleError(va("'%s' program link failed\n", debugName));
		}
	}

	return success;
}

static qbool CreateGraphicsProgram(PipelineId pipelineId, const char* vs, const char* fs, const char* debugName)
{
	Pipeline* const pipeline = &gl.pipelines[pipelineId];
	Program* const prog = &pipeline->program;
	if(!CreateShader(&prog->vertexShader, pipelineId, GL_VERTEX_SHADER, vs, debugName) ||
	   !CreateShader(&prog->fragmentShader, pipelineId, GL_FRAGMENT_SHADER, fs, debugName))
	{
		return qfalse;
	}

	prog->program = glCreateProgram();
	glAttachShader(prog->program, prog->vertexShader);
	glAttachShader(prog->program, prog->fragmentShader);

	// glBindAttribLocation must be called before the program gets linked
	for(int i = 0; i < VB_COUNT; ++i)
	{
		if(pipeline->arrayBuffers[i].enabled)
		{
			glBindAttribLocation(pipeline->program.program, i, pipeline->arrayBuffers[i].attribName);
		}
	}

	glLinkProgram(prog->program);

	return FinalizeProgram(prog, debugName);
}

static qbool CreateComputeProgram(Program* prog, const char* cs, const char* debugName)
{
	if(!CreateShader(&prog->computeShader, PID_COUNT, GL_COMPUTE_SHADER, cs, debugName))
	{
		return qfalse;
	}

	prog->program = glCreateProgram();
	glAttachShader(prog->program, prog->computeShader);
	glLinkProgram(prog->program);

	return FinalizeProgram(prog, debugName);
}

static void CreateColorTextureStorageMS(int* samples)
{
	GLenum internalFormat, format, type;
	GL_GetRenderTargetFormat(&internalFormat, &format, &type, r_rtColorFormat->integer);

	int sampleCount = r_msaa->integer;
	while(glGetError() != GL_NO_ERROR) {} // clear the error queue

	if(GLEW_VERSION_4_2 || GLEW_ARB_internalformat_query)
	{
		GLint maxSampleCount = 0;
		glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, internalFormat, GL_SAMPLES, 1, &maxSampleCount);
		if(glGetError() == GL_NO_ERROR)
		{
			sampleCount = min(sampleCount, (int)maxSampleCount);
		}
	}

	GLenum errorCode = GL_NO_ERROR;
	for(;;)
	{
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sampleCount, internalFormat, glConfig.vidWidth, glConfig.vidHeight, GL_TRUE);
		errorCode = glGetError();
		if(errorCode == GL_NO_ERROR || sampleCount == 0)
		{
			break;
		}

		--sampleCount;
	}

	if(errorCode != GL_NO_ERROR)
	{
		ri.Error(ERR_FATAL, "Failed to create multi-sampled texture storage (error 0x%X)\n", (unsigned int)errorCode);
	}

	GLint realSampleCount = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &realSampleCount);
	if(glGetError() == GL_NO_ERROR && realSampleCount > 0)
	{
		sampleCount = realSampleCount;
	}

	*samples = sampleCount;
}

static void FBO_CreateSS(FrameBuffer* fb, qbool color, qbool depthStencil, const char* name)
{
	if(depthStencil)
	{
		glGenTextures(1, &fb->depthStencil);
		glBindTexture(GL_TEXTURE_2D, fb->depthStencil);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, glConfig.vidWidth, glConfig.vidHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		SetDebugName(GL_TEXTURE, fb->depthStencil, va("%s depth/stencil attachment", name));
	}

	if(color)
	{
		GLenum internalFormat, format, type;
		GL_GetRenderTargetFormat(&internalFormat, &format, &type, r_rtColorFormat->integer);
		glGenTextures(1, &fb->color);
		glBindTexture(GL_TEXTURE_2D, fb->color);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, glConfig.vidWidth, glConfig.vidHeight, 0, format, type, NULL);
		SetDebugName(GL_TEXTURE, fb->color, va("%s color attachment 0", name));
	}
	
	glGenFramebuffers(1, &fb->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);
	if(color)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->color, 0);
	}
	if(depthStencil)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, fb->depthStencil, 0);
	}	

	const GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(fboStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		ri.Error(ERR_FATAL, "Failed to create FBO (status 0x%X, error 0x%X)\n", (unsigned int)fboStatus, (unsigned int)glGetError());
	}

	SetDebugName(GL_FRAMEBUFFER, fb->fbo, va("%s frame buffer", name));

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	fb->multiSampled = qfalse;
	fb->hasDepthStencil = depthStencil;
	fb->hasColor = color;
}

static void FBO_CreateMS(int* sampleCount, FrameBuffer* fb, const char* name)
{
	glGenFramebuffers(1, &fb->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

	glGenTextures(1, &fb->color);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fb->color);
	CreateColorTextureStorageMS(sampleCount);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, fb->color, 0);
	SetDebugName(GL_TEXTURE, fb->color, va("%s color attachment 0", name));

	glGenTextures(1, &fb->depthStencil);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fb->depthStencil);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, *sampleCount, GL_DEPTH24_STENCIL8, glConfig.vidWidth, glConfig.vidHeight, GL_TRUE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, fb->depthStencil, 0);
	SetDebugName(GL_TEXTURE, fb->depthStencil, va("%s depth/stencil attachment", name));

	const GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(fboStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		ri.Error(ERR_FATAL, "Failed to create FBO (status 0x%X, error 0x%X)\n", (unsigned int)fboStatus, (unsigned int)glGetError());
	}

	SetDebugName(GL_FRAMEBUFFER, fb->fbo, va("%s frame buffer", name));

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	fb->multiSampled = qtrue;
	fb->hasDepthStencil = qtrue;
	fb->hasColor = qtrue;
}

static void FBO_Init()
{
	gl.fbMSEnabled = r_msaa->integer >= 2 && r_colorMipLevels->integer == 0;
	int finalSampleCount = 1;

	if(gl.fbMSEnabled)
	{
		FBO_CreateMS(&finalSampleCount, &gl.fbMS, "main");
		FBO_CreateSS(&gl.fbSS[0], qtrue, qfalse, "post-process #1");
		FBO_CreateSS(&gl.fbSS[1], qtrue, qfalse, "post-process #2");
	}
	else
	{
		FBO_CreateSS(&gl.fbSS[0], qtrue, qtrue, "post-process #1");
		FBO_CreateSS(&gl.fbSS[1], qtrue, qtrue, "post-process #2");
	}

	glInfo.msaaSampleCount = finalSampleCount;
}

static void FBO_Bind(const FrameBuffer* fb)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
}

static void FBO_Bind()
{
	if(gl.fbMSEnabled)
	{
		FBO_Bind(&gl.fbMS);
	}
	else
	{
		FBO_Bind(&gl.fbSS[gl.fbReadIndex]);
	}
}

static void FBO_BlitToBackBuffer()
{
	// fixing up the blit mode here to avoid unnecessary glClear calls
	int blitMode = r_blitMode->integer;
	if(r_mode->integer != VIDEOMODE_UPSCALE)
	{
		blitMode = BLITMODE_STRETCHED;
	}

	if(blitMode != BLITMODE_STRETCHED)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	const FrameBuffer& fbo = gl.fbSS[gl.fbReadIndex];
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_BACK);

	const int sw = glConfig.vidWidth;
	const int sh = glConfig.vidHeight;
	const int dw = glInfo.winWidth;
	const int dh = glInfo.winHeight;
	if(blitMode == BLITMODE_STRETCHED)
	{
		glBlitFramebuffer(0, 0, sw, sh, 0, 0, dw, dh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
	else if(blitMode == BLITMODE_CENTERED)
	{
		const int dx = (dw - sw) / 2;
		const int dy = (dh - sh) / 2;
		glBlitFramebuffer(0, 0, sw, sh, dx, dy, dx + sw, dy + sh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
	else // blitMode == BLITMODE_ASPECT
	{
		const float rx = (float)dw / (float)sw;
		const float ry = (float)dh / (float)sh;
		const float ar = min(rx, ry);
		const int w = (int)(sw * ar);
		const int h = (int)(sh * ar);
		const int x = (dw - w) / 2;
		const int y = (dh - h) / 2;
		glBlitFramebuffer(0, 0, sw, sh, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
}

static void FBO_ResolveColor()
{
	const FrameBuffer& r = gl.fbMS;
	const FrameBuffer& d = gl.fbSS[gl.fbReadIndex];
	glBindFramebuffer(GL_READ_FRAMEBUFFER, r.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d.fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;
	glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

static void ApplyActiveTexture(int slot)
{
	if(slot == gl.activeTextureSlot)
	{
		return;
	}

	glActiveTexture(GL_TEXTURE0 + slot);
	gl.activeTextureSlot = slot;
}

static void ApplyPipeline(PipelineId pipelineId)
{
	if(pipelineId == gl.pipelineId)
	{
		return;
	}

	// The depth fade pipeline is the only one reading from the depth texture
	// but doesn't write to it.
	// Any change to that pipeline requires a texture barrier with OpenGL 4.5+
	// to make sure we get valid data when reading the depth texture.
	// See "Feedback Loops Between Textures and the Framebuffer" in the specs.
	if((GLEW_VERSION_4_5 || GLEW_ARB_texture_barrier) &&
	   pipelineId == PID_SOFT_SPRITE)
	{
		glTextureBarrier();
	}

	gl.pipelineId = pipelineId;

	Pipeline* const pipeline = &gl.pipelines[pipelineId];
	glUseProgram(pipeline->program.program);
	backEnd.pc3D[RB_SHADER_CHANGES]++;

	for(int i = 0; i < VB_COUNT; ++i)
	{
		if(pipeline->arrayBuffers[i].enabled)
		{
			ArrayBuffer* const buffer = &gl.arrayBuffers[i];
			glEnableVertexAttribArray(i);
			glBindBuffer(GL_ARRAY_BUFFER, buffer->buffer);
			glVertexAttribPointer(i, buffer->componentCount, buffer->dataType, buffer->normalized, buffer->itemSize, (const void*)0);
		}
		else
		{
			glDisableVertexAttribArray(i);
		}
	}

	glUniform1i(pipeline->textureLocations[0], 0);
	ApplyActiveTexture(1);
	if(pipelineId == PID_SOFT_SPRITE && gl.fbMSEnabled)
	{
		// we don't have a "BindTextureMS" function for caching/tracking MS texture binds
		// since this is the only one we read from a fragment shader at the moment
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, gl.fbMS.depthStencil);
	}
	glUniform1i(pipeline->textureLocations[1], 1);
	ApplyActiveTexture(0);

	memset(pipeline->uniformsDirty, 0xFF, sizeof(pipeline->uniformsDirty));
}

static GLint GetTextureWrapMode(textureWrap_t w)
{
	switch(w)
	{
		case TW_REPEAT: return GL_REPEAT;
		case TW_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
		default: return GL_REPEAT;
	}
}

static GLint GetTextureInternalFormat(textureFormat_t f)
{
	switch(f)
	{
		case TF_RGBA8:
		default: return GL_RGBA8;
	}
}

static GLenum GetTextureFormat(textureFormat_t f)
{
	switch(f)
	{
		case TF_RGBA8:
		default: return GL_RGBA;
	}
}

static void BindTexture(int slot, GLuint texture)
{
	if(texture == gl.boundTextures[slot])
	{
		return;
	}

	ApplyActiveTexture(slot);
	glBindTexture(GL_TEXTURE_2D, texture);
	gl.boundTextures[slot] = texture;
}

static void BindImage(int slot, const image_t* image)
{
	const GLuint texture = (GLuint)image->texnum;
	BindTexture(slot, texture);
}

static void UpdateAnimatedImage(image_t* image, int w, int h, const byte* data, qbool dirty)
{
	glBindTexture(GL_TEXTURE_2D, (GLuint)image->texnum);
	if(w != image->width || h != image->height)
	{
		// if the scratchImage isn't in the format we want, specify it as a new texture
		image->width = w;
		image->height = h;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	else if(dirty)
	{
		// otherwise, just update it
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}
}

static const image_t* GetBundleImage(const textureBundle_t* bundle)
{
	return R_UpdateAndGetBundleImage(bundle, &UpdateAnimatedImage);
}

static void BindBundle(int slot, const textureBundle_t* bundle)
{
	BindImage(slot, GetBundleImage(bundle));
}

static void ApplyViewportAndScissor(int x, int y, int w, int h)
{
	glViewport(x, y, w, h);
	glScissor(x, y, w, h);
}

static GLenum GetSourceBlend(unsigned int bits)
{
	switch(bits)
	{
		case GLS_SRCBLEND_ZERO: return GL_ZERO;
		case GLS_SRCBLEND_ONE: return GL_ONE;
		case GLS_SRCBLEND_DST_COLOR: return GL_DST_COLOR;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
		case GLS_SRCBLEND_SRC_ALPHA: return GL_SRC_ALPHA;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
		case GLS_SRCBLEND_DST_ALPHA: return GL_DST_ALPHA;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
		case GLS_SRCBLEND_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
		default: return GL_ONE;
	}
}

static GLenum GetDestinationBlend(unsigned int bits)
{
	switch(bits)
	{
		case GLS_DSTBLEND_ZERO: return GL_ZERO;
		case GLS_DSTBLEND_ONE: return GL_ONE;
		case GLS_DSTBLEND_SRC_COLOR: return GL_SRC_COLOR;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
		case GLS_DSTBLEND_SRC_ALPHA: return GL_SRC_ALPHA;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
		case GLS_DSTBLEND_DST_ALPHA: return GL_DST_ALPHA;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
		default: return GL_ONE;
	}
}

static AlphaTest GetAlphaTest(unsigned int bits)
{
	switch(bits)
	{
		case 0: return AT_ALWAYS;
		case GLS_ATEST_GT_0: return AT_GREATER_THAN_0;
		case GLS_ATEST_LT_80: return AT_LESS_THAN_HALF;
		case GLS_ATEST_GE_80: return AT_GREATER_OR_EQUAL_TO_HALF;
		default: return AT_ALWAYS;
	}
}

static void ApplyCullType(cullType_t cullType)
{
	if(cullType == gl.cullType)
	{
		return;
	}

	gl.cullType = cullType;
	if(cullType == CT_TWO_SIDED)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(cullType == CT_FRONT_SIDED ? GL_FRONT : GL_BACK);
	}
}

static void ApplyBlendFunc(unsigned int srcBlendBits, unsigned int dstBlendBits)
{
	if(srcBlendBits == gl.srcBlendBits && dstBlendBits == gl.dstBlendBits)
	{
		return;
	}

	gl.srcBlendBits = srcBlendBits;
	gl.dstBlendBits = dstBlendBits;
	if((srcBlendBits | dstBlendBits) == 0)
	{
		glDisable(GL_BLEND);
	}
	else
	{
		glEnable(GL_BLEND);
		glBlendFunc(GetSourceBlend(srcBlendBits), GetDestinationBlend(dstBlendBits));
	}
}

static void ApplyDepthTest(qbool enableDepthTest)
{
	if(enableDepthTest == gl.enableDepthTest)
	{
		return;
	}

	gl.enableDepthTest = enableDepthTest;
	if(enableDepthTest)
	{
		glEnable(GL_DEPTH_TEST);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
	}
}

static void ApplyDepthFunc(GLenum depthFunc)
{
	if(depthFunc == gl.depthFunc)
	{
		return;
	}

	gl.depthFunc = depthFunc;
	glDepthFunc(depthFunc);
}

static void ApplyDepthMask(GLboolean enableDepthWrite)
{
	if(enableDepthWrite == gl.enableDepthWrite)
	{
		return;
	}

	gl.enableDepthWrite = enableDepthWrite;
	glDepthMask(enableDepthWrite ? GL_TRUE : GL_FALSE);
}

static void ApplyPolygonMode(GLenum polygonMode)
{
	if(polygonMode == gl.polygonMode)
	{
		return;
	}

	gl.polygonMode = polygonMode;
	glPolygonMode(GL_FRONT_AND_BACK, polygonMode);
}

static void ApplyPolygonOffset(qbool enablePolygonOffset)
{
	if(enablePolygonOffset == gl.enablePolygonOffset)
	{
		return;
	}

	gl.enablePolygonOffset = enablePolygonOffset;
	if(enablePolygonOffset)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
	}
	else
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
}

static void ApplyClipPlane(qbool enableClipPlane)
{
	if(enableClipPlane == gl.enableClipPlane)
	{
		return;
	}

	gl.enableClipPlane = enableClipPlane;
	if(enableClipPlane)
	{
		glEnable(GL_CLIP_DISTANCE0);
	}
	else
	{
		glDisable(GL_CLIP_DISTANCE0);
	}
}

static void ApplyAlphaTest(AlphaTest alphaTest)
{
	const qbool enableA2C = glInfo.alphaToCoverageSupport && gl.pipelineId == PID_GENERIC && alphaTest != AT_ALWAYS;
	if(enableA2C != gl.enableAlphaToCoverage)
	{
		gl.enableAlphaToCoverage = enableA2C;
		if(enableA2C)
		{
			glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}
		else
		{
			glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}
	}

	if(alphaTest == gl.alphaTest)
	{
		return;
	}
	gl.alphaTest = alphaTest;

	if(gl.pipelineId == PID_GENERIC)
	{
		gl.pipelines[PID_GENERIC].uniformsDirty[GU_ALPHA_TEX] = qtrue;
	}
	else if(gl.pipelineId == PID_SOFT_SPRITE)
	{
		gl.pipelines[PID_SOFT_SPRITE].uniformsDirty[SU_ALPHA_TEST] = qtrue;
	}
}

static void ApplyState(unsigned int stateBits, cullType_t cullType, qbool polygonOffset)
{
	// fix up the cull mode for mirrors
	if(backEnd.viewParms.isMirror)
	{
		if(cullType == CT_BACK_SIDED)
		{
			cullType = CT_FRONT_SIDED;
		}
		else if(cullType == CT_FRONT_SIDED)
		{
			cullType = CT_BACK_SIDED;
		}
	}
	ApplyCullType(cullType);

	const unsigned int srcBlendBits = stateBits & GLS_SRCBLEND_BITS;
	const unsigned int dstBlendBits = stateBits & GLS_DSTBLEND_BITS;
	ApplyBlendFunc(srcBlendBits, dstBlendBits);

	const qbool disableDepthTest = ((stateBits & GLS_DEPTHTEST_DISABLE) != 0) || backEnd.projection2D;
	ApplyDepthTest(!disableDepthTest);

	const qbool depthFuncEqual = (stateBits & GLS_DEPTHFUNC_EQUAL) != 0;
	ApplyDepthFunc(depthFuncEqual ? GL_EQUAL : GL_LEQUAL);

	const qbool enableDepthWrite = (stateBits & GLS_DEPTHMASK_TRUE) != 0 && gl.pipelineId != PID_SOFT_SPRITE;
	ApplyDepthMask(enableDepthWrite ? GL_TRUE : GL_FALSE);

	const qbool wireFrame = (stateBits & GLS_POLYMODE_LINE) ? 1 : 0;
	ApplyPolygonMode(wireFrame ? GL_LINE : GL_FILL);

	ApplyPolygonOffset(polygonOffset);

	ApplyAlphaTest(GetAlphaTest(stateBits & GLS_ATEST_BITS));
}

static void ApplyTexEnv(texEnv_t texEnv)
{
	if(gl.pipelineId == PID_GENERIC && texEnv != gl.texEnv)
	{
		gl.pipelines[PID_GENERIC].uniformsDirty[GU_ALPHA_TEX] = qtrue;
	}
	gl.texEnv = texEnv;
}

static void BindVertexArray(VertexBufferId)
{
}

static void Buffer_WaitForRange(ArrayBuffer* buffer)
{
	buffer->writeIndex = buffer->writeRangeIndex * (buffer->capacity / LARGEBUFFER_MAX_FRAMES);

	GLsync& fence = buffer->fences[buffer->writeRangeIndex];
	if(fence == NULL)
	{
		return;
	}

	GLbitfield waitFlags = 0;
	GLuint64 waitDurationNS = 0;
	for(;;)
	{
		GLenum waitRet = glClientWaitSync(fence, waitFlags, waitDurationNS);
		if(waitRet == GL_ALREADY_SIGNALED || waitRet == GL_CONDITION_SATISFIED)
		{
			glDeleteSync(fence);
			fence = NULL;
			return;
		}

		if(waitRet == GL_WAIT_FAILED)
		{
			ri.Error(ERR_FATAL, "glClientWaitSync failed with GL_WAIT_FAILED\n");
		}

		// after the first time, we need to start flushing and wait as long as necessary
		waitFlags = GL_SYNC_FLUSH_COMMANDS_BIT;
		waitDurationNS = 1e9;
	}
}

static void Buffer_LockRange(ArrayBuffer* buffer)
{
	GLsync& fence = buffer->fences[buffer->writeRangeIndex];
	assert(fence == NULL);
	if(fence == NULL)
	{
		fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		assert(fence != NULL);
	}

	buffer->writeRangeIndex = (buffer->writeRangeIndex + 1) % LARGEBUFFER_MAX_FRAMES;
	buffer->writeIndex = buffer->writeRangeIndex * (buffer->capacity / LARGEBUFFER_MAX_FRAMES);
}

static void Buffers_Wait()
{
	for(int i = 0; i < VB_COUNT; ++i)
	{
		Buffer_WaitForRange(&gl.arrayBuffers[i]);
	}

	Buffer_WaitForRange(&gl.indexBuffer);
}

static void Buffers_Lock()
{
	for(int i = 0; i < VB_COUNT; ++i)
	{
		Buffer_LockRange(&gl.arrayBuffers[i]);
	}

	Buffer_LockRange(&gl.indexBuffer);
}

// if qtrue, we have a large buffer for multiple frames and use fences for synchronization
static qbool MappingType_UsesLargeBuffers()
{
	return gl.mappingType == MT_PERSISTENT || gl.mappingType == MT_UNSYNC || gl.mappingType == MT_AMDPIN;
}

static void UploadGeometry(ArrayBuffer* buffer, const void* data, int itemCount)
{
	const GLenum target = buffer->indexBuffer ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
	if(MappingType_UsesLargeBuffers())
	{
		const int rangeLength = buffer->capacity / LARGEBUFFER_MAX_FRAMES;
		const int endRangeIndex = (buffer->writeIndex + itemCount - 1) / rangeLength;
#if defined(_DEBUG)
		assert(endRangeIndex == buffer->writeRangeIndex ||
			   endRangeIndex == buffer->writeRangeIndex + 1 ||
			   (endRangeIndex == 0 && buffer->writeRangeIndex == LARGEBUFFER_MAX_FRAMES - 1));
		const int startRangeIndex = buffer->writeIndex == 0 ? 0 : ((buffer->writeIndex - 1) / rangeLength);
		assert(startRangeIndex == buffer->writeRangeIndex ||
			   startRangeIndex == (buffer->writeRangeIndex + LARGEBUFFER_MAX_FRAMES - 1) % LARGEBUFFER_MAX_FRAMES);
#endif
		if(endRangeIndex == buffer->writeRangeIndex + 1)
		{
			Buffer_LockRange(buffer);
			Buffer_WaitForRange(buffer);
		}

		void* mappedData = NULL;
		if(gl.mappingType == MT_UNSYNC)
		{
			mappedData = glMapBufferRange(target, buffer->writeIndex * buffer->itemSize, itemCount * buffer->itemSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
			if(mappedData == NULL)
			{
				ri.Error(ERR_FATAL, "Couldn't map buffer\n");
			}
		}
		else
		{
			mappedData = buffer->mappedData + buffer->writeIndex * buffer->itemSize;
		}
		memcpy(mappedData, data, itemCount * buffer->itemSize);
		if(gl.mappingType == MT_UNSYNC)
		{
			glUnmapBuffer(target);
		}

		buffer->readIndex = buffer->writeIndex;
		buffer->writeIndex += itemCount;
	}
	else
	{
		glBufferSubData(target, (GLintptr)0, itemCount * buffer->itemSize, data);
		buffer->readIndex = 0;
	}
}

static void UploadVertexArray(VertexBufferId vbid, const void* data)
{
	ArrayBuffer* buffer = &gl.arrayBuffers[vbid];

	glBindBuffer(GL_ARRAY_BUFFER, buffer->buffer);
	UploadGeometry(buffer, data, tess.numVertexes);
	if(MappingType_UsesLargeBuffers())
	{
		glVertexAttribPointer(vbid, buffer->componentCount, buffer->dataType, buffer->normalized, buffer->itemSize, (const GLvoid*)(GLintptr)(buffer->readIndex * buffer->itemSize));
	}
}

static void UploadIndices(const void* data, int indexCount)
{
	ArrayBuffer* buffer = &gl.indexBuffer;

	// @NOTE: we only have 1 index buffer and it's already bound
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->buffer);
	UploadGeometry(buffer, data, indexCount);
}

static void CreateGeometryBufferStorage(ArrayBuffer* buffer)
{
	const GLenum target = buffer->indexBuffer ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
	if(gl.mappingType == MT_PERSISTENT)
	{
		glGenBuffers(1, &buffer->buffer);
		glBindBuffer(target, buffer->buffer);
		const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glBufferStorage(target, buffer->capacity * buffer->itemSize, NULL, flags);
		buffer->mappedData = (byte*)glMapBufferRange(target, 0, buffer->capacity * buffer->itemSize, flags);
		if(buffer->mappedData == NULL)
		{
			ri.Error(ERR_FATAL, "Couldn't map buffer storage\n");
		}
	}
	else if(gl.mappingType == MT_AMDPIN)
	{
		while(glGetError() != GL_NO_ERROR) {} // clear the error queue
		GLenum errorCode = GL_NO_ERROR;

		AllocatePinnedMemory(buffer);
		if(buffer->mappedData == NULL)
		{
			ri.Error(ERR_FATAL, "Couldn't allocate buffer storage\n");
		}
		glGenBuffers(1, &buffer->buffer);
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, buffer->buffer);
		if((errorCode = glGetError()) != GL_NO_ERROR)
		{
			ri.Error(ERR_FATAL, "glBindBuffer GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD failed with error code: 0x%08X\n", (unsigned int)errorCode);
		}
		glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, buffer->pinnedByteCount, buffer->mappedData, GL_DYNAMIC_DRAW);
		if((errorCode = glGetError()) != GL_NO_ERROR)
		{
			ri.Error(ERR_FATAL, "glBufferData GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD failed with error code: 0x%08X\n", (unsigned int)errorCode);
		}
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, 0);
		glBindBuffer(target, buffer->buffer);
	}
	else
	{
		glGenBuffers(1, &buffer->buffer);
		glBindBuffer(target, buffer->buffer);
		glBufferData(target, buffer->capacity * buffer->itemSize, NULL, GL_DYNAMIC_DRAW);
	}
}

static void DrawElements(int indexCount)
{
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (const GLvoid*)(GLintptr)(gl.indexBuffer.readIndex * gl.indexBuffer.itemSize));
	backEnd.pc3D[RB_DRAW_CALLS]++;
}

static void SetDefaultState()
{
	glViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	glScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glCullFace(GL_FRONT);
	glPolygonOffset(-1.0f, -1.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_CLIP_DISTANCE0);
	glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	if(r_depthClamp->integer)
	{
		glEnable(GL_DEPTH_CLAMP);
	}
	else
	{
		glDisable(GL_DEPTH_CLAMP);
	}

	gl.boundTextures[0] = GLuint(-1);
	gl.boundTextures[1] = GLuint(-1);
	gl.activeTextureSlot = 0;
	gl.cullType = CT_TWO_SIDED;
	gl.srcBlendBits = GLS_SRCBLEND_SRC_ALPHA;
	gl.dstBlendBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	gl.enableDepthTest = qtrue;
	gl.depthFunc = GL_LEQUAL;
	gl.enableDepthWrite = GL_FALSE;
	gl.polygonMode = GL_FILL;
	gl.enablePolygonOffset = qfalse;
	gl.enableClipPlane = qfalse;
	gl.enableAlphaToCoverage = qfalse;
}

static qbool InitCompute()
{
	while(glGetError() != GL_NO_ERROR) {} // clear the error queue

	glGenTextures(ARRAY_LEN(gl.mipGen.textures), gl.mipGen.textures);
	glBindTexture(GL_TEXTURE_2D, gl.mipGen.textures[0]);
	SetDebugName(GL_TEXTURE, gl.mipGen.textures[0], "mip-gen float16 texture #1");
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, gl.maxTextureSize, gl.maxTextureSize);
	glBindTexture(GL_TEXTURE_2D, gl.mipGen.textures[1]);
	SetDebugName(GL_TEXTURE, gl.mipGen.textures[1], "mip-gen float16 texture #2");
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, gl.maxTextureSize, gl.maxTextureSize);
	glBindTexture(GL_TEXTURE_2D, gl.mipGen.textures[2]);
	SetDebugName(GL_TEXTURE, gl.mipGen.textures[2], "mip-gen uint8 texture");
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, gl.maxTextureSize, gl.maxTextureSize);

	if(glGetError() != GL_NO_ERROR)
	{
		HandleError("Failed to allocate storage for the mip-map generation textures\n");
		return qfalse;
	}

	if(!CreateComputeProgram(&gl.mipGen.programs[CPID_GAMMA_TO_LINEAR], gammaToLinear_cs, "gamma to linear") ||
	   !CreateComputeProgram(&gl.mipGen.programs[CPID_LINEAR_TO_GAMMA], linearToGamma_cs, "linear to gamma") ||
	   !CreateComputeProgram(&gl.mipGen.programs[CPID_DOWN_SAMPLE], downSample_cs, "down sample"))
	{
		HandleError("Failed to compile compute shaders for GPU-side mip-map generation\n");
		return qfalse;
	}

	return qtrue;
}

static MappingType GetMappingTypeFromCvar()
{
	const int mode = r_gl3_geoStream->integer;
	if(mode == GL3MAP_SUBDATA)
	{
		return MT_SUBDATA;
	}

	if(mode == GL3MAP_MAPUNSYNC)
	{
		return MT_UNSYNC;
	}

	if(mode == GL3MAP_AMDPIN && GLEW_AMD_pinned_memory)
	{
		return MT_AMDPIN;
	}

	if((mode == GL3MAP_AUTO || mode == GL3MAP_MAPPERS) && (GLEW_VERSION_4_4 || GLEW_ARB_buffer_storage))
	{
		return MT_PERSISTENT;
	}

	if(GLEW_AMD_pinned_memory)
	{
		return MT_AMDPIN;
	}

	if(strstr((const char*)glGetString(GL_RENDERER), "Intel") != NULL)
	{
		return MT_UNSYNC;
	}

	return MT_SUBDATA;
}

static void InitQueries()
{
	glGenQueries(ARRAY_LEN(gl.timerQueries), &gl.timerQueries[0]);
}

static void BeginQueries()
{
	glBeginQuery(GL_TIME_ELAPSED, gl.timerQueries[gl.queryWriteIndex]);
	gl.queryStarted[gl.queryWriteIndex] = qtrue;
}

static void EndQueries()
{
	// finish this frame
	glEndQuery(GL_TIME_ELAPSED);
	gl.queryWriteIndex = (gl.queryWriteIndex + 1) % ARRAY_LEN(gl.timerQueries);

	// try to grab a previous frame's result
	if(gl.queryStarted[gl.queryReadIndex])
	{
		const GLuint query = gl.timerQueries[gl.queryReadIndex];
		backEnd.pc3D[RB_USEC_GPU] = 0;
		GLint done = GL_FALSE;
		glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &done);
		if(done != GL_FALSE)
		{
			GLint durationNS = 0;
			glGetQueryObjectiv(query, GL_QUERY_RESULT, &durationNS);
			if(durationNS > 0)
			{
				backEnd.pc3D[RB_USEC_GPU] = durationNS / 1000;
			}
			gl.queryReadIndex = (gl.queryReadIndex + 1) % ARRAY_LEN(gl.timerQueries);
		}
	}
}

static void Init()
{
	memset(&gl, 0, sizeof(gl));

	GLint maxTextureSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	gl.maxTextureSize = maxTextureSize > 0 ? min((int)maxTextureSize, MAX_GPU_TEXTURE_SIZE) : MAX_GPU_TEXTURE_SIZE;
	glConfig.unused_maxTextureSize = gl.maxTextureSize;
	glInfo.maxTextureSize = gl.maxTextureSize;
	glInfo.depthFadeSupport = r_depthFade->integer == 1;

	FBO_Init();
	if(gl.fbMSEnabled && r_alphaToCoverage->integer)
	{
		glInfo.alphaToCoverageSupport = qtrue;
	}

	int maxVertexCount = SHADER_MAX_VERTEXES;
	int maxIndexCount = SHADER_MAX_INDEXES;
	gl.mappingType = GetMappingTypeFromCvar();
	if(MappingType_UsesLargeBuffers())
	{
		maxVertexCount = LARGEBUFFER_MAX_VERTEXES * LARGEBUFFER_MAX_FRAMES;
		maxIndexCount = LARGEBUFFER_MAX_INDEXES * LARGEBUFFER_MAX_FRAMES;
	}

	gl.arrayBuffers[VB_POSITION].capacity = maxVertexCount;
	gl.arrayBuffers[VB_POSITION].itemSize = sizeof(tess.xyz[0]);
	gl.arrayBuffers[VB_POSITION].componentCount = 4;
	gl.arrayBuffers[VB_POSITION].dataType = GL_FLOAT;
	gl.arrayBuffers[VB_POSITION].normalized = GL_FALSE;
	gl.arrayBuffers[VB_NORMAL].capacity = maxVertexCount;
	gl.arrayBuffers[VB_NORMAL].itemSize = sizeof(tess.normal[0]);
	gl.arrayBuffers[VB_NORMAL].componentCount = 4;
	gl.arrayBuffers[VB_NORMAL].dataType = GL_FLOAT;
	gl.arrayBuffers[VB_NORMAL].normalized = GL_FALSE;
	gl.arrayBuffers[VB_TEXCOORD].capacity = maxVertexCount;
	gl.arrayBuffers[VB_TEXCOORD].itemSize = sizeof(tess.svars[0].texcoords[0]);
	gl.arrayBuffers[VB_TEXCOORD].componentCount = 2;
	gl.arrayBuffers[VB_TEXCOORD].dataType = GL_FLOAT;
	gl.arrayBuffers[VB_TEXCOORD].normalized = GL_FALSE;
	gl.arrayBuffers[VB_TEXCOORD2].capacity = maxVertexCount;
	gl.arrayBuffers[VB_TEXCOORD2].itemSize = sizeof(tess.svars[0].texcoords[0]);
	gl.arrayBuffers[VB_TEXCOORD2].componentCount = 2;
	gl.arrayBuffers[VB_TEXCOORD2].dataType = GL_FLOAT;
	gl.arrayBuffers[VB_TEXCOORD2].normalized = GL_FALSE;
	gl.arrayBuffers[VB_COLOR].capacity = maxVertexCount;
	gl.arrayBuffers[VB_COLOR].itemSize = sizeof(tess.svars[0].colors[0]);
	gl.arrayBuffers[VB_COLOR].componentCount = 4;
	gl.arrayBuffers[VB_COLOR].dataType = GL_UNSIGNED_BYTE;
	gl.arrayBuffers[VB_COLOR].normalized = GL_TRUE;
	gl.indexBuffer.capacity = maxIndexCount;
	gl.indexBuffer.itemSize = sizeof(tess.indexes[0]);
	gl.indexBuffer.indexBuffer = qtrue;

	gl.pipelines[PID_GENERIC].arrayBuffers[VB_POSITION].enabled = qtrue;
	gl.pipelines[PID_GENERIC].arrayBuffers[VB_POSITION].attribName = "position";
	gl.pipelines[PID_GENERIC].arrayBuffers[VB_TEXCOORD].enabled = qtrue;
	gl.pipelines[PID_GENERIC].arrayBuffers[VB_TEXCOORD].attribName = "texCoords1";
	gl.pipelines[PID_GENERIC].arrayBuffers[VB_TEXCOORD2].enabled = qtrue;
	gl.pipelines[PID_GENERIC].arrayBuffers[VB_TEXCOORD2].attribName = "texCoords2";
	gl.pipelines[PID_GENERIC].arrayBuffers[VB_COLOR].enabled = qtrue;
	gl.pipelines[PID_GENERIC].arrayBuffers[VB_COLOR].attribName = "color";
	gl.pipelines[PID_GENERIC].uniformNames[GU_MODELVIEW] = "modelView";
	gl.pipelines[PID_GENERIC].uniformNames[GU_PROJECTION] = "projection";
	gl.pipelines[PID_GENERIC].uniformNames[GU_CLIP_PLANE] = "clipPlane";
	gl.pipelines[PID_GENERIC].uniformNames[GU_ALPHA_TEX] = "alphaTex";
	gl.pipelines[PID_GENERIC].uniformNames[GU_GREYSCALE] = "greyscale";
	gl.pipelines[PID_GENERIC].uniformNames[GU_GAMMA_BRIGHT_NOISE_SEED] = "gammaBrightNoiseSeed";
	gl.pipelines[PID_GENERIC].uniformNames[GU_A2C_ALPHA_BOOST] = "alphaBoost";

	gl.pipelines[PID_DYNAMIC_LIGHT].arrayBuffers[VB_POSITION].enabled = qtrue;
	gl.pipelines[PID_DYNAMIC_LIGHT].arrayBuffers[VB_POSITION].attribName = "position";
	gl.pipelines[PID_DYNAMIC_LIGHT].arrayBuffers[VB_NORMAL].enabled = qtrue;
	gl.pipelines[PID_DYNAMIC_LIGHT].arrayBuffers[VB_NORMAL].attribName = "normal";
	gl.pipelines[PID_DYNAMIC_LIGHT].arrayBuffers[VB_TEXCOORD].enabled = qtrue;
	gl.pipelines[PID_DYNAMIC_LIGHT].arrayBuffers[VB_TEXCOORD].attribName = "texCoords1";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_MODELVIEW] = "modelView";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_PROJECTION] = "projection";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_CLIP_PLANE] = "clipPlane";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_LIGHT_POS] = "osLightPos";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_EYE_POS] = "osEyePos";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_LIGHT_COLOR_RADIUS] = "lightColorRadius";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_OPAQUE] = "opaque";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_INTENSITY] = "intensity";
	gl.pipelines[PID_DYNAMIC_LIGHT].uniformNames[DU_GREYSCALE] = "greyscale";

	gl.pipelines[PID_SOFT_SPRITE].arrayBuffers[VB_POSITION].enabled = qtrue;
	gl.pipelines[PID_SOFT_SPRITE].arrayBuffers[VB_POSITION].attribName = "position";
	gl.pipelines[PID_SOFT_SPRITE].arrayBuffers[VB_TEXCOORD].enabled = qtrue;
	gl.pipelines[PID_SOFT_SPRITE].arrayBuffers[VB_TEXCOORD].attribName = "texCoords1";
	gl.pipelines[PID_SOFT_SPRITE].arrayBuffers[VB_COLOR].enabled = qtrue;
	gl.pipelines[PID_SOFT_SPRITE].arrayBuffers[VB_COLOR].attribName = "color";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_MODELVIEW] = "modelView";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_PROJECTION] = "projection";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_CLIP_PLANE] = "clipPlane";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_ALPHA_TEST] = "alphaTest";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_DIST_OFFSET] = "distOffset";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_COLOR_SCALE] = "colorScale";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_COLOR_BIAS] = "colorBias";
	gl.pipelines[PID_SOFT_SPRITE].uniformNames[SU_GREYSCALE] = "greyscale";

	gl.pipelines[PID_POST_PROCESS].uniformNames[PU_BRIGHT_GAMMA_GREY] = "brightGammaGrey";

	CreateGraphicsProgram(PID_GENERIC, generic_vs, generic_fs, "generic");
	CreateGraphicsProgram(PID_DYNAMIC_LIGHT, dl_vs, dl_fs, "dynamic light");
	CreateGraphicsProgram(PID_SOFT_SPRITE, sprite_vs, sprite_fs, "soft sprite");
	CreateGraphicsProgram(PID_POST_PROCESS, post_vs, post_fs, "post-process");

	GLuint vertexArray;
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	CreateGeometryBufferStorage(&gl.indexBuffer);
	for(int i = 0; i < VB_COUNT; ++i)
	{
		CreateGeometryBufferStorage(&gl.arrayBuffers[i]);
	}

	for(int p = 0; p < PID_COUNT; ++p)
	{
		Pipeline* pipeline = &gl.pipelines[p];

		pipeline->textureLocations[0] = glGetUniformLocation(pipeline->program.program, "texture1");
		pipeline->textureLocations[1] = glGetUniformLocation(pipeline->program.program, "texture2");

		for(int i = 0; i < ARRAY_LEN(pipeline->uniformLocations); ++i)
		{
			if(pipeline->uniformNames[i] != NULL)
			{
				pipeline->uniformLocations[i] = glGetUniformLocation(pipeline->program.program, pipeline->uniformNames[i]);
#if defined(_DEBUG)
				if((p == PID_GENERIC && i == GU_GAMMA_BRIGHT_NOISE_SEED && r_dither->integer == 0) ||
				   (p == PID_GENERIC && i == GU_A2C_ALPHA_BOOST && !glInfo.alphaToCoverageSupport))
				{
					continue;
				}
				assert(pipeline->uniformLocations[i] != -1);
#endif
			}
		}
	}

	if(r_gpuMipGen->integer && (GLEW_VERSION_4_3 || (GLEW_ARB_compute_shader && GLEW_ARB_texture_storage && GLEW_ARB_shader_image_load_store && GLEW_ARB_copy_image)))
	{
		gl.errorMode = EM_PRINT;
		glInfo.mipGenSupport = InitCompute();
		gl.errorMode = EM_FATAL;
	}

	gl.pipelineId = PID_COUNT;
	ApplyPipeline(PID_GENERIC);

	InitQueries();
}

static void InitGLConfig()
{
	// @NOTE: could use glGetStringi in a loop to grab the extension list, but it's useless either way
	Q_strncpyz(glConfig.vendor_string, (const char*)glGetString(GL_VENDOR), sizeof(glConfig.vendor_string));
	Q_strncpyz(glConfig.renderer_string, (const char*)glGetString(GL_RENDERER), sizeof(glConfig.renderer_string));
	Q_strncpyz(glConfig.version_string, (const char*)glGetString(GL_VERSION), sizeof(glConfig.version_string));
	Q_strncpyz(glConfig.extensions_string, "", sizeof(glConfig.extensions_string));
	glConfig.unused_maxTextureSize = MAX_GPU_TEXTURE_SIZE;
	glConfig.unused_maxActiveTextures = 0;
	glConfig.unused_driverType = 0;		// ICD
	glConfig.unused_hardwareType = 0;	// generic
	glConfig.unused_deviceSupportsGamma = qtrue;
	glConfig.unused_textureCompression = 0;	// no compression
	glConfig.unused_textureEnvAddAvailable = qtrue;
	glConfig.unused_displayFrequency = 0;
	glConfig.unused_isFullscreen = !!r_fullscreen->integer;
	glConfig.unused_stereoEnabled = qfalse;
	glConfig.unused_smpActive = qfalse;
}

static void InitGLInfo()
{
	glInfo.maxTextureSize = MAX_GPU_TEXTURE_SIZE;

	if(GLEW_EXT_texture_filter_anisotropic)
	{
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glInfo.maxAnisotropy);
	}
	else
	{
		glInfo.maxAnisotropy = 0;
	}

	glInfo.depthFadeSupport = qfalse;
	glInfo.mipGenSupport = qfalse;
	glInfo.alphaToCoverageSupport = qfalse;
}

static qbool GAL_Init()
{
	if(glConfig.vidWidth == 0)
	{
		// the order of these calls can not be changed
		Sys_V_Init(GAL_GL3);
		if(!GLEW_VERSION_3_2)
		{
			ri.Error(ERR_FATAL, "OpenGL 3.2 is required by the selected back-end!\n");
		}
		InitGLConfig();
		InitGLInfo();
		Init();

		// apply the current V-Sync option after the first rendered frame
		r_swapInterval->modified = qtrue;
	}

	SetDefaultState();

	const int err = glGetError();
	if(err != GL_NO_ERROR)
	{
		ri.Printf(PRINT_ALL, "glGetError() = 0x%x\n", err);
	}

	return qtrue;
}

static void GAL_ShutDown(qbool fullShutDown)
{
	for(int i = 0; i < tr.numImages; ++i)
	{
		const GLuint texture = (GLuint)tr.images[i]->texnum;
		glDeleteTextures(1, &texture);
	}

	tr.numImages = 0;
	memset(tr.images, 0, sizeof(tr.images));

	gl.boundTextures[0] = GLuint(-1);
	gl.boundTextures[1] = GLuint(-1);

	if(fullShutDown && gl.mappingType == MT_AMDPIN)
	{
		// We flush the command queue and wait for all commands to be done executing
		// to make sure the GPU is done accessing our own memory buffers.
		// We could also have used a fence instead.
		glFlush();
		glFinish();

		// Now that it's safe to do so, free our memory buffers.
		for(int i = 0; i < ARRAY_LEN(gl.arrayBuffers); ++i)
		{
			FreePinnedMemory(&gl.arrayBuffers[i]);
		}
		FreePinnedMemory(&gl.indexBuffer);
	}
}

static void GAL_BeginFrame()
{
	BeginQueries();

	FBO_Bind();

	ApplyViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);

	if(r_clear->integer)
	{
		glClearColor(1.0f, 0.0f, 0.5f, 1.0f);
	}
	else
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(MappingType_UsesLargeBuffers())
	{
		Buffers_Wait();
	}
}

static void GAL_EndFrame()
{
	if(MappingType_UsesLargeBuffers())
	{
		Buffers_Lock();
	}

	if(gl.fbMSEnabled)
	{
		FBO_ResolveColor();
	}

	ApplyPipeline(PID_POST_PROCESS);
	ApplyState(GLS_DEPTHTEST_DISABLE, CT_TWO_SIDED, qfalse);
	ApplyViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	BindTexture(0, gl.fbSS[gl.fbReadIndex].color);
	Pipeline* const pipeline = &gl.pipelines[PID_POST_PROCESS];
	glUniform3f(pipeline->uniformLocations[PU_BRIGHT_GAMMA_GREY], r_brightness->value, 1.0f / r_gamma->value, r_greyscale->value);
	gl.fbReadIndex ^= 1;
	FBO_Bind(&gl.fbSS[gl.fbReadIndex]);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	backEnd.pc3D[RB_DRAW_CALLS]++;

	ApplyViewportAndScissor(0, 0, glInfo.winWidth, glInfo.winHeight);
	FBO_BlitToBackBuffer();

	EndQueries();
}

static void DrawGeneric()
{
	Pipeline* const pipeline = &gl.pipelines[PID_GENERIC];

	if(pipeline->uniformsDirty[GU_MODELVIEW])
	{
		glUniformMatrix4fv(pipeline->uniformLocations[GU_MODELVIEW], 1, GL_FALSE, gl.modelViewMatrix);
		pipeline->uniformsDirty[GU_MODELVIEW] = qfalse;
	}
	if(pipeline->uniformsDirty[GU_PROJECTION])
	{
		glUniformMatrix4fv(pipeline->uniformLocations[GU_PROJECTION], 1, GL_FALSE, gl.projectionMatrix);
		pipeline->uniformsDirty[GU_PROJECTION] = qfalse;
	}
	if(pipeline->uniformsDirty[GU_CLIP_PLANE])
	{
		glUniform4fv(pipeline->uniformLocations[GU_CLIP_PLANE], 1, gl.clipPlane);
		pipeline->uniformsDirty[GU_CLIP_PLANE] = qfalse;
	}
	if(pipeline->uniformsDirty[GU_GAMMA_BRIGHT_NOISE_SEED] &&
	   pipeline->uniformLocations[GU_GAMMA_BRIGHT_NOISE_SEED] != -1)
	{
		glUniform4f(
			pipeline->uniformLocations[GU_GAMMA_BRIGHT_NOISE_SEED],
			1.0f / r_gamma->value,
			1.0f / r_brightness->value,
			backEnd.projection2D ? 0.0f : r_ditherStrength->value,
			(float)rand() / (float)RAND_MAX);
		pipeline->uniformsDirty[GU_GAMMA_BRIGHT_NOISE_SEED] = qfalse;
	}
	if(pipeline->uniformsDirty[GU_A2C_ALPHA_BOOST] &&
	   pipeline->uniformLocations[GU_A2C_ALPHA_BOOST] != -1)
	{
		glUniform1f(pipeline->uniformLocations[GU_A2C_ALPHA_BOOST], r_alphaToCoverageMipBoost->value);
		pipeline->uniformsDirty[GU_A2C_ALPHA_BOOST] = qfalse;
	}
	if(pipeline->uniformsDirty[GU_GREYSCALE] ||
	   tess.greyscale != gl.greyscale)
	{
		glUniform1f(pipeline->uniformLocations[GU_GREYSCALE], tess.greyscale);
		gl.greyscale = tess.greyscale;
		pipeline->uniformsDirty[GU_GREYSCALE] = qfalse;
	}

	UploadVertexArray(VB_POSITION, tess.xyz);
	UploadIndices(tess.indexes, tess.numIndexes);

	for(int i = 0; i < tess.shader->numStages; ++i)
	{
		const shaderStage_t* const stage = tess.xstages[i];
		ApplyState(stage->stateBits, tess.shader->cullType, tess.shader->polygonOffset);

		UploadVertexArray(VB_TEXCOORD, tess.svars[i].texcoordsptr);
		UploadVertexArray(VB_COLOR, tess.svars[i].colors);

		BindBundle(0, &stage->bundle);

		if(stage->mtStages == 0)
		{
			BindImage(1, tr.whiteImage);
			BindVertexArray(VB_TEXCOORD2);
			ApplyTexEnv(TE_DISABLED);
		}
		else
		{
			const shaderStage_t* const stage2 = tess.xstages[i + 1];
			BindBundle(1, &stage2->bundle);
			UploadVertexArray(VB_TEXCOORD2, tess.svars[i + 1].texcoordsptr);
			ApplyTexEnv(stage2->mtEnv);
			++i;
		}

		if(pipeline->uniformsDirty[GU_ALPHA_TEX])
		{
			glUniform2ui(pipeline->uniformLocations[GU_ALPHA_TEX], gl.alphaTest, gl.texEnv);
			pipeline->uniformsDirty[GU_ALPHA_TEX] = qfalse;
		}

		DrawElements(tess.numIndexes);
	}

	if(tess.drawFog)
	{
		ApplyState(tess.fogStateBits, tess.shader->cullType, tess.shader->polygonOffset);

		UploadVertexArray(VB_TEXCOORD, tess.svarsFog.texcoordsptr);
		BindVertexArray(VB_TEXCOORD2);
		UploadVertexArray(VB_COLOR, tess.svarsFog.colors);

		BindImage(0, tr.fogImage);
		BindImage(1, tr.whiteImage);

		ApplyTexEnv(TE_DISABLED);
		if(pipeline->uniformsDirty[GU_ALPHA_TEX])
		{
			glUniform2ui(pipeline->uniformLocations[GU_ALPHA_TEX], gl.alphaTest, gl.texEnv);
			pipeline->uniformsDirty[GU_ALPHA_TEX] = qfalse;
		}

		DrawElements(tess.numIndexes);
	}
}

static void DrawDynamicLight()
{
	Pipeline* const pipeline = &gl.pipelines[PID_DYNAMIC_LIGHT];

	const int stageIndex = tess.shader->lightingStages[ST_DIFFUSE];
	const shaderStage_t* stage = tess.xstages[stageIndex];

	UploadVertexArray(VB_POSITION, tess.xyz);
	UploadVertexArray(VB_NORMAL, tess.normal);
	UploadVertexArray(VB_TEXCOORD, tess.svars[stageIndex].texcoordsptr);
	UploadIndices(tess.dlIndexes, tess.dlNumIndexes);

	ApplyState(backEnd.dlStateBits, tess.shader->cullType, tess.shader->polygonOffset);
	BindBundle(0, &stage->bundle);

	if(backEnd.dlOpaque != gl.dlOpaque)
	{
		gl.dlOpaque = backEnd.dlOpaque;
		pipeline->uniformsDirty[DU_OPAQUE] = qtrue;
	}

	if(backEnd.dlIntensity != gl.dlIntensity)
	{
		gl.dlIntensity = backEnd.dlIntensity;
		pipeline->uniformsDirty[DU_INTENSITY] = qtrue;
	}

	if(tess.greyscale != gl.greyscale)
	{
		gl.greyscale = tess.greyscale;
		pipeline->uniformsDirty[DU_GREYSCALE] = qtrue;
	}

	if(pipeline->uniformsDirty[DU_MODELVIEW])
	{
		glUniformMatrix4fv(pipeline->uniformLocations[DU_MODELVIEW], 1, GL_FALSE, gl.modelViewMatrix);
	}
	if(pipeline->uniformsDirty[DU_PROJECTION])
	{
		glUniformMatrix4fv(pipeline->uniformLocations[DU_PROJECTION], 1, GL_FALSE, gl.projectionMatrix);
	}
	if(pipeline->uniformsDirty[DU_CLIP_PLANE])
	{
		glUniform4fv(pipeline->uniformLocations[DU_CLIP_PLANE], 1, gl.clipPlane);
	}
	if(pipeline->uniformsDirty[DU_OPAQUE])
	{
		glUniform1f(pipeline->uniformLocations[DU_OPAQUE], gl.dlOpaque ? 1.0f : 0.0f);
	}
	if(pipeline->uniformsDirty[DU_INTENSITY])
	{
		glUniform1f(pipeline->uniformLocations[DU_INTENSITY], gl.dlIntensity);
	}
	if(pipeline->uniformsDirty[DU_GREYSCALE])
	{
		glUniform1f(pipeline->uniformLocations[DU_GREYSCALE], tess.greyscale);
	}

	memset(pipeline->uniformsDirty, 0, sizeof(pipeline->uniformsDirty));

	DrawElements(tess.dlNumIndexes);
}

static void DrawDepthFade()
{
	Pipeline* const pipeline = &gl.pipelines[PID_SOFT_SPRITE];

	if(pipeline->uniformsDirty[SU_PROJECTION])
	{
		glUniformMatrix4fv(pipeline->uniformLocations[SU_PROJECTION], 1, GL_FALSE, gl.projectionMatrix);
		pipeline->uniformsDirty[SU_PROJECTION] = qfalse;
	}
	if(pipeline->uniformsDirty[SU_MODELVIEW])
	{
		glUniformMatrix4fv(pipeline->uniformLocations[SU_MODELVIEW], 1, GL_FALSE, gl.modelViewMatrix);
		pipeline->uniformsDirty[SU_MODELVIEW] = qfalse;
	}
	if(pipeline->uniformsDirty[SU_CLIP_PLANE])
	{
		glUniform4fv(pipeline->uniformLocations[SU_CLIP_PLANE], 1, gl.clipPlane);
		pipeline->uniformsDirty[SU_CLIP_PLANE] = qfalse;
	}
	if(pipeline->uniformsDirty[SU_COLOR_SCALE] ||
	   memcmp(gl.depthFadeScale, r_depthFadeScale[tess.shader->dfType], sizeof(gl.depthFadeScale)) != 0)
	{
		glUniform4fv(pipeline->uniformLocations[SU_COLOR_SCALE], 1, r_depthFadeScale[tess.shader->dfType]);
		memcpy(gl.depthFadeScale, r_depthFadeScale[tess.shader->dfType], sizeof(gl.depthFadeScale));
		pipeline->uniformsDirty[SU_COLOR_SCALE] = qfalse;
	}
	if(pipeline->uniformsDirty[SU_COLOR_BIAS] ||
	   memcmp(gl.depthFadeBias, r_depthFadeBias[tess.shader->dfType], sizeof(gl.depthFadeBias)) != 0)
	{
		glUniform4fv(pipeline->uniformLocations[SU_COLOR_BIAS], 1, r_depthFadeBias[tess.shader->dfType]);
		memcpy(gl.depthFadeBias, r_depthFadeBias[tess.shader->dfType], sizeof(gl.depthFadeBias));
		pipeline->uniformsDirty[SU_COLOR_BIAS] = qfalse;
	}
	if(pipeline->uniformsDirty[SU_DIST_OFFSET] ||
	   tess.shader->dfInvDist != gl.depthFadeDist ||
	   tess.shader->dfBias != gl.depthFadeOffset)
	{
		glUniform2f(pipeline->uniformLocations[SU_DIST_OFFSET], tess.shader->dfInvDist, tess.shader->dfBias);
		gl.depthFadeDist = tess.shader->dfInvDist;
		gl.depthFadeOffset = tess.shader->dfBias;
		pipeline->uniformsDirty[SU_DIST_OFFSET] = qfalse;
	}
	if(pipeline->uniformsDirty[SU_GREYSCALE] ||
	   tess.greyscale != gl.greyscale)
	{
		glUniform1f(pipeline->uniformLocations[SU_GREYSCALE], tess.greyscale);
		gl.greyscale = tess.greyscale;
		pipeline->uniformsDirty[SU_GREYSCALE] = qfalse;
	}

	UploadVertexArray(VB_POSITION, tess.xyz);

	for(int i = 0; i < tess.shader->numStages; ++i)
	{
		const shaderStage_t* stage = tess.xstages[i];

		// We have already made sure (in theory) we won't have depth writes enabled
		// to avoid "feedback loops" on the depth texture, resulting in undefined behavior.
		// See "Feedback Loops Between Textures and the Framebuffer" in the GL specs.
		// However, this is not enough for OpenGL 4.5+, where glTextureBarrier is needed too
		// because caching means a feedback loop can happen across draw calls.
		assert((stage->stateBits & GLS_DEPTHTEST_DISABLE) == 0);

		ApplyState(stage->stateBits, tess.shader->cullType, tess.shader->polygonOffset);

		UploadVertexArray(VB_TEXCOORD, tess.svars[i].texcoordsptr);
		UploadVertexArray(VB_COLOR, tess.svars[i].colors);
		UploadIndices(tess.indexes, tess.numIndexes);

		if(pipeline->uniformsDirty[SU_ALPHA_TEST])
		{
			glUniform1ui(pipeline->uniformLocations[SU_ALPHA_TEST], gl.alphaTest);
			pipeline->uniformsDirty[SU_ALPHA_TEST] = qfalse;
		}

		BindBundle(0, &stage->bundle);
		if(!gl.fbMSEnabled)
		{
			BindTexture(1, gl.fbSS[gl.fbReadIndex].depthStencil);
		}

		DrawElements(tess.numIndexes);
	}
}

static void GAL_Draw(drawType_t type)
{
	if(type == DT_GENERIC)
	{
		ApplyPipeline(PID_GENERIC);
		DrawGeneric();
	}
	else if(type == DT_DYNAMIC_LIGHT)
	{
		ApplyPipeline(PID_DYNAMIC_LIGHT);
		DrawDynamicLight();
	}
	else if(type == DT_SOFT_SPRITE)
	{
		ApplyPipeline(PID_SOFT_SPRITE);
		DrawDepthFade();
	}
}

static void GAL_Begin3D()
{
	ApplyPipeline(PID_GENERIC);
	R_MakeIdentityMatrix(gl.modelViewMatrix);
	memcpy(gl.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(gl.projectionMatrix));
	ApplyViewportAndScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	if(backEnd.viewParms.isPortal)
	{
		float plane[4];
		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		float plane2[4];
		plane2[0] = DotProduct(backEnd.viewParms.orient.axis[0], plane);
		plane2[1] = DotProduct(backEnd.viewParms.orient.axis[1], plane);
		plane2[2] = DotProduct(backEnd.viewParms.orient.axis[2], plane);
		plane2[3] = DotProduct(plane, backEnd.viewParms.orient.origin) - plane[3];

		float* o = plane;
		const float* m = s_flipMatrix;
		const float* v = plane2;
		o[0] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12] * v[3];
		o[1] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13] * v[3];
		o[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
		o[3] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];

		memcpy(gl.clipPlane, plane, sizeof(gl.clipPlane));
		ApplyClipPlane(qtrue);
	}
	else
	{
		memset(gl.clipPlane, 0, sizeof(gl.clipPlane));
		ApplyClipPlane(qfalse);
	}

	ApplyState(GLS_DEFAULT, CT_TWO_SIDED, qfalse);

	GLbitfield clearBits = GL_DEPTH_BUFFER_BIT;
	if(backEnd.refdef.rdflags & RDF_HYPERSPACE)
	{
		clearBits |= GL_COLOR_BUFFER_BIT;
		const float c = RB_HyperspaceColor();
		glClearColor(c, c, c, 1.0f);
	}
	else if(r_fastsky->integer && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		clearBits |= GL_COLOR_BUFFER_BIT;
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}
	glClear(clearBits);

	// in case the generic pipeline was already active before calling this function
	gl.pipelines[PID_GENERIC].uniformsDirty[GU_PROJECTION] = qtrue;
	gl.pipelines[PID_GENERIC].uniformsDirty[GU_MODELVIEW] = qtrue;
	gl.pipelines[PID_GENERIC].uniformsDirty[GU_CLIP_PLANE] = qtrue;
}

static void GAL_BeginSkyAndClouds(double depth)
{
	gl.prevEnableClipPlane = gl.enableClipPlane;
	ApplyClipPlane(qfalse);
	glDepthRange(depth, depth);
}

static void GAL_EndSkyAndClouds()
{
	glDepthRange(0.0, 1.0);
	ApplyClipPlane(gl.prevEnableClipPlane);
}

static int GetMaxAnisotropy(image_t* image)
{
	if((image->flags & IMG_NOAF) == 0 && glInfo.maxAnisotropy >= 2 && r_ext_max_anisotropy->integer >= 2)
	{
		return min(r_ext_max_anisotropy->integer, glInfo.maxAnisotropy);
	}

	return 1;
}

static void GAL_CreateTexture(image_t* image, int mipCount, int w, int h)
{
	GLuint id;
	glGenTextures(1, &id);
	image->texnum = (textureHandle_t)id;

	BindImage(0, image);
	SetDebugName(GL_TEXTURE, id, image->name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GetMaxAnisotropy(image));

	if(image->flags & IMG_LMATLAS)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GetTextureInternalFormat(image->format), w, h, 0, GetTextureFormat(image->format), GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		return;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GetTextureWrapMode(image->wrapClampMode));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GetTextureWrapMode(image->wrapClampMode));

	if(Q_stricmp(r_textureMode->string, "GL_NEAREST") == 0 &&
	   (image->flags & (IMG_EXTLMATLAS | IMG_NOPICMIP)) == 0)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else if(image->flags & IMG_NOMIPMAP)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}

static void GAL_UpdateTexture(image_t* image, int mip, int x, int y, int w, int h, const void* data)
{
	BindImage(0, image);
	if(image->flags & IMG_LMATLAS)
	{
		glTexSubImage2D(GL_TEXTURE_2D, (GLint)mip, x, y, w, h, GetTextureFormat(image->format), GL_UNSIGNED_BYTE, data);
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, (GLint)mip, GetTextureInternalFormat(image->format), w, h, 0, GetTextureFormat(image->format), GL_UNSIGNED_BYTE, data);
	}
}

static void GAL_UpdateScratch(image_t* image, int w, int h, const void* data, qbool dirty)
{
	BindImage(0, image);

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if(w != image->width || h != image->height)
	{
		image->width = w;
		image->height = h;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else if(dirty)
	{
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}
}

static void GAL_CreateTextureEx(image_t* image, int mipCount, int mipOffset, int w, int h, const void* mip0)
{
	enum { GroupSize = 8, GroupMask = GroupSize - 1 };

	assert(image->format == TF_RGBA8);
	assert(GetTextureInternalFormat(image->format) == GL_RGBA8);

	// remember what program we had bound before...
	GLint previousProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);

	// create the texture with all mip levels
	GLuint id;
	glGenTextures(1, &id);
	image->texnum = (textureHandle_t)id;
	BindTexture(0, id);
	glTexStorage2D(GL_TEXTURE_2D, mipCount - mipOffset, GL_RGBA8, image->width, image->height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GetTextureWrapMode(image->wrapClampMode));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GetTextureWrapMode(image->wrapClampMode));
	if(Q_stricmp(r_textureMode->string, "GL_NEAREST") == 0 &&
	   (image->flags & (IMG_LMATLAS | IMG_EXTLMATLAS | IMG_NOPICMIP)) == 0)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GetMaxAnisotropy(image));
	SetDebugName(GL_TEXTURE, id, image->name);

	// upload source mip level 0
	BindTexture(0, gl.mipGen.textures[2]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, mip0);

	// create a linear color space copy of source mip 0
	glUseProgram(gl.mipGen.programs[CPID_GAMMA_TO_LINEAR].program);
	glUniform1f(0, r_mipGenGamma->value);
	glBindImageTexture(0, gl.mipGen.textures[2], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
	glBindImageTexture(1, gl.mipGen.textures[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	glDispatchCompute((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);

	// copy to destination mip 0 now if needed
	if(mipOffset == 0)
	{
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glUseProgram(gl.mipGen.programs[CPID_LINEAR_TO_GAMMA].program);
		glUniform1f(0, r_intensity->value);
		glUniform4fv(1, 1, r_mipBlendColors[0]);
		glUniform1f(2, 1.0f / r_mipGenGamma->value);
		glBindImageTexture(0, gl.mipGen.textures[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
		glBindImageTexture(1, id, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
		glDispatchCompute((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);
	}

	for(int i = 1; i < mipCount; ++i)
	{
		const int w1 = w;
		const int h1 = h;
		w = max(w / 2, 1);
		h = max(h / 2, 1);

		// down-sample on the X-axis
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glUseProgram(gl.mipGen.programs[CPID_DOWN_SAMPLE].program);
		glUniform4fv(0, 1, tr.mipFilter);
		glUniform2i(1, w1 - 1, h1 - 1); // maxSize
		glUniform2i(2, w1 / w, 1); // scale
		glUniform2i(3, 1, 0); // offset
		glUniform1ui(4, image->wrapClampMode == TW_CLAMP_TO_EDGE ? 1 : 0);
		glBindImageTexture(0, gl.mipGen.textures[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
		glBindImageTexture(1, gl.mipGen.textures[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glDispatchCompute((w + GroupMask) / GroupSize, (h1 + GroupMask) / GroupSize, 1);

		// down-sample on the Y-axis
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glUseProgram(gl.mipGen.programs[CPID_DOWN_SAMPLE].program);
		glUniform4fv(0, 1, tr.mipFilter);
		glUniform2i(1, w - 1, h1 - 1); // maxSize
		glUniform2i(2, 1, h1 / h); // scale
		glUniform2i(3, 0, 1); // offset
		glUniform1ui(4, image->wrapClampMode == TW_CLAMP_TO_EDGE ? 1 : 0);
		glBindImageTexture(0, gl.mipGen.textures[1], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
		glBindImageTexture(1, gl.mipGen.textures[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glDispatchCompute((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);

		const int destMip = i - mipOffset;
		if(destMip >= 0)
		{
			// copy the gamma-corrected result to the desired mip slice
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			glUseProgram(gl.mipGen.programs[CPID_LINEAR_TO_GAMMA].program);
			glUniform1f(0, r_intensity->value);
			glUniform4fv(1, 1, r_mipBlendColors[r_colorMipLevels->integer ? destMip : 0]);
			glUniform1f(2, 1.0f / r_mipGenGamma->value);
			glBindImageTexture(0, gl.mipGen.textures[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
			glBindImageTexture(1, id, i - mipOffset, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
			glDispatchCompute((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);
		}
	}

	// restore program
	glUseProgram(previousProgram);
}

static void GAL_BeginDynamicLight()
{
	Pipeline* const pipeline = &gl.pipelines[PID_DYNAMIC_LIGHT];
	const dlight_t* const dl = tess.light;

	ApplyPipeline(PID_DYNAMIC_LIGHT);

	glUniform3fv(pipeline->uniformLocations[DU_EYE_POS], 1, backEnd.orient.viewOrigin);
	glUniform3fv(pipeline->uniformLocations[DU_LIGHT_POS], 1, dl->transformed);
	glUniform4f(pipeline->uniformLocations[DU_LIGHT_COLOR_RADIUS], dl->color[0], dl->color[1], dl->color[2], 1.0f / Square(dl->radius));
}

static void GAL_ReadPixels(int x, int y, int w, int h, int alignment, colorSpace_t colorSpace, void* out)
{
	const GLenum format = colorSpace == CS_BGR ? GL_BGR : GL_RGBA;
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	glReadPixels(x, y, w, h, format, GL_UNSIGNED_BYTE, out);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
}

static void GAL_Begin2D()
{
	ApplyPipeline(PID_GENERIC);
	R_MakeIdentityMatrix(gl.modelViewMatrix);
	R_MakeOrthoProjectionMatrix(gl.projectionMatrix, glConfig.vidWidth, glConfig.vidHeight);
	ApplyViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	ApplyClipPlane(qfalse);
	ApplyState(GLS_DEFAULT_2D, CT_TWO_SIDED, qfalse);

	// in case the generic pipeline was already active before calling this function
	gl.pipelines[PID_GENERIC].uniformsDirty[GU_MODELVIEW] = qtrue;
	gl.pipelines[PID_GENERIC].uniformsDirty[GU_PROJECTION] = qtrue;
	gl.pipelines[PID_GENERIC].uniformsDirty[GU_CLIP_PLANE] = qfalse; // not used
}

static void GAL_SetModelViewMatrix(const float* matrix)
{
	memcpy(gl.modelViewMatrix, matrix, sizeof(gl.modelViewMatrix));
	if(gl.pipelineId == PID_GENERIC)
	{
		gl.pipelines[PID_GENERIC].uniformsDirty[GU_MODELVIEW] = qtrue;
	}
	else if(gl.pipelineId == PID_DYNAMIC_LIGHT)
	{
		gl.pipelines[PID_DYNAMIC_LIGHT].uniformsDirty[DU_MODELVIEW] = qtrue;
	}
	else if(gl.pipelineId == PID_SOFT_SPRITE)
	{
		gl.pipelines[PID_SOFT_SPRITE].uniformsDirty[SU_MODELVIEW] = qtrue;
	}
}

static void GAL_SetDepthRange(double zNear, double zFar)
{
	glDepthRange(zNear, zFar);
}

static const char* GetMappingTypeName(MappingType type)
{
	switch(type)
	{
		case MT_SUBDATA: return "glBufferSubData";
		case MT_PERSISTENT: return "glMapBufferRange + GL_MAP_PERSISTENT_BIT";
		case MT_UNSYNC: return "glMapBufferRange + GL_MAP_UNSYNCHRONIZED_BIT";
		case MT_AMDPIN: return "glBufferData + GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD";
		default: return "?";
	}
}

static void GAL_PrintInfo()
{
	ri.Printf(PRINT_ALL, "Geometry upload strategy: %s\n", GetMappingTypeName(gl.mappingType));
}

qbool GAL_GetGL3(graphicsAPILayer_t* rb)
{
	rb->Init = &GAL_Init;
	rb->ShutDown = &GAL_ShutDown;
	rb->BeginSkyAndClouds = &GAL_BeginSkyAndClouds;
	rb->EndSkyAndClouds = &GAL_EndSkyAndClouds;
	rb->ReadPixels = &GAL_ReadPixels;
	rb->BeginFrame = &GAL_BeginFrame;
	rb->EndFrame = &GAL_EndFrame;
	rb->CreateTexture = &GAL_CreateTexture;
	rb->UpdateTexture = &GAL_UpdateTexture;
	rb->UpdateScratch = &GAL_UpdateScratch;
	rb->CreateTextureEx = &GAL_CreateTextureEx;
	rb->Draw = &GAL_Draw;
	rb->Begin2D = &GAL_Begin2D;
	rb->Begin3D = &GAL_Begin3D;
	rb->SetModelViewMatrix = &GAL_SetModelViewMatrix;
	rb->SetDepthRange = &GAL_SetDepthRange;
	rb->BeginDynamicLight = &GAL_BeginDynamicLight;
	rb->PrintInfo = &GAL_PrintInfo;

	return qtrue;
}
