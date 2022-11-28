#include "ui_local.h"
#include <lauxlib.h>
#include <initializer_list>

//#undef cast
//#include "LuaBridge/LuaBridge.h"


#define LUA_INTFIELD(L, V, F) do { lua_pushstring( L, #F); lua_pushinteger( L, V.F ); lua_settable( L, -3 ); } while (false)
#define LUA_BOOLFIELD(L, V, F) do { lua_pushstring( L, #F); lua_pushboolean( L, V.F ); lua_settable( L, -3 ); } while (false)
#define LUA_NUMFIELD(L, V, F) do { lua_pushstring( L, #F); lua_pushnumber( L, V.F ); lua_settable( L, -3 ); } while (false)
#define LUA_STRFIELD(L, V, F) do { lua_pushstring( L, #F); lua_pushstring( L, V.F ); lua_settable( L, -3 ); } while (false)

//void			trap_Error( const char *string ) __attribute__( ( noreturn ) );
static int lua_Error( lua_State *L ) {
	const char *message = luaL_checkstring( L, 1 );
	trap_Error( message );
	return 0;
}

//void			trap_Print( const char *string );
static int lua_Print( lua_State *L ) {
	const char *message = luaL_checkstring( L, 1 );
	trap_Print( message );
	return 0;
}

//int				trap_Milliseconds( void );
static int lua_Milliseconds( lua_State *L ) {
	lua_pushinteger( L, trap_Milliseconds() );
	return 1;
}

//void			trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
static int lua_Cvar_Register( lua_State *L ) {
	const char *varName = luaL_checkstring( L, 1 );
	const char *defaultValue = luaL_checkstring( L, 2 );
	int flags = luaL_checkinteger( L, 3 );
	vmCvar_t *vmCvar = (vmCvar_t*)mspace_malloc( ui_mspace, sizeof( vmCvar_t ) );
	memset( (void*)vmCvar, 0, sizeof( vmCvar_t ) );
	trap_Cvar_Register( vmCvar, varName, defaultValue, flags );
	vmCvar_t **pvmCvar = (vmCvar_t**)lua_newuserdata( L, sizeof( vmCvar_t* ) );
	*pvmCvar = vmCvar;
	return 1;
}

//void			trap_Cvar_Update( vmCvar_t *vmCvar );
static int lua_Cvar_Update( lua_State *L ) {
	if ( !lua_isuserdata( L, 1 ) ) {
		return 0;
	}
	vmCvar_t **pvmCvar = (vmCvar_t**)lua_touserdata( L, 1 );
	trap_Cvar_Update( *pvmCvar );
	return 0;
}

//void			trap_Cvar_Set( const char *var_name, const char *value );
static int lua_Cvar_Set( lua_State *L ) {
	const char *var_name = luaL_checkstring( L, 1 );
	const char *value = luaL_checkstring( L, 2 );
	trap_Cvar_Set( var_name, value );
	return 0;
}

//float			trap_Cvar_VariableValue( const char *var_name );
static int lua_Cvar_VariableValue( lua_State *L ) {
	const char *var_name = luaL_checkstring( L, 1 );
	lua_pushnumber( L, trap_Cvar_VariableValue( var_name ) );
	return 1;
}

//void			trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
static int lua_Cvar_VariableStringBuffer( lua_State *L ) {
	const char *var_name = luaL_checkstring( L, 1 );
	static char buffer[1024];
	trap_Cvar_VariableStringBuffer( var_name, buffer, ARRAY_LEN( buffer ) );
	lua_pushstring( L, buffer );
	return 1;
}

//void			trap_Cvar_SetValue( const char *var_name, float value );
static int lua_Cvar_SetValue( lua_State *L ) {
	const char *var_name = luaL_checkstring( L, 1 );
	lua_Number value = luaL_checknumber( L, 2 );
	trap_Cvar_SetValue( var_name, (float)value );
	return 0;
}

//void			trap_Cvar_Reset( const char *name );
static int lua_Cvar_Reset( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	trap_Cvar_Reset( name );
	return 0;
}
//void			trap_Cvar_Create( const char *var_name, const char *var_value, int flags );
static int lua_Cvar_Create( lua_State *L ) {
	const char *var_name = luaL_checkstring( L, 1 );
	const char *var_value = luaL_checkstring( L, 2 );
	int flags = luaL_checkinteger( L, 3 );
	trap_Cvar_Create( var_name, var_value, flags );
	return 0;
}

//void			trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize );
static int lua_Cvar_InfoStringBuffer( lua_State *L ) {
	static char buffer[1024];
	int bit = luaL_checkinteger( L, 1 );
	trap_Cvar_InfoStringBuffer( bit, buffer, ARRAY_LEN( buffer ) );
	lua_pushstring( L, buffer );
	return 1;
}

//int				trap_Argc( void );
static int lua_Argc( lua_State *L ) {
	lua_pushinteger( L, trap_Argc() );
	return 1;
}

//void			trap_Argv( int n, char *buffer, int bufferLength );
static int lua_Argv( lua_State *L ) {
	static char buffer[1024];
	int n = luaL_checkinteger( L, 1 );
	trap_Argv( n, buffer, ARRAY_LEN( buffer ) );
	lua_pushstring( L, buffer );
	return 1;
}

//void			trap_Cmd_ExecuteText( int exec_when, const char *text );	// don't use EXEC_NOW!
static int lua_Cmd_ExecuteText( lua_State *L ) {
	int when = luaL_checkinteger( L, 1 );
	const char *text = luaL_checkstring( L, 2 );
	trap_Cmd_ExecuteText( when, text );
	return 0;
}

//int				trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
static int lua_FS_FOpenFile( lua_State *L ) {
	const char *qpath = luaL_checkstring( L, 1 );
	const fsMode_t mode = (fsMode_t)luaL_checkinteger( L, 2 );
	fileHandle_t f;
	int len = trap_FS_FOpenFile( qpath, &f, mode );
	lua_pushinteger( L, f );
	lua_pushinteger( L, len );
	return 2;
}

//void			trap_FS_Read( void *buffer, int len, fileHandle_t f );
static int lua_FS_Read( lua_State *L ) {
	int len = luaL_checkinteger( L, 1 );
	fileHandle_t f = luaL_checkinteger( L, 2 );
	void *buffer = dlmalloc( len );
	trap_FS_Read( buffer, len, f );
	lua_pushlstring( L, (char*)buffer, len );
	dlfree( buffer );
	return 1;
}

//void			trap_FS_Write( const void *buffer, int len, fileHandle_t f );
static int lua_FS_Write( lua_State *L ) {
	size_t len;
	const char *buffer = luaL_checklstring( L, 1, &len );
	fileHandle_t f = luaL_checkinteger( L, 2 );
	trap_FS_Write( buffer, (int)len, f );
	return 0;
}

//void			trap_FS_FCloseFile( fileHandle_t f );
static int lua_FS_FCloseFile( lua_State *L ) {
	fileHandle_t f = luaL_checkinteger( L, 1 );
	trap_FS_FCloseFile( f );
	return 0;
}

//int				trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
static int lua_FS_GetFileList( lua_State *L ) {
	const char *path = luaL_checkstring( L, 1 );
	const char *extension = luaL_checkstring( L, 2 );
	const int bufsize = 16384;
	char *listbuf = (char*)dlmalloc( bufsize );
	int nFiles = trap_FS_GetFileList( path, extension, listbuf, bufsize );
	char *bufend = listbuf;
	for ( int i = 0; i < nFiles; i++ ) {
		bufend += strlen( bufend ) + 1;
	}
	lua_pushinteger( L, nFiles );
	lua_pushlstring( L, listbuf, bufend - listbuf );
	dlfree( listbuf );
	return 2;
}

//int				trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t
static int lua_FS_Seek( lua_State *L ) {
	fileHandle_t f = luaL_checkinteger( L, 1 );
	long offset = luaL_checklong( L, 2 );
	int origin = luaL_checkinteger( L, 3 );
	lua_pushinteger( L, trap_FS_Seek( f, offset, origin ) );
	return 1;
}

//qhandle_t		trap_R_RegisterModel( const char *name );
static int lua_R_RegisterModel( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	lua_pushinteger( L, trap_R_RegisterModel( name ) );
	return 1;
}

//qhandle_t		trap_R_RegisterSkin( const char *name );
static int lua_R_RegisterSkin( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	lua_pushinteger( L, trap_R_RegisterSkin( name ) );
	return 1;
}

//qhandle_t		trap_R_RegisterShaderNoMip( const char *name );
static int lua_R_RegisterShaderNoMip( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	lua_pushinteger( L, trap_R_RegisterShaderNoMip( name ) );
	return 1;
}

//void			trap_R_ClearScene( void );
static int lua_R_ClearScene( lua_State *L ) {
	trap_R_ClearScene();
	return 0;
}

template <class ... Args>
void luapp_getfields(  lua_State *L, Args ... args ) {
	const auto list = { args... };
	for ( auto item : list ) {
		lua_getfield( L, 1, item );
	}
}

//void			trap_R_AddRefEntityToScene( const refEntity_t *re );
static int lua_R_AddRefEntityToScene( lua_State *L ) {
	refEntity_t ent;

	lua_settop( L, 1 );
	luaL_checktype( L, 1, LUA_TTABLE );

	luapp_getfields( L, 
		/* -20 */ "reType",
		/* -19 */ "renderfx",
		/* -18 */ "hModel",
		/* -17 */ "lightingOrigin",
		/* -16 */ "shadowPlane",
		/* -15 */ "axis",
		/* -14 */ "nonNormalizedAxes",
		/* -13 */ "origin",
		/* -12 */ "frame",
		/* -11 */ "oldOrigin",
		/* -10 */ "oldframe",
		/*  -9 */ "backlerp",
		/*  -8 */ "skinNum",
		/*  -7 */ "customSkin",
		/*  -6 */ "customShader",
		/*  -5 */ "shaderRGBA",
		/*  -4 */ "shaderTexCoord",
		/*  -3 */  "shaderTime",
		/*  -2 */ "radius",
		/*  -1 */ "rotation"
	);

	memset( &ent, 0, sizeof( refEntity_t ) );


	return 0;
}

//void			trap_R_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts );
static int lua_R_AddPolyToScene( lua_State *L ) {
	// not implemented
	return 0;
}

//void			trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
static int lua_R_AddLightToScene( lua_State *L ) {
	// not implemented
	return 0;
}

//void			trap_R_RenderScene( const refdef_t *fd );
static int lua_R_RenderScene( lua_State *L ) {
	// not implemented
	return 0;
}

//void			trap_R_SetColor( const float *rgba );
static int lua_R_SetColor( lua_State *L ) {
	float rgba[4];

	if ( lua_isnil( L, 1 ) ) {
		trap_R_SetColor( NULL );
	} else {
		rgba[0] = (float)luaL_checknumber( L, 1 );
		rgba[1] = (float)luaL_checknumber( L, 2 );
		rgba[2] = (float)luaL_checknumber( L, 3 );
		rgba[3] = (float)luaL_optnumber( L, 4, 1.0 );
		trap_R_SetColor( rgba );
	}
	return 0;
}

//void			trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
static int lua_R_DrawStretchPic( lua_State *L ) {
	float x = (float)luaL_checknumber( L, 1 );
	float y = (float)luaL_checknumber( L, 2 );
	float w = (float)luaL_checknumber( L, 3 );
	float h = (float)luaL_checknumber( L, 4 );
	float s1 = (float)luaL_checknumber( L, 5 );
	float t1 = (float)luaL_checknumber( L, 6 );
	float s2 = (float)luaL_checknumber( L, 7 );
	float t2 = (float)luaL_checknumber( L, 8 );
	qhandle_t hShader = luaL_checkinteger( L, 9 );
	trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader );
	return 0;
}

//void			trap_UpdateScreen( void );
static int lua_UpdateScreen( lua_State *L ) {
	trap_UpdateScreen();
	return 0;
}

//int			trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName );
static int lua_CM_LerpTag( lua_State *L ) {
	// not implemented
	return 0;
}

//void			trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
static int lua_S_StartLocalSound( lua_State *L ) {
	sfxHandle_t sfx = luaL_checkinteger( L, 1 );
	int channelNum = luaL_checkinteger( L, 2 );
	trap_S_StartLocalSound( sfx, channelNum );
	return 0;
}

//sfxHandle_t		trap_S_RegisterSound( const char *sample, qboolean compressed );
static int lua_S_RegisterSound( lua_State *L ) {
	const char *sample = luaL_checkstring( L, 1 );
	qboolean compressed = (qboolean)lua_toboolean( L, 2 );
	lua_pushinteger( L, trap_S_RegisterSound( sample, compressed ) );
	return 1;
}


//void			trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
static int lua_Key_KeynumToStringBuf( lua_State *L ) {
	int keynum = luaL_checkinteger( L, 1 );
	static char buf[32];
	trap_Key_KeynumToStringBuf( keynum, buf, ARRAY_LEN( buf ) );
	lua_pushstring( L, buf );
	return 1;
}

//void			trap_Key_GetBindingBuf( int keynum, char *buf, int buflen );
static int lua_Key_GetBindingBuf( lua_State *L ) {
	int keynum = luaL_checkinteger( L, 1 );
	static char buf[32];
	trap_Key_GetBindingBuf( keynum, buf, ARRAY_LEN( buf ) );
	lua_pushstring( L, buf );
	return 1;
}

//void			trap_Key_SetBinding( int keynum, const char *binding );
static int lua_Key_SetBinding( lua_State *L ) {
	int keynum = luaL_checkinteger( L, 1 );
	const char *binding = luaL_checkstring( L, 2 );
	trap_Key_SetBinding( keynum, binding );
	return 0;
}

//qboolean		trap_Key_IsDown( int keynum );
static int lua_Key_IsDown( lua_State *L ) {
	int keynum = luaL_checkinteger( L, 1 );
	lua_pushboolean( L, trap_Key_IsDown( keynum ) );
	return 1;
}

//qboolean		trap_Key_GetOverstrikeMode( void );
static int lua_Key_GetOverstrikeMode( lua_State *L ) {
	lua_pushboolean( L, trap_Key_GetOverstrikeMode() );
	return 1;
}

//void			trap_Key_SetOverstrikeMode( qboolean state );
static int lua_Key_SetOverstrikeMode( lua_State *L ) {
	qboolean state = ( qboolean )lua_toboolean( L, 1 );
	trap_Key_SetOverstrikeMode( state );
	return 0;
}

//void			trap_Key_ClearStates( void );
static int lua_Key_ClearStates( lua_State *L ) {
	trap_Key_ClearStates();
	return 0;
}

//int				trap_Key_GetCatcher( void );
static int lua_Key_GetCatcher( lua_State *L ) {
	lua_pushinteger( L, trap_Key_GetCatcher() );
	return 1;
}

//void			trap_Key_SetCatcher( int catcher );
static int lua_Key_SetCatcher( lua_State *L ) {
	int catcher = luaL_checkinteger( L, 1 );
	trap_Key_SetCatcher( catcher );
	return 0;
}

//void			trap_GetClipboardData( char *buf, int bufsize );
static int lua_GetClipboardData( lua_State *L ) {
	static char buf[1024];
	trap_GetClipboardData( buf, ARRAY_LEN( buf ) );
	lua_pushstring( L, buf );
	return 1;
}

//void			trap_GetClientState( uiClientState_t *state );
static int lua_GetClientState( lua_State *L ) {
	uiClientState_t state;
	trap_GetClientState( &state );

	lua_newtable( L );
	LUA_INTFIELD( L, state, connState );
	LUA_INTFIELD( L, state, connectPacketCount );
	LUA_INTFIELD( L, state, clientNum );
	LUA_STRFIELD( L, state, servername );
	LUA_STRFIELD( L, state, updateInfoString );
	LUA_STRFIELD( L, state, messageString );

	return 1;
}

//void			trap_GetGlconfig( glconfig_t *glconfig );
static int lua_GetGlconfig( lua_State *L ) {
	glconfig_t glconfig;

	trap_GetGlconfig( &glconfig );

	lua_newtable( L );
	LUA_STRFIELD( L, glconfig, renderer_string );
	LUA_STRFIELD( L, glconfig, vendor_string );
	LUA_STRFIELD( L, glconfig, version_string );
	LUA_STRFIELD( L, glconfig, extensions_string );
	LUA_INTFIELD( L, glconfig, maxTextureSize );
	LUA_INTFIELD( L, glconfig, numTextureUnits );
	LUA_INTFIELD( L, glconfig, colorBits );
	LUA_INTFIELD( L, glconfig, depthBits );
	LUA_INTFIELD( L, glconfig, stencilBits );
	LUA_INTFIELD( L, glconfig, driverType );
	LUA_INTFIELD( L, glconfig, hardwareType );
	LUA_BOOLFIELD( L, glconfig, deviceSupportsGamma );
	LUA_INTFIELD( L, glconfig, textureCompression );
	LUA_BOOLFIELD( L, glconfig, textureEnvAddAvailable );
	LUA_INTFIELD( L, glconfig, vidWidth );
	LUA_INTFIELD( L, glconfig, vidHeight );
	LUA_NUMFIELD( L, glconfig, windowAspect );
	LUA_INTFIELD( L, glconfig, displayFrequency );
	LUA_BOOLFIELD( L, glconfig, isFullscreen );
	LUA_BOOLFIELD( L, glconfig, stereoEnabled );
	LUA_BOOLFIELD( L, glconfig, smpActive );

	return 1;
}

//int				trap_GetConfigString( int index, char *buff, int buffsize );
static int lua_GetConfigString( lua_State *L ) {
	char buf[MAX_CONFIGSTRINGS];
	int index = luaL_checkinteger( L, 1 );
	int result = trap_GetConfigString( index, buf, ARRAY_LEN( buf ) );
	if ( result ) {
		lua_pushstring( L, buf );
	} else {
		lua_pushnil( L );
	}
	return 1;
}

//int				trap_LAN_GetServerCount( int source );
static int lua_LAN_GetServerCount( lua_State *L ) {
	int source = luaL_checkinteger( L, 1 );
	lua_pushinteger( L, trap_LAN_GetServerCount( source ) );
	return 1;
}

//void			trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen );
static int lua_LAN_GetServerAddressString( lua_State *L ) {
	int source = luaL_checkinteger( L, 1 );
	int n = luaL_checkinteger( L, 2 );
	static char buf[1024];
	trap_LAN_GetServerAddressString( source, n, buf, ARRAY_LEN( buf ) );
	lua_pushstring( L, buf );
	return 1;
}

//void			trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen );
static int lua_LAN_GetServerInfo( lua_State *L ) {
	int source = luaL_checkinteger( L, 1 );
	int n = luaL_checkinteger( L, 2 );
	static char buf[1024];
	trap_LAN_GetServerInfo( source, n, buf, ARRAY_LEN( buf ) );
	lua_pushstring( L, buf );
	return 1;
}

//int				trap_LAN_GetPingQueueCount( void );
static int lua_LAN_GetPingQueueCount( lua_State *L ) {
	lua_pushinteger( L, trap_LAN_GetPingQueueCount() );
	return 1;
}

//int				trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen );
static int lua_LAN_ServerStatus( lua_State *L ) {
	const char *serverAddress = luaL_checkstring( L, 1 );
	static char serverStatus[1024];
	trap_LAN_ServerStatus( serverAddress, serverStatus, ARRAY_LEN( serverStatus ) );
	lua_pushstring( L, serverStatus );
	return 1;
}

//void			trap_LAN_ClearPing( int n );
static int lua_LAN_ClearPing( lua_State *L ) {
	int n = luaL_checkinteger( L, 1 );
	trap_LAN_ClearPing( n );
	return 0;
}

//void			trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime );
static int lua_LAN_GetPing( lua_State *L ) {
	int n = luaL_checkinteger( L, 1 );
	static char buf[1024];
	int pingtime;
	trap_LAN_GetPing( n, buf, ARRAY_LEN( buf ), &pingtime );
	lua_pushstring( L, buf );
	lua_pushinteger( L, pingtime );
	return 2;
}

//void			trap_LAN_GetPingInfo( int n, char *buf, int buflen );
static int lua_LAN_GetPingInfo( lua_State *L ) {
	int n = luaL_checkinteger( L, 1 );
	static char buf[1024];
	trap_LAN_GetPingInfo( n, buf, ARRAY_LEN( buf ) );
	lua_pushstring( L, buf );
	return 1;
}

//int				trap_MemoryRemaining( void );
static int lua_MemoryRemaining( lua_State *L ) {
	lua_pushinteger( L, trap_MemoryRemaining() );
	return 1;
}

//void			trap_GetCDKey( char *buf, int buflen );
static int lua_GetCDKey( lua_State *L ) {
	static char buf[1024];
	trap_GetCDKey( buf, ARRAY_LEN( buf ) );
	lua_pushstring( L, buf );
	return 1;
}

//void			trap_SetCDKey( char *buf );
static int lua_SetCDKey( lua_State *L ) {
	const char *buf = luaL_checkstring( L, 1 );
	trap_SetCDKey( buf );
	return 0;
}

//qboolean        trap_VerifyCDKey( const char *key, const char *chksum );
static int lua_VerifyCDKey( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	const char *chksum = luaL_checkstring( L, 2 );
	lua_pushboolean( L, trap_VerifyCDKey( key, chksum ) );
	return 1;
}

//void			trap_SetPbClStatus( int status );
static int lua_SetPbClStatus( lua_State *L ) {
	int status = luaL_checkinteger( L, 1 );
	trap_SetPbClStatus( status );
	return 0;
}

//void trap_R_SetMatrix( const float *matrix )
static int lua_R_SetMatrix( lua_State *L ) {
	if ( lua_isnil( L, 1 ) ) {
		trap_R_SetMatrix( NULL );
	} else {
		// todo: pass the actual matrix
		trap_R_SetMatrix( NULL );
	}
	return 0;
}

static const luaL_Reg uilib[] = {
	{ "Print", lua_Print },
	{ "Error", lua_Error },
	{ "Milliseconds", lua_Milliseconds },
	{ "Cvar_Register", lua_Cvar_Register },
	{ "Cvar_Update", lua_Cvar_Update },
	{ "Cvar_Set", lua_Cvar_Set },
	{ "Cvar_VariableValue", lua_Cvar_VariableValue },
	{ "Cvar_VariableStringBuffer", lua_Cvar_VariableStringBuffer },
	{ "Cvar_SetValue", lua_Cvar_SetValue },
	{ "Cvar_Reset", lua_Cvar_Reset },
	{ "Cvar_Create", lua_Cvar_Create },
	{ "Cvar_InfoStringBuffer", lua_Cvar_InfoStringBuffer },
	{ "Argc", lua_Argc },
	{ "Argv", lua_Argv },
	{ "Cmd_ExecuteText", lua_Cmd_ExecuteText },
	{ "FS_FOpenFile", lua_FS_FOpenFile },
	{ "FS_Read", lua_FS_Read },
	{ "FS_Write", lua_FS_Write },
	{ "FS_FCloseFile", lua_FS_FCloseFile },
	{ "FS_GetFileList", lua_FS_GetFileList },
	{ "FS_Seek", lua_FS_Seek },
	{ "R_RegisterModel", lua_R_RegisterModel },
	{ "R_RegisterSkin", lua_R_RegisterSkin },
	{ "R_RegisterShaderNoMip", lua_R_RegisterShaderNoMip },
	{ "R_ClearScene", lua_R_ClearScene },
	{ "R_AddRefEntityToScene", lua_R_AddRefEntityToScene },
	{ "R_AddPolyToScene", lua_R_AddPolyToScene },
	{ "R_AddLightToScene", lua_R_AddLightToScene },
	{ "R_RenderScene", lua_R_RenderScene },
	{ "R_SetColor", lua_R_SetColor },
	{ "R_DrawStretchPic", lua_R_DrawStretchPic },
	{ "UpdateScreen", lua_UpdateScreen },
	{ "CM_LerpTag", lua_CM_LerpTag },
	{ "S_StartLocalSound", lua_S_StartLocalSound },
	{ "S_RegisterSound", lua_S_RegisterSound },
	{ "Key_KeynumToStringBuf", lua_Key_KeynumToStringBuf },
	{ "Key_GetBindingBuf", lua_Key_GetBindingBuf },
	{ "Key_SetBinding", lua_Key_SetBinding },
	{ "Key_IsDown", lua_Key_IsDown },
	{ "Key_GetOverstrikeMode", lua_Key_GetOverstrikeMode },
	{ "Key_SetOverstrikeMode", lua_Key_SetOverstrikeMode },
	{ "Key_ClearStates", lua_Key_ClearStates },
	{ "Key_GetCatcher", lua_Key_GetCatcher },
	{ "Key_SetCatcher", lua_Key_SetCatcher },
	{ "GetClipboardData", lua_GetClipboardData },
	{ "GetClientState", lua_GetClientState },
	{ "GetGlconfig", lua_GetGlconfig },
	{ "GetConfigString", lua_GetConfigString },
	{ "LAN_GetServerCount", lua_LAN_GetServerCount },
	{ "LAN_GetServerAddressString", lua_LAN_GetServerAddressString },
	{ "LAN_GetServerInfo", lua_LAN_GetServerInfo },
	{ "LAN_GetPingQueueCount", lua_LAN_GetPingQueueCount },
	{ "LAN_ServerStatus", lua_LAN_ServerStatus },
	{ "LAN_ClearPing", lua_LAN_ClearPing },
	{ "LAN_GetPing", lua_LAN_GetPing },
	{ "LAN_GetPingInfo", lua_LAN_GetPingInfo },
	{ "MemoryRemaining", lua_MemoryRemaining },
	{ "GetCDKey", lua_GetCDKey },
	{ "SetCDKey", lua_SetCDKey },
	{ "VerifyCDKey", lua_VerifyCDKey },
	{ "SetPbClStatus", lua_SetPbClStatus },
	{ "R_SetMatrix", lua_R_SetMatrix },
	{ NULL, NULL }
};

typedef struct {
	const char *name;
	int value;
} luaL_Integer;

static const luaL_Integer uilib_intconst[] = {
	{ "KEYCATCH_CONSOLE", KEYCATCH_CONSOLE },
	{ "KEYCATCH_UI", KEYCATCH_UI },
	{ "KEYCATCH_MESSAGE", KEYCATCH_MESSAGE },
	{ "KEYCATCH_CGAME", KEYCATCH_CGAME },
	{ "EXEC_NOW", EXEC_NOW },
	{ "EXEC_INSERT", EXEC_INSERT },
	{ "EXEC_APPEND", EXEC_APPEND },
	{ "FS_READ", FS_READ },
	{ "FS_WRITE", FS_WRITE },
	{ "FS_APPEND", FS_APPEND },
	{ "FS_APPEND_SYNC", FS_APPEND_SYNC },
	{ "FS_SEEK_CUR", FS_SEEK_CUR },
	{ "FS_SEEK_END", FS_SEEK_END },
	{ "FS_SEEK_SET", FS_SEEK_SET },
	{ "CHAN_AUTO", CHAN_AUTO },
	{ "CHAN_LOCAL", CHAN_LOCAL },
	{ "CHAN_WEAPON", CHAN_WEAPON },
	{ "CHAN_VOICE", CHAN_VOICE },
	{ "CHAN_ITEM", CHAN_ITEM },
	{ "CHAN_BODY", CHAN_BODY },
	{ "CHAN_LOCAL_SOUND", CHAN_LOCAL_SOUND },
	{ "CHAN_ANNOUNCER", CHAN_ANNOUNCER },
	{ "CA_UNINITIALIZED", CA_UNINITIALIZED },
	{ "CA_DISCONNECTED", CA_DISCONNECTED },
	{ "CA_AUTHORIZING", CA_AUTHORIZING },
	{ "CA_CONNECTING", CA_CONNECTING },
	{ "CA_CHALLENGING", CA_CHALLENGING },
	{ "CA_CONNECTED", CA_CONNECTED },
	{ "CA_LOADING", CA_LOADING },
	{ "CA_PRIMED", CA_PRIMED },
	{ "CA_ACTIVE", CA_ACTIVE },
	{ "CA_CINEMATIC", CA_CINEMATIC },
	{ "GLDRV_ICD", GLDRV_ICD },
	{ "GLDRV_STANDALONE", GLDRV_STANDALONE },
	{ "GLDRV_VOODOO", GLDRV_VOODOO },
	{ "GLHW_GENERIC", GLHW_GENERIC },
	{ "GLHW_3DFX_2D3D", GLHW_3DFX_2D3D },
	{ "GLHW_RIVA128", GLHW_RIVA128 },
	{ "GLHW_RAGEPRO" ,GLHW_RAGEPRO },
	{ "GLHW_PERMEDIA2", GLHW_PERMEDIA2 },
	{ "AS_LOCAL", AS_LOCAL },
	{ "AS_MPLAYER", AS_MPLAYER },
	{ "AS_GLOBAL", AS_GLOBAL },
	{ "AS_FAVORITES", AS_FAVORITES },

// refEntityType_t
	{ "RT_MODEL", RT_MODEL },
	{ "RT_POLY", RT_POLY },
	{ "RT_SPRITE", RT_SPRITE },
	{ "RT_BEAM", RT_BEAM },
	{ "RT_RAIL_CORE", RT_RAIL_CORE },
	{ "RT_RAIL_RINGS", RT_RAIL_RINGS },
	{ "RT_LIGHTNING", RT_LIGHTNING },
	{ "RT_PORTALSURFACE", RT_PORTALSURFACE },
	{ "RT_MAX_REF_ENTITY_TYPE", RT_MAX_REF_ENTITY_TYPE },

// Key codes
	{ "K_TAB", K_TAB },
	{ "K_ENTER", K_ENTER },
	{ "K_ESCAPE", K_ESCAPE },
	{ "K_SPACE", K_SPACE },

	{ "K_BACKSPACE", K_BACKSPACE },

	{ "K_COMMAND", K_COMMAND },
	{ "K_CAPSLOCK", K_CAPSLOCK },
	{ "K_POWER", K_POWER },
	{ "K_PAUSE", K_PAUSE },

	{ "K_UPARROW", K_UPARROW },
	{ "K_DOWNARROW", K_DOWNARROW },
	{ "K_LEFTARROW", K_LEFTARROW },
	{ "K_RIGHTARROW", K_RIGHTARROW },

	{ "K_ALT", K_ALT },
	{ "K_CTRL", K_CTRL },
	{ "K_SHIFT", K_SHIFT },
	{ "K_INS", K_INS },
	{ "K_DEL", K_DEL },
	{ "K_PGDN", K_PGDN },
	{ "K_PGUP", K_PGUP },
	{ "K_HOME", K_HOME },
	{ "K_END", K_END },

	{ "K_F1", K_F1 },
	{ "K_F2", K_F2 },
	{ "K_F3", K_F3 },
	{ "K_F4", K_F4 },
	{ "K_F5", K_F5 },
	{ "K_F6", K_F6 },
	{ "K_F7", K_F7 },
	{ "K_F8", K_F8 },
	{ "K_F9", K_F9 },
	{ "K_F10", K_F10 },
	{ "K_F11", K_F11 },
	{ "K_F12", K_F12 },
	{ "K_F13", K_F13 },
	{ "K_F14", K_F14 },
	{ "K_F15", K_F15 },

	{ "K_KP_HOME", K_KP_HOME },
	{ "K_KP_UPARROW", K_KP_UPARROW },
	{ "K_KP_PGUP", K_KP_PGUP },
	{ "K_KP_LEFTARROW", K_KP_LEFTARROW },
	{ "K_KP_5", K_KP_5 },
	{ "K_KP_RIGHTARROW", K_KP_RIGHTARROW },
	{ "K_KP_END", K_KP_END },
	{ "K_KP_DOWNARROW", K_KP_DOWNARROW },
	{ "K_KP_PGDN", K_KP_PGDN },
	{ "K_KP_ENTER", K_KP_ENTER },
	{ "K_KP_INS", K_KP_INS },
	{ "K_KP_DEL", K_KP_DEL },
	{ "K_KP_SLASH", K_KP_SLASH },
	{ "K_KP_MINUS", K_KP_MINUS },
	{ "K_KP_PLUS", K_KP_PLUS },
	{ "K_KP_NUMLOCK", K_KP_NUMLOCK },
	{ "K_KP_STAR", K_KP_STAR },
	{ "K_KP_EQUALS", K_KP_EQUALS },

	{ "K_MOUSE1", K_MOUSE1 },
	{ "K_MOUSE2", K_MOUSE2 },
	{ "K_MOUSE3", K_MOUSE3 },
	{ "K_MOUSE4", K_MOUSE4 },
	{ "K_MOUSE5", K_MOUSE5 },

	{ "K_MWHEELDOWN", K_MWHEELDOWN },
	{ "K_MWHEELUP", K_MWHEELUP },

	{ "K_JOY1", K_JOY1 },
	{ "K_JOY2", K_JOY2 },
	{ "K_JOY3", K_JOY3 },
	{ "K_JOY4", K_JOY4 },
	{ "K_JOY5", K_JOY5 },
	{ "K_JOY6", K_JOY6 },
	{ "K_JOY7", K_JOY7 },
	{ "K_JOY8", K_JOY8 },
	{ "K_JOY9", K_JOY9 },
	{ "K_JOY10", K_JOY10 },
	{ "K_JOY11", K_JOY11 },
	{ "K_JOY12", K_JOY12 },
	{ "K_JOY13", K_JOY13 },
	{ "K_JOY14", K_JOY14 },
	{ "K_JOY15", K_JOY15 },
	{ "K_JOY16", K_JOY16 },
	{ "K_JOY17", K_JOY17 },
	{ "K_JOY18", K_JOY18 },
	{ "K_JOY19", K_JOY19 },
	{ "K_JOY20", K_JOY20 },
	{ "K_JOY21", K_JOY21 },
	{ "K_JOY22", K_JOY22 },
	{ "K_JOY23", K_JOY23 },
	{ "K_JOY24", K_JOY24 },
	{ "K_JOY25", K_JOY25 },
	{ "K_JOY26", K_JOY26 },
	{ "K_JOY27", K_JOY27 },
	{ "K_JOY28", K_JOY28 },
	{ "K_JOY29", K_JOY29 },
	{ "K_JOY30", K_JOY30 },
	{ "K_JOY31", K_JOY31 },
	{ "K_JOY32", K_JOY32 },

	{ "K_AUX1", K_AUX1 },
	{ "K_AUX2", K_AUX2 },
	{ "K_AUX3", K_AUX3 },
	{ "K_AUX4", K_AUX4 },
	{ "K_AUX5", K_AUX5 },
	{ "K_AUX6", K_AUX6 },
	{ "K_AUX7", K_AUX7 },
	{ "K_AUX8", K_AUX8 },
	{ "K_AUX9", K_AUX9 },
	{ "K_AUX10", K_AUX10 },
	{ "K_AUX11", K_AUX11 },
	{ "K_AUX12", K_AUX12 },
	{ "K_AUX13", K_AUX13 },
	{ "K_AUX14", K_AUX14 },
	{ "K_AUX15", K_AUX15 },
	{ "K_AUX16", K_AUX16 },

	{ "K_WORLD_0", K_WORLD_0 },
	{ "K_WORLD_1", K_WORLD_1 },
	{ "K_WORLD_2", K_WORLD_2 },
	{ "K_WORLD_3", K_WORLD_3 },
	{ "K_WORLD_4", K_WORLD_4 },
	{ "K_WORLD_5", K_WORLD_5 },
	{ "K_WORLD_6", K_WORLD_6 },
	{ "K_WORLD_7", K_WORLD_7 },
	{ "K_WORLD_8", K_WORLD_8 },
	{ "K_WORLD_9", K_WORLD_9 },
	{ "K_WORLD_10", K_WORLD_10 },
	{ "K_WORLD_11", K_WORLD_11 },
	{ "K_WORLD_12", K_WORLD_12 },
	{ "K_WORLD_13", K_WORLD_13 },
	{ "K_WORLD_14", K_WORLD_14 },
	{ "K_WORLD_15", K_WORLD_15 },
	{ "K_WORLD_16", K_WORLD_16 },
	{ "K_WORLD_17", K_WORLD_17 },
	{ "K_WORLD_18", K_WORLD_18 },
	{ "K_WORLD_19", K_WORLD_19 },
	{ "K_WORLD_20", K_WORLD_20 },
	{ "K_WORLD_21", K_WORLD_21 },
	{ "K_WORLD_22", K_WORLD_22 },
	{ "K_WORLD_23", K_WORLD_23 },
	{ "K_WORLD_24", K_WORLD_24 },
	{ "K_WORLD_25", K_WORLD_25 },
	{ "K_WORLD_26", K_WORLD_26 },
	{ "K_WORLD_27", K_WORLD_27 },
	{ "K_WORLD_28", K_WORLD_28 },
	{ "K_WORLD_29", K_WORLD_29 },
	{ "K_WORLD_30", K_WORLD_30 },
	{ "K_WORLD_31", K_WORLD_31 },
	{ "K_WORLD_32", K_WORLD_32 },
	{ "K_WORLD_33", K_WORLD_33 },
	{ "K_WORLD_34", K_WORLD_34 },
	{ "K_WORLD_35", K_WORLD_35 },
	{ "K_WORLD_36", K_WORLD_36 },
	{ "K_WORLD_37", K_WORLD_37 },
	{ "K_WORLD_38", K_WORLD_38 },
	{ "K_WORLD_39", K_WORLD_39 },
	{ "K_WORLD_40", K_WORLD_40 },
	{ "K_WORLD_41", K_WORLD_41 },
	{ "K_WORLD_42", K_WORLD_42 },
	{ "K_WORLD_43", K_WORLD_43 },
	{ "K_WORLD_44", K_WORLD_44 },
	{ "K_WORLD_45", K_WORLD_45 },
	{ "K_WORLD_46", K_WORLD_46 },
	{ "K_WORLD_47", K_WORLD_47 },
	{ "K_WORLD_48", K_WORLD_48 },
	{ "K_WORLD_49", K_WORLD_49 },
	{ "K_WORLD_50", K_WORLD_50 },
	{ "K_WORLD_51", K_WORLD_51 },
	{ "K_WORLD_52", K_WORLD_52 },
	{ "K_WORLD_53", K_WORLD_53 },
	{ "K_WORLD_54", K_WORLD_54 },
	{ "K_WORLD_55", K_WORLD_55 },
	{ "K_WORLD_56", K_WORLD_56 },
	{ "K_WORLD_57", K_WORLD_57 },
	{ "K_WORLD_58", K_WORLD_58 },
	{ "K_WORLD_59", K_WORLD_59 },
	{ "K_WORLD_60", K_WORLD_60 },
	{ "K_WORLD_61", K_WORLD_61 },
	{ "K_WORLD_62", K_WORLD_62 },
	{ "K_WORLD_63", K_WORLD_63 },
	{ "K_WORLD_64", K_WORLD_64 },
	{ "K_WORLD_65", K_WORLD_65 },
	{ "K_WORLD_66", K_WORLD_66 },
	{ "K_WORLD_67", K_WORLD_67 },
	{ "K_WORLD_68", K_WORLD_68 },
	{ "K_WORLD_69", K_WORLD_69 },
	{ "K_WORLD_70", K_WORLD_70 },
	{ "K_WORLD_71", K_WORLD_71 },
	{ "K_WORLD_72", K_WORLD_72 },
	{ "K_WORLD_73", K_WORLD_73 },
	{ "K_WORLD_74", K_WORLD_74 },
	{ "K_WORLD_75", K_WORLD_75 },
	{ "K_WORLD_76", K_WORLD_76 },
	{ "K_WORLD_77", K_WORLD_77 },
	{ "K_WORLD_78", K_WORLD_78 },
	{ "K_WORLD_79", K_WORLD_79 },
	{ "K_WORLD_80", K_WORLD_80 },
	{ "K_WORLD_81", K_WORLD_81 },
	{ "K_WORLD_82", K_WORLD_82 },
	{ "K_WORLD_83", K_WORLD_83 },
	{ "K_WORLD_84", K_WORLD_84 },
	{ "K_WORLD_85", K_WORLD_85 },
	{ "K_WORLD_86", K_WORLD_86 },
	{ "K_WORLD_87", K_WORLD_87 },
	{ "K_WORLD_88", K_WORLD_88 },
	{ "K_WORLD_89", K_WORLD_89 },
	{ "K_WORLD_90", K_WORLD_90 },
	{ "K_WORLD_91", K_WORLD_91 },
	{ "K_WORLD_92", K_WORLD_92 },
	{ "K_WORLD_93", K_WORLD_93 },
	{ "K_WORLD_94", K_WORLD_94 },
	{ "K_WORLD_95", K_WORLD_95 },

	{ "K_SUPER", K_SUPER },
	{ "K_COMPOSE", K_COMPOSE },
	{ "K_MODE", K_MODE },
	{ "K_HELP", K_HELP },
	{ "K_PRINT", K_PRINT },
	{ "K_SYSREQ", K_SYSREQ },
	{ "K_SCROLLOCK", K_SCROLLOCK },
	{ "K_BREAK", K_BREAK },
	{ "K_MENU", K_MENU },
	{ "K_EURO", K_EURO },
	{ "K_UNDO", K_UNDO },

	// Gamepad controls
	// Ordered to match SDL2 game controller buttons and axes
	// Do not change this order without also changing IN_GamepadMove() in SDL_input.c
	{ "K_PAD0_A", K_PAD0_A },
	{ "K_PAD0_B", K_PAD0_B },
	{ "K_PAD0_X", K_PAD0_X },
	{ "K_PAD0_Y", K_PAD0_Y },
	{ "K_PAD0_BACK", K_PAD0_BACK },
	{ "K_PAD0_GUIDE", K_PAD0_GUIDE },
	{ "K_PAD0_START", K_PAD0_START },
	{ "K_PAD0_LEFTSTICK_CLICK", K_PAD0_LEFTSTICK_CLICK },
	{ "K_PAD0_RIGHTSTICK_CLICK", K_PAD0_RIGHTSTICK_CLICK },
	{ "K_PAD0_LEFTSHOULDER", K_PAD0_LEFTSHOULDER },
	{ "K_PAD0_RIGHTSHOULDER", K_PAD0_RIGHTSHOULDER },
	{ "K_PAD0_DPAD_UP", K_PAD0_DPAD_UP },
	{ "K_PAD0_DPAD_DOWN", K_PAD0_DPAD_DOWN },
	{ "K_PAD0_DPAD_LEFT", K_PAD0_DPAD_LEFT },
	{ "K_PAD0_DPAD_RIGHT", K_PAD0_DPAD_RIGHT },

	{ "K_PAD0_LEFTSTICK_LEFT", K_PAD0_LEFTSTICK_LEFT },
	{ "K_PAD0_LEFTSTICK_RIGHT", K_PAD0_LEFTSTICK_RIGHT },
	{ "K_PAD0_LEFTSTICK_UP", K_PAD0_LEFTSTICK_UP },
	{ "K_PAD0_LEFTSTICK_DOWN", K_PAD0_LEFTSTICK_DOWN },
	{ "K_PAD0_RIGHTSTICK_LEFT", K_PAD0_RIGHTSTICK_LEFT },
	{ "K_PAD0_RIGHTSTICK_RIGHT", K_PAD0_RIGHTSTICK_RIGHT },
	{ "K_PAD0_RIGHTSTICK_UP", K_PAD0_RIGHTSTICK_UP },
	{ "K_PAD0_RIGHTSTICK_DOWN", K_PAD0_RIGHTSTICK_DOWN },
	{ "K_PAD0_LEFTTRIGGER", K_PAD0_LEFTTRIGGER },
	{ "K_PAD0_RIGHTTRIGGER", K_PAD0_RIGHTTRIGGER },

	// Pseudo-key that brings the console down
	{ "K_CONSOLE", K_CONSOLE },

	{ "MAX_KEYS", MAX_KEYS },

	{ "K_CHAR_FLAG", K_CHAR_FLAG },
// End of key codes
	{ "CVAR_ARCHIVE", CVAR_ARCHIVE },
	{ "CVAR_USERINFO", CVAR_USERINFO },
	{ "CVAR_SERVERINFO", CVAR_SERVERINFO },
	{ "CVAR_SYSTEMINFO", CVAR_SYSTEMINFO },
	{ "CVAR_INIT", CVAR_INIT },
	{ "CVAR_LATCH", CVAR_LATCH },
	{ "CVAR_ROM", CVAR_ROM },
	{ "CVAR_USER_CREATED", CVAR_USER_CREATED },
	{ "CVAR_TEMP", CVAR_TEMP },
	{ "CVAR_CHEAT", CVAR_CHEAT },
	{ "CVAR_NORESTART", CVAR_NORESTART },

	{ "CVAR_SERVER_CREATED", CVAR_SERVER_CREATED },
	{ "CVAR_VM_CREATED", CVAR_VM_CREATED },
	{ "CVAR_PROTECTED", CVAR_PROTECTED },
	{ "CVAR_MODIFIED", CVAR_MODIFIED },
	{ "CVAR_NONEXISTENT", CVAR_NONEXISTENT },

	{ "UIMENU_NONE", UIMENU_NONE },
	{ "UIMENU_MAIN", UIMENU_MAIN },
	{ "UIMENU_INGAME", UIMENU_INGAME },
	{ "UIMENU_NEED_CD", UIMENU_NEED_CD },
	{ "UIMENU_BAD_CD_KEY", UIMENU_BAD_CD_KEY },
	{ "UIMENU_TEAM", UIMENU_TEAM },
	{ "UIMENU_POSTGAME", UIMENU_POSTGAME },

#if defined( QC )
	{ "UIMENU_DEATH", UIMENU_DEATH },
	{ "UIMENU_CHAMPIONS", UIMENU_CHAMPIONS },
	{ "UIMENU_CHAMPIONS_INGAME", UIMENU_CHAMPIONS_INGAME },
#endif

	{ NULL, 0 },
};

static void lua_setintconstants( lua_State *L, const luaL_Integer *l ) {
	for ( ; l->name != NULL; l++ ) {
		lua_pushinteger( L, l->value );
		lua_setfield( L, -2, l->name );
	}
}

LUAMOD_API int luaopen_trap( lua_State *L ) {
	lua_newtable( L );
	luaL_setfuncs( L, uilib, 0 );
	lua_setintconstants( L, uilib_intconst );
	lua_setglobal( L, "trap" );
	return 1;
}
