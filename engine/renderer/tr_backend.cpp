/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"


backEndData_t*	backEndData;
backEndState_t	backEnd;

static const shader_t* prevShader = NULL;
static const shaderStage_t** prevStages = NULL;
static shader_t shader;
static shaderStage_t stage;
static const shaderStage_t* stagePtr = &stage;


void RB_PushSingleStageShader( int stateBits, cullType_t cullType )
{
	prevShader = tess.shader;
	prevStages = tess.xstages;
	tess.xstages = &stagePtr;
	tess.shader = &shader;

	memset(&stage, 0, sizeof(stage));
	stage.active = qtrue;
	stage.bundle.image[0] = tr.whiteImage;
	stage.stateBits = stateBits;
	stage.rgbGen = CGEN_IDENTITY;
	stage.alphaGen = AGEN_IDENTITY;
	stage.tcGen = TCGEN_TEXTURE;
	
	memset(&shader, 0, sizeof(shader));
	shader.cullType = cullType;
	shader.numStages = 1;
	shader.stages[0] = &stage;
}


void RB_PopShader()
{
	tess.shader = prevShader;
	tess.xstages = prevStages;
}


// used when a player has predicted a teleport, but hasn't arrived yet
float RB_HyperspaceColor()
{
	if ( r_teleporterFlash->integer == 0 )
	{
		return 0.0f;
	}

	const float c = 0.25f + 0.5f * sinf(M_PI * (backEnd.refdef.time & 0x01FF) / 0x0200);

	return c;
}
