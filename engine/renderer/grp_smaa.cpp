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
// Gameplay Rendering Pipeline - Enhanced Subpixel Morphological Antialiasing


#include "grp_local.h"
#include "smaa_area_texture.h"
#include "smaa_search_texture.h"
#include "hlsl/complete_smaa.h"


#define SMAA_PRESET_LIST(P) \
	P(low) \
	P(medium) \
	P(high) \
	P(ultra)

#define SMAA_PRESET(Name) \
	ShaderByteCode(Name##_1_vs), \
	ShaderByteCode(Name##_1_ps), \
	ShaderByteCode(Name##_2_vs), \
	ShaderByteCode(Name##_2_ps), \
	ShaderByteCode(Name##_3_vs), \
	ShaderByteCode(Name##_3_ps),

// 4 presets, 3 passes, 2 shaders per pass
static const ShaderByteCode shaders[4 * 3 * 2] =
{
	SMAA_PRESET_LIST(SMAA_PRESET)
};

#undef SMAA_PRESET
#undef SMAA_PRESET_LIST


#pragma pack(push, 4)
struct RootConstants
{
	vec4_t rtMetrics;
};
#pragma pack(pop)


static const ShaderByteCode& GetShaderByteCode(uint32_t passIndex, bool pixelShader)
{
	Q_assert(r_smaa->integer >= 1 && r_smaa->integer <= 4);
	const uint32_t presetIndex = r_smaa->integer - 1;
	const uint32_t shaderIndex = (presetIndex * 6) + (passIndex * 2) + (pixelShader ? 1 : 0);
	Q_assert(shaderIndex < ARRAY_LEN(shaders));

	return shaders[shaderIndex];
}


void SMAA::Init()
{
	Update();
}

void SMAA::Update()
{
	const Mode::Id newMode = (Mode::Id)r_smaa->integer;
	const int newWidth = glConfig.vidWidth;
	const int newHeight = glConfig.vidHeight;

	const bool alwaysEnabled = mode != Mode::Disabled && newMode != Mode::Disabled;
	const bool justEnabled = mode == Mode::Disabled && newMode != Mode::Disabled;
	const bool justDisabled = mode != Mode::Disabled && newMode == Mode::Disabled;
	const bool resChanged = newWidth != width || newHeight != height;
	const bool presetChanged = newMode != mode;

	bool createFixed = justEnabled && !fixedLoaded;
	bool destroyFixed = justDisabled && fixedLoaded;

	bool createResDep = justEnabled || (alwaysEnabled && resChanged);
	bool destroyResDep = justDisabled || (alwaysEnabled && resChanged);

	bool createPresetDep = justEnabled || (alwaysEnabled && presetChanged);
	bool destroyPresetDep = justDisabled || (alwaysEnabled && presetChanged);

	if(grp.firstInit)
	{
		// first init or device change: we have nothing to destroy
		const bool enableSMAA = newMode != Mode::Disabled;
		createFixed = enableSMAA;
		createResDep = enableSMAA;
		createPresetDep = enableSMAA;
		destroyFixed = false;
		destroyResDep = false;
		destroyPresetDep = false;
		mode = Mode::Disabled;
		width = -1;
		height = -1;
		fixedLoaded = false;
	}

	if(destroyFixed || destroyResDep || destroyPresetDep)
	{
		WaitUntilDeviceIsIdle();
	}

	if(destroyFixed)
	{
		fixedLoaded = false;
		DestroyTexture(areaTexture);
		DestroyTexture(searchTexture);
		DestroyRootSignature(rootSignature);
		DestroyDescriptorTable(descriptorTable);
	}

	if(createFixed)
	{
		fixedLoaded = true;
		{
			TextureDesc desc("SMAA area", AREATEX_WIDTH, AREATEX_HEIGHT);
			desc.initialState = ResourceStates::PixelShaderAccessBit;
			desc.allowedState = ResourceStates::PixelShaderAccessBit;
			desc.committedResource = true;
			desc.format = TextureFormat::RG16_UNorm;
			areaTexture = CreateTexture(desc);

			MappedTexture texture;
			BeginTextureUpload(texture, areaTexture);
			Q_assert(texture.srcRowByteCount == AREATEX_PITCH);
			for(uint32_t r = 0; r < AREATEX_HEIGHT; ++r)
			{
				memcpy(texture.mappedData + r * texture.dstRowByteCount, areaTexBytes + r * texture.srcRowByteCount, texture.srcRowByteCount);
			}
			EndTextureUpload();
		}
		{
			TextureDesc desc("SMAA search", SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT);
			desc.initialState = ResourceStates::PixelShaderAccessBit;
			desc.allowedState = ResourceStates::PixelShaderAccessBit;
			desc.committedResource = true;
			desc.format = TextureFormat::R8_UNorm;
			searchTexture = CreateTexture(desc);

			MappedTexture texture;
			BeginTextureUpload(texture, searchTexture);
			Q_assert(texture.srcRowByteCount == SEARCHTEX_PITCH);
			for(uint32_t r = 0; r < SEARCHTEX_HEIGHT; ++r)
			{
				memcpy(texture.mappedData + r * texture.dstRowByteCount, searchTexBytes + r * texture.srcRowByteCount, texture.srcRowByteCount);
			}
			EndTextureUpload();
		}
		{
			RootSignatureDesc desc("SMAA");
			desc.constants[ShaderStage::Vertex].byteCount = sizeof(RootConstants);
			desc.constants[ShaderStage::Pixel].byteCount = sizeof(RootConstants);
			desc.samplerVisibility = ShaderStages::PixelBit;
			desc.samplerCount = 2;
			desc.genericVisibility = ShaderStages::PixelBit;
			desc.AddRange(DescriptorType::Texture, 0, 5);
			rootSignature = CreateRootSignature(desc);
		}
		{
			DescriptorTableDesc desc("SMAA", rootSignature);
			descriptorTable = CreateDescriptorTable(desc);

			const HSampler samplers[] =
			{
				// can do linear & point or linear & linear
				// it seems point was some GCN-specific optimization
				grp.samplers[GetSamplerIndex(TW_CLAMP_TO_EDGE, TextureFilter::Linear)],
				grp.samplers[GetSamplerIndex(TW_CLAMP_TO_EDGE, TextureFilter::Linear)]
			};
			DescriptorTableUpdate update;
			update.SetSamplers(ARRAY_LEN(samplers), samplers);
			UpdateDescriptorTable(descriptorTable, update);
		}
	}

	if(destroyResDep)
	{
		DestroyTexture(edgeTexture);
		DestroyTexture(blendTexture);
		DestroyTexture(stencilTexture);
		DestroyTexture(inputTexture);
		DestroyTexture(outputTexture);
	}

	if(createResDep)
	{
		{
			TextureDesc desc("SMAA edges", glConfig.vidWidth, glConfig.vidHeight);
			desc.initialState = ResourceStates::RenderTargetBit;
			desc.allowedState = ResourceStates::RenderTargetBit | ResourceStates::PixelShaderAccessBit;
			Vector4Clear(desc.clearColor);
			desc.usePreferredClearValue = true;
			desc.committedResource = true;
			desc.format = TextureFormat::RG16_UNorm;
			edgeTexture = CreateTexture(desc);
		}
		{
			TextureDesc desc("SMAA blend", glConfig.vidWidth, glConfig.vidHeight);
			desc.initialState = ResourceStates::RenderTargetBit;
			desc.allowedState = ResourceStates::RenderTargetBit | ResourceStates::PixelShaderAccessBit;
			Vector4Clear(desc.clearColor);
			desc.usePreferredClearValue = true;
			desc.committedResource = true;
			desc.format = TextureFormat::RGBA32_UNorm;
			blendTexture = CreateTexture(desc);
		}
		{
			TextureDesc desc("SMAA tone mapped input", glConfig.vidWidth, glConfig.vidHeight);
			desc.initialState = ResourceStates::RenderTargetBit;
			desc.allowedState = ResourceStates::RenderTargetBit | ResourceStates::PixelShaderAccessBit;
			desc.committedResource = true;
			desc.format = TextureFormat::RGBA32_UNorm;
			inputTexture = CreateTexture(desc);
		}
		{
			TextureDesc desc("SMAA tone mapped output", glConfig.vidWidth, glConfig.vidHeight);
			desc.initialState = ResourceStates::RenderTargetBit;
			desc.allowedState = ResourceStates::RenderTargetBit | ResourceStates::PixelShaderAccessBit;
			desc.committedResource = true;
			desc.format = TextureFormat::RGBA32_UNorm;
			outputTexture = CreateTexture(desc);

			grp.post.SetInverseToneMapInput(outputTexture);
		}
		{
			TextureDesc desc("SMAA stencil buffer", glConfig.vidWidth, glConfig.vidHeight);
			desc.initialState = ResourceStates::DepthWriteBit;
			desc.allowedState = ResourceStates::DepthWriteBit;
			desc.committedResource = true;
			desc.format = TextureFormat::Depth24_Stencil8;
			desc.clearDepth = 0.0f;
			desc.clearStencil = 0;
			desc.usePreferredClearValue = true;
			stencilTexture = CreateTexture(desc);
		}
		{
			const HTexture textures[] =
			{
				inputTexture, edgeTexture, areaTexture, searchTexture, blendTexture
			};

			DescriptorTableUpdate update;
			update.SetTextures(ARRAY_LEN(textures), textures);
			UpdateDescriptorTable(descriptorTable, update);
		}
	}

	if(destroyPresetDep)
	{
		DestroyPipeline(firstPassPipeline);
		DestroyPipeline(secondPassPipeline);
		DestroyPipeline(thirdPassPipeline);
	}

	if(createPresetDep)
	{
		{
			GraphicsPipelineDesc desc("SMAA edge detection", rootSignature);
			desc.vertexShader = GetShaderByteCode(0, false);
			desc.pixelShader = GetShaderByteCode(0, true);
			desc.depthStencil.DisableDepth();
			desc.depthStencil.enableStencil = true;
			desc.depthStencil.depthStencilFormat = TextureFormat::Depth24_Stencil8;
			desc.depthStencil.frontFace.passOp = StencilOp::Replace;
			desc.depthStencil.backFace.passOp = StencilOp::Replace;
			desc.rasterizer.cullMode = CT_TWO_SIDED;
			desc.AddRenderTarget(0, TextureFormat::RG16_UNorm);
			firstPassPipeline = CreateGraphicsPipeline(desc);
		}
		{
			GraphicsPipelineDesc desc("SMAA blend weight computation", rootSignature);
			desc.vertexShader = GetShaderByteCode(1, false);
			desc.pixelShader = GetShaderByteCode(1, true);
			desc.depthStencil.DisableDepth();
			desc.depthStencil.enableStencil = true;
			desc.depthStencil.stencilWriteMask = 0;
			desc.depthStencil.depthStencilFormat = TextureFormat::Depth24_Stencil8;
			desc.depthStencil.frontFace.comparison = ComparisonFunction::Equal;
			desc.depthStencil.backFace.comparison = ComparisonFunction::Equal;
			desc.rasterizer.cullMode = CT_TWO_SIDED;
			desc.AddRenderTarget(0, TextureFormat::RGBA32_UNorm);
			secondPassPipeline = CreateGraphicsPipeline(desc);
		}
		{
			GraphicsPipelineDesc desc("SMAA neighborhood blending", rootSignature);
			desc.vertexShader = GetShaderByteCode(2, false);
			desc.pixelShader = GetShaderByteCode(2, true);
			desc.depthStencil.DisableDepth();
			desc.rasterizer.cullMode = CT_TWO_SIDED;
			desc.AddRenderTarget(0, TextureFormat::RGBA32_UNorm);
			thirdPassPipeline = CreateGraphicsPipeline(desc);
		}
	}

	mode = (Mode::Id)r_smaa->integer;
	width = glConfig.vidWidth;
	height = glConfig.vidHeight;
}

void SMAA::Draw(const viewParms_t& parms)
{
	if(r_smaa->integer == 0)
	{
		return;
	}

	// a render pass for every little 3D head in the scoreboard? no thanks
	const int sizeThreshold = (max(glConfig.vidWidth, glConfig.vidHeight) + 29) / 30;
	if(parms.viewportWidth <= sizeThreshold && parms.viewportHeight <= sizeThreshold)
	{
		return;
	}

	const char* const presetNames[] = { "", "Low", "Medium", "High", "Ultra" };
	SCOPED_RENDER_PASS(va("SMAA %s", presetNames[r_smaa->integer]), 0.5f, 0.25f, 0.75f);

	// can't clear targets if they're not in render target state
	const TextureBarrier clearBarriers[2] =
	{
		TextureBarrier(edgeTexture, ResourceStates::RenderTargetBit),
		TextureBarrier(blendTexture, ResourceStates::RenderTargetBit)
	};
	CmdBarrier(ARRAY_LEN(clearBarriers), clearBarriers);

	CmdClearColorTarget(edgeTexture, vec4_zero);
	CmdClearColorTarget(blendTexture, vec4_zero);
	CmdClearDepthStencilTarget(stencilTexture, false, 0.0f, true, 0);
	CmdSetStencilReference(255);

	CmdSetViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	CmdSetScissor(parms.viewportX, parms.viewportY, parms.viewportWidth, parms.viewportHeight);

	// tone map for higher quality AA
	{
		const TextureBarrier barriers[2] =
		{
			TextureBarrier(grp.renderTarget, ResourceStates::PixelShaderAccessBit),
			TextureBarrier(inputTexture, ResourceStates::RenderTargetBit)
		};
		CmdBarrier(ARRAY_LEN(barriers), barriers);

		CmdBindRenderTargets(1, &inputTexture, NULL);
		grp.post.ToneMap(); // RTCF_R8G8B8A8 is assumed
	}

	CmdBindRootSignature(rootSignature);
	CmdBindDescriptorTable(rootSignature, descriptorTable);

	RootConstants rc = {};
	rc.rtMetrics[0] = 1.0f / (float)glConfig.vidWidth;
	rc.rtMetrics[1] = 1.0f / (float)glConfig.vidHeight;
	rc.rtMetrics[2] = (float)glConfig.vidWidth;
	rc.rtMetrics[3] = (float)glConfig.vidHeight;
	CmdSetRootConstants(rootSignature, ShaderStage::Vertex, &rc);
	CmdSetRootConstants(rootSignature, ShaderStage::Pixel, &rc);

	// run edge detection
	{
		const TextureBarrier barrier(inputTexture, ResourceStates::PixelShaderAccessBit);
		CmdBarrier(1, &barrier);

		CmdBindRenderTargets(1, &edgeTexture, &stencilTexture);
		CmdBindPipeline(firstPassPipeline);
		CmdDraw(3, 0);
	}

	// compute blend weights
	{
		const TextureBarrier barrier(edgeTexture, ResourceStates::PixelShaderAccessBit);
		CmdBarrier(1, &barrier);

		CmdBindRenderTargets(1, &blendTexture, &stencilTexture);
		CmdBindPipeline(secondPassPipeline);
		CmdDraw(3, 0);
	}

	// blend pass
	{
		const TextureBarrier barriers[2] =
		{
			TextureBarrier(blendTexture, ResourceStates::PixelShaderAccessBit),
			TextureBarrier(outputTexture, ResourceStates::RenderTargetBit)
		};
		CmdBarrier(ARRAY_LEN(barriers), barriers);

		CmdBindRenderTargets(1, &outputTexture, NULL);
		CmdBindPipeline(thirdPassPipeline);
		CmdDraw(3, 0);
	}

	// inverse tone map and write back to the main render target
	{
		const TextureBarrier barriers[2] =
		{
			TextureBarrier(outputTexture, ResourceStates::PixelShaderAccessBit),
			TextureBarrier(grp.renderTarget, ResourceStates::RenderTargetBit)
		};
		CmdBarrier(ARRAY_LEN(barriers), barriers);

		CmdBindRenderTargets(1, &grp.renderTarget, NULL);
		grp.post.InverseToneMap(r_rtColorFormat->integer);
	}
}
