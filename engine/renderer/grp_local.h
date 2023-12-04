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
// Gameplay Rendering Pipeline - private declarations


#pragma once


#include "tr_local.h"
#include "rhi_local.h"


using namespace RHI;


// @TODO: move out
#define CONCAT_IMM(x, y) x ## y
#define CONCAT(x, y) CONCAT_IMM(x, y)


#pragma pack(push, 4)

struct WorldVertexRC
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float clipPlane[4];
};

struct WorldPixelRC
{
	// general
	uint32_t stageIndices[8]; // sampler: 16 - texture: 16
	float greyscale;

	// r_shaderTrace - dynamically enabled
	uint32_t shaderTrace; // shader index: 14 - frame index: 2 - enable: 1
	uint16_t centerPixelX;
	uint16_t centerPixelY;

	// r_depthFade - statically enabled
	uint16_t hFadeDistance;
	uint16_t hFadeOffset;
	uint32_t depthFadeColorTex; // texture index: 12 - color bias: 4 - color scale: 4

	// r_dither - statically enabled
	uint16_t hFrameSeed;
	uint16_t hNoiseScale;
	uint16_t hInvGamma;
	uint16_t hInvBrightness;
};

#pragma pack(pop)

struct BufferBase
{
	bool CanAdd(uint32_t count_)
	{
		return batchFirst + batchCount + count_ <= totalCount;
	}

	void EndBatch()
	{
		batchFirst += batchCount;
		batchCount = 0;
	}

	void EndBatch(uint32_t size)
	{
		batchFirst += size;
		batchCount = 0;
	}

	void Rewind()
	{
		batchFirst = 0;
		batchCount = 0;
	}

	uint32_t totalCount = 0;
	uint32_t batchFirst = 0;
	uint32_t batchCount = 0;
};

struct VertexBuffers : BufferBase
{
	enum BaseId
	{
		BasePosition,
		BaseNormal,
		BaseCount
	};

	enum StageId
	{
		StageTexCoords,
		StageColors,
		StageCount
	};

	void Create(const char* name, MemoryUsage::Id memoryUsage, uint32_t vertexCount)
	{
		totalCount = vertexCount;

		BufferDesc desc = {};
		desc.committedResource = true;
		desc.initialState = ResourceStates::VertexBufferBit;
		desc.memoryUsage = memoryUsage;
		
		desc.name = va("%s position vertex", name);
		desc.byteCount = vertexCount * sizeof(vec3_t);
		buffers[BasePosition] = CreateBuffer(desc);
		strides[BasePosition] = sizeof(vec3_t);

		desc.name = va("%s normal vertex", name);
		desc.byteCount = vertexCount * sizeof(vec3_t);
		buffers[BaseNormal] = CreateBuffer(desc);
		strides[BaseNormal] = sizeof(vec3_t);

		for(uint32_t s = 0; s < MAX_SHADER_STAGES; ++s)
		{
			desc.name = va("%s tex coords #%d vertex", name, (int)s + 1);
			desc.byteCount = vertexCount * sizeof(vec2_t);
			buffers[BaseCount + s * StageCount + StageTexCoords] = CreateBuffer(desc);
			strides[BaseCount + s * StageCount + StageTexCoords] = sizeof(vec2_t);

			desc.name = va("%s color #%d vertex", name, (int)s + 1);
			desc.byteCount = vertexCount * sizeof(color4ub_t);
			buffers[BaseCount + s * StageCount + StageColors] = CreateBuffer(desc);
			strides[BaseCount + s * StageCount + StageColors] = sizeof(color4ub_t);
		}
	}

	void BeginUpload()
	{
		for(uint32_t b = 0; b < BufferCount; ++b)
		{
			mapped[b] = BeginBufferUpload(buffers[b]);
		}
	}

	void EndUpload()
	{
		for(uint32_t b = 0; b < BufferCount; ++b)
		{
			EndBufferUpload(buffers[b]);
			mapped[b] = NULL;
		}
	}

	void Upload(uint32_t firstStage, uint32_t stageCount)
	{
		Q_assert(mapped[0] != NULL);

		const uint32_t batchOffset = batchFirst + batchCount;

		float* pos = (float*)mapped[BasePosition] + 3 * batchOffset;
		for(int v = 0; v < tess.numVertexes; ++v)
		{
			pos[0] = tess.xyz[v][0];
			pos[1] = tess.xyz[v][1];
			pos[2] = tess.xyz[v][2];
			pos += 3;
		}

		float* nor = (float*)mapped[BaseNormal] + 3 * batchOffset;
		for(int v = 0; v < tess.numVertexes; ++v)
		{
			nor[0] = tess.normal[v][0];
			nor[1] = tess.normal[v][1];
			nor[2] = tess.normal[v][2];
			nor += 3;
		}

		for(uint32_t s = 0; s < stageCount; ++s)
		{
			const stageVars_t& sv = tess.svars[s + firstStage];

			uint8_t* const tcBuffer = mapped[BaseCount + s * StageCount + StageTexCoords];
			float* tc = (float*)tcBuffer + 2 * batchOffset;
			memcpy(tc, &sv.texcoords[0], tess.numVertexes * sizeof(vec2_t));

			uint8_t* const colBuffer = mapped[BaseCount + s * StageCount + StageColors];
			uint32_t* col = (uint32_t*)colBuffer + batchOffset;
			memcpy(col, &sv.colors[0], tess.numVertexes * sizeof(color4ub_t));
		}
	}

	static const uint32_t BufferCount = BaseCount + StageCount * MAX_SHADER_STAGES;
	HBuffer buffers[BufferCount] = {};
	uint32_t strides[BufferCount] = {};
	uint8_t* mapped[BufferCount] = {};
};

struct IndexBuffer : BufferBase
{
	void Create(const char* name, MemoryUsage::Id memoryUsage, uint32_t indexCount)
	{
		totalCount = indexCount;

		BufferDesc desc = {};
		desc.committedResource = true;
		desc.initialState = ResourceStates::IndexBufferBit;
		desc.memoryUsage = memoryUsage;
		desc.name = va("%s index", name);
		desc.byteCount = indexCount * sizeof(uint32_t);
		buffer = CreateBuffer(desc);
	}

	void BeginUpload()
	{
		mapped = (uint32_t*)BeginBufferUpload(buffer);
	}

	void EndUpload()
	{
		EndBufferUpload(buffer);
		mapped = NULL;
	}

	void Upload()
	{
		Q_assert(mapped != NULL);

		uint32_t* const idx = mapped + batchFirst + batchCount;
		memcpy(idx, &tess.indexes[0], tess.numIndexes * sizeof(uint32_t));
	}

	uint32_t* GetCurrentAddress()
	{
		return mapped + batchFirst + batchCount;
	}

	HBuffer buffer = RHI_MAKE_NULL_HANDLE();
	uint32_t* mapped = NULL;
};

struct GeometryBuffer : BufferBase
{
	void Init(uint32_t count_, uint32_t stride_)
	{
		buffer = RHI_MAKE_NULL_HANDLE();
		byteCount = count_ * stride_;
		stride = stride_;
		totalCount = count_;
		batchFirst = 0;
		batchCount = 0;
	}

	HBuffer buffer = RHI_MAKE_NULL_HANDLE();
	uint32_t byteCount = 0;
	uint32_t stride = 0;
};

struct GeometryBuffers
{
	void Rewind()
	{
		vertexBuffers.Rewind();
		indexBuffer.Rewind();
	}

	VertexBuffers vertexBuffers;
	IndexBuffer indexBuffer;
};

struct StaticGeometryChunk
{
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t firstGPUVertex;
	uint32_t firstGPUIndex;
	uint32_t firstCPUIndex;
};

struct FrameStats
{
	enum { MaxFrames = 1024 };

	void EndFrame();

	float temp[MaxFrames];
	float p2pMS[MaxFrames];
	stats_t p2pStats;
	int frameCount;
	int frameIndex;
	int skippedFrames;
};

struct BatchType
{
	enum Id
	{
		Standard,
		DynamicLight,
		DepthFade,
		Count
	};
};

struct World
{
	void Init();
	void BeginFrame();
	void EndFrame();
	void Begin();
	void End();
	void DrawPrePass(const drawSceneViewCommand_t& cmd);
	void BeginBatch(const shader_t* shader, bool hasStaticGeo, BatchType::Id batchType);
	void EndBatch();
	void EndSkyBatch();
	void RestartBatch();
	void DrawGUI();
	void ProcessWorld(world_t& world);
	void DrawSceneView(const drawSceneViewCommand_t& cmd);
	void BindVertexBuffers(bool staticGeo, uint32_t firstStage, uint32_t stageCount);
	void BindIndexBuffer(bool staticGeo);
	void DrawFog();
	void DrawLitSurfaces(dlight_t* dl, bool opaque);
	void DrawDynamicLights(bool opaque);
	void DrawSkyBox();
	void DrawClouds();

	typedef uint32_t Index;
	const IndexType::Id indexType = IndexType::UInt32;

	HTexture depthTexture;
	uint32_t depthTextureIndex;

	float clipPlane[4];

	struct BufferFamily
	{
		enum Id
		{
			Invalid,
			PrePass,
			Static,
			Dynamic
		};
	};

	// Z pre-pass
	HRootSignature zppRootSignature;
	HDescriptorTable zppDescriptorTable;
	HPipeline zppPipeline;
	GeometryBuffer zppVertexBuffer;

	// shared
	BufferFamily::Id boundVertexBuffers;
	BufferFamily::Id boundIndexBuffer;
	uint32_t boundStaticVertexBuffersFirst;
	uint32_t boundStaticVertexBuffersCount;
	HPipeline batchPSO;
	BatchType::Id batchType;
	bool batchHasStaticGeo;
	int psoChangeCount;
	bool batchDepthHack;
	bool batchOldDepthHack;
	ShadingRate::Id batchShadingRate;
	ShadingRate::Id batchOldShadingRate;

	// dynamic
	GeometryBuffers dynBuffers[FrameCount];

	// static
	GeometryBuffers statBuffers;
	StaticGeometryChunk statChunks[32768];
	uint32_t statChunkCount;
	uint32_t statIndices[1 << 20];
	uint32_t statIndexCount;

	// fog
	HRootSignature fogRootSignature;
	HDescriptorTable fogDescriptorTable;
	HPipeline fogOutsidePipeline;
	HPipeline fogInsidePipeline;
	HBuffer boxVertexBuffer;
	HBuffer boxIndexBuffer;

	// shader trace
	HBuffer traceRenderBuffer;
	HBuffer traceReadbackBuffer;

	// dynamic lights
	HRootSignature dlRootSignature;
	HPipeline dlPipelines[CT_COUNT * 2 * 2]; // { cull type, polygon offset, depth test }
	bool dlOpaque;
	float dlIntensity; // 1 for most surfaces, but can be scaled down for liquids etc.
	bool dlDepthTestEqual;
	// quick explanation on why dlOpaque is useful in the first place:
	// - opaque surfaces can have a diffuse texture whose alpha isn't 255 everywhere
	// - when that happens and we multiply the color by the the alpha (DL uses additive blending),
	//   we get "light holes" in opaque surfaces, which is not what we want
};

struct UI
{
	void Init();
	void BeginFrame();
	void Begin();
	void End();
	void DrawBatch();
	void UISetColor(const uiSetColorCommand_t& cmd);
	void UIDrawQuad(const uiDrawQuadCommand_t& cmd);
	void UIDrawTriangle(const uiDrawTriangleCommand_t& cmd);

	// 32-bit needed until the render logic is fixed!
	typedef uint32_t Index;
	const IndexType::Id indexType = IndexType::UInt32;

	uint32_t renderPassIndex;

#pragma pack(push, 1)
	struct Vertex
	{
		vec2_t position;
		vec2_t texCoords;
		uint32_t color;
	};
#pragma pack(pop)
	int maxIndexCount;
	int maxVertexCount;
	int firstIndex;
	int firstVertex;
	int indexCount;
	int vertexCount;
	HRootSignature rootSignature;
	HPipeline pipeline;
	HBuffer indexBuffer;
	HBuffer vertexBuffer;
	Index* indices;
	Vertex* vertices;
	uint32_t color;
	const shader_t* shader;
};

struct MipMapGenerator
{
	void Init();
	void GenerateMipMaps(HTexture texture);

	struct Stage
	{
		enum Id
		{
			Start,      // gamma to linear
			DownSample, // down sample on 1 axis
			End,        // linear to gamma
			Count
		};

		HRootSignature rootSignature;
		HDescriptorTable descriptorTable;
		HPipeline pipeline;
	};

	struct MipSlice
	{
		enum Id
		{
			Float16_0,
			Float16_1,
			Count
		};
	};

	HTexture textures[MipSlice::Count];
	Stage stages[3];
};

struct ImGUI
{
	void Init();
	void RegisterFontAtlas();
	void Draw();
	void SafeBeginFrame();
	void SafeEndFrame();

	struct FrameResources
	{
		HBuffer indexBuffer;
		HBuffer vertexBuffer;
	};

	HRootSignature rootSignature;
	HPipeline pipeline;
	HTexture fontAtlas;
	FrameResources frameResources[FrameCount];
	bool frameStarted = false;
};

struct RenderMode
{
	enum Id
	{
		None,
		UI,
		World,
		ImGui,
		Count
	};
};

struct RenderPassQueries
{
	char name[64];
	uint32_t gpuDurationUS;
	uint32_t cpuDurationUS;
	int64_t cpuStartUS;
	uint32_t queryIndex;
};

enum
{
	MaxRenderPasses = 64, // cg_draw3dIcons forces tons of 2D/3D transitions...
	MaxStatsFrameCount = 64
};

struct RenderPassStats
{
	void EndFrame(uint32_t cpu, uint32_t gpu);

	uint32_t samplesCPU[MaxStatsFrameCount];
	uint32_t samplesGPU[MaxStatsFrameCount];
	stats_t statsCPU;
	stats_t statsGPU;
	uint32_t count;
	uint32_t index;
};

struct RenderPassFrame
{
	RenderPassQueries passes[MaxRenderPasses];
	uint32_t count;
};

#pragma pack(push, 1)

struct PSODesc
{
	cullType_t cullType;
	bool polygonOffset;
	bool clampDepth;
	bool depthFade;
};

#pragma pack(pop)

struct CachedPSO
{
#if defined(_DEBUG)
	// lets us know which Q3 shader
	// triggered the creation of this PSO
	char name[MAX_QPATH];
#endif
	PSODesc desc;
	uint32_t stageStateBits[MAX_SHADER_STAGES];
	uint32_t stageCount;
	HPipeline pipeline;
};

struct PostProcess
{
	void Init();
	void Draw(const char* renderPassName, HTexture renderTarget);
	void ToneMap();
	void InverseToneMap(int colorFormat);
	void SetToneMapInput(HTexture toneMapInput);
	void SetInverseToneMapInput(HTexture inverseToneMapInput);

	HPipeline toneMapPipeline;
	HRootSignature toneMapRootSignature;
	HDescriptorTable toneMapDescriptorTable;

	HPipeline inverseToneMapPipelines[RTCF_COUNT];
	HRootSignature inverseToneMapRootSignature;
	HDescriptorTable inverseToneMapDescriptorTable;
};

struct SMAA
{
	void Init();
	void Update();
	void Draw(const viewParms_t& parms);

	// matches r_smaa
	struct Mode
	{
		enum Id
		{
			Disabled,
			Low,
			Medium,
			High,
			Ultra,
			Count
		};
	};

	// fixed
	HTexture areaTexture;
	HTexture searchTexture;
	HRootSignature rootSignature;
	HDescriptorTable descriptorTable;

	// depends on render target resolution
	HTexture edgeTexture;
	HTexture blendTexture;
	HTexture stencilTexture;
	HTexture inputTexture;  // tone mapped
	HTexture outputTexture; // tone mapped

	// depends on selected preset/mode
	// SMAA has 3 passes:
	// 1. edge detection
	// 2. blend weight computation
	// 3. neighborhood blending
	HPipeline firstPassPipeline;
	HPipeline secondPassPipeline;
	HPipeline thirdPassPipeline;

	Mode::Id mode = Mode::Disabled;
	int width = -1;
	int height = -1;
	bool fixedLoaded = false;
};

struct GRP : IRenderPipeline
{
	void Init() override;
	void ShutDown(bool fullShutDown) override;
	void CreateTexture(image_t* image, int mipCount, int width, int height) override;
	void UpoadTextureAndGenerateMipMaps(image_t* image, const byte* data) override;
	void BeginTextureUpload(MappedTexture& mappedTexture, image_t* image) override;
	void EndTextureUpload() override;
	void ProcessWorld(world_t& world) override;
	void ProcessModel(model_t& model) override;
	void ProcessShader(shader_t& shader) override;

	void ExecuteRenderCommands(const byte* data, bool readbackRequested) override;

	void UISetColor(const uiSetColorCommand_t& cmd) override { ui.UISetColor(cmd); }
	void UIDrawQuad(const uiDrawQuadCommand_t& cmd) override { ui.UIDrawQuad(cmd); }
	void UIDrawTriangle(const uiDrawTriangleCommand_t& cmd) override { ui.UIDrawTriangle(cmd); }
	void DrawSceneView(const drawSceneViewCommand_t& cmd) override { world.DrawSceneView(cmd); }
	void TessellationOverflow() override { world.RestartBatch(); }
	void DrawSkyBox() override { world.DrawSkyBox(); }
	void DrawClouds() override { world.DrawClouds(); }

	void ReadPixels(int w, int h, int alignment, colorSpace_t colorSpace, void* out) override;

	void BeginFrame();
	void EndFrame();

	uint32_t RegisterTexture(HTexture htexture);

	uint32_t BeginRenderPass(const char* name, float r, float g, float b);
	void EndRenderPass(uint32_t index);

	void DrawGUI();

	uint32_t CreatePSO(CachedPSO& cache, const char* name);

	void UpdateReadbackTexture();

	UI ui;
	World world;
	MipMapGenerator mipMapGen;
	ImGUI imgui;
	PostProcess post;
	SMAA smaa;
	bool firstInit = true;
	RenderMode::Id renderMode; // necessary for sampler selection, useful for debugging
	float frameSeed;
	bool updateReadbackTexture;

	// @TODO: what's up with rootSignature and uberRootSignature?
	// probably need to nuke one of them...

	HTexture renderTarget;
	TextureFormat::Id renderTargetFormat;
	HTexture readbackRenderTarget;
	RootSignatureDesc rootSignatureDesc;
	HRootSignature rootSignature;
	HDescriptorTable descriptorTable;
	uint32_t textureIndex;
	HSampler samplers[TW_COUNT * TextureFilter::Count * MaxTextureMips];

	RenderPassFrame renderPasses[FrameCount];
	RenderPassFrame tempRenderPasses;
	RenderPassStats renderPassStats[MaxRenderPasses];
	RenderPassStats wholeFrameStats;

	FrameStats frameStats;

	CachedPSO psos[1024];
	uint32_t psoCount;
	HRootSignature uberRootSignature;
};

extern GRP grp;

struct ScopedRenderPass
{
	ScopedRenderPass(const char* name, float r, float g, float b)
	{
		index = grp.BeginRenderPass(name, r, g, b);
	}

	~ScopedRenderPass()
	{
		grp.EndRenderPass(index);
	}

	uint32_t index;
};

#define SCOPED_RENDER_PASS(Name, R, G, B) ScopedRenderPass CONCAT(rp_, __LINE__)(Name, R, G, B)

inline void CmdSetViewportAndScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	CmdSetViewport(x, y, w, h);
	CmdSetScissor(x, y, w, h);
}

inline void CmdSetViewportAndScissor(const viewParms_t& vp)
{
	CmdSetViewportAndScissor(vp.viewportX, vp.viewportY, vp.viewportWidth, vp.viewportHeight);
}

inline bool IsDepthFadeEnabled(const shader_t& shader)
{
	return
		r_depthFade->integer != 0 &&
		shader.dfType > DFT_NONE &&
		shader.dfType < DFT_TBD;
}

const image_t* GetBundleImage(const textureBundle_t& bundle);
uint32_t GetSamplerIndex(textureWrap_t wrap, TextureFilter::Id filter, uint32_t minLOD = 0);
uint32_t GetSamplerIndex(const image_t* image);
