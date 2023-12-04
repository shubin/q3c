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
// Gameplay Rendering Pipeline - world/3D rendering


//#define ENABLE_GUI


#include "grp_local.h"
#include "../client/cl_imgui.h"
namespace zpp
{
#include "hlsl/depth_pre_pass_vs.h"
#include "hlsl/depth_pre_pass_ps.h"
}
namespace fog
{
#include "hlsl/fog_vs.h"
}
namespace fog_inside
{
#include "hlsl/fog_inside_ps.h"
}
namespace fog_outside
{
#include "hlsl/fog_outside_ps.h"
}
namespace dyn_light
{
#include "hlsl/dynamic_light_vs.h"
#include "hlsl/dynamic_light_ps.h"
}


#pragma pack(push, 4)

struct ZPPVertexRC
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
};

struct FogVertexRC
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float boxMin[4];
	float boxMax[4];
};

struct FogPixelRC
{
	float colorDepth[4];
};

struct DynLightVertexRC
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float osLightPos[4];
	float osEyePos[4];
};

struct DynLightPixelRC
{
	uint32_t stageIndices; // low 16 = texture, high 16 = sampler
	uint32_t pad0[3];
	float color[3];
	float recSqrRadius; // 1 / (r * r)
	float greyscale;
	float intensity;
	float opaque;
	float pad1;
};

#pragma pack(pop)


static bool drawPrePass = true;
static bool drawDynamic = true;
static bool drawTransparents = true;
static bool drawFog = true;
static bool drawSky = true;
static bool drawClouds = true;
static bool forceDynamic = false;
static int zppMaxTriangleCount = 64;
static float zppMinRadiusOverZ = 0.5f;
static int zppDrawnTriangleCount = 0;

static const uint32_t zppMaxVertexCount = 1 << 20;
static const uint32_t zppMaxIndexCount = 8 * zppMaxVertexCount;
static uint32_t zppIndices[zppMaxIndexCount];


static bool HasStaticGeo(int staticGeoChunk, const shader_t* shader)
{
	if(forceDynamic)
	{
		return false;
	}

	// @NOTE: the shader->isDynamic check is needed because of the shader editing feature
	// without it, an edited shader that changes vertex attributes won't display correctly
	// because the original "baked" vertex attributes would be used instead
	return
		!shader->isDynamic &&
		staticGeoChunk > 0 &&
		staticGeoChunk < ARRAY_LEN(grp.world.statChunks);
}

static void UpdateEntityData(bool& depthHack, int entityNum, double originalTime)
{
	depthHack = false;

	if(entityNum != ENTITYNUM_WORLD)
	{
		backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
		if(backEnd.currentEntity->intShaderTime)
			backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.iShaderTime / 1000.0;
		else
			backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime.fShaderTime;
		// we have to reset the shaderTime as well otherwise image animations start
		// from the wrong frame
		tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

		// set up the transformation matrix
		R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.orient);

		if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
		{
			depthHack = true;
		}
	}
	else
	{
		backEnd.currentEntity = &tr.worldEntity;
		backEnd.refdef.floatTime = originalTime;
		backEnd.orient = backEnd.viewParms.world;
		// we have to reset the shaderTime as well otherwise image animations on
		// the world (like water) continue with the wrong frame
		tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	}
}

static int GetDynamicLightPipelineIndex(cullType_t cullType, qbool polygonOffset, qbool depthTestEquals)
{
	return (int)cullType + CT_COUNT * (int)polygonOffset + CT_COUNT * 2 * (int)depthTestEquals;
}


void World::Init()
{
	if(grp.firstInit)
	{
		fogDescriptorTable = RHI_MAKE_NULL_HANDLE();
	}

	{
		TextureDesc desc("depth buffer", glConfig.vidWidth, glConfig.vidHeight);
		desc.committedResource = true;
		desc.shortLifeTime = true;
		desc.initialState = ResourceStates::DepthWriteBit;
		desc.allowedState = ResourceStates::DepthAccessBits | ResourceStates::PixelShaderAccessBit;
		desc.format = TextureFormat::Depth32_Float;
		desc.SetClearDepthStencil(0.0f, 0);
		depthTexture = CreateTexture(desc);
		depthTextureIndex = grp.RegisterTexture(depthTexture);

		if(!IsNullHandle(fogDescriptorTable))
		{
			DescriptorTableUpdate update;
			update.SetTextures(1, &depthTexture);
			UpdateDescriptorTable(fogDescriptorTable, update);
		}
	}

	if(grp.firstInit)
	{
		//
		// depth pre-pass
		//
		{
			RootSignatureDesc desc("Z pre-pass");
			desc.usingVertexBuffers = true;
			desc.constants[ShaderStage::Vertex].byteCount = sizeof(ZPPVertexRC);
			desc.constants[ShaderStage::Pixel].byteCount = 0;
			desc.samplerVisibility = ShaderStages::None;
			desc.genericVisibility = ShaderStages::None;
			zppRootSignature = CreateRootSignature(desc);
		}
		{
			zppDescriptorTable = CreateDescriptorTable(DescriptorTableDesc("Z pre-pass", zppRootSignature));
		}
		{
			// we could handle all 3 cull mode modes and alpha testing, but we do a partial pre-pass
			// it wouldn't make sense going any further instead of trying a visibility buffer approach
			GraphicsPipelineDesc desc("Z pre-pass", zppRootSignature);
			desc.vertexShader = ShaderByteCode(zpp::g_vs);
			desc.pixelShader = ShaderByteCode(zpp::g_ps);
			desc.vertexLayout.AddAttribute(0, ShaderSemantic::Position, DataType::Float32, 3, 0);
			desc.depthStencil.depthComparison = ComparisonFunction::GreaterEqual;
			desc.depthStencil.depthStencilFormat = TextureFormat::Depth32_Float;
			desc.depthStencil.enableDepthTest = true;
			desc.depthStencil.enableDepthWrites = true;
			desc.rasterizer.cullMode = CT_FRONT_SIDED;
			zppPipeline = CreateGraphicsPipeline(desc);
		}
		{
			zppVertexBuffer.Init(zppMaxVertexCount, 3 * sizeof(float));
			{
				BufferDesc desc("depth pre-pass vertex", zppVertexBuffer.byteCount, ResourceStates::VertexBufferBit);
				zppVertexBuffer.buffer = CreateBuffer(desc);
			}
		}

		//
		// dynamic (streamed) geometry
		//
		for(uint32_t f = 0; f < FrameCount; ++f)
		{
			// the doubled index count is for the depth pre-pass
			const int MaxDynamicVertexCount = 256 << 10;
			const int MaxDynamicIndexCount = MaxDynamicVertexCount * 8 * 2;
			GeometryBuffers& db = dynBuffers[f];
			db.vertexBuffers.Create(va("dynamic #%d", f + 1), MemoryUsage::Upload, MaxDynamicVertexCount);
			db.indexBuffer.Create(va("dynamic #%d", f + 1), MemoryUsage::Upload, MaxDynamicIndexCount);
		}

		//
		// static (GPU-resident) geometry
		//
		{
			const int MaxVertexCount = 256 << 10;
			const int MaxIndexCount = MaxVertexCount * 8;
			statBuffers.vertexBuffers.Create("static", MemoryUsage::GPU, MaxVertexCount);
			statBuffers.indexBuffer.Create("static", MemoryUsage::GPU, MaxIndexCount);
		}

		//
		// fog
		//
		{
			RootSignatureDesc desc("fog");
			desc.usingVertexBuffers = true;
			desc.AddRange(DescriptorType::Texture, 0, 1);
			desc.genericVisibility = ShaderStages::PixelBit;
			desc.constants[ShaderStage::Vertex].byteCount = sizeof(FogVertexRC);
			desc.constants[ShaderStage::Pixel].byteCount = sizeof(FogPixelRC);
			fogRootSignature = CreateRootSignature(desc);
		}
		{
			DescriptorTableDesc desc("fog", fogRootSignature);
			fogDescriptorTable = CreateDescriptorTable(desc);

			DescriptorTableUpdate update;
			update.SetTextures(1, &depthTexture);
			UpdateDescriptorTable(fogDescriptorTable, update);
		}
		{
			const uint32_t indices[] =
			{
				0, 1, 2, 2, 1, 3,
				4, 0, 6, 6, 0, 2,
				7, 5, 6, 6, 5, 4,
				3, 1, 7, 7, 1, 5,
				4, 5, 0, 0, 5, 1,
				3, 7, 2, 2, 7, 6
			};

			BufferDesc desc("box index", sizeof(indices), ResourceStates::IndexBufferBit);
			boxIndexBuffer = CreateBuffer(desc);

			uint8_t* mapped = BeginBufferUpload(boxIndexBuffer);
			memcpy(mapped, indices, sizeof(indices));
			EndBufferUpload(boxIndexBuffer);
		}
		{
			const float vertices[] =
			{
				0.0f, 1.0f, 0.0f,
				1.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f,
				1.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 1.0f,
				1.0f, 1.0f, 1.0f,
				0.0f, 0.0f, 1.0f,
				1.0f, 0.0f, 1.0f
			};

			BufferDesc desc("box vertex", sizeof(vertices), ResourceStates::VertexBufferBit);
			boxVertexBuffer = CreateBuffer(desc);

			uint8_t* mapped = BeginBufferUpload(boxVertexBuffer);
			memcpy(mapped, vertices, sizeof(vertices));
			EndBufferUpload(boxVertexBuffer);
		}

		//
		// shader trace
		//
		{
			BufferDesc desc("shader trace opaque", 2 * sizeof(uint32_t), ResourceStates::UnorderedAccessBit);
			traceRenderBuffer = CreateBuffer(desc);

			DescriptorTableUpdate update;
			update.SetRWBuffers(1, &traceRenderBuffer, MAX_DRAWIMAGES * 2);
			UpdateDescriptorTable(grp.descriptorTable, update);
		}
		{
			BufferDesc desc("shader trace opaque readback", 2 * sizeof(uint32_t), ResourceStates::Common);
			desc.memoryUsage = MemoryUsage::Readback;
			traceReadbackBuffer = CreateBuffer(desc);
		}

		//
		// dynamic lights
		//
		{
			RootSignatureDesc desc("dynamic lights");
			desc.usingVertexBuffers = true;
			desc.samplerCount = ARRAY_LEN(grp.samplers);
			desc.samplerVisibility = ShaderStages::PixelBit;
			desc.genericVisibility = ShaderStages::VertexBit | ShaderStages::PixelBit;
			desc.AddRange(DescriptorType::Texture, 0, MAX_DRAWIMAGES * 2);
			desc.AddRange(DescriptorType::RWBuffer, MAX_DRAWIMAGES * 2, 1);
			desc.constants[ShaderStage::Vertex].byteCount = sizeof(DynLightVertexRC);
			desc.constants[ShaderStage::Pixel].byteCount = sizeof(DynLightPixelRC);
			dlRootSignature = CreateRootSignature(desc);
		}
	}

	//
	// fog
	//
	{
		GraphicsPipelineDesc desc("fog outside", fogRootSignature);
		desc.shortLifeTime = true;
		desc.vertexShader = ShaderByteCode(fog::g_vs);
		desc.pixelShader = ShaderByteCode(fog_outside::g_ps);
		desc.depthStencil.DisableDepth();
		desc.rasterizer.cullMode = CT_BACK_SIDED;
		desc.rasterizer.polygonOffset = false;
		desc.rasterizer.clampDepth = true;
		desc.AddRenderTarget(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, grp.renderTargetFormat);
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Position, DataType::Float32, 3, 0);
		fogOutsidePipeline = CreateGraphicsPipeline(desc);
	}
	{
		GraphicsPipelineDesc desc("fog inside", fogRootSignature);
		desc.shortLifeTime = true;
		desc.vertexShader = ShaderByteCode(fog::g_vs);
		desc.pixelShader = ShaderByteCode(fog_inside::g_ps);
		desc.depthStencil.DisableDepth();
		desc.rasterizer.cullMode = CT_FRONT_SIDED;
		desc.rasterizer.polygonOffset = false;
		desc.rasterizer.clampDepth = true;
		desc.AddRenderTarget(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, grp.renderTargetFormat);
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Position, DataType::Float32, 3, 0);
		fogInsidePipeline = CreateGraphicsPipeline(desc);
	}

	//
	// dynamic lights
	//
	{
		GraphicsPipelineDesc desc("dynamic light opaque", dlRootSignature);
		desc.shortLifeTime = true;
		desc.vertexShader = ShaderByteCode(dyn_light::g_vs);
		desc.pixelShader = ShaderByteCode(dyn_light::g_ps);
		desc.depthStencil.enableDepthWrites = false;
		desc.rasterizer.clampDepth = r_depthClamp->integer != 0;
		desc.AddRenderTarget(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, grp.renderTargetFormat);
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Position, DataType::Float32, 3, 0);
		desc.vertexLayout.AddAttribute(1, ShaderSemantic::Normal, DataType::Float32, 3, 0);
		desc.vertexLayout.AddAttribute(2, ShaderSemantic::TexCoord, DataType::Float32, 2, 0);
		desc.vertexLayout.AddAttribute(3, ShaderSemantic::Color, DataType::UNorm8, 4, 0);
		for(int cullType = 0; cullType < CT_COUNT; ++cullType)
		{
			desc.rasterizer.cullMode = (cullType_t)cullType;
			for(int polygonOffset = 0; polygonOffset < 2; ++polygonOffset)
			{
				desc.rasterizer.polygonOffset = polygonOffset != 0;
				for(int depthTestEquals = 0; depthTestEquals < 2; ++depthTestEquals)
				{
					desc.depthStencil.depthComparison = depthTestEquals ? ComparisonFunction::Equal : ComparisonFunction::GreaterEqual;
					const int index = GetDynamicLightPipelineIndex((cullType_t)cullType, (qbool)polygonOffset, (qbool)depthTestEquals);
					dlPipelines[index] = CreateGraphicsPipeline(desc);
				}
			}
		}
	}
}

void World::BeginFrame()
{
	dynBuffers[GetFrameIndex()].Rewind();

	boundVertexBuffers = BufferFamily::Invalid;
	boundIndexBuffer = BufferFamily::Invalid;
	boundStaticVertexBuffersFirst = 0;
	boundStaticVertexBuffersCount = UINT32_MAX;

	DrawGUI();

	psoChangeCount = 0; // read by the GUI code
}

void World::EndFrame()
{
	tr.tracedWorldShaderIndex = -1;
	if(tr.traceWorldShader && tr.world != NULL)
	{
		// schedule a GPU -> CPU transfer
		{
			BufferBarrier barrier(traceRenderBuffer, ResourceStates::CopySourceBit);
			CmdBarrier(0, NULL, 1, &barrier);
		}
		CmdCopyBuffer(traceReadbackBuffer, traceRenderBuffer);
		{
			BufferBarrier barrier(traceRenderBuffer, ResourceStates::UnorderedAccessBit);
			CmdBarrier(0, NULL, 1, &barrier);
		}

		// grab last frame's result
		uint32_t* shaderIndices = (uint32_t*)MapBuffer(traceReadbackBuffer);
		const uint32_t shaderIndex = shaderIndices[RHI::GetFrameIndex() ^ 1];
		UnmapBuffer(traceReadbackBuffer);
		if(shaderIndex < (uint32_t)tr.numShaders)
		{
			tr.tracedWorldShaderIndex = (int)shaderIndex;
		}
	}
}

void World::Begin()
{
	grp.renderMode = RenderMode::World;

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

		memcpy(clipPlane, plane, sizeof(clipPlane));
	}
	else
	{
		memset(clipPlane, 0, sizeof(clipPlane));
	}

	CmdSetViewportAndScissor(backEnd.viewParms);
	batchOldDepthHack = false;
	batchDepthHack = false;
	batchOldShadingRate = ShadingRate::SR_1x1;
	batchShadingRate = ShadingRate::SR_1x1;

	TextureBarrier tb(depthTexture, ResourceStates::DepthWriteBit);
	CmdBarrier(1, &tb);
}

void World::End()
{
	grp.renderMode = RenderMode::None;
}

void World::DrawPrePass(const drawSceneViewCommand_t& cmd)
{
	if(!drawPrePass || tr.world == NULL)
	{
		return;
	}

	GeometryBuffers& db = dynBuffers[GetFrameIndex()];
	const uint32_t firstIndex = db.indexBuffer.batchFirst;
	uint32_t* indices = db.indexBuffer.GetCurrentAddress();
	int fullIndexCount = 0;

	// we can't do a single draw because some world surfaces could be referenced by animated entities
	const int surfCount = cmd.numDrawSurfs;
	const drawSurf_t* drawSurf = cmd.drawSurfs;
	for(int ds = 0; ds < surfCount; ++ds, ++drawSurf)
	{
		int entityNum = 0;
		const shader_t* shader = NULL;
		R_DecomposeSort(drawSurf->sort, &entityNum, &shader);
		const int surfIndexCount = drawSurf->zppIndexCount;
		if(surfIndexCount <= 0 ||
			entityNum != ENTITYNUM_WORLD ||
			surfIndexCount > zppMaxTriangleCount * 3 ||
			drawSurf->radiusOverZ < zppMinRadiusOverZ)
		{
			continue;
		}

		if(!db.indexBuffer.CanAdd(surfIndexCount))
		{
			Q_assert(!"Dynamic buffer too small!");
			break;
		}
		
		memcpy(indices, zppIndices + drawSurf->zppFirstIndex, surfIndexCount * sizeof(uint32_t));
		fullIndexCount += surfIndexCount;
		indices += surfIndexCount;
		db.indexBuffer.EndBatch(surfIndexCount);
	}
	zppDrawnTriangleCount = fullIndexCount;
	if(fullIndexCount <= 0)
	{
		return;
	}

	SCOPED_RENDER_PASS("Depth Pre-Pass", 0.75f, 0.75f, 0.375f);

	CmdBindRenderTargets(0, NULL, &depthTexture);
	CmdBindRootSignature(zppRootSignature);
	CmdBindPipeline(zppPipeline);
	psoChangeCount++;
	CmdBindDescriptorTable(zppRootSignature, zppDescriptorTable);

	ZPPVertexRC vertexRC;
	memcpy(vertexRC.modelViewMatrix, backEnd.viewParms.world.modelMatrix, sizeof(vertexRC.modelViewMatrix));
	memcpy(vertexRC.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(vertexRC.projectionMatrix));
	CmdSetRootConstants(zppRootSignature, ShaderStage::Vertex, &vertexRC);
	const uint32_t vertexStride = 3 * sizeof(float);
	CmdBindVertexBuffers(1, &zppVertexBuffer.buffer, &vertexStride, NULL);
	boundVertexBuffers = BufferFamily::PrePass;
	BindIndexBuffer(false);
	CmdDrawIndexed(fullIndexCount, firstIndex, 0);
}

void World::BeginBatch(const shader_t* shader, bool hasStaticGeo, BatchType::Id type)
{
	if(type == BatchType::DepthFade && batchType != BatchType::DepthFade)
	{
		TextureBarrier barrier(depthTexture, ResourceStates::PixelShaderAccessBit);
		CmdBarrier(1, &barrier);
		CmdBindRenderTargets(1, &grp.renderTarget, NULL);
	}
	else if(type != BatchType::DepthFade && batchType == BatchType::DepthFade)
	{
		TextureBarrier barrier(depthTexture, ResourceStates::DepthWriteBit);
		CmdBarrier(1, &barrier);
		CmdBindRenderTargets(1, &grp.renderTarget, &depthTexture);
	}

	tess.numVertexes = 0;
	tess.numIndexes = 0;
	tess.depthFade = DFT_NONE;
	tess.deformsPreApplied = qfalse;
	tess.xstages = (const shaderStage_t**)shader->stages;
	tess.shader = shader;
	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if(tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime)
	{
		tess.shaderTime = tess.shader->clampTime;
	}
	batchHasStaticGeo = hasStaticGeo;
	batchType = type;
}

void World::EndBatch()
{
	const int vertexCount = tess.numVertexes;
	const int indexCount = tess.numIndexes;
	if((!batchHasStaticGeo && vertexCount <= 0) ||
		indexCount <= 0 ||
		tess.shader->numStages == 0 ||
		tess.shader->numPipelines <= 0)
	{
		goto clean_up;
	}

	if(batchType == BatchType::DynamicLight)
	{
		if(tess.shader->lightingStages[ST_DIFFUSE] < 0 ||
			tess.shader->lightingStages[ST_DIFFUSE] >= MAX_SHADER_STAGES)
		{
			goto clean_up;
		}
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if(r_debugSort->value > 0.0f &&
		r_debugSort->value < tess.shader->sort)
	{
		goto clean_up;
	}

	GeometryBuffers& db = dynBuffers[GetFrameIndex()];
	if(!db.vertexBuffers.CanAdd(vertexCount) ||
		!db.indexBuffer.CanAdd(indexCount))
	{
		Q_assert(!"Dynamic buffer too small!");
		goto clean_up;
	}

	const shader_t* const shader = tess.shader;
	if(!tess.deformsPreApplied)
	{
		RB_DeformTessGeometry(0, vertexCount, 0, indexCount);
		if(batchType == BatchType::DynamicLight)
		{
			const int stageIndex = shader->lightingStages[ST_DIFFUSE];
			if(stageIndex >= 0 && stageIndex < MAX_SHADER_STAGES)
			{
				R_ComputeColors(shader->stages[stageIndex], tess.svars[0], 0, vertexCount);
				R_ComputeTexCoords(shader->stages[stageIndex], tess.svars[0], 0, vertexCount, qfalse);
			}
		}
		else
		{
			for(int s = 0; s < shader->numStages; ++s)
			{
				R_ComputeColors(shader->stages[s], tess.svars[s], 0, vertexCount);
				R_ComputeTexCoords(shader->stages[s], tess.svars[s], 0, vertexCount, qfalse);
			}
		}
	}

	// darken the hell fog in the BS pit on cpm21 for debugging
#if 0
	if(Q_stristr(shader->name, "hellfog"))
	{
		for(int v = 0; v < vertexCount; ++v)
		{
			tess.svars[0].colors[v][0] = 127;
			tess.svars[0].colors[v][1] = 127;
			tess.svars[0].colors[v][2] = 127;
			tess.svars[0].colors[v][3] = 255;
		}
	}
#endif

	if(!batchHasStaticGeo)
	{
		db.vertexBuffers.Upload(0, shader->numStages);
	}
	db.indexBuffer.Upload();

	BindIndexBuffer(false);

	if(batchDepthHack != batchOldDepthHack)
	{
		const viewParms_t& vp = backEnd.viewParms;
		CmdSetViewport(vp.viewportX, vp.viewportY, vp.viewportWidth, vp.viewportHeight, batchDepthHack ? 0.7f : 0.0f, 1.0f);
		batchOldDepthHack = batchDepthHack;
	}

	if(batchShadingRate != batchOldShadingRate)
	{
		CmdSetShadingRate(batchShadingRate);
		batchOldShadingRate = batchShadingRate;
	}

	if(batchType == BatchType::DynamicLight)
	{
		const dlight_t* const dl = tess.light;
		const int stageIndex = tess.shader->lightingStages[ST_DIFFUSE];
		const image_t* image = GetBundleImage(shader->stages[stageIndex]->bundle);
		const uint32_t texIdx = image->textureIndex;
		const uint32_t sampIdx = GetSamplerIndex(image);
		Q_assert(texIdx > 0);

		const int pipelineIndex = GetDynamicLightPipelineIndex(shader->cullType, shader->polygonOffset, dlDepthTestEqual);
		const HPipeline pipeline = dlPipelines[pipelineIndex];
		if(batchPSO != pipeline)
		{
			batchPSO = pipeline;
			CmdBindPipeline(batchPSO);
			psoChangeCount++;
		}

		DynLightVertexRC vertexRC = {};
		memcpy(vertexRC.modelViewMatrix, backEnd.orient.modelMatrix, sizeof(vertexRC.modelViewMatrix));
		memcpy(vertexRC.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(vertexRC.projectionMatrix));
		VectorCopy(dl->transformed, vertexRC.osLightPos);
		vertexRC.osLightPos[3] = 1.0f;
		VectorCopy(backEnd.orient.viewOrigin, vertexRC.osEyePos);
		vertexRC.osEyePos[3] = 1.0f;
		CmdSetRootConstants(dlRootSignature, ShaderStage::Vertex, &vertexRC);

		DynLightPixelRC pixelRC = {};
		pixelRC.greyscale = tess.greyscale;
		pixelRC.stageIndices = (sampIdx << 16) | (texIdx);
		VectorCopy(tess.light->color, pixelRC.color);
		pixelRC.recSqrRadius = 1.0f / Square(dl->radius);
		pixelRC.intensity = dlIntensity;
		pixelRC.opaque = dlOpaque ? 1.0f : 0.0f;
		CmdSetRootConstants(dlRootSignature, ShaderStage::Pixel, &pixelRC);

		BindVertexBuffers(batchHasStaticGeo, 0, 1);
		CmdDrawIndexed(indexCount, db.indexBuffer.batchFirst, batchHasStaticGeo ? 0 : db.vertexBuffers.batchFirst);
	}
	else
	{
		for(int p = 0; p < shader->numPipelines; ++p)
		{
			const pipeline_t& pipeline = shader->pipelines[p];
			const int psoIndex = backEnd.viewParms.isMirror ? pipeline.mirrorPipeline : pipeline.pipeline;
			if(batchPSO != grp.psos[psoIndex].pipeline)
			{
				batchPSO = grp.psos[psoIndex].pipeline;
				CmdBindPipeline(batchPSO);
				psoChangeCount++;
			}

			WorldVertexRC vertexRC = {};
			memcpy(vertexRC.modelViewMatrix, backEnd.orient.modelMatrix, sizeof(vertexRC.modelViewMatrix));
			memcpy(vertexRC.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(vertexRC.projectionMatrix));
			memcpy(vertexRC.clipPlane, clipPlane, sizeof(vertexRC.clipPlane));
			CmdSetRootConstants(grp.uberRootSignature, ShaderStage::Vertex, &vertexRC);

			WorldPixelRC pixelRC = {};
			pixelRC.greyscale = tess.greyscale;
			pixelRC.hFrameSeed = f32tof16(grp.frameSeed);
			pixelRC.hNoiseScale = f32tof16(r_ditherStrength->value);
			pixelRC.hInvGamma = f32tof16(1.0f / r_gamma->value);
			pixelRC.hInvBrightness = f32tof16(1.0f / r_brightness->value);
			pixelRC.shaderTrace = (uint32_t)!!tr.traceWorldShader | (RHI::GetFrameIndex() << 1) | ((uint32_t)shader->index << 3);
			pixelRC.centerPixelX = glConfig.vidWidth / 2;
			pixelRC.centerPixelY = glConfig.vidHeight / 2;
			pixelRC.hFadeDistance = f32tof16(tess.shader->dfInvDist);
			pixelRC.hFadeOffset = f32tof16(tess.shader->dfBias);
			pixelRC.depthFadeColorTex = (uint32_t)r_depthFadeScaleAndBias[tess.shader->dfType] | (depthTextureIndex << 8);

			for(int s = 0; s < pipeline.numStages; ++s)
			{
				const image_t* image = GetBundleImage(shader->stages[pipeline.firstStage + s]->bundle);
				const uint32_t texIdx = image->textureIndex;
				const uint32_t sampIdx = GetSamplerIndex(image);
				Q_assert(texIdx > 0);
				pixelRC.stageIndices[s] = (sampIdx << 16) | (texIdx);
			}
			CmdSetRootConstants(grp.uberRootSignature, ShaderStage::Pixel, &pixelRC);

			BindVertexBuffers(batchHasStaticGeo, pipeline.firstStage, pipeline.numStages);
			CmdDrawIndexed(indexCount, db.indexBuffer.batchFirst, batchHasStaticGeo ? 0 : db.vertexBuffers.batchFirst);
		}
	}

	if(!batchHasStaticGeo)
	{
		db.vertexBuffers.EndBatch(tess.numVertexes);
	}
	db.indexBuffer.EndBatch(tess.numIndexes);

clean_up:
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}

void World::EndSkyBatch()
{
	// this only exists as a separate function from EndBatch so that
	// we don't have to deal with recursion (through the call to RB_DrawSky)

	if(tess.shader == NULL ||
		!tess.shader->isSky ||
		(!drawSky && !drawClouds) ||
		tess.numVertexes <= 0 ||
		tess.numIndexes <= 0)
	{
		return;
	}

	Q_assert(batchHasStaticGeo == false);

	SCOPED_RENDER_PASS("Sky", 0.0, 0.5f, 1.0f);

	CmdSetShadingRate(ShadingRate::SR_1x1);

	const viewParms_t& vp = backEnd.viewParms;
	CmdSetViewport(vp.viewportX, vp.viewportY, vp.viewportWidth, vp.viewportHeight, 0.0f, 0.0f);
	RB_DrawSky();
	CmdSetViewport(vp.viewportX, vp.viewportY, vp.viewportWidth, vp.viewportHeight, 0.0f, 1.0f);
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	batchOldDepthHack = false;
	batchDepthHack = false;
	batchOldShadingRate = ShadingRate::SR_1x1;
	batchShadingRate = ShadingRate::SR_1x1;
}

void World::RestartBatch()
{
	EndBatch();
	BeginBatch(tess.shader, batchHasStaticGeo, batchType);
}

void World::DrawGUI()
{
#if defined(ENABLE_GUI)
	if(tr.world == NULL)
	{
		return;
	}

#if 0
	TextureBarrier tb(depthTexture, ResourceStates::DepthReadBit);
	CmdBarrier(1, &tb);
	if(ImGui::Begin("depth", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Image((ImTextureID)depthTextureIndex, ImVec2(640, 360));
	}
	ImGui::End();
#endif

	static bool active = false;
	ToggleBooleanWithShortcut(active, ImGuiKey_W);
	GUI_AddMainMenuItem(GUI_MainMenu::Info, "World rendering", "Ctrl+W", &active);
	if(active)
	{
		if(ImGui::Begin("World", &active, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Checkbox("Depth Pre-Pass", &drawPrePass);
			ImGui::SliderInt("Pre-pass Max Triangles", &zppMaxTriangleCount, 1, 256);
			ImGui::SliderFloat("Pre-pass Min Radius", &zppMinRadiusOverZ, 1.0f / 256.0f, 256.0f / 256.0f);
			ImGui::Checkbox("Draw Dynamic", &drawDynamic);
			ImGui::Checkbox("Draw Transparents", &drawTransparents);
			ImGui::Checkbox("Draw Fog", &drawFog);
			ImGui::Checkbox("Draw Sky", &drawSky);
			ImGui::Checkbox("Draw Clouds", &drawClouds);
			ImGui::Checkbox("Force Dynamic", &forceDynamic);

			ImGui::Text("PSO count: %d", (int)grp.psoCount);
			ImGui::Text("PSO changes: %d", psoChangeCount);
			ImGui::Text("ZPP triangles: %d", zppDrawnTriangleCount);

#if 0
			vec3_t axis[3];
			R_CameraAxisVectorsFromMatrix(axis[0], axis[1], axis[2], backEnd.viewParms.world.modelMatrix);
			const char* axisNames[] = { "X", "Y", "Z", };
			for(int a = 0; a < 3; ++a)
			{
				ImGui::Text("%s: %s", axisNames[a], v3tos(axis[a]));
			}

			vec3_t cameraPos;
			R_CameraPositionFromMatrix(cameraPos, backEnd.viewParms.world.modelMatrix);
			ImGui::Text("Camera Position: %s", v3tos(cameraPos));

			for(int f = 1; f < tr.world->numfogs; ++f)
			{
				ImGui::Text("Fog: %s", v4tos(tr.world->fogs[f].surface));
			}
#endif
		}
		ImGui::End();
	}
#endif
}

void World::ProcessWorld(world_t& world)
{
	{
		zppVertexBuffer.batchFirst = 0;
		zppVertexBuffer.batchCount = 0;

		float* vtx = (float*)BeginBufferUpload(zppVertexBuffer.buffer);

		int firstVertex = 0;
		int firstIndex = 0;
		for(int s = 0; s < world.numsurfaces; ++s)
		{
			msurface_t* const surf = &world.surfaces[s];
			if(surf->shader->numStages == 0 ||
				surf->shader->isDynamic ||
				!surf->shader->isOpaque ||
				surf->shader->isAlphaTestedOpaque ||
				surf->shader->cullType != CT_FRONT_SIDED)
			{
				continue;
			}

			tess.numVertexes = 0;
			tess.numIndexes = 0;
			tess.shader = surf->shader; // unsure if needed, but just in case
			R_TessellateSurface(surf->data);
			const int surfVertexCount = tess.numVertexes;
			const int surfIndexCount = tess.numIndexes;
			if(surfVertexCount <= 0 || surfIndexCount <= 0)
			{
				continue;
			}
			if(firstVertex + surfVertexCount >= zppVertexBuffer.totalCount)
			{
				break;
			}

			for(int v = 0; v < surfVertexCount; ++v)
			{
				*vtx++ = tess.xyz[v][0];
				*vtx++ = tess.xyz[v][1];
				*vtx++ = tess.xyz[v][2];
			}

			uint32_t* idx = zppIndices + firstIndex;
			for(int i = 0; i < surfIndexCount; ++i)
			{
				*idx++ = tess.indexes[i] + firstVertex;
			}

			surf->zppFirstIndex = firstIndex;
			surf->zppIndexCount = surfIndexCount;
			firstVertex += surfVertexCount;
			firstIndex += surfIndexCount;
			tess.numVertexes = 0;
			tess.numIndexes = 0;
		}

		EndBufferUpload(zppVertexBuffer.buffer);

		zppVertexBuffer.batchCount = firstVertex;
		zppVertexBuffer.batchFirst = 0;
	}

	statChunkCount = 1; // index 0 is invalid
	statIndexCount = 0;

	statBuffers.vertexBuffers.Rewind();
	statBuffers.indexBuffer.Rewind();

	statBuffers.vertexBuffers.BeginUpload();
	statBuffers.indexBuffer.BeginUpload();

	for(int s = 0; s < world.numsurfaces; ++s)
	{
		if(statChunkCount >= ARRAY_LEN(statChunks))
		{
			Q_assert(0);
			break;
		}

		msurface_t* const surf = &world.surfaces[s];
		surf->staticGeoChunk = 0;

		if(surf->shader->numStages == 0 ||
			surf->shader->isDynamic)
		{
			continue;
		}

		tess.numVertexes = 0;
		tess.numIndexes = 0;
		tess.shader = surf->shader; // needed by R_ComputeTexCoords at least
		R_TessellateSurface(surf->data);
		const int surfVertexCount = tess.numVertexes;
		const int surfIndexCount = tess.numIndexes;
		if(surfVertexCount <= 0 || surfIndexCount <= 0)
		{
			continue;
		}
		
		if(!statBuffers.vertexBuffers.CanAdd(surfVertexCount) ||
			!statBuffers.indexBuffer.CanAdd(surfIndexCount) ||
			statIndexCount + surfIndexCount > ARRAY_LEN(statIndices))
		{
			Q_assert(0);
			break;
		}

		for(int i = 0; i < surf->shader->numStages; ++i)
		{
			const shaderStage_t* const stage = surf->shader->stages[i];
			R_ComputeColors(stage, tess.svars[i], 0, tess.numVertexes);
			R_ComputeTexCoords(stage, tess.svars[i], 0, tess.numVertexes, qfalse);
		}

		// update GPU buffers
		statBuffers.vertexBuffers.Upload(0, surf->shader->numStages);
		statBuffers.indexBuffer.Upload();

		// update CPU buffer
		const uint32_t firstGPUVertex = statBuffers.vertexBuffers.batchFirst;
		const uint32_t firstCPUIndex = statIndexCount;
		uint32_t* cpuIndices = statIndices + firstCPUIndex;
		for(int i = 0; i < surfIndexCount; ++i)
		{
			cpuIndices[i] = tess.indexes[i] + firstGPUVertex;
		}
		statIndexCount += surfIndexCount;

		const uint32_t chunkIndex = statChunkCount++;
		StaticGeometryChunk& chunk = statChunks[chunkIndex];
		chunk.vertexCount = surfVertexCount;
		chunk.indexCount = surfIndexCount;
		chunk.firstGPUVertex = statBuffers.vertexBuffers.batchFirst;
		chunk.firstGPUIndex = statBuffers.indexBuffer.batchFirst;
		chunk.firstCPUIndex = firstCPUIndex;
		surf->staticGeoChunk = chunkIndex;

		statBuffers.vertexBuffers.EndBatch(surfVertexCount);
		statBuffers.indexBuffer.EndBatch(surfIndexCount);
	}

	statBuffers.vertexBuffers.EndUpload();
	statBuffers.indexBuffer.EndUpload();

	tess.numVertexes = 0;
	tess.numIndexes = 0;
}

void World::DrawSceneView(const drawSceneViewCommand_t& cmd)
{
	if(cmd.shouldClearColor)
	{
		const viewParms_t& vp = cmd.viewParms;
		const Rect rect(vp.viewportX, vp.viewportY, vp.viewportWidth, vp.viewportHeight);
		CmdClearColorTarget(grp.renderTarget, cmd.clearColor, &rect);
	}

	if(cmd.numDrawSurfs <= 0 || !cmd.shouldDrawScene)
	{
		return;
	}

	backEnd.refdef = cmd.refdef;
	backEnd.viewParms = cmd.viewParms;

	Begin();

	CmdClearDepthStencilTarget(depthTexture, true, 0.0f);

	boundVertexBuffers = BufferFamily::Invalid;
	boundIndexBuffer = BufferFamily::Invalid;

	// the index buffer is also used for the Z pre-pass
	GeometryBuffers& db = dynBuffers[GetFrameIndex()];
	db.vertexBuffers.BeginUpload();
	db.indexBuffer.BeginUpload();

	// portals get the chance to write to depth and color before everyone else
	{
		const shader_t* shader = NULL;
		int entityNum;
		R_DecomposeSort(cmd.drawSurfs->shaderSort, &entityNum, &shader);
		if(shader->sort != SS_PORTAL)
		{
			DrawPrePass(cmd);
		}
	}

	SCOPED_RENDER_PASS("3D Scene View", 1.0f, 0.5f, 0.5f);

	CmdBindRootSignature(grp.uberRootSignature);
	CmdBindDescriptorTable(grp.uberRootSignature, grp.descriptorTable);
	CmdBindRenderTargets(1, &grp.renderTarget, &depthTexture);
	batchPSO = RHI_MAKE_NULL_HANDLE();
	batchType = BatchType::Standard;

	const drawSurf_t* drawSurfs = cmd.drawSurfs;
	const int surfCount = cmd.numDrawSurfs;
	const int opaqueCount = cmd.numDrawSurfs - cmd.numTranspSurfs;
	const int transpCount = cmd.numTranspSurfs;
	const double originalTime = backEnd.refdef.floatTime;

	const shader_t* shader = NULL;
	const shader_t* oldShader = NULL;
	int oldEntityNum = -1;
	bool oldHasStaticGeo = false;
	bool forceEntityChange = false;
	backEnd.currentEntity = &tr.worldEntity;

	ShadingRate::Id lowShadingRate = (ShadingRate::Id)r_shadingRate->integer;
	batchShadingRate = ShadingRate::SR_1x1;
	batchOldShadingRate = ShadingRate::SR_1x1;

	tess.numVertexes = 0;
	tess.numIndexes = 0;

	int ds;
	const drawSurf_t* drawSurf;
	for(ds = 0, drawSurf = drawSurfs; ds < surfCount; ++ds, ++drawSurf)
	{
		if(ds == opaqueCount)
		{
			EndSkyBatch();
			EndBatch();

			CmdSetShadingRate(lowShadingRate);
			batchOldShadingRate = lowShadingRate;
			DrawDynamicLights(qtrue);
			DrawFog();

			// needed after DrawDynamicLights
			forceEntityChange = true;

			CmdBindRootSignature(grp.uberRootSignature);
			CmdBindDescriptorTable(grp.uberRootSignature, grp.descriptorTable);
			CmdBindRenderTargets(1, &grp.renderTarget, &depthTexture);
			batchPSO = RHI_MAKE_NULL_HANDLE();
			batchType = BatchType::Standard;
			boundVertexBuffers = BufferFamily::Invalid;
			boundIndexBuffer = BufferFamily::Invalid;

			const TextureBarrier depthWriteBarrier(depthTexture, ResourceStates::DepthWriteBit);
			CmdBarrier(1, &depthWriteBarrier);
		}

		int entityNum;
		R_DecomposeSort(drawSurf->sort, &entityNum, &shader);

		// sky shaders can have no stages and be valid (box drawn with no clouds)
		if(!shader->isSky)
		{
			if(shader->numPipelines == 0 ||
				shader->pipelines[0].pipeline <= 0 ||
				shader->pipelines[0].numStages <= 0)
			{
				continue;
			}
		}

		const bool hasStaticGeo = HasStaticGeo(drawSurf->staticGeoChunk, shader);
		const bool staticChanged = hasStaticGeo != oldHasStaticGeo;
		const bool shaderChanged = shader != oldShader;
		bool entityChanged = entityNum != oldEntityNum;
		Q_assert(shader != NULL);
		if(!hasStaticGeo && !drawDynamic)
		{
			continue;
		}
		if(ds >= opaqueCount && !drawTransparents)
		{
			continue;
		}

		const BatchType::Id type = IsDepthFadeEnabled(*shader) ? BatchType::DepthFade : BatchType::Standard;
		if(staticChanged || shaderChanged || entityChanged)
		{
			oldShader = shader;
			oldEntityNum = entityNum;
			oldHasStaticGeo = hasStaticGeo;
			EndSkyBatch();
			EndBatch();
			BeginBatch(shader, hasStaticGeo, type);
			tess.greyscale = drawSurf->greyscale;
			batchShadingRate = lowShadingRate;
		}

		if(entityChanged || forceEntityChange)
		{
			UpdateEntityData(batchDepthHack, entityNum, originalTime);
			forceEntityChange = false;
		}

		if(hasStaticGeo)
		{
			const StaticGeometryChunk& chunk = statChunks[drawSurf->staticGeoChunk];
			if(tess.numIndexes + chunk.indexCount > SHADER_MAX_INDEXES)
			{
				EndBatch();
				BeginBatch(tess.shader, batchHasStaticGeo, type);
				batchShadingRate = lowShadingRate;
			}

			memcpy(tess.indexes + tess.numIndexes, statIndices + chunk.firstCPUIndex, chunk.indexCount * sizeof(uint32_t));
			tess.numIndexes += chunk.indexCount;
		}
		else
		{
			R_TessellateSurface(drawSurf->surface);
		}

		// we want full rate for nopicmipped sprites and nopicmipped alpha tests
		if((shader->imgflags & IMG_NOPICMIP) != 0)
		{
			if(entityNum != ENTITYNUM_WORLD && tr.refdef.entities[entityNum].e.reType == RT_SPRITE)
			{
				batchShadingRate = ShadingRate::SR_1x1;
			}
			else if(shader->isAlphaTestedOpaque)
			{
				batchShadingRate = ShadingRate::SR_1x1;
			}
		}
	}

	backEnd.refdef.floatTime = originalTime;

	EndSkyBatch();
	EndBatch();

	if(transpCount <= 0)
	{
		DrawDynamicLights(qtrue);
		CmdSetShadingRate(lowShadingRate);
		batchOldShadingRate = lowShadingRate;
		DrawFog();
	}
	else
	{
		DrawDynamicLights(qfalse);
	}

	db.vertexBuffers.EndUpload();
	db.indexBuffer.EndUpload();

	// restores the potentially "hacked" depth range as well
	CmdSetViewportAndScissor(backEnd.viewParms);
	batchOldDepthHack = false;
	batchDepthHack = false;

	CmdSetShadingRate(ShadingRate::SR_1x1);
	batchOldShadingRate = ShadingRate::SR_1x1;
	batchShadingRate = ShadingRate::SR_1x1;
}

void World::BindVertexBuffers(bool staticGeo, uint32_t firstStage, uint32_t stageCount)
{
	const BufferFamily::Id type = staticGeo ? BufferFamily::Static : BufferFamily::Dynamic;
	if(type == boundVertexBuffers &&
		firstStage == boundStaticVertexBuffersFirst &&
		stageCount == boundStaticVertexBuffersCount)
	{
		return;
	}

	VertexBuffers& vb = staticGeo ? statBuffers.vertexBuffers : dynBuffers[GetFrameIndex()].vertexBuffers;
	const uint32_t count = VertexBuffers::BaseCount + VertexBuffers::StageCount * stageCount;
	HBuffer buffers[VertexBuffers::BufferCount];
	memcpy(buffers, vb.buffers, VertexBuffers::BaseCount * sizeof(HBuffer));
	memcpy(
		buffers + VertexBuffers::BaseCount,
		vb.buffers + VertexBuffers::BaseCount + VertexBuffers::StageCount * firstStage,
		VertexBuffers::StageCount * stageCount * sizeof(HBuffer));
	CmdBindVertexBuffers(count, buffers, vb.strides, NULL);
	boundVertexBuffers = type;
	boundStaticVertexBuffersFirst = firstStage;
	boundStaticVertexBuffersCount = stageCount;
}

void World::BindIndexBuffer(bool staticGeo)
{
	const BufferFamily::Id type = staticGeo ? BufferFamily::Static : BufferFamily::Dynamic;
	if(type == boundIndexBuffer)
	{
		return;
	}

	IndexBuffer& ib = staticGeo ? statBuffers.indexBuffer : dynBuffers[GetFrameIndex()].indexBuffer;
	CmdBindIndexBuffer(ib.buffer, indexType, 0);
	boundIndexBuffer = type;
}

void World::DrawFog()
{
	// fog 0 is invalid
	if(!drawFog ||
		tr.world == NULL ||
		tr.world->numfogs <= 1)
	{
		return;
	}

	SCOPED_RENDER_PASS("Fog", 0.25f, 0.125f, 0.0f);

	// fog 0 is invalid, it must be skipped
	int insideIndex = -1;
	if(tr.world->numfogs > 1)
	{
		CmdBindPipeline(fogOutsidePipeline);
		psoChangeCount++;
		CmdBindRootSignature(fogRootSignature);
		CmdBindDescriptorTable(fogRootSignature, fogDescriptorTable);

		const uint32_t stride = sizeof(vec3_t);
		CmdBindVertexBuffers(1, &boxVertexBuffer, &stride, NULL);
		CmdBindIndexBuffer(boxIndexBuffer, IndexType::UInt32, 0);

		CmdBindRenderTargets(1, &grp.renderTarget, NULL);
		const TextureBarrier depthReadBarrier(depthTexture, ResourceStates::PixelShaderAccessBit);
		CmdBarrier(1, &depthReadBarrier);

		for(int f = 1; f < tr.world->numfogs; ++f)
		{
			const fog_t& fog = tr.world->fogs[f];

			bool inside = true;
			for(int a = 0; a < 3; ++a)
			{
				if(backEnd.viewParms.orient.origin[a] <= fog.bounds[0][a] ||
					backEnd.viewParms.orient.origin[a] >= fog.bounds[1][a])
				{
					inside = false;
					break;
				}
			}

			if(inside)
			{
				insideIndex = f;
				break;
			}
		}
	}

	for(int f = 1; f < tr.world->numfogs; ++f)
	{
		if(f == insideIndex)
		{
			continue;
		}

		const fog_t& fog = tr.world->fogs[f];

		FogVertexRC vertexRC = {};
		memcpy(vertexRC.modelViewMatrix, backEnd.viewParms.world.modelMatrix, sizeof(vertexRC.modelViewMatrix));
		memcpy(vertexRC.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(vertexRC.projectionMatrix));
		VectorCopy(fog.bounds[0], vertexRC.boxMin);
		VectorCopy(fog.bounds[1], vertexRC.boxMax);
		CmdSetRootConstants(fogRootSignature, ShaderStage::Vertex, &vertexRC);

		FogPixelRC pixelRC = {};
		VectorScale(fog.parms.color, tr.identityLight, pixelRC.colorDepth);
		pixelRC.colorDepth[3] = fog.parms.depthForOpaque;
		CmdSetRootConstants(fogRootSignature, ShaderStage::Pixel, &pixelRC);

		CmdDrawIndexed(36, 0, 0);
	}

	if(insideIndex > 0)
	{
		CmdBindPipeline(fogInsidePipeline);
		psoChangeCount++;
		CmdBindRootSignature(fogRootSignature);
		CmdBindDescriptorTable(fogRootSignature, fogDescriptorTable);

		const fog_t& fog = tr.world->fogs[insideIndex];

		FogVertexRC vertexRC = {};
		memcpy(vertexRC.modelViewMatrix, backEnd.viewParms.world.modelMatrix, sizeof(vertexRC.modelViewMatrix));
		memcpy(vertexRC.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(vertexRC.projectionMatrix));
		VectorCopy(fog.bounds[0], vertexRC.boxMin);
		VectorCopy(fog.bounds[1], vertexRC.boxMax);
		CmdSetRootConstants(fogRootSignature, ShaderStage::Vertex, &vertexRC);

		FogPixelRC pixelRC = {};
		VectorScale(fog.parms.color, tr.identityLight, pixelRC.colorDepth);
		pixelRC.colorDepth[3] = fog.parms.depthForOpaque;
		CmdSetRootConstants(fogRootSignature, ShaderStage::Pixel, &pixelRC);

		CmdDrawIndexed(36, 0, 0);
	}
}

void World::DrawLitSurfaces(dlight_t* dl, bool opaque)
{
	if(dl->head == NULL)
	{
		return;
	}

	if(!opaque && !drawTransparents)
	{
		return;
	}

	const shader_t* shader = NULL;
	const shader_t* oldShader = NULL;
	int oldEntityNum = -666;
	bool oldHasStaticGeo = false;

	// save original time for entity shader offsets
	const double originalTime = backEnd.refdef.floatTime;

	// draw everything
	const int liquidFlags = CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER;
	oldEntityNum = -1;
	dlOpaque = opaque;
	backEnd.currentEntity = &tr.worldEntity;
	tess.light = dl;
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	for(litSurf_t* litSurf = dl->head; litSurf != NULL; litSurf = litSurf->next)
	{
		backEnd.pc3D[RB_LIT_SURFACES]++;

		int entityNum;
		R_DecomposeLitSort(litSurf->sort, &entityNum, &shader);

		if(opaque && shader->sort > SS_OPAQUE)
		{
			continue;
		}
		if(!opaque && shader->sort <= SS_OPAQUE)
		{
			continue;
		}

		const int stageIndex = shader->lightingStages[ST_DIFFUSE];
		if(stageIndex < 0 || stageIndex >= shader->numStages)
		{
			continue;
		}

		const bool hasStaticGeo = HasStaticGeo(litSurf->staticGeoChunk, shader);
		const bool staticChanged = hasStaticGeo != oldHasStaticGeo;
		const bool shaderChanged = shader != oldShader;
		bool entityChanged = entityNum != oldEntityNum;
		Q_assert(shader != NULL);
		if(!hasStaticGeo && !drawDynamic)
		{
			continue;
		}

		if(staticChanged || shaderChanged || entityChanged)
		{
			oldShader = shader;
			oldEntityNum = entityNum;
			oldHasStaticGeo = hasStaticGeo;
			EndBatch();
			BeginBatch(shader, hasStaticGeo, BatchType::DynamicLight);
			tess.greyscale = litSurf->greyscale;

			const shaderStage_t* const stage = shader->stages[stageIndex];
			dlIntensity = (shader->contentFlags & liquidFlags) != 0 ? 0.5f : 1.0f;
			dlDepthTestEqual = opaque || (stage->stateBits & GLS_ATEST_BITS) != 0;
		}

		if(entityChanged)
		{
			UpdateEntityData(batchDepthHack, entityNum, originalTime);
		}

		R_TransformDlights(1, dl, &backEnd.orient);

		if(hasStaticGeo)
		{
			const StaticGeometryChunk& chunk = statChunks[litSurf->staticGeoChunk];
			if(tess.numIndexes + chunk.indexCount > SHADER_MAX_INDEXES)
			{
				EndBatch();
				BeginBatch(tess.shader, batchHasStaticGeo, BatchType::DynamicLight);
			}

			memcpy(tess.indexes + tess.numIndexes, statIndices + chunk.firstCPUIndex, chunk.indexCount * sizeof(uint32_t));
			tess.numIndexes += chunk.indexCount;
			backEnd.pc3D[RB_LIT_INDICES_LATECULL_IN] += chunk.indexCount;
		}
		else
		{
			const int oldNumIndexes = tess.numIndexes;
			R_TessellateSurface(litSurf->surface);
			backEnd.pc3D[RB_LIT_INDICES_LATECULL_IN] += tess.numIndexes - oldNumIndexes;
		}
	}

	// draw the contents of the last shader batch
	if(shader != NULL)
	{
		EndBatch();
	}

	backEnd.refdef.floatTime = originalTime;
}

void World::DrawDynamicLights(bool opaque)
{
	if(r_dynamiclight->integer == 0 ||
		backEnd.viewParms.isMirror)
	{
		return;
	}

	SCOPED_RENDER_PASS(opaque ? "DL Opaque" : "DL Transp.", 1.0f, 0.25f, 0.25f);

	if(backEnd.refdef.num_dlights == 0)
	{
		return;
	}

	CmdBindRootSignature(dlRootSignature);
	CmdBindDescriptorTable(dlRootSignature, grp.descriptorTable);
	CmdBindRenderTargets(1, &grp.renderTarget, &depthTexture);
	batchPSO = RHI_MAKE_NULL_HANDLE();
	boundVertexBuffers = BufferFamily::Invalid;
	boundIndexBuffer = BufferFamily::Invalid;

	ShadingRate::Id lowShadingRate = (ShadingRate::Id)r_shadingRate->integer;
	batchShadingRate = lowShadingRate;
	batchOldShadingRate = ShadingRate::SR_1x1;
	CmdSetShadingRate(lowShadingRate);

	CmdSetViewportAndScissor(backEnd.viewParms);
	batchOldDepthHack = false;
	batchDepthHack = false;

	tess.numVertexes = 0;
	tess.numIndexes = 0;

	for(int i = 0; i < backEnd.refdef.num_dlights; ++i)
	{
		DrawLitSurfaces(&backEnd.refdef.dlights[i], opaque);
	}
}

void World::DrawSkyBox()
{
	if(!drawSky)
	{
		tess.numVertexes = 0;
		tess.numIndexes = 0;
		return;
	}

	// force creation of a PSO for the temp shader
	grp.ProcessShader((shader_t&)*tess.shader);

	tess.deformsPreApplied = qtrue;
	Q_assert(batchHasStaticGeo == false);
	EndBatch();
}

void World::DrawClouds()
{
	if(!drawClouds)
	{
		tess.numVertexes = 0;
		tess.numIndexes = 0;
		return;
	}

	Q_assert(batchHasStaticGeo == false);
	EndBatch();
}
