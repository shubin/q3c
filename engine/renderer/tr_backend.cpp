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

static int64_t startTime;


static void RB_Set2D()
{
	backEnd.projection2D = qtrue;
	backEnd.pc = backEnd.pc2D;

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = (double)backEnd.refdef.time / 1000.0;

	gal.Begin2D();
}


static const void* RB_SetColor( const void* data )
{
	const setColorCommand_t* cmd = (const setColorCommand_t*)data;

	backEnd.color2D[0] = (byte)(cmd->color[0] * 255);
	backEnd.color2D[1] = (byte)(cmd->color[1] * 255);
	backEnd.color2D[2] = (byte)(cmd->color[2] * 255);
	backEnd.color2D[3] = (byte)(cmd->color[3] * 255);

	return (const void*)(cmd + 1);
}


static const void* RB_StretchPic( const void* data )
{
	const stretchPicCommand_t* cmd = (const stretchPicCommand_t*)data;

	if ( !backEnd.projection2D )
		RB_Set2D();

	const shader_t* shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
		tess.greyscale = 0.0f;
	}

	RB_CHECKOVERFLOW( 4, 6 );
	int numVerts = tess.numVertexes;
	int numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	*(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] =
		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0] = cmd->s1;
	tess.texCoords[ numVerts ][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][1] = cmd->t2;

	return (const void*)(cmd + 1);
}

static const void* RB_Triangle( const void* data )
{
	const triangleCommand_t* cmd = (const triangleCommand_t*)data;

	if ( !backEnd.projection2D )
		RB_Set2D();

	const shader_t* shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
		tess.greyscale = 0.0f;
	}

	RB_CHECKOVERFLOW( 3, 3 );
	int numVerts = tess.numVertexes;
	int numIndexes = tess.numIndexes;

	tess.numVertexes += 3;
	tess.numIndexes += 3;

	tess.indexes[ numIndexes + 0 ] = numVerts + 0;
	tess.indexes[ numIndexes + 1 ] = numVerts + 1;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;

	*(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] = *(int *)backEnd.color2D;

	tess.xyz[ numVerts ][0] = cmd->x0;
	tess.xyz[ numVerts ][1] = cmd->y0;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0] = cmd->s0;
	tess.texCoords[ numVerts ][1] = cmd->t0;

	tess.xyz[ numVerts + 1 ][0] = cmd->x1;
	tess.xyz[ numVerts + 1 ][1] = cmd->y1;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0] = cmd->s1;
	tess.texCoords[ numVerts + 1 ][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x2;
	tess.xyz[ numVerts + 2 ][1] = cmd->y2;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][1] = cmd->t2;

	return (const void*)(cmd + 1);
}


static qbool AreShadersStillBatchable( const shader_t* a, const shader_t* b )
{
	if ( a->lightmapIndex != b->lightmapIndex ||
		a->sort != b->sort ||
		a->fogPass != FP_NONE ||
		b->fogPass != FP_NONE ||
		a->cullType != b->cullType ||
		a->polygonOffset != b->polygonOffset ||
		a->imgflags != b->imgflags ||
		a->numStages != b->numStages ||
		a->dfType != b->dfType )
		return qfalse;

	for ( int i = 0; i < a->numStages; ++i ) {
		const shaderStage_t* const sa = a->stages[i];
		const shaderStage_t* const sb = b->stages[i];
		if ( sa->active != sb->active ||
			sa->type != ST_DIFFUSE ||
			sb->type != ST_DIFFUSE ||
			sa->stateBits != sb->stateBits ||
			sa->type != sb->type ||
			sa->tcGen != sb->tcGen ||
			sa->mtStages != sb->mtStages ||
			sa->bundle.isVideoMap != qfalse ||
			sb->bundle.isVideoMap != qfalse ||
			sa->bundle.image[0] != sb->bundle.image[0] )
			return qfalse;
	}

	return qtrue;
}


static void RB_RenderDrawSurfList( const drawSurf_t* drawSurfs, int numDrawSurfs, qbool beginView )
{
	int i;
	const shader_t* shader = NULL;
	unsigned int sort = (unsigned int)-1;

	// save original time for entity shader offsets
	double originalTime = backEnd.refdef.floatTime;

	// we will need to change the projection matrix before drawing 2D images again
	backEnd.projection2D = qfalse;
	backEnd.pc = backEnd.pc3D;

	if ( beginView )
		gal.Begin3D();

	// draw everything
	int oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	qbool oldDepthRange = qfalse;
	qbool depthRange = qfalse;

	backEnd.pc[RB_SURFACES] += numDrawSurfs;

	const drawSurf_t* drawSurf;
	for ( i = 0, drawSurf = drawSurfs; i < numDrawSurfs; ++i, ++drawSurf ) {
		if ( drawSurf->sort == sort ) {
			// fast path, same as previous sort
			const int firstVertex = tess.numVertexes;
			const int firstIndex = tess.numIndexes;
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			if ( tess.deformsPreApplied ) {
				// across multiple shaders though, so we need to compute all the results now
				const int numVertexes = tess.numVertexes - firstVertex;
				const int numIndexes = tess.numIndexes - firstIndex;
				RB_DeformTessGeometry( firstVertex, numVertexes, firstIndex, numIndexes );
				for ( int s = 0; s < shader->numStages; ++s ) {
					R_ComputeColors( shader->stages[s], tess.svars[s], firstVertex, numVertexes );
					R_ComputeTexCoords( shader->stages[s], tess.svars[s], firstVertex, numVertexes, qfalse );
				}
			}
			continue;
		}

		int fogNum;
		const shader_t* shaderPrev = shader;
		int entityNum;
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum );
		const qbool depthFadeChange = shader->dfType != tess.depthFade;

		// detect and batch surfaces across different (but sufficiently similar) shaders
		if ( !depthFadeChange &&
			oldEntityNum == ENTITYNUM_WORLD &&
			entityNum == ENTITYNUM_WORLD &&
			AreShadersStillBatchable( shaderPrev, shader ) ) {
			if ( !tess.deformsPreApplied ) {
				// this is the second shader in the sequence,
				// so we need to compute everything added with the first one now
				tess.shader = shaderPrev;
				RB_DeformTessGeometry( 0, tess.numVertexes, 0, tess.numIndexes );
				for ( int s = 0; s < shaderPrev->numStages; ++s ) {
					R_ComputeColors( shaderPrev->stages[s], tess.svars[s], 0, tess.numVertexes );
					R_ComputeTexCoords( shaderPrev->stages[s], tess.svars[s], 0, tess.numVertexes, qfalse );
				}
			}
			tess.shader = shader;
			tess.deformsPreApplied = qtrue;
			const int firstVertex = tess.numVertexes;
			const int firstIndex = tess.numIndexes;
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			const int numVertexes = tess.numVertexes - firstVertex;
			const int numIndexes = tess.numIndexes - firstIndex;
			RB_DeformTessGeometry( firstVertex, numVertexes, firstIndex, numIndexes );
			for ( int s = 0; s < shader->numStages; ++s ) {
				R_ComputeColors( shader->stages[s], tess.svars[s], firstVertex, numVertexes );
				R_ComputeTexCoords( shader->stages[s], tess.svars[s], firstVertex, numVertexes, qfalse );
			}
			sort = drawSurf->sort;
			oldEntityNum = entityNum;
			continue;
		}

		// "entityMergable" shaders can have surfaces from multiple refentities
		// merged into a single batch, like (CONCEPTUALLY) smoke and blood puff sprites
		// only legacy code still uses them though, because refents are so heavyweight:
		// modern code just billboards in cgame and submits raw polys, all of which are
		// ENTITYNUM_WORLD and thus automatically take the "same sort" fast path

		if ( !shader->entityMergable || ((sort ^ drawSurf->sort) & ~QSORT_ENTITYNUM_MASK) || depthFadeChange ) {
			if (shaderPrev)
				RB_EndSurface();
			RB_BeginSurface( shader, fogNum );
			tess.depthFade = shader->dfType;
			tess.greyscale = drawSurf->greyscale;
		}

		sort = drawSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = qfalse;

			if ( entityNum != ENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				if (backEnd.currentEntity->intShaderTime)
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.iShaderTime / 1000.0;
				else
				    backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime.fShaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.orient );

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.orient = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
			}

			gal.SetModelViewMatrix( backEnd.orient.modelMatrix );

			//
			// change depthrange if needed
			//
			if ( oldDepthRange != depthRange ) {
				if ( depthRange ) {
					gal.SetDepthRange( 0, 0.3 );
				} else {
					gal.SetDepthRange( 0, 1 );
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (shader) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	gal.SetModelViewMatrix( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		gal.SetDepthRange( 0, 1 );
	}
}


static void RB_RenderLitSurfList( dlight_t* dl, qbool opaque )
{
	if (!dl->head)
		return;

	const shader_t* shader = NULL;

	int				entityNum, oldEntityNum;
	qbool			depthRange, oldDepthRange;
	unsigned int sort = (unsigned int)-1;

	// save original time for entity shader offsets
	double originalTime = backEnd.refdef.floatTime;

	// draw everything
	const int liquidFlags = CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER;
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.dlOpaque = opaque;
	oldDepthRange = qfalse;
	depthRange = qfalse;
	tess.light = dl;

	for ( litSurf_t* litSurf = dl->head; litSurf; litSurf = litSurf->next ) {
		++backEnd.pc[RB_LIT_SURFACES];
		if ( litSurf->sort == sort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
			continue;
		}

		int fogNum;
		const shader_t* shaderPrev = shader;
		R_DecomposeSort( litSurf->sort, &entityNum, &shader, &fogNum );

		if (opaque && shader->sort > SS_OPAQUE)
			continue;

		if (!opaque && shader->sort <= SS_OPAQUE)
			continue;

		const int stageIndex = shader->lightingStages[ST_DIFFUSE];
		if (stageIndex < 0 || stageIndex >= shader->numStages)
			continue;

		if (shaderPrev)
			RB_EndSurface();
		RB_BeginSurface( shader, fogNum );
		tess.greyscale = litSurf->greyscale;

		const shaderStage_t* const stage = shader->stages[stageIndex];
		backEnd.dlIntensity = (shader->contentFlags & liquidFlags) != 0 ? 0.5f : 1.0f;
		backEnd.dlStateBits =
			(opaque || (stage->stateBits & GLS_ATEST_BITS) != 0) ?
			(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL):
			(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);

		sort = litSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = qfalse;

			if ( entityNum != ENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				if (backEnd.currentEntity->intShaderTime)
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.iShaderTime / 1000.0;
				else
                    backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime.fShaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.orient );

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.orient = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				R_TransformDlights( 1, dl, &backEnd.orient );
			}

			R_TransformDlights( 1, dl, &backEnd.orient );
			gal.BeginDynamicLight();

			gal.SetModelViewMatrix( backEnd.orient.modelMatrix );

			//
			// change depthrange if needed
			//
			if ( oldDepthRange != depthRange ) {
				if ( depthRange ) {
					gal.SetDepthRange( 0, 0.3 );
				} else {
					gal.SetDepthRange( 0, 1 );
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (shader) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	gal.SetModelViewMatrix( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		gal.SetDepthRange( 0, 1 );
	}
}


static void R_DebugPolygon( int colorMask, int numPoints, const float* points )
{
	RB_PushSingleStageShader( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, CT_FRONT_SIDED );
	shaderStage_t& stage = *tess.shader->stages[0];

	// solid
	for ( int i = 0; i < numPoints; ++i ) {
		tess.xyz[i][0] = points[i * 3 + 0];
		tess.xyz[i][1] = points[i * 3 + 1];
		tess.xyz[i][2] = points[i * 3 + 2];
		tess.xyz[i][3] = 1.0f;
	}
	for ( int i = 1, n = 0; i < numPoints - 1; ++i ) {
		tess.indexes[n++] = 0;
		tess.indexes[n++] = i + 0;
		tess.indexes[n++] = i + 1;
	}
	tess.numVertexes = numPoints;
	tess.numIndexes = (numPoints - 2) * 3;	
	stage.rgbGen = CGEN_CONST;
	stage.constantColor[0] =  (colorMask       & 1) ? 255 : 0;
	stage.constantColor[1] = ((colorMask >> 1) & 1) ? 255 : 0;
	stage.constantColor[2] = ((colorMask >> 2) & 1) ? 255 : 0;
	stage.constantColor[3] = 255;
	R_ComputeColors( &stage, tess.svars[0], 0, numPoints );
	gal.Draw( DT_GENERIC );

	// wireframe
	for ( int i = 0, n = 0; i < numPoints; ++i ) {
		tess.indexes[n++] = i;
		tess.indexes[n++] = i;
		tess.indexes[n++] = (i + 1) % numPoints;
	}
	tess.numIndexes = numPoints * 3;
	stage.stateBits |= GLS_POLYMODE_LINE;
	stage.rgbGen = CGEN_IDENTITY;
	R_ComputeColors( &stage, tess.svars[0], 0, numPoints );
	gal.SetDepthRange( 0, 0 );
	gal.Draw( DT_GENERIC );
	gal.SetDepthRange( 0, 1 );

	RB_PopShader();
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}


static const void* RB_DrawSurfs( const void* data )
{
	const drawSurfsCommand_t* cmd = (const drawSurfsCommand_t*)data;

	// finish any 2D drawing if needed
	if ( tess.numIndexes )
		RB_EndSurface();

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	const int opaqueCount = cmd->numDrawSurfs - cmd->numTranspSurfs;
	const int transpCount = cmd->numTranspSurfs;

	tess.pass = shaderCommands_t::TP_BASE;
	RB_RenderDrawSurfList( cmd->drawSurfs, opaqueCount, qtrue );

	tess.pass = shaderCommands_t::TP_LIGHT;
	for ( int i = 0; i < backEnd.refdef.num_dlights; ++i ) {
		RB_RenderLitSurfList( &backEnd.refdef.dlights[i], qtrue );
	}

	tess.pass = shaderCommands_t::TP_BASE;
	RB_RenderDrawSurfList( cmd->drawSurfs + opaqueCount, transpCount, qfalse );

	tess.pass = shaderCommands_t::TP_LIGHT;
	for ( int i = 0; i < backEnd.refdef.num_dlights; ++i ) {
		RB_RenderLitSurfList( &backEnd.refdef.dlights[i], qfalse );
	}

	tess.pass = shaderCommands_t::TP_BASE;

	// draw main system development information (surface outlines, etc)
	if ( r_debugSurface->integer )
		ri.CM_DrawDebugSurface( R_DebugPolygon );

	return (const void*)(cmd + 1);
}


static const void* RB_BeginFrame( const void* data )
{
	const beginFrameCommand_t* cmd = (const beginFrameCommand_t*)data;

	R_SetColorMappings();
	gal.BeginFrame();

	return (const void*)(cmd + 1);
}


static const void* RB_SwapBuffers( const void* data )
{
	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// This has been moved here to make sure the Present/SwapBuffer
	// call gets ignored for CPU timing as V-Sync would mess it all up.
	// We can't really "charge" 2D/3D properly, so it all counts as 3D.
	const int64_t endTime = ri.Microseconds();
	backEnd.pc3D[RB_USEC] = (int)( endTime - startTime );

	const swapBuffersCommand_t* cmd = (const swapBuffersCommand_t*)data;
	gal.EndFrame();
	Sys_V_EndFrame();
	const int64_t swapTime = ri.Microseconds();
	backEnd.pc3D[RB_USEC_END] = (int)( swapTime - endTime );
	backEnd.projection2D = qfalse;
	backEnd.pc = backEnd.pc3D;

	return (const void*)(cmd + 1);
}


void RB_ExecuteRenderCommands( const void *data )
{
	startTime = ri.Microseconds();

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_TRIANGLE:
			data = RB_Triangle( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_BEGIN_FRAME:
			data = RB_BeginFrame( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd( (const screenshotCommand_t*)data );
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd( data );
			break;

		case RC_END_OF_LIST:
		default:
			return;
		}
	}
}


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
	const float c = 0.25f + 0.5f * sinf(M_PI * (backEnd.refdef.time & 0x01FF) / 0x0200);

	return c;
}
