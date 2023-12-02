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


void R_IssueRenderCommands()
{
	renderCommandList_t* cmdList = &backEndData->commands;

	// add an end-of-list command
	*(int *)(cmdList->cmds + cmdList->used) = RC_END_OF_LIST;

	// clear it out, in case this is a sync and not a buffer flip
	cmdList->used = 0;

    RB_ExecuteRenderCommands( cmdList->cmds );
}


void* R_FindRenderCommand( renderCommand_t type )
{
	renderCommandList_t* cmdList = &backEndData->commands;
	void* data = cmdList->cmds;
	void* end = cmdList->cmds + cmdList->used;

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		if( *(int *)data == type )
			return data;

		if ( data >= end )
			return NULL;

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = (char*)data + sizeof(setColorCommand_t);
			break;
		case RC_STRETCH_PIC:
			data = (char*)data + sizeof(stretchPicCommand_t);
			break;
		case RC_TRIANGLE:
			data = (char*)data + sizeof(triangleCommand_t);
			break;
		case RC_DRAW_SURFS:
			data = (char*)data + sizeof(drawSurfsCommand_t);
			break;
		case RC_BEGIN_FRAME:
			data = (char*)data + sizeof(beginFrameCommand_t);
			break;
		case RC_SWAP_BUFFERS:
			data = (char*)data + sizeof(swapBuffersCommand_t);
			break;
		case RC_SCREENSHOT:
			data = (char*)data + sizeof(screenshotCommand_t);
			break;
		case RC_VIDEOFRAME:
			data = (char*)data + sizeof(videoFrameCommand_t);
			break;

		case RC_END_OF_LIST:
		default:
			return NULL;
		}
	}
}


static void R_RemoveRenderCommand( void* cmd, int cmdSize )
{
	renderCommandList_t* cmdList = &backEndData->commands;
	const int endOffset = ((char*)cmd + cmdSize) - (char*)cmdList->cmds;
	const int endBytes = cmdList->used - endOffset;
	assert( cmd >= cmdList->cmds && ((char*)cmd + cmdSize) <= ((char*)cmdList->cmds + cmdList->used) );

	memmove( cmd, (char*)cmd + cmdSize, endBytes );
	cmdList->used -= cmdSize;
}


/*
============
R_GetCommandBuffer

make sure there is enough command space, waiting on the
render thread if needed.
============
*/
void *R_GetCommandBuffer( int bytes, qbool endFrame ) {
	const int reservedBytes = (int)( sizeof(swapBuffersCommand_t) + sizeof(screenshotCommand_t) );
	const int endOffset = endFrame ? 0 : reservedBytes;
	renderCommandList_t* const cmdList = &backEndData->commands;
	bytes = PAD(bytes, sizeof(void *));

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

	return cmdList->cmds + cmdList->used - bytes;
}


// technically, all commands should probably check tr.registered
// but realistically, only begin+end frame really need to
#define R_CMD_RET(T, ID)   T* cmd = (T*)R_GetCommandBuffer( sizeof(T), qfalse ); if (!cmd) return; cmd->commandId = ID
#define R_CMD_NORET(T, ID) T* cmd = (T*)R_GetCommandBuffer( sizeof(T), qfalse ); if (cmd)  cmd->commandId = ID
#define R_CMD_END(T, ID)   T* cmd = (T*)R_GetCommandBuffer( sizeof(T), qtrue  );           cmd->commandId = ID


void R_AddDrawSurfCmd( drawSurf_t* drawSurfs, int numDrawSurfs, int numTranspSurfs )
{
	R_CMD_RET( drawSurfsCommand_t, RC_DRAW_SURFS );

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;
	cmd->numTranspSurfs = numTranspSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;
}


// passing NULL will set the color to white

void RE_SetColor( const float* rgba )
{
	R_CMD_RET( setColorCommand_t, RC_SET_COLOR );

	if ( !rgba )
		rgba = colorWhite;

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}


void RE_StretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader )
{
	R_CMD_RET( stretchPicCommand_t, RC_STRETCH_PIC );

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
	R_CMD_RET( triangleCommand_t, RC_TRIANGLE );

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
	if (!tr.registered)
		return;

	tr.frameCount++;
	tr.frameSceneNum = 0;

	// delayed screenshot
	if ( r_delayedScreenshotPending ) {
		r_delayedScreenshotFrame++;
		if ( r_delayedScreenshotFrame >= 2 ) {
			R_CMD_NORET( screenshotCommand_t, RC_SCREENSHOT );
			*cmd = r_delayedScreenshot;
			r_delayedScreenshotPending = qfalse;
			r_delayedScreenshotFrame = 0;
		}
	}

	//
	// draw buffer stuff
	//
	R_CMD_NORET( beginFrameCommand_t, RC_BEGIN_FRAME );
}


void RE_EndFrame( int* pcFE, int* pc2D, int* pc3D, qbool render )
{
	if (!tr.registered)
		return;

	qbool delayScreenshot = qfalse;
	if ( !render && r_delayedScreenshotPending )
		render = qtrue;

	if ( !render ) {
		screenshotCommand_t* ssCmd = (screenshotCommand_t*)R_FindRenderCommand( RC_SCREENSHOT );

		if ( ssCmd )
			render = qtrue;

		if ( ssCmd && !ssCmd->delayed ) {
			// save and remove the command so we can push it back after the frame's done
			r_delayedScreenshot = *ssCmd;
			r_delayedScreenshot.delayed = qtrue;
			R_RemoveRenderCommand( ssCmd, sizeof(screenshotCommand_t) );
			delayScreenshot = qtrue;
		}
	}

	if ( render ) {
		{
			// forcing the sub-scope to prevent variable shadowing
			R_CMD_END( swapBuffersCommand_t, RC_SWAP_BUFFERS );
		}
		if ( delayScreenshot ) {
			R_CMD_END( screenshotCommand_t, RC_SCREENSHOT );
			*cmd = r_delayedScreenshot;
		}
	} else {
		R_ClearFrame();
	}

	R_IssueRenderCommands();

	R_ClearFrame();

	if (pcFE)
		Com_Memcpy( pcFE, &tr.pc, sizeof( tr.pc ) );

	if (pc2D)
		Com_Memcpy( pc2D, &backEnd.pc2D, sizeof( backEnd.pc2D ) );

	if (pc3D)
		Com_Memcpy( pc3D, &backEnd.pc3D, sizeof( backEnd.pc3D ) );

	Com_Memset( &tr.pc, 0, sizeof( tr.pc ) );
	Com_Memset( &backEnd.pc2D, 0, sizeof( backEnd.pc2D ) );
	Com_Memset( &backEnd.pc3D, 0, sizeof( backEnd.pc3D ) );
}


void RE_TakeVideoFrame( int width, int height, byte *captureBuffer, byte *encodeBuffer, qbool motionJpeg )
{
	R_CMD_RET( videoFrameCommand_t, RC_VIDEOFRAME );

	cmd->width = width;
	cmd->height = height;
	cmd->captureBuffer = captureBuffer;
	cmd->encodeBuffer = encodeBuffer;
	cmd->motionJpeg = motionJpeg;
}

#if defined( QC )
void RE_GetAdvertisements( int *num, float *verts, void *shaders ) {
	*num = 0;
}
#endif
