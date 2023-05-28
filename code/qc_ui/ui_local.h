#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__


extern "C" {
	#include "../qcommon/q_shared.h"
	#include "../renderercommon/tr_types.h"
	#include "../client/keycodes.h"
	#include "../game/bg_public.h"
	#include "ui_public.h"
	#include <lapi.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

#include <dlmalloc.h>

// ui_atoms.cpp
extern mspace	ui_mspace;

void			UI_Init( void );
void			UI_Shutdown( void );
void			UI_KeyEvent( int key, int down );
void			UI_MouseEvent( int dx, int dy );
void			UI_Refresh( int realtime );
qboolean		UI_IsFullscreen( void );
void			UI_SetActiveMenu( uiMenuCommand_t menu );
qboolean		UI_ConsoleCommand( int realTime );
void			UI_DrawConnectScreen( qboolean overlay );

// ui_syscalls.cpp
void			trap_Print( const char *string );
void			trap_Error( const char *string ) __attribute__( ( noreturn ) );
int				trap_Milliseconds( void );
void			trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void			trap_Cvar_Update( vmCvar_t *vmCvar );
void			trap_Cvar_Set( const char *var_name, const char *value );
float			trap_Cvar_VariableValue( const char *var_name );
void			trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void			trap_Cvar_SetValue( const char *var_name, float value );
void			trap_Cvar_Reset( const char *name );
void			trap_Cvar_Create( const char *var_name, const char *var_value, int flags );
void			trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize );
int				trap_Argc( void );
void			trap_Argv( int n, char *buffer, int bufferLength );
void			trap_Cmd_ExecuteText( int exec_when, const char *text );	// don't use EXEC_NOW!
int				trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void			trap_FS_Read( void *buffer, int len, fileHandle_t f );
void			trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void			trap_FS_FCloseFile( fileHandle_t f );
int				trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
int				trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t
qhandle_t		trap_R_RegisterModel( const char *name );
qhandle_t		trap_R_RegisterSkin( const char *name );
qhandle_t		trap_R_RegisterShaderNoMip( const char *name );
void			trap_R_ClearScene( void );
void			trap_R_AddRefEntityToScene( const refEntity_t *re );
void			trap_R_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts );
void			trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void			trap_R_RenderScene( const refdef_t *fd );
void			trap_R_SetColor( const float *rgba );
void			trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
void			trap_UpdateScreen( void );
int				trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName );
void			trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
sfxHandle_t		trap_S_RegisterSound( const char *sample, qboolean compressed );
void			trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
void			trap_Key_GetBindingBuf( int keynum, char *buf, int buflen );
void			trap_Key_SetBinding( int keynum, const char *binding );
qboolean		trap_Key_IsDown( int keynum );
qboolean		trap_Key_GetOverstrikeMode( void );
void			trap_Key_SetOverstrikeMode( qboolean state );
void			trap_Key_ClearStates( void );
int				trap_Key_GetCatcher( void );
void			trap_Key_SetCatcher( int catcher );
void			trap_GetClipboardData( char *buf, int bufsize );
void			trap_GetClientState( uiClientState_t *state );
void			trap_GetGlconfig( glconfig_t *glconfig );
int				trap_GetConfigString( int index, char *buff, int buffsize );
int				trap_LAN_GetServerCount( int source );
void			trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen );
void			trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen );
int				trap_LAN_GetPingQueueCount( void );
int				trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen );
void			trap_LAN_ClearPing( int n );
void			trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime );
void			trap_LAN_GetPingInfo( int n, char *buf, int buflen );
int				trap_MemoryRemaining( void );
void			trap_GetCDKey( char *buf, int buflen );
void			trap_SetCDKey( const char *buf );
qboolean        trap_VerifyCDKey( const char *key, const char *chksum );
void			trap_SetPbClStatus( int status );

void			trap_R_RenderGeometry( const void *vertices, int numVertices, const int *indices, int numIndices, int textureHandle, const vec2_t translation );
void			trap_R_SaveScissor( qboolean save );
void			trap_R_EnableScissor( qboolean enable );
void			trap_R_SetScissor( int x, int y, int w, int h );
qhandle_t		trap_R_LoadTexture( const char *source, int *w, int *h );
qhandle_t		trap_R_UploadTexture( const byte *source, int w, int h );
void			trap_R_SetMatrix( const float *matrix );

// ui_lua.cpp
LUAMOD_API int	luaopen_trap( lua_State *L );

// ui_rml.cpp
void			UI_InitRML( lua_State *L );
void			UI_ShutdownRML( void );

#endif
