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


#define RC(Enum, Type) (int)sizeof(Type),
const int renderCommandSizes[RC_COUNT + 1] =
{
	RENDER_COMMAND_LIST(RC)
	0
};
#undef RC

// we reserve space for frame ending commands as well as transition commands (begin/end 2D/3D rendering)
static const int CmdListReservedBytes = (int)(sizeof(swapBuffersCommand_t) + sizeof(screenshotCommand_t) + 16 * sizeof(void*));


static qbool R_IsReadbackRequested( readbackCommands_t* readback )
{
	Q_assert( readback );

	return
		readback->screenshot.requested != 0 ||
		readback->videoFrame.requested != 0;
}


void R_IssueRenderCommands()
{
	// add an end-of-list command
	renderCommandList_t* cmdList = &backEndData->commands;
	((renderCommandBase_t*)(cmdList->cmds + cmdList->used))->commandId = RC_END_OF_LIST;

	// clear it out, in case this is a sync and not a buffer flip
	cmdList->used = 0;

	// process render commands
	readbackCommands_t* const readback = &backEndData->readbackCommands;
	renderPipeline->ExecuteRenderCommands( cmdList->cmds, R_IsReadbackRequested( readback ) );

	// process readback commands
	if ( readback->screenshot.requested ) {
		RB_TakeScreenshotCmd( &readback->screenshot );
	}
	if ( readback->videoFrame.requested ) {
		RB_TakeVideoFrameCmd( &readback->videoFrame );
	}
	readback->screenshot.requested = qfalse;
	readback->videoFrame.requested = qfalse;
}


static qbool CanAllocateRenderCommand( int bytes, qbool endFrame )
{
	const int endOffset = endFrame ? 0 : CmdListReservedBytes;
	const renderCommandList_t* const cmdList = &backEndData->commands;
	if ( cmdList->used + bytes + endOffset > MAX_RENDER_COMMANDS ) {
		return qfalse;
	}

	return qtrue;
}


template<typename T>
static qbool CanAllocateRenderCommand( qbool endFrame = qfalse )
{
	return CanAllocateRenderCommand( sizeof(T), endFrame );
}


byte* R_AllocateRenderCommand( int bytes, int commandId, qbool endFrame )
{
	Q_assert( bytes % 8 == 0 );

	const int endOffset = endFrame ? 0 : CmdListReservedBytes;
	renderCommandList_t* const cmdList = &backEndData->commands;

	// always leave room for the end of list command
	if ( cmdList->used + bytes + endOffset > MAX_RENDER_COMMANDS ) {
		if ( bytes > MAX_RENDER_COMMANDS - endOffset ) {
			ri.Error( ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes );
		}
		if ( endFrame ) {
			// we reserved memory specifically for this case
			// so this really shouldn't ever happen
			ri.Error( ERR_FATAL, "R_GetCommandBuffer: can't allocate %i bytes to end the frame", bytes );
		}
		// if we run out of room, just start dropping commands
		return NULL;
	}

	cmdList->used += bytes;

	byte* const cmd = cmdList->cmds + cmdList->used - bytes;
	((renderCommandBase_t*)cmd)->commandId = commandId;

	return cmd;
}


template<typename T>
static T* AllocateRenderCommand( int commandId, qbool endFrame = qfalse )
{
	Q_assert( commandId >= 0 );
	Q_assert( commandId < RC_COUNT );
	Q_assert( (int)sizeof(T) == renderCommandSizes[commandId] );

	return (T*)R_AllocateRenderCommand( sizeof(T), commandId, endFrame );
}


static void Begin2D()
{
	if ( tr.renderMode != RM_UI ) {
		tr.renderMode = RM_UI;
		AllocateRenderCommand<beginUICommand_t>( RC_BEGIN_UI );
	}
}


static void End2D( qbool endFrame = qfalse )
{
	if ( tr.renderMode == RM_UI ) {
		tr.renderMode = RM_NONE;
		AllocateRenderCommand<endUICommand_t>( RC_END_UI, endFrame );
	}
}


static void Begin3D()
{
	if ( tr.renderMode != RM_3D ) {
		tr.renderMode = RM_3D;
		AllocateRenderCommand<begin3DCommand_t>( RC_BEGIN_3D );
	}
}


static void End3D( qbool endFrame = qfalse )
{
	if ( tr.renderMode == RM_3D ) {
		tr.renderMode = RM_NONE;
		AllocateRenderCommand<end3DCommand_t>( RC_END_3D, endFrame );
	}
}


static void BeginNK()
{
	if ( tr.renderMode != RM_NK ) {
		tr.renderMode = RM_NK;
		AllocateRenderCommand<beginNuklearCommand_t>( RC_BEGIN_NK );
	}
}


static void EndNK( qbool endFrame = qfalse )
{
	if ( tr.renderMode == RM_NK ) {
		tr.renderMode = RM_NONE;
		AllocateRenderCommand<endNuklearCommand_t>( RC_END_NK, endFrame );
	}
}


void R_AddDrawSurfCmd( drawSurf_t* drawSurfs, int numDrawSurfs, int numTranspSurfs )
{
	if ( !CanAllocateRenderCommand<drawSceneViewCommand_t>() )
		return;

	End2D();
	EndNK();
	Begin3D();

	drawSceneViewCommand_t* const cmd = AllocateRenderCommand<drawSceneViewCommand_t>(RC_DRAW_SCENE_VIEW);
	Q_assert( cmd );

	qbool shouldClearColor = qfalse;
	qbool shouldDrawScene = qtrue;
	vec4_t clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	if ( tr.refdef.rdflags & RDF_HYPERSPACE ) {
		const float c = RB_HyperspaceColor();
		clearColor[0] = c;
		clearColor[1] = c;
		clearColor[2] = c;
		shouldClearColor = qtrue;
		shouldDrawScene = qfalse;
	} else if ( r_fastsky->integer && !(tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		shouldClearColor = qtrue;
	} else if ( r_clear->integer ) {
		clearColor[0] = 1.0f;
		clearColor[1] = 0.0f;
		clearColor[2] = 1.0f;
		shouldClearColor = qtrue;
	}

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;
	cmd->numTranspSurfs = numTranspSurfs;
	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;
	cmd->shouldClearColor = shouldClearColor;
	cmd->shouldDrawScene = shouldDrawScene;
	Vector4Copy( clearColor, cmd->clearColor );
}


// passing NULL will set the color to white

void RE_SetColor( const float* rgba )
{
	if ( !CanAllocateRenderCommand<uiSetColorCommand_t>() )
		return;

	uiSetColorCommand_t* const cmd = AllocateRenderCommand<uiSetColorCommand_t>(RC_UI_SET_COLOR);
	Q_assert( cmd );

	if ( !rgba )
		rgba = colorWhite;

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}


void RE_StretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader )
{
	if ( !CanAllocateRenderCommand<uiDrawQuadCommand_t>() )
		return;

	End3D();
	EndNK();
	Begin2D();

	uiDrawQuadCommand_t* const cmd = AllocateRenderCommand<uiDrawQuadCommand_t>(RC_UI_DRAW_QUAD);
	Q_assert( cmd );

	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}


void RE_DrawTriangle( float x0, float y0, float x1, float y1, float x2, float y2, float s0, float t0, float s1, float t1, float s2, float t2, qhandle_t hShader )
{
	if ( !CanAllocateRenderCommand<uiDrawTriangleCommand_t>() )
		return;

	End3D();
	EndNK();
	Begin2D();

	uiDrawTriangleCommand_t* const cmd = AllocateRenderCommand<uiDrawTriangleCommand_t>(RC_UI_DRAW_TRIANGLE);
	Q_assert( cmd );

	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x0 = x0;
	cmd->y0 = y0;
	cmd->x1 = x1;
	cmd->y1 = y1;
	cmd->x2 = x2;
	cmd->y2 = y2;
	cmd->s0 = s0;
	cmd->t0 = t0;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}


// if running in stereo, RE_BeginFrame will be called twice for each RE_EndFrame

void RE_BeginFrame( stereoFrame_t stereoFrame )
{
	if ( !tr.registered )
		return;

	tr.frameCount++;
	tr.frameSceneNum = 0;
	tr.renderMode = RM_NONE;

	// delayed screenshot
	if ( r_delayedScreenshotPending ) {
		r_delayedScreenshotFrame++;
		if ( r_delayedScreenshotFrame >= 1 ) {
			backEndData->readbackCommands.screenshot = r_delayedScreenshot;
			backEndData->readbackCommands.screenshot.requested = qtrue;
			r_delayedScreenshotPending = qfalse;
			r_delayedScreenshotFrame = 0;
		}
	}

	//
	// draw buffer stuff
	//
	AllocateRenderCommand<beginFrameCommand_t>( RC_BEGIN_FRAME );
}


void RE_EndFrame( qbool render )
{
	if ( !tr.registered )
		return;

	if ( !render && r_delayedScreenshotPending )
		render = qtrue;

	if ( !render ) {
		readbackCommands_t* const readback = &backEndData->readbackCommands;
		if ( R_IsReadbackRequested( readback ) )
			render = qtrue;
	}

	backEnd.renderFrame = render;

	End2D( qtrue );
	End3D( qtrue );
	EndNK( qtrue );

	if ( render ) {
		AllocateRenderCommand<swapBuffersCommand_t>( RC_SWAP_BUFFERS, qtrue );
	} else {
		R_ClearFrame();
	}

	R_IssueRenderCommands();

	R_ClearFrame();

	Com_Memset( &tr.pc, 0, sizeof( tr.pc ) );
	Com_Memset( &backEnd.pc2D, 0, sizeof( backEnd.pc2D ) );
	Com_Memset( &backEnd.pc3D, 0, sizeof( backEnd.pc3D ) );
}


void RE_TakeVideoFrame( int width, int height, byte *captureBuffer, byte *encodeBuffer, qbool motionJpeg )
{
	if ( !CanAllocateRenderCommand<videoFrameCommand_t>() )
		return;

	End2D();
	End3D();
	EndNK();

	videoFrameCommand_t* const cmd = &backEndData->readbackCommands.videoFrame;
	cmd->requested = qtrue;
	cmd->width = width;
	cmd->height = height;
	cmd->captureBuffer = captureBuffer;
	cmd->encodeBuffer = encodeBuffer;
	cmd->motionJpeg = motionJpeg;
}


void R_EndScene( const viewParms_t* viewParms )
{
	if ( !CanAllocateRenderCommand<videoFrameCommand_t>() )
		return;

	Q_assert( tr.renderMode == RM_3D );

	endSceneCommand_t* const cmd = AllocateRenderCommand<endSceneCommand_t>( RC_END_SCENE );
	Q_assert( cmd );

	cmd->viewParms = *viewParms;
}

void RE_UploadNuklear( void* vertexes, int numVertexBytes, void* indexes, int numIndexBytes )
{
	if ( !CanAllocateRenderCommand<nuklearUploadCommand_t>() )
		return;

	End2D();
	End3D();
	BeginNK();

	Q_assert( tr.renderMode == RM_NK );

	nuklearUploadCommand_t* const cmd = AllocateRenderCommand<nuklearUploadCommand_t>( RC_NK_UPLOAD );
	Q_assert( cmd );

	cmd->vertexes = vertexes;
	cmd->numVertexBytes = numVertexBytes;
	cmd->indexes = indexes;
	cmd->numIndexBytes = numIndexBytes;
}


void RE_DrawNuklear( int firstIndex, int numIndexes, qhandle_t shader, const int* scissorRect )
{
	if ( !CanAllocateRenderCommand<nuklearDrawCommand_t>() )
		return;

	Q_assert( tr.renderMode == RM_NK );
	if ( tr.renderMode != RM_NK )
		return;

	nuklearDrawCommand_t* const cmd = AllocateRenderCommand<nuklearDrawCommand_t>( RC_NK_DRAW );
	Q_assert( cmd );

	cmd->firstIndex = firstIndex;
	cmd->numIndexes = numIndexes;
	cmd->shader = shader;
	cmd->scissorRect[0] = scissorRect[0];
	cmd->scissorRect[1] = scissorRect[1];
	cmd->scissorRect[2] = scissorRect[2];
	cmd->scissorRect[3] = scissorRect[3];
}

#if defined( QC )
void RE_GetAdvertisements(int* num, float* verts, void* shaders)
{
	*num = 0;
}
#endif
