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
// Rendering Hardware Interface - private interface


#pragma once


#include "tr_local.h"


namespace RHI
{
	// FrameCount has 2 meanings:
	// 1. maximum number of frames queued
	// 2. number of frames in the back buffer
	const uint32_t FrameCount = 2;
	const uint32_t MaxVertexBuffers = 16;
	const uint32_t MaxVertexAttributes = 32;
	const uint32_t MaxRenderTargets = 8;
	const uint32_t MaxDurationQueries = 64;
	const uint32_t MaxTextureMips = 16;
	const uint32_t InvalidDescriptorIndex = UINT16_MAX;
	const uint32_t MaxCPUGenericDescriptors = 16384; // real max: unlimited
	const uint32_t MaxCPUSamplerDescriptors = 128; // real max: 2048
	const uint32_t MaxCPURTVDescriptors = 64;
	const uint32_t MaxCPUDSVDescriptors = 64;
	const uint32_t MaxCPUDescriptors =
		MaxCPUGenericDescriptors +
		MaxCPUSamplerDescriptors +
		MaxCPURTVDescriptors +
		MaxCPUDSVDescriptors;

#define RHI_ENUM_OPERATORS(EnumType) \
	inline EnumType operator|(EnumType a, EnumType b) { return (EnumType)((uint32_t)(a) | (uint32_t)(b)); } \
	inline EnumType operator&(EnumType a, EnumType b) { return (EnumType)((uint32_t)(a) & (uint32_t)(b)); } \
	inline EnumType operator|=(EnumType& a, EnumType b) { return a = (a | b); } \
	inline EnumType operator&=(EnumType& a, EnumType b) { return a = (a & b); } \
	inline EnumType operator~(EnumType a) { return (EnumType)(~(uint32_t)(a)); }

#define RHI_BIT(Bit) (1 << Bit)
#define RHI_BIT_MASK(BitCount) ((1 << BitCount) - 1)

#define RHI_GET_HANDLE_VALUE(Handle) (Handle.v)
#define RHI_MAKE_HANDLE(Value) { Value }
#define RHI_MAKE_NULL_HANDLE() { 0 }

	struct IndexType
	{
		enum Id
		{
			UInt32,
			UInt16,
			Count
		};
	};

	struct ResourceStates
	{
		enum Flags
		{
			Common = 0,
			VertexBufferBit = RHI_BIT(0),
			IndexBufferBit = RHI_BIT(1),
			ConstantBufferBit = RHI_BIT(2),
			RenderTargetBit = RHI_BIT(3),
			VertexShaderAccessBit = RHI_BIT(4),
			PixelShaderAccessBit = RHI_BIT(5),
			ComputeShaderAccessBit = RHI_BIT(6),
			CopySourceBit = RHI_BIT(7),
			CopyDestinationBit = RHI_BIT(8),
			DepthReadBit = RHI_BIT(9),
			DepthWriteBit = RHI_BIT(10),
			UnorderedAccessBit = RHI_BIT(11),
			PresentBit = RHI_BIT(12),
			ShaderAccessBits = VertexShaderAccessBit | PixelShaderAccessBit | ComputeShaderAccessBit,
			DepthAccessBits = DepthReadBit | DepthWriteBit
		};
	};
	RHI_ENUM_OPERATORS(ResourceStates::Flags);

	struct MemoryUsage
	{
		enum Id
		{
			CPU, // Host
			GPU, // DeviceLocal
			Upload, // CPU -> GPU
			Readback, // GPU -> CPU
			Count
		};
	};

	struct ShaderStage
	{
		enum Id
		{
			Vertex,
			Pixel,
			Compute,
			Count
		};
	};

	struct ShaderStages
	{
		enum Flags
		{
			None = 0,
			VertexBit = 1 << ShaderStage::Vertex,
			PixelBit = 1 << ShaderStage::Pixel,
			ComputeBit = 1 << ShaderStage::Compute,
			AllGraphicsBits = VertexBit | PixelBit
		};
	};
	RHI_ENUM_OPERATORS(ShaderStages::Flags);

	struct DataType
	{
		enum Id
		{
			Float32,
			UNorm8,
			UInt32,
			Count
		};
	};

	struct ShaderSemantic
	{
		enum Id
		{
			Position,
			Normal,
			TexCoord,
			Color,
			Count
		};
	};

	struct TextureFormat
	{
		enum Id
		{
			Invalid,
			RGBA32_UNorm,
			RGBA64_UNorm,
			RGBA64_Float,
			Depth32_Float,
			RG16_UNorm,
			R8_UNorm,
			Depth24_Stencil8,
			R10G10B10A2_UNorm,
			Count
		};
	};

	struct ComparisonFunction
	{
		enum Id
		{
			Never,
			Less,
			Equal,
			LessEqual,
			Greater,
			NotEqual,
			GreaterEqual,
			Always,
			Count
		};
	};

	struct DescriptorType
	{
		enum Id
		{
			Buffer, // CBV, HBuffer
			RWBuffer, // UAV, HBuffer
			Texture, // SRV, HTexture
			RWTexture, // UAV, HTexture
			Sampler,
			Count
		};
	};

	struct TextureFilter
	{
		enum Id
		{
			Point,
			Linear,
			Anisotropic,
			Count
		};
	};

	struct PipelineType
	{
		enum Id
		{
			Graphics,
			Compute,
			Count
		};
	};

	struct StencilOp
	{
		enum Id
		{
			Keep,
			Zero,
			Replace,
			SaturatedIncrement,
			SaturatedDecrement,
			Invert,
			WrappedIncrement,
			WrappedDecrement,
			Count
		};
	};

	struct ShadingRate
	{
		enum Id
		{
			// Guaranteed modes:
			SR_1x1,
			SR_2x1,
			SR_1x2,
			SR_2x2,
			// Additional modes:
			SR_4x2,
			SR_2x4,
			SR_4x4,
			Count
		};
	};

	struct RootSignatureDesc
	{
		RootSignatureDesc() = default;
		RootSignatureDesc(const char* name_)
		{
			name = name_;
		}

		const char* name = NULL;
		bool shortLifeTime = false;
		bool usingVertexBuffers = false;
		struct PerStageConstants
		{
			uint32_t byteCount = 0;
		}
		constants[ShaderStage::Count];
		struct DescriptorRange
		{
			DescriptorType::Id type = DescriptorType::Count;
			uint32_t firstIndex = 0;
			uint32_t count = 0;
		}
		genericRanges[64];
		uint32_t genericRangeCount = 0;
		uint32_t samplerCount = 0;
		ShaderStages::Flags genericVisibility = ShaderStages::None; // ignored by compute pipelines
		ShaderStages::Flags samplerVisibility = ShaderStages::None; // ignored by compute pipelines
		PipelineType::Id pipelineType = PipelineType::Graphics;

		void AddRange(DescriptorType::Id type, uint32_t firstIndex, uint32_t count)
		{
			Q_assert(genericRangeCount < ARRAY_LEN(genericRanges));
			DescriptorRange& r = genericRanges[genericRangeCount++];
			r.type = type;
			r.firstIndex = firstIndex;
			r.count = count;
		}
	};

	struct ShaderByteCode
	{
		ShaderByteCode() = default;
		ShaderByteCode(const void* data_, uint32_t byteCount_)
		{
			data = data_;
			byteCount = byteCount_;
		}

		template<uint32_t N>
		ShaderByteCode(const uint8_t (&byteCode)[N])
		{
			data = byteCode;
			byteCount = N;
		}

		const void* data = NULL;
		uint32_t byteCount = 0;
	};

	struct ShaderMacro
	{
		ShaderMacro() = default;
		ShaderMacro(const char* name_, const char* value_)
		{
			name = name_;
			value = value_;
		}

		const char* name = NULL;
		const char* value = NULL;
	};

	struct VertexAttribute
	{
		uint32_t vertexBufferIndex = 0; // also called "binding" or "input slot"
		ShaderSemantic::Id semantic = ShaderSemantic::Count; // intended usage
		DataType::Id dataType = DataType::Count; // for a single component of the vector
		uint32_t vectorLength = 4; // number of components per vector
		uint32_t structByteOffset = 0; // where in the struct to look when using interleaved data
	};

	struct DepthStencilOpDesc
	{
		ComparisonFunction::Id comparison = ComparisonFunction::Always;
		StencilOp::Id passOp = StencilOp::Keep;
		StencilOp::Id failOp = StencilOp::Keep;
		StencilOp::Id depthFailOp = StencilOp::Keep;
	};

	struct GraphicsPipelineDesc
	{
		GraphicsPipelineDesc() = default;
		GraphicsPipelineDesc(const char* name_, HRootSignature rootSignature_)
		{
			name = name_;
			rootSignature = rootSignature_;
		}

		const char* name = NULL;
		bool shortLifeTime = false;
		HRootSignature rootSignature = RHI_MAKE_NULL_HANDLE();
		ShaderByteCode vertexShader;
		ShaderByteCode pixelShader;
		struct VertexLayout
		{
			VertexAttribute attributes[MaxVertexAttributes];
			uint32_t attributeCount = 0;
			uint32_t bindingStrides[MaxVertexBuffers] = { 0 }; // total byte size of a vertex for each buffer

			void AddAttribute(
				uint32_t vertexBufferIndex,
				ShaderSemantic::Id semantic,
				DataType::Id dataType,
				uint32_t vectorLength,
				uint32_t structByteOffset)
			{
				Q_assert(attributeCount < MaxVertexAttributes);
				VertexAttribute& va = attributes[attributeCount++];
				va.dataType = dataType;
				va.semantic = semantic;
				va.structByteOffset = structByteOffset;
				va.vectorLength = vectorLength;
				va.vertexBufferIndex = vertexBufferIndex;
			}
		}
		vertexLayout;
		struct DepthStencil
		{
			void DisableDepth()
			{
				enableDepthTest = false;
				enableDepthWrites = false;
			}

			bool enableDepthTest = true;
			bool enableDepthWrites = true;
			ComparisonFunction::Id depthComparison = ComparisonFunction::GreaterEqual;
			TextureFormat::Id depthStencilFormat = TextureFormat::Depth32_Float;

			bool enableStencil = false;
			uint8_t stencilReadMask = 0xFF;
			uint8_t stencilWriteMask = 0xFF;
			DepthStencilOpDesc frontFace;
			DepthStencilOpDesc backFace;
		}
		depthStencil;
		struct Rasterizer
		{
			cullType_t cullMode = CT_FRONT_SIDED;
			bool polygonOffset = false;
			bool clampDepth = false;
		}
		rasterizer;
		struct RenderTarget
		{
			uint32_t q3BlendMode = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			TextureFormat::Id format = TextureFormat::RGBA32_UNorm;
		}
		renderTargets[MaxRenderTargets];
		uint32_t renderTargetCount = 0;

		void AddRenderTarget(uint32_t q3BlendMode, TextureFormat::Id format)
		{
			Q_assert(renderTargetCount < MaxRenderTargets);
			RenderTarget& rt = renderTargets[renderTargetCount++];
			rt.q3BlendMode = q3BlendMode;
			rt.format = format;
		}
	};

	struct ComputePipelineDesc
	{
		ComputePipelineDesc() = default;
		ComputePipelineDesc(const char* name_, HRootSignature rootSignature_)
		{
			name = name_;
			rootSignature = rootSignature_;
		}

		const char* name = NULL;
		bool shortLifeTime = false;
		HRootSignature rootSignature = RHI_MAKE_NULL_HANDLE();
		ShaderByteCode shader;
	};

	struct BufferDesc
	{
		BufferDesc() = default;
		BufferDesc(const char* name_, uint32_t byteCount_, ResourceStates::Flags initialState_)
		{
			name = name_;
			byteCount = byteCount_;
			initialState = initialState_;
			memoryUsage = MemoryUsage::GPU;
			committedResource = false;
		}

		const char* name = NULL;
		bool shortLifeTime = false;
		uint32_t byteCount = 0;
		ResourceStates::Flags initialState = ResourceStates::Common;
		MemoryUsage::Id memoryUsage = MemoryUsage::GPU;
		bool committedResource = false;
		uint32_t structureByteCount = 0; // > 0 means structured buffer, == 0 means byte address buffer
	};

	struct TextureDesc
	{
		TextureDesc() = default;
		TextureDesc(const char* name_, uint32_t width_, uint32_t height_, uint32_t mipCount_ = 1)
		{
			name = name_;
			width = width_;
			height = height_;
			mipCount = mipCount_;
			sampleCount = 1;
			initialState = ResourceStates::PixelShaderAccessBit;
			allowedState = ResourceStates::PixelShaderAccessBit;
			format = TextureFormat::RGBA32_UNorm;
			committedResource = false;
			usePreferredClearValue = false;
		}

		const char* name = NULL;
		bool shortLifeTime = false;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t mipCount = 1;
		uint32_t sampleCount = 1;
		ResourceStates::Flags initialState = ResourceStates::PixelShaderAccessBit;
		ResourceStates::Flags allowedState = ResourceStates::PixelShaderAccessBit;
		TextureFormat::Id format = TextureFormat::RGBA32_UNorm;
		bool committedResource = false;
		bool usePreferredClearValue = false; // for render targets and depth/stencil buffers
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		float clearDepth = 1.0f;
		byte clearStencil = 0;
		void* nativeResource = NULL; // ID3D12Resource*, VkImage, etc.

		void SetClearColor(const vec4_t rgba)
		{
			usePreferredClearValue = true;
			Vector4Copy(rgba, clearColor);
		}

		void SetClearDepthStencil(float depth, byte stencil = 0)
		{
			usePreferredClearValue = true;
			clearDepth = depth;
			clearStencil = stencil;
		}
	};

	struct SamplerDesc
	{
		SamplerDesc() = default;
		SamplerDesc(textureWrap_t wrapMode_, TextureFilter::Id filterMode_, float minLOD_ = 0.0f, float mipLODBias_ = 0.0f)
		{
			wrapMode = wrapMode_;
			filterMode = filterMode_;
			minLOD = minLOD_;
			mipLODBias = mipLODBias_;
		}

		textureWrap_t wrapMode = TW_REPEAT;
		TextureFilter::Id filterMode = TextureFilter::Linear;
		float minLOD = 0.0f;
		float mipLODBias = 0.0f;
		bool shortLifeTime = false;
	};

	struct DescriptorTableDesc
	{
		DescriptorTableDesc() = default;
		DescriptorTableDesc(const char* name_, HRootSignature rootSignature_)
		{
			name = name_;
			rootSignature = rootSignature_;
		}

		const char* name = NULL;
		bool shortLifeTime = false;
		HRootSignature rootSignature = RHI_MAKE_NULL_HANDLE();
	};

	struct BufferBarrier
	{
		BufferBarrier() = default;
		BufferBarrier(HBuffer buffer_, ResourceStates::Flags newState_)
		{
			buffer = buffer_;
			newState = newState_;
		}

		HBuffer buffer = RHI_MAKE_NULL_HANDLE();
		ResourceStates::Flags newState = ResourceStates::Common;
	};

	struct TextureBarrier
	{
		TextureBarrier() = default;
		TextureBarrier(HTexture texture_, ResourceStates::Flags newState_)
		{
			texture = texture_;
			newState = newState_;
		}

		HTexture texture = RHI_MAKE_NULL_HANDLE();
		ResourceStates::Flags newState = ResourceStates::Common;
	};

	struct DescriptorTableUpdate
	{
		// note that for our texture UAVs,
		// we only allow 2 options:
		// 1) "bind all mips" (mip chain)
		// 2) "bind this single mip" (slide)

		uint32_t firstIndex = 0;
		uint32_t resourceCount = 0;
		DescriptorType::Id type = DescriptorType::Count;
		union // based on type
		{
			const HTexture* textures = NULL;
			const HBuffer* buffers;
			const HSampler* samplers;
		};
		uint32_t uavMipSlice = 0; // UAV textures: bind this specific mip
		bool uavMipChain = false; // UAV textures: bind all mips if true, the specific mip slice otherwise

		void SetSamplers(uint32_t count, const HSampler* samplers_, uint32_t tableIndex = 0)
		{
			firstIndex = tableIndex;
			resourceCount = count;
			type = DescriptorType::Sampler;
			samplers = samplers_;
		}

		void SetBuffers(uint32_t count, const HBuffer* buffers_, uint32_t tableIndex = 0)
		{
			firstIndex = tableIndex;
			resourceCount = count;
			type = DescriptorType::Buffer;
			buffers = buffers_;
		}

		void SetRWBuffers(uint32_t count, const HBuffer* buffers_, uint32_t tableIndex = 0)
		{
			firstIndex = tableIndex;
			resourceCount = count;
			type = DescriptorType::RWBuffer;
			buffers = buffers_;
		}

		void SetTextures(uint32_t count, const HTexture* textures_, uint32_t tableIndex = 0)
		{
			firstIndex = tableIndex;
			resourceCount = count;
			type = DescriptorType::Texture;
			textures = textures_;
		}

		void SetRWTexturesSlice(uint32_t count, const HTexture* textures_, uint32_t tableIndex = 0, uint32_t slice = 0)
		{
			firstIndex = tableIndex;
			resourceCount = count;
			type = DescriptorType::RWTexture;
			textures = textures_;
			uavMipChain = false;
			uavMipSlice = slice;
		}

		void SetRWTexturesChain(uint32_t count, const HTexture* textures_, uint32_t tableIndex = 0)
		{
			firstIndex = tableIndex;
			resourceCount = count;
			type = DescriptorType::RWTexture;
			textures = textures_;
			uavMipChain = true;
			uavMipSlice = 0;
		}
	};

	struct Rect
	{
		Rect() = default;
		Rect(uint32_t x_, uint32_t y_, uint32_t w_, uint32_t h_)
		{
			x = x_;
			y = y_;
			w = w_;
			h = h_;
		}

		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t w = 0;
		uint32_t h = 0;
	};

	struct ShaderDesc
	{
		ShaderDesc() = default;
		ShaderDesc(ShaderStage::Id stage_, uint32_t sourceLength_, const void* source_, const char* entryPoint_ = "main", uint32_t macroCount_ = 0, const ShaderMacro* macros_ = NULL)
		{
			stage = stage_;
			source = source_;
			sourceLength = sourceLength_;
			entryPoint = entryPoint_;
			macroCount = macroCount_;
			macros = macros_;
		}

		ShaderStage::Id stage = ShaderStage::Count;
		uint32_t sourceLength = 0;
		const void* source = NULL;
		const char* entryPoint = "main";
		uint32_t macroCount = 0;
		const ShaderMacro* macros = NULL;
	};

	bool Init(); // true when a full init happened (the device was created)
	void ShutDown(bool destroyWindow);

	void BeginFrame();
	void EndFrame();

	uint32_t GetFrameIndex();
	HTexture GetSwapChainTexture();

	HBuffer CreateBuffer(const BufferDesc& desc);
	void DestroyBuffer(HBuffer buffer);
	uint8_t* MapBuffer(HBuffer buffer);
	void UnmapBuffer(HBuffer buffer);

	HTexture CreateTexture(const TextureDesc& desc);
	void DestroyTexture(HTexture texture);

	HSampler CreateSampler(const SamplerDesc& sampler);
	void DestroySampler(HSampler sampler);

	HRootSignature CreateRootSignature(const RootSignatureDesc& desc);
	void DestroyRootSignature(HRootSignature signature);

	HDescriptorTable CreateDescriptorTable(const DescriptorTableDesc& desc);
	void UpdateDescriptorTable(HDescriptorTable table, const DescriptorTableUpdate& update);
	void DestroyDescriptorTable(HDescriptorTable table);

	HPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);
	HPipeline CreateComputePipeline(const ComputePipelineDesc& desc);
	void DestroyPipeline(HPipeline pipeline);

	HShader CreateShader(const ShaderDesc& desc);
	ShaderByteCode GetShaderByteCode(HShader shader);
	void DestroyShader(HShader shader);

	void CmdBindRenderTargets(uint32_t colorCount, const HTexture* colorTargets, const HTexture* depthStencilTarget);
	void CmdBindRootSignature(HRootSignature rootSignature);
	void CmdBindDescriptorTable(HRootSignature sigHandle, HDescriptorTable table);
	void CmdBindPipeline(HPipeline pipeline);
	void CmdBindVertexBuffers(uint32_t count, const HBuffer* vertexBuffers, const uint32_t* byteStrides, const uint32_t* startByteOffsets);
	void CmdBindIndexBuffer(HBuffer indexBuffer, IndexType::Id type, uint32_t startByteOffset);
	void CmdSetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h, float minDepth = 0.0f, float maxDepth = 1.0f);
	void CmdSetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
	void CmdSetRootConstants(HRootSignature rootSignature, ShaderStage::Id shaderType, const void* constants);
	void CmdDraw(uint32_t vertexCount, uint32_t firstVertex);
	void CmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex);
	void CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
	uint32_t CmdBeginDurationQuery();
	void CmdEndDurationQuery(uint32_t index);
	void CmdBarrier(uint32_t texCount, const TextureBarrier* textures, uint32_t buffCount = 0, const BufferBarrier* buffers = NULL);
	void CmdClearColorTarget(HTexture texture, const vec4_t clearColor, const Rect* rect = NULL);
	void CmdClearDepthStencilTarget(HTexture texture, bool clearDepth, float depth, bool clearStencil = false, uint8_t stencil = 0, const Rect* rect = NULL);
	void CmdInsertDebugLabel(const char* name, float r = 1.0f, float g = 1.0f, float b = 1.0f);
	void CmdBeginDebugLabel(const char* name, float r = 1.0f, float g = 1.0f, float b = 1.0f);
	void CmdEndDebugLabel();
	void CmdSetStencilReference(uint8_t stencilRef);
	void CmdCopyBuffer(HBuffer dest, HBuffer source);
	void CmdSetShadingRate(ShadingRate::Id shadingRate);

	// the duration at index 0 is for the entire frame
	uint32_t GetDurationCount();
	void GetDurations(uint32_t* gpuMicroSeconds);

	uint8_t* BeginBufferUpload(HBuffer buffer);
	void EndBufferUpload(HBuffer buffer);

	void BeginTextureUpload(MappedTexture& mappedTexture, HTexture texture);
	void EndTextureUpload();

	// the temporary command list is guaranteed to be done executing before the next BeginFrame call ends
	void BeginTempCommandList();
	void EndTempCommandList();

	void BeginTextureReadback(MappedTexture& mappedTexture, HTexture texture);
	void EndTextureReadback();

	void WaitUntilDeviceIsIdle();

	const Handle HandleIndexBitCount = 16;
	const Handle HandleIndexBitOffset = 0;
	const Handle HandleGenBitCount = 10;
	const Handle HandleGenBitOffset = 16;
	const Handle HandleTypeBitCount = 6;
	const Handle HandleTypeBitOffset = 26;

	inline Handle CreateHandle(Handle type, Handle index, Handle generation)
	{
		return
			(type << HandleTypeBitOffset) |
			(index << HandleIndexBitOffset) |
			(generation << HandleGenBitOffset);
	}

	inline void DecomposeHandle(Handle* type, Handle* index, Handle* generation, Handle handle)
	{
		*type = (handle >> HandleTypeBitOffset) & RHI_BIT_MASK(HandleTypeBitCount);
		*index = (handle >> HandleIndexBitOffset) & RHI_BIT_MASK(HandleIndexBitCount);
		*generation = (handle >> HandleGenBitOffset) & RHI_BIT_MASK(HandleGenBitCount);
	}

	template<typename T>
	bool IsNullHandle(T handle)
	{
		return RHI_GET_HANDLE_VALUE(handle) == 0;
	}

	template<typename T, typename HT, RHI::Handle RT, int N>
	struct StaticPool
	{
	private:
		struct Item
		{
			T item;
			uint16_t generation;
			uint16_t next : 15;
			uint16_t used : 1;
		};

	public:
		StaticPool()
		{
			Clear();
		}

		void Clear()
		{
			freeList = 0;
			for(int i = 0; i < N; ++i)
			{
				At(i).generation = 0;
				At(i).used = 0;
				At(i).next = i + 1;
			}
			At(N - 1).next = RHI_BIT_MASK(15);
		}

		HT Add(const T& item)
		{
			ASSERT_OR_DIE(freeList < N, "The pool is full");
			At(freeList).item = item;
			At(freeList).used = qtrue;
			const Handle handle = CreateHandle(RT, freeList, At(freeList).generation);
			freeList = At(freeList).next;
			return RHI_MAKE_HANDLE(handle);
		}

		void Remove(HT handle)
		{
			ASSERT_OR_DIE(!IsNullHandle(handle), "Null pool handle");
			Item& item = GetItemRef(handle);
			ASSERT_OR_DIE(item.used, "Memory pool item was already freed");
			item.generation = (item.generation + 1) & RHI_BIT_MASK(HandleGenBitCount);
			item.used = 0;
			item.next = freeList;
			freeList = (uint16_t)(&item - &At(0));
		}

		T& Get(HT handle)
		{
			ASSERT_OR_DIE(!IsNullHandle(handle), "Null pool handle");
			return GetItemRef(handle).item;
		}

		T* TryGet(HT handle)
		{
			if(handle == 0)
			{
				return NULL;
			}

			return &GetItemRef(handle).item;
		}

		bool FindNext(T** object, HT* handle, int* index)
		{
			Q_assert(object);
			Q_assert(handle);
			Q_assert(index);

			for(int i = *index; i < N; ++i)
			{
				if(At(i).used)
				{
					*object = &At(i).item;
					*handle = RHI_MAKE_HANDLE(CreateHandle(RT, i, At(i).generation));
					*index = i + 1;
					return true;
				}
			}

			return false;
		}

		int CountUsedSlots() const
		{
			int used = 0;
			for(int i = 0; i < N; ++i)
			{
				if(At(i).used)
				{
					used++;
				}
			}

			return used;
		}

	private:
		StaticPool(const StaticPool<T, HT, RT, N>&);
		void operator=(const StaticPool<T, HT, RT, N>&);

		Item& GetItemRef(HT handle)
		{
			ASSERT_OR_DIE(!IsNullHandle(handle), "Null pool handle");

			Handle type, index, gen;
			DecomposeHandle(&type, &index, &gen, RHI_GET_HANDLE_VALUE(handle));
			ASSERT_OR_DIE(type == RT, "Invalid pool handle (wrong resource type)");
			ASSERT_OR_DIE(index <= (Handle)N, "Invalid pool handle (bad index)");

			Item& item = At(index);
			ASSERT_OR_DIE(item.used, "Invalid pool handle (unused slot)");
			if(gen > (Handle)item.generation)
			{
				DIE("Invalid pool handle (allocation from the future)");
			}
			else if(gen < (Handle)item.generation)
			{
				DIE("Invalid pool handle (the object has been freed)");
			}

			return item;
		}

		Item& At(uint32_t index)
		{
			ASSERT_OR_DIE(index < N, "Invalid pool index");
			return *(Item*)&items[index * sizeof(Item)];
		}

		const Item& At(uint32_t index) const
		{
			ASSERT_OR_DIE(index < N, "Invalid pool index");
			return *(Item*)&items[index * sizeof(Item)];
		}

		byte items[N * sizeof(Item)];
		uint16_t freeList;

	public:
		const int size = N;
	};

	template<typename T, uint32_t N>
	struct StaticUnorderedArray
	{
		StaticUnorderedArray()
		{
			Clear();
		}

		void Add(const T& value)
		{
			Q_assert(count < N);
			if(count >= N)
			{
				return;
			}

			items[count++] = value;
		}

		void Remove(uint32_t index)
		{
			Q_assert(index < N);
			if(count >= N)
			{
				return;
			}

			if(index < count - 1)
			{
				items[index] = items[count - 1];
			}
			count--;
		}

		void Clear()
		{
			count = 0;
		}

		T& operator[](uint32_t index)
		{
			Q_assert(index < N);

			return items[index];
		}

		const T& operator[](uint32_t index) const
		{
			Q_assert(index < N);

			return items[index];
		}

	private:
		StaticUnorderedArray(const StaticUnorderedArray<T, N>&);
		void operator=(const StaticUnorderedArray<T, N>&);

	public:
		T items[N];
		uint32_t count;
	};

	template<typename T, uint32_t Invalid>
	struct StaticFreeList
	{
		void Init(T* items_, uint32_t size_)
		{
			items = items_;
			size = size_;
			Clear();
		}

		uint32_t Allocate()
		{
			ASSERT_OR_DIE(firstFree != Invalid, "Free list out of memory");

			const T index = firstFree;
			firstFree = items[index];
			items[index] = Invalid;
			allocatedItemCount++;

			return index;
		}

		void Free(uint32_t index)
		{
			ASSERT_OR_DIE(index < size, "Invalid free list slot");

			const T oldList = firstFree;
			firstFree = index;
			items[index] = oldList;
			allocatedItemCount--;
		}

		void Clear()
		{
			for(uint32_t i = 0; i < size - 1; ++i)
			{
				items[i] = i + 1;
			}
			items[size - 1] = Invalid;
			firstFree = 0;
			allocatedItemCount = 0;
		}

		T* items;
		T firstFree;
		uint32_t allocatedItemCount;
		uint32_t size;
	};

	struct LinearAllocator
	{
		void Init(byte* data_, uint32_t size_)
		{
			data = data_;
			size = size_;
			offset = 0;
		}

		void Clear()
		{
			offset = 0;
		}

		const char* Allocate(const char* string)
		{
			const uint32_t l = strlen(string);
			if(offset + l + 1 > size)
			{
				Q_assert(!"StringAllocator ran out of memory");
				return "out of memory";
			}

			char* newString = (char*)data + offset;
			memcpy(newString, string, l);
			newString[l] = '\0';
			offset += l + 1;

			return newString;
		}

		byte* Allocate(uint32_t byteCount)
		{
			if(offset + byteCount > size)
			{
				Q_assert(!"StringAllocator ran out of memory");
				return data;
			}

			byte* newData = data + offset;
			offset += byteCount;

			return newData;
		}

		byte* data = NULL;
		uint32_t size = 0;
		uint32_t offset = 0;
	};
}
