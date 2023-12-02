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

// tr_shader.c -- this file deals with the parsing and definition of shaders

const float r_depthFadeScale[DFT_COUNT][4] =
{
	{ 0.0f, 0.0f, 0.0f, 0.0f }, // DFT_NONE
	{ 1.0f, 1.0f, 1.0f, 0.0f }, // DFT_BLEND
	{ 0.0f, 0.0f, 0.0f, 1.0f }, // DFT_ADD
	{ 0.0f, 0.0f, 0.0f, 1.0f }, // DFT_MULT
	{ 0.0f, 0.0f, 0.0f, 0.0f }, // DFT_PMA
	{ 0.0f, 0.0f, 0.0f, 0.0f }  // DFT_TBD
};

const float r_depthFadeBias[DFT_COUNT][4] =
{
	{ 0.0f, 0.0f, 0.0f, 0.0f }, // DFT_NONE
	{ 0.0f, 0.0f, 0.0f, 0.0f }, // DFT_BLEND
	{ 0.0f, 0.0f, 0.0f, 0.0f }, // DFT_ADD
	{ 1.0f, 1.0f, 1.0f, 0.0f }, // DFT_MULT
	{ 0.0f, 0.0f, 0.0f, 0.0f }, // DFT_PMA
	{ 0.0f, 0.0f, 0.0f, 0.0f }  // DFT_TBD
};

static char* s_shaderText = 0;
static int s_numShaderFiles = 0;
static char* s_shaderFileNames = 0;
static int* s_shaderFileOffsets = 0;
static int* s_shaderFileNameOffsets = 0;
static int* s_shaderPakChecksums = 0;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static	shader_t		shader;
static	shaderStage_t	stages[MAX_SHADER_STAGES];
static	texModInfo_t	texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];

#define FILE_HASH_SIZE 1024
static shader_t* hashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH 2048
static char** shaderTextHashTable[MAX_SHADERTEXT_HASH];


static qbool ParseVector( const char** text, int count, float *v )
{
	int		i;

	// FIXME: spaces are currently required after parens, should change parseext...
	const char* token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, "(" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	for ( i = 0 ; i < count ; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
			return qfalse;
		}
		v[i] = atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, ")" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	return qtrue;
}


/*
===============
NameToAFunc
===============
*/
static unsigned NameToAFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "GT0" ) )
	{
		return GLS_ATEST_GT_0;
	}
	else if ( !Q_stricmp( funcname, "LT128" ) )
	{
		return GLS_ATEST_LT_80;
	}
	else if ( !Q_stricmp( funcname, "GE128" ) )
	{
		return GLS_ATEST_GE_80;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name );
	return 0;
}


/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return GF_NOISE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name );
	return GF_SIN;
}


static void ParseWaveForm( const char** text, waveForm_t* wave )
{
	const char* token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->func = NameToGenFunc( token );

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->base = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->amplitude = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->phase = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->frequency = atof( token );
}


static void ParseTexMod( const char** text, shaderStage_t *stage )
{
	const char *token;
	texModInfo_t *tmi;

	if ( stage->numTexMods == TR_MAX_TEXMODS ) {
		ri.Error( ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name );
		return;
	}

	tmi = &stage->texMods[stage->numTexMods];
	stage->numTexMods++;

	token = COM_ParseExt( text, qfalse );

	//
	// turb
	//
	if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = atof( token );

		tmi->type = TMOD_TURBULENT;
	}
	//
	// scale
	//
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[1] = atof( token );
		tmi->type = TMOD_SCALE;
	}
	//
	// scroll
	//
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[0] = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[1] = atof( token );
		tmi->type = TMOD_SCROLL;
	}
	//
	// stretch
	//
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.func = NameToGenFunc( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = atof( token );

		tmi->type = TMOD_STRETCH;
	}
	//
	// transform
	//
	else if ( !Q_stricmp( token, "transform" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = atof( token );

		tmi->type = TMOD_TRANSFORM;
	}
	//
	// rotate
	//
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->rotateSpeed = atof( token );
		tmi->type = TMOD_ROTATE;
	}
	//
	// entityTranslate
	//
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		ri.Printf( PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name );
	}
}


static qbool ParseStage( const char** text, shaderStage_t* stage )
{
	int depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
#if defined( QC )
	int depthTestBits = 0;
#endif // QC
	qbool depthMaskExplicit = qfalse;

	stage->active = qtrue;

	const char* token;
	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no matching '}' found\n" );
			return qfalse;
		}

		if ( token[0] == '}' )
		{
			break;
		}
		//
		// map <name>
		//
		else if ( !Q_stricmp( token, "map" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "$whiteimage" ) )
			{
				stage->bundle.image[0] = tr.whiteImage;
				continue;
			}
			else if ( !Q_stricmp( token, "$lightmap" ) )
			{
				if ( shader.lightmapIndex < 0 ) {
					stage->bundle.image[0] = tr.whiteImage;
				} else {
					stage->bundle.image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				stage->type = ST_LIGHTMAP;
/*
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
				// this HAS to match the rgbgen of the previous stage (ie the diffuse)
				// or they can't be collapsed - but the previous stage will have
				// (incorrectly) been defaulted to CGEN_IDENTITY_LIGHTING
				// when both of them SHOULD be CGEN_IDENTITY
				stage->rgbGen = CGEN_IDENTITY_LIGHTING;
*/
				continue;
			}
			else
			{
				stage->bundle.image[0] = R_FindImageFile( token, shader.imgflags, TW_REPEAT );
				if ( !stage->bundle.image[0] )
				{
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return qfalse;
				}
			}
		}
		//
		// clampmap <name>
		//
		else if ( !Q_stricmp( token, "clampmap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			stage->bundle.image[0] = R_FindImageFile( token, shader.imgflags, TW_CLAMP_TO_EDGE );
			if ( !stage->bundle.image[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
				return qfalse;
			}
		}
		//
		// animMap <frequency> <image1> .... <imageN>
		//
		else if ( !Q_stricmp( token, "animMap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle.imageAnimationSpeed = atof( token );

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while ( 1 ) {
				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) {
					break;
				}
				int num = stage->bundle.numImageAnimations;
				if ( num < MAX_IMAGE_ANIMATIONS ) {
					stage->bundle.image[num] = R_FindImageFile( token, shader.imgflags, TW_REPEAT );
					if ( !stage->bundle.image[num] )
					{
						ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
						return qfalse;
					}
					stage->bundle.numImageAnimations++;
				}
			}
		}
		else if ( !Q_stricmp( token, "videoMap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle.videoMapHandle = ri.CIN_PlayCinematic( token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader));
			if (stage->bundle.videoMapHandle != -1) {
				stage->bundle.isVideoMap = qtrue;
				stage->bundle.image[0] = tr.scratchImage[stage->bundle.videoMapHandle];
			}
		}
		//
		// alphafunc <func>
		//
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			atestBits = NameToAFunc( token );
		}
		//
		// depthFunc <func>
		//
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				depthFuncBits = 0;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// detail
		//
		else if ( !Q_stricmp( token, "detail" ) )
		{
			stage->isDetail = qtrue;
		}
		//
		// blendfunc <srcFactor> <dstFactor>
		// or blendfunc <add|filter|blend>
		//
		else if ( !Q_stricmp( token, "blendfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
				continue;
			}
			// check for "simple" blends first
			if ( !Q_stricmp( token, "add" ) ) {
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				// complex double blends
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}

			// clear depth mask for blended surfaces
			if ( !depthMaskExplicit )
			{
				depthMaskBits = 0;
			}
		}
		//
		// rgbGen
		//
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = CGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				vec3_t color;
				ParseVector( text, 3, color );
				stage->constantColor[0] = 255 * color[0];
				stage->constantColor[1] = 255 * color[1];
				stage->constantColor[2] = 255 * color[2];
				stage->rgbGen = CGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->rgbGen = CGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "identityLighting" ) )
			{
				stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->rgbGen = CGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->rgbGen = CGEN_VERTEX;
				if ( stage->alphaGen == 0 ) {
					stage->alphaGen = AGEN_VERTEX;
				}
			}
			else if ( !Q_stricmp( token, "exactVertex" ) )
			{
				stage->rgbGen = CGEN_EXACT_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingDiffuse" ) )
			{
				stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// alphaGen
		//
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = AGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				token = COM_ParseExt( text, qfalse );
				stage->constantColor[3] = 255 * atof( token );
				stage->alphaGen = AGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->alphaGen = AGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->alphaGen = AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->alphaGen = AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingSpecular" ) )
			{
				stage->alphaGen = AGEN_LIGHTING_SPECULAR;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					shader.portalRange = 256;
					ri.Printf( PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name );
				}
				else
				{
					shader.portalRange = atof( token );
				}
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// tcGen <function>
		//
		else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				stage->tcGen = TCGEN_ENVIRONMENT_MAPPED;
			}
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				stage->tcGen = TCGEN_LIGHTMAP;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
				stage->tcGen = TCGEN_TEXTURE;
			}
			else if ( !Q_stricmp( token, "vector" ) )
			{
				ParseVector( text, 3, stage->tcGenVectors[0] );
				ParseVector( text, 3, stage->tcGenVectors[1] );
				stage->tcGen = TCGEN_VECTOR;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name );
			}
		}
		//
		// tcMod <type> <...>
		//
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			ParseTexMod( text, stage );
			continue;
		}
		//
		// depthmask
		//
		else if ( !Q_stricmp( token, "depthwrite" ) )
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qtrue;
			continue;
		}
#if defined( QC )
		else if ( !Q_stricmp( token, "depthtest" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing depthtest parm in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "disable" ) ) {
				depthTestBits |= GLS_DEPTHTEST_DISABLE;
				continue;
			} else if ( !Q_stricmp( token, "enable" ) ) {
				depthTestBits &= ~GLS_DEPTHTEST_DISABLE;
				continue;
			} else {
				ri.Printf(PRINT_WARNING, "WARNING: unknown depthtest parm in shader '%s'\n", shader.name);
			}
		}
#endif // QC
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	//
	// if cgen isn't explicitly specified, use either identity or identitylighting
	//
	if ( stage->rgbGen == CGEN_BAD ) {
		if ( blendSrcBits == 0 ||
			blendSrcBits == GLS_SRCBLEND_ONE ||
			blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) {
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		} else {
			stage->rgbGen = CGEN_IDENTITY;
		}
	}


	//
	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	//
	if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) &&
		 ( blendDstBits == GLS_DSTBLEND_ZERO ) )
	{
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	// decide which agens we can skip
	if ( stage->alphaGen == AGEN_IDENTITY ) {
		if ( stage->rgbGen == CGEN_IDENTITY
			|| stage->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			stage->alphaGen = AGEN_SKIP;
		}
	}

	//
	// compute state bits
	//
	stage->stateBits = depthMaskBits | depthFuncBits |
					   blendSrcBits | blendDstBits |
					   atestBits;
#if defined( QC )
	stage->stateBits |= depthTestBits;
#endif // QC
	return qtrue;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
===============
*/
static void ParseDeform( const char** text )
{
	const char* token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name );
		return;
	}

	if ( shader.numDeforms == MAX_SHADER_DEFORMS ) {
		ri.Printf( PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name );
		return;
	}

	deformStage_t* ds = &shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if ( !Q_stricmp( token, "autosprite" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE;
		return;
	}

	if ( !Q_stricmp( token, "autosprite2" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE2;
		return;
	}

	if ( !Q_stricmpn( token, "text", 4 ) ) {
		int		n;

		n = token[4] - '0';
		if ( n < 0 || n > 7 ) {
			n = 0;
		}
		ds->deformation = deform_t(DEFORM_TEXT0 + n);
		return;
	}

	if ( !Q_stricmp( token, "bulge" ) )	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeWidth = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeHeight = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeSpeed = atof( token );

		ds->deformation = DEFORM_BULGE;
		return;
	}

	if ( !Q_stricmp( token, "wave" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}

		if ( atof( token ) != 0 )
		{
			ds->deformationSpread = 1.0f / atof( token );
		}
		else
		{
			ds->deformationSpread = 100.0f;
			ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_WAVE;
		return;
	}

	if ( !Q_stricmp( token, "normal" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.amplitude = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.frequency = atof( token );

		ds->deformation = DEFORM_NORMALS;
		return;
	}

	if ( !Q_stricmp( token, "move" ) ) {
		int		i;

		for ( i = 0 ; i < 3 ; i++ ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
				return;
			}
			ds->moveVector[i] = atof( token );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_MOVE;
		return;
	}

	//ri.Printf( PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
	ri.Error( ERR_FATAL, "unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
}


// skyParms <outerbox> <cloudheight> <innerbox>

static void ParseSkyParms( const char** text )
{
	static const char* suf[6] = { "rt", "lf", "bk", "ft", "up", "dn" };
	const char* token;
	char		pathname[MAX_QPATH];
	int			i;

	// outerbox
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i = 0; i < 6; ++i) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga", token, suf[i] );
			shader.sky.outerbox[i] = R_FindImageFile( pathname, IMG_NOMIPMAP | IMG_NOPICMIP, TW_CLAMP_TO_EDGE );
			if ( !shader.sky.outerbox[i] ) {
				shader.sky.outerbox[i] = tr.defaultImage;
			}
		}
	}

	// cloudheight
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	shader.sky.cloudHeight = atof( token );
	if ( !shader.sky.cloudHeight ) {
		shader.sky.cloudHeight = 512;
	}
	R_InitSkyTexCoords( shader.sky.cloudHeight );

	// innerbox
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i = 0; i < 6; ++i) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga", token, suf[i] );
			shader.sky.innerbox[i] = R_FindImageFile( pathname, IMG_NOMIPMAP | IMG_NOPICMIP, TW_REPEAT );
			if ( !shader.sky.innerbox[i] ) {
				shader.sky.innerbox[i] = tr.defaultImage;
			}
		}
	}

	shader.sort = SS_ENVIRONMENT;
	shader.isSky = qtrue;
}


static void ParseSort( const char** text )
{
	const char* token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name );
		return;
	}

	if ( !Q_stricmp( token, "portal" ) ) {
		shader.sort = SS_PORTAL;
	} else if ( !Q_stricmp( token, "sky" ) ) {
		shader.sort = SS_ENVIRONMENT;
	} else if ( !Q_stricmp( token, "opaque" ) ) {
		shader.sort = SS_OPAQUE;
	}else if ( !Q_stricmp( token, "decal" ) ) {
		shader.sort = SS_DECAL;
	} else if ( !Q_stricmp( token, "seeThrough" ) ) {
		shader.sort = SS_SEE_THROUGH;
	} else if ( !Q_stricmp( token, "banner" ) ) {
		shader.sort = SS_BANNER;
	} else if ( !Q_stricmp( token, "additive" ) ) {
		shader.sort = SS_BLEND1;
	} else if ( !Q_stricmp( token, "nearest" ) ) {
		shader.sort = SS_NEAREST;
	} else if ( !Q_stricmp( token, "underwater" ) ) {
		shader.sort = SS_UNDERWATER;
	} else {
		shader.sort = atof( token );
	}
}



// this table is also present in q3map

typedef struct {
	const char* name;
	int clearSolid, surfaceFlags, contents;
} infoParm_t;

static infoParm_t infoParms[] = {
	// server relevant contents
	{"water",		1,	0,	CONTENTS_WATER },
	{"slime",		1,	0,	CONTENTS_SLIME },		// mildly damaging
	{"lava",		1,	0,	CONTENTS_LAVA },		// very damaging
	{"playerclip",	1,	0,	CONTENTS_PLAYERCLIP },
	{"monsterclip",	1,	0,	CONTENTS_MONSTERCLIP },
	{"nodrop",		1,	0,	int(CONTENTS_NODROP) },	// don't drop items or leave bodies (death fog, lava, etc)
#if defined( QC )
	{ "nototem",	1,	0,	CONTENTS_NOTOTEM },		// don't drop totems here
#endif                                        // QC
	{"nonsolid",	1,	SURF_NONSOLID,	0},						// clears the solid flag

	// utility relevant attributes
	{"origin",		1,	0,	CONTENTS_ORIGIN },		// center of rotating brushes
	{"trans",		0,	0,	CONTENTS_TRANSLUCENT },	// don't eat contained surfaces
	{"detail",		0,	0,	CONTENTS_DETAIL },		// don't include in structural bsp
	{"structural",	0,	0,	CONTENTS_STRUCTURAL },	// force into structural bsp even if trnas
	{"areaportal",	1,	0,	CONTENTS_AREAPORTAL },	// divides areas
	{"clusterportal", 1,0,  CONTENTS_CLUSTERPORTAL },	// for bots
	{"donotenter",  1,  0,  CONTENTS_DONOTENTER },		// for bots

	{"fog",			1,	0,	CONTENTS_FOG},			// carves surfaces entering
	{"sky",			0,	SURF_SKY,		0 },		// emit light from an environment map
	{"lightfilter",	0,	SURF_LIGHTFILTER, 0 },		// filter light going through it
	{"alphashadow",	0,	SURF_ALPHASHADOW, 0 },		// test light on a per-pixel basis
	{"hint",		0,	SURF_HINT,		0 },		// use as a primary splitter

	// server attributes
	{"slick",		0,	SURF_SLICK,		0 },
	{"noimpact",	0,	SURF_NOIMPACT,	0 },		// don't make impact explosions or marks
	{"nomarks",		0,	SURF_NOMARKS,	0 },		// don't make impact marks, but still explode
	{"ladder",		0,	SURF_LADDER,	0 },
	{"nodamage",	0,	SURF_NODAMAGE,	0 },
	{"metalsteps",	0,	SURF_METALSTEPS,0 },
	{"flesh",		0,	SURF_FLESH,		0 },
	{"nosteps",		0,	SURF_NOSTEPS,	0 },

	// drawsurf attributes
	{"nodraw",		0,	SURF_NODRAW,	0 },	// don't generate a drawsurface (or a lightmap)
	{"pointlight",	0,	SURF_POINTLIGHT, 0 },	// sample lighting at vertexes
	{"nolightmap",	0,	SURF_NOLIGHTMAP,0 },	// don't generate a lightmap
	{"nodlight",	0,	SURF_NODLIGHT, 0 },		// don't ever add dynamic lights
	{"dust",		0,	SURF_DUST, 0}			// leave a dust trail when walking on this surface
};


// surfaceparm <name>

static void ParseSurfaceParm( const char** text )
{
	const char* token;
	int		numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	int		i;

	token = COM_ParseExt( text, qfalse );
	for ( i = 0 ; i < numInfoParms ; i++ ) {
		if ( !Q_stricmp( token, infoParms[i].name ) ) {
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
#if 0
			if ( infoParms[i].clearSolid ) {
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
			break;
		}
	}
}


// q3map_cnq3_depthFade <scale> <bias>

static void ParseDepthFade( const char** text )
{
	const char* token = COM_ParseExt( text, qfalse );
	float scale;
	if ( token[0] == '\0' || sscanf( token, "%f", &scale ) != 1 || scale <= 0.0f ) {
		ri.Printf( PRINT_WARNING,
				  "WARNING: invalid/missing depth fade scale argument '%s' in shader '%s'! "
				  "Ignoring the directive completely.\n",
				  token, shader.name );
		return;
	}

	token = COM_ParseExt( text, qfalse );
	float bias;
	if ( token[0] == '\0' || sscanf( token, "%f", &bias ) != 1 ) {
		ri.Printf( PRINT_WARNING,
				  "WARNING: invalid/missing depth fade bias argument '%s' in shader '%s'! "
				  "Ignoring the directive completely.\n",
				  token, shader.name );
		return;
	}

	shader.dfType = DFT_TBD;
	shader.dfInvDist = 1.0f / scale;
	shader.dfBias = bias;
}


// the current text pointer is at the explicit text definition of the shader.
// parse it into the global shader variable. later functions will optimize it.

static qbool ParseShader( const char** text )
{
	const char* token;
	int s = 0;

	token = COM_ParseExt( text, qtrue );
	if ( token[0] != '{' )
	{
		ri.Printf( PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name );
		return qfalse;
	}

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name );
			return qfalse;
		}

		// end of shader definition
		if ( token[0] == '}' )
		{
			break;
		}
		// stage definition
		else if ( token[0] == '{' )
		{
			if ( s >= MAX_SHADER_STAGES ) {
				ri.Error( ERR_DROP, "too many stages in shader %s\n", shader.name );
				return qfalse;
			}

			if ( !ParseStage( text, &stages[s] ) )
			{
				return qfalse;
			}
			stages[s].active = qtrue;
			s++;

			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if ( !Q_stricmpn( token, "qer", 3 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "deformVertexes" ) ) {
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "clampTime" ) ) {
			token = COM_ParseExt( text, qfalse );
			if (token[0]) {
				shader.clampTime = atof(token);
			}
		}
		else if ( !Q_stricmp( token, "q3map_cnq3_depthFade" ) ) {
			ParseDepthFade( text );
			continue;
		}
		// skip stuff that only the q3map needs
		else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if ( !Q_stricmp( token, "surfaceParm" ) ) {
			ParseSurfaceParm( text );
			continue;
		}
		// no mip maps
		else if ( !Q_stricmp( token, "nomipmaps" ) )
		{
			shader.imgflags |= IMG_NOMIPMAP | IMG_NOPICMIP;
			continue;
		}
		// no picmip adjustment
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			shader.imgflags |= IMG_NOPICMIP;
			continue;
		}
#if defined( QC )
		else if ( !Q_stricmp( token, "novlcollapse" ) )
		{
			shader.noVLCollapse = qtrue;
			continue;
		}
#endif
		// polygonOffset
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			shader.polygonOffset = qtrue;
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			shader.entityMergable = qtrue;
			continue;
		}
		// fogParms
		else if ( !Q_stricmp( token, "fogParms" ) )
		{
			if ( !ParseVector( text, 3, shader.fogParms.color ) ) {
				return qfalse;
			}

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name );
				continue;
			}
			shader.fogParms.depthForOpaque = atof( token );

			// skip any old gradient directions
			SkipRestOfLine( text );
			continue;
		}
		// portal
		else if ( !Q_stricmp(token, "portal") )
		{
			shader.sort = SS_PORTAL;
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if ( !Q_stricmp( token, "skyparms" ) )
		{
			ParseSkyParms( text );
			continue;
		}
		// light <value> determines flaring in q3map, not needed here
		else if ( !Q_stricmp(token, "light") )
		{
			token = COM_ParseExt( text, qfalse );
			continue;
		}
		// cull <face>
		else if ( !Q_stricmp( token, "cull") )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) )
			{
				shader.cullType = CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
			{
				shader.cullType = CT_BACK_SIDED;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		// sort
		else if ( !Q_stricmp( token, "sort" ) )
		{
			ParseSort( text );
			continue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	//
	// ignore shaders that don't have any stages, unless it is a sky or fog
	//
	if ( s == 0 && !shader.isSky && !(shader.contentFlags & CONTENTS_FOG ) ) {
		return qfalse;
	}

	shader.explicitlyDefined = qtrue;

	return qtrue;
}


static int R_CompareShaders( const void* aPtr, const void* bPtr )
{
	const shader_t* const a = *(const shader_t**)aPtr;
	const shader_t* const b = *(const shader_t**)bPtr;
	if ( a->sort < b->sort )
		return -1;
	if ( a->sort > b->sort )
		return 1;

	if ( a->polygonOffset ^ b->polygonOffset )
		return a->polygonOffset - b->polygonOffset;

	return a->cullType - b->cullType;
}


/*
Positions the most recently created shader in the tr.sortedShaders[] array
such that the shader->sort key is sorted relative to the other shaders.

Sets shader->sortedIndex
*/
static void SortNewShader()
{
	shader_t* newShader = tr.shaders[ tr.numShaders - 1 ];
	float sort = newShader->sort;

	int i;
	for ( i = tr.numShaders - 2 ; i >= 0 ; i-- ) {
		if ( tr.sortedShaders[ i ]->sort <= sort ) {
			break;
		}
		tr.sortedShaders[i+1] = tr.sortedShaders[i];
		tr.sortedShaders[i+1]->sortedIndex++;
	}

	newShader->sortedIndex = i+1;
	tr.sortedShaders[i+1] = newShader;

	// sort it more aggressively for better performance
	qsort( tr.sortedShaders, tr.numShaders, sizeof(shader_t*), &R_CompareShaders );
	for ( i = 0; i < tr.numShaders; ++i ) {
		tr.sortedShaders[i]->sortedIndex = i;
	}

	//
	// If we register a new shader when surfaces are already added,
	// the decoded sorted shader index for the added surfaces will not always be correct anymore.
	// This can lead to incorrect rendering (wrong shader used)
	// and potentially crashes too (lit surfaces referencing shaders with no diffuse stage).
	// We'll therefore update all surfaces that have already been added for this entire frame,
	// not just the last view. Hence why we don't use firstDrawSurf / firstLitSurf.
	// The extra CPU cost for the fix-up is nothing compared to loading new textures mid-frame.
	//

	int entityNum, fogNum;
	const shader_t* wrongShader;

	const int numDrawSurfs = tr.refdef.numDrawSurfs;
	drawSurf_t* drawSurfs = tr.refdef.drawSurfs;
	for( i = 0; i < numDrawSurfs; ++i, ++drawSurfs ) {
		R_DecomposeSort( drawSurfs->sort, &entityNum, &wrongShader, &fogNum );
		drawSurfs->sort = R_ComposeSort( entityNum, tr.shaders[drawSurfs->shaderNum], fogNum );
	}

	const int numLitSurfs = tr.refdef.numLitSurfs;
	litSurf_t* litSurfs = tr.refdef.litSurfs;
	for ( i = 0; i < numLitSurfs; ++i, ++litSurfs ) {
		R_DecomposeSort( litSurfs->sort, &entityNum, &wrongShader, &fogNum );
		litSurfs->sort = R_ComposeSort( entityNum, tr.shaders[litSurfs->shaderNum], fogNum );
	}
}


static shader_t* GeneratePermanentShader()
{
	if ( tr.numShaders == MAX_SHADERS ) {
		ri.Printf( PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	shader_t* newShader = RI_New<shader_t>();
	*newShader = shader;

	if ( shader.sort <= SS_OPAQUE ) {
		newShader->fogPass = FP_EQUAL;
	} else if ( shader.contentFlags & CONTENTS_FOG ) {
		newShader->fogPass = FP_LE;
	}

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for ( int i = 0; i < newShader->numStages; ++i ) {
		if ( !stages[i].active ) {
			newShader->numStages = i;
			break;
		}
		newShader->stages[i] = RI_New<shaderStage_t>();
		*newShader->stages[i] = stages[i];

		int n = newShader->stages[i]->numTexMods;
		newShader->stages[i]->texMods = RI_New<texModInfo_t>( n );
		Com_Memcpy( newShader->stages[i]->texMods, stages[i].texMods, n * sizeof( texModInfo_t ) );
	}

	SortNewShader();

	int hash = Q_FileHash(newShader->name, FILE_HASH_SIZE);
	newShader->next = hashTable[hash];
	hashTable[hash] = newShader;

	return newShader;
}


/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/


typedef struct {
	int			blendA;
	int			blendB;
	texEnv_t	multitextureEnv;
	int			multitextureBlend;
} collapse_t;

static const collapse_t collapse[] = {
	// the most common (and most worthwhile) collapse is for DxLM shaders
	{ 0, GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO,
		TE_MODULATE, 0 },

	{ 0, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		TE_MODULATE, 0 },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		TE_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		TE_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		TE_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		TE_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ 0, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
		TE_ADD, 0 },

	{ GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
		TE_ADD, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE },
#if 0
	{ 0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
		TE_DECAL, 0 },
#endif
	{ -1 }
};

/*
================
CollapseMultitexture

Attempt to combine two stages into a single multitexture stage
FIXME: I think modulated add + modulated add collapses incorrectly
=================
*/
static qbool CollapseMultitexture( void ) {
	int abits, bbits;
	int i;

	// make sure both stages are active
	if ( !stages[0].active || !stages[1].active ) {
		return qfalse;
	}

	abits = stages[0].stateBits;
	bbits = stages[1].stateBits;

	// make sure that both stages have identical state other than blend modes
	if ( ( abits & ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS | GLS_DEPTHMASK_TRUE ) ) !=
		( bbits & ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS | GLS_DEPTHMASK_TRUE ) ) ) {
		return qfalse;
	}

	abits &= ( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
	bbits &= ( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );

	// search for a valid multitexture blend function
	for ( i = 0; collapse[i].blendA != -1 ; i++ ) {
		if ( abits == collapse[i].blendA
			&& bbits == collapse[i].blendB ) {
			break;
		}
	}

	// nothing found
	if ( collapse[i].blendA == -1 ) {
		return qfalse;
	}

	// make sure waveforms have identical parameters
	if ( ( stages[0].rgbGen != stages[1].rgbGen ) ||
		( stages[0].alphaGen != stages[1].alphaGen ) )  {
		return qfalse;
	}

	// an add collapse can only have identity colors
	if ( collapse[i].multitextureEnv == TE_ADD && stages[0].rgbGen != CGEN_IDENTITY ) {
		return qfalse;
	}

	if ( stages[0].rgbGen == CGEN_WAVEFORM )
	{
		if ( memcmp( &stages[0].rgbWave,
					 &stages[1].rgbWave,
					 sizeof( stages[0].rgbWave ) ) )
		{
			return qfalse;
		}
	}
	if ( stages[0].alphaGen == AGEN_WAVEFORM )
	{
		if ( memcmp( &stages[0].alphaWave,
					 &stages[1].alphaWave,
					 sizeof( stages[0].alphaWave ) ) )
		{
			return qfalse;
		}
	}


	return qfalse;
/*
	// set the new blend state bits
	shader.multitextureEnv = collapse[i].multitextureEnv;
	stages[0].stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
	stages[0].stateBits |= collapse[i].multitextureBlend;

	//
	// move down subsequent shaders
	//
	memmove( &stages[1], &stages[2], sizeof( stages[0] ) * ( MAX_SHADER_STAGES - 2 ) );
	Com_Memset( &stages[MAX_SHADER_STAGES-1], 0, sizeof( stages[0] ) );

	return qtrue;
*/
}


/*
collapses can be a bit tricky, so we set a few simplifying groundrules:
first off, GL_REPLACE and GL_DECAL are almost completely pointless
since they're just subsets of GL_MODULATE. the only time they can ever
have value is in a multi-add collapse where one layer is modulated by
an rgbgen but the other needs to maintain identity colors
*/

#define GLS_BLEND_BITS (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)

static void CollapseStages()
{
	int i;
	// NEVER reference the global stages[] etc in here, only these locals
	int numStages = shader.numStages;
	shaderStage_t* aStages = &stages[0];

#define CollapseFailure { ++aStages; --numStages; continue; }

	while (numStages >= 2) {
		int abits = aStages[0].stateBits;
		int bbits = aStages[1].stateBits;

		if ( ( abits & ~(GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE) ) !=
			 ( bbits & ~(GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE) ) )
			CollapseFailure;

		if ( ( aStages[0].rgbGen != aStages[1].rgbGen ) || ( aStages[0].alphaGen != aStages[1].alphaGen ) )
			CollapseFailure;

		abits &= GLS_BLEND_BITS;
		bbits &= GLS_BLEND_BITS;

		for ( i = 0; collapse[i].blendA != -1; ++i ) {
			if ( (abits == collapse[i].blendA) && (bbits == collapse[i].blendB) ) {
				break;
			}
		}

		if ( collapse[i].blendA == -1 )
			CollapseFailure;

		// Check that all colors are opaque white on the second stage
		// because the stage iterator can't currently specify
		// another color array.
		// Example shader broken without this extra test:
		// "textures/sfx/diamond2cjumppad"
		// The ring pulses in and out instead of only out.

		// These cases must always be rejected because they depend
		// on time elapsed, camera position, entity colors, etc.
		if (	aStages[1].rgbGen == CGEN_LIGHTING_DIFFUSE ||
				aStages[1].rgbGen == CGEN_WAVEFORM ||
				aStages[1].rgbGen == CGEN_ENTITY ||
				aStages[1].rgbGen == CGEN_ONE_MINUS_ENTITY ||
				aStages[1].alphaGen == AGEN_WAVEFORM ||
				aStages[1].alphaGen == AGEN_LIGHTING_SPECULAR ||
				aStages[1].alphaGen == AGEN_ENTITY ||
				aStages[1].alphaGen == AGEN_ONE_MINUS_ENTITY ||
				aStages[1].alphaGen == AGEN_PORTAL )
			CollapseFailure;

		// For the remaining cases, we generate and test the colors.
		R_ComputeColors( &aStages[1], tess.svars[0], 0, tess.numVertexes );
		const int* colors = (const int*)tess.svars[0].colors;
		const int colorCount = tess.numVertexes;
		int allOnes = -1;
		for ( int c = 0; c < colorCount; ++c )
			allOnes &= colors[c];
		if ( allOnes != -1 )
			CollapseFailure;

		aStages[0].stateBits &= ~GLS_BLEND_BITS;
		aStages[0].stateBits |= collapse[i].multitextureBlend;
		aStages[1].mtEnv = collapse[i].multitextureEnv;
		aStages[0].mtStages = 1;
		aStages += 2;
		numStages -= 2;
	}

#undef CollapseFailure
}


static void FindLightingStages()
{
	int i;

	for ( i = 0; i < ST_MAX; ++i )
		shader.lightingStages[i] = -1;

	for ( i = 0; i < shader.numStages; ++i ) {
		stageType_t type = stages[i].type;
#if defined(_DEBUG)
		//if ( (shader.lightingStages[type] != -1) && (type != ST_DIFFUSE) )
		//	ri.Printf( PRINT_WARNING, "Duplicate stagetype %d in shader %s\n", type, shader.name );
#endif
		// the LAST at-least-partially-opaque layer is the one we want to use as the diffuse
		// because of things like the T4 weapon spawn points etc
		if (type == ST_DIFFUSE) {
			if (stages[i].tcGen != TCGEN_TEXTURE)
				continue;
			if ((stages[i].stateBits & GLS_BLEND_BITS) == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE))
				continue;
		}

		shader.lightingStages[ type ] = i;
	}
}


static qbool IsAdditiveBlendDepthFade()
{
	for (int i = 0; i < shader.numStages; ++i) {
		if (!stages[i].active)
			continue;

		const unsigned int bits = stages[i].stateBits;
		const unsigned int src = bits & GLS_SRCBLEND_BITS;
		const unsigned int dst = bits & GLS_DSTBLEND_BITS;
		if ((src != GLS_SRCBLEND_ONE && src != GLS_SRCBLEND_SRC_ALPHA && src != GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA) ||
		   dst != GLS_DSTBLEND_ONE ||
		   (bits & GLS_DEPTHMASK_TRUE) != 0)
		   return qfalse;
	}

	return qtrue;
}


static qbool IsNormalBlendDepthFade()
{
	for (int i = 0; i < shader.numStages; ++i) {
		if (!stages[i].active)
			continue;

		const unsigned int bits = stages[i].stateBits;
		const unsigned int src = bits & GLS_SRCBLEND_BITS;
		const unsigned int dst = bits & GLS_DSTBLEND_BITS;
		if (src != GLS_SRCBLEND_SRC_ALPHA ||
			dst != GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ||
			(bits & GLS_DEPTHMASK_TRUE) != 0)
			return qfalse;
	}

	return qtrue;
}


static qbool IsMultiplicativeBlendDepthFade()
{
	for (int i = 0; i < shader.numStages; ++i) {
		if (!stages[i].active)
			continue;

		const unsigned int bits = stages[i].stateBits;
		const unsigned int src = bits & GLS_SRCBLEND_BITS;
		const unsigned int dst = bits & GLS_DSTBLEND_BITS;
		const qbool multA = src == GLS_SRCBLEND_DST_COLOR && dst == GLS_DSTBLEND_ZERO;
		const qbool multB = src == GLS_SRCBLEND_ZERO && dst == GLS_DSTBLEND_SRC_COLOR;
		if ((!multA && !multB) ||
			(bits & GLS_DEPTHMASK_TRUE) != 0)
		return qfalse;
	}

	return qtrue;
}


static qbool IsPreMultAlphaBlendDepthFade()
{
	for (int i = 0; i < shader.numStages; ++i) {
		if (!stages[i].active)
			continue;

		const unsigned int bits = stages[i].stateBits;
		const unsigned int src = bits & GLS_SRCBLEND_BITS;
		const unsigned int dst = bits & GLS_DSTBLEND_BITS;
		if (src != GLS_SRCBLEND_ONE ||
			dst != GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ||
			(bits & GLS_DEPTHMASK_TRUE) != 0)
			return qfalse;
	}

	return qtrue;
}


static void ProcessDepthFade()
{
	struct ssShader {
		const char* name;
		float distance;
		float offset;
	};

	const ssShader ssShaders[] = {
		{ "rocketExplosion", 24.0f },
		{ "rocketExplosionNPM", 24.0f },
		{ "grenadeExplosion", 24.0f },
		{ "grenadeExplosionNPM", 24.0f },
		{ "grenadeCPMA_NPM", 24.0f },
		{ "grenadeCPMA", 24.0f },
		{ "bloodTrail", 24.0f },
		{ "sprites/particleSmoke", 24.0f },
		{ "plasmaExplosion", 8.0f, 4.0f },
		{ "plasmaExplosionNPM", 8.0f, 4.0f },
		{ "plasmanewExplosion", 8.0f, 4.0f },
		{ "plasmanewExplosionNPM", 8.0f, 4.0f },
		{ "bulletExplosion", 8.0f, 4.0f },
		{ "bulletExplosionNPM", 8.0f, 4.0f },
		{ "railExplosion", 8.0f },
		{ "railExplosionNPM", 8.0f },
		{ "bfgExplosion", 8.0f },
		{ "bfgExplosionNPM", 8.0f },
		{ "bloodExplosion", 8.0f },
		{ "bloodExplosionNPM", 8.0f },
		{ "smokePuff", 8.0f },
		{ "smokePuffNPM", 8.0f },
		{ "shotgunSmokePuff", 8.0f },
		{ "shotgunSmokePuffNPM", 8.0f }
	};

	const qbool shaderEnabled = shader.dfType == DFT_TBD;
	shader.dfType = DFT_NONE;

	if (!glInfo.depthFadeSupport)
	   return;

	if (shader.sort <= SS_OPAQUE)
		return;

	int activeStages = 0;
	for (int i = 0; i < shader.numStages; ++i) {
		if (stages[i].active)
			++activeStages;
	}

	if (activeStages <= 0)
		return;

	if (!shaderEnabled) {
		qbool found = qfalse;
		for (int i = 0; i < ARRAY_LEN(ssShaders); ++i) {
			if (!Q_stricmp(shader.name, ssShaders[i].name)) {
				shader.dfInvDist = 1.0f / ssShaders[i].distance;
				shader.dfBias = ssShaders[i].offset;
				found = qtrue;
				break;
			}
		}
		if (!found)
			return;
	}

	if (IsAdditiveBlendDepthFade())
		shader.dfType = DFT_ADD;
	else if (IsNormalBlendDepthFade())
		shader.dfType = DFT_BLEND;
	else if (IsMultiplicativeBlendDepthFade())
		shader.dfType = DFT_MULT;
	else if (IsPreMultAlphaBlendDepthFade())
		shader.dfType = DFT_PMA;
}


static void ProcessGreyscale()
{
	// maps: q3wcp1/5/9/14/22 ct3ctf1/2 cpmctf1/2 q3w5
	// I'll thank DEZ here for compiling most of this list
	const char* names[] =
	{
		"textures/ctf_unified/banner01_red",
		"textures/ctf_unified/bounce_red",
		"textures/ssctf5/s_red_xian_dm3padwall",
		"textures/base_light/light1red_2000",
		"textures/ctf_unified/plaque_shiny_red",
		"textures/ctf_unified/pointer_right_red",
		"textures/ssctf5/s_redbase",
		"textures/ctf_unified/pointer_left_red",
		"textures/ssctf5/s_bluebase",
		"textures/ctf_unified/bounce_blue",
		"textures/base_light/light1blue_5000",
		"textures/ctf_unified/weapfloor_blue",
		"textures/ctf_unified/banner01_blue",
		"textures/ctf_unified/pointer_left_blue",
		"textures/ctf_unified/pointer_right_blue",
		"textures/ctf_unified/weapfloor_red",
		"textures/base_light/light1blue_2000",
		"textures/ctf_unified/plaque_notshiny_blue",
		"textures/ctf_unified/direction_blue",
		"textures/ctf_unified/direction_red",
		"textures/ctf_unified/monologo_noflash_red",
		"textures/ctf_unified/wall_decal_red",
		"textures/ssctf3/s_beam_red",
		"textures/ctf_unified/banner02_red",
		"textures/ctf_unified/floor_decal_red",
		"textures/base_light/ceil1_22a_8k",
		"textures/ssctf3/s_beam_blue",
		"textures/ctf_unified/monologo_noflash_blue",
		"textures/ctf_unified/wall_decal_blue",
		"textures/ctf_unified/banner02_blue",
		"textures/ctf_unified/floor_decal_blue",
		"textures/base_light/ceil1_30_8k",
		"textures/ct3ctf1/jumppad_01_blue",
		"textures/base_light/ceil1_34",
		"textures/base_light/ceil1_22a",
		"textures/ct3ctf1/jumppad_01_red",
		"textures/base_light/light1red_5000",
		"textures/ninemil-ctf2/promode_logo_red",
		"textures/ninemil-ctf2/promode_logo_blue",
		"textures/ninemil-ctf2/bounce_red",
		"textures/ninemil-ctf2/bounce_blue",
		"textures/ninemil-ctf2/promode_logo_blue_large",
		"textures/ninemil-ctf2/test_blue",
		"textures/gothic_light/ironcrossltblue_5000",
		"textures/ninemil-ctf2/ctf_blueflag",
		"textures/ninemil-ctf2/test_red",
		"textures/gothic_light/ironcrossltred_5000",
		"textures/ninemil-ctf2/ctf_redflag",
		"textures/ninemil-ctf2/promode_logo_red_large",
		"textures/ninemil-ctf2/weapfloor_red",
		"textures/ninemil-ctf2/weapfloor_blue",
		"textures/japanc_q3w/Katana_r",
		"textures/japanc_q3w/trim_shadow_r",
		"textures/japanc_q3w/killblock_i2_small_r",
		"textures/japanc_q3w/pic1_r",
		"textures/japanc_q3w/drag_r",
		"textures/ctf_unified/pointer_red",
		"textures/ctf_unified/pointer_blue",
		"textures/japanc_q3w/drag_b",
		"textures/japanc_q3w/killblock_i2_small_b",
		"textures/japanc_q3w/Katana_b",
		"textures/japanc_q3w/trim_shadow_b",
		"textures/japanc_q3w/samurai_b",
		"textures/japanc_q3w/pic1_b",
		"textures/japanc_q3w/samurai_r",
		"textures/cpmctf1/banner2_b2",
		"textures/cpmctf1/banner2_r2",
		"textures/cpmctf1/metal_r",
		"textures/cpmctf1/metal_b",
		"textures/cpmctf1/trim_b",
		"textures/cpmctf1/banner1_b2",
		"textures/ctf/blocks18c_b",
		"textures/cpmctf1/trim_r",
		"textures/ctf/blocks18c_r",
		"textures/cpmctf1/banner1_r2",
		"textures/medieval/flr_marble1_c3trn_jp",
		"textures/medieval/flr_marble5_c3trn_jp",
		"textures/medieval/flr_marble1_c2trn",
		"textures/medieval/flr_marble5_c2trn",
		"textures/ssctf4/s_flameanim_blue",
		"textures/ssctf4/s_flameanim_red",
		"textures/ctf/ctf_tower_redfin_shiny",
		"textures/ctf/ctf_tower_bluefin_shiny",
		"textures/ctf/supportborder_blue",
		"textures/gothic_trim/supportborder",
		"textures/ssctf4/s_strucco_arch3",
		"textures/ctf_unified/banner03_blue",
		"textures/ssctf4/s_strucco_arch4",
		"textures/ssctf4/ssctf4_fogblue",
		"textures/ssctf4/ssctf4_fog",
		"textures/gothic_trim/baseboard08_ered",
		"textures/gothic_trim/baseboard08_dblue",
		"textures/gothic_trim/baseboard09_blue",
		"textures/gothic_trim/baseboard09_red",
		"textures/ninemil-ctf2/killblock_i2_r",
		"textures/ninemil-ctf2/killblock_i2_b",
		"textures/ctf_unified/plaque_shiny_blue",
		"textures/japanc_q3w/red_laq1",
		"textures/japanc_q3w/blue_laq1",
		"textures/sfx/xmetalfloor_wall_5b",
		"textures/sfx/xmetalfloor_wall_9b",
		"textures/sfx/xian_dm3padwall",
		"textures/crew/redwall",
		"textures/crew/bluewall",
		"textures/ctf_unified/monologo_flash_red",
		"textures/ctf_unified/monologo_flash_blue",
		"textures/crew/border11cx_r",
		"textures/crew/border11cx_b",
		"textures/ct_ct3ctf2/red_arrows_2",
		"textures/ct_ct3ctf2/blue_arrows_2",
		"textures/ct_ct3ctf2/jp_01_red",
		"textures/ct_ct3ctf2/jp_01",
		"textures/ct_ct3ctf2/red_pipe_liquid",
		"textures/ct_ct3ctf2/blue_pipe_liquid",
		"textures/ct_ct3ctf2/light_trim_red",
		"textures/ct_ct3ctf2/light_trim_blue",
		"textures/ct_ct3ctf2/red_jumppad_wall",
		"textures/ct_ct3ctf2/blue_jumppad_wall"
	};

	// This takes less than 80 us when loading cpm32_b1 on my old 2600K.
	// So no, we don't need a hash map.
	for (int i = 0; i < ARRAY_LEN(names); ++i) {
		if (!Q_stricmp(shader.name, names[i])) {
			shader.greyscaleCTF = qtrue;
			return;
		}
	}

	shader.greyscaleCTF = qfalse;
}


static qbool UsesInternalLightmap( const shaderStage_t* stage )
{
	return
		stage->active &&
		stage->type == ST_LIGHTMAP;
}


static qbool UsesExternalLightmap( const shaderStage_t* stage )
{
	return
		stage->active &&
		stage->type == ST_DIFFUSE &&
		!stage->bundle.isVideoMap &&
		stage->bundle.numImageAnimations <= 1 &&
		stage->bundle.image[0] != NULL &&
		(stage->bundle.image[0]->flags & IMG_EXTLMATLAS) != 0;
}


static int GetLightmapStageIndex()
{
	// look for "real" lightmaps first (straight from the .bsp file itself)
	for ( int i = 0; i < shader.numStages; ++i ) {
		if ( UsesInternalLightmap( &stages[i] ) )
			return i;
	}

	// look for external lightmaps next
	for ( int i = 0; i < shader.numStages; ++i ) {
		if ( UsesExternalLightmap( &stages[i] ) )
			return i;
	}

	return -1;
}


static qbool IsReplaceBlendMode( unsigned int stateBits )
{
	if ( (stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == 0 )
		return qtrue;

	if ( (stateBits & GLS_SRCBLEND_BITS) == GLS_SRCBLEND_ONE &&
		(stateBits & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_ZERO )
	   return qtrue;

	return qfalse;
}


static void ApplyVertexLighting()
{
	if (shader.sort > SS_OPAQUE)
		return;

	for (int stage = 0; stage < MAX_SHADER_STAGES; stage++) {
		shaderStage_t* const pStage = &stages[stage];
		if (UsesInternalLightmap(pStage) || UsesExternalLightmap(pStage)) {
			// keep the ST_LIGHTMAP type so that
			// dynamic lighting uses the right texture
			pStage->type = ST_LIGHTMAP;
			pStage->bundle.image[0] = tr.whiteImage;
			pStage->rgbGen = CGEN_EXACT_VERTEX;
			pStage->alphaGen = AGEN_SKIP;
			shader.vlApplied = qtrue;
		}
	}
}


static void BuildPerImageShaderList( shader_t* newShader )
{
	if ( newShader->isSky ) {
		image_t** const boxes[2] = { (image_t**)newShader->sky.outerbox, (image_t**)newShader->sky.innerbox };
		for ( int b = 0; b < 2; ++b ) {
			image_t** const images = boxes[b];
			for ( int i = 0; i < 6; ++i ) {
				R_AddImageShader( images[i], newShader );
			}
		}
	} else {
		for ( int s = 0; s < newShader->numStages; ++s ) {
			shaderStage_t* const newStage = newShader->stages[s];
			const int numImages = max( newStage->bundle.numImageAnimations, 1 );
			for ( int i = 0; i < numImages; ++i ) {
				R_AddImageShader( newStage->bundle.image[i], newShader );
			}
		}
	}
}


// returns a freshly allocated shader with info
// copied from the current global working shader

static shader_t* FinishShader()
{
	int stage;

	//
	// set polygon offset
	//
	if ( shader.polygonOffset && !shader.sort ) {
		shader.sort = SS_DECAL;
	}

	//
	// set appropriate stage information
	//
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		// check for a missing texture
		if ( !pStage->bundle.image[0] ) {
			ri.Printf( PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name );
			pStage->active = qfalse;
			continue;
		}

		//
		// ditch this stage if it's detail and detail textures are disabled
		//
		if ( pStage->isDetail && !r_detailTextures->integer ) {
			const int toMove = MAX_SHADER_STAGES - stage - 1;
			memmove( pStage, pStage + 1, sizeof( *pStage ) * toMove );
			Com_Memset( &stages[MAX_SHADER_STAGES - 1], 0, sizeof( *pStage ) );
			stage--;
			continue;
		}

		//
		// default texture coordinate generation
		//
		if ( pStage->type == ST_LIGHTMAP ) {
			if ( pStage->tcGen == TCGEN_BAD ) {
				pStage->tcGen = TCGEN_LIGHTMAP;
			}
		} else {
			if ( pStage->tcGen == TCGEN_BAD ) {
				pStage->tcGen = TCGEN_TEXTURE;
			}
		}

		//
		// determine sort order and fog color adjustment
		//
		if ( ( pStage->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) &&
			 ( stages[0].stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) ) {
			int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			// fog color adjustment only works for blend modes that have a contribution
			// that aproaches 0 as the modulate values aproach 0 --
			// GL_ONE, GL_ONE
			// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
			// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

			// modulate, additive
			if ( ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE ) ) ||
				( ( blendSrcBits == GLS_SRCBLEND_ZERO ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) ) {
				pStage->adjustColorsForFog = ACFF_MODULATE_RGB;
			}
			// strict blend
			else if ( ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
			}
			// premultiplied alpha
			else if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_RGBA;
			} else {
				// we can't adjust this one correctly, so it won't be exactly correct in fog
			}

			// don't screw with sort order if this is a portal or environment
			if ( !shader.sort ) {
				// see through item, like a grill or grate
				if ( pStage->stateBits & GLS_DEPTHMASK_TRUE ) {
					shader.sort = SS_SEE_THROUGH;
				} else {
					shader.sort = SS_BLEND0;
				}
			}
		}
	}

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if ( !shader.sort ) {
		shader.sort = SS_OPAQUE;
	}

	//
	// if we are in r_vertexLight mode, never use a lightmap texture
	//
#if defined( QC )
	if ( shader.vlWanted && !shader.noVLCollapse )
#else // QC
	if ( shader.vlWanted )
#endif // QC
		ApplyVertexLighting();

	//
	// look for multitexture potential
	//
	if ( stage > 1 && CollapseMultitexture() ) {
		stage--;
	}

	shader.numStages = stage;

	FindLightingStages();

	CollapseStages();

	if ( r_fullbright->integer ) {
		// we replace the lightmap texture with the white texture
		for ( int i = 0; i < shader.numStages; ++i ) {
			if ( UsesInternalLightmap( &stages[i] ) || UsesExternalLightmap( &stages[i] ) ) {
				stages[i].type = ST_DIFFUSE;
				stages[i].bundle.image[0] = tr.fullBrightImage;
			}
		}
	} else if (
		r_lightmap->integer &&
		( shader.contentFlags & CONTENTS_TRANSLUCENT ) == 0 &&
		shader.sort <= SS_OPAQUE ) {
		// we reduce it down to a single lightmap stage with the same state bits as
		// the current first stage (if the shader uses a lightmap at all)

		// @NOTE: we ignore all surfaces that aren't fully opaque because:
		// a) it's hard to know what level of uniform opacity would emulate the original look best
		// b) alpha-tested surfaces that have been turned into alpha-blended ones can't just use any given sort key:
		//    you can always find or design a case that won't work correctly
		// for decals entirely pressed against opaque surfaces, we could use a keyword ("polygonoffset" or something new)
		// to know that we should not draw them at all, but we just shouldn't trust level designers

		const int stageIndex = GetLightmapStageIndex();
		const int stateBits = stages[0].stateBits;
		if ( stageIndex > 0 )
			memcpy( stages, stages + stageIndex, sizeof( stages[0] ) );

		if ( stageIndex >= 0 ) {
			for ( int i = 1; i < shader.numStages; ++i ) {
				stages[i].active = qfalse;
			}
			stages[0].stateBits = stateBits;
			stages[0].mtStages = 0;
			shader.lightingStages[ST_DIFFUSE] = 0; // for working dynamic lights
			shader.lightingStages[ST_LIGHTMAP] = 0;
		}
	} else if ( r_lightmap->integer ) {
		// now we deal with r_lightmap on a non-opaque shader
		// first    stage must have: alphaFunc, depthWrite, blendFunc replace
		// lightmap stage must have: depthFunc equal
		// keep first stage as is, move lightmap stage as second stage, disable all others,
		// change lightmap stage to enforce blendFunc replace

		const int stageIndex = GetLightmapStageIndex();

		if ( stageIndex > 0 ) {
			const unsigned int firstBits = stages[0].stateBits;
			const unsigned int lightBits = stages[stageIndex].stateBits;

			if ( (firstBits & GLS_ATEST_BITS) != 0 &&
				(firstBits & GLS_DEPTHMASK_TRUE) != 0 &&
				IsReplaceBlendMode(firstBits) &&
				(lightBits & GLS_DEPTHFUNC_EQUAL) != 0 &&
				(lightBits & GLS_DEPTHTEST_DISABLE) == 0 ) {
				if ( stageIndex > 1 )
					memcpy( stages + 1, stages + stageIndex, sizeof(stages[0]) );

				stages[1].stateBits &= ~( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS );

				for ( int i = 2; i < shader.numStages; ++i ) {
					stages[i].active = qfalse;
				}

				shader.lightingStages[ST_DIFFUSE] = 0; // for working dynamic lights
				shader.lightingStages[ST_LIGHTMAP] = 0;
			}
		}
	}

	// non-sky fog-only shaders don't have any normal passes
	if ( !shader.isSky && stage == 0 ) {
		shader.sort = SS_FOG;
	}

	ProcessDepthFade();

	ProcessGreyscale();

	shader_t* const newShader = GeneratePermanentShader();

	BuildPerImageShaderList( newShader );

	return newShader;
}


///////////////////////////////////////////////////////////////


// searches the combined text of ALL shader files for the given shader name
// return the body of the shader if found, else NULL

static const char* FindShaderInShaderText( const char* shadername )
{
	const int hash = Q_FileHash( shadername, MAX_SHADERTEXT_HASH );

	// since the hash table always contains all loaded shaders
	// there's no need to actually scan through s_shaderText itself
	for (int i = 0; shaderTextHashTable[hash][i]; i++) {
		const char* p = shaderTextHashTable[hash][i];
		const char* const token = COM_ParseExt( &p, qtrue );

		if ( !Q_stricmp( token, shadername ) ) {
			return p;
		}
	}

	return NULL;
}


/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as appropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as appropriate for
most world construction surfaces.

===============
*/
shader_t* R_FindShader( const char *name, int lightmapIndex, int flags )
{
	char		strippedName[MAX_QPATH];
	char		fileName[MAX_QPATH];
	int			hash;
	shader_t	*sh;

	if ( name[0] == 0 ) {
		return tr.defaultShader;
	}

	// use (fullbright) vertex lighting if the bsp file doesn't have lightmaps
	if ( lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps )
		flags |= FINDSHADER_VERTEXLIGHT_BIT;

	COM_StripExtension(name, strippedName, sizeof(strippedName));

	hash = Q_FileHash(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( (sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
				!Q_stricmp(sh->name, strippedName)) {
			return sh;
		}
	}

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );
	Q_strncpyz(shader.name, strippedName, sizeof(shader.name));
	shader.lightmapIndex = lightmapIndex;
	for ( int i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		stages[i].texMods = texMods[i];
	}

	//
	// attempt to define shader from an explicit parameter file
	//
	const char* shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText ) {
#if 0 // enable this when building a pak file to get a global list of all explicit shaders
		ri.Printf( PRINT_ALL, "*SHADER* %s\n", name );
#endif

		const int textOffset = (int)( shaderText - s_shaderText );
		if ( !ParseShader( &shaderText ) ) {
			// had errors, so use default shader
			shader.defaultShader = qtrue;
		}
		if ( flags & FINDSHADER_VERTEXLIGHT_BIT ) {
			shader.vlWanted = qtrue;
		}
		sh = FinishShader();

		int fileIndex = -1;
		for ( int i = 0; i < s_numShaderFiles; ++i ) {
			if ( textOffset >= s_shaderFileOffsets[i] ) {
				fileIndex = i;
			}
		}
		sh->fileIndex = fileIndex;
		sh->text = s_shaderText + textOffset;

		return sh;
	}

	// if not defined in the in-memory shader descriptions,
	// look for a raw texture (saves needing shaders for trivial opaque surfs)
	//
	Q_strncpyz( fileName, name, sizeof( fileName ) );
	COM_DefaultExtension( fileName, sizeof( fileName ), ".tga" );

	image_t* image;
	if (flags & FINDSHADER_MIPRAWIMAGE_BIT)
		image = R_FindImageFile( fileName, 0, TW_REPEAT );
	else
		image = R_FindImageFile( fileName, IMG_NOMIPMAP | IMG_NOPICMIP, TW_CLAMP_TO_EDGE );

	if ( !image ) {
		ri.Printf( PRINT_DEVELOPER, "Couldn't find image for shader %s\n", name );
		shader.defaultShader = qtrue;
		return FinishShader();
	}

	// create the default shading commands

	stages[0].active = qtrue;
	stages[0].type = ST_DIFFUSE;
	stages[0].bundle.image[0] = image;

	if ( shader.lightmapIndex == LIGHTMAP_BROKEN ) {
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
		// dynamic colors at vertexes
		stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_2D ) {
		// GUI elements
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	} else if ( flags & FINDSHADER_VERTEXLIGHT_BIT ) {
		// explicit colors at vertexes
		stages[0].rgbGen = CGEN_EXACT_VERTEX;
		stages[0].alphaGen = AGEN_SKIP;
		stages[0].stateBits = GLS_DEFAULT;
		shader.vlWanted = qtrue;
		shader.vlApplied = qtrue;
	} else {
		// two pass lightmap
		stages[0].rgbGen = CGEN_IDENTITY;
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].active = qtrue;
		stages[1].type = ST_LIGHTMAP;
		stages[1].bundle.image[0] = tr.lightmaps[shader.lightmapIndex];
		stages[1].rgbGen = CGEN_IDENTITY;	// lightmaps are scaled on creation for identitylight
		stages[1].stateBits = GLS_DEFAULT | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}

	return FinishShader();
}


// KHB !!!  this code is stupid
// shaders registered from raw data should be "anonymous" and unsearchable
// because they don't have the supercession concept of "real" shaders

qhandle_t RE_RegisterShaderFromImage( const char* name, image_t* image )
{
	const shader_t* sh;

	// see if the shader is already loaded
	int hash = Q_FileHash(name, FILE_HASH_SIZE);
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ((sh->lightmapIndex == LIGHTMAP_2D || sh->defaultShader) && !Q_stricmp(sh->name, name)) {
			return sh->index;
		}
	}

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );
	Q_strncpyz(shader.name, name, sizeof(shader.name));
	shader.lightmapIndex = LIGHTMAP_2D;
	for (int i = 0; i < MAX_SHADER_STAGES; ++i) {
		stages[i].texMods = texMods[i];
	}

	// create the default shading commands: this can only ever be a 2D/UI shader
	stages[0].bundle.image[0] = image;
	stages[0].active = qtrue;
	stages[0].rgbGen = CGEN_VERTEX;
	stages[0].alphaGen = AGEN_VERTEX;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

	sh = FinishShader();
	return sh->index;
}


// we want to return 0 if the shader failed to load for some reason
// but R_FindShader should still keep a name allocated for it
// so we can fail quickly if something tries to register it again

static qhandle_t RE_RegisterShaderInternal( const char* name, int lightmapIndex, qbool mip )
{
	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterShader: name exceeds MAX_QPATH\n" );
		return 0;
	}

	int flags = 0;
	if ( mip )
		flags |= FINDSHADER_MIPRAWIMAGE_BIT;
	if ( r_vertexLight->integer )
		flags |= FINDSHADER_VERTEXLIGHT_BIT;
	const shader_t* sh = R_FindShader( name, lightmapIndex, flags );
	return sh->defaultShader ? 0 : sh->index;
}


/*
these are the exported shader entry points for the rest of the system
they always return a valid index, ie the default shader if there's a problem

should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
*/

qhandle_t RE_RegisterShader( const char* name )
{
	return RE_RegisterShaderInternal( name, LIGHTMAP_2D, qtrue );
}


// for menu graphics that should never be picmiped

qhandle_t RE_RegisterShaderNoMip( const char* name )
{
	return RE_RegisterShaderInternal( name, LIGHTMAP_2D, qfalse );
}


// when a handle is passed in by another module, this range checks
// it and returns a valid (possibly default) shader_t to be used internally

const shader_t* R_GetShaderByHandle( qhandle_t hShader )
{
	if ((hShader < 0) || (hShader >= tr.numShaders)) {
		ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}


// dump information on all valid shaders to the console
// a second parameter will cause it to print in sorted order

void R_ShaderList_f( void )
{
	const char* const match = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : NULL;

	ri.Printf( PRINT_ALL, "S   : Stage count\n" );
	ri.Printf( PRINT_ALL, "P   : Pass count (collapsed stages)\n" );
	ri.Printf( PRINT_ALL, "L   : Lightmap index\n" );
	ri.Printf( PRINT_ALL, "E   : Explicitly defined (i.e. defined by code)\n" );
	ri.Printf( PRINT_ALL, "func: 'sky' if it's a sky shader, empty otherwise\n" );
	ri.Printf( PRINT_ALL, "\n" );

	ri.Printf( PRINT_ALL, "S P L E func order   name \n" );

	int count = 0;
	for ( int i = 0 ; i < tr.numShaders ; i++ ) {
		const shader_t* sh = tr.sortedShaders[i];

		if ( match && !Com_Filter( match, sh->name ) )
			continue;

		int passes = sh->numStages;
		for ( int s = 0; s < sh->numStages; ++s )
			passes -= sh->stages[s]->mtStages;

		ri.Printf( PRINT_ALL, "%i %i ", sh->numStages, passes );

		if (sh->lightmapIndex >= 0 ) {
			ri.Printf( PRINT_ALL, "L " );
		} else {
			ri.Printf( PRINT_ALL, "  " );
		}
		if ( sh->explicitlyDefined ) {
			ri.Printf( PRINT_ALL, "E " );
		} else {
			ri.Printf( PRINT_ALL, "  " );
		}

		if ( sh->sort == SS_ENVIRONMENT ) {
			ri.Printf( PRINT_ALL, "sky " );
		} else {
			ri.Printf( PRINT_ALL, "    " );
		}

		ri.Printf( PRINT_ALL, "%5.2f ", sh->sort );

		if ( sh->defaultShader ) {
			ri.Printf( PRINT_ALL,  ": %s (DEFAULTED)\n", sh->name );
		} else {
			ri.Printf( PRINT_ALL,  ": %s\n", sh->name );
		}
		count++;
	}

	ri.Printf( PRINT_ALL, "%i shaders found\n", count );
	ri.Printf( PRINT_ALL, "--------------------\n" );
}


static void AutoCompleteShaderName( fieldCallback_t callback )
{
	for ( int i = 0; i < tr.numShaders; i++ ) {
		callback( tr.shaders[i]->name );
	}
}


void R_CompleteShaderName_f( int startArg, int compArg )
{
	if ( startArg + 1 == compArg )
		Field_AutoCompleteCustom( startArg, compArg, &AutoCompleteShaderName );
}


void R_ShaderInfo_f()
{
	if ( Cmd_Argc() <= 1 ) {
		ri.Printf( PRINT_ALL, "usage: %s <shadername> [code]\n", Cmd_Argv(0) );
		return;
	}

	const char* const name = Cmd_Argv(1);
	const shader_t* sh = NULL;
	for ( int i = 0; i < tr.numShaders; i++ ) {
		if ( !Q_stricmp( tr.shaders[i]->name, name ) ) {
			sh = tr.shaders[i];
			break;
		}
	}

	if ( sh == NULL ) {
		ri.Printf( PRINT_ALL, "shader not found\n" );
		return;
	}

	if ( sh->text == NULL ) {
		const char* type;
		if ( sh->vlApplied ) {
			type = "vertex-lit surface";
		} else {
			switch ( sh->lightmapIndex ) {
				case LIGHTMAP_BROKEN:    type = "broken lit surface"; break;
				case LIGHTMAP_2D:        type = "UI element";         break;
				case LIGHTMAP_NONE:      type = "opaque surface";     break;
				default:                 type = "lit surface";        break;
			}
		}
		ri.Printf( PRINT_ALL, "shader has no code (type: %s)\n", type );
		return;
	}

	const char* const shaderPath = R_GetShaderPath( sh );
	if ( shaderPath != NULL ) {
		ri.Printf( PRINT_ALL, "%s\n", shaderPath );
	}

	if ( Q_stricmp( Cmd_Argv(2), "code" ) ) {
		return;
	}

	const char* s = sh->text;
	int tabs = 0;
	for ( ;; ) {
		const char c0 = s[0];
		const char c1 = s[1];
		if ( c0 == '{' ) {
			tabs++;
			ri.Printf( PRINT_ALL, "{" );
		} else if ( c0 == '\n' ) {
			ri.Printf( PRINT_ALL, "\n" );
			if ( c1 == '}' ) {
				tabs--;
				if ( tabs == 0 ) {
					ri.Printf( PRINT_ALL, "}\n" );
					return;
				}
			}
			for( int i = 0; i < tabs; i++ ) {
				ri.Printf( PRINT_ALL, "    " );
			}
		} else {
			ri.Printf( PRINT_ALL, "%c", c0 );
		}
		s++;
	}
}


void R_ShaderMixedUse_f()
{
	for ( int i = 0; i < tr.numImages; ++i ) {
		image_t* const image = tr.images[i];
		if ( image->numShaders < 2 || !Q_stricmp(image->name, "*white") ) {
			continue;
		}

		const int mixedFlags = image->flags0 & image->flags1;
		if ( ( mixedFlags & ( IMG_NOMIPMAP | IMG_NOPICMIP ) ) == 0 ) {
			continue;
		}

		ri.Printf( PRINT_ALL, "^5%s:\n", image->name );
		for ( int is = 0; is < ARRAY_LEN( tr.imageShaders ); ++is ) {
			const int imageIndex = tr.imageShaders[is] & 0xFFFF;
			if ( imageIndex != i ) {
				continue;
			}

			const int s = (tr.imageShaders[is] >> 16) & 0xFFFF;
			const shader_t* const sh = tr.shaders[s];
			const qbool nmmS = sh->imgflags & IMG_NOMIPMAP;
			const qbool npmS = sh->imgflags & IMG_NOPICMIP;
			ri.Printf( PRINT_ALL, "%s %s %s\n",
					  nmmS ? "NMM" : "   ",
					  npmS ? "NPM" : "   ",
					  sh->name);

			const char* const shaderPath = R_GetShaderPath( sh );
			if ( shaderPath != NULL ) {
				ri.Printf( PRINT_ALL, "        -> %s\n", shaderPath );
			}
		}
	}
}


// finds and loads all .shader files, combining them into
// a single large text block that can be scanned for shader names
// note that this does a lot of things very badly, e.g. still loads superceded shaders

static void ScanAndLoadShaderFiles()
{
	static const int MAX_SHADER_FILES = 4096;
	char* buffers[MAX_SHADER_FILES];
	int len[MAX_SHADER_FILES];

	int i;
	char* p;

	int numShaderFiles;
	char** shaderFileNames = ri.FS_ListFiles( "scripts", ".shader", &numShaderFiles );

	if ( !shaderFileNames || !numShaderFiles )
	{
		ri.Printf( PRINT_WARNING, "WARNING: no shader files found\n" );
		return;
	}

	if ( numShaderFiles > MAX_SHADER_FILES )
		ri.Error( ERR_DROP, "Shader file limit exceeded" );

	s_shaderFileOffsets = RI_New<int>( numShaderFiles );
	s_shaderFileNameOffsets = RI_New<int>( numShaderFiles );
	s_shaderPakChecksums = RI_New<int>( numShaderFiles );
	s_numShaderFiles = numShaderFiles;

	long sum = 0;
	long sumNames = 0;
	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ )
	{
		char filename[MAX_QPATH];
		Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFileNames[i] );
		ri.FS_ReadFilePak( filename, (void **)&buffers[i], &s_shaderPakChecksums[i] );
		if ( !buffers[i] )
			ri.Error( ERR_DROP, "Couldn't load %s", filename );
		len[i] = COM_Compress( buffers[i] );
		sum += len[i];
		sumNames += strlen( shaderFileNames[i] ) + 1;
	}

	s_shaderText = RI_New<char>( sum + numShaderFiles + 1 );
	s_shaderFileNames = RI_New<char>( sumNames );

	char* s = s_shaderFileNames;
	for ( i = 0; i < numShaderFiles; i++ ) {
		s_shaderFileNameOffsets[i] = (int)( s - s_shaderFileNames );
		const int l = strlen( shaderFileNames[i] );
		Com_Memcpy( s, shaderFileNames[i], l );
		s += l;
		*s++ = '\0';
	}

	s = s_shaderText;
	for ( i = 0; i < numShaderFiles; i++ ) {
		s_shaderFileOffsets[i] = (int)( s - s_shaderText );
		Com_Memcpy( s, buffers[i], len[i] );
		s += len[i];
		*s++ = '\n';
	}
	*s = '\0';

	// the files have to be freed backwards because the hunk isn't a real MM
	for (i = numShaderFiles - 1; i >= 0; --i)
		ri.FS_FreeFile( buffers[i] );

	ri.FS_FreeFileList( shaderFileNames );

	int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH];
	Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));

	const char* token;
	int size = 0, hash;

	p = s_shaderText;
	while (p < s) {
		token = COM_ParseExt( (const char**)&p, qtrue );
		if ( token[0] == 0 )
			break;
		hash = Q_FileHash( token, MAX_SHADERTEXT_HASH );
		shaderTextHashTableSizes[hash]++;
		size++;
		SkipBracedSection( (const char**)&p );
	}

	size += MAX_SHADERTEXT_HASH;
	char** hashMem = RI_New<char*>( size );

	for (i = 0; i < MAX_SHADERTEXT_HASH; i++) {
		shaderTextHashTable[i] = hashMem;
		hashMem += (shaderTextHashTableSizes[i] + 1);
	}

	Com_Memset( shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes) );

	p = s_shaderText;
	while (p < s) {
		char* oldp = p;
		token = COM_ParseExt( (const char**)&p, qtrue );
		if ( token[0] == 0 )
			break;
		hash = Q_FileHash( token, MAX_SHADERTEXT_HASH );
		shaderTextHashTable[hash][shaderTextHashTableSizes[hash]++] = oldp;
		SkipBracedSection( (const char**)&p );
	}
}


static void CreateInternalShaders()
{
	tr.numShaders = 0;

	// init the default shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );

	Q_strncpyz( shader.name, "<default>", sizeof( shader.name ) );
	shader.lightmapIndex = LIGHTMAP_NONE;
	stages[0].bundle.image[0] = tr.defaultImage;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	tr.defaultShader = FinishShader();

	Q_strncpyz( shader.name, "<scratch>", sizeof( shader.name ) );
	tr.scratchShader = FinishShader();
}


void R_InitShaders()
{
	ri.Printf( PRINT_ALL, "Initializing Shaders\n" );

	Com_Memset( hashTable, 0, sizeof(hashTable) );

	CreateInternalShaders();

	ScanAndLoadShaderFiles();
}


const char* R_GetShaderPath( const shader_t* sh )
{
	const int fileIndex = sh->fileIndex;
	if ( fileIndex < 0 || fileIndex >= s_numShaderFiles ) {
		return NULL;
	}

	const int nameOffset = s_shaderFileNameOffsets[fileIndex];
	const char* const fileName = s_shaderFileNames + nameOffset;

	char pakName[256];
	const int pakChecksum = s_shaderPakChecksums[fileIndex];
	if( FS_GetPakPath( pakName, sizeof(pakName), pakChecksum ) ) {
		return va( "%s/scripts/%s", pakName, fileName );
	}

	return va( "scripts/%s", fileName );
}
