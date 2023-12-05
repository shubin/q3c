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
// Gameplay Rendering Pipeline - Nuklear integration


#include "grp_local.h"
#include "hlsl/nuklear_vs.h"
#include "hlsl/nuklear_ps.h"


#define MAX_NUKLEAR_VERTEX_COUNT    (1024 * 1024)
#define MAX_NUKLEAR_INDEX_COUNT     ( 256 * 1024)


#pragma pack(push, 4)
struct VertexRC
{
	float mvp[16];
};

struct PixelRC
{
	uint32_t texture;
	uint32_t sampler;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NuklearVertex
{
	float position[2];
	float texCoords[2];
	byte color[4];
};
#pragma pack(pop)


void Nuklear::Init()
{
	if(grp.firstInit)
	{
		for(int i = 0; i < FrameCount; i++)
		{
			FrameResources* fr = &frameResources[i];

			BufferDesc vtx("Nuklear index", MAX_NUKLEAR_INDEX_COUNT * sizeof(uint32_t), ResourceStates::IndexBufferBit);
			vtx.memoryUsage = MemoryUsage::Upload;
			fr->indexBuffer = CreateBuffer(vtx);

			BufferDesc idx("Nuklear vertex", MAX_NUKLEAR_VERTEX_COUNT * sizeof(NuklearVertex), ResourceStates::VertexBufferBit);
			idx.memoryUsage = MemoryUsage::Upload;
			fr->vertexBuffer = CreateBuffer(idx);
		}

		{
			RootSignatureDesc desc = grp.rootSignatureDesc;
			desc.name = "Nuklear";
			desc.constants[ShaderStage::Vertex].byteCount = sizeof(VertexRC);
			desc.constants[ShaderStage::Pixel].byteCount = sizeof(PixelRC);
			rootSignature = CreateRootSignature(desc);
		}
	}

	{
		GraphicsPipelineDesc desc("Nuklear", rootSignature);
		desc.shortLifeTime = true;
		desc.vertexShader = ShaderByteCode(g_vs);
		desc.pixelShader = ShaderByteCode(g_ps);
		desc.vertexLayout.bindingStrides[0] = sizeof(NuklearVertex);
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Position,
			DataType::Float32, 2, offsetof(NuklearVertex, position));
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::TexCoord,
			DataType::Float32, 2, offsetof(NuklearVertex, texCoords));
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Color,
			DataType::UNorm8, 4, offsetof(NuklearVertex, color));
		desc.depthStencil.depthComparison = ComparisonFunction::Always;
		desc.depthStencil.depthStencilFormat = TextureFormat::Depth32_Float;
		desc.depthStencil.enableDepthTest = false;
		desc.depthStencil.enableDepthWrites = false;
		desc.rasterizer.cullMode = CT_TWO_SIDED;
		desc.AddRenderTarget(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, grp.renderTargetFormat);
		pipeline = CreateGraphicsPipeline(desc);
	}
}

void Nuklear::BeginFrame()
{
	firstVertex = 0;
	firstIndex = 0;
	numVertexes = 0;
	numIndexes = 0;
}

void Nuklear::Begin()
{
	if(grp.renderMode == RenderMode::Nuklear)
	{
		return;
	}

	grp.renderMode = RenderMode::Nuklear;

	renderPassIndex = grp.BeginRenderPass("Nuklear", 0.75f, 0.75f, 1.0f);

	FrameResources* const fr = &frameResources[GetFrameIndex()];

	const uint32_t vertexStride = sizeof(NuklearVertex);
	CmdBindRenderTargets(1, &grp.renderTarget, NULL);
	CmdBindRootSignature(rootSignature);
	CmdBindPipeline(pipeline);
	CmdBindDescriptorTable(rootSignature, grp.descriptorTable);
	CmdBindVertexBuffers(1, &fr->vertexBuffer, &vertexStride, NULL);
	CmdBindIndexBuffer(fr->indexBuffer, IndexType::UInt32, 0);
	CmdSetViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);

	const float L = 0.0f;
	const float R = glConfig.vidWidth;
	const float T = 0.0f;
	const float B = glConfig.vidHeight;
	const VertexRC vertexRC =
	{
		2.0f / (R - L),    0.0f,              0.0f, 0.0f,
		0.0f,              2.0f / (T - B),    0.0f, 0.0f,
		0.0f,              0.0f,              0.5f, 0.0f,
		(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f
	};
	CmdSetRootConstants(rootSignature, ShaderStage::Vertex, &vertexRC);

	for(int i = 0; i < 4; ++i)
	{
		prevScissorRect[i] = 0;
	}
}

void Nuklear::End()
{
	grp.EndRenderPass(renderPassIndex);

	grp.renderMode = RenderMode::None;
}

void Nuklear::Upload(const nuklearUploadCommand_t& cmd)
{
	firstVertex += numVertexes;
	firstIndex += numIndexes;
	numVertexes = cmd.numVertexBytes / sizeof(NuklearVertex);
	numIndexes = cmd.numIndexBytes / sizeof(uint32_t);
	if(firstVertex + numVertexes > MAX_NUKLEAR_VERTEX_COUNT ||
		firstIndex + numIndexes > MAX_NUKLEAR_INDEX_COUNT)
	{
		return;
	}

	FrameResources* const fr = &frameResources[GetFrameIndex()];
	NuklearVertex* const vtxDst = (NuklearVertex*)MapBuffer(fr->vertexBuffer) + firstVertex;
	uint32_t* const idxDst = (uint32_t*)MapBuffer(fr->indexBuffer) + firstIndex;
	memcpy(vtxDst, cmd.vertexes, cmd.numVertexBytes);
	memcpy(idxDst, cmd.indexes, cmd.numIndexBytes);
	UnmapBuffer(fr->vertexBuffer);
	UnmapBuffer(fr->indexBuffer);
}

void Nuklear::Draw(const nuklearDrawCommand_t& cmd)
{
	const shader_t* const shader = R_GetShaderByHandle(cmd.shader);
	if(shader == NULL ||
		shader->numStages < 1 ||
		shader->stages[0] == NULL ||
		!shader->stages[0]->active ||
		shader->stages[0]->bundle.image[0] == NULL)
	{
		Q_assert(!"Invalid shader!");
		return;
	}

	const image_t* const image = R_GetShaderByHandle(cmd.shader)->stages[0]->bundle.image[0];

	PixelRC pixelRC = {};
	pixelRC.texture = (uint32_t)image->textureIndex;
	pixelRC.sampler = GetSamplerIndex(image->wrapClampMode, TextureFilter::Linear);
	CmdSetRootConstants(rootSignature, ShaderStage::Pixel, &pixelRC);

	if(memcmp(cmd.scissorRect, prevScissorRect, sizeof(prevScissorRect)) != 0)
	{
		CmdSetScissor(cmd.scissorRect[0], cmd.scissorRect[1], cmd.scissorRect[2], cmd.scissorRect[3]);
		memcpy(prevScissorRect, cmd.scissorRect, sizeof(prevScissorRect));
	}
	CmdDrawIndexed(cmd.numIndexes, firstIndex + cmd.firstIndex, firstVertex);
}
