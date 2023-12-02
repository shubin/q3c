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
// Direct3D 11 rendering back-end

#if defined(_WIN32)


#include "tr_local.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>

#pragma region Windows 10 SDK

	#if !defined(__dxgicommon_h__)
		enum DXGI_COLOR_SPACE_TYPE;
	#endif
	#include "dxgi/dxgi1_4.h"
	#include "dxgi/dxgi1_5.h"

	#if !defined(DXGI_PRESENT_ALLOW_TEARING)
		#define DXGI_PRESENT_ALLOW_TEARING 0x00000200UL
	#endif

	#if !defined(DXGI_SWAP_EFFECT_FLIP_DISCARD)
		#define DXGI_SWAP_EFFECT_FLIP_DISCARD ((DXGI_SWAP_EFFECT)4)
	#endif

	#if !defined(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
		#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING ((DXGI_SWAP_CHAIN_FLAG)2048)
	#endif

#pragma endregion

#include "hlsl/generic_vs.h"
#include "hlsl/generic_ps.h"
#include "hlsl/generic_a_ps.h"
#include "hlsl/generic_d_ps.h"
#include "hlsl/generic_ad_ps.h"
#include "hlsl/post_vs.h"
#include "hlsl/post_ps.h"
#include "hlsl/clear_vs.h"
#include "hlsl/clear_ps.h"
#include "hlsl/dl_vs.h"
#include "hlsl/dl_ps.h"
#include "hlsl/sprite_vs.h"
#include "hlsl/sprite_ps.h"
#include "hlsl/mip_start_cs.h"
#include "hlsl/mip_pass_cs.h"
#include "hlsl/mip_end_cs.h"

struct ShaderDesc
{
	const void* code;
	size_t size;
	const char* name;
};

static ShaderDesc genericPixelShaders[4] = 
{
	{ g_generic_ps, ARRAY_LEN(g_generic_ps), "generic pixel shader" },
	{ g_generic_a_ps, ARRAY_LEN(g_generic_a_ps), "generic A2C pixel shader" },
	{ g_generic_d_ps, ARRAY_LEN(g_generic_d_ps), "generic dithered pixel shader" },
	{ g_generic_ad_ps, ARRAY_LEN(g_generic_ad_ps), "generic dithered A2C pixel shader" }
};

#if defined(near)
#	undef near
#endif

#if defined(far)
#	undef far
#endif

#if !defined(D3DDDIERR_DEVICEREMOVED)
#	define D3DDDIERR_DEVICEREMOVED ((HRESULT)0x88760870L)
#endif

#define MAX_GPU_TEXTURE_SIZE 2048 // instead of D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION

#define BLEND_STATE_COUNT (D3D11_BLEND_SRC_ALPHA_SAT + 1)

// a special addition used by the partial depth clear pass
#define GLS_DEPTHFUNC_ALWAYS 0x80000000


/*
Current info:
- feature level 10.1 minimum
- feature level 11.0 for mip-map generation with compute
- shader target 4.1 for graphics (SV_VertexID, unsized Texture2DMS)
- shader target 5.0 for compute  (typed UAVs)

Known issues:
- device re-creation isn't handled by OBS' capture plug-in
*/


enum AlphaTest
{
	AT_ALWAYS,
	AT_GREATER_THAN_0,
	AT_LESS_THAN_HALF,
	AT_GREATER_OR_EQUAL_TO_HALF
};

enum PipelineId
{
	PID_GENERIC,
	PID_SOFT_SPRITE,
	PID_DYNAMIC_LIGHT,
	PID_POST_PROCESS,
	PID_SCREENSHOT,
	PID_CLEAR,
	PID_COUNT
};

enum ErrorMode
{
	EM_FATAL,
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

enum TextureMode
{
	TM_BILINEAR,
	TM_ANISOTROPIC,
	TM_NEAREST,
	TM_COUNT
};

enum DepthFunc
{
	DF_LEQUAL,
	DF_EQUAL,
	DF_ALWAYS,
	DF_COUNT
};

// @NOTE: MSDN says "you must set the ByteWidth value of D3D11_BUFFER_DESC in multiples of 16"
#pragma pack(push, 16)

struct GenericVSData
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float clipPlane[4];
};

struct GenericPSData
{
	uint32_t alphaTest; // AlphaTest enum
	uint32_t texEnv; // texEnv_t enum
	float seed;
	float greyscale;
	float invGamma;
	float invBrightness;
	float noiseScale;
	float alphaBoost;
};

struct DepthFadeVSData
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float clipPlane[4];
};

struct DepthFadePSData
{
	uint32_t alphaTest; // AlphaTest enum
	float distance;
	float offset;
	float greyscale;
	float scale[4];
	float bias[4];
};

struct DynamicLightVSData
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float clipPlane[4];
	float osLightPos[4];
	float osEyePos[4];
};

struct DynamicLightPSData
{
	float lightColor[3];
	float lightRadius;
	float opaque;
	float intensity;
	float greyscale;
	float dummy;
};

struct PostVSData
{
	float scaleX;
	float scaleY;
	float dummy[2];
};

struct PostPSData
{
	float gamma;
	float brightness;
	float greyscale;
	float dummy;
};

struct ClearPSData
{
	float color[4];
};

struct Down4CSData
{
	float weights[4];
	uint32_t maxSize[2];
	uint32_t scale[2];
	uint32_t offset[2];
	uint32_t clampMode; // 0 = repeat
	uint32_t dummy;
};

struct LinearToGammaCSData
{
	float blendColor[4];
	float intensity;
	float invGamma;
	float dummy[2];
};

struct GammaToLinearCSData
{
	float gamma;
	float dummy[3];
};

#pragma pack(pop)

struct Texture
{
	ID3D11Texture2D* texture;
	ID3D11ShaderResourceView* view;
};

struct Pipeline
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* inputLayout; // can be NULL
	ID3D11Buffer* vertexBuffer; // can be NULL
	ID3D11Buffer* pixelBuffer; // can be NULL
};

struct MipGenTexture
{
	ID3D11Texture2D* texture;
	ID3D11ShaderResourceView* srv;
	ID3D11UnorderedAccessView* uav;
};

struct VertexBuffer
{
	ID3D11Buffer* buffer;
	int itemSize;
	int capacity;
	int writeIndex;
	int readIndex;
	qbool discard;
};

struct AdapterInfo
{
	qbool valid;
	int dedicatedSystemMemoryMB;
	int dedicatedVideoMemoryMB;
	int sharedSystemMemoryMB;
};

struct FrameQueries
{
	ID3D11Query* disjoint;
	ID3D11Query* frameStart;
	ID3D11Query* frameEnd;
	qbool valid;
};

struct Direct3D
{
	// constant buffer data
	PostVSData postVSData;
	PostPSData postPSData;
	ClearPSData clearPSData;
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float clipPlane[4];
	float osLightPos[4];
	float osEyePos[4];
	float lightColor[3];
	float lightRadius;
	AlphaTest alphaTest;
	texEnv_t texEnv;
	float frameSeed;

	DXGI_FORMAT formatColorRT;
	DXGI_FORMAT formatDepth;     // float: DXGI_FORMAT_R32_TYPELESS
	DXGI_FORMAT formatDepthRTV;  // float: DXGI_FORMAT_R32_FLOAT
	DXGI_FORMAT formatDepthView; // float: DXGI_FORMAT_D32_FLOAT

	Texture textures[MAX_DRAWIMAGES];
	int textureCount;

	ID3D11SamplerState* samplerStates[TW_COUNT * TM_COUNT];
	int samplerStateIndices[2];

	ID3D11BlendState* blendStates[2 * BLEND_STATE_COUNT * BLEND_STATE_COUNT];
	int blendStateIndex;

	ID3D11DepthStencilState* depthStencilStates[2 * 2 * DF_COUNT];
	int depthStencilStateIndex;

	ID3D11RasterizerState* rasterStates[12];
	int rasterStateIndex;

	ID3D11ShaderResourceView* pixelShaderResources[2];

	Pipeline pipelines[PID_COUNT];
	PipelineId pipelineIndex;

	MipGenTexture mipGenTextures[3]; // 0,1=float16  2=uint8

	VertexBuffer vertexBuffers[VB_COUNT];
	VertexBuffer indexBuffer;

	// for the calls to IASetVertexBuffers
	VertexBufferId vbIds[VB_COUNT];
	ID3D11Buffer* vbBuffers[VB_COUNT];
	UINT vbStrides[VB_COUNT];
	int vbCount;
	qbool splitBufferOffsets;

	ID3D11Texture2D* backBufferTexture;
	ID3D11RenderTargetView* backBufferRTView;
	ID3D11Texture2D* renderTargetTextureMS;
	ID3D11RenderTargetView* renderTargetViewMS;
	ID3D11Texture2D* resolveTexture;
	ID3D11ShaderResourceView* resolveTextureShaderView;
	ID3D11Texture2D* depthStencilTexture;
	ID3D11DepthStencilView* depthStencilView;
	ID3D11ShaderResourceView* depthStencilShaderView;
	ID3D11Texture2D* readbackTexture; // allowed to be NULL!
	ID3D11Texture2D* screenshotTexture; // allowed to be NULL!
	ID3D11RenderTargetView* screenshotTextureRTView; // allowed to be NULL!

	ID3D11ComputeShader* mipGammaToLinearComputeShader;
	ID3D11ComputeShader* mipLinearToGammaComputeShader;
	ID3D11ComputeShader* mipDownSampleComputeShader;
	ID3D11Buffer* mipDownSampleConstBuffer;
	ID3D11Buffer* mipLinearToGammaConstBuffer;
	ID3D11Buffer* mipGammaToLinearConstBuffer;

	FrameQueries frameQueries[32];
	int frameQueriesWriteIndex;
	int frameQueriesReadIndex;

	// cached when starting sky rendering
	float oldSkyClipPlane[4];
	D3D11_VIEWPORT oldSkyViewport;

	ErrorMode errorMode;
};

struct Direct3DStatic
{
	ID3D11Device* device;
	ID3D11DeviceContext* context;
	IDXGISwapChain* swapChain;

	HMODULE library;
	qbool flipAndTear;

	AdapterInfo adapterInfo;
};

__declspec(align(16)) static Direct3D d3d;
__declspec(align(16)) static Direct3DStatic d3ds;


#define COM_RELEASE(p)			do { if(p) { p->Release(); p = NULL; } } while((void)0,0)
#define COM_RELEASE_ARRAY(a)	do { for(int i = 0; i < ARRAY_LEN(a); ++i) { COM_RELEASE(a[i]); } } while((void)0,0)


static void GAL_UpdateTexture(image_t* image, int mip, int x, int y, int w, int h, const void* data);


static const char* GetSystemErrorString(HRESULT hr)
{
	// FormatMessage might not always give us the string we want but that's ok,
	// we always print the original error code anyhow
	static char systemErrorStr[1024];
	const DWORD written = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM, NULL, (DWORD)hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		systemErrorStr, sizeof(systemErrorStr) - 1, NULL);
	if(written == 0)
	{
		// we have nothing valid
		Q_strncpyz(systemErrorStr, "???", sizeof(systemErrorStr));
	}
	else
	{
		// remove the trailing whitespace
		char* s = systemErrorStr + strlen(systemErrorStr) - 1;
		while(s >= systemErrorStr)
		{
			if(*s == '\r' || *s == '\n' || *s == '\t' || *s == ' ')
			{
				*s-- = '\0';
			}
			else
			{
				break;
			}
		}
	}

	return systemErrorStr;
}

static qbool Check(HRESULT hr, const char* function)
{
	if(SUCCEEDED(hr))
	{
		return qtrue;
	}

	if(d3d.errorMode == EM_FATAL)
	{
		ri.Error(ERR_FATAL, "'%s' failed with code 0x%08X (%s)\n", function, (unsigned int)hr, GetSystemErrorString(hr));
	}
	return qfalse;
}

static qbool CheckAndName(HRESULT hr, const char* function, ID3D11DeviceChild* resource, const char* resourceName)
{
	if(SUCCEEDED(hr))
	{
		resource->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(resourceName), resourceName);
		return qtrue;
	}

	if(d3d.errorMode == EM_FATAL)
	{
		ri.Error(ERR_FATAL, "'%s' failed to create '%s' with code 0x%08X (%s)\n", function, resourceName, (unsigned int)hr, GetSystemErrorString(hr));
	}
	return qfalse;
}

static qbool D3D11_CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView, const char* name)
{
	const HRESULT hr = d3ds.device->CreateRenderTargetView(pResource, pDesc, ppRTView);
	return CheckAndName(hr, "CreateRenderTargetView", *ppRTView, name);
}

static qbool D3D11_CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D, const char* name)
{
	const HRESULT hr = d3ds.device->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
	return CheckAndName(hr, "CreateTexture2D", *ppTexture2D, name);
}

static qbool D3D11_CreateShaderResourceView(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView, const char* name)
{
	const HRESULT hr = d3ds.device->CreateShaderResourceView(pResource, pDesc, ppSRView);
	return CheckAndName(hr, "CreateShaderResourceView", *ppSRView, name);
}

static qbool D3D11_CreateUnorderedAccessView(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView, const char* name)
{
	const HRESULT hr = d3ds.device->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
	return CheckAndName(hr, "CreateUnorderedAccessView", *ppUAView, name);
}

static qbool D3D11_CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader, const char* name)
{
	const HRESULT hr = d3ds.device->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
	return CheckAndName(hr, "CreateVertexShader", *ppVertexShader, name);
}

static qbool D3D11_CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader, const char* name)
{
	const HRESULT hr = d3ds.device->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
	return CheckAndName(hr, "CreatePixelShader", *ppPixelShader, name);
}

static qbool D3D11_CreateComputeShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader, const char* name)
{
	const HRESULT hr = d3ds.device->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	return CheckAndName(hr, "CreateComputeShader", *ppComputeShader, name);
}

static qbool D3D11_CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer, const char* name)
{
	const HRESULT hr = d3ds.device->CreateBuffer(pDesc, pInitialData, ppBuffer);
	return CheckAndName(hr, "CreateBuffer", *ppBuffer, name);
}

static qbool D3D11_CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout, const char* name)
{
	const HRESULT hr = d3ds.device->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
	return CheckAndName(hr, "CreateInputLayout", *ppInputLayout, name);
}

static const char* GetDeviceRemovedReasonString(HRESULT reason)
{
	switch(reason)
	{
		case DXGI_ERROR_DEVICE_HUNG: return "device hung";
		case DXGI_ERROR_DEVICE_REMOVED: return "device removed";
		case DXGI_ERROR_DEVICE_RESET: return "device reset";
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return "internal driver error";
		case DXGI_ERROR_INVALID_CALL: return "invalid call";
		case S_OK: return "no error";
		default: return va("unknown error code 0x%08X", (unsigned int)reason);
	}
}

static AlphaTest GetAlphaTest(unsigned int stateBits)
{
	switch(stateBits & GLS_ATEST_BITS)
	{
		case 0: return AT_ALWAYS;
		case GLS_ATEST_GT_0: return AT_GREATER_THAN_0;
		case GLS_ATEST_LT_80: return AT_LESS_THAN_HALF;
		case GLS_ATEST_GE_80: return AT_GREATER_OR_EQUAL_TO_HALF;
		default: return AT_ALWAYS;
	}
}

static DepthFunc GetDepthFunc(unsigned int stateBits)
{
	if(stateBits & GLS_DEPTHFUNC_ALWAYS)
	{
		return DF_ALWAYS;
	}
	
	if(stateBits & GLS_DEPTHFUNC_EQUAL)
	{
		return DF_EQUAL;
	}

	return DF_LEQUAL;
}

static D3D11_COMPARISON_FUNC GetDepthComparison(DepthFunc depthFunc)
{
	switch(depthFunc)
	{
		case DF_ALWAYS: return D3D11_COMPARISON_ALWAYS;
		case DF_EQUAL: return D3D11_COMPARISON_EQUAL;
		default: return D3D11_COMPARISON_LESS_EQUAL;
	}
}

static D3D11_TEXTURE_ADDRESS_MODE GetTextureAddressMode(textureWrap_t wrap)
{
	switch(wrap)
	{
		case TW_CLAMP_TO_EDGE: return D3D11_TEXTURE_ADDRESS_CLAMP;
		case TW_REPEAT: return D3D11_TEXTURE_ADDRESS_WRAP;
		default: return D3D11_TEXTURE_ADDRESS_CLAMP;
	}
}

static DXGI_FORMAT GetTextureFormat(textureFormat_t f)
{
	switch(f)
	{
		case TF_RGBA8:
		default: return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
}

static D3D11_CULL_MODE GetCullMode(cullType_t t)
{
	switch(t)
	{
		case CT_BACK_SIDED: return D3D11_CULL_BACK;
		case CT_FRONT_SIDED: return D3D11_CULL_FRONT;
		case CT_TWO_SIDED: return D3D11_CULL_NONE;
		default: return D3D11_CULL_NONE;
	}
}

static D3D11_BLEND GetSourceBlend(unsigned int stateBits)
{
	switch(stateBits & GLS_SRCBLEND_BITS)
	{
		case GLS_SRCBLEND_ZERO: return D3D11_BLEND_ZERO;
		case GLS_SRCBLEND_ONE: return D3D11_BLEND_ONE;
		case GLS_SRCBLEND_DST_COLOR: return D3D11_BLEND_DEST_COLOR;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: return D3D11_BLEND_INV_DEST_COLOR;
		case GLS_SRCBLEND_SRC_ALPHA: return D3D11_BLEND_SRC_ALPHA;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
		case GLS_SRCBLEND_DST_ALPHA: return D3D11_BLEND_DEST_ALPHA;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA: return D3D11_BLEND_INV_DEST_ALPHA;
		case GLS_SRCBLEND_ALPHA_SATURATE: return D3D11_BLEND_SRC_ALPHA_SAT;
		default: return D3D11_BLEND_ONE;
	}
}

static D3D11_BLEND GetDestinationBlend(unsigned int stateBits)
{
	switch(stateBits & GLS_DSTBLEND_BITS)
	{
		case GLS_DSTBLEND_ZERO: return D3D11_BLEND_ZERO;
		case GLS_DSTBLEND_ONE: return D3D11_BLEND_ONE;
		case GLS_DSTBLEND_SRC_COLOR: return D3D11_BLEND_SRC_COLOR;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: return D3D11_BLEND_INV_SRC_COLOR;
		case GLS_DSTBLEND_SRC_ALPHA: return D3D11_BLEND_SRC_ALPHA;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
		case GLS_DSTBLEND_DST_ALPHA: return D3D11_BLEND_DEST_ALPHA;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA: return D3D11_BLEND_INV_DEST_ALPHA;
		default: return D3D11_BLEND_ONE;
	}
}

static D3D11_BLEND GetAlphaBlendFromColorBlend(D3D11_BLEND colorBlend)
{
	switch(colorBlend)
	{
		case D3D11_BLEND_SRC_COLOR: return D3D11_BLEND_SRC_ALPHA;
		case D3D11_BLEND_INV_SRC_COLOR: return D3D11_BLEND_INV_SRC_ALPHA;
		case D3D11_BLEND_DEST_COLOR: return D3D11_BLEND_DEST_ALPHA;
		case D3D11_BLEND_INV_DEST_COLOR: return D3D11_BLEND_INV_DEST_ALPHA;
		default: return colorBlend;
	}
}

static DXGI_FORMAT GetRenderTargetColorFormat(int format)
{
	switch(format)
	{
		case RTCF_R8G8B8A8: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case RTCF_R10G10B10A2: return DXGI_FORMAT_R10G10B10A2_UNORM;
		case RTCF_R16G16B16A16: return DXGI_FORMAT_R16G16B16A16_UNORM;
		default: return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
}

static void ResetShaderData(ID3D11Resource* buffer, const void* data, size_t bytes)
{
	D3D11_MAPPED_SUBRESOURCE ms;
	const HRESULT hr = d3ds.context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
	Check(hr, "Map on shader data");
	memcpy(ms.pData, data, bytes);
	d3ds.context->Unmap(buffer, NULL);
}

static void AppendVertexData(VertexBuffer* buffer, const void* data, int itemCount)
{
	D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if(buffer->discard || buffer->writeIndex + itemCount > buffer->capacity)
	{
		buffer->discard = qfalse;
		buffer->writeIndex = 0;
		mapType = D3D11_MAP_WRITE_DISCARD;
	}

	if(data != NULL || mapType == D3D11_MAP_WRITE_DISCARD)
	{
		D3D11_MAPPED_SUBRESOURCE ms;
		const HRESULT hr = d3ds.context->Map(buffer->buffer, 0, mapType, NULL, &ms);
		Check(hr, "Map on vertex data");
		if(data != NULL)
		{
			memcpy((byte*)ms.pData + buffer->writeIndex * buffer->itemSize, data, itemCount * buffer->itemSize);
		}
		d3ds.context->Unmap(buffer->buffer, NULL);
	}

	buffer->readIndex = buffer->writeIndex;
	buffer->writeIndex += itemCount;
}

static void AppendVertexDataGroup(const void* data[VB_COUNT], int vertexCount)
{
	for(int i = 0; i < VB_COUNT; ++i)
	{
		AppendVertexData(&d3d.vertexBuffers[i], data[i], vertexCount);
	}
}

static void UploadPendingShaderData()
{
	if((unsigned)d3d.pipelineIndex >= PID_COUNT)
	{
		return;
	}

	const PipelineId pid = d3d.pipelineIndex;
	Pipeline* const pipeline = &d3d.pipelines[pid];

	if(pid == PID_GENERIC)
	{
		GenericVSData vsData;
		GenericPSData psData;
		memcpy(vsData.modelViewMatrix, d3d.modelViewMatrix, sizeof(vsData.modelViewMatrix));
		memcpy(vsData.projectionMatrix, d3d.projectionMatrix, sizeof(vsData.projectionMatrix));
		memcpy(vsData.clipPlane, d3d.clipPlane, sizeof(vsData.clipPlane));
		psData.alphaTest = d3d.alphaTest;
		psData.texEnv = d3d.texEnv;
		psData.seed = d3d.frameSeed;
		psData.greyscale = tess.greyscale;
		psData.invGamma = 1.0f / r_gamma->value;
		psData.invBrightness = 1.0f / r_brightness->value;
		psData.noiseScale = backEnd.projection2D ? 0.0f : r_ditherStrength->value;
		psData.alphaBoost = r_alphaToCoverageMipBoost->value;
		ResetShaderData(pipeline->vertexBuffer, &vsData, sizeof(vsData));
		ResetShaderData(pipeline->pixelBuffer, &psData, sizeof(psData));
	}
	else if(pid == PID_SOFT_SPRITE)
	{
		DepthFadeVSData vsData;
		DepthFadePSData psData;
		memcpy(vsData.modelViewMatrix, d3d.modelViewMatrix, sizeof(vsData.modelViewMatrix));
		memcpy(vsData.projectionMatrix, d3d.projectionMatrix, sizeof(vsData.projectionMatrix));
		memcpy(vsData.clipPlane, d3d.clipPlane, sizeof(vsData.clipPlane));
		psData.alphaTest = d3d.alphaTest;
		memcpy(psData.scale, r_depthFadeScale[tess.shader->dfType], sizeof(psData.scale));
		memcpy(psData.bias, r_depthFadeBias[tess.shader->dfType], sizeof(psData.bias));
		psData.distance = tess.shader->dfInvDist;
		psData.offset = tess.shader->dfBias;
		psData.greyscale = tess.greyscale;
		ResetShaderData(pipeline->vertexBuffer, &vsData, sizeof(vsData));
		ResetShaderData(pipeline->pixelBuffer, &psData, sizeof(psData));
	}
	else if(pid == PID_DYNAMIC_LIGHT)
	{
		DynamicLightVSData vsData;
		DynamicLightPSData psData;
		memcpy(vsData.modelViewMatrix, d3d.modelViewMatrix, sizeof(vsData.modelViewMatrix));
		memcpy(vsData.projectionMatrix, d3d.projectionMatrix, sizeof(vsData.projectionMatrix));
		memcpy(vsData.clipPlane, d3d.clipPlane, sizeof(vsData.clipPlane));
		memcpy(vsData.osEyePos, d3d.osEyePos, sizeof(vsData.osEyePos));
		memcpy(vsData.osLightPos, d3d.osLightPos, sizeof(vsData.osLightPos));
		memcpy(psData.lightColor, d3d.lightColor, sizeof(psData.lightColor));
		psData.lightRadius = d3d.lightRadius;
		psData.opaque = backEnd.dlOpaque ? 1.0f : 0.0f;
		psData.intensity = backEnd.dlIntensity;
		psData.greyscale = tess.greyscale;
		ResetShaderData(pipeline->vertexBuffer, &vsData, sizeof(vsData));
		ResetShaderData(pipeline->pixelBuffer, &psData, sizeof(psData));
	}
	else if(pid == PID_POST_PROCESS)
	{
		ResetShaderData(pipeline->vertexBuffer, &d3d.postVSData, sizeof(d3d.postVSData));
		ResetShaderData(pipeline->pixelBuffer, &d3d.postPSData, sizeof(d3d.postPSData));
	}
	else if(pid == PID_CLEAR)
	{
		ResetShaderData(pipeline->pixelBuffer, &d3d.clearPSData, sizeof(d3d.clearPSData));
	}
}

static int ComputeSamplerStateIndex(int textureWrap, int textureMode)
{
	return textureMode * TW_COUNT + textureWrap;
}

static void ApplySamplerState(UINT slot, textureWrap_t textureWrap, TextureMode textureMode)
{
	const int index = ComputeSamplerStateIndex(textureWrap, textureMode);
	if(index == d3d.samplerStateIndices[slot])
	{
		return;
	}

	d3ds.context->PSSetSamplers(slot, 1, &d3d.samplerStates[index]);
	d3d.samplerStateIndices[slot] = index;
}

static void ApplyPixelShaderResource(UINT slot, ID3D11ShaderResourceView* srv)
{
	if(srv == d3d.pixelShaderResources[slot])
	{
		return;
	}

	d3ds.context->PSSetShaderResources(slot, 1, &srv);
	d3d.pixelShaderResources[slot] = srv;
}

static void DrawIndexed(int indexCount)
{
	if(d3d.splitBufferOffsets)
	{
		UINT offsets[VB_COUNT];
		for(int i = 0; i < d3d.vbCount; ++i)
		{
			VertexBuffer* const vb = &d3d.vertexBuffers[d3d.vbIds[i]];
			offsets[i] = vb->readIndex * vb->itemSize; // in bytes, not vertices
		}

		d3ds.context->IASetVertexBuffers(0, d3d.vbCount, d3d.vbBuffers, d3d.vbStrides, offsets);
		d3ds.context->DrawIndexed(indexCount, d3d.indexBuffer.readIndex, 0);
	}
	else
	{
		d3ds.context->DrawIndexed(indexCount, d3d.indexBuffer.readIndex, d3d.vertexBuffers[VB_POSITION].readIndex);
	}
	backEnd.pc3D[RB_DRAW_CALLS]++;
}

static void ApplyPipeline(PipelineId index)
{
	if(index == d3d.pipelineIndex || (unsigned)index >= PID_COUNT)
	{
		return;
	}

	const PipelineId unfixedIndex = index;
	if(index == PID_SCREENSHOT)
	{
		index = PID_POST_PROCESS;
	}

	Pipeline* const pipeline = &d3d.pipelines[index];
	if(pipeline->inputLayout)
	{
		d3ds.context->IASetInputLayout(pipeline->inputLayout);

		int count = 0;
		VertexBufferId* const ids = d3d.vbIds;
		if(index == PID_GENERIC)
		{
			ids[count++] = VB_POSITION;
			ids[count++] = VB_COLOR;
			ids[count++] = VB_TEXCOORD;
			ids[count++] = VB_TEXCOORD2;
		}
		else if(index == PID_SOFT_SPRITE)
		{
			ids[count++] = VB_POSITION;
			ids[count++] = VB_COLOR;
			ids[count++] = VB_TEXCOORD;
		}
		else if(index == PID_DYNAMIC_LIGHT)
		{
			ids[count++] = VB_POSITION;
			ids[count++] = VB_NORMAL;
			ids[count++] = VB_COLOR;
			ids[count++] = VB_TEXCOORD;
		}
		d3d.vbCount = count;

		for(int i = 0; i < count; ++i)
		{
			VertexBuffer* const vb = &d3d.vertexBuffers[ids[i]];
			d3d.vbBuffers[i] = vb->buffer;
			d3d.vbStrides[i] = vb->itemSize;
		}

		if(!d3d.splitBufferOffsets)
		{
			UINT offsets[VB_COUNT] = { 0 };
			d3ds.context->IASetVertexBuffers(0, count, d3d.vbBuffers, d3d.vbStrides, offsets);
		}
	}
	else
	{
		d3ds.context->IASetInputLayout(NULL);
		d3ds.context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
		d3d.vbCount = 0;
	}

	d3ds.context->VSSetShader(pipeline->vertexShader, NULL, 0);
	d3ds.context->PSSetShader(pipeline->pixelShader, NULL, 0);
	backEnd.pc3D[RB_SHADER_CHANGES]++;

	if(pipeline->vertexBuffer)
	{
		d3ds.context->VSSetConstantBuffers(0, 1, &pipeline->vertexBuffer);
	}
	if(pipeline->pixelBuffer)
	{
		d3ds.context->PSSetConstantBuffers(0, 1, &pipeline->pixelBuffer);
	}

	if(unfixedIndex == PID_POST_PROCESS)
	{
		d3ds.context->OMSetRenderTargets(1, &d3d.backBufferRTView, NULL);
	}
	else if(unfixedIndex == PID_SCREENSHOT)
	{
		d3ds.context->OMSetRenderTargets(1, &d3d.screenshotTextureRTView, NULL);
	}
	else if(unfixedIndex == PID_SOFT_SPRITE)
	{
		d3ds.context->OMSetRenderTargets(1, &d3d.renderTargetViewMS, NULL);
		ApplyPixelShaderResource(1, d3d.depthStencilShaderView);
		ApplySamplerState(1, TW_CLAMP_TO_EDGE, TM_BILINEAR);
	}
	else
	{
		// keep this call order to make sure the depth buffer isn't bound as a SRV anymore
		// when we set it as a render target
		ApplyPixelShaderResource(1, d3d.textures[0].view);
		d3ds.context->OMSetRenderTargets(1, &d3d.renderTargetViewMS, d3d.depthStencilView);
	}

	d3d.pipelineIndex = index;
}

static void ApplyViewport(int x, int y, int w, int h, int th)
{
	const int top = th - y - h;

	D3D11_VIEWPORT vp;
	vp.TopLeftX = x;
	vp.TopLeftY = top;
	vp.Width = w;
	vp.Height = h;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	d3ds.context->RSSetViewports(1, &vp);
}

static void ApplyScissor(int x, int y, int w, int h, int th)
{
	const int top = th - y - h;
	const int bottom = th - y;

	D3D11_RECT sr;
	sr.left = x;
	sr.top = top;
	sr.right = x + w;
	sr.bottom = bottom;
	d3ds.context->RSSetScissorRects(1, &sr);
}

static void ApplyViewportAndScissor(int x, int y, int w, int h, int th)
{
	ApplyViewport(x, y, w, h, th);
	ApplyScissor(x, y, w, h, th);
}

static void CreateTexture(Texture* texture, image_t* image, int mipCount, int w, int h)
{
	COM_RELEASE(texture->texture);
	COM_RELEASE(texture->view);

	ID3D11Texture2D* texture2D;
	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.Format = GetTextureFormat(image->format);
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.MipLevels = mipCount;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	D3D11_CreateTexture2D(&texDesc, NULL, &texture2D, image->name);

	ID3D11ShaderResourceView* view;
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	ZeroMemory(&viewDesc, sizeof(viewDesc));
	viewDesc.Format = texDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels = UINT(-1);
	viewDesc.Texture2D.MostDetailedMip = 0;
	D3D11_CreateShaderResourceView(texture2D, &viewDesc, &view, image->name);

	texture->texture = texture2D;
	texture->view = view;
}

static void UpdateAnimatedImage(image_t* image, int w, int h, const byte* data, qbool dirty)
{
	if(w != image->width || h != image->height)
	{
		image->width = w;
		image->height = h;
		CreateTexture(&d3d.textures[image->texnum], image, 1, w, h);
		GAL_UpdateTexture(image, 0, 0, 0, w, h, data);
	}
	else if(dirty)
	{
		GAL_UpdateTexture(image, 0, 0, 0, w, h, data);
	}
}

static const image_t* GetBundleImage(const textureBundle_t* bundle)
{
	return R_UpdateAndGetBundleImage(bundle, &UpdateAnimatedImage);
}

static int ComputeBlendStateIndex(int srcBlend, int dstBlend, qbool alphaToCoverage)
{
	return alphaToCoverage * (BLEND_STATE_COUNT * BLEND_STATE_COUNT) + (srcBlend * BLEND_STATE_COUNT) + dstBlend;
}

static void ApplyBlendState(D3D11_BLEND srcBlend, D3D11_BLEND dstBlend, qbool aphaToCoverage)
{
	const int index = ComputeBlendStateIndex(srcBlend, dstBlend, aphaToCoverage);
	if((unsigned)index >= ARRAY_LEN(d3d.blendStates))
		ri.Error(ERR_FATAL, "Tried to set an invalid blend state combo!");
	if(d3d.blendStates[index] == NULL)
		ri.Error(ERR_FATAL, "Tried to set an unregistered blend state!");

	if(index == d3d.blendStateIndex)
	{
		return;
	}

	d3ds.context->OMSetBlendState(d3d.blendStates[index], NULL, 0xFFFFFFFF);
	d3d.blendStateIndex = index;
}

static int ComputeDepthStencilStateIndex(int disableDepth, int depthFunc, int maskTrue)
{
	return maskTrue + (depthFunc + (disableDepth * DF_COUNT)) * 2;
}

static void ApplyDepthStencilState(qbool disableDepth, DepthFunc depthFunc, qbool maskTrue)
{
	const int index = ComputeDepthStencilStateIndex(disableDepth, (int)depthFunc, maskTrue);
	if(index == d3d.depthStencilStateIndex)
	{
		return;
	}

	d3d.depthStencilStateIndex = index;
	d3ds.context->OMSetDepthStencilState(d3d.depthStencilStates[index], 0);
}

static int ComputeRasterizerStateIndex(int wireFrame, int cullType, int polygonOffset)
{
	return cullType * 4 + wireFrame * 2 + polygonOffset;
}

static void ApplyRasterizerState(qbool wireFrame, cullType_t cullType, qbool polygonOffset)
{
	const int index = ComputeRasterizerStateIndex(wireFrame, cullType, polygonOffset);
	if(index == d3d.rasterStateIndex)
	{
		return;
	}

	d3d.rasterStateIndex = index;
	d3ds.context->RSSetState(d3d.rasterStates[index]);
}

static void ApplyState(unsigned int stateBits, cullType_t cullType, qbool polygonOffset)
{
	static unsigned int oldStateBits = 0;

	const unsigned int diffBits = oldStateBits ^ stateBits;
	oldStateBits = stateBits;

	d3d.alphaTest = GetAlphaTest(stateBits);

	if(diffBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_ATEST_BITS))
	{
		const D3D11_BLEND srcBlend = (stateBits & GLS_SRCBLEND_BITS) ? GetSourceBlend(stateBits) : D3D11_BLEND_ONE;
		const D3D11_BLEND dstBlend = (stateBits & GLS_DSTBLEND_BITS) ? GetDestinationBlend(stateBits) : D3D11_BLEND_ZERO;
		ApplyBlendState(srcBlend, dstBlend, glInfo.alphaToCoverageSupport && d3d.pipelineIndex == PID_GENERIC && d3d.alphaTest != AT_ALWAYS);
	}

	const qbool disableDepth = (stateBits & GLS_DEPTHTEST_DISABLE) ? 1 : 0;
	const DepthFunc depthFunc = GetDepthFunc(stateBits);
	const qbool maskTrue = (stateBits & GLS_DEPTHMASK_TRUE) ? 1 : 0;
	ApplyDepthStencilState(disableDepth, depthFunc, maskTrue);

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
	ApplyRasterizerState((stateBits & GLS_POLYMODE_LINE) != 0, cullType, polygonOffset);
}

static void BindImage(UINT slot, const image_t* image)
{
	ID3D11ShaderResourceView* view = d3d.textures[image->texnum].view;
	ApplyPixelShaderResource(slot, view);
	TextureMode mode = TM_ANISOTROPIC;
	if(Q_stricmp(r_textureMode->string, "GL_NEAREST") == 0 &&
	   !backEnd.projection2D &&
	   (image->flags & (IMG_LMATLAS | IMG_EXTLMATLAS | IMG_NOPICMIP)) == 0)
	{
		mode = TM_NEAREST;
	}
	else if((image->flags & IMG_NOAF) != 0)
	{
		mode = TM_BILINEAR;
	}
	ApplySamplerState(slot, image->wrapClampMode, mode);
}

static void BindBundle(UINT slot, const textureBundle_t* bundle)
{
	BindImage(slot, GetBundleImage(bundle));
}

static void FindBestAvailableAA(DXGI_SAMPLE_DESC* sampleDesc)
{
	// @NOTE: D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT == D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT
	sampleDesc->Count = (UINT)min(r_msaa->integer, D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT);
	sampleDesc->Quality = 0;

	if(r_colorMipLevels->integer)
	{
		sampleDesc->Count = 0;
	}

	while(sampleDesc->Count > 0)
	{
		UINT levelCount = 0;
		if(SUCCEEDED(d3ds.device->CheckMultisampleQualityLevels(d3d.formatColorRT, sampleDesc->Count, &levelCount)) &&
		   levelCount > 0 &&
		   SUCCEEDED(d3ds.device->CheckMultisampleQualityLevels(d3d.formatDepth, sampleDesc->Count, &levelCount)) &&
		   levelCount > 0)
		   break;

		--sampleDesc->Count;
	}

	if(sampleDesc->Count <= 1)
	{
		sampleDesc->Count = 1;
		sampleDesc->Quality = 0;
	}
}

static qbool CheckFlipAndTearSupport()
{
	if(r_d3d11_presentMode->integer != DXGIPM_FLIPDISCARD)
	{
		return qfalse;
	}

	HMODULE library = LoadLibraryA("DXGI.dll");
	if(library == NULL)
	{
		ri.Printf(PRINT_WARNING, "CheckTearingSupport: DXGI.dll couldn't be found or opened\n");
		return qfalse;
	}

	typedef HRESULT (WINAPI *PFN_CreateDXGIFactory)(REFIID riid, _Out_ void **ppFactory);
	PFN_CreateDXGIFactory pCreateDXGIFactory = (PFN_CreateDXGIFactory)GetProcAddress(library, "CreateDXGIFactory");
	if(pCreateDXGIFactory == NULL)
	{
		FreeLibrary(library);
		ri.Printf(PRINT_WARNING, "CheckTearingSupport: Failed to locate CreateDXGIFactory in DXGI.dll\n");
		return qfalse;
	}

	HRESULT hr;
	BOOL enabled = FALSE;
	IDXGIFactory5* pFactory;
	hr = (*pCreateDXGIFactory)(__uuidof(IDXGIFactory5), (void**)&pFactory);
	if(FAILED(hr))
	{
		FreeLibrary(library);
		ri.Printf(PRINT_WARNING, "CheckTearingSupport: 'CreateDXGIFactory' failed with code 0x%08X (%s)\n", (unsigned int)hr, GetSystemErrorString(hr));
		return qfalse;
	}
	hr = pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &enabled, sizeof(enabled));
	pFactory->Release();
	FreeLibrary(library);

	if(FAILED(hr))
	{
		ri.Printf(PRINT_WARNING, "CheckTearingSupport: 'IDXGIFactory5::CheckFeatureSupport' failed with code 0x%08X (%s)\n", (unsigned int)hr, GetSystemErrorString(hr));
		return qfalse;
	}

	return enabled != 0;
}

static qbool GAL_Init()
{
	Sys_V_Init(GAL_D3D11);

	ZeroMemory(&d3d, sizeof(d3d));

	HRESULT hr = S_OK;
	qbool fullInit = qfalse;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	if(d3ds.library == NULL)
	{
		fullInit = qtrue;

		d3ds.library = LoadLibraryA("D3D11.dll");
		if(d3ds.library == NULL)
			ri.Error(ERR_FATAL, "D3D11.dll couldn't be found or opened");

		PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN pD3D11CreateDeviceAndSwapChain =
			(PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(d3ds.library, "D3D11CreateDeviceAndSwapChain");
		if(pD3D11CreateDeviceAndSwapChain == NULL)
			ri.Error(ERR_FATAL, "Failed to locate D3D11CreateDeviceAndSwapChain in D3D11.dll");

		const D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
		UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined(_DEBUG)
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		d3ds.flipAndTear = CheckFlipAndTearSupport();

		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		swapChainDesc.BufferDesc.Width = glInfo.winWidth;
		swapChainDesc.BufferDesc.Height = glInfo.winHeight;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = GetActiveWindow();
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Windowed = TRUE;
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		if(d3ds.flipAndTear)
		{
			// flip and tear, until it is done
			swapChainDesc.BufferCount = 2;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}
		else
		{
			swapChainDesc.BufferCount = 1;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			swapChainDesc.Flags = 0;
		}

	create_device:
		hr = (*pD3D11CreateDeviceAndSwapChain)(
			NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, featureLevels, ARRAY_LEN(featureLevels), D3D11_SDK_VERSION,
			&swapChainDesc, &d3ds.swapChain, &d3ds.device, NULL, &d3ds.context);
		if(hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
		{
			ri.Printf(PRINT_WARNING, "D3D11CreateDeviceAndSwapChain failed because you don't have the SDK installed.\n");
			ri.Printf(PRINT_WARNING, "Trying to create the device again without the debug layer...\n");
			flags &= ~D3D11_CREATE_DEVICE_DEBUG;
			goto create_device;
		}
		Check(hr, "D3D11CreateDeviceAndSwapChain");
	}
	else
	{
		hr = d3ds.swapChain->GetDesc(&swapChainDesc);
		Check(hr, "IDXGISwapChain::GetDesc");
	}

	d3d.formatColorRT = GetRenderTargetColorFormat(r_rtColorFormat->integer);
	d3d.formatDepth = DXGI_FORMAT_R24G8_TYPELESS;
	d3d.formatDepthRTV = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	d3d.formatDepthView = DXGI_FORMAT_D24_UNORM_S8_UINT;

	D3D11_TEXTURE2D_DESC readbackTexDesc;
	ZeroMemory(&readbackTexDesc, sizeof(readbackTexDesc));
	readbackTexDesc.Width = glConfig.vidWidth;
	readbackTexDesc.Height = glConfig.vidHeight;
	readbackTexDesc.MipLevels = 1;
	readbackTexDesc.ArraySize = 1;
	readbackTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	readbackTexDesc.SampleDesc.Count = 1;
	readbackTexDesc.SampleDesc.Quality = 0;
	readbackTexDesc.Usage = D3D11_USAGE_STAGING;
	readbackTexDesc.BindFlags = 0;
	readbackTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	readbackTexDesc.MiscFlags = 0;
	d3d.errorMode = EM_SILENT;
	if(!D3D11_CreateTexture2D(&readbackTexDesc, 0, &d3d.readbackTexture, "readback texture"))
		ri.Printf(PRINT_WARNING, "Screengrab texture creation failed! /" S_COLOR_CMD "screenshot^7* and /" S_COLOR_CMD "video^7 won't work\n");
	d3d.errorMode = EM_FATAL;

	if(d3d.readbackTexture != NULL && r_mode->integer == VIDEOMODE_UPSCALE)
	{
		d3d.errorMode = EM_SILENT;

		D3D11_TEXTURE2D_DESC screenshotTexDesc;
		ZeroMemory(&screenshotTexDesc, sizeof(screenshotTexDesc));
		screenshotTexDesc.Width = glConfig.vidWidth;
		screenshotTexDesc.Height = glConfig.vidHeight;
		screenshotTexDesc.MipLevels = 1;
		screenshotTexDesc.ArraySize = 1;
		screenshotTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		screenshotTexDesc.SampleDesc.Count = 1;
		screenshotTexDesc.SampleDesc.Quality = 0;
		screenshotTexDesc.Usage = D3D11_USAGE_DEFAULT;
		screenshotTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		screenshotTexDesc.CPUAccessFlags = 0;
		screenshotTexDesc.MiscFlags = 0;
		if(!D3D11_CreateTexture2D(&screenshotTexDesc, 0, &d3d.screenshotTexture, "screenshot texture"))
			ri.Printf(PRINT_WARNING, "Screenshot texture creation failed! /" S_COLOR_CMD "screenshot^7* and /" S_COLOR_CMD "video^7 may not work\n");

		D3D11_RENDER_TARGET_VIEW_DESC screenshotRTVDesc;
		ZeroMemory(&screenshotRTVDesc, sizeof(screenshotRTVDesc));
		screenshotRTVDesc.Format = swapChainDesc.BufferDesc.Format;
		screenshotRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		screenshotRTVDesc.Texture2D.MipSlice = 0;
		if(!D3D11_CreateRenderTargetView(d3d.screenshotTexture, &screenshotRTVDesc, &d3d.screenshotTextureRTView, "screenshot texture render target view"))
			ri.Printf(PRINT_WARNING, "Screenshot texture RTV creation failed! /" S_COLOR_CMD "screenshot^7* and /" S_COLOR_CMD "video^7 may not work\n");

		d3d.errorMode = EM_FATAL;
	}

	hr = d3ds.swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&d3d.backBufferTexture);
	CheckAndName(hr, "GetBuffer", d3d.backBufferTexture, "back buffer texture");

	D3D11_RENDER_TARGET_VIEW_DESC colorViewDesc; // needed?
	ZeroMemory(&colorViewDesc, sizeof(colorViewDesc));
	colorViewDesc.Format = swapChainDesc.BufferDesc.Format;
	colorViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	colorViewDesc.Texture2D.MipSlice = 0;
	D3D11_CreateRenderTargetView(d3d.backBufferTexture, &colorViewDesc, &d3d.backBufferRTView, "back buffer render target view");

	DXGI_SAMPLE_DESC sampleDesc;
	FindBestAvailableAA(&sampleDesc);
	const qbool alphaToCoverageOK = sampleDesc.Count > 1 && r_alphaToCoverage->integer != 0;

	D3D11_TEXTURE2D_DESC renderTargetTexDesc;
	ZeroMemory(&renderTargetTexDesc, sizeof(renderTargetTexDesc));
	renderTargetTexDesc.Width = glConfig.vidWidth;
	renderTargetTexDesc.Height = glConfig.vidHeight;
	renderTargetTexDesc.MipLevels = 1;
	renderTargetTexDesc.ArraySize = 1;
	renderTargetTexDesc.Format = d3d.formatColorRT;
	renderTargetTexDesc.SampleDesc.Count = sampleDesc.Count;
	renderTargetTexDesc.SampleDesc.Quality = sampleDesc.Quality;
	renderTargetTexDesc.Usage = D3D11_USAGE_DEFAULT;
	renderTargetTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	renderTargetTexDesc.CPUAccessFlags = 0;
	renderTargetTexDesc.MiscFlags = 0;
	D3D11_CreateTexture2D(&renderTargetTexDesc, 0, &d3d.renderTargetTextureMS, "MS render target texture");

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
	rtvDesc.Format = renderTargetTexDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	D3D11_CreateRenderTargetView(d3d.renderTargetTextureMS, &rtvDesc, &d3d.renderTargetViewMS, "MS render target view");

	ZeroMemory(&renderTargetTexDesc, sizeof(renderTargetTexDesc));
	renderTargetTexDesc.Width = glConfig.vidWidth;
	renderTargetTexDesc.Height = glConfig.vidHeight;
	renderTargetTexDesc.MipLevels = 1;
	renderTargetTexDesc.ArraySize = 1;
	renderTargetTexDesc.Format = d3d.formatColorRT;
	renderTargetTexDesc.SampleDesc.Count = 1;
	renderTargetTexDesc.SampleDesc.Quality = 0;
	renderTargetTexDesc.Usage = D3D11_USAGE_DEFAULT;
	renderTargetTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	renderTargetTexDesc.CPUAccessFlags = 0;
	renderTargetTexDesc.MiscFlags = 0;
	D3D11_CreateTexture2D(&renderTargetTexDesc, 0, &d3d.resolveTexture, "resolve texture");

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = renderTargetTexDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	D3D11_CreateShaderResourceView(d3d.resolveTexture, &srvDesc, &d3d.resolveTextureShaderView, "resolve texture shader resource view");

	D3D11_TEXTURE2D_DESC depthStencilTexDesc;
	ZeroMemory(&depthStencilTexDesc, sizeof(depthStencilTexDesc));
	depthStencilTexDesc.Width = glConfig.vidWidth;
	depthStencilTexDesc.Height = glConfig.vidHeight;
	depthStencilTexDesc.MipLevels = 1;
	depthStencilTexDesc.ArraySize = 1;
	depthStencilTexDesc.Format = d3d.formatDepth;
	depthStencilTexDesc.SampleDesc.Count = sampleDesc.Count;
	depthStencilTexDesc.SampleDesc.Quality = sampleDesc.Quality;
	depthStencilTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	depthStencilTexDesc.CPUAccessFlags = 0;
	depthStencilTexDesc.MiscFlags = 0;
	D3D11_CreateTexture2D(&depthStencilTexDesc, 0, &d3d.depthStencilTexture, "depth stencil texture");

	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
	depthStencilViewDesc.Format = d3d.formatDepthView;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	depthStencilViewDesc.Texture2D.MipSlice = 0;
	hr = d3ds.device->CreateDepthStencilView(d3d.depthStencilTexture, &depthStencilViewDesc, &d3d.depthStencilView);
	CheckAndName(hr, "CreateDepthStencilView", d3d.depthStencilView, "depth stencil view");

	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = d3d.formatDepthRTV;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
	D3D11_CreateShaderResourceView(d3d.depthStencilTexture, &srvDesc, &d3d.depthStencilShaderView, "depth stencil shader resource view");

	const ShaderDesc* const genericPS = &genericPixelShaders[(alphaToCoverageOK != 0) + 2 * (r_dither->integer != 0)];
	D3D11_CreateVertexShader(g_generic_vs, ARRAY_LEN(g_generic_vs), NULL, &d3d.pipelines[PID_GENERIC].vertexShader, "generic vertex shader");
	D3D11_CreatePixelShader(genericPS->code, genericPS->size, NULL, &d3d.pipelines[PID_GENERIC].pixelShader, genericPS->name);

	D3D11_INPUT_ELEMENT_DESC genericInputLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	D3D11_CreateInputLayout(genericInputLayoutDesc, ARRAY_LEN(genericInputLayoutDesc), g_generic_vs, ARRAY_LEN(g_generic_vs), &d3d.pipelines[PID_GENERIC].inputLayout, "generic input layout");

	d3ds.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const int maxVertexCount = SHADER_MAX_VERTEXES;
	const int maxIndexCount = SHADER_MAX_INDEXES;

	VertexBuffer* const vb = d3d.vertexBuffers;
	vb[VB_POSITION].itemSize = sizeof(vec4_t);
	vb[VB_NORMAL].itemSize = sizeof(vec4_t);
	vb[VB_TEXCOORD].itemSize = sizeof(vec2_t);
	vb[VB_TEXCOORD2].itemSize = sizeof(vec2_t);
	vb[VB_COLOR].itemSize = sizeof(color4ub_t);
	d3d.indexBuffer.itemSize = sizeof(uint32_t);
	for(int i = 0; i < ARRAY_LEN(d3d.vertexBuffers); ++i)
	{
		vb[i].capacity = maxVertexCount;
		vb[i].discard = qtrue;
	}
	d3d.indexBuffer.capacity = maxIndexCount;
	d3d.indexBuffer.discard = qtrue;

	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = maxVertexCount * vb[VB_POSITION].itemSize;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&vertexBufferDesc, NULL, &vb[VB_POSITION].buffer, "position vertex buffer");
	D3D11_CreateBuffer(&vertexBufferDesc, NULL, &vb[VB_NORMAL].buffer, "normal vertex buffer");

	D3D11_BUFFER_DESC colorBufferDesc;
	ZeroMemory(&colorBufferDesc, sizeof(colorBufferDesc));
	colorBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	colorBufferDesc.ByteWidth = maxVertexCount * vb[VB_COLOR].itemSize;
	colorBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	colorBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&colorBufferDesc, NULL, &vb[VB_COLOR].buffer, "color vertex buffer");

	D3D11_BUFFER_DESC texCoordBufferDesc;
	ZeroMemory(&texCoordBufferDesc, sizeof(texCoordBufferDesc));
	texCoordBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	texCoordBufferDesc.ByteWidth = maxVertexCount * vb[VB_TEXCOORD].itemSize;
	texCoordBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	texCoordBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&texCoordBufferDesc, NULL, &vb[VB_TEXCOORD].buffer, "texture coordinates vertex buffer #1");
	D3D11_CreateBuffer(&texCoordBufferDesc, NULL, &vb[VB_TEXCOORD2].buffer, "texture coordinates vertex buffer #2");

	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
	indexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	indexBufferDesc.ByteWidth = maxIndexCount * d3d.indexBuffer.itemSize;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&indexBufferDesc, NULL, &d3d.indexBuffer.buffer, "index buffer");

	d3ds.context->IASetIndexBuffer(d3d.indexBuffer.buffer, DXGI_FORMAT_R32_UINT, 0);

	D3D11_BUFFER_DESC vertexShaderBufferDesc;
	ZeroMemory(&vertexShaderBufferDesc, sizeof(vertexShaderBufferDesc));
	vertexShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexShaderBufferDesc.ByteWidth = sizeof(GenericVSData);
	vertexShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vertexShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&vertexShaderBufferDesc, NULL, &d3d.pipelines[PID_GENERIC].vertexBuffer, "generic vertex shader buffer");

	D3D11_BUFFER_DESC pixelShaderBufferDesc;
	ZeroMemory(&pixelShaderBufferDesc, sizeof(pixelShaderBufferDesc));
	pixelShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	pixelShaderBufferDesc.ByteWidth = sizeof(GenericPSData);
	pixelShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pixelShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&pixelShaderBufferDesc, NULL, &d3d.pipelines[PID_GENERIC].pixelBuffer, "generic pixel shader buffer");

	// create all sampler states
	for(int textureMode = 0; textureMode < TM_COUNT; ++textureMode)
	{
		for(int wrapMode = 0; wrapMode < TW_COUNT; ++wrapMode)
		{
			const int index = ComputeSamplerStateIndex(wrapMode, textureMode);

			// @NOTE: D3D10_REQ_MAXANISOTROPY == D3D11_REQ_MAXANISOTROPY
			const int maxAnisotropy = Com_ClampInt(1, D3D11_REQ_MAXANISOTROPY, r_ext_max_anisotropy->integer);
			const D3D11_TEXTURE_ADDRESS_MODE mode = GetTextureAddressMode((textureWrap_t)wrapMode);
			ID3D11SamplerState* samplerState;
			D3D11_SAMPLER_DESC samplerDesc;
			ZeroMemory(&samplerDesc, sizeof(samplerDesc));
			samplerDesc.Filter = textureMode == TM_NEAREST ?
				D3D11_FILTER_MIN_MAG_MIP_POINT :
				((textureMode == TM_BILINEAR || maxAnisotropy == 1) ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_ANISOTROPIC);
			samplerDesc.AddressU = mode;
			samplerDesc.AddressV = mode;
			samplerDesc.AddressW = mode;
			samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
			samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
			samplerDesc.MaxAnisotropy = textureMode == TM_ANISOTROPIC ? maxAnisotropy : 1;
			hr = d3ds.device->CreateSamplerState(&samplerDesc, &samplerState);
			CheckAndName(hr, "CreateSamplerState", samplerState, va("sampler state %03d", index));

			d3d.samplerStates[index] = samplerState;
		}
	}

	// force set the default sampler states
	for(int i = 0; i < ARRAY_LEN(d3d.samplerStateIndices); ++i)
	{
		d3d.samplerStateIndices[i] = -1;
		ApplySamplerState(i, TW_CLAMP_TO_EDGE, TM_BILINEAR);
	}

	// create all blend states
	const int coverageModes = alphaToCoverageOK ? 2 : 1;
	for(int a = 0; a < coverageModes; ++a)
	{
		for(int s = 1; s < BLEND_STATE_COUNT; ++s)
		{
			for(int d = 1; d < BLEND_STATE_COUNT; ++d)
			{
				const int index = ComputeBlendStateIndex(s, d, a);

				ID3D11BlendState* blendState;
				D3D11_BLEND_DESC blendDesc;
				ZeroMemory(&blendDesc, sizeof(blendDesc));
				blendDesc.AlphaToCoverageEnable = a == 1 ? TRUE : FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlend = (D3D11_BLEND)s;
				blendDesc.RenderTarget[0].DestBlend = (D3D11_BLEND)d;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = GetAlphaBlendFromColorBlend((D3D11_BLEND)s);
				blendDesc.RenderTarget[0].DestBlendAlpha = GetAlphaBlendFromColorBlend((D3D11_BLEND)d);
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = d3ds.device->CreateBlendState(&blendDesc, &blendState);
				CheckAndName(hr, "CreateBlendState", blendState, va("blend state %03d", index));

				d3d.blendStates[index] = blendState;
			}
		}
	}

	// create all the depth/stencil states
	for(int disableDepth = 0; disableDepth < 2; ++disableDepth)
	{
		for(int depthFunc = 0; depthFunc < DF_COUNT; ++depthFunc)
		{
			for(int maskTrue = 0; maskTrue < 2; ++maskTrue)
			{
				const int index = ComputeDepthStencilStateIndex(disableDepth, depthFunc, maskTrue);

				ID3D11DepthStencilState* depthState;
				D3D11_DEPTH_STENCIL_DESC depthDesc;
				ZeroMemory(&depthDesc, sizeof(depthDesc));
				depthDesc.DepthEnable = disableDepth ? FALSE : TRUE;
				depthDesc.DepthFunc = GetDepthComparison((DepthFunc)depthFunc);
				depthDesc.DepthWriteMask = maskTrue ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
				depthDesc.StencilEnable = FALSE;
				hr = d3ds.device->CreateDepthStencilState(&depthDesc, &depthState);
				CheckAndName(hr, "CreateDepthStencilState", depthState, va("depth/stencil state %03d", index));
				
				d3d.depthStencilStates[index] = depthState;
			}
		}
	}

	// create all the raster states
	for(int polygonOffset = 0; polygonOffset < 2; ++polygonOffset)
	{
		for(int wireFrame = 0; wireFrame < 2; ++wireFrame)
		{
			for(int cullType = 0; cullType < CT_COUNT; ++cullType)
			{
				const int index = ComputeRasterizerStateIndex(wireFrame, cullType, polygonOffset);

				ID3D11RasterizerState* rasterState;
				D3D11_RASTERIZER_DESC rasterDesc;
				ZeroMemory(&rasterDesc, sizeof(rasterDesc));
				rasterDesc.FillMode = wireFrame ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
				rasterDesc.CullMode = GetCullMode((cullType_t)cullType);
				rasterDesc.FrontCounterClockwise = TRUE;
				rasterDesc.ScissorEnable = TRUE;
				rasterDesc.DepthClipEnable = r_depthClamp->integer ? FALSE : TRUE;
				rasterDesc.DepthBiasClamp = 0.0f;
				rasterDesc.DepthBias = polygonOffset ? -1 : 0;
				rasterDesc.SlopeScaledDepthBias = polygonOffset ? -1.0f : 0.0f;
				hr = d3ds.device->CreateRasterizerState(&rasterDesc, &rasterState);
				CheckAndName(hr, "CreateRasterizerState", rasterState, va("raster state %03d", index));

				d3d.rasterStates[index] = rasterState;
			}
		}
	}

	//
	// post-processing
	//

	D3D11_CreateVertexShader(g_post_vs, ARRAY_LEN(g_post_vs), NULL, &d3d.pipelines[PID_POST_PROCESS].vertexShader, "post-process vertex shader");
	D3D11_CreatePixelShader(g_post_ps, ARRAY_LEN(g_post_ps), NULL, &d3d.pipelines[PID_POST_PROCESS].pixelShader, "post-process pixel shader");

	ZeroMemory(&vertexShaderBufferDesc, sizeof(vertexShaderBufferDesc));
	vertexShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexShaderBufferDesc.ByteWidth = sizeof(d3d.postVSData);
	vertexShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vertexShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&vertexShaderBufferDesc, NULL, &d3d.pipelines[PID_POST_PROCESS].vertexBuffer, "post-process vertex shader buffer");

	ZeroMemory(&pixelShaderBufferDesc, sizeof(pixelShaderBufferDesc));
	pixelShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	pixelShaderBufferDesc.ByteWidth = sizeof(d3d.postPSData);
	pixelShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pixelShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&pixelShaderBufferDesc, NULL, &d3d.pipelines[PID_POST_PROCESS].pixelBuffer, "post-process pixel shader buffer");

	//
	// partial render target clears
	//

	D3D11_CreateVertexShader(g_clear_vs, ARRAY_LEN(g_clear_vs), NULL, &d3d.pipelines[PID_CLEAR].vertexShader, "clear vertex shader");
	D3D11_CreatePixelShader(g_clear_ps, ARRAY_LEN(g_clear_ps), NULL, &d3d.pipelines[PID_CLEAR].pixelShader, "clear pixel shader");

	ZeroMemory(&pixelShaderBufferDesc, sizeof(pixelShaderBufferDesc));
	pixelShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	pixelShaderBufferDesc.ByteWidth = sizeof(d3d.clearPSData);
	pixelShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pixelShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&pixelShaderBufferDesc, NULL, &d3d.pipelines[PID_CLEAR].pixelBuffer, "clear pixel shader buffer");

	//
	// dynamic lights
	//

	D3D11_CreateVertexShader(g_dl_vs, ARRAY_LEN(g_dl_vs), NULL, &d3d.pipelines[PID_DYNAMIC_LIGHT].vertexShader, "dynamic light vertex shader");
	D3D11_CreatePixelShader(g_dl_ps, ARRAY_LEN(g_dl_ps), NULL, &d3d.pipelines[PID_DYNAMIC_LIGHT].pixelShader, "dynamic light pixel shader");

	D3D11_INPUT_ELEMENT_DESC dlInputLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	D3D11_CreateInputLayout(dlInputLayoutDesc, ARRAY_LEN(dlInputLayoutDesc), g_dl_vs, ARRAY_LEN(g_dl_vs), &d3d.pipelines[PID_DYNAMIC_LIGHT].inputLayout, "dynamic light input layout");

	ZeroMemory(&vertexShaderBufferDesc, sizeof(vertexShaderBufferDesc));
	vertexShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexShaderBufferDesc.ByteWidth = sizeof(DynamicLightVSData);
	vertexShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vertexShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&vertexShaderBufferDesc, NULL, &d3d.pipelines[PID_DYNAMIC_LIGHT].vertexBuffer, "dynamic light vertex shader buffer");

	ZeroMemory(&pixelShaderBufferDesc, sizeof(pixelShaderBufferDesc));
	pixelShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	pixelShaderBufferDesc.ByteWidth = sizeof(DynamicLightPSData);
	pixelShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pixelShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&pixelShaderBufferDesc, NULL, &d3d.pipelines[PID_DYNAMIC_LIGHT].pixelBuffer, "dynamic light pixel shader buffer");

	//
	// soft sprites
	//

	D3D11_CreateVertexShader(g_sprite_vs, ARRAY_LEN(g_sprite_vs), NULL, &d3d.pipelines[PID_SOFT_SPRITE].vertexShader, "soft sprite vertex shader");
	D3D11_CreatePixelShader(g_sprite_ps, ARRAY_LEN(g_sprite_ps), NULL, &d3d.pipelines[PID_SOFT_SPRITE].pixelShader, "soft sprite pixel shader");
	
	D3D11_INPUT_ELEMENT_DESC ssInputLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	D3D11_CreateInputLayout(ssInputLayoutDesc, ARRAY_LEN(ssInputLayoutDesc), g_sprite_vs, ARRAY_LEN(g_sprite_vs), &d3d.pipelines[PID_SOFT_SPRITE].inputLayout, "soft sprite input layout");
	
	ZeroMemory(&vertexShaderBufferDesc, sizeof(vertexShaderBufferDesc));
	vertexShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexShaderBufferDesc.ByteWidth = sizeof(DepthFadeVSData);
	vertexShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vertexShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&vertexShaderBufferDesc, NULL, &d3d.pipelines[PID_SOFT_SPRITE].vertexBuffer, "soft sprite vertex shader buffer");

	ZeroMemory(&pixelShaderBufferDesc, sizeof(pixelShaderBufferDesc));
	pixelShaderBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	pixelShaderBufferDesc.ByteWidth = sizeof(DepthFadePSData);
	pixelShaderBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pixelShaderBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_CreateBuffer(&pixelShaderBufferDesc, NULL, &d3d.pipelines[PID_SOFT_SPRITE].pixelBuffer, "soft sprite pixel shader buffer");

	//
	// mip-map generation
	//

	qbool mipGenOK = qfalse;
	if(r_gpuMipGen->integer && d3ds.device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0)
	{
		d3d.errorMode = EM_SILENT;

		mipGenOK = qtrue;
		mipGenOK &= D3D11_CreateComputeShader(g_mip_pass_cs, ARRAY_LEN(g_mip_pass_cs), NULL, &d3d.mipDownSampleComputeShader, "mip-map down-sampling compute shader");
		mipGenOK &= D3D11_CreateComputeShader(g_mip_start_cs, ARRAY_LEN(g_mip_start_cs), NULL, &d3d.mipGammaToLinearComputeShader, "gamma-to-linear compute shader");
		mipGenOK &= D3D11_CreateComputeShader(g_mip_end_cs, ARRAY_LEN(g_mip_end_cs), NULL, &d3d.mipLinearToGammaComputeShader, "linear-to-gamma compute shader");

		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(bufferDesc));
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.ByteWidth = sizeof(Down4CSData);
		mipGenOK &= D3D11_CreateBuffer(&bufferDesc, NULL, &d3d.mipDownSampleConstBuffer, "mip-map down-sampling compute shader buffer");
		bufferDesc.ByteWidth = sizeof(LinearToGammaCSData);
		mipGenOK &= D3D11_CreateBuffer(&bufferDesc, NULL, &d3d.mipLinearToGammaConstBuffer, "mip-map linear-to-gamma compute shader buffer");
		bufferDesc.ByteWidth = sizeof(GammaToLinearCSData);
		mipGenOK &= D3D11_CreateBuffer(&bufferDesc, NULL, &d3d.mipGammaToLinearConstBuffer, "mip-map gamma-to-linear compute shader buffer");

		for(int i = 0; i < ARRAY_LEN(d3d.mipGenTextures); ++i)
		{
			D3D11_TEXTURE2D_DESC textureDesc;
			ZeroMemory(&textureDesc, sizeof(textureDesc));
			textureDesc.Width = MAX_GPU_TEXTURE_SIZE;
			textureDesc.Height = MAX_GPU_TEXTURE_SIZE;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.Format = i == 2 ? DXGI_FORMAT_R8G8B8A8_UINT : DXGI_FORMAT_R16G16B16A16_FLOAT;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Usage = D3D11_USAGE_DEFAULT;
			textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			textureDesc.CPUAccessFlags = 0;
			textureDesc.MiscFlags = 0;
			mipGenOK &= D3D11_CreateTexture2D(&textureDesc, 0, &d3d.mipGenTextures[i].texture, va("mip-map generation texture #%d", i + 1));

			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			mipGenOK &= D3D11_CreateShaderResourceView(d3d.mipGenTextures[i].texture, &srvDesc, &d3d.mipGenTextures[i].srv, va("mip-map generation SRV #%d", i + 1));

			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			ZeroMemory(&uavDesc, sizeof(uavDesc));
			uavDesc.Format = textureDesc.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;
			mipGenOK &= D3D11_CreateUnorderedAccessView(d3d.mipGenTextures[i].texture, &uavDesc, &d3d.mipGenTextures[i].uav, va("mip-map generation SRV #%d", i + 1));
		}

		d3d.errorMode = EM_FATAL;
	}
	
	//
	// misc.
	//

	// select the generic pipeline to begin with
	d3d.pipelineIndex = (PipelineId)-1;
	ApplyPipeline(PID_GENERIC);

	// force set all the default non-sampler states
	d3d.blendStateIndex = -1;
	d3d.depthStencilStateIndex = -1;
	d3d.rasterStateIndex = -1;
	ApplyState(GLS_DEFAULT, CT_TWO_SIDED, qfalse);

	glConfig.colorBits = 32;
	glConfig.depthBits = 24;
	glConfig.stencilBits = 8;
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
	glConfig.extensions_string[0] = '\0';
	glConfig.renderer_string[0] = '\0';
	glConfig.vendor_string[0] = '\0';
	glConfig.version_string[0] = '\0';
	glInfo.displayFrequency = 0;
	glInfo.maxAnisotropy = D3D11_REQ_MAXANISOTROPY;	// @NOTE: D3D10_REQ_MAXANISOTROPY == D3D11_REQ_MAXANISOTROPY
	glInfo.maxTextureSize = MAX_GPU_TEXTURE_SIZE;
	glInfo.depthFadeSupport = r_depthFade->integer == 1;
	glInfo.mipGenSupport = mipGenOK;
	glInfo.alphaToCoverageSupport = alphaToCoverageOK;
	glInfo.msaaSampleCount = sampleDesc.Count;

	if(fullInit)
	{
		d3ds.adapterInfo.valid = qfalse;

		IDXGIDevice* dxgiDevice;
		if(SUCCEEDED(d3ds.device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice)))
		{
			IDXGIAdapter* dxgiAdapter;
			if(SUCCEEDED(dxgiDevice->GetAdapter(&dxgiAdapter)))
			{
				DXGI_ADAPTER_DESC desc;
				if(SUCCEEDED(dxgiAdapter->GetDesc(&desc)))
				{
					char name[ARRAY_LEN(desc.Description) + 1];
					if(WideCharToMultiByte(CP_UTF7, 0, desc.Description, -1, name, sizeof(name) - 1, NULL, NULL) > 0)
					{
						Q_strncpyz(glConfig.renderer_string, name, sizeof(glConfig.renderer_string));
					}

					d3ds.adapterInfo.valid = qtrue;
					d3ds.adapterInfo.dedicatedSystemMemoryMB = (int)(desc.DedicatedSystemMemory >> 20);
					d3ds.adapterInfo.dedicatedVideoMemoryMB = (int)(desc.DedicatedVideoMemory >> 20);
					d3ds.adapterInfo.sharedSystemMemoryMB = (int)(desc.SharedSystemMemory >> 20);
				}
			}

			COM_RELEASE(dxgiDevice);
		}
	}

	if(r_d3d11_syncOffsets->integer == D3D11SO_AUTO)
	{
#if 0
		// only nVidia's drivers seem to consistently handle the extra IASetVertexBuffers calls well enough
		d3d.splitBufferOffsets = Q_stristr(glConfig.renderer_string, "NVIDIA") != NULL;
#else
		// we'll just treat all drivers as equally bad for now and force synced offsets
		// it also seems that nVidia's new drivers might have a problem with the split offsets code path
		// (yet to be confirmed)
		d3d.splitBufferOffsets = qfalse;
#endif
	}
	else
	{
		d3d.splitBufferOffsets = r_d3d11_syncOffsets->integer == D3D11SO_SPLITOFFSETS;
	}

	return qtrue;
}

static void GAL_ShutDown(qbool fullShutDown)
{
	for(int i = 0; i < d3d.textureCount; ++i)
	{
		COM_RELEASE(d3d.textures[i].view);
		COM_RELEASE(d3d.textures[i].texture);
	}

	for(int i = 0; i < ARRAY_LEN(d3d.pipelines); ++i)
	{
		COM_RELEASE(d3d.pipelines[i].inputLayout);
		COM_RELEASE(d3d.pipelines[i].vertexShader);
		COM_RELEASE(d3d.pipelines[i].pixelShader);
		COM_RELEASE(d3d.pipelines[i].vertexBuffer);
		COM_RELEASE(d3d.pipelines[i].pixelBuffer);
	}

	for(int i = 0; i < ARRAY_LEN(d3d.mipGenTextures); ++i)
	{
		COM_RELEASE(d3d.mipGenTextures[i].texture);
		COM_RELEASE(d3d.mipGenTextures[i].srv);
		COM_RELEASE(d3d.mipGenTextures[i].uav);
	}

	for(int i = 0; i < ARRAY_LEN(d3d.vertexBuffers); ++i)
	{
		COM_RELEASE(d3d.vertexBuffers[i].buffer);
	}
	COM_RELEASE(d3d.indexBuffer.buffer);

	COM_RELEASE_ARRAY(d3d.samplerStates);
	COM_RELEASE_ARRAY(d3d.blendStates);
	COM_RELEASE_ARRAY(d3d.depthStencilStates);
	COM_RELEASE_ARRAY(d3d.rasterStates);

	COM_RELEASE(d3d.backBufferTexture);
	COM_RELEASE(d3d.backBufferRTView);
	COM_RELEASE(d3d.renderTargetTextureMS);
	COM_RELEASE(d3d.renderTargetViewMS);
	COM_RELEASE(d3d.resolveTexture);
	COM_RELEASE(d3d.resolveTextureShaderView);
	COM_RELEASE(d3d.depthStencilTexture);
	COM_RELEASE(d3d.depthStencilView);
	COM_RELEASE(d3d.depthStencilShaderView);
	COM_RELEASE(d3d.readbackTexture);
	COM_RELEASE(d3d.screenshotTexture);
	COM_RELEASE(d3d.screenshotTextureRTView);
	COM_RELEASE(d3d.mipGammaToLinearComputeShader);
	COM_RELEASE(d3d.mipLinearToGammaComputeShader);
	COM_RELEASE(d3d.mipDownSampleComputeShader);
	COM_RELEASE(d3d.mipDownSampleConstBuffer);
	COM_RELEASE(d3d.mipLinearToGammaConstBuffer);
	COM_RELEASE(d3d.mipGammaToLinearConstBuffer);

	for(int i = 0; i < ARRAY_LEN(d3d.frameQueries); ++i)
	{
		COM_RELEASE(d3d.frameQueries[i].disjoint);
		COM_RELEASE(d3d.frameQueries[i].frameStart);
		COM_RELEASE(d3d.frameQueries[i].frameEnd);
	}

	if(fullShutDown)
	{
#if defined(_DEBUG)
		// DXGIGetDebugInterface would be nicer but it requires Windows 8...
		// It doesn't reference the device, so the device doesn't show up as a false positive.
		ID3D11Debug* debug = NULL;
		const HRESULT debugQuery = d3ds.device->QueryInterface(IID_PPV_ARGS(&debug));
#endif

		d3ds.context->Release();
		d3ds.device->Release();
		d3ds.swapChain->Release();

#if defined(_DEBUG)
		OutputDebugStringA("================================================================\n");
		if(SUCCEEDED(debugQuery))
		{
			OutputDebugStringA("Summary\n");
			debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
			OutputDebugStringA("================================================================\n");
			OutputDebugStringA("Details\n");
			debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
			debug->Release();
		}
		else
		{
			OutputDebugStringA("ID3D11Device::QueryInterface of ID3D11Debug failed!\n");
			OutputDebugStringA(va("%s\n", GetSystemErrorString(debugQuery)));
		}
		OutputDebugStringA("================================================================\n");
#endif

		if(d3ds.library != NULL)
			FreeLibrary(d3ds.library);

		memset(&d3ds, 0, sizeof(d3ds));
	}

	memset(&d3d, 0, sizeof(d3d));

	tr.numImages = 0;
	memset(tr.images, 0, sizeof(tr.images));
}

static void BeginQueries()
{
	FrameQueries* const queries = &d3d.frameQueries[d3d.frameQueriesWriteIndex];
	queries->valid = qfalse;
	COM_RELEASE(queries->disjoint);
	COM_RELEASE(queries->frameStart);
	COM_RELEASE(queries->frameEnd);

	D3D11_QUERY_DESC qd;
	qd.MiscFlags = 0;
	qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	d3ds.device->CreateQuery(&qd, &queries->disjoint);
	qd.Query = D3D11_QUERY_TIMESTAMP;
	d3ds.device->CreateQuery(&qd, &queries->frameStart);
	d3ds.device->CreateQuery(&qd, &queries->frameEnd);
	if(queries->disjoint != NULL &&
	   queries->frameStart != NULL &&
	   queries->frameEnd != NULL)
	{
		queries->valid = qtrue;
		d3ds.context->Begin(queries->disjoint);
		d3ds.context->End(queries->frameStart);
	}
	else
	{
		COM_RELEASE(queries->disjoint);
		COM_RELEASE(queries->frameStart);
		COM_RELEASE(queries->frameEnd);
	}
}

static void EndQueries()
{
	// finish this frame
	FrameQueries* queries = &d3d.frameQueries[d3d.frameQueriesWriteIndex];
	if(queries->valid)
	{
		d3ds.context->End(queries->frameEnd);
		d3ds.context->End(queries->disjoint);
		d3d.frameQueriesWriteIndex = (d3d.frameQueriesWriteIndex + 1) % ARRAY_LEN(d3d.frameQueries);
	}

	// try to grab a previous frame's results
	D3D10_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = { 0 };
	backEnd.pc3D[RB_USEC_GPU] = 0; // pessimism...
	queries = &d3d.frameQueries[d3d.frameQueriesReadIndex];
	if(queries->valid &&
	   d3ds.context->GetData(queries->disjoint, &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
	{
		UINT64 start = 0;
		UINT64 end = 0;
		if(!disjoint.Disjoint &&
		   disjoint.Frequency > 0 &&
		   d3ds.context->GetData(queries->frameStart, &start, sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
		   d3ds.context->GetData(queries->frameEnd, &end, sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
		{
			backEnd.pc3D[RB_USEC_GPU] = int(((end - start) * UINT64(1000000)) / disjoint.Frequency);
		}
		d3d.frameQueriesReadIndex = (d3d.frameQueriesReadIndex + 1) % ARRAY_LEN(d3d.frameQueries);
	}
}

static void GAL_BeginFrame()
{
	BeginQueries();

	d3d.frameSeed = (float)rand() / (float)RAND_MAX;

	const FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const FLOAT clearColorDebug[4] = { 1.0f, 0.0f, 0.5f, 1.0f };
	d3ds.context->ClearRenderTargetView(d3d.renderTargetViewMS, r_clear->integer ? clearColorDebug : clearColor);
	d3ds.context->ClearDepthStencilView(d3d.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	ApplyPipeline(PID_GENERIC);
	ApplyViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight, glConfig.vidHeight);
}

static void DrawPostProcess(float vsX, float vsY, float srX, float srY, qbool screenshot)
{
	d3d.postPSData.gamma = 1.0f / r_gamma->value;
	d3d.postPSData.brightness = r_brightness->value;
	d3d.postPSData.greyscale = r_greyscale->value;
	d3d.postVSData.scaleX = vsX;
	d3d.postVSData.scaleY = vsY;
	ApplyPipeline(screenshot ? PID_SCREENSHOT : PID_POST_PROCESS);
	ApplyState(GLS_DEPTHTEST_DISABLE, CT_TWO_SIDED, qfalse);
	UploadPendingShaderData();
	BindImage(0, tr.whiteImage);
	ApplyPixelShaderResource(0, d3d.resolveTextureShaderView);
	ApplySamplerState(0, TW_CLAMP_TO_EDGE, TM_BILINEAR);
	if(screenshot)
	{
		ApplyViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight, glConfig.vidHeight);
	}
	else
	{
		if(vsX < 1.0f || vsY < 1.0f)
		{
			const int x = (glInfo.winWidth - glInfo.winWidth * vsX) / 2.0f;
			const int y = (glInfo.winHeight - glInfo.winHeight * vsY) / 2.0f;
			ApplyViewport(0, 0, glInfo.winWidth, glInfo.winHeight, glInfo.winHeight);
			ApplyScissor(x, y, glConfig.vidWidth * srX, glConfig.vidHeight * srY, glInfo.winHeight);
		}
		else
		{
			ApplyViewportAndScissor(0, 0, glInfo.winWidth, glInfo.winHeight, glInfo.winHeight);
		}
	}
	d3ds.context->Draw(3, 0);
	backEnd.pc3D[RB_DRAW_CALLS]++;
}

static void GAL_EndFrame()
{
	float vsX = 1.0f; // vertex shader scale factors
	float vsY = 1.0f;
	float srX = 1.0f; // scissor rectangle scale factors
	float srY = 1.0f;
	if(r_fullscreen->integer == 1 && r_mode->integer == VIDEOMODE_UPSCALE)
	{
		if(r_blitMode->integer == BLITMODE_CENTERED)
		{
			vsX = (float)glConfig.vidWidth / (float)glInfo.winWidth;
			vsY = (float)glConfig.vidHeight / (float)glInfo.winHeight;
		}
		else if(r_blitMode->integer == BLITMODE_ASPECT)
		{
			const float ars = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
			const float ard = (float)glInfo.winWidth / (float)glInfo.winHeight;
			if(ard > ars)
			{
				vsX = ars / ard;
				vsY = 1.0f;
				srX = (float)glInfo.winHeight / (float)glConfig.vidHeight;
				srY = srX;
			}
			else
			{
				vsX = 1.0f;
				vsY = ard / ars;
				srX = (float)glInfo.winWidth / (float)glConfig.vidWidth;
				srY = srX;
			}
		}

		if(vsX != 1.0f || vsY != 1.0f)
		{
			const FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			d3ds.context->ClearRenderTargetView(d3d.backBufferRTView, clearColor);
		}
	}

	d3ds.context->ResolveSubresource(d3d.resolveTexture, 0, d3d.renderTargetTextureMS, 0, d3d.formatColorRT);
	DrawPostProcess(vsX, vsY, srX, srY, qfalse);

	EndQueries();

	const UINT presentFlags = d3ds.flipAndTear && r_swapInterval->integer == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0;
	const HRESULT hr = d3ds.swapChain->Present(abs(r_swapInterval->integer), presentFlags);

	enum PresentError
	{
		PE_NONE,
		PE_DEVICE_REMOVED,
		PE_DEVICE_RESET
	};
	PresentError presentError = PE_NONE;
	HRESULT deviceRemovedReason = S_OK;
	if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == D3DDDIERR_DEVICEREMOVED)
	{
		deviceRemovedReason = d3ds.device->GetDeviceRemovedReason();
		if(deviceRemovedReason == DXGI_ERROR_DEVICE_RESET)
		{
			presentError = PE_DEVICE_RESET;
		}
		else
		{
			presentError = PE_DEVICE_REMOVED;
		}
	}
	else if(hr == DXGI_ERROR_DEVICE_RESET)
	{
		presentError = PE_DEVICE_RESET;
	}

	if(presentError == PE_DEVICE_REMOVED)
	{
		ri.Error(ERR_FATAL, "Direct3D device was removed! Reason: %s", GetDeviceRemovedReasonString(deviceRemovedReason));
	}
	else if(presentError == PE_DEVICE_RESET)
	{
		ri.Printf(PRINT_ERROR, "Direct3D device was reset! Restarting the video system...");
		Cmd_ExecuteString("vid_restart;");
	}
}

static void GAL_BeginSkyAndClouds(double depth)
{
	const float clipPlane[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	memcpy(d3d.oldSkyClipPlane, d3d.clipPlane, sizeof(d3d.oldSkyClipPlane));
	memcpy(d3d.clipPlane, clipPlane, sizeof(d3d.clipPlane));

	UINT numVP = 1;
	d3ds.context->RSGetViewports(&numVP, &d3d.oldSkyViewport);
	d3d.oldSkyViewport.MinDepth = (FLOAT)depth;
	d3d.oldSkyViewport.MaxDepth = (FLOAT)depth;
	d3ds.context->RSSetViewports(1, &d3d.oldSkyViewport);
}

static void GAL_EndSkyAndClouds()
{
	d3d.oldSkyViewport.MinDepth = 0.0f;
	d3d.oldSkyViewport.MaxDepth = 1.0f;
	d3ds.context->RSSetViewports(1, &d3d.oldSkyViewport);

	memcpy(d3d.clipPlane, d3d.oldSkyClipPlane, sizeof(d3d.clipPlane));
}

static void WriteInvalidImage(int w, int h, int alignment, colorSpace_t colorSpace, void* out)
{
	if(colorSpace == CS_RGBA)
		memset(out, 0x7F, PAD(w * 4, alignment) * h);
	else if(colorSpace == CS_BGR)
		memset(out, 0x7F, PAD(w * 3, alignment) * h);
}

static void GAL_ReadPixels(int, int, int w, int h, int alignment, colorSpace_t colorSpace, void* out)
{
	if(d3d.readbackTexture == NULL)
	{
		WriteInvalidImage(w, h, alignment, colorSpace, out);
		return;
	}

	if(r_mode->integer != VIDEOMODE_UPSCALE)
	{
		// matching dimensions means we can copy the data directly from the back buffer
		d3ds.context->CopyResource(d3d.readbackTexture, d3d.backBufferTexture);
	}
	else
	{
		if(d3d.screenshotTexture == NULL || d3d.screenshotTextureRTView == NULL)
		{
			WriteInvalidImage(w, h, alignment, colorSpace, out);
			return;
		}

		// we render the post-process pass into an intermediate texture and
		// copy its content into the readback texture
		DrawPostProcess(1.0f, 1.0f, 1.0f, 1.0f, qtrue);
		d3ds.context->CopyResource(d3d.readbackTexture, d3d.screenshotTexture);
	}

	D3D11_MAPPED_SUBRESOURCE ms;
	HRESULT hr = d3ds.context->Map(d3d.readbackTexture, 0, D3D11_MAP_READ, NULL, &ms);
	if(FAILED(hr))
	{
		WriteInvalidImage(w, h, alignment, colorSpace, out);
		return;
	}

	if(colorSpace == CS_RGBA)
	{
		const byte* srcRow = (const byte*)ms.pData;
		byte* dstRow = (byte*)out + PAD(w * 4, alignment) * (h - 1);
		for(int y = 0; y < h; ++y)
		{
			const byte* s = srcRow;
			byte* d = dstRow;
			for(int x = 0; x < w; ++x)
			{
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				d[3] = 255;
				d += 4;
				s += 4;
			}

			srcRow += ms.RowPitch;
			dstRow -= PAD(w * 4, alignment);
		}
	}
	else if(colorSpace == CS_BGR)
	{
		const byte* srcRow = (const byte*)ms.pData;
		byte* dstRow = (byte*)out + PAD(w * 3, alignment) * (h - 1);
		for(int y = 0; y < h; ++y)
		{
			const byte* s = srcRow;
			byte* d = dstRow;
			for(int x = 0; x < w; ++x)
			{
				d[2] = s[0];
				d[1] = s[1];
				d[0] = s[2];
				d += 3;
				s += 4;
			}

			srcRow += ms.RowPitch;
			dstRow -= PAD(w * 3, alignment);
		}
	}

	d3ds.context->Unmap(d3d.readbackTexture, NULL);
}

static void GAL_CreateTexture(image_t* image, int mipCount, int w, int h)
{
	if(d3d.textureCount >= ARRAY_LEN(d3d.textures))
		ri.Error(ERR_FATAL, "Too many textures allocated for the Direct3D 11 back-end");

	CreateTexture(&d3d.textures[d3d.textureCount], image, mipCount, w, h);
	image->texnum = d3d.textureCount++;
}

static void GAL_UpdateTexture(image_t* image, int mip, int x, int y, int w, int h, const void* data)
{
	ID3D11Texture2D* texture = d3d.textures[image->texnum].texture;
	if(texture == NULL)
	{
		return;
	}

	const int rowBytes = image->format == TF_RGBA8 ? (w * 4) : w;
	const int imageBytes = rowBytes * h;
	D3D11_BOX box;
	box.front = 0;
	box.back = 1;
	box.left = x;
	box.right = x + w;
	box.top = y;
	box.bottom = y + h;
	d3ds.context->UpdateSubresource(texture, mip, &box, data, rowBytes, imageBytes);
}

static void GAL_UpdateScratch(image_t* image, int w, int h, const void* data, qbool dirty)
{
	if(image->texnum <= 0 || image->texnum > ARRAY_LEN(d3d.textures))
	{
		return;
	}

	if(w != image->width || h != image->height)
	{
		image->width = w;
		image->height = h;
		CreateTexture(&d3d.textures[image->texnum], image, 1, w, h);
		GAL_UpdateTexture(image, 0, 0, 0, w, h, data);
	}
	else if(dirty)
	{
		GAL_UpdateTexture(image, 0, 0, 0, w, h, data);
	}
}

static void GAL_CreateTextureEx(image_t* image, int mipCount, int mipOffset, int w, int h, const void* mip0)
{
	enum { GroupSize = 8, GroupMask = GroupSize - 1 };

	// needed so we don't bind a resource that's already bound
	ID3D11ShaderResourceView* const srvNull = NULL;
	ID3D11UnorderedAccessView* const uavNull = NULL;
	ID3D11Buffer* const bufferNull = NULL;

	GAL_CreateTexture(image, mipCount - mipOffset, image->width, image->height);
	const Texture* const texture = &d3d.textures[image->texnum];

	// upload source mip 0
	const int rowBytes = w * 4;
	const int imageBytes = rowBytes * h;
	D3D11_BOX box;
	box.front = 0;
	box.back = 1;
	box.left = 0;
	box.right = w;
	box.top = 0;
	box.bottom = h;
	d3ds.context->UpdateSubresource(d3d.mipGenTextures[2].texture, 0, &box, mip0, rowBytes, imageBytes);

	GammaToLinearCSData dataG2L;
	dataG2L.gamma = r_mipGenGamma->value;

	// create a linear color space copy of source mip 0
	int readIndex = 2;
	int writeIndex = 0;
	ResetShaderData(d3d.mipGammaToLinearConstBuffer, &dataG2L, sizeof(dataG2L));
	d3ds.context->CSSetShader(d3d.mipGammaToLinearComputeShader, NULL, 0);
	d3ds.context->CSSetConstantBuffers(0, 1, &bufferNull);
	d3ds.context->CSSetShaderResources(0, 1, &srvNull);
	d3ds.context->CSSetUnorderedAccessViews(0, 1, &uavNull, NULL);
	d3ds.context->CSSetConstantBuffers(0, 1, &d3d.mipGammaToLinearConstBuffer);
	d3ds.context->CSSetShaderResources(0, 1, &d3d.mipGenTextures[readIndex].srv);
	d3ds.context->CSSetUnorderedAccessViews(0, 1, &d3d.mipGenTextures[writeIndex].uav, NULL);
	d3ds.context->Dispatch((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);

	LinearToGammaCSData dataL2G;
	dataL2G.intensity = r_intensity->value;
	dataL2G.invGamma = 1.0f / r_mipGenGamma->value;

	// copy to destination mip 0 now if needed
	if(mipOffset == 0)
	{
		readIndex = 0;
		writeIndex = 2;
		memcpy(dataL2G.blendColor, r_mipBlendColors[0], sizeof(dataL2G.blendColor));
		ResetShaderData(d3d.mipLinearToGammaConstBuffer, &dataL2G, sizeof(dataL2G));
		d3ds.context->CSSetShader(d3d.mipLinearToGammaComputeShader, NULL, 0);
		d3ds.context->CSSetConstantBuffers(0, 1, &bufferNull);
		d3ds.context->CSSetShaderResources(0, 1, &srvNull);
		d3ds.context->CSSetUnorderedAccessViews(0, 1, &uavNull, NULL);
		d3ds.context->CSSetConstantBuffers(0, 1, &d3d.mipLinearToGammaConstBuffer);
		d3ds.context->CSSetShaderResources(0, 1, &d3d.mipGenTextures[readIndex].srv);
		d3ds.context->CSSetUnorderedAccessViews(0, 1, &d3d.mipGenTextures[writeIndex].uav, NULL);
		d3ds.context->Dispatch((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);

		box.front = 0;
		box.back = 1;
		box.left = 0;
		box.right = w;
		box.top = 0;
		box.bottom = h;
		d3ds.context->CopySubresourceRegion(texture->texture, 0, 0, 0, 0, d3d.mipGenTextures[2].texture, 0, &box);
	}

	Down4CSData dataDown;
	memcpy(dataDown.weights, tr.mipFilter, sizeof(dataDown.weights));
	dataDown.clampMode = image->wrapClampMode == TW_REPEAT ? 0 : 1;

	for(int i = 1; i < mipCount; ++i)
	{
		const int w1 = w;
		const int h1 = h;
		w = max(w / 2, 1);
		h = max(h / 2, 1);
		
		// down-sample on the X-axis
		readIndex = 0;
		writeIndex = 1;
		dataDown.scale[0] = w1 / w;
		dataDown.scale[1] = 1;
		dataDown.maxSize[0] = w1 - 1;
		dataDown.maxSize[1] = h1 - 1;
		dataDown.offset[0] = 1;
		dataDown.offset[1] = 0;
		ResetShaderData(d3d.mipDownSampleConstBuffer, &dataDown, sizeof(dataDown));
		d3ds.context->CSSetShader(d3d.mipDownSampleComputeShader, NULL, 0);
		d3ds.context->CSSetConstantBuffers(0, 1, &bufferNull);
		d3ds.context->CSSetShaderResources(0, 1, &srvNull);
		d3ds.context->CSSetUnorderedAccessViews(0, 1, &uavNull, NULL);
		d3ds.context->CSSetConstantBuffers(0, 1, &d3d.mipDownSampleConstBuffer);
		d3ds.context->CSSetShaderResources(0, 1, &d3d.mipGenTextures[readIndex].srv);
		d3ds.context->CSSetUnorderedAccessViews(0, 1, &d3d.mipGenTextures[writeIndex].uav, NULL);
		d3ds.context->Dispatch((w + GroupMask) / GroupSize, (h1 + GroupMask) / GroupSize, 1);

		// down-sample on the Y-axis
		readIndex = 1;
		writeIndex = 0;
		dataDown.scale[0] = 1;
		dataDown.scale[1] = h1 / h;
		dataDown.maxSize[0] = w - 1;
		dataDown.maxSize[1] = h1 - 1;
		dataDown.offset[0] = 0;
		dataDown.offset[1] = 1;
		ResetShaderData(d3d.mipDownSampleConstBuffer, &dataDown, sizeof(dataDown));
		d3ds.context->CSSetShaderResources(0, 1, &srvNull);
		d3ds.context->CSSetUnorderedAccessViews(0, 1, &uavNull, NULL);
		d3ds.context->CSSetShaderResources(0, 1, &d3d.mipGenTextures[readIndex].srv);
		d3ds.context->CSSetUnorderedAccessViews(0, 1, &d3d.mipGenTextures[writeIndex].uav, NULL);
		d3ds.context->Dispatch((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);

		const int destMip = i - mipOffset;
		if(destMip >= 0)
		{
			// convert to final format
			readIndex = 0;
			writeIndex = 2;
			memcpy(dataL2G.blendColor, r_mipBlendColors[r_colorMipLevels->integer ? destMip : 0], sizeof(dataL2G.blendColor));
			ResetShaderData(d3d.mipLinearToGammaConstBuffer, &dataL2G, sizeof(dataL2G));
			d3ds.context->CSSetShader(d3d.mipLinearToGammaComputeShader, NULL, 0);
			d3ds.context->CSSetConstantBuffers(0, 1, &bufferNull);
			d3ds.context->CSSetShaderResources(0, 1, &srvNull);
			d3ds.context->CSSetUnorderedAccessViews(0, 1, &uavNull, NULL);
			d3ds.context->CSSetConstantBuffers(0, 1, &d3d.mipLinearToGammaConstBuffer);
			d3ds.context->CSSetShaderResources(0, 1, &d3d.mipGenTextures[readIndex].srv);
			d3ds.context->CSSetUnorderedAccessViews(0, 1, &d3d.mipGenTextures[writeIndex].uav, NULL);
			d3ds.context->Dispatch((w + GroupMask) / GroupSize, (h + GroupMask) / GroupSize, 1);

			// write out the result
			box.front = 0;
			box.back = 1;
			box.left = 0;
			box.right = w;
			box.top = 0;
			box.bottom = h;
			d3ds.context->CopySubresourceRegion(texture->texture, destMip, 0, 0, 0, d3d.mipGenTextures[2].texture, 0, &box);
		}
	}
}

static void DrawGeneric()
{
	AppendVertexData(&d3d.indexBuffer, tess.indexes, tess.numIndexes);
	if(d3d.splitBufferOffsets)
	{
		AppendVertexData(&d3d.vertexBuffers[VB_POSITION], tess.xyz, tess.numVertexes);
	}

	for(int i = 0; i < tess.shader->numStages; ++i)
	{
		const shaderStage_t* stage = tess.xstages[i];

		if(d3d.splitBufferOffsets)
		{
			AppendVertexData(&d3d.vertexBuffers[VB_TEXCOORD], tess.svars[i].texcoordsptr, tess.numVertexes);
			AppendVertexData(&d3d.vertexBuffers[VB_COLOR], tess.svars[i].colors, tess.numVertexes);
			if(stage->mtStages == 1)
			{
				AppendVertexData(&d3d.vertexBuffers[VB_TEXCOORD2], tess.svars[i + 1].texcoordsptr, tess.numVertexes);
			}
		}
		else
		{
			const void* pointers[VB_COUNT];
			pointers[VB_POSITION] = tess.xyz;
			pointers[VB_NORMAL] = NULL;
			pointers[VB_TEXCOORD] = tess.svars[i].texcoordsptr;
			pointers[VB_TEXCOORD2] = stage->mtStages == 1 ? tess.svars[i + 1].texcoordsptr : NULL;
			pointers[VB_COLOR] = tess.svars[i].colors;
			AppendVertexDataGroup(pointers, tess.numVertexes);
		}

		ApplyState(stage->stateBits, tess.shader->cullType, tess.shader->polygonOffset);

		BindBundle(0, &stage->bundle);

		if(stage->mtStages == 1)
		{
			const shaderStage_t* stage2 = tess.xstages[i + 1];
			d3d.texEnv = stage2->mtEnv;
			BindBundle(1, &stage2->bundle);
			i += 1;
		}
		else
		{
			BindImage(1, tr.whiteImage);
			d3d.texEnv = TE_DISABLED;
		}

		UploadPendingShaderData();

		DrawIndexed(tess.numIndexes);
	}

	if(tess.drawFog)
	{
		if(d3d.splitBufferOffsets)
		{
			AppendVertexData(&d3d.vertexBuffers[VB_TEXCOORD], tess.svarsFog.texcoordsptr, tess.numVertexes);
			AppendVertexData(&d3d.vertexBuffers[VB_COLOR], tess.svarsFog.colors, tess.numVertexes);
		}
		else
		{
			const void* pointers[VB_COUNT];
			pointers[VB_POSITION] = tess.xyz;
			pointers[VB_NORMAL] = NULL;
			pointers[VB_TEXCOORD] = tess.svarsFog.texcoordsptr;
			pointers[VB_TEXCOORD2] = NULL;
			pointers[VB_COLOR] = tess.svarsFog.colors;
			AppendVertexDataGroup(pointers, tess.numVertexes);
		}

		ApplyState(tess.fogStateBits, tess.shader->cullType, tess.shader->polygonOffset);

		BindImage(0, tr.fogImage);
		BindImage(1, tr.whiteImage);

		d3d.texEnv = TE_DISABLED;
		UploadPendingShaderData();

		DrawIndexed(tess.numIndexes);
	}
}

static void DrawDynamicLight()
{
	const int stageIndex = tess.shader->lightingStages[ST_DIFFUSE];
	const shaderStage_t* stage = tess.xstages[stageIndex];

	AppendVertexData(&d3d.indexBuffer, tess.dlIndexes, tess.dlNumIndexes);
	if(d3d.splitBufferOffsets)
	{
		AppendVertexData(&d3d.vertexBuffers[VB_POSITION], tess.xyz, tess.numVertexes);
		AppendVertexData(&d3d.vertexBuffers[VB_NORMAL], tess.normal, tess.numVertexes);
		AppendVertexData(&d3d.vertexBuffers[VB_TEXCOORD], tess.svars[stageIndex].texcoordsptr, tess.numVertexes);
	}
	else
	{
		const void* pointers[VB_COUNT];
		pointers[VB_POSITION] = tess.xyz;
		pointers[VB_NORMAL] = tess.normal;
		pointers[VB_TEXCOORD] = tess.svars[stageIndex].texcoordsptr;
		pointers[VB_TEXCOORD2] = NULL;
		pointers[VB_COLOR] = NULL;
		AppendVertexDataGroup(pointers, tess.numVertexes);
	}

	ApplyState(backEnd.dlStateBits, tess.shader->cullType, tess.shader->polygonOffset);
	BindBundle(0, &stage->bundle);

	UploadPendingShaderData();

	DrawIndexed(tess.dlNumIndexes);
}

static void DrawDepthFade()
{
	AppendVertexData(&d3d.indexBuffer, tess.indexes, tess.numIndexes);
	if(d3d.splitBufferOffsets)
	{
		AppendVertexData(&d3d.vertexBuffers[VB_POSITION], tess.xyz, tess.numVertexes);
	}

	for(int i = 0; i < tess.shader->numStages; ++i)
	{
		const shaderStage_t* stage = tess.xstages[i];

		if(d3d.splitBufferOffsets)
		{
			AppendVertexData(&d3d.vertexBuffers[VB_TEXCOORD], tess.svars[i].texcoordsptr, tess.numVertexes);
			AppendVertexData(&d3d.vertexBuffers[VB_COLOR], tess.svars[i].colors, tess.numVertexes);
		}
		else
		{
			const void* pointers[VB_COUNT];
			pointers[VB_POSITION] = tess.xyz;
			pointers[VB_NORMAL] = NULL;
			pointers[VB_TEXCOORD] = tess.svars[i].texcoordsptr;
			pointers[VB_TEXCOORD2] = NULL;
			pointers[VB_COLOR] = tess.svars[i].colors;
			AppendVertexDataGroup(pointers, tess.numVertexes);
		}

		ApplyState(stage->stateBits, tess.shader->cullType, tess.shader->polygonOffset);

		BindBundle(0, &stage->bundle);

		UploadPendingShaderData();

		DrawIndexed(tess.numIndexes);
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

static void GAL_Begin2D()
{
	R_MakeIdentityMatrix(d3d.modelViewMatrix);
	R_MakeOrthoProjectionMatrix(d3d.projectionMatrix, glConfig.vidWidth, glConfig.vidHeight);
	ApplyViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight, glConfig.vidHeight);
	ApplyState(GLS_DEFAULT_2D, CT_TWO_SIDED, qfalse);
}

static void ClearViews(qbool shouldClearColor, const FLOAT* clearColor)
{
	// Direct3D 11.1 does provide support for partial clears for color and depth-only views.
	// However, depth/stencil views are not supported so we can't use that right now.
	// Getting rid of the stencil buffer is definitely on the cards.

	const qbool fullClear =
		backEnd.viewParms.viewportX == 0 &&
		backEnd.viewParms.viewportY == 0 &&
		backEnd.viewParms.viewportWidth == glConfig.vidWidth &&
		backEnd.viewParms.viewportHeight == glConfig.vidHeight;

	if(fullClear)
	{
		d3ds.context->ClearDepthStencilView(d3d.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		if(shouldClearColor)
		{
			d3ds.context->ClearRenderTargetView(d3d.renderTargetViewMS, clearColor);
		}
	}
	else
	{
		const unsigned int stateBits =
			GLS_DEPTHMASK_TRUE | GLS_DEPTHFUNC_ALWAYS |
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		ApplyPipeline(PID_CLEAR);
		ApplyState(stateBits, CT_TWO_SIDED, qfalse);
		d3d.clearPSData.color[0] = clearColor[0];
		d3d.clearPSData.color[1] = clearColor[1];
		d3d.clearPSData.color[2] = clearColor[2];
		d3d.clearPSData.color[3] = shouldClearColor ? 1.0f : 0.0f;
		UploadPendingShaderData();
		d3ds.context->Draw(3, 0);
		backEnd.pc3D[RB_DRAW_CALLS]++;
		ApplyPipeline(PID_GENERIC);
	}
}

static void GAL_Begin3D()
{
	ApplyPipeline(PID_GENERIC);
	memcpy(d3d.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(d3d.projectionMatrix));
	ApplyViewportAndScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, glConfig.vidHeight);

	qbool shouldClearColor = qfalse;
	FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	if(backEnd.refdef.rdflags & RDF_HYPERSPACE)
	{
		const FLOAT c = RB_HyperspaceColor();
		clearColor[0] = c;
		clearColor[1] = c;
		clearColor[2] = c;
		shouldClearColor = qtrue;
	}
	else if(r_fastsky->integer && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		shouldClearColor = qtrue;
	}
	ClearViews(shouldClearColor, clearColor);

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
		o[0] = m[0] * v[0] + m[4] * v[1] + m[ 8] * v[2] + m[12] * v[3];
		o[1] = m[1] * v[0] + m[5] * v[1] + m[ 9] * v[2] + m[13] * v[3];
		o[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
		o[3] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];

		memcpy(d3d.clipPlane, plane, sizeof(d3d.clipPlane));
	}
	else
	{
		const float clipPlane[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		memcpy(d3d.clipPlane, clipPlane, sizeof(d3d.clipPlane));
	}

	ApplyState(GLS_DEFAULT, CT_TWO_SIDED, qfalse);
}

static void GAL_SetModelViewMatrix(const float* matrix)
{
	memcpy(d3d.modelViewMatrix, matrix, sizeof(d3d.modelViewMatrix));
}

static void GAL_SetDepthRange(double near, double far)
{
	D3D11_VIEWPORT viewport;
	UINT numVP = 1;
	d3ds.context->RSGetViewports(&numVP, &viewport);

	viewport.MinDepth = (float)near;
	viewport.MaxDepth = (float)far;
	d3ds.context->RSSetViewports(1, &viewport);
}

static void GAL_BeginDynamicLight()
{
	const dlight_t* const dl = tess.light;

	d3d.osEyePos[0] = backEnd.orient.viewOrigin[0];
	d3d.osEyePos[1] = backEnd.orient.viewOrigin[1];
	d3d.osEyePos[2] = backEnd.orient.viewOrigin[2];
	d3d.osEyePos[3] = 1.0f;
	d3d.osLightPos[0] = dl->transformed[0];
	d3d.osLightPos[1] = dl->transformed[1];
	d3d.osLightPos[2] = dl->transformed[2];
	d3d.osLightPos[3] = 1.0f;
	d3d.lightColor[0] = dl->color[0];
	d3d.lightColor[1] = dl->color[1];
	d3d.lightColor[2] = dl->color[2];
	d3d.lightRadius = 1.0f / Square(dl->radius);
}

static void GAL_PrintInfo()
{
	ri.Printf(PRINT_ALL, "Direct3D device feature level: %s\n", d3ds.device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0 ? "11.0" : "10.1");
	ri.Printf(PRINT_ALL, "Direct3D vertex buffer upload strategy: %s\n", d3d.splitBufferOffsets ? "split offsets" : "sync'd offsets");
	ri.Printf(PRINT_ALL, "DXGI presentation model: %s\n", d3ds.flipAndTear ? "flip + discard" : "blit + discard");
	if(d3ds.adapterInfo.valid)
	{
		ri.Printf(PRINT_ALL, "%6d MB of dedicated GPU memory\n", d3ds.adapterInfo.dedicatedVideoMemoryMB);
		ri.Printf(PRINT_ALL, "%6d MB of shared system memory\n", d3ds.adapterInfo.sharedSystemMemoryMB);
		ri.Printf(PRINT_ALL, "%6d MB of dedicated system memory\n", d3ds.adapterInfo.dedicatedSystemMemoryMB);
	}
}

qbool GAL_GetD3D11(graphicsAPILayer_t* rb)
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


#else


#include "tr_local.h"


qbool GAL_GetD3D11(graphicsAPILayer_t* rb)
{
	return qfalse;
}


#endif
