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
// Gameplay Rendering Pipeline - post-process pass


#include "grp_local.h"
namespace tone_map
{
#include "hlsl/post_gamma_vs.h"
#include "hlsl/post_gamma_ps.h"
}
namespace inverse_tone_map
{
#include "hlsl/post_inverse_gamma_vs.h"
#include "hlsl/post_inverse_gamma_ps.h"
}


#pragma pack(push, 4)

struct GammaVertexRC
{
	float scaleX;
	float scaleY;
};

struct GammaPixelRC
{
	float invGamma;
	float brightness;
	float greyscale;
};

struct InverseGammaPixelRC
{
	float gamma;
	float invBrightness;
};

#pragma pack(pop)


void PostProcess::Init()
{
	if(!grp.firstInit)
	{
		return;
	}

	TextureFormat::Id rtFormats[RTCF_COUNT] = {};
	rtFormats[RTCF_R8G8B8A8] = TextureFormat::RGBA32_UNorm;
	rtFormats[RTCF_R10G10B10A2] = TextureFormat::R10G10B10A2_UNorm;
	rtFormats[RTCF_R16G16B16A16] = TextureFormat::RGBA64_UNorm;
	for(int i = 0; i < RTCF_COUNT; ++i)
	{
		Q_assert((int)rtFormats[i] > 0 && (int)rtFormats[i] < TextureFormat::Count);
	}

	{
		RootSignatureDesc desc("tone map");
		desc.usingVertexBuffers = false;
		desc.constants[ShaderStage::Vertex].byteCount = sizeof(GammaVertexRC);
		desc.constants[ShaderStage::Pixel].byteCount = sizeof(GammaPixelRC);
		desc.samplerCount = 1;
		desc.samplerVisibility = ShaderStages::PixelBit;
		desc.AddRange(DescriptorType::Texture, 0, 1);
		desc.genericVisibility = ShaderStages::PixelBit;
		toneMapRootSignature = CreateRootSignature(desc);
	}
	{
		DescriptorTableDesc desc("tone map", toneMapRootSignature);
		toneMapDescriptorTable = CreateDescriptorTable(desc);

		DescriptorTableUpdate update;
		update.SetSamplers(1, &grp.samplers[GetSamplerIndex(TW_CLAMP_TO_EDGE, TextureFilter::Linear)]);
		UpdateDescriptorTable(toneMapDescriptorTable, update);
	}
	{
		GraphicsPipelineDesc desc("tone map", toneMapRootSignature);
		desc.vertexShader = ShaderByteCode(tone_map::g_vs);
		desc.pixelShader = ShaderByteCode(tone_map::g_ps);
		desc.depthStencil.DisableDepth();
		desc.rasterizer.cullMode = CT_TWO_SIDED;
		desc.AddRenderTarget(0, TextureFormat::RGBA32_UNorm);
		toneMapPipeline = CreateGraphicsPipeline(desc);
	}

	{
		RootSignatureDesc desc("inverse tone map");
		desc.usingVertexBuffers = false;
		desc.constants[ShaderStage::Vertex].byteCount = 0;
		desc.constants[ShaderStage::Pixel].byteCount = sizeof(InverseGammaPixelRC);
		desc.samplerCount = 1;
		desc.samplerVisibility = ShaderStages::PixelBit;
		desc.AddRange(DescriptorType::Texture, 0, 1);
		desc.genericVisibility = ShaderStages::PixelBit;
		inverseToneMapRootSignature = CreateRootSignature(desc);
	}
	{
		DescriptorTableDesc desc("inverse tone map", inverseToneMapRootSignature);
		inverseToneMapDescriptorTable = CreateDescriptorTable(desc);

		DescriptorTableUpdate update;
		update.SetSamplers(1, &grp.samplers[GetSamplerIndex(TW_CLAMP_TO_EDGE, TextureFilter::Linear)]);
		UpdateDescriptorTable(inverseToneMapDescriptorTable, update);
	}
	for(int i = 0; i < RTCF_COUNT; ++i)
	{
		GraphicsPipelineDesc desc("inverse tone map", inverseToneMapRootSignature);
		desc.vertexShader = ShaderByteCode(inverse_tone_map::g_vs);
		desc.pixelShader = ShaderByteCode(inverse_tone_map::g_ps);
		desc.depthStencil.DisableDepth();
		desc.rasterizer.cullMode = CT_TWO_SIDED;
		desc.AddRenderTarget(0, rtFormats[i]);
		inverseToneMapPipelines[i] = CreateGraphicsPipeline(desc);
	}
}

void PostProcess::Draw(const char* renderPassName, HTexture renderTarget)
{
	SCOPED_RENDER_PASS(renderPassName, 0.125f, 0.125f, 0.5f);

	const TextureBarrier barriers[2] =
	{
		TextureBarrier(grp.renderTarget, ResourceStates::PixelShaderAccessBit),
		TextureBarrier(renderTarget, ResourceStates::RenderTargetBit)
	};
	CmdBarrier(ARRAY_LEN(barriers), barriers);

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
	}

	if(vsX != 1.0f || vsY != 1.0f)
	{
		CmdClearColorTarget(renderTarget, colorBlack);

		const int x = (glInfo.winWidth - glInfo.winWidth * vsX) / 2.0f;
		const int y = (glInfo.winHeight - glInfo.winHeight * vsY) / 2.0f;
		CmdSetViewport(0, 0, glInfo.winWidth, glInfo.winHeight);
		CmdSetScissor(x, y, glConfig.vidWidth * srX, glConfig.vidHeight * srY);
	}
	else
	{
		CmdSetViewportAndScissor(0, 0, glInfo.winWidth, glInfo.winHeight);
	}

	GammaVertexRC vertexRC = {};
	vertexRC.scaleX = vsX;
	vertexRC.scaleY = vsY;

	GammaPixelRC pixelRC = {};
	pixelRC.invGamma = 1.0f / r_gamma->value;
	pixelRC.brightness = r_brightness->value;
	pixelRC.greyscale = r_greyscale->value;

	CmdBindRenderTargets(1, &renderTarget, NULL);
	CmdBindPipeline(toneMapPipeline);
	CmdBindRootSignature(toneMapRootSignature);
	CmdBindDescriptorTable(toneMapRootSignature, toneMapDescriptorTable);
	CmdSetRootConstants(toneMapRootSignature, ShaderStage::Vertex, &vertexRC);
	CmdSetRootConstants(toneMapRootSignature, ShaderStage::Pixel, &pixelRC);
	CmdDraw(3, 0);
}

void PostProcess::ToneMap()
{
	GammaVertexRC vertexRC = {};
	vertexRC.scaleX = 1.0f;
	vertexRC.scaleY = 1.0f;

	GammaPixelRC pixelRC = {};
	pixelRC.invGamma = 1.0f / r_gamma->value;
	pixelRC.brightness = r_brightness->value;
	pixelRC.greyscale = 0.0f;

	CmdBindPipeline(toneMapPipeline);
	CmdBindRootSignature(toneMapRootSignature);
	CmdBindDescriptorTable(toneMapRootSignature, toneMapDescriptorTable);
	CmdSetRootConstants(toneMapRootSignature, ShaderStage::Vertex, &vertexRC);
	CmdSetRootConstants(toneMapRootSignature, ShaderStage::Pixel, &pixelRC);
	CmdDraw(3, 0);
}

void PostProcess::InverseToneMap(int colorFormat)
{
	InverseGammaPixelRC pixelRC = {};
	pixelRC.gamma = r_gamma->value;
	pixelRC.invBrightness = 1.0f / r_brightness->value;

	CmdBindPipeline(inverseToneMapPipelines[colorFormat]);
	CmdBindRootSignature(inverseToneMapRootSignature);
	CmdBindDescriptorTable(inverseToneMapRootSignature, inverseToneMapDescriptorTable);
	CmdSetRootConstants(inverseToneMapRootSignature, ShaderStage::Pixel, &pixelRC);
	CmdDraw(3, 0);
}

void PostProcess::SetToneMapInput(HTexture toneMapInput)
{
	DescriptorTableUpdate update;
	update.SetTextures(1, &toneMapInput);
	UpdateDescriptorTable(toneMapDescriptorTable, update);
}

void PostProcess::SetInverseToneMapInput(HTexture inverseToneMapInput)
{
	DescriptorTableUpdate update;
	update.SetTextures(1, &inverseToneMapInput);
	UpdateDescriptorTable(inverseToneMapDescriptorTable, update);
}
