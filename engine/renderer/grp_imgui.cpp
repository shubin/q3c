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
// Gameplay Rendering Pipeline - Dear ImGUI integration


#include "grp_local.h"
#include "../imgui/imgui.h"
#include "hlsl/imgui_vs.h"
#include "hlsl/imgui_ps.h"


#define MAX_VERTEX_COUNT (64 << 10)
#define MAX_INDEX_COUNT  (MAX_VERTEX_COUNT << 3)


#pragma pack(push, 4)
struct VertexRC
{
	float mvp[16];
};

struct PixelRC
{
	uint32_t texture;
	uint32_t sampler;
	float mip;
};
#pragma pack(pop)


void ImGUI::Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = glConfig.vidWidth;
	io.DisplaySize.y = glConfig.vidHeight;

	if(grp.firstInit)
	{
		io.BackendRendererUserData = this;
		io.BackendRendererName = "CNQ3 Direct3D 12";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

		for(int i = 0; i < FrameCount; i++)
		{
			FrameResources* fr = &frameResources[i];

			BufferDesc vtx("Dear ImGUI index", MAX_INDEX_COUNT * sizeof(ImDrawIdx), ResourceStates::IndexBufferBit);
			vtx.memoryUsage = MemoryUsage::Upload;
			fr->indexBuffer = CreateBuffer(vtx);

			BufferDesc idx("Dear ImGUI vertex", MAX_VERTEX_COUNT * sizeof(ImDrawData), ResourceStates::VertexBufferBit);
			idx.memoryUsage = MemoryUsage::Upload;
			fr->vertexBuffer = CreateBuffer(idx);
		}

		{
			RootSignatureDesc desc = grp.rootSignatureDesc;
			desc.name = "Dear ImGUI";
			desc.constants[ShaderStage::Vertex].byteCount = sizeof(VertexRC);
			desc.constants[ShaderStage::Pixel].byteCount = sizeof(PixelRC);
			rootSignature = CreateRootSignature(desc);
		}

		{
			unsigned char* pixels;
			int width, height;
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

			TextureDesc desc("Dear ImGUI font atlas", width, height, 1);
			fontAtlas = CreateTexture(desc);

			MappedTexture update;
			BeginTextureUpload(update, fontAtlas);
			for(uint32_t r = 0; r < update.rowCount; ++r)
			{
				memcpy(update.mappedData + r * update.dstRowByteCount, pixels + r * update.srcRowByteCount, update.srcRowByteCount);
			}
			EndTextureUpload();
		}
	}

	{
		GraphicsPipelineDesc desc("Dear ImGUI", rootSignature);
		desc.shortLifeTime = true;
		desc.vertexShader = ShaderByteCode(g_vs);
		desc.pixelShader = ShaderByteCode(g_ps);
		desc.vertexLayout.bindingStrides[0] = sizeof(ImDrawVert);
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Position,
			DataType::Float32, 2, offsetof(ImDrawVert, pos));
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::TexCoord,
			DataType::Float32, 2, offsetof(ImDrawVert, uv));
		desc.vertexLayout.AddAttribute(0, ShaderSemantic::Color,
			DataType::UNorm8, 4, offsetof(ImDrawVert, col));
		desc.depthStencil.depthComparison = ComparisonFunction::Always;
		desc.depthStencil.depthStencilFormat = TextureFormat::Depth32_Float;
		desc.depthStencil.enableDepthTest = false;
		desc.depthStencil.enableDepthWrites = false;
		desc.rasterizer.cullMode = CT_TWO_SIDED;
		desc.AddRenderTarget(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, grp.renderTargetFormat);
		pipeline = CreateGraphicsPipeline(desc);
	}

	RegisterFontAtlas();
}

void ImGUI::RegisterFontAtlas()
{
	ImGuiIO& io = ImGui::GetIO();

	const uint32_t fontIndex = grp.RegisterTexture(fontAtlas);
	io.Fonts->SetTexID((ImTextureID)fontIndex);
}

void ImGUI::Draw()
{
	if(r_debugUI->integer == 0)
	{
		SafeEndFrame();
		return;
	}

	grp.renderMode = RenderMode::ImGui;

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = glConfig.vidWidth;
	io.DisplaySize.y = glConfig.vidHeight;

	SafeEndFrame();
	ImGui::Render();

	const ImDrawData* drawData = ImGui::GetDrawData();

	// avoid rendering when minimized
	if(drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
	{
		grp.renderMode = RenderMode::None;
		return;
	}

	SCOPED_RENDER_PASS("Dear ImGUI", 0.5f, 0.5f, 1.0f);

	FrameResources* fr = &frameResources[GetFrameIndex()];

	// Upload vertex/index data into a single contiguous GPU buffer
	ImDrawVert* vtxDst = (ImDrawVert*)MapBuffer(fr->vertexBuffer);
	ImDrawIdx* idxDst = (ImDrawIdx*)MapBuffer(fr->indexBuffer);
	ImDrawVert* const vtxDstEnd = vtxDst + MAX_VERTEX_COUNT;
	ImDrawIdx* const idxDstEnd = idxDst + MAX_INDEX_COUNT;
	for(int cl = 0; cl < drawData->CmdListsCount; cl++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[cl];
		if(vtxDst + cmdList->VtxBuffer.Size > vtxDstEnd)
		{
			Q_assert(!"Dear ImGui vertex buffer too small");
			break;
		}
		if(idxDst + cmdList->IdxBuffer.Size > idxDstEnd)
		{
			Q_assert(!"Dear ImGui index buffer too small");
			break;
		}
		memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtxDst += cmdList->VtxBuffer.Size;
		idxDst += cmdList->IdxBuffer.Size;
	}
	UnmapBuffer(fr->vertexBuffer);
	UnmapBuffer(fr->indexBuffer);

	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
	const float L = drawData->DisplayPos.x;
	const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
	const float T = drawData->DisplayPos.y;
	const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
	const VertexRC vertexRC =
	{
		2.0f / (R - L),    0.0f,              0.0f, 0.0f,
		0.0f,              2.0f / (T - B),    0.0f, 0.0f,
		0.0f,              0.0f,              0.5f, 0.0f,
		(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f
	};
	const uint32_t vertexStride = sizeof(ImDrawVert);
	static_assert(sizeof(ImDrawIdx) == 4, "uint32 indices expected!");

	CmdBindRenderTargets(1, &grp.renderTarget, NULL);
	CmdBindRootSignature(rootSignature);
	CmdBindPipeline(pipeline);
	CmdBindDescriptorTable(rootSignature, grp.descriptorTable);
	CmdBindVertexBuffers(1, &fr->vertexBuffer, &vertexStride, NULL);
	CmdBindIndexBuffer(fr->indexBuffer, IndexType::UInt32, 0);
	CmdSetViewport(0, 0, drawData->DisplaySize.x, drawData->DisplaySize.y);
	CmdSetRootConstants(rootSignature, ShaderStage::Vertex, &vertexRC);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int globalVtxOffset = 0;
	int globalIdxOffset = 0;
	ImVec2 clipOff = drawData->DisplayPos;
	for(int cl = 0; cl < drawData->CmdListsCount; cl++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[cl];
		for(int c = 0; c < cmdList->CmdBuffer.Size; c++)
		{
			const ImDrawCmd* cmd = &cmdList->CmdBuffer[c];

			// Project scissor/clipping rectangles into framebuffer space
			ImVec2 clip_min(cmd->ClipRect.x - clipOff.x, cmd->ClipRect.y - clipOff.y);
			ImVec2 clip_max(cmd->ClipRect.z - clipOff.x, cmd->ClipRect.w - clipOff.y);
			if(clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
			{
				continue;
			}

			PixelRC pixelRC = {};
			pixelRC.texture = (uint32_t)cmd->TextureId & 0xFFFF;
			pixelRC.sampler = GetSamplerIndex(TW_CLAMP_TO_EDGE, TextureFilter::Linear);
			pixelRC.mip = (float)(((uint32_t)cmd->TextureId >> 16) & 0xFFFF);
			CmdSetRootConstants(rootSignature, ShaderStage::Pixel, &pixelRC);

			// Apply Scissor/clipping rectangle, Draw
			CmdSetScissor(clip_min.x, clip_min.y, clip_max.x - clip_min.x, clip_max.y - clip_min.y);
			CmdDrawIndexed(cmd->ElemCount, cmd->IdxOffset + globalIdxOffset, cmd->VtxOffset + globalVtxOffset);
		}

		globalIdxOffset += cmdList->IdxBuffer.Size;
		globalVtxOffset += cmdList->VtxBuffer.Size;
	}

	grp.renderMode = RenderMode::None;
}

void ImGUI::SafeBeginFrame()
{
	if(!frameStarted)
	{
		ImGui::NewFrame();
		frameStarted = true;
	}
}

void ImGUI::SafeEndFrame()
{
	if(frameStarted)
	{
		ImGui::EndFrame();
		frameStarted = false;
	}
}
