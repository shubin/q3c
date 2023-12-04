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

// ui_public.h -- engine calls and types exposed to the ui module
// A user mod should never modify this file

#include "../qcommon/tr_types.h"


#define UI_API_VERSION	4

typedef struct {
	connstate_t		connState;
	int				connectPacketCount;
	int				clientNum;
	char			servername[MAX_STRING_CHARS];
	char			updateInfoString[MAX_STRING_CHARS];
	char			messageString[MAX_STRING_CHARS];
} uiClientState_t;

typedef enum {
	UI_ERROR,
	UI_PRINT,
	UI_MILLISECONDS,
	UI_CVAR_SET,
	UI_CVAR_VARIABLEVALUE,
	UI_CVAR_VARIABLESTRINGBUFFER,
	UI_CVAR_SETVALUE,
	UI_CVAR_RESET,
	UI_CVAR_CREATE,
	UI_CVAR_INFOSTRINGBUFFER,
	UI_ARGC,
	UI_ARGV,
	UI_CMD_EXECUTETEXT,
	UI_FS_FOPENFILE,
	UI_FS_READ,
	UI_FS_WRITE,
	UI_FS_FCLOSEFILE,
	UI_FS_GETFILELIST,
	UI_R_REGISTERMODEL,
	UI_R_REGISTERSKIN,
	UI_R_REGISTERSHADERNOMIP,
	UI_R_CLEARSCENE,
	UI_R_ADDREFENTITYTOSCENE,
	UI_R_ADDPOLYTOSCENE,
	UI_R_ADDLIGHTTOSCENE,
	UI_R_RENDERSCENE,
	UI_R_SETCOLOR,
	UI_R_DRAWSTRETCHPIC,
	UI_UPDATESCREEN,
	UI_CM_LERPTAG,
	UI_CM_LOADMODEL,
	UI_S_REGISTERSOUND,
	UI_S_STARTLOCALSOUND,
	UI_KEY_KEYNUMTOSTRINGBUF,
	UI_KEY_GETBINDINGBUF,
	UI_KEY_SETBINDING,
	UI_KEY_ISDOWN,
	UI_KEY_GETOVERSTRIKEMODE,
	UI_KEY_SETOVERSTRIKEMODE,
	UI_KEY_CLEARSTATES,
	UI_KEY_GETCATCHER,
	UI_KEY_SETCATCHER,
	UI_GETCLIPBOARDDATA,
	UI_GETGLCONFIG,
	UI_GETCLIENTSTATE,
	UI_GETCONFIGSTRING,
	UI_LAN_GETPINGQUEUECOUNT,
	UI_LAN_CLEARPING,
	UI_LAN_GETPING,
	UI_LAN_GETPINGINFO,
	UI_CVAR_REGISTER,
	UI_CVAR_UPDATE,
	UI_MEMORY_REMAINING,
	UI_GET_CDKEY,
	UI_SET_CDKEY,
	DO_NOT_WANT_UI_R_REGISTERFONT,
	UI_R_MODELBOUNDS,
	DO_NOT_WANT_UI_PC_ADD_GLOBAL_DEFINE,
	DO_NOT_WANT_UI_PC_LOAD_SOURCE,
	DO_NOT_WANT_UI_PC_FREE_SOURCE,
	DO_NOT_WANT_UI_PC_READ_TOKEN,
	DO_NOT_WANT_UI_PC_SOURCE_FILE_AND_LINE,
	UI_S_STOPBACKGROUNDTRACK,
	UI_S_STARTBACKGROUNDTRACK,
	UI_REAL_TIME,
	UI_LAN_GETSERVERCOUNT,
	UI_LAN_GETSERVERADDRESSSTRING,
	UI_LAN_GETSERVERINFO,
	UI_LAN_MARKSERVERVISIBLE,
	UI_LAN_UPDATEVISIBLEPINGS,
	UI_LAN_RESETPINGS,
	UI_LAN_LOADCACHEDSERVERS,
	UI_LAN_SAVECACHEDSERVERS,
	UI_LAN_ADDSERVER,
	UI_LAN_REMOVESERVER,
	UI_CIN_PLAYCINEMATIC,
	UI_CIN_STOPCINEMATIC,
	UI_CIN_RUNCINEMATIC,
	UI_CIN_DRAWCINEMATIC,
	UI_CIN_SETEXTENTS,
	DO_NOT_WANT_UI_R_REMAP_SHADER,
	UI_VERIFY_CDKEY,
	UI_LAN_SERVERSTATUS,
	UI_LAN_GETSERVERPING,
	UI_LAN_SERVERISVISIBLE,
	UI_LAN_COMPARESERVERS,
	// 1.32
	UI_FS_SEEK,
	UI_SET_PBCLSTATUS,

	UI_MEMSET = 100,
	UI_MEMCPY,
	UI_STRNCPY,
	UI_SIN,
	UI_COS,
	UI_ATAN2,
	UI_SQRT,
	UI_FLOOR,
	UI_CEIL,

#if defined( QC )
	UI_R_DRAWTRIANGLE = 500,
#endif

	// engine extensions
	// the mod should _never_ use these symbols
	UI_EXT_GETVALUE = 700,
	UI_EXT_LOCATEINTEROPDATA,
	UI_EXT_R_ADDREFENTITYTOSCENE2,
	UI_EXT_CVAR_SETRANGE,
	UI_EXT_CVAR_SETHELP,
	UI_EXT_CMD_SETHELP,
	UI_EXT_ERROR2,
	UI_EXT_ENABLEERRORCALLBACK
} uiImport_t;


void		trap_Print( const char *string );
void		trap_Error( const char *string );
int			trap_Milliseconds( void );
int			trap_RealTime(qtime_t *qtime);
void		trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void		trap_Cvar_Update( vmCvar_t *vmCvar );
void		trap_Cvar_Set( const char *var_name, const char *value );
float		trap_Cvar_VariableValue( const char *var_name );
void		trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void		trap_Cvar_SetValue( const char *var_name, float value );
void		trap_Cvar_Reset( const char *name );
void		trap_Cvar_Create( const char *var_name, const char *var_value, int flags );
void		trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize );
int			trap_Argc( void );
void		trap_Argv( int n, char *buffer, int bufferLength );

int			trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void		trap_FS_Read( void *buffer, int len, fileHandle_t f );
void		trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void		trap_FS_FCloseFile( fileHandle_t f );
int			trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
int			trap_FS_Seek( fileHandle_t f, long offset, fsOrigin_t origin );

qhandle_t	trap_R_RegisterModel( const char *name );
qhandle_t	trap_R_RegisterSkin( const char *name );
qhandle_t	trap_R_RegisterShaderNoMip( const char *name );
void		trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
void		trap_R_ClearScene( void );
void		trap_R_AddRefEntityToScene( const refEntity_t *re );
void		trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void		trap_R_AddLightToScene( const vec3_t org, float radius, float r, float g, float b );
void		trap_R_RenderScene( const refdef_t *fd );
void		trap_R_SetColor( const float *rgba );
void		trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
void		trap_UpdateScreen( void );
int			trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName );

void		trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
void		trap_S_StopBackgroundTrack( void );
void		trap_S_StartBackgroundTrack( const char *intro, const char *loop);

#if !defined(Q3_VM)
sfxHandle_t	shit_S_RegisterSound( const char* sample, qboolean ignored );
#endif
#define		trap_S_RegisterSound( x ) shit_S_RegisterSound( x, qfalse )


void		trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
void		trap_Key_GetBindingBuf( int keynum, char *buf, int buflen );
void		trap_Key_SetBinding( int keynum, const char *binding );
qboolean	trap_Key_IsDown( int keynum );
qboolean	trap_Key_GetOverstrikeMode( void );
void		trap_Key_SetOverstrikeMode( qboolean state );
void		trap_Key_ClearStates( void );
int			trap_Key_GetCatcher( void );
void		trap_Key_SetCatcher( int catcher );

void		trap_GetClipboardData( char *buf, int bufsize );
void		trap_GetClientState( uiClientState_t *state );
void		trap_GetGlconfig( glconfig_t *glconfig );
int			trap_GetConfigString( int index, char* buff, int buffsize );
int			trap_MemoryRemaining( void );

int			trap_LAN_GetServerCount( int source );
void		trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen );
void		trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen );
int			trap_LAN_GetPingQueueCount( void );
int			trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen );
void		trap_LAN_ClearPing( int n );
void		trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime );
void		trap_LAN_GetPingInfo( int n, char *buf, int buflen );
int			trap_LAN_GetServerPing( int source, int n );
void		trap_LAN_LoadCachedServers();
void		trap_LAN_SaveCachedServers();
void		trap_LAN_MarkServerVisible(int source, int n, qboolean visible);
int			trap_LAN_ServerIsVisible( int source, int n);
qboolean	trap_LAN_UpdateVisiblePings( int source );
int			trap_LAN_AddServer(int source, const char *name, const char *addr);
void		trap_LAN_RemoveServer(int source, const char *addr);
void		trap_LAN_ResetPings(int n);
int			trap_LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 );

void		trap_GetCDKey( char *buf, int buflen );
void		trap_SetCDKey( char *buf );
qboolean	trap_VerifyCDKey( const char *key, const char *chksum);

void		trap_SetPbClStatus( int status );


// yet another retarded trap we're stuck with  >:(
#if !defined(Q3_VM)
void		trap_Cmd_ExecuteText( int ignored, const char *text );
#endif
#define		trap_SendConsoleCommand( x ) trap_Cmd_ExecuteText( 2, x )


typedef enum {
	UIMENU_NONE,
	UIMENU_MAIN,
	UIMENU_INGAME,
	UIMENU_NEED_CD,
	UIMENU_BAD_CD_KEY,
	UIMENU_TEAM,
	UIMENU_POSTGAME
} uiMenuCommand_t;

#define SORT_HOST			0
#define SORT_MAP			1
#define SORT_CLIENTS		2
#define SORT_GAME			3
#define SORT_PING			4
#define SORT_PUNKBUSTER		5


typedef enum {
	UI_GETAPIVERSION = 0,	// system reserved

	UI_INIT,
//	void	UI_Init( void );

	UI_SHUTDOWN,
//	void	UI_Shutdown( void );

	UI_KEY_EVENT,
//	void	UI_KeyEvent( int key, int down );

	UI_MOUSE_EVENT,
//	void	UI_MouseEvent( int dx, int dy );

	UI_REFRESH,
//	void	UI_Refresh( int time );

	UI_IS_FULLSCREEN,
//	qboolean UI_IsFullscreen( void );

	UI_SET_ACTIVE_MENU,
//	void	UI_SetActiveMenu( uiMenuCommand_t menu );

	UI_CONSOLE_COMMAND,
//	qboolean UI_ConsoleCommand( int realTime );

	UI_DRAW_CONNECT_SCREEN,
//	void	UI_DrawConnectScreen( qboolean overlay );

	UI_HASUNIQUECDKEY
// if !overlay, the background will be drawn, otherwise it will be
// overlayed over whatever the cgame has drawn.
// a GetClientState syscall will be made to get the current strings
} uiExport_t;

