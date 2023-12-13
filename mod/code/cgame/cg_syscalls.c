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
//
// cg_syscalls.c -- this file is only included when building a dll
// cg_syscalls.asm is included instead when building a qvm
#ifdef Q3_VM
#error "Do not use in VM build"
#endif

#include "cg_local.h"

#if defined( QC )
#define MSABI __attribute__((ms_abi))
#endif // QC

typedef intptr_t( MSABI *syscall_t )( intptr_t callNum,
	intptr_t arg0,
	intptr_t arg1,
	intptr_t arg2,
	intptr_t arg3,
	intptr_t arg4,
	intptr_t arg5,
	intptr_t arg6,
	intptr_t arg7,
	intptr_t arg8,
	intptr_t arg9,
	intptr_t arg10,
	intptr_t arg11,
	intptr_t arg12,
	intptr_t arg13,
	intptr_t arg14,
	intptr_t arg15
	);

syscall_t psyscall;

#define  syscall1(cmd                                                                      ) psyscall(cmd, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall2(cmd, a0                                                                  ) psyscall(cmd, (intptr_t)a0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall3(cmd, a0, a1                                                              ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall4(cmd, a0, a1, a2                                                          ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall5(cmd, a0, a1, a2, a3                                                      ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall6(cmd, a0, a1, a2, a3, a4                                                  ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall7(cmd, a0, a1, a2, a3, a4, a5                                              ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall8(cmd, a0, a1, a2, a3, a4, a5, a6                                          ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t) 0, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define  syscall9(cmd, a0, a1, a2, a3, a4, a5, a6, a7                                      ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t) 0, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define syscall10(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8                                  ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t) 0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define syscall11(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9                              ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t)a9, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define syscall12(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10                         ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t)a9, (intptr_t)a10, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define syscall13(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11                    ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t)a9, (intptr_t)a10, (intptr_t)a11, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define syscall14(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12               ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t)a9, (intptr_t)a10, (intptr_t)a11, (intptr_t)a12, (intptr_t)  0, (intptr_t)  0, (intptr_t)  0)
#define syscall15(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13          ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t)a9, (intptr_t)a10, (intptr_t)a11, (intptr_t)a12, (intptr_t)a13, (intptr_t)  0, (intptr_t)  0)
#define syscall16(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14     ) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t)a9, (intptr_t)a10, (intptr_t)a11, (intptr_t)a12, (intptr_t)a13, (intptr_t)a14, (intptr_t)  0)
#define syscall17(cmd, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) psyscall(cmd, (intptr_t)a0, (intptr_t)a1, (intptr_t)a2, (intptr_t)a3, (intptr_t)a4, (intptr_t)a5, (intptr_t)a6, (intptr_t)a7, (intptr_t)a8, (intptr_t)a9, (intptr_t)a10, (intptr_t)a11, (intptr_t)a12, (intptr_t)a13, (intptr_t)a14, (intptr_t)a15)

Q_EXPORT void dllEntry( intptr_t (MSABI *syscallptr)( intptr_t arg,
	intptr_t arg0,
	intptr_t arg1,
	intptr_t arg2,
	intptr_t arg3,
	intptr_t arg4,
	intptr_t arg5,
	intptr_t arg6,
	intptr_t arg7,
	intptr_t arg8,
	intptr_t arg9,
	intptr_t arg10,
	intptr_t arg11,
	intptr_t arg12,
	intptr_t arg13,
	intptr_t arg14,
	intptr_t arg15
	) ) {
	psyscall = syscallptr;
}

int PASSFLOAT( float x ) {
	floatint_t fi;
	fi.f = x;
	return fi.i;
}

void	trap_Print( const char *fmt ) {
	syscall2( CG_PRINT, fmt );	
}

void trap_Error(const char *fmt)
{
	syscall2(CG_ERROR, fmt);	
	// shut up GCC warning about returning functions, because we know better
	exit(1);
}

int		trap_Milliseconds( void ) {
	return syscall1( CG_MILLISECONDS ); 	
}

void	trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	syscall5( CG_CVAR_REGISTER, vmCvar, varName, defaultValue, flags );	
}

void	trap_Cvar_Update( vmCvar_t *vmCvar ) {
	syscall2( CG_CVAR_UPDATE, vmCvar );	
}

void	trap_Cvar_Set( const char *var_name, const char *value ) {
	syscall3( CG_CVAR_SET, var_name, value );	
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	syscall4( CG_CVAR_VARIABLESTRINGBUFFER, var_name, buffer, bufsize );	
}

int		trap_Argc( void ) {
	return syscall1( CG_ARGC );	
}

void	trap_Argv( int n, char *buffer, int bufferLength ) {
	syscall4( CG_ARGV, n, buffer, bufferLength );	
}

void	trap_Args( char *buffer, int bufferLength ) {
	syscall3( CG_ARGS, buffer, bufferLength );	
}

int		trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	return syscall4( CG_FS_FOPENFILE, qpath, f, mode );	
}

void	trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	syscall4( CG_FS_READ, buffer, len, f );	
}

void	trap_FS_Write( const void *buffer, int len, fileHandle_t f ) {
	syscall4( CG_FS_WRITE, buffer, len, f );	
}

void	trap_FS_FCloseFile( fileHandle_t f ) {
	syscall2( CG_FS_FCLOSEFILE, f );	
}

int trap_FS_Seek( fileHandle_t f, long offset, int origin ) {
	return syscall4( CG_FS_SEEK, f, offset, origin );	
}

void	trap_SendConsoleCommand( const char *text ) {
	syscall2( CG_SENDCONSOLECOMMAND, text );	
}

void	trap_AddCommand( const char *cmdName ) {
	syscall2( CG_ADDCOMMAND, cmdName );	
}

void	trap_RemoveCommand( const char *cmdName ) {
	syscall2( CG_REMOVECOMMAND, cmdName );	
}

void	trap_SendClientCommand( const char *s ) {
	syscall2( CG_SENDCLIENTCOMMAND, s );	
}

void	trap_UpdateScreen( void ) {
	syscall1( CG_UPDATESCREEN );	
}

void	trap_CM_LoadMap( const char *mapname ) {
	syscall2( CG_CM_LOADMAP, mapname );	
}

int		trap_CM_NumInlineModels( void ) {
	return syscall1( CG_CM_NUMINLINEMODELS );	
}

clipHandle_t trap_CM_InlineModel( int index ) {
	return syscall2( CG_CM_INLINEMODEL, index );	
}

clipHandle_t trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs ) {
	return syscall3( CG_CM_TEMPBOXMODEL, mins, maxs );	
}

clipHandle_t trap_CM_TempCapsuleModel( const vec3_t mins, const vec3_t maxs ) {
	return syscall3( CG_CM_TEMPCAPSULEMODEL, mins, maxs );	
}

int		trap_CM_PointContents( const vec3_t p, clipHandle_t model ) {
	return syscall3( CG_CM_POINTCONTENTS, p, model );	
}

int		trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles ) {
	return syscall5( CG_CM_TRANSFORMEDPOINTCONTENTS, p, model, origin, angles );	
}

void	trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  clipHandle_t model, int brushmask ) {
	syscall8( CG_CM_BOXTRACE, results, start, end, mins, maxs, model, brushmask );	
}

void	trap_CM_CapsuleTrace( trace_t *results, const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  clipHandle_t model, int brushmask ) {
	syscall8( CG_CM_CAPSULETRACE, results, start, end, mins, maxs, model, brushmask );	
}

void	trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  clipHandle_t model, int brushmask,
						  const vec3_t origin, const vec3_t angles ) {
	syscall10( CG_CM_TRANSFORMEDBOXTRACE, results, start, end, mins, maxs, model, brushmask, origin, angles );	
}

void	trap_CM_TransformedCapsuleTrace( trace_t *results, const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  clipHandle_t model, int brushmask,
						  const vec3_t origin, const vec3_t angles ) {
	syscall10( CG_CM_TRANSFORMEDCAPSULETRACE, results, start, end, mins, maxs, model, brushmask, origin, angles );	
}

int		trap_CM_MarkFragments( int numPoints, const vec3_t *points, 
				const vec3_t projection,
				int maxPoints, vec3_t pointBuffer,
				int maxFragments, markFragment_t *fragmentBuffer ) {
	return syscall8( CG_CM_MARKFRAGMENTS, numPoints, points, projection, maxPoints, pointBuffer, maxFragments, fragmentBuffer );	
}

#if defined( QC )
int		trap_CM_ProjectDecal( 
			const vec3_t origin, const vec3_t dir,
			vec_t radius, vec_t depth, vec_t orientation,
			int maxPoints, vec3_t pointBuffer, vec3_t attribBuffer,
			int maxFragments, markFragment_t *fragmentBuffer ) {
	return syscall11( CG_CM_PROJECTDECAL,	
		origin, dir, 
		PASSFLOAT( radius ), PASSFLOAT( depth ), PASSFLOAT( orientation ),
		maxPoints, pointBuffer, attribBuffer,
		maxFragments, fragmentBuffer
	);
}
#endif // QC

void	trap_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx ) {
	syscall5( CG_S_STARTSOUND, origin, entityNum, entchannel, sfx );	
}

void	trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum ) {
	syscall3( CG_S_STARTLOCALSOUND, sfx, channelNum );	
}

void	trap_S_ClearLoopingSounds( qboolean killall ) {
	syscall2( CG_S_CLEARLOOPINGSOUNDS, killall );	
}

void	trap_S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx ) {
	syscall5( CG_S_ADDLOOPINGSOUND, entityNum, origin, velocity, sfx );	
}

void	trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx ) {
#if defined( QC )
	// CNQ3 does not implement CG_S_ADDREALLOOPINGSOUND
	syscall5( CG_S_ADDLOOPINGSOUND, entityNum, origin, velocity, sfx );	
#else
	syscall5( CG_S_ADDREALLOOPINGSOUND, entityNum, origin, velocity, sfx );	
#endif
}

void	trap_S_StopLoopingSound( int entityNum ) {
	syscall2( CG_S_STOPLOOPINGSOUND, entityNum );	
}

void	trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	syscall3( CG_S_UPDATEENTITYPOSITION, entityNum, origin );	
}

void	trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater ) {
	syscall5( CG_S_RESPATIALIZE, entityNum, origin, axis, inwater );	
}

sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed ) {
	return syscall3( CG_S_REGISTERSOUND, sample, compressed );	
}

void	trap_S_StartBackgroundTrack( const char *intro, const char *loop ) {
	syscall3( CG_S_STARTBACKGROUNDTRACK, intro, loop );	
}

void	trap_R_LoadWorldMap( const char *mapname ) {
	syscall2( CG_R_LOADWORLDMAP, mapname );	
}

qhandle_t trap_R_RegisterModel( const char *name ) {
	return syscall2( CG_R_REGISTERMODEL, name );	
}

qhandle_t trap_R_RegisterSkin( const char *name ) {
	return syscall2( CG_R_REGISTERSKIN, name );	
}

qhandle_t trap_R_RegisterShader( const char *name ) {
	return syscall2( CG_R_REGISTERSHADER, name );	
}

qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	return syscall2( CG_R_REGISTERSHADERNOMIP, name );	
}

void trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	syscall4(CG_R_REGISTERFONT, fontName, pointSize, font );	
}

void	trap_R_ClearScene( void ) {
	syscall1( CG_R_CLEARSCENE );	
}

void	trap_R_AddRefEntityToScene( const refEntity_t *re ) {
	syscall2( CG_R_ADDREFENTITYTOSCENE, re );	
}

void	trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts ) {
	syscall4( CG_R_ADDPOLYTOSCENE, hShader, numVerts, verts );	
}

void	trap_R_AddPolysToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num ) {
	syscall5( CG_R_ADDPOLYSTOSCENE, hShader, numVerts, verts, num );	
}

int		trap_R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir ) {
	return syscall5( CG_R_LIGHTFORPOINT, point, ambientLight, directedLight, lightDir );	
}

void	trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	syscall6( CG_R_ADDLIGHTTOSCENE, org, PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b) );	
}

void	trap_R_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	syscall6( CG_R_ADDADDITIVELIGHTTOSCENE, org, PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b) );	
}

void	trap_R_RenderScene( const refdef_t *fd ) {
	syscall2( CG_R_RENDERSCENE, fd );	
}

void	trap_R_SetColor( const float *rgba ) {
#if defined( QC )
	if ( rgba == NULL ) {
		cg.lastColor[0] = cg.lastColor[1] = cg.lastColor[2] = cg.lastColor[3] = 1.0f;
	} else {
		memcpy( cg.lastColor, rgba, sizeof( cg.lastColor ) );
	}
#endif
	syscall2( CG_R_SETCOLOR, rgba );	
}

void	trap_R_DrawStretchPic( float x, float y, float w, float h, 
							   float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	syscall10( CG_R_DRAWSTRETCHPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader );	
}

void	trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	syscall4( CG_R_MODELBOUNDS, model, mins, maxs );	
}

int		trap_R_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, 
					   float frac, const char *tagName ) {
	return syscall7( CG_R_LERPTAG, tag, mod, startFrame, endFrame, PASSFLOAT(frac), tagName );	
}

void	trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset ) {
	syscall4( CG_R_REMAP_SHADER, oldShader, newShader, timeOffset );	
}

void		trap_GetGlconfig( glconfig_t *glconfig ) {
	syscall2( CG_GETGLCONFIG, glconfig );	
}

void		trap_GetGameState( gameState_t *gamestate ) {
	syscall2( CG_GETGAMESTATE, gamestate );	
}

void		trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	syscall3( CG_GETCURRENTSNAPSHOTNUMBER, snapshotNumber, serverTime );	
}

qboolean	trap_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	return syscall3( CG_GETSNAPSHOT, snapshotNumber, snapshot );	
}

qboolean	trap_GetServerCommand( int serverCommandNumber ) {
	return syscall2( CG_GETSERVERCOMMAND, serverCommandNumber );	
}

int			trap_GetCurrentCmdNumber( void ) {
	return syscall1( CG_GETCURRENTCMDNUMBER );	
}

qboolean	trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	return syscall3( CG_GETUSERCMD, cmdNumber, ucmd );	
}

void		trap_SetUserCmdValue( int stateValue, float sensitivityScale ) {
	syscall3( CG_SETUSERCMDVALUE, stateValue, PASSFLOAT(sensitivityScale) );	
}

void		testPrintInt( char *string, int i ) {
	syscall3( CG_TESTPRINTINT, string, i );	
}

void		testPrintFloat( char *string, float f ) {
	syscall3( CG_TESTPRINTFLOAT, string, PASSFLOAT(f) );	
}

int trap_MemoryRemaining( void ) {
	return syscall1( CG_MEMORY_REMAINING );	
}

qboolean trap_Key_IsDown( int keynum ) {
	return syscall2( CG_KEY_ISDOWN, keynum );	
}

int trap_Key_GetCatcher( void ) {
	return syscall1( CG_KEY_GETCATCHER );	
}

void trap_Key_SetCatcher( int catcher ) {
	syscall2( CG_KEY_SETCATCHER, catcher );	
}

int trap_Key_GetKey( const char *binding ) {
	return syscall2( CG_KEY_GETKEY, binding );	
}

int trap_PC_AddGlobalDefine( char *define ) {
	return syscall2( CG_PC_ADD_GLOBAL_DEFINE, define );	
}

int trap_PC_LoadSource( const char *filename ) {
	return syscall2( CG_PC_LOAD_SOURCE, filename );	
}

int trap_PC_FreeSource( int handle ) {
	return syscall2( CG_PC_FREE_SOURCE, handle );	
}

int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	return syscall3( CG_PC_READ_TOKEN, handle, pc_token );	
}

int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	return syscall4( CG_PC_SOURCE_FILE_AND_LINE, handle, filename, line );	
}

void	trap_S_StopBackgroundTrack( void ) {
	syscall1( CG_S_STOPBACKGROUNDTRACK );	
}

int trap_RealTime(qtime_t *qtime) {
	return syscall2( CG_REAL_TIME, qtime );	
}

#if !defined( QC )
void trap_SnapVector( float *v ) {
	syscall2( CG_SNAPVECTOR, v );	
}
#endif // QC

// this returns a handle.  arg0 is the name in the format "idlogo.roq", set arg1 to NULL, alteredstates to qfalse (do not alter gamestate)
int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits) {
  return syscall7(CG_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, bits);
}
 
// stops playing the cinematic and ends it.  should always return FMV_EOF
// cinematics must be stopped in reverse order of when they are started
e_status trap_CIN_StopCinematic(int handle) {
  return syscall2(CG_CIN_STOPCINEMATIC, handle);
}


// will run a frame of the cinematic but will not draw it.  Will return FMV_EOF if the end of the cinematic has been reached.
e_status trap_CIN_RunCinematic (int handle) {
  return syscall2(CG_CIN_RUNCINEMATIC, handle);
}
 

// draws the current frame
void trap_CIN_DrawCinematic (int handle) {
  syscall2(CG_CIN_DRAWCINEMATIC, handle);
}
 

// allows you to resize the animation dynamically
void trap_CIN_SetExtents (int handle, int x, int y, int w, int h) {
  syscall6(CG_CIN_SETEXTENTS, handle, x, y, w, h);
}

/*
qboolean trap_loadCamera( const char *name ) {
	return syscall2( CG_LOADCAMERA, name );	
}

void trap_startCamera(int time) {
	syscall2(CG_STARTCAMERA, time);	
}

qboolean trap_getCameraInfo( int time, vec3_t *origin, vec3_t *angles) {
	return syscall4( CG_GETCAMERAINFO, time, origin, angles );	
}
*/

qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	return syscall3( CG_GET_ENTITY_TOKEN, buffer, bufferSize );	
}

qboolean trap_R_inPVS( const vec3_t p1, const vec3_t p2 ) {
	return syscall3( CG_R_INPVS, p1, p2 );	
}

#if defined( QC )
// QC engine extensions

void trap_Get_Advertisements( int *num, float *verts, char shaders[][MAX_QPATH] ) {
	syscall4( CG_GET_ADVERTISEMENTS, num, verts, shaders );	
}

void	trap_R_DrawTriangle(
	float x0, float y0, float x1, float y1, float x2, float y2, 
	float s0, float t0, float s1, float t1, float s2, float t2,
	qhandle_t hShader )
{
	syscall14( CG_R_DRAWTRIANGLE, 	
		PASSFLOAT(x0), PASSFLOAT(y0), PASSFLOAT(x1), PASSFLOAT(y1), PASSFLOAT(x2), PASSFLOAT(y2),
		PASSFLOAT(s0), PASSFLOAT(t0), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2),
		hShader );
}

#endif

#if defined( QC )
// CNQ3 engine extensions

qboolean trap_GetValue( char *value, int valueSize, const char *key ) {
	return syscall4( CG_EXT_GETVALUE, value, valueSize, key );	
}

void trap_LocateInteropData( unsigned char *interopBufferIn, int interopBufferInSize, unsigned char *interopBufferOut, int interopBufferOutSize ) {
	syscall5( CG_EXT_LOCATEINTEROPDATA, interopBufferIn, interopBufferInSize, interopBufferOut, interopBufferOutSize );	
}

void trap_R_AddRefEntityToScene2( const refEntity_t *re ) {
	syscall2( CG_EXT_R_ADDREFENTITYTOSCENE2, re );	
}

void trap_ForceFixedDLights( void ) {
	syscall1( CG_EXT_R_FORCEFIXEDDLIGHTS );	
}

void trap_SetInputForwarding( int cgameForwardInput ) {
	syscall2( CG_EXT_SETINPUTFORWARDING, cgameForwardInput );	
}

void trap_Cvar_SetRange( const char *var_name, int cvarType, const char *vmin, const char *vmax ) {
	syscall5( CG_EXT_CVAR_SETRANGE, var_name, cvarType, vmin, vmax );	
}

void trap_Cvar_SetHelp( const char *var_name, const char *help ) {
	syscall3( CG_EXT_CVAR_SETHELP, var_name, help );	
}

void trap_Cmd_SetHelp( const char *cmd_name, const char *help ) {
	syscall3( CG_EXT_CMD_SETHELP, cmd_name, help ); 	
}

void trap_MatchAlertEvent( int event ) {
	syscall2( CG_EXT_MATCHALERTEVENT, event );	
}

void trap_Error2( const char *message, qboolean realError ) {
	syscall3( CG_EXT_ERROR2, message, realError );	
}

qboolean trap_IsRecordingDemo( void ) {
	return syscall1( CG_EXT_ISRECORDINGDEMO );	
}

qboolean trap_NDP_Enable( int analyzeCommands, int generateCommands, int isCsNeeded, int analyzeSnapshot, int endAnalyzis ) {
	return syscall6( CG_EXT_NDP_ENABLE, analyzeCommands, generateCommands, isCsNeeded, analyzeSnapshot, endAnalyzis );	
}

int trap_NDP_Seek( int serverTime ) {
	return syscall2( CG_EXT_NDP_SEEK, serverTime );	
}

void trap_NDP_ReadUntil( int serverTime ) {
	syscall2( CG_EXT_NDP_READUNTIL, serverTime );	
}

qboolean trap_NDP_StartVideo( const char *fileNameNoExt, int aviFrameRate ) {
	return syscall3( CG_EXT_NDP_STARTVIDEO, fileNameNoExt, aviFrameRate ); 	
}

void trap_NDP_StopVideo( void ) {
	syscall1( CG_EXT_NDP_STOPVIDEO );	
}

void trap_R_RenderScene2( const refdef_t *fd, int us ) {
	syscall3( CG_EXT_R_RENDERSCENE, fd, us );	
}

#endif
