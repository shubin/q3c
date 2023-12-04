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
// Gameplay Rendering Pipeline - UI/2D rendering


#include "grp_local.h"
#include "hlsl/ui_vs.h"
#include "hlsl/ui_ps.h"


#pragma pack(push, 4)

struct VertexRC
{
	float scale[2];
};

struct PixelRC
{
	uint32_t texture;
	uint32_t sampler;
};

#pragma pack(pop)


void UI::Init()
{
	if(grp.firstInit)
	{
		{
			RootSignatureDesc desc = grp.rootSignatureDesc;
			desc.name = "UI";
			desc.constants[ShaderStage::Vertex].byteCount = 8;
			desc.constants[ShaderStage::Pixel].byteCount = 8;
			rootSignature = CreateRootSignature(desc);
		}
		maxVertexCount = 640 << 10;
		maxIndexCount = 8 * maxVertexCount;
		{
			BufferDesc desc("UI index", sizeof(UI::Index) * maxIndexCount * FrameCount, ResourceStates::IndexBufferBit);
			desc.memoryUsage = MemoryUsage::Upload;
			indexBuffer = CreateBuffer(desc);
			indices = (UI::Index*)MapBuffer(indexBuffer);
		}
		{
			BufferDesc desc("UI vertex", sizeof(UI::Vertex) * maxVertexCount * FrameCount, ResourceStates::VertexBufferBit);
			desc.memoryUsage = MemoryUsage::Upload;
			vertexBuffer = CreateBuffer(desc);
			vertices = (UI::Vertex*)MapBuffer(vertexBuffer);
		}
	}
	{
		GraphicsPipelineDesc desc("UI", rootSignature);
		desc.shortLifeTime = true;
		desc.vertexShader = ShaderByteCode(g_vs);
		desc.pixelShader = ShaderByteCode(g_ps);
		desc.vertexLayout.bindingStrides[0] = sizeof(UI::Vertex);
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Position,
			DataType::Float32, 2, offsetof(UI::Vertex, position));
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::TexCoord,
			DataType::Float32, 2, offsetof(UI::Vertex, texCoords));
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Color,
			DataType::UNorm8, 4, offsetof(UI::Vertex, color));
		desc.depthStencil.depthComparison = ComparisonFunction::Always;
		desc.depthStencil.depthStencilFormat = TextureFormat::Depth32_Float;
		desc.depthStencil.enableDepthTest = false;
		desc.depthStencil.enableDepthWrites = false;
		desc.rasterizer.cullMode = CT_TWO_SIDED;
		desc.AddRenderTarget(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, grp.renderTargetFormat);
		pipeline = CreateGraphicsPipeline(desc);
	}
}

void UI::BeginFrame()
{
	// move to this frame's dedicated buffer section
	const uint32_t frameIndex = GetFrameIndex();
	firstIndex = frameIndex * maxIndexCount;
	firstVertex = frameIndex * maxVertexCount;
	renderPassIndex = UINT32_MAX;
}

void UI::Begin()
{
	grp.renderMode = RenderMode::UI;

	renderPassIndex = grp.BeginRenderPass("UI", 0.0f, 0.85f, 1.0f);

	CmdBindRenderTargets(1, &grp.renderTarget, NULL);

	// UI always uses the entire render surface
	CmdSetViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);

	CmdBindRootSignature(rootSignature);
	CmdBindPipeline(pipeline);
	CmdBindDescriptorTable(rootSignature, grp.descriptorTable);
	const uint32_t stride = sizeof(UI::Vertex);
	CmdBindVertexBuffers(1, &vertexBuffer, &stride, NULL);
	CmdBindIndexBuffer(indexBuffer, indexType, 0);

	VertexRC vertexRC = {};
	vertexRC.scale[0] = 2.0f / glConfig.vidWidth;
	vertexRC.scale[1] = 2.0f / glConfig.vidHeight;
	CmdSetRootConstants(rootSignature, ShaderStage::Vertex, &vertexRC);
}

void UI::End()
{
	DrawBatch();

	grp.EndRenderPass(renderPassIndex);

	grp.renderMode = RenderMode::None;
}

void UI::DrawBatch()
{
	if(indexCount <= 0)
	{
		return;
	}

	// @TODO: support for custom shaders?
	Q_assert(shader->stages[0] != NULL);
	const textureBundle_t& bundle = shader->stages[0]->bundle;
	const textureWrap_t wrapMode = bundle.image[0] != NULL ? bundle.image[0]->wrapClampMode : TW_REPEAT;
	PixelRC pixelRC = {};
	pixelRC.texture = GetBundleImage(bundle)->textureIndex;
	pixelRC.sampler = GetSamplerIndex(wrapMode, TextureFilter::Linear);
	CmdSetRootConstants(rootSignature, ShaderStage::Pixel, &pixelRC);

	CmdDrawIndexed(indexCount, firstIndex, 0);
	firstIndex += indexCount;
	firstVertex += vertexCount;
	indexCount = 0;
	vertexCount = 0;
}

void UI::UISetColor(const uiSetColorCommand_t& cmd)
{
	const float rgbScale = tr.identityLight * 255.0f;
	byte* const colors = (byte*)&color;
	colors[0] = (byte)(cmd.color[0] * rgbScale);
	colors[1] = (byte)(cmd.color[1] * rgbScale);
	colors[2] = (byte)(cmd.color[2] * rgbScale);
	colors[3] = (byte)(cmd.color[3] * 255.0f);
}

void UI::UIDrawQuad(const uiDrawQuadCommand_t& cmd)
{
	if(vertexCount + 4 > maxVertexCount ||
		indexCount + 6 > maxIndexCount)
	{
		return;
	}

	if(shader != cmd.shader)
	{
		DrawBatch();
	}

	shader = cmd.shader;

	const int v = firstVertex + vertexCount;
	const int i = firstIndex + indexCount;
	vertexCount += 4;
	indexCount += 6;

	indices[i + 0] = v + 3;
	indices[i + 1] = v + 0;
	indices[i + 2] = v + 2;
	indices[i + 3] = v + 2;
	indices[i + 4] = v + 0;
	indices[i + 5] = v + 1;

	vertices[v + 0].position[0] = cmd.x;
	vertices[v + 0].position[1] = cmd.y;
	vertices[v + 0].texCoords[0] = cmd.s1;
	vertices[v + 0].texCoords[1] = cmd.t1;
	vertices[v + 0].color = color;

	vertices[v + 1].position[0] = cmd.x + cmd.w;
	vertices[v + 1].position[1] = cmd.y;
	vertices[v + 1].texCoords[0] = cmd.s2;
	vertices[v + 1].texCoords[1] = cmd.t1;
	vertices[v + 1].color = color;

	vertices[v + 2].position[0] = cmd.x + cmd.w;
	vertices[v + 2].position[1] = cmd.y + cmd.h;
	vertices[v + 2].texCoords[0] = cmd.s2;
	vertices[v + 2].texCoords[1] = cmd.t2;
	vertices[v + 2].color = color;

	vertices[v + 3].position[0] = cmd.x;
	vertices[v + 3].position[1] = cmd.y + cmd.h;
	vertices[v + 3].texCoords[0] = cmd.s1;
	vertices[v + 3].texCoords[1] = cmd.t2;
	vertices[v + 3].color = color;
}

void UI::UIDrawTriangle(const uiDrawTriangleCommand_t& cmd)
{
	if(vertexCount + 3 > maxVertexCount ||
		indexCount + 3 > maxIndexCount)
	{
		return;
	}

	if(shader != cmd.shader)
	{
		DrawBatch();
	}

	shader = cmd.shader;

	const int v = firstVertex + vertexCount;
	const int i = firstIndex + indexCount;
	vertexCount += 3;
	indexCount += 3;

	indices[i + 0] = v + 0;
	indices[i + 1] = v + 1;
	indices[i + 2] = v + 2;

	vertices[v + 0].position[0] = cmd.x0;
	vertices[v + 0].position[1] = cmd.y0;
	vertices[v + 0].texCoords[0] = cmd.s0;
	vertices[v + 0].texCoords[1] = cmd.t0;
	vertices[v + 0].color = color;

	vertices[v + 1].position[0] = cmd.x1;
	vertices[v + 1].position[1] = cmd.y1;
	vertices[v + 1].texCoords[0] = cmd.s1;
	vertices[v + 1].texCoords[1] = cmd.t1;
	vertices[v + 1].color = color;

	vertices[v + 2].position[0] = cmd.x2;
	vertices[v + 2].position[1] = cmd.y2;
	vertices[v + 2].texCoords[0] = cmd.s2;
	vertices[v + 2].texCoords[1] = cmd.t2;
	vertices[v + 2].color = color;
}
