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
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "../qcommon/tr_types.h"


// print levels for severity-coloring (FIXME: move to qcommon for cgame as well?)
typedef enum {
	PRINT_ALL,
	PRINT_DEVELOPER,		// only print when "developer 1"
	PRINT_WARNING,
	PRINT_ERROR
} printParm_t;


enum {
	RF_USEC,
	RF_LEAF_CLUSTER,
	RF_LEAF_AREA,
	RF_LEAFS,

	RF_BEZ_CULL_S_IN,
	RF_BEZ_CULL_S_CLIP,
	RF_BEZ_CULL_S_OUT,
	RF_BEZ_CULL_B_IN,
	RF_BEZ_CULL_B_CLIP,
	RF_BEZ_CULL_B_OUT,

	RF_MD3_CULL_S_IN,
	RF_MD3_CULL_S_CLIP,
	RF_MD3_CULL_S_OUT,
	RF_MD3_CULL_B_IN,
	RF_MD3_CULL_B_CLIP,
	RF_MD3_CULL_B_OUT,

	RF_LIGHT_CULL_IN,
	RF_LIGHT_CULL_OUT,
	RF_LIT_LEAFS,
	RF_LIT_SURFS,
	RF_LIT_CULLS,

	RF_STATS_MAX
};

// backend stats, especially V/I counts, can skyrocket in 2D
// which is technically correct, but not actually useful
// so we maintain two sets and switch between them as projection2D changes

enum {
	RB_USEC,
	RB_USEC_END, // post-process etc. + buffer swap
	RB_USEC_GPU, // not always available

	RB_VERTICES,
	RB_INDICES,
	RB_SURFACES,
	RB_BATCHES,
	RB_SHADER_CHANGES, // vertex + pixel shader combos, not Q3 shaders
	RB_DRAW_CALLS,     // dispatched API draw calls

	RB_LIT_VERTICES,
	RB_LIT_INDICES,
	RB_LIT_SURFACES,
	RB_LIT_BATCHES,

	RB_LIT_VERTICES_LATECULLTEST,
	RB_LIT_INDICES_LATECULL_IN,
	RB_LIT_INDICES_LATECULL_OUT,

	RB_STATS_MAX
};

//
// these are the functions exported by the refresh module
//
typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( qbool destroyWindow );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const char *name );
	qhandle_t (*RegisterSkin)( const char *name );
	qhandle_t (*RegisterShader)( const char *name );
	qhandle_t (*RegisterShaderNoMip)( const char *name );
	void	(*LoadWorld)( const char *name );

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)();

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)();
	void	(*AddRefEntityToScene)( const refEntity_t *re, qbool intShaderTime );
	void	(*AddPolyToScene)( qhandle_t hShader, int numVerts, const polyVert_t *verts, int num );
	qbool	(*LightForPoint)( const vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddLightToScene)( const vec3_t org, float radius, float r, float g, float b );
	void	(*RenderScene)( const refdef_t *fd, int us );

	void	(*SetColor)( const float* rgba );	// NULL = 1,1,1,1
	void	(*DrawStretchPic)( float x, float y, float w, float h,
					float s1, float t1, float s2, float t2, qhandle_t hShader );
	void	(*DrawTriangle)( float x0, float y0, float x1, float y1, float x2, float y2,
					float s0, float t0, float s1, float t1, float s2, float t2, qhandle_t hShader );

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qbool dirty);

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, they will be filled with stats tables
	void	(*EndFrame)( int* pcFE, int* pc2D, int* pc3D, qbool render );

	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
					int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );
#if defined( QC )
	int		(*ProjectDecal)(
		const vec3_t origin, const vec3_t dir, 
		vec_t radius, vec_t depth, vec_t orientation,
		int maxPoints, vec3_t pointBuffer, vec3_t attribBuffer,
		int maxFragments, markFragment_t *fragmentBuffer );
#endif // QC

	int		(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame,
					float frac, const char *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

	qbool (*GetEntityToken)( char* buffer, int size );
	qbool (*inPVS)( const vec3_t p1, const vec3_t p2 );

	void (*TakeVideoFrame)( int h, int w, byte* captureBuffer, byte *encodeBuffer, qbool motionJpeg );

	// when the final model-view matrix is computed, for cl_drawMouseLag
	int		(*GetCameraMatrixTime)();

	// qtrue means it should be safe to call any other function
	qbool	(*Registered)();

	// do we need to sleep this frame to maintain the frame-rate cap?
	qbool	(*ShouldSleep)();

	// is depth clamping enabled?
	qbool	(*DepthClamp)();

#if defined( QC )
	void (*GetAdvertisements)(int *num, float *verts, void *shaders);
#endif
} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	float	(*SetConsoleVisibility)( float fraction );

	// print message on the local console
	void	(QDECL *Printf)( printParm_t printLevel, PRINTF_FORMAT_STRING const char *fmt, ... );

	// abort the game
	void	(QDECL *Error)( int errorLevel, PRINTF_FORMAT_STRING const char *fmt, ... );

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	// time scaled
	int		(*Milliseconds)();

	// for profiling only
	// not time scaled
	int64_t	(*Microseconds)();

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( int size, ha_pref pref, char *label, char *file, int line );
#else
	void	*(*Hunk_Alloc)( int size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( int size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( int bytes );
	void	(*Free)( void *buf );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_SetHelp)( const char *name, const char *help );
	void	(*Cvar_Set)( const char *name, const char *value );

	void	(*Cvar_RegisterTable)( const cvarTableItem_t* cvars, int count );

	void	(*Cmd_RegisterTable)( const cmdTableItem_t* cmds, int count );
	void	(*Cmd_UnregisterModule)();

	int		(*Cmd_Argc)();
	const char* (*Cmd_Argv)(int i);

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, const float* points) );

	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int		(*FS_ReadFile)( const char *name, void **buf );
	int		(*FS_ReadFilePak)( const char *name, void **buf, int* pakChecksum );
	void	(*FS_FreeFile)( void *buf );
	char**	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	qbool	(*FS_GetPakPath)( char *name, int nameSize, int pakChecksum );

	// cinematic stuff
	qbool	(*CIN_GrabCinematic)( int handle, int* w, int* h, const byte** data, int* client, qbool* dirty );
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits );
	e_status (*CIN_RunCinematic)( int handle );

	void	(*CL_WriteAVIVideoFrame)( const byte *buffer, int size );
} refimport_t;


// this is the only function actually exported at the linker level
// if the module can't init to a valid rendering state, it will return NULL
const refexport_t* GetRefAPI( const refimport_t* rimp );


#endif	// __TR_PUBLIC_H
