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
// Direct3D 12 Rendering Hardware Interface


/*
to do:

- use ID3D12DebugCommandList::AssertResourceState
- git cherry-pick 693415a6e2f2f3789215ec037b15d505c5132fd4
- git cherry-pick c75b2b27fa936854d27dadf458e3ec3b03829561
- https://asawicki.info/news_1758_an_idea_for_visualization_of_frame_times
- GPU resident vertex data for models: load on demand based on submitted { surface, shader } pairs
- use Application Verifier to catch issues
- tone mapping: look at https://github.com/h3r2tic/tony-mc-mapface
- ubershader PS: run-time alpha test evaluation to reduce PSO count?
- when creating the root signature, validate that neither of the tables have any gap
- use root signature 1.1 to use the hints that help the drivers optimize out static resources
- is it possible to force Resource Binding Tier 2 somehow? are we supposed to run on old HW to test? :(
	see if WARP allows us to do that?
- don't do persistent mapping to help out RenderDoc?
- Intel GPU: try storing only 6*2 samplers instead of 6*16: 1 set for no mip bias and 1 set for r_picmip
- Intel GPU: evaluate the benefit of static samplers
- defragment on partial inits with D3D12MA?
- use ID3D12Device4::CreateCommandList1 to create closed command lists
- move UI to the uber shader system, tessellate to generate proper data
- share structs between HLSL and C++ with .hlsli files -> change cbuffer to ConstantBuffer<MyStruct>
- share texture and sampler array sizes between HLSL and C++ with .hlsli files
- roq video textures support?
*/

/*
All three types of command list use the ID3D12GraphicsCommandList interface,
however only a subset of the methods are supported for copy and compute.

Copy and compute command lists can use the following methods:
Close
CopyBufferRegion
CopyResource
CopyTextureRegion
CopyTiles
Reset
ResourceBarrier

Compute command lists can also use the following methods:
ClearState
ClearUnorderedAccessViewFloat
ClearUnorderedAccessViewUint
DiscardResource
Dispatch
ExecuteIndirect
SetComputeRoot32BitConstant
SetComputeRoot32BitConstants
SetComputeRootConstantBufferView
SetComputeRootDescriptorTable
SetComputeRootShaderResourceView
SetComputeRootSignature
SetComputeRootUnorderedAccessView
SetDescriptorHeaps
SetPipelineState
SetPredication
EndQuery
*/

/*
There is an additional restriction for Tier 1 hardware that applies to all heaps,
and to Tier 2 hardware that applies to CBV and UAV heaps,
that all descriptor heap entries covered by descriptor tables in the root signature
must be populated with descriptors by the time the shader executes,
even if the shader (perhaps due to branching) does not need the descriptor.

There is no such restriction for Tier 3 hardware.
One mitigation for this restriction is the diligent use of Null descriptors.
*/

/*
State decay to common

The flip-side of common state promotion is decay back to D3D12_RESOURCE_STATE_COMMON.
Resources that meet certain requirements are considered to be stateless and effectively
return to the common state when the GPU finishes execution of an ExecuteCommandLists operation.
Decay does not occur between command lists executed together in the same ExecuteCommandLists call.

The following resources will decay when an ExecuteCommandLists operation is completed on the GPU:
- Resources being accessed on a Copy queue, or
- Buffer resources on any queue type, or
- Texture resources on any queue type that have the D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS flag set, or
- Any resource implicitly promoted to a read-only state.

------------------------------------

So... textures written to on the copy queue can end up in the D3D12_RESOURCE_STATE_COMMON state?
*/

/*
Depth24_Stencil8 requires:
DXGI_FORMAT_R24G8_TYPELESS
DXGI_FORMAT_D24_UNORM_S8_UINT
DXGI_FORMAT_R24_UNORM_X8_TYPELESS
*/

/*
Never got the PIX programmable capture API to work
PIXBeginCapture returns "not implemented"

// before include
#define USE_PIX 1

// before creating the device
PIXLoadLatestWinPixGpuCapturerLibrary();
HRESULT hr = PIXSetTargetWindow(GetActiveWindow());
Check(hr, "PIXSetTargetWindow");

// whenever...
PIXCaptureParameters params = {};
params.GpuCaptureParameters.FileName = L"temp.wpix";
HRESULT hr = PIXBeginCapture(0, &params);
Check(hr, "PIXBeginCapture");
*/

/*
The legacy API fails as well
DXGIGetDebugInterface1 returns "no such interface supported"

#include <DXProgrammableCapture.h>

IDXGraphicsAnalysis* graphicsAnalysis;
D3D(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&graphicsAnalysis)));
*/


#if defined(_DEBUG)
#define D3D_DEBUG
#endif
#define D3D_AGILITY_SDK
//#define D3D_GPU_BASED_VALIDATION
//#define RHI_DEBUG_FENCE
//#define RHI_ENABLE_NVAPI


#include "rhi_local.h"
#include <Windows.h>
#include "d3d12/d3d12.h"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <dwmapi.h> // for DwmGetCompositionTimingInfo
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"
#if defined(RHI_ENABLE_NVAPI)
#include "../nvapi/nvapi.h"
#endif
#include "../pix/pix3.h"
#include "../client/cl_imgui.h"


// @TODO: grab from ri.GetNextTargetTimeUS instead
extern int64_t com_nextTargetTimeUS;


#if defined(D3D_DEBUG) || defined(D3D_AGILITY_SDK)
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\cnq3\\"; }
#endif


RHIExport rhie;
RHIInfo rhiInfo;


#define DXGI_FORMAT_LIST(X) \
	X(UNKNOWN) \
	X(R32G32B32A32_TYPELESS) \
	X(R32G32B32A32_FLOAT) \
	X(R32G32B32A32_UINT) \
	X(R32G32B32A32_SINT) \
	X(R32G32B32_TYPELESS) \
	X(R32G32B32_FLOAT) \
	X(R32G32B32_UINT) \
	X(R32G32B32_SINT) \
	X(R16G16B16A16_TYPELESS) \
	X(R16G16B16A16_FLOAT) \
	X(R16G16B16A16_UNORM) \
	X(R16G16B16A16_UINT) \
	X(R16G16B16A16_SNORM) \
	X(R16G16B16A16_SINT) \
	X(R32G32_TYPELESS) \
	X(R32G32_FLOAT) \
	X(R32G32_UINT) \
	X(R32G32_SINT) \
	X(R32G8X24_TYPELESS) \
	X(D32_FLOAT_S8X24_UINT) \
	X(R32_FLOAT_X8X24_TYPELESS) \
	X(X32_TYPELESS_G8X24_UINT) \
	X(R10G10B10A2_TYPELESS) \
	X(R10G10B10A2_UNORM) \
	X(R10G10B10A2_UINT) \
	X(R11G11B10_FLOAT) \
	X(R8G8B8A8_TYPELESS) \
	X(R8G8B8A8_UNORM) \
	X(R8G8B8A8_UNORM_SRGB) \
	X(R8G8B8A8_UINT) \
	X(R8G8B8A8_SNORM) \
	X(R8G8B8A8_SINT) \
	X(R16G16_TYPELESS) \
	X(R16G16_FLOAT) \
	X(R16G16_UNORM) \
	X(R16G16_UINT) \
	X(R16G16_SNORM) \
	X(R16G16_SINT) \
	X(R32_TYPELESS) \
	X(D32_FLOAT) \
	X(R32_FLOAT) \
	X(R32_UINT) \
	X(R32_SINT) \
	X(R24G8_TYPELESS) \
	X(D24_UNORM_S8_UINT) \
	X(R24_UNORM_X8_TYPELESS) \
	X(X24_TYPELESS_G8_UINT) \
	X(R8G8_TYPELESS) \
	X(R8G8_UNORM) \
	X(R8G8_UINT) \
	X(R8G8_SNORM) \
	X(R8G8_SINT) \
	X(R16_TYPELESS) \
	X(R16_FLOAT) \
	X(D16_UNORM) \
	X(R16_UNORM) \
	X(R16_UINT) \
	X(R16_SNORM) \
	X(R16_SINT) \
	X(R8_TYPELESS) \
	X(R8_UNORM) \
	X(R8_UINT) \
	X(R8_SNORM) \
	X(R8_SINT) \
	X(A8_UNORM) \
	X(R1_UNORM) \
	X(R9G9B9E5_SHAREDEXP) \
	X(R8G8_B8G8_UNORM) \
	X(G8R8_G8B8_UNORM) \
	X(BC1_TYPELESS) \
	X(BC1_UNORM) \
	X(BC1_UNORM_SRGB) \
	X(BC2_TYPELESS) \
	X(BC2_UNORM) \
	X(BC2_UNORM_SRGB) \
	X(BC3_TYPELESS) \
	X(BC3_UNORM) \
	X(BC3_UNORM_SRGB) \
	X(BC4_TYPELESS) \
	X(BC4_UNORM) \
	X(BC4_SNORM) \
	X(BC5_TYPELESS) \
	X(BC5_UNORM) \
	X(BC5_SNORM) \
	X(B5G6R5_UNORM) \
	X(B5G5R5A1_UNORM) \
	X(B8G8R8A8_UNORM) \
	X(B8G8R8X8_UNORM) \
	X(R10G10B10_XR_BIAS_A2_UNORM) \
	X(B8G8R8A8_TYPELESS) \
	X(B8G8R8A8_UNORM_SRGB) \
	X(B8G8R8X8_TYPELESS) \
	X(B8G8R8X8_UNORM_SRGB) \
	X(BC6H_TYPELESS) \
	X(BC6H_UF16) \
	X(BC6H_SF16) \
	X(BC7_TYPELESS) \
	X(BC7_UNORM) \
	X(BC7_UNORM_SRGB) \
	X(AYUV) \
	X(Y410) \
	X(Y416) \
	X(NV12) \
	X(P010) \
	X(P016) \
	X(420_OPAQUE) \
	X(YUY2) \
	X(Y210) \
	X(Y216) \
	X(NV11) \
	X(AI44) \
	X(IA44) \
	X(P8) \
	X(A8P8) \
	X(B4G4R4A4_UNORM) \
	X(P208) \
	X(V208) \
	X(V408) \
	X(SAMPLER_FEEDBACK_MIN_MIP_OPAQUE) \
	X(SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE)


namespace RHI
{
	// D3D_FEATURE_LEVEL_12_0 is the minimum to ensure at least Resource Binding Tier 2:
	// - unlimited SRVs
	// - 14 CBVs
	// - 64 UAVs
	// - 2048 samplers
	static const D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_12_0;

	struct ResourceType
	{
		enum Id
		{
			// @NOTE: a valid type never being 0 means we can discard 0 handles right away
			Invalid,
			Buffer,
			Texture,
			Sampler,
			RootSignature,
			DescriptorTable,
			Pipeline,
			DurationQuery,
			Shader,
			Count
		};
	};

#define D3D_RESOURCE_LIST(R) \
	R(CommandQueue, "command queue") \
	R(CommandAllocator, "command allocator") \
	R(PipelineState, "pipeline state") \
	R(CommandList, "command list") \
	R(Fence, "fence") \
	R(RootSignature, "root signature") \
	R(DescriptorHeap, "descriptor heap") \
	R(Heap, "heap") \
	R(QueryHeap, "query heap") \
	R(Texture, "texture") \
	R(Buffer, "buffer") \
	R(Sampler, "samplers")

#define R(Enum, Name) Enum,
	struct D3DResourceType
	{
		enum Id
		{
			D3D_RESOURCE_LIST(R)
			Count
		};
	};
#undef R

#define R(Enum, Name) Name,
	static const char* D3DResourceNames[] =
	{
		D3D_RESOURCE_LIST(R)
		""
	};
#undef R

#undef D3D_RESOURCE_LIST

	struct Buffer
	{
		BufferDesc desc;
		D3D12MA::Allocation* allocation;
		ID3D12Resource* buffer;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
		D3D12_RESOURCE_STATES currentState;
		uint32_t cbvIndex;
		uint32_t srvIndex;
		uint32_t uavIndex;
		bool mapped;
		bool uploading;
		UINT64 uploadByteOffset;
		bool shortLifeTime = false;
	};

	struct Texture
	{
		TextureDesc desc;
		D3D12MA::Allocation* allocation;
		ID3D12Resource* texture;
		uint32_t srvIndex;
		uint32_t rtvIndex;
		uint32_t dsvIndex;
		D3D12_RESOURCE_STATES currentState;
		struct Mip
		{
			uint32_t uavIndex;
		}
		mips[MaxTextureMips];
		bool uploading;
		uint32_t uploadByteOffset;
		bool shortLifeTime = false;
	};

	struct RootSignature
	{
		struct PerStageConstants
		{
			UINT parameterIndex;
		};
		RootSignatureDesc desc;
		ID3D12RootSignature* signature;
		PerStageConstants constants[ShaderStage::Count];
		UINT genericTableIndex;
		UINT samplerTableIndex;
		UINT genericDescCount;
		UINT samplerDescCount;
		bool shortLifeTime = false;
	};

	struct DescriptorTable
	{
		ID3D12DescriptorHeap* genericHeap; // SRV, CBV, UAV
		ID3D12DescriptorHeap* samplerHeap;
		bool shortLifeTime = false;
	};

	struct Pipeline
	{
		GraphicsPipelineDesc graphicsDesc;
		ComputePipelineDesc computeDesc;
		ID3D12PipelineState* pso = NULL;
		PipelineType::Id type = PipelineType::Graphics;
		bool shortLifeTime = false;
	};

	struct Shader
	{
		IDxcBlob* blob = NULL;
		bool shortLifeTime = false;
	};

	struct Sampler
	{
		SamplerDesc desc;
		uint32_t heapIndex = UINT32_MAX;
		bool shortLifeTime = true;
	};

	struct QueryState
	{
		enum Id
		{
			Free,  // ready to be (re-)used
			Begun, // first  call done, not resolved yet
			Ended, // second call done, not resolved yet
			Count
		};
	};

	struct Fence
	{
		void Create(UINT64 value, const char* name);
		void Signal(ID3D12CommandQueue* queue, UINT64 value);
		void WaitOnCPU(UINT64 value);
		void WaitOnGPU(ID3D12CommandQueue* queue, UINT64 value);
		bool HasCompleted(UINT64 value);
		void Release();

		ID3D12Fence* fence;
		HANDLE event;
	};

	struct UploadManager
	{
		void Create();
		void Release();
		uint8_t* BeginBufferUpload(HBuffer buffer);
		void EndBufferUpload(HBuffer buffer);
		void BeginTextureUpload(MappedTexture& mappedTexture, HTexture texture);
		void EndTextureUpload();
		void WaitToStartDrawing(ID3D12CommandQueue* commandQueue);

		ID3D12CommandQueue* commandQueue;
		ID3D12CommandAllocator* commandAllocator;
		ID3D12GraphicsCommandList* commandList;

		HBuffer uploadHBuffer;
		uint32_t bufferByteCount;
		uint32_t bufferByteOffset;
		uint8_t* mappedBuffer;

		Fence fence;
		UINT64 fenceValue;

		HTexture currentTexture;
		int bufferUploadCounter;
		bool multiBufferUpload;
		bool needsRewind;
		int batchTextureCount;
		int batchBufferCount;

	private:
		void WaitToStartUploading(uint32_t uploadByteCount);
		void EndOfBufferReached();
	};

	struct ReadbackManager
	{
		void Create();
		void Release();
		void ResizeIfNeeded();
		void BeginTextureReadback(MappedTexture& mappedTexture, HTexture texture);
		void EndTextureReadback();

		ID3D12CommandAllocator* readbackCommandAllocator;
		ID3D12GraphicsCommandList* readbackCommandList;
		HBuffer readbackBuffer;
		Fence readbackFence;
		UINT64 readbackFenceValue;
		uint32_t bufferByteCount;
	};

	struct DescriptorHeap
	{
		void Create(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size, uint16_t* freeListItems, const char* name);
		void Release();
		uint32_t Allocate();
		void Free(uint32_t index);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index);
		uint32_t CreateSRV(ID3D12Resource* resource, D3D12_SHADER_RESOURCE_VIEW_DESC& desc);
		uint32_t CreateUAV(ID3D12Resource* resource, D3D12_UNORDERED_ACCESS_VIEW_DESC& desc);
		uint32_t CreateRTV(ID3D12Resource* resource, D3D12_RENDER_TARGET_VIEW_DESC& desc);
		uint32_t CreateDSV(ID3D12Resource* resource, D3D12_DEPTH_STENCIL_VIEW_DESC& desc);
		uint32_t CreateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC& desc);
		uint32_t CreateSampler(D3D12_SAMPLER_DESC& desc);

		StaticFreeList<uint16_t, InvalidDescriptorIndex> freeList;
		ID3D12DescriptorHeap* heap;
		D3D12_CPU_DESCRIPTOR_HANDLE startAddress;
		UINT descriptorSize;
		D3D12_DESCRIPTOR_HEAP_TYPE type;
	};

	struct DurationQuery
	{
		QueryState::Id state;
	};

	struct FrameQueries
	{
		DurationQuery durationQueries[MaxDurationQueries];
		uint32_t durationQueryCount;
	};

	struct ResolvedQueries
	{
		uint32_t gpuMicroSeconds[MaxDurationQueries];
		uint32_t durationQueryCount;
	};

	struct Pix
	{
		typedef void(WINAPI* BeginEventOnCommandListPtr)(ID3D12GraphicsCommandList* commandList, UINT64 color, _In_ PCSTR formatString);
		typedef void(WINAPI* EndEventOnCommandListPtr)(ID3D12GraphicsCommandList* commandList);
		typedef void(WINAPI* SetMarkerOnCommandListPtr)(ID3D12GraphicsCommandList* commandList, UINT64 color, _In_ PCSTR formatString);

		BeginEventOnCommandListPtr BeginEventOnCommandList;
		EndEventOnCommandListPtr EndEventOnCommandList;
		SetMarkerOnCommandListPtr SetMarkerOnCommandList;

		HMODULE module;
		bool canBeginAndEnd;
	};

	struct RHIPrivate
	{
		bool initialized;

		ID3D12Debug* debug; // can be NULL
		ID3D12InfoQueue* infoQueue; // can be NULL
		IDXGIInfoQueue* dxgiInfoQueue; // can be NULL
#if defined(D3D_DEBUG)
		IDXGIFactory2* factory;
#else
		IDXGIFactory1* factory;
#endif
		IDXGIAdapter1* adapter;
		ID3D12Device* device;
		D3D12MA::Allocator* allocator;
		D3D12MA::Pool* umaPool; // only non-NULL when using a cache-coherent UMA adapter
		ID3D12CommandQueue* mainCommandQueue;
		ID3D12CommandQueue* computeCommandQueue;
		IDXGISwapChain3* swapChain;
		HTexture renderTargets[FrameCount];
		ID3D12CommandAllocator* mainCommandAllocators[FrameCount];
#if defined( QC )
		ID3D12GraphicsCommandList* mainCommandList;
#else
		ID3D12GraphicsCommandList6* mainCommandList;
#endif
		ID3D12CommandAllocator* tempCommandAllocator;
#if defined( QC )
		ID3D12GraphicsCommandList* tempCommandList;
#else
		ID3D12GraphicsCommandList6* tempCommandList;
#endif
		bool tempCommandListOpen;
#if defined( QC )
		ID3D12GraphicsCommandList* commandList; // not owned, don't release it!
#else
		ID3D12GraphicsCommandList6* commandList; // not owned, don't release it!
#endif
		uint32_t swapChainBufferCount;
		uint32_t renderFrameCount;
		HANDLE frameLatencyWaitableObject;
		bool frameLatencyWaitNeeded;
		UINT frameIndex;
		UINT swapChainBufferIndex;
		Fence mainFence;
		UINT64 mainFenceValues[FrameCount];
		Fence tempFence;
		UINT64 tempFenceValue;
		ID3D12QueryHeap* timeStampHeaps[FrameCount];
		HBuffer timeStampBuffers[FrameCount];
		uint32_t frameDurationQueryIndex;
		HRootSignature currentRootSignature;
		bool isTearingSupported;
		bool vsync;
		bool frameBegun;
		bool baseVRSSupport;
		bool extendedVRSSupport;

		HMODULE dxcModule;
		HMODULE dxilModule;
		IDxcUtils* dxcUtils;
		IDxcCompiler3* dxcCompiler;

		uint16_t descriptorFreeListData[MaxCPUDescriptors];
		DescriptorHeap descHeapGeneric;
		DescriptorHeap descHeapSamplers;
		DescriptorHeap descHeapRTVs;
		DescriptorHeap descHeapDSVs;

#define POOL(Type, Size) StaticPool<Type, H##Type, ResourceType::Type, Size>
		POOL(Buffer, 128) buffers;
		POOL(Texture, MAX_DRAWIMAGES * 2) textures;
		POOL(RootSignature, 64) rootSignatures;
		POOL(DescriptorTable, 64) descriptorTables;
		POOL(Pipeline, 256) pipelines;
		POOL(Shader, 16) shaders;
		POOL(Sampler, 128) samplers;
#undef POOL

#define DESTROY_POOL_LIST(POOL) \
		POOL(buffers, DestroyBuffer) \
		POOL(textures, DestroyTexture) \
		POOL(rootSignatures, DestroyRootSignature) \
		POOL(descriptorTables, DestroyDescriptorTable) \
		POOL(pipelines, DestroyPipeline) \
		POOL(shaders, DestroyShader) \
		POOL(samplers, DestroySampler)

		// null resources, no manual clean-up needed
		HTexture nullTexture; // SRV
		HTexture nullRWTexture; // UAV
		HBuffer nullBuffer; // CBV
		HBuffer nullRWBuffer; // UAV
		HSampler nullSampler;

		byte persStringData[64 << 10];
		byte tempStringData[64 << 10];
		char adapterName[256];
		LinearAllocator persStringAllocator;
		LinearAllocator tempStringAllocator;
		UploadManager upload;
		ReadbackManager readback;
		StaticUnorderedArray<HTexture, MAX_DRAWIMAGES> texturesToTransition;
		StaticUnorderedArray<HBuffer, 64> buffersToTransition;
		FrameQueries frameQueries[FrameCount];
		ResolvedQueries resolvedQueries;
		Pix pix;
		int64_t beforeInputSamplingUS;
		int64_t beforeRenderingUS;
	};

	static RHIPrivate rhi;

#define COM_RELEASE(p)       do { if(p) { p->Release(); p = NULL; } } while((void)0,0)
#define COM_RELEASE_ARRAY(a) do { for(int i = 0; i < ARRAY_LEN(a); ++i) { COM_RELEASE(a[i]); } } while((void)0,0)

#define D3D(Exp) Check((Exp), #Exp)

#if defined(near)
#	undef near
#endif

#if defined(far)
#	undef far
#endif

#if !defined(D3DDDIERR_DEVICEREMOVED)
#	define D3DDDIERR_DEVICEREMOVED ((HRESULT)0x88760870L)
#endif

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

	static bool Check(HRESULT hr, const char* function)
	{
		if(SUCCEEDED(hr))
		{
			return true;
		}

		if(1) // @TODO: fatal error mode always on for now
		{
			ri.Error(ERR_FATAL, "'%s' failed with code 0x%08X (%s)\n", function, (unsigned int)hr, GetSystemErrorString(hr));
		}
		return false;
	}

	static void SetDebugName(ID3D12DeviceChild* resource, const char* resourceName, D3DResourceType::Id resourceType)
	{
		if(resourceName == NULL || (uint32_t)resourceType >= D3DResourceType::Count)
		{
			return;
		}

		const char* const name = va("%s %s", resourceName, D3DResourceNames[resourceType]);

		// ID3D12Object::SetName is a Unicode wrapper for
		// ID3D12Object::SetPrivateData with WKPDID_D3DDebugObjectNameW
		resource->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name), name);
	}

	static uint32_t GetBytesPerPixel(TextureFormat::Id format)
	{
		switch(format)
		{
			case TextureFormat::RGBA64_Float:
				return 8;
			case TextureFormat::RGBA32_UNorm:
			case TextureFormat::Depth32_Float:
			case TextureFormat::Depth24_Stencil8:
			case TextureFormat::R10G10B10A2_UNorm:
				return 4;
			case TextureFormat::RG16_UNorm:
				return 2;
			case TextureFormat::R8_UNorm:
				return 1;
			default:
				Q_assert(!"Unsupported texture format");
				return 4;
		}
	}

	static ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT size, bool shaderVisible, const char* name)
	{
		if(size == 0)
		{
			return NULL;
		}

		ID3D12DescriptorHeap* heap;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { 0 };
		heapDesc.Type = type;
		heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NumDescriptors = size;
		heapDesc.NodeMask = 0;
		D3D(rhi.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)));
		SetDebugName(heap, name, D3DResourceType::DescriptorHeap);

		return heap;
	}

	static UINT GetTextureByteCount(ID3D12Resource* texture)
	{
		const D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
		rhi.device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, NULL, NULL, NULL);
		Q_assert(layout.Footprint.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
		Q_assert((UINT64)layout.Footprint.Width == textureDesc.Width);
		Q_assert(layout.Footprint.Height == textureDesc.Height);

		return layout.Footprint.RowPitch * layout.Footprint.Height;
	}

	void Fence::Create(UINT64 value, const char* name)
	{
		D3D(rhi.device->CreateFence(value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		SetDebugName(fence, name, D3DResourceType::Fence);

		event = CreateEvent(NULL, FALSE, FALSE, NULL);
		if(event == NULL)
		{
			Check(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent");
		}
	}

	void Fence::Signal(ID3D12CommandQueue* queue, UINT64 value)
	{
		D3D(queue->Signal(fence, value));
	}

	void Fence::WaitOnCPU(UINT64 value)
	{
		if(fence->GetCompletedValue() < value)
		{
			D3D(fence->SetEventOnCompletion(value, event));
			WaitForSingleObjectEx(event, INFINITE, FALSE);
		}
	}

	void Fence::WaitOnGPU(ID3D12CommandQueue* queue, UINT64 value)
	{
		D3D(queue->Wait(fence, value));
	}

	bool Fence::HasCompleted(UINT64 value)
	{
		return fence->GetCompletedValue() >= value;
	}

	void Fence::Release()
	{
		CloseHandle(event);
		event = NULL;
		COM_RELEASE(fence);
	}

	void UploadManager::Create()
	{
		BufferDesc bufferDesc("upload", 128 << 20, ResourceStates::CopyDestinationBit);
		bufferDesc.memoryUsage = MemoryUsage::Upload;
		uploadHBuffer = CreateBuffer(bufferDesc);
		bufferByteCount = bufferDesc.byteCount;
		bufferByteOffset = 0;
		mappedBuffer = MapBuffer(uploadHBuffer);

		D3D12_COMMAND_QUEUE_DESC queueDesc = { 0 };
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.NodeMask = 0;
		D3D(rhi.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
		SetDebugName(commandQueue, "upload", D3DResourceType::CommandQueue);

		D3D(rhi.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&commandAllocator)));
		SetDebugName(commandAllocator, "upload", D3DResourceType::CommandAllocator);

		D3D(rhi.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, commandAllocator, NULL, IID_PPV_ARGS(&commandList)));
		SetDebugName(commandList, "upload", D3DResourceType::CommandList);
		D3D(commandList->Close());

		fence.Create(0, "upload");
		fenceValue = 0;

		currentTexture = RHI_MAKE_NULL_HANDLE();

		bufferUploadCounter = 0;
		multiBufferUpload = false;
		needsRewind = false;
		batchTextureCount = 0;
		batchBufferCount = 0;
	}

	void UploadManager::Release()
	{
		UnmapBuffer(uploadHBuffer);
		fence.Release();
		COM_RELEASE(commandQueue);
		COM_RELEASE(commandList);
		COM_RELEASE(commandAllocator);
	}

	uint8_t* UploadManager::BeginBufferUpload(HBuffer userHBuffer)
	{
		Q_assert(bufferUploadCounter >= 0);
		bufferUploadCounter++;
		if(bufferUploadCounter > 1)
		{
			multiBufferUpload = true;
		}

		Buffer& userBuffer = rhi.buffers.Get(userHBuffer);
		Q_assert(!userBuffer.uploading);

		uint8_t* mapped = NULL;
		Q_assert(userBuffer.desc.memoryUsage != MemoryUsage::Readback);
		if(userBuffer.desc.memoryUsage == MemoryUsage::GPU &&
			rhi.umaPool == NULL)
		{
			const uint32_t uploadByteCount = userBuffer.desc.byteCount;
			WaitToStartUploading(uploadByteCount);

			mapped = mappedBuffer + bufferByteOffset;
			userBuffer.uploadByteOffset = bufferByteOffset;

			bufferByteOffset = AlignUp<uint32_t>(bufferByteOffset + uploadByteCount, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

			if(multiBufferUpload)
			{
				needsRewind = true;
			}
			batchBufferCount++;
		}
		else
		{
			mapped = (uint8_t*)MapBuffer(userHBuffer);
			Q_assert(mapped != NULL);
		}

		userBuffer.uploading = true;

		return mapped;
	}

	void UploadManager::EndBufferUpload(HBuffer userHBuffer)
	{
		bufferUploadCounter--;
		Q_assert(bufferUploadCounter >= 0);

		Buffer& userBuffer = rhi.buffers.Get(userHBuffer);
		Q_assert(userBuffer.uploading);

		Buffer& uploadBuffer = rhi.buffers.Get(uploadHBuffer);

		if(!userBuffer.mapped)
		{
			D3D(commandList->Reset(commandAllocator, NULL));

			const UINT64 byteCount = min(userBuffer.desc.byteCount, uploadBuffer.desc.byteCount);
			commandList->CopyBufferRegion(userBuffer.buffer, 0, uploadBuffer.buffer, userBuffer.uploadByteOffset, byteCount);

			ID3D12CommandList* commandLists[] = { commandList };
			D3D(commandList->Close());
			commandQueue->ExecuteCommandLists(ARRAY_LEN(commandLists), commandLists);
			fenceValue++;
			commandQueue->Signal(fence.fence, fenceValue);
		}
		else
		{
			UnmapBuffer(userHBuffer);
		}

		userBuffer.uploading = false;

		if(bufferUploadCounter == 0 && multiBufferUpload)
		{
			if(needsRewind)
			{
				EndOfBufferReached();
				needsRewind = false;
			}
			multiBufferUpload = false;
		}
	}

	void UploadManager::BeginTextureUpload(MappedTexture& mappedTexture, HTexture htexture)
	{
		Q_assert(IsNullHandle(currentTexture));

		Texture& texture = rhi.textures.Get(htexture);
		Q_assert(!texture.uploading);
		
		const D3D12_RESOURCE_DESC textureDesc = texture.texture->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
		UINT64 uploadByteCount;
		rhi.device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, NULL, NULL, &uploadByteCount);
		WaitToStartUploading(uploadByteCount);

		const UINT sourcePitch = (UINT)(texture.desc.width * GetBytesPerPixel(texture.desc.format));
		mappedTexture.mappedData = mappedBuffer + bufferByteOffset;
		mappedTexture.rowCount = texture.desc.height;
		mappedTexture.columnCount = texture.desc.width;
		mappedTexture.srcRowByteCount = sourcePitch;
		mappedTexture.dstRowByteCount = AlignUp<uint32_t>(layout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		texture.uploadByteOffset = bufferByteOffset;
		texture.uploading = true;
		bufferByteOffset = AlignUp<uint32_t>(bufferByteOffset + uploadByteCount, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
		currentTexture = htexture;
		batchTextureCount++;
	}

	void UploadManager::EndTextureUpload()
	{
		Q_assert(!IsNullHandle(currentTexture));

		const HTexture htexture = currentTexture;
		Texture& texture = rhi.textures.Get(htexture);
		Q_assert(texture.uploading);

		const D3D12_RESOURCE_DESC textureDesc = texture.texture->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
		rhi.device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, NULL, NULL, NULL);

		Buffer& buffer = rhi.buffers.Get(uploadHBuffer);
		D3D12_TEXTURE_COPY_LOCATION dstLoc = { 0 };
		D3D12_TEXTURE_COPY_LOCATION srcLoc = { 0 };
		dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLoc.pResource = texture.texture;
		dstLoc.SubresourceIndex = 0;
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLoc.pResource = buffer.buffer;
		srcLoc.PlacedFootprint = layout;
		srcLoc.PlacedFootprint.Offset = texture.uploadByteOffset;
		D3D12_BOX srcBox = { 0 };
		srcBox.left = 0;
		srcBox.top = 0;
		srcBox.front = 0;
		srcBox.right = textureDesc.Width;
		srcBox.bottom = textureDesc.Height;
		srcBox.back = 1;

		D3D(commandList->Reset(commandAllocator, NULL));

		commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

		ID3D12CommandList* commandLists[] = { commandList };
		D3D(commandList->Close());
		commandQueue->ExecuteCommandLists(ARRAY_LEN(commandLists), commandLists);
		fenceValue++;
		commandQueue->Signal(fence.fence, fenceValue);

		texture.uploading = false;
		currentTexture = RHI_MAKE_NULL_HANDLE();
	}

	void UploadManager::WaitToStartDrawing(ID3D12CommandQueue* commandQueue_)
	{
		fence.WaitOnGPU(commandQueue_, fenceValue);
	}

	void UploadManager::WaitToStartUploading(uint32_t uploadByteCount)
	{
		if(uploadByteCount > bufferByteCount)
		{
			ri.Error(ERR_FATAL, "Upload request too large!\n");
		}

		if(bufferByteOffset + uploadByteCount > bufferByteCount)
		{
			EndOfBufferReached();
		}
	}

	void UploadManager::EndOfBufferReached()
	{
#if defined(_DEBUG)
		ri.Printf(PRINT_ALL, "Waiting for GPU upload: %s (%d T, %d B)\n",
			Com_FormatBytes(bufferByteOffset),
			batchTextureCount,
			batchBufferCount);
#endif
		fence.WaitOnCPU(fenceValue);
		D3D(commandAllocator->Reset());
		bufferByteOffset = 0;
		batchTextureCount = 0;
		batchBufferCount = 0;
	}

	void ReadbackManager::Create()
	{
		D3D(rhi.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&readbackCommandAllocator)));
		SetDebugName(readbackCommandAllocator, "readback", D3DResourceType::CommandAllocator);

		D3D(rhi.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, readbackCommandAllocator, NULL, IID_PPV_ARGS(&readbackCommandList)));
		SetDebugName(readbackCommandList, "readback", D3DResourceType::CommandList);
		D3D(readbackCommandList->Close());

		Texture& texture = rhi.textures.Get(GetSwapChainTexture());
		Q_assert(texture.desc.format == TextureFormat::RGBA32_UNorm);
		const uint32_t byteCount = GetTextureByteCount(texture.texture);
		BufferDesc desc("readback", byteCount, ResourceStates::CopyDestinationBit);
		desc.memoryUsage = MemoryUsage::Readback;
		readbackBuffer = CreateBuffer(desc);
		bufferByteCount = byteCount;

		readbackFence.Create(readbackFenceValue, "readback");
	}

	void ReadbackManager::Release()
	{
		readbackFence.Release();
		COM_RELEASE(readbackCommandList);
		COM_RELEASE(readbackCommandAllocator);
	}

	void ReadbackManager::ResizeIfNeeded()
	{
		Texture& texture = rhi.textures.Get(GetSwapChainTexture());
		Q_assert(texture.desc.format == TextureFormat::RGBA32_UNorm);
		const UINT byteCount = GetTextureByteCount(texture.texture);
		if(byteCount <= bufferByteCount)
		{
			return;
		}

		// @NOTE: this is called after the device has become idle
		DestroyBuffer(readbackBuffer);

		BufferDesc desc("readback", byteCount, ResourceStates::CopyDestinationBit);
		desc.memoryUsage = MemoryUsage::Readback;
		readbackBuffer = CreateBuffer(desc);
		bufferByteCount = byteCount;
	}

	void ReadbackManager::BeginTextureReadback(MappedTexture& mappedTexture, HTexture htexture)
	{
		D3D(readbackCommandAllocator->Reset());
		D3D(readbackCommandList->Reset(readbackCommandAllocator, NULL));

		Texture& texture = rhi.textures.Get(htexture);
		Q_assert(texture.desc.format == TextureFormat::RGBA32_UNorm);
		const D3D12_RESOURCE_DESC textureDesc = texture.texture->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
		rhi.device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, NULL, NULL, NULL);
		Q_assert(layout.Footprint.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
		Q_assert(layout.Footprint.Width == texture.desc.width);
		Q_assert(layout.Footprint.Height == texture.desc.height);

		Buffer& buffer = rhi.buffers.Get(readbackBuffer);
		D3D12_TEXTURE_COPY_LOCATION dstLoc = { 0 };
		D3D12_TEXTURE_COPY_LOCATION srcLoc = { 0 };
		dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstLoc.pResource = buffer.buffer;
		dstLoc.PlacedFootprint = layout;
		dstLoc.PlacedFootprint.Offset = 0;
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcLoc.pResource = texture.texture;
		srcLoc.SubresourceIndex = 0;
		D3D12_BOX srcBox = { 0 };
		srcBox.left = 0;
		srcBox.top = 0;
		srcBox.front = 0;
		srcBox.right = textureDesc.Width;
		srcBox.bottom = textureDesc.Height;
		srcBox.back = 1;

		const D3D12_RESOURCE_STATES prevState = texture.currentState;

		// @TODO: use CmdBarrier
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = texture.texture;
		barrier.Transition.StateBefore = prevState;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		if(texture.currentState != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			readbackCommandList->ResourceBarrier(1, &barrier);
			texture.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
		}
		readbackCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.StateAfter = prevState;
		if(texture.currentState != prevState)
		{
			readbackCommandList->ResourceBarrier(1, &barrier);
			texture.currentState = prevState;
		}

		D3D(readbackCommandList->Close());
		ID3D12CommandList* commandListArray[] = { readbackCommandList };
		rhi.mainCommandQueue->ExecuteCommandLists(ARRAY_LEN(commandListArray), commandListArray);

		readbackFenceValue++;
		readbackFence.Signal(rhi.mainCommandQueue, readbackFenceValue);
		readbackFence.WaitOnCPU(readbackFenceValue);

		mappedTexture.mappedData = MapBuffer(readbackBuffer);
		mappedTexture.rowCount = layout.Footprint.Height;
		mappedTexture.columnCount = layout.Footprint.Width;
		mappedTexture.srcRowByteCount = layout.Footprint.RowPitch;
		mappedTexture.dstRowByteCount = 0;
	}

	void ReadbackManager::EndTextureReadback()
	{
		UnmapBuffer(readbackBuffer);
	}

	void DescriptorHeap::Create(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t size, uint16_t* freeListItems, const char* name)
	{
		heap = CreateDescriptorHeap(heapType, size, false, name);
		freeList.Init(freeListItems, size);
		startAddress = heap->GetCPUDescriptorHandleForHeapStart();
		descriptorSize = rhi.device->GetDescriptorHandleIncrementSize(heapType);
		type = heapType;
	}

	void DescriptorHeap::Release()
	{
		COM_RELEASE(heap);
	}

	uint32_t DescriptorHeap::Allocate()
	{
		return freeList.Allocate();
	}

	void DescriptorHeap::Free(uint32_t index)
	{
		freeList.Free(index);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandle(uint32_t index)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = startAddress;
		handle.ptr += index * descriptorSize;

		return handle;
	}

	uint32_t DescriptorHeap::CreateSRV(ID3D12Resource* resource, D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
	{
		Q_assert(resource);
		Q_assert(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		if(desc.Format == DXGI_FORMAT_D32_FLOAT)
		{
			desc.Format = DXGI_FORMAT_R32_FLOAT;
		}

		const uint32_t index = freeList.Allocate();
		rhi.device->CreateShaderResourceView(resource, &desc, GetCPUHandle(index));

		return index;
	}

	uint32_t DescriptorHeap::CreateUAV(ID3D12Resource* resource, D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
	{
		Q_assert(resource);
		Q_assert(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		const uint32_t index = freeList.Allocate();
		rhi.device->CreateUnorderedAccessView(resource, NULL, &desc, GetCPUHandle(index));

		return index;
	}

	uint32_t DescriptorHeap::CreateRTV(ID3D12Resource* resource, D3D12_RENDER_TARGET_VIEW_DESC& desc)
	{
		Q_assert(resource);
		Q_assert(type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		const uint32_t index = freeList.Allocate();
		rhi.device->CreateRenderTargetView(resource, &desc, GetCPUHandle(index));

		return index;
	}

	uint32_t DescriptorHeap::CreateDSV(ID3D12Resource* resource, D3D12_DEPTH_STENCIL_VIEW_DESC& desc)
	{
		Q_assert(resource);
		Q_assert(type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		const uint32_t index = freeList.Allocate();
		rhi.device->CreateDepthStencilView(resource, &desc, GetCPUHandle(index));

		return index;
	}

	uint32_t DescriptorHeap::CreateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
	{
		Q_assert(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		const uint32_t index = freeList.Allocate();
		rhi.device->CreateConstantBufferView(&desc, GetCPUHandle(index));

		return index;
	}

	uint32_t DescriptorHeap::CreateSampler(D3D12_SAMPLER_DESC& desc)
	{
		Q_assert(type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		const uint32_t index = freeList.Allocate();
		rhi.device->CreateSampler(&desc, GetCPUHandle(index));

		return index;
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

	static DXGI_GPU_PREFERENCE GetGPUPreference(int preference)
	{
		switch(preference)
		{
			case GPUPREF_HIGHPERF: return DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
			case GPUPREF_LOWPOWER: return DXGI_GPU_PREFERENCE_MINIMUM_POWER;
			default: return DXGI_GPU_PREFERENCE_UNSPECIFIED;
		}
	}

	static const char* GetUTF8String(const WCHAR* wideStr, const char* defaultUTF8Str)
	{
		static char utf8Str[256];
		const char* utf8StrPtr = defaultUTF8Str;
		if(WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, utf8Str, sizeof(utf8Str), NULL, NULL) > 0)
		{
			utf8StrPtr = utf8Str;
		}

		return utf8StrPtr;
	}

	static bool IsSuitableAdapter(IDXGIAdapter1* adapter)
	{
		HRESULT hr = S_OK;

		DXGI_ADAPTER_DESC1 desc;
		hr = adapter->GetDesc1(&desc);
		if(FAILED(hr))
		{
			ri.Printf(PRINT_WARNING, "D3D12: IDXGIAdapter1::GetDesc1 failed with code 0x%08X (%s)\n",
				(unsigned int)hr, GetSystemErrorString(hr));
			return false;
		}

		if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			//ri.Printf(PRINT_WARNING, "D3D12: '%s' is not real hardware\n",
				//GetUTF8Name(desc.Description, "unknown adapter"));
			return false;
		}

		hr = D3D12CreateDevice(adapter, FeatureLevel, __uuidof(ID3D12Device), NULL);
		if(FAILED(hr))
		{
			ri.Printf(PRINT_WARNING, "D3D12: can't create device for '%s' with code 0x%08X (%s)\n",
				GetUTF8String(desc.Description, "unknown adapter"), (unsigned int)hr, GetSystemErrorString(hr));
			return false;
		}

		return true;
	}

	static IDXGIAdapter1* FindMostSuitableAdapter(IDXGIFactory1* factory, int enginePreference)
	{
		IDXGIAdapter1* adapter = NULL;
		IDXGIFactory6* factory6 = NULL;
		if(SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			const DXGI_GPU_PREFERENCE dxgiPreference = GetGPUPreference(enginePreference);

			UINT i = 0;
			while(SUCCEEDED(factory6->EnumAdapterByGpuPreference(i++, dxgiPreference, IID_PPV_ARGS(&adapter))))
			{
				if(IsSuitableAdapter(adapter))
				{
					COM_RELEASE(factory6);
					return adapter;
				}
				COM_RELEASE(adapter);
			}
		}
		COM_RELEASE(factory6);

		UINT i = 0;
		while(SUCCEEDED(rhi.factory->EnumAdapters1(i++, &adapter)))
		{
			if(IsSuitableAdapter(adapter))
			{
				return adapter;
			}
			COM_RELEASE(adapter);
		}

		ri.Error(ERR_FATAL, "No suitable DXGI adapter was found!\n");
		return NULL;
	}

	static void Present()
	{
		UINT flags;
		UINT swapInterval;
		if(r_vsync->integer)
		{
			swapInterval = 1;
			flags = 0;
		}
		else
		{
			swapInterval = 0;
			flags = rhi.isTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
		}
		const HRESULT hr = rhi.swapChain->Present(swapInterval, flags);
		rhi.frameLatencyWaitNeeded = true;

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
			deviceRemovedReason = rhi.device->GetDeviceRemovedReason();
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
#if defined(D3D_DEBUG)
		else if(hr != S_OK)
		{
			Sys_DebugPrintf("Present error: 0x%08X (%s)\n", (unsigned int)hr, GetSystemErrorString(hr));
		}
#endif

		if(presentError == PE_DEVICE_REMOVED)
		{
			ri.Error(ERR_FATAL, "Direct3D device was removed! Reason: %s", GetDeviceRemovedReasonString(deviceRemovedReason));
		}
		else if(presentError == PE_DEVICE_RESET)
		{
			ri.Printf(PRINT_ERROR, "Direct3D device was reset! Restarting the video system...");
			Cbuf_AddText("vid_restart\n");
		}
	}

#if defined(_DEBUG)
	static bool CanWriteCommands()
	{
		// @TODO: check that the command list is open
		return rhi.commandList != NULL;
	}
#endif

	template<typename T, typename HT, Handle RT, int N>
	static void DestroyPool(StaticPool<T, HT, RT, N>& pool, void (*DestroyResource)(HT), bool fullShutDown)
	{
		T* resource;
		HT handle;
		for(int i = 0; pool.FindNext(&resource, &handle, &i);)
		{
			if(fullShutDown || resource->shortLifeTime)
			{
				(*DestroyResource)(handle);
			}
		}

		if(fullShutDown)
		{
			pool.Clear();
		}
	}

	static const char* AllocateName(const char* name, bool shortLifeTime)
	{
		LinearAllocator& allocator = shortLifeTime ? rhi.tempStringAllocator : rhi.persStringAllocator;
		
		return allocator.Allocate(name);
	}

	template<typename T>
	static void AllocateAndFixName(const T& desc)
	{
		((BufferDesc&)desc).name = AllocateName(desc.name, desc.shortLifeTime);
	}

	static DXGI_FORMAT GetD3DIndexFormat(IndexType::Id type)
	{
		return type == IndexType::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	}

	static D3D12_SHADER_VISIBILITY GetD3DVisibility(ShaderStage::Id shaderType)
	{
		switch(shaderType)
		{
			case ShaderStage::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
			case ShaderStage::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
			case ShaderStage::Compute: return D3D12_SHADER_VISIBILITY_ALL; // @TODO: assert here too?
			default: Q_assert(!"Unsupported shader type"); return D3D12_SHADER_VISIBILITY_ALL;
		}
	}

	static D3D12_SHADER_VISIBILITY GetD3DVisibility(ShaderStages::Flags flags)
	{
		if(__popcnt(flags & ShaderStages::AllGraphicsBits) > 1)
		{
			return D3D12_SHADER_VISIBILITY_ALL;
		}

		if(flags & ShaderStages::VertexBit)
		{
			return D3D12_SHADER_VISIBILITY_VERTEX;
		}

		if(flags & ShaderStages::PixelBit)
		{
			return D3D12_SHADER_VISIBILITY_PIXEL;
		}

		return D3D12_SHADER_VISIBILITY_ALL;
	}

	static D3D12_DESCRIPTOR_RANGE_TYPE GetD3DDescriptorRangeType(DescriptorType::Id descType)
	{
		switch(descType)
		{
			case DescriptorType::Texture: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			case DescriptorType::Buffer: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			case DescriptorType::RWTexture: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			case DescriptorType::RWBuffer: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			case DescriptorType::Sampler: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			default: Q_assert(!"Unsupported descriptor type"); return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		}
	}

	static const char* GetD3DSemanticName(ShaderSemantic::Id semantic)
	{
		switch(semantic)
		{
			case ShaderSemantic::Position: return "POSITION";
			case ShaderSemantic::Normal: return "NORMAL";
			case ShaderSemantic::TexCoord: return "TEXCOORD";
			case ShaderSemantic::Color: return "COLOR";
			default: Q_assert(!"Unsupported shader semantic"); return "";
		}
	}

	static DXGI_FORMAT GetD3DFormat(DataType::Id dataType, uint32_t vectorLength)
	{
		if(vectorLength < 1 || vectorLength > 4)
		{
			Q_assert(!"Invalid vector length");
			return DXGI_FORMAT_UNKNOWN;
		}

		switch(dataType)
		{
			case DataType::Float32:
				switch(vectorLength)
				{
					case 1: return DXGI_FORMAT_R32_FLOAT;
					case 2: return DXGI_FORMAT_R32G32_FLOAT;
					case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
					case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
			case DataType::UInt32:
				switch(vectorLength)
				{
					case 1: return DXGI_FORMAT_R32_UINT;
					case 2: return DXGI_FORMAT_R32G32_UINT;
					case 3: return DXGI_FORMAT_R32G32B32_UINT;
					case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
				}
			case DataType::UNorm8:
				switch(vectorLength)
				{
					case 1: return DXGI_FORMAT_R8_UNORM;
					case 2: return DXGI_FORMAT_R8G8_UNORM;
					case 3: Q_assert(!"Unsupported format"); return DXGI_FORMAT_UNKNOWN;
					case 4: return DXGI_FORMAT_R8G8B8A8_UNORM;
				}
			default: Q_assert(!"Unsupported data type"); return DXGI_FORMAT_UNKNOWN;
		}
	}

	static D3D12_COMPARISON_FUNC GetD3DComparisonFunction(ComparisonFunction::Id function)
	{
		switch(function)
		{
			case ComparisonFunction::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
			case ComparisonFunction::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
			case ComparisonFunction::Greater: return D3D12_COMPARISON_FUNC_GREATER;
			case ComparisonFunction::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
			case ComparisonFunction::Less: return D3D12_COMPARISON_FUNC_LESS;
			case ComparisonFunction::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
			case ComparisonFunction::Never: return D3D12_COMPARISON_FUNC_NEVER;
			case ComparisonFunction::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
			default: Q_assert(!"Unsupported comparison function"); return D3D12_COMPARISON_FUNC_ALWAYS;
		}
	}

	static DXGI_FORMAT GetD3DFormat(TextureFormat::Id format)
	{
		switch(format)
		{
			case TextureFormat::RGBA32_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case TextureFormat::RGBA64_UNorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
			case TextureFormat::RGBA64_Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case TextureFormat::Depth32_Float: return DXGI_FORMAT_D32_FLOAT;
			case TextureFormat::Depth24_Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
			case TextureFormat::RG16_UNorm: return DXGI_FORMAT_R8G8_UNORM;
			case TextureFormat::R8_UNorm: return DXGI_FORMAT_R8_UNORM;
			case TextureFormat::R10G10B10A2_UNorm: return DXGI_FORMAT_R10G10B10A2_UNORM;
			default: Q_assert(!"Unsupported texture format"); return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
	}

	static D3D12_CULL_MODE GetD3DCullMode(cullType_t cullMode)
	{
		switch(cullMode)
		{
			case CT_TWO_SIDED: return D3D12_CULL_MODE_NONE;
			case CT_BACK_SIDED: return D3D12_CULL_MODE_BACK;
			case CT_FRONT_SIDED: return D3D12_CULL_MODE_FRONT;
			default: Q_assert(!"Unsupported cull mode"); return D3D12_CULL_MODE_NONE;
		}
	}

	static D3D12_TEXTURE_ADDRESS_MODE GetD3DTextureAddressMode(textureWrap_t wrap)
	{
		switch(wrap)
		{
			case TW_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			case TW_CLAMP_TO_EDGE: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			default: Q_assert(!"Unsupported texture wrap mode"); return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
	}

	static D3D12_FILTER GetD3DFilter(TextureFilter::Id filter)
	{
		switch(filter)
		{
			case TextureFilter::Point: return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
			case TextureFilter::Linear: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			case TextureFilter::Anisotropic: return D3D12_FILTER_ANISOTROPIC;
			default: Q_assert(!"Unsupported texture filter mode"); return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		}
	}

	static D3D12_STENCIL_OP GetD3DStencilOp(StencilOp::Id stencilOp)
	{
		switch(stencilOp)
		{
			case StencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
			case StencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
			case StencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
			case StencilOp::SaturatedIncrement: return D3D12_STENCIL_OP_INCR_SAT;
			case StencilOp::SaturatedDecrement: return D3D12_STENCIL_OP_DECR_SAT;
			case StencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
			case StencilOp::WrappedIncrement: return D3D12_STENCIL_OP_INCR;
			case StencilOp::WrappedDecrement: return D3D12_STENCIL_OP_DECR;
			default: Q_assert(!"Unsupported stencop operation"); return D3D12_STENCIL_OP_REPLACE;
		}
	}

	static D3D12_RESOURCE_STATES GetD3DResourceStates(ResourceStates::Flags flags)
	{
#define ADD_BITS(RHIBit, D3DBits) \
		if(flags & ResourceStates::RHIBit) \
		{ \
			states |= D3DBits; \
		}

		D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
		ADD_BITS(VertexBufferBit, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		ADD_BITS(IndexBufferBit, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		ADD_BITS(ConstantBufferBit, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		ADD_BITS(RenderTargetBit, D3D12_RESOURCE_STATE_RENDER_TARGET);
		ADD_BITS(VertexShaderAccessBit, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		ADD_BITS(PixelShaderAccessBit, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		ADD_BITS(ComputeShaderAccessBit, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		ADD_BITS(CopySourceBit, D3D12_RESOURCE_STATE_COPY_SOURCE);
		ADD_BITS(CopyDestinationBit, D3D12_RESOURCE_STATE_COPY_DEST);
		ADD_BITS(DepthReadBit, D3D12_RESOURCE_STATE_DEPTH_READ);
		ADD_BITS(DepthWriteBit, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		ADD_BITS(UnorderedAccessBit, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ADD_BITS(PresentBit, D3D12_RESOURCE_STATE_PRESENT);

		return states;

#undef ADD_BITS
	}

	static D3D12_BLEND GetD3DSourceBlend(uint32_t stateBits)
	{
		switch(stateBits & GLS_SRCBLEND_BITS)
		{
			case 0: return D3D12_BLEND_ONE;
			case GLS_SRCBLEND_ZERO: return D3D12_BLEND_ZERO;
			case GLS_SRCBLEND_ONE: return D3D12_BLEND_ONE;
			case GLS_SRCBLEND_DST_COLOR: return D3D12_BLEND_DEST_COLOR;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: return D3D12_BLEND_INV_DEST_COLOR;
			case GLS_SRCBLEND_SRC_ALPHA: return D3D12_BLEND_SRC_ALPHA;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA: return D3D12_BLEND_INV_SRC_ALPHA;
			case GLS_SRCBLEND_DST_ALPHA: return D3D12_BLEND_DEST_ALPHA;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA: return D3D12_BLEND_INV_DEST_ALPHA;
			case GLS_SRCBLEND_ALPHA_SATURATE: return D3D12_BLEND_SRC_ALPHA_SAT;
			default: Q_assert(!"Unsupported source blend mode"); return D3D12_BLEND_ONE;
		}
	}

	static D3D12_BLEND GetD3DDestBlend(uint32_t stateBits)
	{
		switch(stateBits & GLS_DSTBLEND_BITS)
		{
			case 0: return D3D12_BLEND_ZERO;
			case GLS_DSTBLEND_ZERO: return D3D12_BLEND_ZERO;
			case GLS_DSTBLEND_ONE: return D3D12_BLEND_ONE;
			case GLS_DSTBLEND_SRC_COLOR: return D3D12_BLEND_SRC_COLOR;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: return D3D12_BLEND_INV_SRC_COLOR;
			case GLS_DSTBLEND_SRC_ALPHA: return D3D12_BLEND_SRC_ALPHA;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: return D3D12_BLEND_INV_SRC_ALPHA;
			case GLS_DSTBLEND_DST_ALPHA: return D3D12_BLEND_DEST_ALPHA;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA: return D3D12_BLEND_INV_DEST_ALPHA;
			default: Q_assert(!"Unsupported dest blend mode"); return D3D12_BLEND_ONE;
		}
	}

	D3D12_SHADING_RATE GetD3DShadingRate(ShadingRate::Id shadingRate)
	{
		switch(shadingRate)
		{
			case ShadingRate::SR_1x1: return D3D12_SHADING_RATE_1X1;
			case ShadingRate::SR_1x2: return D3D12_SHADING_RATE_1X2;
			case ShadingRate::SR_2x1: return D3D12_SHADING_RATE_2X1;
			case ShadingRate::SR_2x2: return D3D12_SHADING_RATE_2X2;
			case ShadingRate::SR_2x4: return D3D12_SHADING_RATE_2X4;
			case ShadingRate::SR_4x2: return D3D12_SHADING_RATE_4X2;
			case ShadingRate::SR_4x4: return D3D12_SHADING_RATE_4X4;
			default: Q_assert(!"Unsupported shading rate"); return D3D12_SHADING_RATE_1X1;
		}
	}

	static D3D12_BLEND GetAlphaBlendFromColorBlend(D3D12_BLEND colorBlend)
	{
		switch(colorBlend)
		{
			case D3D12_BLEND_SRC_COLOR: return D3D12_BLEND_SRC_ALPHA;
			case D3D12_BLEND_INV_SRC_COLOR: return D3D12_BLEND_INV_SRC_ALPHA;
			case D3D12_BLEND_DEST_COLOR: return D3D12_BLEND_DEST_ALPHA;
			case D3D12_BLEND_INV_DEST_COLOR: return D3D12_BLEND_INV_DEST_ALPHA;
			default: return colorBlend;
		}
	}

	static bool IsD3DDepthFormat(DXGI_FORMAT format)
	{
		switch(format)
		{
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
				return true;
			default:
				return false;
		}
	}

	static const char* GetNameForD3DResourceStates(D3D12_RESOURCE_STATES states)
	{
		switch(states)
		{
			case D3D12_RESOURCE_STATE_COMMON: return "common/present";
			case D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER: return "vertex/constant buffer";
			case D3D12_RESOURCE_STATE_INDEX_BUFFER: return "index buffer";
			case D3D12_RESOURCE_STATE_RENDER_TARGET: return "render target";
			case D3D12_RESOURCE_STATE_UNORDERED_ACCESS: return "UAV";
			case D3D12_RESOURCE_STATE_DEPTH_WRITE: return "depth write";
			case D3D12_RESOURCE_STATE_DEPTH_READ: return "depth read";
			case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE: return "non-pixel shader resource";
			case D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE: return "pixel shader resource";
			case D3D12_RESOURCE_STATE_COPY_DEST: return "copy destination";
			case D3D12_RESOURCE_STATE_COPY_SOURCE: return "copy source";
			case D3D12_RESOURCE_STATE_GENERIC_READ: return "generic read";
			case D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE: return "generic shader resource";
			default: return "???";
		}
	}

	static const char* GetNameForD3DFormat(DXGI_FORMAT format)
	{
		switch(format)
		{
#define FORMAT(Enum) case DXGI_FORMAT_##Enum: return #Enum;
			DXGI_FORMAT_LIST(FORMAT)
			default: return "???";
#undef FORMAT
		}
	}

	static const char* GetHeapTypeName(D3D12_HEAP_TYPE type)
	{
		switch(type)
		{
			case D3D12_HEAP_TYPE_DEFAULT: return "GPU";
			case D3D12_HEAP_TYPE_UPLOAD: return "upload";
			case D3D12_HEAP_TYPE_READBACK: return "readback";
			case D3D12_HEAP_TYPE_CUSTOM: return "UMA";
			default: Q_assert(!"Unsupported heap type"); return "unknown";
		}
	}

	static const char* GetResourceHeapName(ID3D12Resource* resource)
	{
		D3D12_HEAP_PROPERTIES props;
		D3D12_HEAP_FLAGS flags;
		if(SUCCEEDED(resource->GetHeapProperties(&props, &flags)))
		{
			return GetHeapTypeName(props.Type);
		}

		return "unknown";
	}

	static void ValidateResourceStateForBarrier(D3D12_RESOURCE_STATES state)
	{
		if(state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ||
			state == D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			return;
		}

		const D3D12_RESOURCE_STATES readOnly[] =
		{
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			D3D12_RESOURCE_STATE_INDEX_BUFFER,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_DEPTH_READ
		};
		const D3D12_RESOURCE_STATES readWrite[] =
		{
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		};
		const D3D12_RESOURCE_STATES writeOnly[] =
		{
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_STREAM_OUT
		};

		int rBits = 0;
		int wBits = 0;

		for(auto bit : readOnly)
		{
			if(state & bit)
			{
				rBits++;
			}
		}
		for(auto bit : readWrite)
		{
			if(state & bit)
			{
				rBits++;
				wBits++;
			}
		}
		for(auto bit : writeOnly)
		{
			if(state & bit)
			{
				wBits++;
			}
		}

		// MS: "At most one write bit can be set."
		Q_assert(wBits == 0 || wBits == 1);

		if(wBits == 1)
		{
			// MS: "If any write bit is set, then no read bit may be set."
			Q_assert(rBits == 0);
		}
	}

	// returns true if the barrier should be used
	static bool SetBarrier(
		D3D12_RESOURCE_STATES& currentState, D3D12_RESOURCE_BARRIER& barrier,
		ResourceStates::Flags newState, ID3D12Resource* resource)
	{
		const D3D12_RESOURCE_STATES before = currentState;
		const D3D12_RESOURCE_STATES after = GetD3DResourceStates(newState);
		ValidateResourceStateForBarrier(before);
		ValidateResourceStateForBarrier(after);

		if(before & after & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			// note that UAV barriers are unnecessary in a bunch of cases:
			// - before/after access is read-only
			// - before/after access is write-only, but to different ranges
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.UAV.pResource = resource;
		}
		else
		{
			if(before == after)
			{
				return false;
			}

			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = resource;
			barrier.Transition.StateBefore = before;
			barrier.Transition.StateAfter = after;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			currentState = after;
		}

		return true;
	}

	static void ResolveDurationQueries()
	{
		const uint32_t frameIndex = (rhi.frameIndex + 1) % rhi.renderFrameCount;
		const HBuffer hbuffer = rhi.timeStampBuffers[frameIndex];
		const Buffer& buffer = rhi.buffers.Get(hbuffer);

#if defined(D3D_DEBUG)
		if(r_vsync->integer)
		{
			Q_assert(rhi.frameIndex == 0);
			Q_assert(frameIndex == 0);
		}
#endif

		FrameQueries& fq = rhi.frameQueries[frameIndex];
		if(fq.durationQueryCount == 0)
		{
			rhi.resolvedQueries.durationQueryCount = 0;
			return;
		}

		UINT64 gpuFrequencyU64;
		if(FAILED(rhi.mainCommandQueue->GetTimestampFrequency(&gpuFrequencyU64)))
		{
			for(uint32_t q = 0; q < fq.durationQueryCount; ++q)
			{
				DurationQuery& dq = fq.durationQueries[q];
				dq.state = QueryState::Free;
			}
			fq.durationQueryCount = 0;
			rhi.resolvedQueries.durationQueryCount = 0;
		}
		const double gpuFrequencyF64 = (double)gpuFrequencyU64;

		const UINT timestampQueryCount = fq.durationQueryCount * 2;
		rhi.commandList->ResolveQueryData(rhi.timeStampHeaps[frameIndex], D3D12_QUERY_TYPE_TIMESTAMP, 0, timestampQueryCount, buffer.buffer, 0);
		const UINT64* const timeStamps = (const UINT64*)MapBuffer(hbuffer);

		uint32_t* const gpuMicroSeconds = rhi.resolvedQueries.gpuMicroSeconds;
		for(uint32_t q = 0; q < fq.durationQueryCount; ++q)
		{
			DurationQuery& dq = fq.durationQueries[q];
			Q_assert(dq.state == QueryState::Ended);
			if(dq.state != QueryState::Ended)
			{
				gpuMicroSeconds[q] = 0;
				dq.state = QueryState::Free;
				continue;
			}

			const UINT timeStampBeginIndex = q * 2;
			const UINT timeStampEndIndex = timeStampBeginIndex + 1;
			const UINT64 beginTime = timeStamps[timeStampBeginIndex];
			const UINT64 endTime = timeStamps[timeStampEndIndex];
			if(endTime > beginTime)
			{
				const UINT64 elapsed = endTime - beginTime;
				gpuMicroSeconds[q] = (uint32_t)((elapsed / gpuFrequencyF64) * 1000000.0);
			}
			else
			{
				gpuMicroSeconds[q] = 0;
			}

			dq.state = QueryState::Free;
		}
		rhi.resolvedQueries.durationQueryCount = fq.durationQueryCount;
		fq.durationQueryCount = 0;

		UnmapBuffer(hbuffer);
	}

	static void GrabSwapChainTextures()
	{
		for(uint32_t b = 0; b < rhi.swapChainBufferCount; ++b)
		{
			ID3D12Resource* renderTarget;
			D3D(rhi.swapChain->GetBuffer(b, IID_PPV_ARGS(&renderTarget)));

			TextureDesc desc(va("swap chain #%d", b + 1), glConfig.vidWidth, glConfig.vidHeight);
			desc.nativeResource = renderTarget;
			desc.initialState = ResourceStates::PresentBit;
			desc.allowedState = ResourceStates::PresentBit | ResourceStates::RenderTargetBit;
			rhi.renderTargets[b] = CreateTexture(desc);
		}
	}

	static void GetMonitorRefreshRate()
	{
		DWM_TIMING_INFO info = {};
		info.cbSize = sizeof(info);
		if(SUCCEEDED(DwmGetCompositionTimingInfo(NULL, &info)))
		{
			rhie.monitorFrameDurationMS = 1000.0f * ((float)(info.rateRefresh.uiDenominator) / (float)info.rateRefresh.uiNumerator);
		}
		else
		{
			rhie.monitorFrameDurationMS = 0.0f;
		}

		if(r_vsync->integer == 0)
		{
			const float maxFPS = ri.Cvar_Get("com_maxfps", "125", CVAR_ARCHIVE)->value;
			rhie.targetFrameDurationMS = 1000.0f / maxFPS;
			
		}
		else if(rhie.monitorFrameDurationMS  > 0.0f)
		{
			rhie.targetFrameDurationMS = rhie.monitorFrameDurationMS;
		}
		else
		{
			rhie.targetFrameDurationMS = 1.0f / 120.0f; // 120 Hz by default
		}
	}

	static void CreateNullResources()
	{
		{
			TextureDesc desc("null", 1, 1);
			rhi.nullTexture = CreateTexture(desc);
		}
		{
			TextureDesc desc("null RW", 1, 1);
			desc.format = TextureFormat::RGBA32_UNorm;
			desc.initialState = ResourceStates::UnorderedAccessBit;
			desc.allowedState = ResourceStates::UnorderedAccessBit | ResourceStates::PixelShaderAccessBit;
			rhi.nullRWTexture = CreateTexture(desc);
		}
		{
			BufferDesc desc("null", 256, ResourceStates::ShaderAccessBits);
			desc.memoryUsage = MemoryUsage::GPU;
			rhi.nullBuffer = CreateBuffer(desc);
		}
		{
			BufferDesc desc("null RW", 256, ResourceStates::UnorderedAccessBit);
			desc.memoryUsage = MemoryUsage::GPU;
			rhi.nullRWBuffer = CreateBuffer(desc);
		}
		rhi.nullSampler = CreateSampler(SamplerDesc());
	}

	static void CopyDescriptor(ID3D12DescriptorHeap* dstHeap, uint32_t dstIndex, DescriptorHeap& srcHeap, uint32_t srcIndex)
	{
		Q_assert(srcIndex != InvalidDescriptorIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = dstHeap->GetCPUDescriptorHandleForHeapStart();
		dstHandle.ptr += dstIndex * srcHeap.descriptorSize;
		rhi.device->CopyDescriptorsSimple(1, dstHandle, srcHeap.GetCPUHandle(srcIndex), srcHeap.type);
	}

	static UINT BGRAUIntFromFloat(float r, float g, float b)
	{
		const BYTE br = (BYTE)(Com_Clamp(0.0f, 1.0f, r) * 255.0f);
		const BYTE bg = (BYTE)(Com_Clamp(0.0f, 1.0f, g) * 255.0f);
		const BYTE bb = (BYTE)(Com_Clamp(0.0f, 1.0f, b) * 255.0f);

		return PIX_COLOR(br, bg, bb);
	}

	static bool IsTearingSupported()
	{
		HMODULE library = LoadLibraryA("DXGI.dll");
		if(library == NULL)
		{
			ri.Printf(PRINT_WARNING, "D3D12: DXGI.dll couldn't be found or opened\n");
			return false;
		}

		typedef HRESULT(WINAPI* PFN_CreateDXGIFactory)(REFIID riid, _Out_ void** ppFactory);
		PFN_CreateDXGIFactory pCreateDXGIFactory = (PFN_CreateDXGIFactory)GetProcAddress(library, "CreateDXGIFactory");
		if(pCreateDXGIFactory == NULL)
		{
			FreeLibrary(library);
			ri.Printf(PRINT_WARNING, "D3D12: Failed to locate CreateDXGIFactory in DXGI.dll\n");
			return false;
		}

		HRESULT hr;
		BOOL enabled = FALSE;
		IDXGIFactory5* pFactory;
		hr = (*pCreateDXGIFactory)(__uuidof(IDXGIFactory5), (void**)&pFactory);
		if(FAILED(hr))
		{
			FreeLibrary(library);
			ri.Printf(PRINT_WARNING, "D3D12: 'CreateDXGIFactory' failed with code 0x%08X (%s)\n", (unsigned int)hr, GetSystemErrorString(hr));
			return false;
		}
		hr = pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &enabled, sizeof(enabled));
		pFactory->Release();
		FreeLibrary(library);

		if(FAILED(hr))
		{
			ri.Printf(PRINT_WARNING, "D3D12: 'IDXGIFactory5::CheckFeatureSupport' failed with code 0x%08X (%s)\n", (unsigned int)hr, GetSystemErrorString(hr));
			return false;
		}

		return enabled != 0;
	}

	static UINT GetSwapChainFlags()
	{
		UINT flags = 0;
		if(r_vsync->integer)
		{
			flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		}
		else
		{
			flags = rhi.isTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
		}

		return flags;
	}

	static void WaitForTempCommandList()
	{
		rhi.tempFence.WaitOnCPU(rhi.tempFenceValue);
		if(rhi.tempCommandListOpen)
		{
			rhi.tempCommandList->Close();
		}
		D3D(rhi.tempCommandAllocator->Reset());
		D3D(rhi.tempCommandList->Reset(rhi.tempCommandAllocator, NULL));
		rhi.tempCommandListOpen = true;
	}

	static void WaitForSwapChain()
	{
		if(rhi.frameLatencyWaitableObject != NULL && rhi.frameLatencyWaitNeeded)
		{
			Q_assert(r_vsync->integer != 0);
			WaitForSingleObjectEx(rhi.frameLatencyWaitableObject, INFINITE, TRUE);
			rhi.frameLatencyWaitNeeded = false;
		}
	}

	static void DrawResourceUsage()
	{
		if(BeginTable("Handles", 3))
		{
			TableHeader(3, "Type", "Count", "Max");

#define ITEM(Name, Variable) TableRow(3, Name, va("%d", (int)Variable.CountUsedSlots()), va("%d", (int)Variable.size))
			ITEM("Buffers", rhi.buffers);
			ITEM("Textures", rhi.textures);
			ITEM("Root Signatures", rhi.rootSignatures);
			ITEM("Descriptor Tables", rhi.descriptorTables);
			ITEM("Pipelines", rhi.pipelines);
			ITEM("Shaders", rhi.shaders);
			ITEM("Samplers", rhi.samplers);
#undef ITEM
			TableRow(3, "Duration Queries",
				va("%d", rhi.frameQueries[rhi.frameIndex].durationQueryCount),
				va("%d", MaxDurationQueries));

			ImGui::EndTable();
		}

		ImGui::NewLine();
		if(BeginTable("Descriptors", 3))
		{
			TableHeader(3, "Type", "Count", "Max");

#define ITEM(Name, Variable) TableRow(3, Name, va("%d", (int)Variable.allocatedItemCount), va("%d", (int)Variable.size))
			ITEM("CBV/SRV/UAV", rhi.descHeapGeneric.freeList);
			ITEM("Samplers", rhi.descHeapSamplers.freeList);
			ITEM("RTV", rhi.descHeapRTVs.freeList);
			ITEM("DSV", rhi.descHeapDSVs.freeList);
#undef ITEM

			ImGui::EndTable();
		}

		ImGui::NewLine();
		if(BeginTable("Memory", 2))
		{
			D3D12MA::Budget budget;
			rhi.allocator->GetBudget(&budget, NULL);
			TableRow2("UMA", rhi.allocator->IsUMA());
			TableRow2("Cache coherent UMA", rhi.allocator->IsCacheCoherentUMA());
			TableRow(2, "Total", Com_FormatBytes(rhi.allocator->GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_LOCAL)));
			TableRow(2, "Budget", Com_FormatBytes(budget.BudgetBytes));
			TableRow(2, "Usage", Com_FormatBytes(budget.UsageBytes));
			TableRow(2, "Allocated", Com_FormatBytes(budget.Stats.BlockBytes));
			TableRow(2, "Used", Com_FormatBytes(budget.Stats.AllocationBytes));
			TableRow(2, "Block count", va("%d", budget.Stats.BlockCount));
			TableRow(2, "Allocation count", va("%d", budget.Stats.AllocationCount));

			ImGui::EndTable();
		}
	}

	static void DrawCaps()
	{
		if(BeginTable("Capabilities", 2))
		{
			TableRow(2, "Adapter", rhi.adapterName);

			D3D12_FEATURE_DATA_D3D12_OPTIONS options0 = { 0 };
			if(SUCCEEDED(rhi.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options0, sizeof(options0))))
			{
				const char* tier = "Unknown";
				switch(options0.ResourceBindingTier)
				{
					case D3D12_RESOURCE_BINDING_TIER_1: tier = "1"; break;
					case D3D12_RESOURCE_BINDING_TIER_2: tier = "2"; break;
					case D3D12_RESOURCE_BINDING_TIER_3: tier = "3"; break;
					default: break;
				}
				TableRow(2, "Resource binding tier", tier);
			}

			D3D12_FEATURE_DATA_D3D12_OPTIONS2 options2 = { 0 };
			if(SUCCEEDED(rhi.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &options2, sizeof(options2))))
			{
				TableRow2("Depth bounds test", options2.DepthBoundsTestSupported ? "YES" : "NO");
			}

			D3D12_FEATURE_DATA_ARCHITECTURE arch0 = { 0 };
			if(SUCCEEDED(rhi.device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch0, sizeof(arch0))))
			{
				TableRow2("Tile-based renderer", arch0.TileBasedRenderer ? "YES" : "NO");
			}

			D3D12_FEATURE_DATA_ROOT_SIGNATURE root0 = {};
			if(SUCCEEDED(rhi.device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &root0, sizeof(root0))))
			{
				const char* version = "Unknown";
				switch(root0.HighestVersion)
				{
					case D3D_ROOT_SIGNATURE_VERSION_1_0: version = "1.0"; break;
					case D3D_ROOT_SIGNATURE_VERSION_1_1: version = "1.1"; break;
					default: break;
				}
				TableRow(2, "Root signature version", version);
			}

			D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = { 0 };
			if(SUCCEEDED(rhi.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
			{
				const char* tier = "Unknown";
				switch(options5.RenderPassesTier)
				{
					case D3D12_RENDER_PASS_TIER_0: tier = "0"; break;
					case D3D12_RENDER_PASS_TIER_1: tier = "1"; break;
					case D3D12_RENDER_PASS_TIER_2: tier = "2"; break;
					default: break;
				}
				TableRow(2, "Render passes tier", tier);

				tier = "Unknown";
				switch(options5.RaytracingTier)
				{
					case D3D12_RAYTRACING_TIER_NOT_SUPPORTED: tier = "Not supported"; break;
					case D3D12_RAYTRACING_TIER_1_0: tier = "1.0"; break;
					case D3D12_RAYTRACING_TIER_1_1: tier = "1.1"; break;
					default: break;
				}
				TableRow(2, "Raytracing (DXR) tier", tier);
			}

			D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = { 0 };
			if(SUCCEEDED(rhi.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6))))
			{
				const char* tier = "Unknown";
				switch(options6.VariableShadingRateTier)
				{
					case D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED: tier = "N/A"; break;
					case D3D12_VARIABLE_SHADING_RATE_TIER_1: tier = "1"; break;
					case D3D12_VARIABLE_SHADING_RATE_TIER_2: tier = "2"; break;
					default: break;
				}
				TableRow(2, "Variable-rate shading (VRS) tier", tier);

				TableRow(2, "VRS: 2x4, 4x2, 4x4 support", options6.AdditionalShadingRatesSupported ? "YES" : "NO");
			}

			// the validation layer reports live objects at shutdown when NvAPI_D3D12_QueryCpuVisibleVidmem is called
#if defined(RHI_ENABLE_NVAPI)
			NvU64 cvvTotal, cvvFree;
			if(NvAPI_D3D12_QueryCpuVisibleVidmem(rhi.device, &cvvTotal, &cvvFree) == NvAPI_Status::NVAPI_OK &&
				cvvTotal > 0)
			{
				TableRow(2, "CPU Visible VRAM Total", Com_FormatBytes(cvvTotal));
				TableRow(2, "CPU Visible VRAM Free", Com_FormatBytes(cvvFree));
			}
			else
			{
				TableRow(2, "CPU Visible VRAM", "N/A");
			}
#endif

			ImGui::EndTable();
		}
	}

	static void DrawTextures()
	{
		static char filter[256];
		if(ImGui::Button("Clear filter"))
		{
			filter[0] = '\0';
		}
		ImGui::SameLine();
		ImGui::InputText(" ", filter, ARRAY_LEN(filter));

		if(BeginTable("Textures", 4))
		{
			TableHeader(4, "Name", "State", "Size", "Format");

			int i = 0;
			Texture* texture;
			HTexture htexture;
			while(rhi.textures.FindNext(&texture, &htexture, &i))
			{
				if(filter[0] != '\0' && !Com_Filter(filter, texture->desc.name))
				{
					continue;
				}
				const D3D12_RESOURCE_DESC desc = texture->texture->GetDesc();
				const uint64_t byteCount = texture->allocation != NULL ? texture->allocation->GetSize() : 0;
				TableRow(4,
					texture->desc.name,
					GetNameForD3DResourceStates(texture->currentState),
					Com_FormatBytes(byteCount),
					GetNameForD3DFormat(desc.Format));
			}

			ImGui::EndTable();
		}
	}

	static void DrawBuffers()
	{
		static char filter[256];
		if(ImGui::Button("Clear filter"))
		{
			filter[0] = '\0';
		}
		ImGui::SameLine();
		ImGui::InputText(" ", filter, ARRAY_LEN(filter));

		if(BeginTable("Buffers", 4))
		{
			TableHeader(4, "Buffer", "State", "Heap", "Size");

			int i = 0;
			Buffer* buffer;
			HBuffer hbuffer;
			while(rhi.buffers.FindNext(&buffer, &hbuffer, &i))
			{
				if(filter[0] != '\0' && !Com_Filter(filter, buffer->desc.name))
				{
					continue;
				}
				TableRow(4,
					buffer->desc.name,
					GetNameForD3DResourceStates(buffer->currentState),
					GetResourceHeapName(buffer->buffer),
					Com_FormatBytes(buffer->allocation->GetSize()));
			}

			ImGui::EndTable();
		}
	}

	typedef void (*UICallback)();

	static void DrawSection(const char* name, UICallback callback)
	{
		if(ImGui::BeginTabItem(name))
		{
			(*callback)();
			ImGui::EndTabItem();
		}
	}

	static void DrawGUI()
	{
		static bool resourcesActive = false;
		ToggleBooleanWithShortcut(resourcesActive, ImGuiKey_R);
		GUI_AddMainMenuItem(GUI_MainMenu::Info, "RHI Resources", "Ctrl+R", &resourcesActive);
		if(resourcesActive)
		{
			if(ImGui::Begin("Direct3D 12 RHI", &resourcesActive))
			{
				ImGui::BeginTabBar("Tabs#RHI");
				DrawSection("Resources", &DrawResourceUsage);
				DrawSection("Caps", &DrawCaps);
				DrawSection("Textures", &DrawTextures);
				DrawSection("Buffers", &DrawBuffers);
				ImGui::EndTabBar();
			}
			ImGui::End();
		}
	}

	bool Init()
	{
		Sys_V_Init();

		if(rhi.device != NULL)
		{
			DXGI_SWAP_CHAIN_DESC desc;
			D3D(rhi.swapChain->GetDesc(&desc));

			// V-Sync toggles require changing the swap chain flags,
			// which means ResizeBuffers can't be used
			const bool vsync = r_vsync->integer != 0;
			rhi.renderFrameCount = vsync ? 1 : 2;

			if(glInfo.winWidth != desc.BufferDesc.Width || 
				glInfo.winHeight != desc.BufferDesc.Height ||
				vsync != rhi.vsync)
			{
				WaitUntilDeviceIsIdle();

				for(uint32_t f = 0; f < rhi.swapChainBufferCount; ++f)
				{
					DestroyTexture(rhi.renderTargets[f]);
				}
		
				const UINT flags = GetSwapChainFlags();
				if(vsync == rhi.vsync)
				{
					D3D(rhi.swapChain->ResizeBuffers(desc.BufferCount, glInfo.winWidth, glInfo.winHeight, desc.BufferDesc.Format, flags));
				}
				else
				{
					if(rhi.frameLatencyWaitableObject != NULL)
					{
						CloseHandle(rhi.frameLatencyWaitableObject);
						rhi.frameLatencyWaitableObject = NULL;
					}

					COM_RELEASE(rhi.swapChain);

					IDXGISwapChain* dxgiSwapChain;
					DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
					swapChainDesc.BufferCount = rhi.swapChainBufferCount;
					swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					swapChainDesc.BufferDesc.Width = glInfo.winWidth;
					swapChainDesc.BufferDesc.Height = glInfo.winHeight;
					swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
					swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
					swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
					swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
					swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
					swapChainDesc.Flags = flags;
					swapChainDesc.OutputWindow = GetActiveWindow();
					swapChainDesc.SampleDesc.Count = 1;
					swapChainDesc.SampleDesc.Quality = 0;
					swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
					swapChainDesc.Windowed = TRUE;
					D3D(rhi.factory->CreateSwapChain(rhi.mainCommandQueue, &swapChainDesc, &dxgiSwapChain));

					D3D(dxgiSwapChain->QueryInterface(IID_PPV_ARGS(&rhi.swapChain)));
					COM_RELEASE(dxgiSwapChain);

					if(vsync)
					{
						rhi.frameLatencyWaitableObject = rhi.swapChain->GetFrameLatencyWaitableObject();
						rhi.frameLatencyWaitNeeded = true;
						D3D(rhi.swapChain->SetMaximumFrameLatency(1));
					}
				}

				GrabSwapChainTextures();

				rhi.swapChainBufferIndex = rhi.swapChain->GetCurrentBackBufferIndex();

				for(uint32_t f = 0; f < FrameCount; ++f)
				{
					rhi.mainFenceValues[f] = 0;
				}

				rhi.readback.ResizeIfNeeded();
			}

			GetMonitorRefreshRate();

			rhi.tempStringAllocator.Clear();

			rhi.vsync = vsync;

			return false;
		}

		// @NOTE: we can't use memset because of the StaticPool members
		new (&rhi) RHIPrivate();

		// check for the presence of our 3 DLLs ASAP
		{
			HMODULE coreModule = LoadLibraryA("cnq3/D3D12Core.dll");
			if(coreModule == NULL)
			{
				ri.Error(ERR_FATAL, "Failed to locate/open cnq3/D3D12Core.dll\n");
			}
			FreeLibrary(coreModule);

			rhi.dxilModule = LoadLibraryA("cnq3/dxil.dll");
			if(rhi.dxilModule == NULL)
			{
				ri.Error(ERR_FATAL, "Failed to locate/open cnq3/dxil.dll\n");
			}

			rhi.dxcModule = LoadLibraryA("cnq3/dxcompiler.dll");
			if(rhi.dxcModule == NULL)
			{
				ri.Error(ERR_FATAL, "Failed to locate/open cnq3/dxcompiler.dll\n");
			}
		}

		rhi.persStringAllocator.Init(rhi.persStringData, sizeof(rhi.persStringData));
		rhi.tempStringAllocator.Init(rhi.tempStringData, sizeof(rhi.tempStringData));

#if defined(D3D_DEBUG)
		if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&rhi.debug))))
		{
			// calling after device creation will remove the device
			// if you hit this error:
			// "D3D12 SDKLayers dll does not match the D3D12SDKVersion of D3D12 Core dll."
			// make sure your D3D12SDKVersion and D3D12SDKPath are valid!
			rhi.debug->EnableDebugLayer();

#if defined(D3D_GPU_BASED_VALIDATION)
			ID3D12Debug1* debug1;
			if(SUCCEEDED(rhi.debug->QueryInterface(IID_PPV_ARGS(&debug1))))
			{
				debug1->SetEnableGPUBasedValidation(TRUE);
				debug1->SetEnableSynchronizedCommandQueueValidation(TRUE);
			}
#endif
		}

		UINT dxgiFactoryFlags = 0;
		if(SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&rhi.dxgiInfoQueue))))
		{
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			rhi.dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
			rhi.dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		}
#endif

#if defined(D3D_DEBUG)
		D3D(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&rhi.factory)));
#else
		D3D(CreateDXGIFactory1(IID_PPV_ARGS(&rhi.factory)));
#endif

		rhi.adapter = FindMostSuitableAdapter(rhi.factory, r_gpuPreference->integer);
		{
			char adapterName[256];
			const char* adapterNamePtr = "unknown";
			DXGI_ADAPTER_DESC1 desc;
			if(SUCCEEDED(rhi.adapter->GetDesc1(&desc)) &&
				WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName, sizeof(adapterName), NULL, NULL) > 0)
			{
				adapterNamePtr = adapterName;
			}
			ri.Printf(PRINT_ALL, "Selected graphics adapter: %s\n", adapterNamePtr);
			Q_strncpyz(rhi.adapterName, adapterNamePtr, sizeof(rhi.adapterName));
		}

		D3D(D3D12CreateDevice(rhi.adapter, FeatureLevel, IID_PPV_ARGS(&rhi.device)));

		{
			D3D12MA::ALLOCATOR_DESC desc = {};
			desc.pDevice = rhi.device;
			desc.pAdapter = rhi.adapter;
			desc.Flags = D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED;
			D3D(D3D12MA::CreateAllocator(&desc, &rhi.allocator));
		}

		if(rhi.allocator->IsCacheCoherentUMA())
		{
			D3D12MA::POOL_DESC poolDesc = {};
			poolDesc.HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
			poolDesc.HeapProperties.CreationNodeMask = 0;
			poolDesc.HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // system
			poolDesc.HeapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
			poolDesc.HeapProperties.VisibleNodeMask = 0;
			poolDesc.HeapFlags = D3D12_HEAP_FLAG_NONE;
			poolDesc.Flags = D3D12MA::POOL_FLAG_NONE;

			D3D12MA::Pool* pool;
			if(SUCCEEDED(rhi.allocator->CreatePool(&poolDesc, &pool)))
			{
				rhi.umaPool = pool;
			}
		}

#if defined(D3D_DEBUG)
		if(rhi.debug)
		{
			rhi.device->QueryInterface(IID_PPV_ARGS(&rhi.infoQueue));
			if(rhi.infoQueue)
			{
				rhi.infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				rhi.infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
				rhi.infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

				D3D12_MESSAGE_ID filteredMessages[] =
				{
					// can't remember what this one is for...
					//D3D12_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
					// clear color mismatch will happen when going through a teleporter
					D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
				};
				D3D12_INFO_QUEUE_FILTER filter = { 0 };
				filter.DenyList.NumIDs = ARRAY_LEN(filteredMessages);
				filter.DenyList.pIDList = filteredMessages;
				rhi.infoQueue->AddStorageFilterEntries(&filter);
			}
		}

		if(rhi.debug)
		{
			ID3D12DebugDevice1* debugDevice1;
			if(SUCCEEDED(rhi.device->QueryInterface(IID_PPV_ARGS(&debugDevice1))))
			{
				// defaults:
				// D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE_UNGUARDED_VALIDATION
				// 256
				// D3D12_GPU_BASED_VALIDATION_PIPELINE_STATE_CREATE_FLAG_NONE
				D3D12_DEBUG_DEVICE_GPU_BASED_VALIDATION_SETTINGS gbv = {};
				gbv.DefaultShaderPatchMode = D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE_GUARDED_VALIDATION;
				gbv.MaxMessagesPerCommandList = 1024; // defaults to 256
				gbv.PipelineStateCreateFlags = D3D12_GPU_BASED_VALIDATION_PIPELINE_STATE_CREATE_FLAG_FRONT_LOAD_CREATE_GUARDED_VALIDATION_SHADERS;
				debugDevice1->SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_GPU_BASED_VALIDATION_SETTINGS, &gbv, sizeof(gbv));

				// default: D3D12_DEBUG_FEATURE_NONE
				const D3D12_DEBUG_FEATURE features =
					D3D12_DEBUG_FEATURE_ALLOW_BEHAVIOR_CHANGING_DEBUG_AIDS |
					D3D12_DEBUG_FEATURE_CONSERVATIVE_RESOURCE_STATE_TRACKING;
				debugDevice1->SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_FEATURE_FLAGS, &features, sizeof(features));
				
				COM_RELEASE(debugDevice1);
			}
		}
#endif

		{
			uint16_t* freeList = rhi.descriptorFreeListData;
			rhi.descHeapGeneric.Create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxCPUGenericDescriptors, freeList, "all-encompassing CBV SRV UAV");
			freeList += MaxCPUGenericDescriptors;
			rhi.descHeapSamplers.Create(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MaxCPUSamplerDescriptors, freeList, "all-encompassing sampler");
			freeList += MaxCPUSamplerDescriptors;
			rhi.descHeapRTVs.Create(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MaxCPURTVDescriptors, freeList, "all-encompassing RTV");
			freeList += MaxCPURTVDescriptors;
			rhi.descHeapDSVs.Create(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MaxCPUDSVDescriptors, freeList, "all-encompassing DSV");
		}

		{
			D3D12_COMMAND_QUEUE_DESC commandQueueDesc = { 0 };
			commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			commandQueueDesc.NodeMask = 0;
			D3D(rhi.device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&rhi.mainCommandQueue)));
			SetDebugName(rhi.mainCommandQueue, "main", D3DResourceType::CommandQueue);

			commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
			D3D(rhi.device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&rhi.computeCommandQueue)));
			SetDebugName(rhi.computeCommandQueue, "compute", D3DResourceType::CommandQueue);
		}

		rhi.isTearingSupported = IsTearingSupported();
		rhi.swapChainBufferCount = 2;
		rhi.renderFrameCount = r_vsync->integer ? 1 : 2;

		{
			const UINT flags = GetSwapChainFlags();

			IDXGISwapChain* dxgiSwapChain;
			DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
			swapChainDesc.BufferCount = rhi.swapChainBufferCount;
			swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.BufferDesc.Width = glInfo.winWidth;
			swapChainDesc.BufferDesc.Height = glInfo.winHeight;
			swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
			swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.Flags = flags;
			swapChainDesc.OutputWindow = GetActiveWindow();
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.SampleDesc.Quality = 0;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.Windowed = TRUE;
			D3D(rhi.factory->CreateSwapChain(rhi.mainCommandQueue, &swapChainDesc, &dxgiSwapChain));
			rhi.vsync = r_vsync->integer != 0;

			D3D(dxgiSwapChain->QueryInterface(IID_PPV_ARGS(&rhi.swapChain)));
			rhi.swapChainBufferIndex = rhi.swapChain->GetCurrentBackBufferIndex();
			COM_RELEASE(dxgiSwapChain);

			if(r_vsync->integer)
			{
				rhi.frameLatencyWaitableObject = rhi.swapChain->GetFrameLatencyWaitableObject();
				rhi.frameLatencyWaitNeeded = true;
				D3D(rhi.swapChain->SetMaximumFrameLatency(1));
			}

			GrabSwapChainTextures();
		}

		GetMonitorRefreshRate();

		for(UINT f = 0; f < FrameCount; ++f)
		{
			D3D(rhi.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&rhi.mainCommandAllocators[f])));
			SetDebugName(rhi.mainCommandAllocators[f], va("main #%d", f + 1), D3DResourceType::CommandAllocator);
		}

		// get command list ready to use during init
		D3D(rhi.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, rhi.mainCommandAllocators[rhi.frameIndex], NULL, IID_PPV_ARGS(&rhi.mainCommandList)));
		SetDebugName(rhi.mainCommandList, "main", D3DResourceType::CommandList);

		D3D(rhi.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&rhi.tempCommandAllocator)));
		SetDebugName(rhi.tempCommandAllocator, "temp", D3DResourceType::CommandAllocator);

		// the temp command list is always left open for the user
		D3D(rhi.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, rhi.tempCommandAllocator, NULL, IID_PPV_ARGS(&rhi.tempCommandList)));
		SetDebugName(rhi.tempCommandList, "temp", D3DResourceType::CommandList);
		rhi.tempCommandListOpen = true;

		// the active/bound command list is the main one by default
		rhi.commandList = rhi.mainCommandList;

		rhi.mainFence.Create(rhi.mainFenceValues[rhi.frameIndex], "main command queue");

		rhi.tempFence.Create(rhi.tempFenceValue, "temp command queue");

		rhi.upload.Create();

		rhi.readback.Create();

		for(uint32_t f = 0; f < FrameCount; ++f)
		{
			D3D12_QUERY_HEAP_DESC desc = { 0 };
			desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
			desc.Count = MaxDurationQueries * 2;
			desc.NodeMask = 0;
			D3D(rhi.device->CreateQueryHeap(&desc, IID_PPV_ARGS(&rhi.timeStampHeaps[f])));
			SetDebugName(rhi.timeStampHeaps[f], va("timestamp #%d", f + 1), D3DResourceType::QueryHeap);
		}

		for(uint32_t f = 0; f < FrameCount; ++f)
		{
			const uint32_t byteCount = MaxDurationQueries * 2 * sizeof(UINT64);
			BufferDesc desc(va("timestamp readback #%d", f + 1), byteCount, ResourceStates::CopySourceBit);
			desc.memoryUsage = MemoryUsage::Readback;
			rhi.timeStampBuffers[f] = CreateBuffer(desc);
		}

		CreateNullResources();

		// queue some actual work...

		D3D(rhi.commandList->Close());

		WaitUntilDeviceIsIdle();

#if defined(RHI_ENABLE_NVAPI)
		{
			const NvAPI_Status nr = NvAPI_Initialize();
			if(nr == NvAPI_Status::NVAPI_OK)
			{
				NvAPI_ShortString version;
				if(NvAPI_GetInterfaceVersionString(version) == NvAPI_Status::NVAPI_OK)
				{
					ri.Printf(PRINT_ALL, "Opened nvapi.dll (%s)\n", version);
				}
				else
				{
					ri.Printf(PRINT_ALL, "Opened nvapi.dll\n");
				}
			}
			else
			{
				NvAPI_ShortString desc;
				if(NvAPI_GetErrorMessage(nr, desc) == NvAPI_Status::NVAPI_OK)
				{
					ri.Printf(PRINT_WARNING, "Failed to load nvapi.dll: %s\n", desc);
				}
				else
				{
					ri.Printf(PRINT_WARNING, "Failed to load nvapi.dll\n");
				}
			}
		}
#endif

		rhi.pix.module = LoadLibraryA("cnq3/WinPixEventRuntime.dll");
		if(rhi.pix.module != NULL)
		{
			rhi.pix.BeginEventOnCommandList = (Pix::BeginEventOnCommandListPtr)GetProcAddress(rhi.pix.module, "PIXBeginEventOnCommandList");
			rhi.pix.EndEventOnCommandList = (Pix::EndEventOnCommandListPtr)GetProcAddress(rhi.pix.module, "PIXEndEventOnCommandList");
			rhi.pix.SetMarkerOnCommandList = (Pix::SetMarkerOnCommandListPtr)GetProcAddress(rhi.pix.module, "PIXSetMarkerOnCommandList");
			rhi.pix.canBeginAndEnd = rhi.pix.BeginEventOnCommandList != NULL && rhi.pix.EndEventOnCommandList != NULL;
		}

		typedef HRESULT (__stdcall* DxcCreateInstancePtr)(REFCLSID, REFIID, LPVOID*);
		DxcCreateInstancePtr dxcCreateInstance = (DxcCreateInstancePtr)GetProcAddress(rhi.dxcModule, "DxcCreateInstance");
		if(dxcCreateInstance == NULL)
		{
			ri.Error(ERR_FATAL, "Failed to locate DxcCreateInstance in cnq3/dxcompiler.dll\n");
		}
		D3D(dxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&rhi.dxcUtils)));
		D3D(dxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&rhi.dxcCompiler)));

		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
			if(SUCCEEDED(rhi.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6))))
			{
				rhi.baseVRSSupport = options6.VariableShadingRateTier != D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
				rhi.extendedVRSSupport = rhi.baseVRSSupport && options6.AdditionalShadingRatesSupported;
			}

			const char* modeLists[] = { "1x1", "1x1 2x1 1x2 2x2", "1x1 2x1 1x2 2x2 4x2 2x4 4x4" };
			const int listIndex = rhi.extendedVRSSupport ? 2 : (rhi.baseVRSSupport ? 1 : 0);
			ri.Printf(PRINT_ALL, "Supported VRS modes: %s\n", modeLists[listIndex]);
		}

		glInfo.maxTextureSize = MAX_TEXTURE_SIZE;
		glInfo.maxAnisotropy = 16;
		glInfo.depthFadeSupport = qtrue;

		Q_strncpyz(glConfig.vendor_string, rhi.adapterName, sizeof(glConfig.vendor_string));
		Q_strncpyz(glConfig.renderer_string, "Direct3D 12", sizeof(glConfig.renderer_string));

		Q_strncpyz(rhiInfo.name, "Direct3D 12", sizeof(rhiInfo.name));
		Q_strncpyz(rhiInfo.adapter, rhi.adapterName, sizeof(rhiInfo.adapter));
		rhiInfo.hasTearing = rhi.isTearingSupported;
		rhiInfo.hasBaseVRS = rhi.baseVRSSupport;
		rhiInfo.hasExtendedVRS = rhi.extendedVRSSupport;
		rhiInfo.isUMA = rhi.allocator->IsUMA();
		rhiInfo.isCacheCoherentUMA = rhi.allocator->IsCacheCoherentUMA();

		rhi.initialized = true;

		return true;
	}

	void ShutDown(bool destroyWindow)
	{
#define DESTROY_POOL(Name, Func) DestroyPool(rhi.Name, &Func, !!destroyWindow);

		if(!destroyWindow &&
			r_gpuPreference->latchedString != NULL &&
			Q_stricmp(r_gpuPreference->latchedString, r_gpuPreference->string) != 0)
		{
			destroyWindow = true;
		}

		if(rhi.frameBegun)
		{
			backEnd.renderFrame = qfalse;
			EndFrame();
			backEnd.renderFrame = qtrue;
		}

		if(!destroyWindow)
		{
			WaitUntilDeviceIsIdle();

			rhi.texturesToTransition.Clear();
			rhi.buffersToTransition.Clear();

			DESTROY_POOL_LIST(DESTROY_POOL);

			return;
		}

		rhi.initialized = false;

		FreeLibrary(rhi.pix.module);

		WaitUntilDeviceIsIdle();

		if(rhi.frameLatencyWaitableObject != NULL)
		{
			CloseHandle(rhi.frameLatencyWaitableObject);
		}

		rhi.upload.Release();
		rhi.readback.Release();
		rhi.mainFence.Release();
		rhi.tempFence.Release();
		rhi.descHeapGeneric.Release();
		rhi.descHeapSamplers.Release();
		rhi.descHeapRTVs.Release();
		rhi.descHeapDSVs.Release();

		DESTROY_POOL_LIST(DESTROY_POOL);

		COM_RELEASE(rhi.dxcCompiler);
		COM_RELEASE(rhi.dxcUtils);
		COM_RELEASE_ARRAY(rhi.timeStampHeaps);
		COM_RELEASE(rhi.mainCommandList);
		COM_RELEASE_ARRAY(rhi.mainCommandAllocators);
		COM_RELEASE(rhi.tempCommandList);
		COM_RELEASE(rhi.tempCommandAllocator);
		COM_RELEASE(rhi.swapChain);
		COM_RELEASE(rhi.computeCommandQueue);
		COM_RELEASE(rhi.mainCommandQueue);
		COM_RELEASE(rhi.infoQueue);
		COM_RELEASE(rhi.umaPool);
		COM_RELEASE(rhi.allocator);
		COM_RELEASE(rhi.device);
		COM_RELEASE(rhi.adapter);
		COM_RELEASE(rhi.factory);
		COM_RELEASE(rhi.dxgiInfoQueue);
		COM_RELEASE(rhi.debug);

		FreeLibrary(rhi.dxilModule);
		FreeLibrary(rhi.dxcModule);

#if defined(RHI_ENABLE_NVAPI)
		NvAPI_Unload();
#endif
		
#if defined(D3D_DEBUG)
		IDXGIDebug1* debug = NULL;
		if(SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
		{
			// DXGI_DEBUG_RLO_ALL is DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL
			OutputDebugStringA("**** >>>> CNQ3: calling ReportLiveObjects\n");
			const HRESULT hr = debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			OutputDebugStringA(va("**** >>>> CNQ3: ReportLiveObjects returned 0x%08X (%s)\n", (unsigned int)hr, GetSystemErrorString(hr)));
			debug->Release();
		}
#endif

#undef DESTROY_POOL
	}

	void BeginFrame()
	{
		if(rhi.frameBegun)
		{
			Sys_DebugPrintf("BeginFrame already called!\n");
			return;
		}
		rhi.frameBegun = true;

		rhi.beforeRenderingUS = Sys_Microseconds();

		WaitForSwapChain();

		{
			const UINT64 currentFenceValue = rhi.mainFenceValues[rhi.frameIndex];
#if RHI_DEBUG_FENCE
			Sys_DebugPrintf("Wait: %d (BeginFrame)\n", (int)currentFenceValue);
#endif
			rhi.mainFence.WaitOnCPU(currentFenceValue);
			rhi.frameIndex = (rhi.frameIndex + 1) % rhi.renderFrameCount;
			rhi.mainFenceValues[rhi.frameIndex] = currentFenceValue + 1;
			rhi.swapChainBufferIndex = rhi.swapChain->GetCurrentBackBufferIndex();
		}

		DrawGUI();

		Q_assert(rhi.commandList == rhi.mainCommandList);

		rhi.currentRootSignature = RHI_MAKE_NULL_HANDLE();

		WaitForTempCommandList();

		// wait for pending copies from the upload manager to be finished
		rhi.upload.WaitToStartDrawing(rhi.mainCommandQueue);

		rhie.inputToRenderUS = (uint32_t)(Sys_Microseconds() - rhi.beforeInputSamplingUS);

		// reclaim used memory and start recording
		D3D(rhi.mainCommandAllocators[rhi.frameIndex]->Reset());
		D3D(rhi.commandList->Reset(rhi.mainCommandAllocators[rhi.frameIndex], NULL));

		rhi.frameDurationQueryIndex = CmdBeginDurationQuery();

		rhi.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const TextureBarrier barrier(rhi.renderTargets[rhi.swapChainBufferIndex], ResourceStates::RenderTargetBit);
		CmdBarrier(1, &barrier);

		static TextureBarrier textureBarriers[MAX_DRAWIMAGES];
		static BufferBarrier bufferBarriers[64];
		for(uint32_t t = 0; t < rhi.texturesToTransition.count; ++t)
		{
			const HTexture handle = rhi.texturesToTransition[t];
			const Texture& texture = rhi.textures.Get(handle);
			textureBarriers[t] = TextureBarrier(handle, texture.desc.initialState);
		}
		for(uint32_t b = 0; b < rhi.buffersToTransition.count; ++b)
		{
			const HBuffer handle = rhi.buffersToTransition[b];
			const Buffer& buffer = rhi.buffers.Get(handle);
			bufferBarriers[b] = BufferBarrier(handle, buffer.desc.initialState);
		}
		CmdBarrier(rhi.texturesToTransition.count, textureBarriers, rhi.buffersToTransition.count, bufferBarriers);
		rhi.texturesToTransition.Clear();
		rhi.buffersToTransition.Clear();

		CmdInsertDebugLabel("RHI::BeginFrame", 0.8f, 0.8f, 0.8f);
	}

	void EndFrame()
	{
		if(!rhi.frameBegun)
		{
			Sys_DebugPrintf("EndFrame already called!\n");
			return;
		}
		rhi.frameBegun = false;

		CmdInsertDebugLabel("RHI::EndFrame", 0.8f, 0.8f, 0.8f);

		const TextureBarrier barrier(rhi.renderTargets[rhi.swapChainBufferIndex], ResourceStates::PresentBit);
		CmdBarrier(1, &barrier);

		CmdEndDurationQuery(rhi.frameDurationQueryIndex);

		// needs to happens before the command list is closed
		ResolveDurationQueries();

		// stop recording
		D3D(rhi.commandList->Close());

#if RHI_DEBUG_FENCE
		Sys_DebugPrintf("Signal: %d (EndFrame)\n", rhi.mainFenceValues[rhi.frameIndex]);
#endif
		rhi.mainFence.Signal(rhi.mainCommandQueue, rhi.mainFenceValues[rhi.frameIndex]);

		const int64_t currentTimeUS = Sys_Microseconds();
		rhie.inputToPresentUS = (uint32_t)(currentTimeUS - rhi.beforeInputSamplingUS);
		rhie.renderToPresentUS = (uint32_t)(currentTimeUS - rhi.beforeRenderingUS);

		if(backEnd.renderFrame)
		{
			ID3D12CommandList* commandListArray[] = { rhi.commandList };
			rhi.mainCommandQueue->ExecuteCommandLists(ARRAY_LEN(commandListArray), commandListArray);

			if(!rhi.vsync && com_nextTargetTimeUS > currentTimeUS)
			{
				const int64_t remainingUS = com_nextTargetTimeUS - currentTimeUS;
				Sys_MicroSleep((int)remainingUS);
			}

			Present();

			static int64_t prevTS = 0;
			const int64_t currTS = Sys_Microseconds();
			const int64_t us = currTS - prevTS;
			prevTS = currTS;
			rhie.presentToPresentUS = us;
		}
		else
		{
			rhie.presentToPresentUS = 0;
		}
	}

	uint32_t GetFrameIndex()
	{
		return rhi.frameIndex;
	}

	HTexture GetSwapChainTexture()
	{
		return rhi.renderTargets[rhi.swapChainBufferIndex];
	}

	HBuffer CreateBuffer(const BufferDesc& rhiDesc)
	{
		// alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, which is effectively 64KB.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/dn903813(v=vs.85).aspx
		D3D12_RESOURCE_DESC desc = { 0 };
		desc.Alignment = 0; // D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Width = rhiDesc.byteCount;
		desc.Height = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		if(rhiDesc.initialState & ResourceStates::UnorderedAccessBit)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		bool transitionNeeded = false;
		D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
		D3D12MA::ALLOCATION_DESC allocDesc = { 0 };
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if(rhiDesc.memoryUsage == MemoryUsage::CPU || rhiDesc.memoryUsage == MemoryUsage::Upload)
		{
			allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ; // mandated
		}
		else if(rhiDesc.memoryUsage == MemoryUsage::Readback)
		{
			allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
			resourceState = D3D12_RESOURCE_STATE_COPY_DEST; // mandated
			desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}
		else
		{
			transitionNeeded = true;
		}
		if(rhiDesc.memoryUsage == MemoryUsage::GPU && rhi.umaPool != NULL)
		{
			// we only use the custom heap for buffers that are not supposed to be CPU-visible
			allocDesc.HeapType = D3D12_HEAP_TYPE_CUSTOM;
			allocDesc.CustomPool = rhi.umaPool;
		}
		allocDesc.Flags = D3D12MA::ALLOCATION_FLAG_STRATEGY_MIN_MEMORY;
		if(rhiDesc.committedResource)
		{
			allocDesc.Flags = (D3D12MA::ALLOCATION_FLAGS)(allocDesc.Flags | D3D12MA::ALLOCATION_FLAG_COMMITTED);
		}

		D3D12MA::Allocation* allocation;
		ID3D12Resource* resource;
		D3D(rhi.allocator->CreateResource(&allocDesc, &desc, resourceState, NULL, &allocation, IID_PPV_ARGS(&resource)));
		AllocateAndFixName(rhiDesc);
		SetDebugName(resource, rhiDesc.name, D3DResourceType::Buffer);

		uint32_t srvIndex = InvalidDescriptorIndex;
		if(rhiDesc.initialState & ResourceStates::ShaderAccessBits)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
			srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srv.Buffer.FirstElement = 0;
			if(rhiDesc.structureByteCount > 0)
			{
				srv.Format = DXGI_FORMAT_UNKNOWN;
				srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srv.Buffer.NumElements = rhiDesc.byteCount / rhiDesc.structureByteCount;
				srv.Buffer.StructureByteStride = rhiDesc.structureByteCount;
				srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			}
			else
			{
				srv.Format = DXGI_FORMAT_R32_TYPELESS;
				srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srv.Buffer.NumElements = rhiDesc.byteCount / 4;
				srv.Buffer.StructureByteStride = 0;
				srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			}
			srvIndex = rhi.descHeapGeneric.CreateSRV(resource, srv);
		}

		uint32_t cbvIndex = InvalidDescriptorIndex;
		if(rhiDesc.initialState & ResourceStates::ConstantBufferBit)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = { 0 };
			cbv.BufferLocation = resource->GetGPUVirtualAddress();
			cbv.SizeInBytes = rhiDesc.byteCount;
			cbvIndex = rhi.descHeapGeneric.CreateCBV(cbv);
		}

		uint32_t uavIndex = InvalidDescriptorIndex;
		if(rhiDesc.initialState & ResourceStates::UnorderedAccessBit)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = { 0 };
			uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav.Buffer.CounterOffsetInBytes = 0;
			uav.Buffer.FirstElement = 0;
			if(rhiDesc.structureByteCount > 0)
			{
				uav.Format = DXGI_FORMAT_UNKNOWN;
				uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				uav.Buffer.NumElements = rhiDesc.byteCount / rhiDesc.structureByteCount;
				uav.Buffer.StructureByteStride = rhiDesc.structureByteCount;
			}
			else
			{
				uav.Format = DXGI_FORMAT_R32_TYPELESS;
				uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
				uav.Buffer.NumElements = rhiDesc.byteCount / 4;
				uav.Buffer.StructureByteStride = 0;
			}
			uavIndex = rhi.descHeapGeneric.CreateUAV(resource, uav);
		}

		Buffer buffer = {};
		buffer.desc = rhiDesc;
		buffer.allocation = allocation;
		buffer.buffer = resource;
		buffer.gpuAddress = resource->GetGPUVirtualAddress();
		buffer.currentState = resourceState;
		buffer.shortLifeTime = rhiDesc.shortLifeTime;
		buffer.cbvIndex = cbvIndex;
		buffer.uavIndex = uavIndex;

		const HBuffer hbuffer = rhi.buffers.Add(buffer);
		if(transitionNeeded)
		{
			rhi.buffersToTransition.Add(hbuffer);
		}

		return hbuffer;
	}

	void DestroyBuffer(HBuffer handle)
	{
		Buffer& buffer = rhi.buffers.Get(handle);
		if(buffer.mapped)
		{
			UnmapBuffer(handle);
		}
		COM_RELEASE(buffer.buffer);
		COM_RELEASE(buffer.allocation);
		rhi.buffers.Remove(handle);
	}

	uint8_t* MapBuffer(HBuffer handle)
	{
		Buffer& buffer = rhi.buffers.Get(handle);
		if(buffer.mapped)
		{
			ri.Error(ERR_FATAL, "Attempted to map buffer '%s' that is already mapped!\n", buffer.desc.name);
			return NULL;
		}

		void* mappedPtr = NULL;
		D3D(buffer.buffer->Map(0, NULL, &mappedPtr));
		buffer.mapped = true;
		Q_assert(mappedPtr != NULL);

		return (uint8_t*)mappedPtr;
	}

	void UnmapBuffer(HBuffer handle)
	{
		Buffer& buffer = rhi.buffers.Get(handle);
		if(!buffer.mapped)
		{
			ri.Error(ERR_FATAL, "Attempted to unmap buffer '%s' that isn't mapped!\n", buffer.desc.name);
			return;
		}

		buffer.buffer->Unmap(0, NULL);
		buffer.mapped = false;
	}

	HTexture CreateTexture(const TextureDesc& rhiDesc)
	{
		Q_assert(rhiDesc.width > 0);
		Q_assert(rhiDesc.height > 0);
		Q_assert(rhiDesc.sampleCount > 0);
		Q_assert(rhiDesc.mipCount > 0);
		Q_assert(rhiDesc.mipCount <= MaxTextureMips);

		// Alignment 0 is the same as specifying D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
		D3D12_RESOURCE_DESC desc = { 0 };
		desc.Alignment = 0;
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		desc.Format = GetD3DFormat(rhiDesc.format);
		desc.Width = rhiDesc.width;
		desc.Height = rhiDesc.height;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = rhiDesc.mipCount;
		desc.SampleDesc.Count = rhiDesc.sampleCount;
		desc.SampleDesc.Quality = 0;
		if(rhiDesc.allowedState & ResourceStates::UnorderedAccessBit)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		if(rhiDesc.allowedState & ResourceStates::RenderTargetBit)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		if(rhiDesc.allowedState & ResourceStates::DepthAccessBits)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}
		if((rhiDesc.allowedState & ResourceStates::ShaderAccessBits) == 0)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}

		D3D12MA::ALLOCATION_DESC allocDesc = { 0 };
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		allocDesc.ExtraHeapFlags = D3D12_HEAP_FLAG_NONE;
		allocDesc.Flags = D3D12MA::ALLOCATION_FLAG_STRATEGY_MIN_MEMORY;
		if(rhiDesc.committedResource)
		{
			allocDesc.Flags = (D3D12MA::ALLOCATION_FLAGS)(allocDesc.Flags | D3D12MA::ALLOCATION_FLAG_COMMITTED);
		}

		D3D12_CLEAR_VALUE clearValue = {};
		const D3D12_CLEAR_VALUE* pClearValue = NULL;
		if(rhiDesc.usePreferredClearValue)
		{
			pClearValue = &clearValue;
			clearValue.Format = desc.Format;
			if(IsD3DDepthFormat(clearValue.Format))
			{
				clearValue.DepthStencil.Depth = rhiDesc.clearDepth;
				clearValue.DepthStencil.Stencil = rhiDesc.clearStencil;
			}
			else
			{
				memcpy(clearValue.Color, rhiDesc.clearColor, sizeof(clearValue.Color));
			}
		}

		if(rhiDesc.format == TextureFormat::Depth24_Stencil8)
		{
			desc.Format = DXGI_FORMAT_R24G8_TYPELESS; // @TODO:
		}

		// @TODO: initial state -> D3D12_RESOURCE_STATE
		D3D12MA::Allocation* allocation = NULL;
		ID3D12Resource* resource;
		if(rhiDesc.nativeResource != NULL)
		{
			resource = (ID3D12Resource*)rhiDesc.nativeResource;
		}
		else
		{
			D3D(rhi.allocator->CreateResource(&allocDesc, &desc, D3D12_RESOURCE_STATE_COPY_DEST, pClearValue, &allocation, IID_PPV_ARGS(&resource)));
		}
		AllocateAndFixName(rhiDesc);
		SetDebugName(resource, rhiDesc.name, D3DResourceType::Texture);

		uint32_t srvIndex = InvalidDescriptorIndex;
		if(rhiDesc.allowedState & ResourceStates::ShaderAccessBits)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv = { 0 };
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv.Format = desc.Format;
			srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv.Texture2D.MipLevels = desc.MipLevels;
			srv.Texture2D.MostDetailedMip = 0;
			srv.Texture2D.PlaneSlice = 0;
			srv.Texture2D.ResourceMinLODClamp = 0.0f;
			if(rhiDesc.format == TextureFormat::Depth24_Stencil8)
			{
				srv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // @TODO:
			}
			srvIndex = rhi.descHeapGeneric.CreateSRV(resource, srv);
		}

		uint32_t rtvIndex = InvalidDescriptorIndex;
		if(rhiDesc.allowedState & ResourceStates::RenderTargetBit)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtv = { 0 };
			rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtv.Format = desc.Format;
			rtv.Texture2D.MipSlice = 0;
			rtv.Texture2D.PlaneSlice = 0;
			rtvIndex = rhi.descHeapRTVs.CreateRTV(resource, rtv);
		}

		uint32_t dsvIndex = InvalidDescriptorIndex;
		if(rhiDesc.allowedState & ResourceStates::DepthWriteBit)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv = { 0 };
			dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv.Format = desc.Format;
			dsv.Flags = D3D12_DSV_FLAG_NONE; // @TODO:
			dsv.Texture2D.MipSlice = 0;
			if(rhiDesc.format == TextureFormat::Depth24_Stencil8)
			{
				dsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // @TODO:
			}
			dsvIndex = rhi.descHeapDSVs.CreateDSV(resource, dsv);
		}

		Texture texture = {};
		texture.desc = rhiDesc;
		texture.allocation = allocation;
		texture.texture = resource;
		texture.srvIndex = srvIndex;
		texture.rtvIndex = rtvIndex;
		texture.dsvIndex = dsvIndex;
		texture.currentState = D3D12_RESOURCE_STATE_COPY_DEST;
		texture.shortLifeTime = rhiDesc.shortLifeTime;
		if(rhiDesc.allowedState & ResourceStates::UnorderedAccessBit)
		{
			for(uint32_t m = 0; m < rhiDesc.mipCount; ++m)
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uav = { 0 };
				uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uav.Format = desc.Format;
				uav.Texture2D.MipSlice = m;
				uav.Texture2D.PlaneSlice = 0;
				texture.mips[m].uavIndex = rhi.descHeapGeneric.CreateUAV(resource, uav);
			}
		}
		else
		{
			for(uint32_t m = 0; m < rhiDesc.mipCount; ++m)
			{
				texture.mips[m].uavIndex = InvalidDescriptorIndex;
			}
		}

		const HTexture handle = rhi.textures.Add(texture);
		if(rhiDesc.nativeResource == NULL)
		{
			rhi.texturesToTransition.Add(handle);
		}

		return handle;
	}

	void DestroyTexture(HTexture handle)
	{
		Texture& texture = rhi.textures.Get(handle);
		if(texture.srvIndex != InvalidDescriptorIndex)
		{
			rhi.descHeapGeneric.Free(texture.srvIndex);
		}
		if(texture.rtvIndex != InvalidDescriptorIndex)
		{
			rhi.descHeapRTVs.Free(texture.rtvIndex);
		}
		if(texture.dsvIndex != InvalidDescriptorIndex)
		{
			rhi.descHeapDSVs.Free(texture.dsvIndex);
		}
		for(uint32_t m = 0; m < texture.desc.mipCount; ++m)
		{
			const uint32_t uavIndex = texture.mips[m].uavIndex;
			if(uavIndex != InvalidDescriptorIndex)
			{
				rhi.descHeapGeneric.Free(uavIndex);
			}
		}
		COM_RELEASE(texture.texture);
		COM_RELEASE(texture.allocation);
		rhi.textures.Remove(handle);
	}

	HSampler CreateSampler(const SamplerDesc& rhiDesc)
	{
		const D3D12_TEXTURE_ADDRESS_MODE addressMode = GetD3DTextureAddressMode(rhiDesc.wrapMode);
		D3D12_FILTER filter = GetD3DFilter(rhiDesc.filterMode);
		UINT maxAnisotropy = r_ext_max_anisotropy->integer;
		if(filter == D3D12_FILTER_ANISOTROPIC && maxAnisotropy <= 1)
		{
			filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			maxAnisotropy = 1;
		}
		if(filter != D3D12_FILTER_ANISOTROPIC)
		{
			maxAnisotropy = 1;
		}

		D3D12_SAMPLER_DESC desc = { 0 };
		desc.AddressU = addressMode;
		desc.AddressV = addressMode;
		desc.AddressW = addressMode;
		desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		desc.MaxAnisotropy = maxAnisotropy;
		desc.MaxLOD = 666.0f;
		desc.MinLOD = rhiDesc.minLOD;
		desc.MipLODBias = rhiDesc.mipLODBias;
		desc.Filter = filter;
		const uint32_t index = rhi.descHeapSamplers.CreateSampler(desc);

		Sampler sampler;
		sampler.desc = rhiDesc;
		sampler.shortLifeTime = rhiDesc.shortLifeTime;
		sampler.heapIndex = index;
		const HSampler handle = rhi.samplers.Add(sampler);

		return handle;
	}

	void DestroySampler(HSampler hsampler)
	{
		const Sampler& sampler = rhi.samplers.Get(hsampler);
		rhi.descHeapSamplers.Free(sampler.heapIndex);
		rhi.samplers.Remove(hsampler);
	}

	static void AddShaderVisibility(bool outVis[ShaderStage::Count], D3D12_SHADER_VISIBILITY inVis)
	{
		switch(inVis)
		{
			case D3D12_SHADER_VISIBILITY_VERTEX: outVis[ShaderStage::Vertex] = true; break;
			case D3D12_SHADER_VISIBILITY_PIXEL: outVis[ShaderStage::Pixel] = true; break;
			default: break;
		}
	}

	HRootSignature CreateRootSignature(const RootSignatureDesc& rhiDesc)
	{
		RootSignature rhiSignature = { 0 };
		rhiSignature.genericTableIndex = UINT32_MAX;
		rhiSignature.samplerTableIndex = UINT32_MAX;
		rhiSignature.genericDescCount = 0;
		rhiSignature.samplerDescCount = rhiDesc.samplerCount;

		bool shaderVis[ShaderStage::Count] = { 0 };

		//
		// root constants
		//
		int parameterCount = 0;
		D3D12_ROOT_PARAMETER parameters[16];
		for(int s = 0; s < ShaderStage::Count; ++s)
		{
			if(rhiDesc.constants[s].byteCount > 0)
			{
				rhiSignature.constants[s].parameterIndex = parameterCount;

				D3D12_ROOT_PARAMETER& p = parameters[parameterCount];
				p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
				p.Constants.Num32BitValues = AlignUp<UINT>(rhiDesc.constants[s].byteCount, 4) / 4;
				p.Constants.RegisterSpace = 0;
				p.Constants.ShaderRegister = 0;
				p.ShaderVisibility = GetD3DVisibility((ShaderStage::Id)s);
				AddShaderVisibility(shaderVis, p.ShaderVisibility);

				parameterCount++;
			}
		}
		Q_assert(parameterCount <= ShaderStage::Count);

		//
		// CBV SRV UAV table
		//
		D3D12_DESCRIPTOR_RANGE genericRanges[ARRAY_LEN(rhiDesc.genericRanges)] = {};
		for(uint32_t rangeIndex = 0; rangeIndex < rhiDesc.genericRangeCount; ++rangeIndex)
		{
			D3D12_DESCRIPTOR_RANGE& r = genericRanges[rangeIndex];
			const RootSignatureDesc::DescriptorRange& rIn = rhiDesc.genericRanges[rangeIndex];
			Q_assert(rIn.count > 0);
			r.BaseShaderRegister = 0;
			r.NumDescriptors = rIn.count;
			r.OffsetInDescriptorsFromTableStart = rIn.firstIndex;
			r.RangeType = GetD3DDescriptorRangeType(rIn.type);
			r.RegisterSpace = 0;
			if(rIn.type == DescriptorType::Buffer)
			{
				// @TODO: or bump up BaseShaderRegister, or let the user decide
				r.RegisterSpace = 1;
			}
			rhiSignature.genericDescCount += rIn.count;
		}
		if(rhiSignature.genericDescCount > 0)
		{
			rhiSignature.genericTableIndex = parameterCount;

			D3D12_ROOT_PARAMETER& p = parameters[parameterCount++];
			p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			p.DescriptorTable.NumDescriptorRanges = rhiDesc.genericRangeCount;
			p.DescriptorTable.pDescriptorRanges = genericRanges;
			p.ShaderVisibility = GetD3DVisibility(rhiDesc.genericVisibility);
			AddShaderVisibility(shaderVis, p.ShaderVisibility);
		}

		//
		// sampler table
		//
		D3D12_DESCRIPTOR_RANGE samplerRange = {};
		if(rhiDesc.samplerCount > 0)
		{
			rhiSignature.samplerTableIndex = parameterCount;

			D3D12_DESCRIPTOR_RANGE& r = samplerRange;
			r.BaseShaderRegister = 0;
			r.NumDescriptors = rhiDesc.samplerCount;
			r.OffsetInDescriptorsFromTableStart = 0;
			r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			r.RegisterSpace = 0;

			D3D12_ROOT_PARAMETER& p = parameters[parameterCount++];
			p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			p.DescriptorTable.NumDescriptorRanges = 1;
			p.DescriptorTable.pDescriptorRanges = &samplerRange;
			p.ShaderVisibility = GetD3DVisibility(rhiDesc.samplerVisibility);
			AddShaderVisibility(shaderVis, p.ShaderVisibility);
		}

		D3D12_ROOT_SIGNATURE_DESC desc = { 0 };
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
		if(!shaderVis[ShaderStage::Vertex])
		{
			desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
		}
		if(!shaderVis[ShaderStage::Pixel])
		{
			desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		}
		if(rhiDesc.usingVertexBuffers)
		{
			desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}
		desc.NumParameters = parameterCount;
		desc.pParameters = parameters;
		desc.NumStaticSamplers = 0;
		desc.pStaticSamplers = NULL;

		ID3DBlob* blob;
		ID3DBlob* errorBlob;
		if(FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errorBlob)))
		{
			ri.Error(ERR_FATAL, "Root signature creation failed!\n%s\n", (const char*)errorBlob->GetBufferPointer());
		}
		COM_RELEASE(errorBlob);

		ID3D12RootSignature* signature;
		D3D(rhi.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&signature)));
		COM_RELEASE(blob);
		AllocateAndFixName(rhiDesc);
		SetDebugName(signature, rhiDesc.name, D3DResourceType::RootSignature);

		rhiSignature.desc = rhiDesc;
		rhiSignature.signature = signature;
		rhiSignature.shortLifeTime = rhiDesc.shortLifeTime;

		return rhi.rootSignatures.Add(rhiSignature);
	}

	void DestroyRootSignature(HRootSignature signature)
	{
		COM_RELEASE(rhi.rootSignatures.Get(signature).signature);
		rhi.rootSignatures.Remove(signature);
	}

	HDescriptorTable CreateDescriptorTable(const DescriptorTableDesc& desc)
	{
		const RootSignature& sig = rhi.rootSignatures.Get(desc.rootSignature);

		const char* srvName = AllocateName(va("%s GPU-visible CBV SRV UAV", desc.name), desc.shortLifeTime);
		const char* samName = AllocateName(va("%s GPU-visible sampler", desc.name), desc.shortLifeTime);

		DescriptorTable table = { 0 };
		table.genericHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sig.genericDescCount, true, srvName);
		table.samplerHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sig.samplerDescCount, true, samName);
		table.shortLifeTime = desc.shortLifeTime;

		const Texture& nullTex = rhi.textures.Get(rhi.nullTexture);
		const Texture& nullRWTex = rhi.textures.Get(rhi.nullRWTexture);
		const Buffer& nullBuffer = rhi.buffers.Get(rhi.nullBuffer);
		const Buffer& nullRWBuffer = rhi.buffers.Get(rhi.nullRWBuffer);

		// bind null CBV SRV UAV resources
		for(uint32_t r = 0; r < sig.desc.genericRangeCount; ++r)
		{
			const RootSignatureDesc::DescriptorRange& range = sig.desc.genericRanges[r];

			uint32_t index;
			switch(range.type)
			{
				case DescriptorType::Texture: index = nullTex.srvIndex; break;
				case DescriptorType::RWTexture: index = nullRWTex.mips[0].uavIndex; break;
				case DescriptorType::Buffer: index = nullBuffer.srvIndex; break;
				case DescriptorType::RWBuffer: index = nullRWBuffer.uavIndex; break;
				default: Q_assert(!"Unsupported descriptor type"); continue;
			}

			for(uint32_t i = 0; i < range.count; ++i)
			{
				CopyDescriptor(table.genericHeap, range.firstIndex + i, rhi.descHeapGeneric, index);
			}
		}

		// bind null samplers
		for(uint32_t d = 0; d < sig.desc.samplerCount; ++d)
		{
			Handle type, index, gen;
			DecomposeHandle(&type, &index, &gen, rhi.nullSampler.v);
			CopyDescriptor(table.samplerHeap, d, rhi.descHeapSamplers, index);
		}

		return rhi.descriptorTables.Add(table);
	}

	void UpdateDescriptorTable(HDescriptorTable htable, const DescriptorTableUpdate& update)
	{
		Q_assert(update.textures != NULL);

		DescriptorTable& table = rhi.descriptorTables.Get(htable);
		
		if(update.type == DescriptorType::Texture && table.genericHeap)
		{
			for(uint32_t i = 0; i < update.resourceCount; ++i)
			{
				const Texture& texture = rhi.textures.Get(update.textures[i]);
				Q_assert(texture.srvIndex != InvalidDescriptorIndex);
				CopyDescriptor(table.genericHeap, update.firstIndex + i, rhi.descHeapGeneric, texture.srvIndex);
			}
		}
		else if(update.type == DescriptorType::RWBuffer && table.genericHeap)
		{
			for(uint32_t i = 0; i < update.resourceCount; ++i)
			{
				const Buffer& buffer = rhi.buffers.Get(update.buffers[i]);
				Q_assert(buffer.uavIndex != InvalidDescriptorIndex);
				CopyDescriptor(table.genericHeap, update.firstIndex + i, rhi.descHeapGeneric, buffer.uavIndex);
			}
		}
		else if(update.type == DescriptorType::RWTexture && table.genericHeap)
		{
			uint32_t destIndex = update.firstIndex;
			for(uint32_t i = 0; i < update.resourceCount; ++i)
			{
				const Texture& texture = rhi.textures.Get(update.textures[i]);
				uint32_t start;
				uint32_t end;
				if(update.uavMipChain)
				{
					start = 0;
					end = texture.desc.mipCount;
				}
				else
				{
					Q_assert(update.uavMipSlice < texture.desc.mipCount);
					start = update.uavMipSlice;
					end = start + 1;
				}

				for(uint32_t m = start; m < end; ++m)
				{
					Q_assert(texture.mips[m].uavIndex != InvalidDescriptorIndex);
					CopyDescriptor(table.genericHeap, destIndex++, rhi.descHeapGeneric, texture.mips[m].uavIndex);
				}
			}
		}
		else if(update.type == DescriptorType::Sampler && table.samplerHeap)
		{
			for(uint32_t i = 0; i < update.resourceCount; ++i)
			{
				Handle htype, index, gen;
				DecomposeHandle(&htype, &index, &gen, update.samplers[i].v);
				Q_assert(index != InvalidDescriptorIndex);
				CopyDescriptor(table.samplerHeap, update.firstIndex + i, rhi.descHeapSamplers, index);
			}
		}
		else
		{
			ri.Error(ERR_FATAL, "UpdateDescriptorTable: unsupported descriptor type\n");
		}
	}

	void DestroyDescriptorTable(HDescriptorTable handle)
	{
		DescriptorTable& table = rhi.descriptorTables.Get(handle);
		COM_RELEASE(table.genericHeap);
		COM_RELEASE(table.samplerHeap);

		rhi.descriptorTables.Remove(handle);
	}

	HPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& rhiDesc)
	{
		Q_assert(rhi.rootSignatures.Get(rhiDesc.rootSignature).desc.pipelineType == PipelineType::Graphics);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = { 0 };
		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE; // none available so far
		desc.pRootSignature = rhi.rootSignatures.Get(rhiDesc.rootSignature).signature;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = UINT_MAX;

		UINT semanticIndices[ShaderSemantic::Count] = { 0 };
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[MaxVertexAttributes];
		for(int a = 0; a < rhiDesc.vertexLayout.attributeCount; ++a)
		{
			const VertexAttribute& va = rhiDesc.vertexLayout.attributes[a];
			D3D12_INPUT_ELEMENT_DESC& ied = inputElementDescs[a];
			ied.SemanticName = GetD3DSemanticName(va.semantic);
			ied.SemanticIndex = semanticIndices[va.semantic]++;
			ied.Format = GetD3DFormat(va.dataType, va.vectorLength);
			ied.InputSlot = va.vertexBufferIndex;
			ied.AlignedByteOffset = va.structByteOffset;
			ied.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			ied.InstanceDataStepRate = 0;
		}
		desc.InputLayout.NumElements = rhiDesc.vertexLayout.attributeCount;
		desc.InputLayout.pInputElementDescs = inputElementDescs;

		for(int t = 0; t < rhiDesc.renderTargetCount; ++t)
		{
			const GraphicsPipelineDesc::RenderTarget& rtIn = rhiDesc.renderTargets[t];
			D3D12_RENDER_TARGET_BLEND_DESC& rtOut = desc.BlendState.RenderTarget[t];
			rtOut.BlendEnable = TRUE;
			rtOut.BlendOp = D3D12_BLEND_OP_ADD;
			rtOut.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			rtOut.LogicOpEnable = FALSE;
			rtOut.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // RGBA
			rtOut.SrcBlend = GetD3DSourceBlend(rtIn.q3BlendMode);
			rtOut.DestBlend = GetD3DDestBlend(rtIn.q3BlendMode);
			rtOut.SrcBlendAlpha = GetAlphaBlendFromColorBlend(rtOut.SrcBlend);
			rtOut.DestBlendAlpha = GetAlphaBlendFromColorBlend(rtOut.DestBlend);
			if(rtOut.SrcBlend == D3D12_BLEND_ONE && rtOut.DestBlend == D3D12_BLEND_ZERO)
			{
				rtOut.BlendEnable = FALSE;
			}
			desc.RTVFormats[t] = GetD3DFormat(rtIn.format);
		}
		desc.NumRenderTargets = rhiDesc.renderTargetCount;

		desc.DepthStencilState.DepthEnable = rhiDesc.depthStencil.enableDepthTest ? TRUE : FALSE;
		desc.DepthStencilState.DepthFunc = GetD3DComparisonFunction(rhiDesc.depthStencil.depthComparison);
		desc.DepthStencilState.DepthWriteMask = rhiDesc.depthStencil.enableDepthWrites ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthStencilState.StencilEnable = rhiDesc.depthStencil.enableStencil;
		desc.DepthStencilState.StencilReadMask = rhiDesc.depthStencil.stencilReadMask;
		desc.DepthStencilState.StencilWriteMask = rhiDesc.depthStencil.stencilWriteMask;
		desc.DepthStencilState.BackFace.StencilFunc = GetD3DComparisonFunction(rhiDesc.depthStencil.backFace.comparison);
		desc.DepthStencilState.BackFace.StencilPassOp = GetD3DStencilOp(rhiDesc.depthStencil.backFace.passOp);
		desc.DepthStencilState.BackFace.StencilFailOp = GetD3DStencilOp(rhiDesc.depthStencil.backFace.failOp);
		desc.DepthStencilState.BackFace.StencilDepthFailOp = GetD3DStencilOp(rhiDesc.depthStencil.backFace.depthFailOp);
		desc.DepthStencilState.FrontFace.StencilFunc = GetD3DComparisonFunction(rhiDesc.depthStencil.frontFace.comparison);
		desc.DepthStencilState.FrontFace.StencilPassOp = GetD3DStencilOp(rhiDesc.depthStencil.frontFace.passOp);
		desc.DepthStencilState.FrontFace.StencilFailOp = GetD3DStencilOp(rhiDesc.depthStencil.frontFace.failOp);
		desc.DepthStencilState.FrontFace.StencilDepthFailOp = GetD3DStencilOp(rhiDesc.depthStencil.frontFace.depthFailOp);
		desc.DSVFormat = GetD3DFormat(rhiDesc.depthStencil.depthStencilFormat);
		
		desc.VS.pShaderBytecode = rhiDesc.vertexShader.data;
		desc.VS.BytecodeLength = rhiDesc.vertexShader.byteCount;
		desc.PS.pShaderBytecode = rhiDesc.pixelShader.data;
		desc.PS.BytecodeLength = rhiDesc.pixelShader.byteCount;

		desc.RasterizerState.AntialiasedLineEnable = FALSE;
		desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		desc.RasterizerState.CullMode = GetD3DCullMode(rhiDesc.rasterizer.cullMode);
		desc.RasterizerState.FrontCounterClockwise = TRUE;
		desc.RasterizerState.DepthBias = rhiDesc.rasterizer.polygonOffset ? 1 : 0;
		desc.RasterizerState.DepthBiasClamp = 0.0f;
		desc.RasterizerState.SlopeScaledDepthBias = rhiDesc.rasterizer.polygonOffset ? 1.0f : 0.0f;
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.ForcedSampleCount = 0;
		desc.RasterizerState.MultisampleEnable = FALSE;
		desc.RasterizerState.DepthClipEnable = rhiDesc.rasterizer.clampDepth ? FALSE : TRUE;

		ID3D12PipelineState* pso;
		D3D(rhi.device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)));
		AllocateAndFixName(rhiDesc);
		SetDebugName(pso, rhiDesc.name, D3DResourceType::PipelineState);

		Pipeline rhiPipeline;
		rhiPipeline.type = PipelineType::Graphics;
		rhiPipeline.graphicsDesc = rhiDesc;
		rhiPipeline.pso = pso;
		rhiPipeline.shortLifeTime = rhiDesc.shortLifeTime;

		return rhi.pipelines.Add(rhiPipeline);
	}

	HPipeline CreateComputePipeline(const ComputePipelineDesc& rhiDesc)
	{
		Q_assert(rhi.rootSignatures.Get(rhiDesc.rootSignature).desc.pipelineType == PipelineType::Compute);

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = { 0 };
		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE; // none available so far
		desc.pRootSignature = rhi.rootSignatures.Get(rhiDesc.rootSignature).signature;
		desc.CS.pShaderBytecode = rhiDesc.shader.data;
		desc.CS.BytecodeLength = rhiDesc.shader.byteCount;

		ID3D12PipelineState* pso;
		D3D(rhi.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso)));
		AllocateAndFixName(rhiDesc);
		SetDebugName(pso, rhiDesc.name, D3DResourceType::PipelineState);

		Pipeline rhiPipeline;
		rhiPipeline.type = PipelineType::Compute;
		rhiPipeline.computeDesc = rhiDesc;
		rhiPipeline.pso = pso;
		rhiPipeline.shortLifeTime = rhiDesc.shortLifeTime;

		return rhi.pipelines.Add(rhiPipeline);
	}

	void DestroyPipeline(HPipeline pipeline)
	{
		COM_RELEASE(rhi.pipelines.Get(pipeline).pso);
		rhi.pipelines.Remove(pipeline);
	}

	HShader CreateShader(const ShaderDesc& desc)
	{
		IDxcBlobEncoding* blobEncoding;
		D3D(rhi.dxcUtils->CreateBlob(desc.source, desc.sourceLength, CP_ACP, &blobEncoding));

		LPCWSTR targetW = L"???";
		LPCSTR targetName = "???";
		switch(desc.stage)
		{
			case ShaderStage::Vertex: targetW = L"vs_6_0"; targetName = "vs"; break;
			case ShaderStage::Pixel: targetW = L"ps_6_0"; targetName = "ps"; break;
			case ShaderStage::Compute: targetW = L"cs_6_0"; targetName = "cs"; break;
			default: Q_assert(0); break;
		}

		wchar_t entryPointW[256];
		MultiByteToWideChar(CP_ACP, 0, desc.entryPoint, -1, entryPointW, ARRAY_LEN(entryPointW));

		struct MacroW
		{
			wchar_t macro[256];
		};
		MacroW macros[16];
		Q_assert(desc.macroCount <= ARRAY_LEN(macros));

		LPCWSTR arguments[64];
		UINT32 argumentCount = 0;
#define PushArg(Arg) arguments[argumentCount++] = Arg
		PushArg(L"E");
		PushArg(L"-E");
		PushArg(entryPointW);
		PushArg(L"-T");
		PushArg(targetW);
		PushArg(DXC_ARG_WARNINGS_ARE_ERRORS); // -WX
#if defined(D3D_DEBUG)
		PushArg(DXC_ARG_DEBUG); // -Zi embeds debug info
		PushArg(DXC_ARG_SKIP_OPTIMIZATIONS); // -Od disables optimizations
		PushArg(DXC_ARG_ENABLE_STRICTNESS); // -Ges enables strict mode
		PushArg(DXC_ARG_IEEE_STRICTNESS); // -Gis forces IEEE strictness
		PushArg(L"-Qembed_debug"); // -Qembed_debug embeds debug info in shader container
#else
		PushArg(L"-Qstrip_debug");
		PushArg(L"-Qstrip_reflect");
		PushArg(DXC_ARG_OPTIMIZATION_LEVEL3); // -O3
#endif
		PushArg(L"-D");
		PushArg(desc.stage == ShaderStage::Vertex ? L"VERTEX_SHADER=1" : L"VERTEX_SHADER=0");
		PushArg(L"-D");
		PushArg(desc.stage == ShaderStage::Pixel ? L"PIXEL_SHADER=1" : L"PIXEL_SHADER=0");
		PushArg(L"-D");
		PushArg(desc.stage == ShaderStage::Compute ? L"COMPUTE_SHADER=1" : L"COMPUTE_SHADER=0");
		for(uint32_t m = 0; m < desc.macroCount; ++m)
		{
			const char* input = va("%s=%s", desc.macros[m].name, desc.macros[m].value);
			MacroW& output = macros[m];
			MultiByteToWideChar(CP_ACP, 0, input, -1, output.macro, ARRAY_LEN(output.macro));
			PushArg(L"-D");
			PushArg(output.macro);
		}
#undef PushArg
		Q_assert(argumentCount <= ARRAY_LEN(arguments));
		
		DxcBuffer sourceBuffer = {};
		sourceBuffer.Ptr = blobEncoding->GetBufferPointer();
		sourceBuffer.Size = blobEncoding->GetBufferSize();
		sourceBuffer.Encoding = 0;

		IDxcResult* result = NULL;
		HRESULT hr = S_OK;
		if(FAILED(rhi.dxcCompiler->Compile(&sourceBuffer, arguments, argumentCount, NULL, IID_PPV_ARGS(&result))) ||
			FAILED(result->GetStatus(&hr)) ||
			FAILED(hr))
		{
			IDxcBlobUtf8* errors;
			if(result != NULL && SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), NULL)) &&
				errors->GetStringLength() > 0)
			{
				ri.Error(ERR_FATAL, "Shader (%s) compilation failed:\n%s\n", targetName, (const char*)errors->GetBufferPointer());
			}
			else
			{
				ri.Error(ERR_FATAL, "Shader (%s) compilation failed:\n", targetName);
			}
			return RHI_MAKE_NULL_HANDLE();
		}

		IDxcBlob* shaderBlob;
		D3D(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), NULL));
		blobEncoding->Release();
		result->Release();

		Shader shader;
		shader.blob = shaderBlob;

		return rhi.shaders.Add(shader);
	}

	ShaderByteCode GetShaderByteCode(HShader shader)
	{
		IDxcBlob* const blob = rhi.shaders.Get(shader).blob;

		ShaderByteCode byteCode;
		byteCode.data = blob->GetBufferPointer();
		byteCode.byteCount = blob->GetBufferSize();

		return byteCode;
	}

	void DestroyShader(HShader shader)
	{
		COM_RELEASE(rhi.shaders.Get(shader).blob);
		rhi.shaders.Remove(shader);
	}

	void CmdBindRenderTargets(uint32_t colorCount, const HTexture* colorTargets, const HTexture* depthStencilTarget)
	{
		Q_assert(CanWriteCommands());
		Q_assert(colorCount > 0 || colorTargets == NULL);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[MaxRenderTargets] = {};
		for(uint32_t t = 0; t < colorCount; ++t)
		{
			const uint32_t rtvIndex = rhi.textures.Get(colorTargets[t]).rtvIndex;
			rtvHandles[t] = rhi.descHeapRTVs.GetCPUHandle(rtvIndex);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandlePtr = NULL;
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		if(depthStencilTarget != NULL)
		{
			const Texture& depthStencil = rhi.textures.Get(*depthStencilTarget);
			dsvHandle = rhi.descHeapDSVs.GetCPUHandle(depthStencil.dsvIndex);
			dsvHandlePtr = &dsvHandle;
		}

		rhi.commandList->OMSetRenderTargets(colorCount, rtvHandles, FALSE, dsvHandlePtr);
	}

	void CmdBindRootSignature(HRootSignature rootSignature)
	{
		Q_assert(CanWriteCommands());

		const RootSignature& sig = rhi.rootSignatures.Get(rootSignature);
		if(sig.desc.pipelineType == PipelineType::Graphics && rootSignature != rhi.currentRootSignature)
		{
			rhi.currentRootSignature = rootSignature;
			rhi.commandList->SetGraphicsRootSignature(sig.signature);
		}
		else if(sig.desc.pipelineType == PipelineType::Compute)
		{
			rhi.commandList->SetComputeRootSignature(sig.signature);
		}
	}

	void CmdBindDescriptorTable(HRootSignature sigHandle, HDescriptorTable handle)
	{
		Q_assert(CanWriteCommands());

		const DescriptorTable& table = rhi.descriptorTables.Get(handle);
		const RootSignature& sig = rhi.rootSignatures.Get(sigHandle);

		UINT heapCount = 0;
		ID3D12DescriptorHeap* heaps[2];
		if(sig.genericTableIndex != UINT32_MAX)
		{
			heaps[heapCount++] = table.genericHeap;
		}
		if(sig.samplerTableIndex != UINT32_MAX)
		{
			heaps[heapCount++] = table.samplerHeap;
		}
		rhi.commandList->SetDescriptorHeaps(heapCount, heaps);

		if(sig.genericTableIndex != UINT32_MAX)
		{
			if(sig.desc.pipelineType == PipelineType::Graphics)
			{
				rhi.commandList->SetGraphicsRootDescriptorTable(sig.genericTableIndex, table.genericHeap->GetGPUDescriptorHandleForHeapStart());
			}
			else if(sig.desc.pipelineType == PipelineType::Compute)
			{
				rhi.commandList->SetComputeRootDescriptorTable(sig.genericTableIndex, table.genericHeap->GetGPUDescriptorHandleForHeapStart());
			}
		}
		if(sig.samplerTableIndex != UINT32_MAX)
		{
			if(sig.desc.pipelineType == PipelineType::Graphics)
			{
				rhi.commandList->SetGraphicsRootDescriptorTable(sig.samplerTableIndex, table.samplerHeap->GetGPUDescriptorHandleForHeapStart());
			}
			else if(sig.desc.pipelineType == PipelineType::Compute)
			{
				rhi.commandList->SetComputeRootDescriptorTable(sig.samplerTableIndex, table.samplerHeap->GetGPUDescriptorHandleForHeapStart());
			}
		}
	}

	void CmdBindPipeline(HPipeline pipeline)
	{
		Q_assert(CanWriteCommands());

		const Pipeline& pipe = rhi.pipelines.Get(pipeline);
		rhi.commandList->SetPipelineState(pipe.pso);
	}

	void CmdBindVertexBuffers(uint32_t count, const HBuffer* vertexBuffers, const uint32_t* byteStrides, const uint32_t* startByteOffsets)
	{
		Q_assert(CanWriteCommands());
		Q_assert(count <= MaxVertexBuffers);

		count = min(count, MaxVertexBuffers);

		D3D12_VERTEX_BUFFER_VIEW views[MaxVertexBuffers];
		for(uint32_t v = 0; v < count; ++v)
		{
			const Buffer& buffer = rhi.buffers.Get(vertexBuffers[v]);
			const uint32_t offset = startByteOffsets ? startByteOffsets[v] : 0;
			views[v].BufferLocation = buffer.gpuAddress + offset;
			views[v].SizeInBytes = buffer.desc.byteCount - offset;
			views[v].StrideInBytes = byteStrides[v];
		}
		rhi.commandList->IASetVertexBuffers(0, count, views);
	}

	void CmdBindIndexBuffer(HBuffer indexBuffer, IndexType::Id type, uint32_t startByteOffset)
	{
		Q_assert(CanWriteCommands());

		const Buffer& buffer = rhi.buffers.Get(indexBuffer);

		D3D12_INDEX_BUFFER_VIEW view = { 0 };
		view.BufferLocation = buffer.gpuAddress + startByteOffset;
		view.Format = GetD3DIndexFormat(type);
		view.SizeInBytes = (UINT)(buffer.desc.byteCount - startByteOffset);
		rhi.commandList->IASetIndexBuffer(&view);
	}

	void CmdSetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h, float minDepth, float maxDepth)
	{
		Q_assert(CanWriteCommands());

		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = x;
		viewport.TopLeftY = y;
		viewport.Width = w;
		viewport.Height = h;
		viewport.MinDepth = minDepth;
		viewport.MaxDepth = maxDepth;
		rhi.commandList->RSSetViewports(1, &viewport);
	}

	void CmdSetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
	{
		Q_assert(CanWriteCommands());

		D3D12_RECT rect;
		rect.left = x;
		rect.top = y;
		rect.right = x + w;
		rect.bottom = y + h;
		rhi.commandList->RSSetScissorRects(1, &rect);
	}

	void CmdSetRootConstants(HRootSignature rootSignature, ShaderStage::Id shaderType, const void* constants)
	{
		Q_assert(CanWriteCommands());
		Q_assert(constants);

		const RootSignature& sig = rhi.rootSignatures.Get(rootSignature);
		const UINT parameterIndex = sig.constants[shaderType].parameterIndex;
		const UINT constantCount = sig.desc.constants[shaderType].byteCount / 4;

		CmdBindRootSignature(rootSignature);

		if(sig.desc.pipelineType == PipelineType::Graphics)
		{
			rhi.commandList->SetGraphicsRoot32BitConstants(parameterIndex, constantCount, constants, 0);
		}
		else if(sig.desc.pipelineType == PipelineType::Compute)
		{
			rhi.commandList->SetComputeRoot32BitConstants(parameterIndex, constantCount, constants, 0);
		}
	}

	void CmdDraw(uint32_t vertexCount, uint32_t firstVertex)
	{
		Q_assert(CanWriteCommands());

		rhi.commandList->DrawInstanced(vertexCount, 1, firstVertex, 0);
	}

	void CmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
	{
		Q_assert(CanWriteCommands());

		rhi.commandList->DrawIndexedInstanced(indexCount, 1, firstIndex, firstVertex, 0);
	}

	void CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		Q_assert(CanWriteCommands());

		rhi.commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
	}

	uint32_t CmdBeginDurationQuery()
	{
		Q_assert(CanWriteCommands());

		FrameQueries& fq = rhi.frameQueries[rhi.frameIndex];
		Q_assert(fq.durationQueryCount < MaxDurationQueries);
		if(fq.durationQueryCount >= MaxDurationQueries)
		{
			return UINT32_MAX;
		}

		const uint32_t durationIndex = fq.durationQueryCount;
		const UINT timeStampBeginIndex = durationIndex * 2;
		rhi.commandList->EndQuery(rhi.timeStampHeaps[rhi.frameIndex], D3D12_QUERY_TYPE_TIMESTAMP, timeStampBeginIndex);

		DurationQuery& query = fq.durationQueries[durationIndex];
		if(backEnd.renderFrame)
		{
			Q_assert(query.state == QueryState::Free);
		}
		query.state = QueryState::Begun;

		fq.durationQueryCount++;

		return durationIndex;
	}

	void CmdEndDurationQuery(uint32_t durationIndex)
	{
		Q_assert(CanWriteCommands());

		FrameQueries& fq = rhi.frameQueries[rhi.frameIndex];
		Q_assert(durationIndex < fq.durationQueryCount);
		if(durationIndex >= fq.durationQueryCount)
		{
			return;
		}

		DurationQuery& query = fq.durationQueries[durationIndex];
		Q_assert(query.state == QueryState::Begun);
		const UINT timeStampEndIndex = durationIndex * 2 + 1;
		rhi.commandList->EndQuery(rhi.timeStampHeaps[rhi.frameIndex], D3D12_QUERY_TYPE_TIMESTAMP, timeStampEndIndex);
		query.state = QueryState::Ended;
	}

	void CmdBarrier(uint32_t texCount, const TextureBarrier* textures, uint32_t buffCount, const BufferBarrier* buffers)
	{
		Q_assert(CanWriteCommands());

		static D3D12_RESOURCE_BARRIER barriers[MAX_DRAWIMAGES * 2];
		Q_assert(buffCount + texCount <= ARRAY_LEN(barriers));

		UINT barrierCount = 0;
		for(uint32_t i = 0; i < texCount; ++i)
		{
			Texture& texture = rhi.textures.Get(textures[i].texture);
			if(SetBarrier(texture.currentState, barriers[barrierCount], textures[i].newState, texture.texture))
			{
				barrierCount++;
			}
			
		}
		for(uint32_t i = 0; i < buffCount; ++i)
		{
			Buffer& buffer = rhi.buffers.Get(buffers[i].buffer);
			if(SetBarrier(buffer.currentState, barriers[barrierCount], buffers[i].newState, buffer.buffer))
			{
				barrierCount++;
			}
		}

		if(barrierCount > 0)
		{
			rhi.commandList->ResourceBarrier(barrierCount, barriers);
		}
	}

	void CmdClearColorTarget(HTexture texture, const vec4_t clearColor, const Rect* rect)
	{
		Q_assert(CanWriteCommands());

		D3D12_RECT* d3dRectPtr = NULL;
		D3D12_RECT d3dRect = {};
		UINT rectCount = 0;
		if(rect != NULL)
		{
			rectCount = 1;
			d3dRect.left = rect->x;
			d3dRect.top = rect->y;
			d3dRect.right = rect->x + rect->w;
			d3dRect.bottom = rect->y + rect->h;
			d3dRectPtr = &d3dRect;
		}

		const Texture& renderTarget = rhi.textures.Get(texture);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rhi.descHeapRTVs.GetCPUHandle(renderTarget.rtvIndex);
		rhi.commandList->ClearRenderTargetView(rtvHandle, clearColor, rectCount, d3dRectPtr);
	}

	void CmdClearDepthStencilTarget(HTexture texture, bool clearDepth, float depth, bool clearStencil, uint8_t stencil, const Rect* rect)
	{
		Q_assert(CanWriteCommands());
		Q_assert(clearDepth || clearStencil);
		if(!clearDepth && !clearStencil)
		{
			return;
		}

		D3D12_RECT* d3dRectPtr = NULL;
		D3D12_RECT d3dRect = {};
		UINT rectCount = 0;
		if(rect != NULL)
		{
			rectCount = 1;
			d3dRect.left = rect->x;
			d3dRect.top = rect->y;
			d3dRect.right = rect->x + rect->w;
			d3dRect.bottom = rect->y + rect->h;
			d3dRectPtr = &d3dRect;
		}

		D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
		if(clearDepth)
		{
			flags |= D3D12_CLEAR_FLAG_DEPTH;
		}
		if(clearStencil)
		{
			flags |= D3D12_CLEAR_FLAG_STENCIL;
		}

		const Texture& depthStencil = rhi.textures.Get(texture);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = rhi.descHeapDSVs.GetCPUHandle(depthStencil.dsvIndex);
		rhi.commandList->ClearDepthStencilView(dsvHandle, flags, depth, stencil, rectCount, d3dRectPtr);
	}

	void CmdInsertDebugLabel(const char* name, float r, float g, float b)
	{
		Q_assert(CanWriteCommands());
		Q_assert(name);

		if(rhi.pix.SetMarkerOnCommandList != NULL)
		{
			rhi.pix.SetMarkerOnCommandList(rhi.commandList, BGRAUIntFromFloat(r, g, b), name);
		}
		else
		{
			rhi.commandList->SetMarker(1, name, strlen(name) + 1);
		}
	}

	void CmdBeginDebugLabel(const char* name, float r, float g, float b)
	{
		Q_assert(CanWriteCommands());
		Q_assert(name);

		if(rhi.pix.canBeginAndEnd)
		{
			rhi.pix.BeginEventOnCommandList(rhi.commandList, BGRAUIntFromFloat(r, g, b), name);
		}
		else
		{
			rhi.commandList->BeginEvent(1, name, strlen(name) + 1);
		}
	}

	void CmdEndDebugLabel()
	{
		Q_assert(CanWriteCommands());

		if(rhi.pix.canBeginAndEnd)
		{
			rhi.pix.EndEventOnCommandList(rhi.commandList);
		}
		else
		{
			rhi.commandList->EndEvent();
		}
	}

	void CmdSetStencilReference(uint8_t stencilRef)
	{
		rhi.commandList->OMSetStencilRef((UINT)stencilRef);
	}

	void CmdCopyBuffer(HBuffer dest, HBuffer source)
	{
		Q_assert(CanWriteCommands());

		const Buffer& dst = rhi.buffers.Get(dest);
		const Buffer& src = rhi.buffers.Get(source);
		const UINT64 byteCount = min(src.desc.byteCount, dst.desc.byteCount);
		rhi.commandList->CopyBufferRegion(dst.buffer, 0, src.buffer, 0, byteCount);
	}

	void CmdSetShadingRate(ShadingRate::Id shadingRate)
	{
		Q_assert(CanWriteCommands());

		if(!rhi.baseVRSSupport)
		{
			return;
		}

		if(!rhi.extendedVRSSupport)
		{
			switch(shadingRate)
			{
				case ShadingRate::SR_2x4:
				case ShadingRate::SR_4x2:
				case ShadingRate::SR_4x4:
					shadingRate = ShadingRate::SR_2x2;
					break;
				default:
					break;
			}
		}
#if !defined( QC )
		rhi.commandList->RSSetShadingRate(GetD3DShadingRate(shadingRate), NULL);
#endif
	}

	uint32_t GetDurationCount()
	{
		return rhi.resolvedQueries.durationQueryCount;
	}

	void GetDurations(uint32_t* gpuMicroSeconds)
	{
		memcpy(gpuMicroSeconds, rhi.resolvedQueries.gpuMicroSeconds, rhi.resolvedQueries.durationQueryCount * sizeof(uint32_t));
	}

	uint8_t* BeginBufferUpload(HBuffer buffer)
	{
		return rhi.upload.BeginBufferUpload(buffer);
	}

	void EndBufferUpload(HBuffer buffer)
	{
		rhi.upload.EndBufferUpload(buffer);
	}

	void BeginTextureUpload(MappedTexture& mappedTexture, HTexture texture)
	{
		rhi.upload.BeginTextureUpload(mappedTexture, texture);
	}

	void EndTextureUpload()
	{
		rhi.upload.EndTextureUpload();
	}

	void BeginTempCommandList()
	{
		Q_assert(!rhi.frameBegun);
		Q_assert(rhi.commandList == rhi.mainCommandList);
		rhi.commandList = rhi.tempCommandList;

		// CPU wait for the temp command list to be done executing on the GPU
		WaitForTempCommandList();

		// GPU wait for the copy queue to be done executing on the GPU
		rhi.upload.WaitToStartDrawing(rhi.computeCommandQueue);
	}

	void EndTempCommandList()
	{
		Q_assert(!rhi.frameBegun);
		Q_assert(rhi.commandList == rhi.tempCommandList);
		rhi.commandList = rhi.mainCommandList;

		// execute and wait on the temporary command list
		ID3D12CommandQueue* const queue = rhi.computeCommandQueue;
		rhi.tempCommandList->Close();
		ID3D12CommandList* tempCommandListArray[] = { rhi.tempCommandList };
		queue->ExecuteCommandLists(ARRAY_LEN(tempCommandListArray), tempCommandListArray);
		rhi.tempFenceValue++;
		rhi.tempFence.Signal(queue, rhi.tempFenceValue);
		rhi.tempCommandListOpen = false;
	}

	void BeginTextureReadback(MappedTexture& mappedTexture, HTexture htexture)
	{
		rhi.readback.BeginTextureReadback(mappedTexture, htexture);
	}

	void EndTextureReadback()
	{
		rhi.readback.EndTextureReadback();
	}

	void WaitUntilDeviceIsIdle()
	{
		// direct queue
		rhi.mainFenceValues[rhi.frameIndex]++;
#if RHI_DEBUG_FENCE
		Sys_DebugPrintf("Signal: %d (WaitUntilDeviceIsIdle)\n", (int)rhi.mainFenceValues[rhi.frameIndex]);
		Sys_DebugPrintf("Wait: %d (WaitUntilDeviceIsIdle)\n", (int)rhi.mainFenceValues[rhi.frameIndex]);
#endif
		rhi.mainFence.Signal(rhi.mainCommandQueue, rhi.mainFenceValues[rhi.frameIndex]);
		rhi.mainFence.WaitOnCPU(rhi.mainFenceValues[rhi.frameIndex]);

		// compute queue
		rhi.tempFence.WaitOnCPU(rhi.tempFenceValue);

		// upload queue
		rhi.upload.fence.WaitOnCPU(rhi.upload.fenceValue);
	}
}

void R_WaitBeforeInputSampling()
{
	RHI::WaitForSwapChain();
	RHI::rhi.beforeInputSamplingUS = Sys_Microseconds();
}
